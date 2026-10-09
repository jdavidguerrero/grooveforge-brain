/**
 * @file main.cpp
 * @brief GrooveForge Brain · Studio head — cabeza gráfica de 800×480.
 *
 * Fase actual: modo demo (sin Teensy). El bucle es de un solo hilo; el hilo de I/O del bridge
 * llega con la tarea 38 (theory doc §2).
 *
 * Uso:
 *   studio-head                      # Mac (SDL) o Pi (FBDEV), modo demo
 *   studio-head --touch /dev/input/event4
 *   studio-head --snapshot perform.ppm [--snapshot-ms 3000]   # guarda la pantalla y sale
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "hal/display.h"
#include "lvgl.h"
#include "sim/demo_source.h"
#include "state/studio_state.h"
#include "studio_tick.h"
#include "ui/screen_boot.h"
#include "ui/screen_perform.h"

namespace {

constexpr uint32_t BOOT_MS = 1200;

void usage() {
    std::printf("studio-head [--touch <dispositivo evdev>] [--snapshot <archivo.ppm> [--snapshot-ms N]]\n");
}

/// Guarda la pantalla activa como PPM (P6, RGB de 8 bits).
bool save_snapshot(const char* path) {
    lv_img_dsc_t* img = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
    if (!img) return false;
    FILE* f = std::fopen(path, "wb");
    if (!f) { lv_snapshot_free(img); return false; }
    const int w = img->header.w, h = img->header.h;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    const lv_color_t* px = reinterpret_cast<const lv_color_t*>(img->data);
    for (int i = 0; i < w * h; ++i) {
        const uint32_t c = lv_color_to32(px[i]);
        const unsigned char rgb[3] = {uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    lv_snapshot_free(img);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const char* touch = nullptr;
    const char* snapshot = nullptr;
    uint32_t snapshot_ms = 3000;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--touch") == 0 && i + 1 < argc) {
            touch = argv[++i];
        } else if (std::strcmp(argv[i], "--snapshot") == 0 && i + 1 < argc) {
            snapshot = argv[++i];
        } else if (std::strcmp(argv[i], "--snapshot-ms") == 0 && i + 1 < argc) {
            snapshot_ms = uint32_t(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        }
    }

    lv_init();
    if (!hal_init(touch)) {
        std::fprintf(stderr, "studio-head: no se pudo inicializar la pantalla\n");
        return 1;
    }

    lv_scr_load(screen_boot_create("DEMO MODE"));

    StudioState state;
    ScreenPerform perform;
    bool booted = false;
    const uint32_t t0 = studio_tick_ms();

    for (;;) {
        const uint32_t now = studio_tick_ms();
        demo_tick(state, now);

        if (!booted && now - t0 >= BOOT_MS) {
            lv_obj_t* old = lv_scr_act();
            lv_scr_load(perform.create());
            lv_obj_del(old);
            booted = true;
        }
        if (booted) perform.update(state);

        if (snapshot && now - t0 >= snapshot_ms) {
            lv_refr_now(nullptr);
            const bool ok = save_snapshot(snapshot);
            std::printf("snapshot %s: %s\n", snapshot, ok ? "ok" : "error");
            return ok ? 0 : 1;
        }

        const uint32_t wait = lv_timer_handler();
        usleep(1000u * (wait < 5 ? wait : 5));
    }
}
