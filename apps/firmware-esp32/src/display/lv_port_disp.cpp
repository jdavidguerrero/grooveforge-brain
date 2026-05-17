/**
 * @file lv_port_disp.cpp
 * @brief LVGL display port — GC9A01 240x240 via TFT_eSPI
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
 * Aumentar a 240*40 si hay animaciones con muchas regiones invalidas simultaneous.
 *
 * lv_disp_flush_ready(): CRITICO — sin esta llamada LVGL cree que el flush
 * sigue en progreso y bloquea todo el rendering. El bug mas comun al portar.
 */

#include "lv_port_disp.h"
#include "ui_theme.h"
#include <TFT_eSPI.h>
#include <lvgl.h>

/* ── Constantes de configuracion ─────────────────────────────────────────── */

/**
 * Lineas por buffer: 20 lineas * 240px * 2 bytes = 9.4KB por buffer.
 * Elegido como balance entre uso de RAM y eficiencia de transferencia SPI.
 * El GC9A01 acepta burst de hasta la pantalla completa (240*240*2 = 115KB),
 * pero no tenemos esa RAM disponible como buffer contiguo.
 */
static constexpr uint16_t DISP_BUF_LINES = 20;

/**
 * GPIO del backlight del modulo Waveshare ESP32-S3-Touch-LCD-1.28.
 * NOTA: 01-architecture.md §3.3 lista GPIO 18 como backlight generico.
 * El modulo Waveshare especifico usa GPIO 40 (dato del hardware datasheet
 * del modulo). Cambiar aqui si el modulo fisico difiere.
 */
static constexpr uint8_t GPIO_BACKLIGHT = 40;

/* ── Buffers de rendering ────────────────────────────────────────────────── */

/* Declarados static para que vivan en BSS (zero-initialized) sin heap fragmentation */
static lv_color_t buf1[BRAIN_DISP_W * DISP_BUF_LINES];
static lv_color_t buf2[BRAIN_DISP_W * DISP_BUF_LINES];

static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t      disp_drv;

/* Instancia TFT_eSPI — usa la configuracion de build_flags en platformio.ini */
static TFT_eSPI tft = TFT_eSPI();

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
    tft.pushColors(reinterpret_cast<uint16_t*>(color_p), w * h, true);
    tft.endWrite();

    /* Avisar a LVGL que el flush termino — sin esto el rendering se bloquea */
    lv_disp_flush_ready(drv);
}

/* ── Implementacion publica ──────────────────────────────────────────────── */

void lv_port_disp_init(void) {
    /* 1. Inicializar el driver SPI y el GC9A01 */
    tft.begin();
    tft.setRotation(0);        /* portrait, 0° — ajustar si el display viene rotado en el enclosure */
    tft.fillScreen(TFT_BLACK); /* negro antes de que LVGL tome control, evita flash blanco */

    /* 2. Encender backlight al maximo brillo */
    pinMode(GPIO_BACKLIGHT, OUTPUT);
    analogWrite(GPIO_BACKLIGHT, 255);

    /* 3. Inicializar LVGL */
    lv_init();

    /* 4. Registrar double buffer — LVGL alterna entre buf1 y buf2 automaticamente */
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, BRAIN_DISP_W * DISP_BUF_LINES);

    /* 5. Registrar driver del display */
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = BRAIN_DISP_W;
    disp_drv.ver_res  = BRAIN_DISP_H;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}
