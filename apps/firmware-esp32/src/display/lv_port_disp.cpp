/**
 * @file lv_port_disp.cpp
 * @brief LVGL display port — GC9A01 240x240 via LovyanGFX
 *
 * Teoria — por que LovyanGFX en lugar de TFT_eSPI:
 *
 * TFT_eSPI 2.5.x es incompatible con Arduino ESP32 v3.x (IDF 5.x).
 * El crash ocurre en spiStartBus(): en IDF5 el switch-case que asigna
 * spi->dev (puntero al registro hardware SPI) no tiene case para todos
 * los hosts, retorna sin asignar, y spi->dev queda NULL.
 * La primera escritura al registro SPI toca NULL+0x10 → StoreProhibited.
 *
 * LovyanGFX resuelve esto con su propia capa de abstraccion SPI que
 * usa el API de IDF5 directamente, sin depender del spi_t interno.
 * Compatible con ESP32-S3 + GC9A01 + LVGL 8.
 *
 * Teoria — pipeline de rendering en LVGL 8:
 *
 * LVGL no escribe directo al display. Trabaja sobre un "draw buffer" en RAM,
 * renderiza ahi, y despues llama a flush_cb() para que el driver transfiera
 * los pixeles al display via SPI.
 *
 * Double buffer (buf1 + buf2): mientras LVGL renderiza en buf2, el SPI puede
 * estar transfiriendo buf1 en paralelo. Reduce tearing y aumenta throughput.
 *
 * Buffer size: 240 * 20 = 4800 pixeles * 2 bytes (RGB565) = 9600 bytes por buffer.
 * Total: ~19KB en SRAM interna. Suficiente para ~60fps en pantalla estatica.
 *
 * lv_disp_flush_ready(): CRITICO — sin esta llamada LVGL cree que el flush
 * sigue en progreso y bloquea todo el rendering. El bug mas comun al portar.
 */

#include "lv_port_disp.h"
#include "ui_theme.h"
#include <LovyanGFX.hpp>
#include <lvgl.h>

/* ── Configuracion del display GC9A01 — Waveshare ESP32-S3-Touch-LCD-1.28 ── */

/**
 * LGFX: clase de configuracion de LovyanGFX para este hardware especifico.
 *
 * Pin mapping del modulo Waveshare ESP32-S3-Touch-LCD-1.28:
 *   SPI2_HOST (FSPI en S3)
 *   MOSI = 11   SCLK = 10   CS = 9   DC = 8   RST = 12   BL = 40
 *
 * GC9A01 es write-only (spi_3wire = true): no tiene pin MISO.
 * freq_write = 40MHz: limite datasheet del GC9A01; probado estable a esta velocidad.
 * invert = true: el GC9A01 necesita inversion de color para mostrar colores correctos
 *   (el panel IPS circular invierte por hardware el mapeado de bits).
 */
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01 _panel;
    lgfx::Bus_SPI      _bus;
    lgfx::Light_PWM    _light;

public:
    LGFX(void) {
        /* ── Bus SPI ──────────────────────────────────────────────────────── */
        {
            auto cfg = _bus.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 40000000; /* 40MHz — limite GC9A01 */
            cfg.freq_read   = 16000000; /* no usado (write-only) */
            cfg.spi_3wire   = true;     /* sin MISO */
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 10;
            cfg.pin_mosi    = 11;
            cfg.pin_miso    = -1;
            cfg.pin_dc      = 8;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }

        /* ── Panel GC9A01 ─────────────────────────────────────────────────── */
        {
            auto cfg = _panel.config();
            cfg.pin_cs           = 9;
            cfg.pin_rst          = 12;
            cfg.pin_busy         = -1;
            cfg.panel_width      = 240;
            cfg.panel_height     = 240;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false; /* write-only */
            cfg.invert           = true;  /* GC9A01 IPS requiere inversion */
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel.config(cfg);
        }

        /* ── Backlight PWM ────────────────────────────────────────────────── */
        {
            auto cfg = _light.config();
            cfg.pin_bl      = 40;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }

        setPanel(&_panel);
    }
};

/* ── Instancia del display ───────────────────────────────────────────────── */

/* Lazy init dentro de lv_port_disp_init() para evitar construccion en static
 * init order (antes de setup()), que puede tocar el bus SPI antes de que el
 * hardware este listo. */
static LGFX* lcd = nullptr;

/* ── Buffers de rendering ────────────────────────────────────────────────── */

/**
 * Lineas por buffer: 20 lineas * 240px * 2 bytes = 9.4KB por buffer.
 * Balance entre uso de RAM y eficiencia de transferencia SPI.
 */
static constexpr uint16_t DISP_BUF_LINES = 20;

static lv_color_t buf1[BRAIN_DISP_W * DISP_BUF_LINES];
static lv_color_t buf2[BRAIN_DISP_W * DISP_BUF_LINES];

static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t      disp_drv;

/* ── flush callback ──────────────────────────────────────────────────────── */

/**
 * @brief Transfiere un area renderizada del buffer LVGL al display via SPI.
 *
 * LovyanGFX gestiona la transaccion SPI internamente. writePixels() con el
 * cast a lgfx::rgb565_t* envia los pixeles en el formato RGB565 que espera
 * el GC9A01 (big-endian por SPI; LovyanGFX hace el swap automaticamente).
 *
 * lv_disp_flush_ready() al final es OBLIGATORIA: le dice a LVGL que puede
 * reusar el buffer y continuar renderizando la proxima region.
 */
static void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;

    lcd->startWrite();
    lcd->setAddrWindow(area->x1, area->y1, w, h);
    lcd->writePixels(reinterpret_cast<lgfx::rgb565_t*>(color_p), w * h);
    lcd->endWrite();

    lv_disp_flush_ready(drv);
}

/* ── Implementacion publica ──────────────────────────────────────────────── */

void lv_port_disp_init(void) {
    /* 1. Construir LGFX aqui (no como global estatico) y arrancar el GC9A01.
     *    new() garantiza construccion dentro de setup(), con hardware disponible. */
    Serial.println("[disp] new LGFX"); Serial.flush();
    lcd = new LGFX();

    Serial.println("[disp] lcd->init()"); Serial.flush();
    lcd->init();

    Serial.println("[disp] setRotation"); Serial.flush();
    lcd->setRotation(0); /* portrait 0 deg */

    Serial.println("[disp] fillScreen"); Serial.flush();
    lcd->fillScreen(TFT_BLACK); /* negro antes de que LVGL tome control */

    Serial.println("[disp] setBrightness"); Serial.flush();
    lcd->setBrightness(255); /* backlight al 100% */

    /* 2. Inicializar LVGL */
    Serial.println("[disp] lv_init()"); Serial.flush();
    lv_init();

    /* 3. Registrar double buffer */
    Serial.println("[disp] draw_buf_init"); Serial.flush();
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, BRAIN_DISP_W * DISP_BUF_LINES);

    /* 4. Registrar driver del display */
    Serial.println("[disp] drv_register"); Serial.flush();
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = BRAIN_DISP_W;
    disp_drv.ver_res  = BRAIN_DISP_H;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    Serial.println("[disp] init done"); Serial.flush();
}
