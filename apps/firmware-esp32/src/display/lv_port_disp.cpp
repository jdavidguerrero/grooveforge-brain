/**
 * @file lv_port_disp.cpp
 * @brief LVGL display port — GC9A01 240x240 via TFT_eSPI
 *
 * Teoria — por que USER_SETUP_LOADED es critico:
 *
 * Sin -DUSER_SETUP_LOADED=1, TFT_eSPI busca e incluye su propio User_Setup.h
 * con pines por defecto que no corresponden al hardware. Al llamar tft->begin(),
 * spiStartBus() recibe un bus_num invalido para IDF5: el switch-case interno no
 * cubre ese valor, retorna sin asignar spi->dev, y la primera escritura al
 * registro SPI toca NULL+0x10 → StoreProhibited en EXCVADDR 0x00000010.
 *
 * Con -DUSER_SETUP_LOADED=1 + -DGC9A01_DRIVER=1 + -DUSE_HSPI_PORT=1 + pines
 * via build flags, TFT_eSPI toma el path correcto para ESP32-S3 / SPI2 / GC9A01.
 * Validado en groove_drum con el mismo modulo Waveshare ESP32-S3-Touch-LCD-1.28.
 *
 * Teoria — pipeline de rendering en LVGL 8:
 *
 * LVGL no escribe directo al display. Trabaja sobre un "draw buffer" en RAM,
 * renderiza ahi, y despues llama a flush_cb() para que el driver transfiera
 * los pixeles al display via SPI.
 *
 * Double buffer (buf1 + buf2): mientras LVGL renderiza en buf2, el SPI puede
 * estar transfiriendo buf1 en paralelo. Reduce tearing y aumenta throughput.
 * Con un solo buffer habria que esperar que el SPI termine antes de renderizar
 * la proxima region.
 *
 * Buffer size: 240 * 20 = 4800 pixeles * 2 bytes (RGB565) = 9600 bytes por buffer.
 * Total: ~19KB en SRAM interna. Suficiente para ~60fps en pantalla estatica.
 * Aumentar a 240*40 si hay animaciones con muchas regiones invalidas simultaneas.
 *
 * lv_disp_flush_ready(): CRITICO — sin esta llamada LVGL cree que el flush
 * sigue en progreso y bloquea todo el rendering. El bug mas comun al portar.
 */

#include "lv_port_disp.h"
#include "ui_theme.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <lvgl.h>

/* ── Constantes de configuracion ─────────────────────────────────────────── */

/**
 * Lineas por buffer: 20 lineas * 240px * 2 bytes = 9.4KB por buffer.
 * Elegido como balance entre uso de RAM y eficiencia de transferencia SPI.
 * El GC9A01 acepta burst de hasta la pantalla completa (240*240*2 = 115KB),
 * pero no tenemos esa RAM disponible como buffer contiguo.
 */
/* Sprint 3.4: 40 lineas (antes 20). Las vistas con canvas/animacion invalidan
 * regiones grandes cada frame; un buffer mas alto reduce el numero de flushes.
 * 240*40*2 = 19.2KB por buffer, 38KB total en BSS — holgado en la SRAM del S3. */
static constexpr uint16_t DISP_BUF_LINES = 40;

/* ── Buffers de rendering ────────────────────────────────────────────────── */

/* Declarados static para que vivan en BSS (zero-initialized) sin heap fragmentation */
static lv_color_t buf1[BRAIN_DISP_W * DISP_BUF_LINES];
static lv_color_t buf2[BRAIN_DISP_W * DISP_BUF_LINES];

static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t      disp_drv;

/* TFT_eSPI como global estatico — patron extraido de groove_drum (mismo HW).
 * El constructor corre en static init (antes de setup()), sin tocar hardware.
 * Con USER_SETUP_LOADED=1 + GC9A01_DRIVER=1 el constructor solo almacena config;
 * el hardware SPI se inicializa en tft.init() dentro de lv_port_disp_init().
 * La instancia existente antes de SPI.begin() es necesaria para que HSPI
 * registre su SPIClass correctamente cuando el bus se pre-inicializa. */
static TFT_eSPI tft(BRAIN_DISP_W, BRAIN_DISP_H);

/* ── flush callback ──────────────────────────────────────────────────────── */

/**
 * @brief Transfiere un area renderizada del buffer al display via SPI.
 *
 * LVGL llama esta funcion cuando tiene una region lista para dibujar.
 * startWrite()/endWrite() habilitan/deshabilitan el chip-select del SPI —
 * necesario para que pushColors() no intervenga con otras transferencias.
 *
 * La llamada a lv_disp_flush_ready() al final es OBLIGATORIA: le dice a LVGL
 * que puede reusar el buffer y continuar renderizando la proxima region.
 */
static void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    const uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
    const uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    /* swap=true: el HAL SPI del ESP32 invierte bytes en la transferencia DMA.
     * pushColors(true) compensa esa inversion — patron de groove_drum (mismo HW). */
    tft.pushColors(reinterpret_cast<uint16_t*>(color_p), w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(drv);
}

/* ── Implementacion publica ──────────────────────────────────────────────── */

void lv_port_disp_init(void) {
    /* Orden identico a groove_drum (mismo HW, funcionaba):
     * 1. SPI.begin() pre-init del bus HSPI con pines correctos
     * 2. Backlight
     * 3. tft.init() + setSwapBytes
     * 4. fillScreen de prueba
     * 5. lv_init() + buffers + driver */

    /* SPI.begin() pre-inicializa el bus HSPI con los pines correctos ANTES de
     * tft.init(). Sin esto spiStartBus() recibe bus_num invalido en IDF5:
     * spi->dev queda NULL → StoreProhibited @ EXCVADDR 0x10. */
    Serial.println("[disp] SPI.begin"); Serial.flush();
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);

    /* Backlight en GPIO 2 (TFT_BL) ANTES de tft.init() — pin validado en
     * groove_drum. TFT_eSPI tambien lo maneja via TFT_BACKLIGHT_ON, esto es
     * belt-and-suspenders para garantizar luz desde el primer instante. */
    pinMode(2, OUTPUT); digitalWrite(2, HIGH);
    Serial.println("[disp] backlight ON"); Serial.flush();

    Serial.println("[disp] tft.init()"); Serial.flush();
    tft.init();
    tft.setSwapBytes(true);

    tft.setRotation(0);

    /* Limpiar la pantalla a negro antes de ceder el control a LVGL. */
    tft.fillScreen(TFT_BLACK);

    /* 2. Inicializar LVGL */
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
