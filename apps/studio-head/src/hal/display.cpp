#include "hal/display.h"

#include <cstdio>

#include "lvgl.h"

#if defined(STUDIO_DISPLAY_SDL)
#include "sdl/sdl.h"
#elif defined(STUDIO_DISPLAY_FBDEV)
#include "display/fbdev.h"
#include "indev/evdev.h"
#endif

namespace {

constexpr int HOR = 800;
constexpr int VER = 480;
// Refresco parcial: buffer de 1/6 de pantalla (theory doc §5.1). Dos buffers para dibujar
// mientras se vuelca el anterior.
constexpr int BUF_PIXELS = HOR * VER / 6;

lv_color_t buf1[BUF_PIXELS];
lv_color_t buf2[BUF_PIXELS];
lv_disp_draw_buf_t draw_buf;
lv_disp_drv_t disp_drv;
lv_indev_drv_t indev_drv;

}  // namespace

bool hal_init(const char* touch_dev) {
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, BUF_PIXELS);
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.hor_res = HOR;
    disp_drv.ver_res = VER;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;

#if defined(STUDIO_DISPLAY_SDL)
    (void)touch_dev;
    sdl_init();
    disp_drv.flush_cb = sdl_display_flush;
    indev_drv.read_cb = sdl_mouse_read;
#elif defined(STUDIO_DISPLAY_FBDEV)
    fbdev_init();
    disp_drv.flush_cb = fbdev_flush;
    evdev_init();
    if (touch_dev && !evdev_set_file(const_cast<char*>(touch_dev))) {
        std::fprintf(stderr, "studio-head: no se pudo abrir el touch %s (sigo sin touch)\n", touch_dev);
    }
    indev_drv.read_cb = evdev_read;
#else
#error "Define STUDIO_DISPLAY_SDL o STUDIO_DISPLAY_FBDEV (lo hace CMake)"
#endif

    if (!lv_disp_drv_register(&disp_drv)) return false;
    lv_indev_drv_register(&indev_drv);
    return true;
}
