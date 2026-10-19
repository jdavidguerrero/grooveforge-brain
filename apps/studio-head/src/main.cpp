/**
 * @file main.cpp
 * @brief GrooveForge Brain · Studio head — cabeza gráfica de 800×480.
 *
 * Sin --uart corre en modo demo (datos simulados). Con --uart, un hilo de I/O aparte habla el
 * Bridge Protocol con el Teensy (src/link/, tarea 38, theory doc §2–4) y la UI dibuja ese estado.
 *
 * Uso:
 *   studio-head                      # Mac (SDL) o Pi (FBDEV), modo demo
 *   studio-head --touch /dev/input/event4
 *   studio-head --uart /dev/ttyAMA0  # datos reales del Teensy por el bridge (Pi 3B: PL011)
 *   studio-head --snapshot fx.ppm [--snapshot-ms N] [--page fx] [--fx-view signature] [--fx-slot 4]
 *   studio-head --snapshot boot.ppm --page boot --snapshot-ms 3100   # un cuadro del splash
 *               [--shift] [--hold] [--bypass] [--overlay vol|in|toast|banner|clip]
 *
 * Navegación en el demo: tocar la barra superior = NAV; B1–B4, MODE y B6 en la barra inferior.
 * En el simulador, además del touch: ← → NAV, Enter push, 1–4 B1–B4, M MODE, S SHIFT, H HOLD,
 * B bypass, E ENGINE SELECT, V vista de FX, − = VOL, [ ] IN, P PAD, L enlace perdido.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "hal/display.h"
#include "link/bridge_link.h"
#include "lvgl.h"
#include "sim/demo_source.h"
#include "state/studio_state.h"
#include "studio_tick.h"
#include "ui/screen_boot.h"
#include "ui/app.h"
#include "ui/input.h"

namespace {

constexpr uint32_t BOOT_MS = BootSplash::DURATION_MS;

void usage() {
    std::printf("studio-head [--touch <evdev>] [--uart <tty>] [--snapshot <archivo.ppm> [--snapshot-ms N]\n"
                "            [--page perform|ai|spectrum|env|fx|engine|presets|error|osc|lfo|sfx|panel|boot] [--shift]\n"
                "            [--engine 0-5] [--fx-view wave|spectrum|signature] [--fx-slot 1-12] [--hold] [--bypass]\n"
                "            [--overlay vol|in|toast|banner|clip]]\n");
}

Page parse_page(const char* p) {
    static const char* const NAMES[] = {"perform", "ai", "spectrum", "env", "fx", "engine", "presets", "error", "osc", "lfo", "sfx", "panel"};
    for (int i = 0; i < int(Page::Count); ++i)
        if (!std::strcmp(p, NAMES[i])) return Page(i);
    return Page::Perform;
}

/// Guarda la pantalla activa + la capa superior (overlays) como PPM (P6, RGB de 8 bits).
bool save_snapshot(const char* path) {
    lv_img_dsc_t* img = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
    if (!img) return false;
    // lv_snapshot no incluye lv_layer_top(): se captura aparte con alfa y se compone encima.
    lv_img_dsc_t* top = lv_snapshot_take(lv_layer_top(), LV_IMG_CF_TRUE_COLOR_ALPHA);
    FILE* f = std::fopen(path, "wb");
    if (!f) {
        lv_snapshot_free(img);
        if (top) lv_snapshot_free(top);
        return false;
    }
    const int w = img->header.w, h = img->header.h;
    const bool blend = top && top->header.w == w && top->header.h == h;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    const lv_color_t* px = reinterpret_cast<const lv_color_t*>(img->data);
    for (int i = 0; i < w * h; ++i) {
        lv_color_t c = px[i];
        if (blend) {
            const uint8_t* t = top->data + i * LV_IMG_PX_SIZE_ALPHA_BYTE;
            lv_color_t tc;
            std::memcpy(&tc, t, sizeof(tc));
            c = lv_color_mix(tc, c, t[LV_IMG_PX_SIZE_ALPHA_BYTE - 1]);
        }
        const uint32_t c32 = lv_color_to32(c);
        const unsigned char rgb[3] = {uint8_t(c32 >> 16), uint8_t(c32 >> 8), uint8_t(c32)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    lv_snapshot_free(img);
    if (top) lv_snapshot_free(top);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const char* touch = nullptr;
    const char* uart = nullptr;
    const char* snapshot = nullptr;
    uint32_t snapshot_ms = BOOT_MS + 1500;  // por defecto, ya pasado el splash
    bool boot_only = false;
    Page start_page = Page::Perform;
    bool start_shift = false, start_hold = false, start_bypass = false;
    int start_cursor = -1;   // --cursor: parada inicial (OSC, LFO / MOD, SYNTH FX) para capturas
    int fx_view = -1, fx_slot = -1, engine = -1;
    const char* overlay = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--touch") == 0 && i + 1 < argc) {
            touch = argv[++i];
        } else if (std::strcmp(argv[i], "--uart") == 0 && i + 1 < argc) {
            uart = argv[++i];
        } else if (std::strcmp(argv[i], "--snapshot") == 0 && i + 1 < argc) {
            snapshot = argv[++i];
        } else if (std::strcmp(argv[i], "--snapshot-ms") == 0 && i + 1 < argc) {
            snapshot_ms = uint32_t(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--page") == 0 && i + 1 < argc) {
            ++i;
            boot_only = !std::strcmp(argv[i], "boot");
            start_page = parse_page(argv[i]);
        } else if (std::strcmp(argv[i], "--cursor") == 0 && i + 1 < argc) {
            start_cursor = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--shift") == 0) {
            start_shift = true;
        } else if (std::strcmp(argv[i], "--hold") == 0) {
            start_hold = true;
        } else if (std::strcmp(argv[i], "--bypass") == 0) {
            start_bypass = true;
        } else if (std::strcmp(argv[i], "--fx-view") == 0 && i + 1 < argc) {
            const char* v = argv[++i];
            fx_view = !std::strcmp(v, "spectrum") ? 1 : !std::strcmp(v, "signature") ? 2 : 0;
        } else if (std::strcmp(argv[i], "--engine") == 0 && i + 1 < argc) {
            engine = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--fx-slot") == 0 && i + 1 < argc) {
            fx_slot = std::atoi(argv[++i]) - 1;
        } else if (std::strcmp(argv[i], "--overlay") == 0 && i + 1 < argc) {
            overlay = argv[++i];
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

    input_keyboard_init();
    BootSplash splash;
    lv_scr_load(splash.create());

    StudioState state;
    demo_init(state);
    state.shift = start_shift;
    if (start_cursor >= 0) state.ui_cursor = uint8_t(start_cursor);
    if (start_page == Page::Fx) state.mode = Mode::Fx;
    if (start_page == Page::Error) state.link = LinkState::Lost;
    state.fx_hold = start_hold;
    if (fx_view >= 0) state.fx_view = FxView(fx_view);
    if (fx_slot >= 0 && fx_slot < FX_SLOTS) state.fx_slot = uint8_t(fx_slot);
    if (engine >= 0 && engine < 6) state.engine = uint8_t(engine);
    if (start_bypass) {
        if (state.mode == Mode::Fx) state.fx_on &= uint16_t(~(1u << state.fx_slot));
        else state.filter_bypass = true;
    }
    // Con el Teensy, el demo solo deja los valores iniciales (presets, FX): lo demás llega por el bridge.
    BridgeLink link;
    const bool use_link = uart && link.start(uart);
    if (uart && !use_link) std::fprintf(stderr, "studio-head: no se pudo abrir %s, sigo en modo demo\n", uart);
    if (use_link) state.link = LinkState::Connecting;

    App app;
    bool booted = false;
    const uint32_t t0 = studio_tick_ms();

    for (;;) {
        const uint32_t now = studio_tick_ms();
        if (use_link) link.apply(state);
        else demo_tick(state, now);

        if (!booted) splash.update(now - t0);
        if (!booted && !boot_only && now - t0 >= BOOT_MS) {
            lv_obj_t* old = lv_scr_act();
            app.create();
            app.show(start_page == Page::Error ? Page::Perform : start_page);
            lv_obj_del(old);
            booted = true;
            Ctl drop;
            while (input_poll(drop)) {}  // el splash no recibe input (08g): se descarta lo tocado antes de READY
            if (overlay) {
                const Overlay o = !std::strcmp(overlay, "in") ? Overlay::Input : !std::strcmp(overlay, "toast") ? Overlay::Bypass
                                : !std::strcmp(overlay, "banner") ? Overlay::ModeBanner : !std::strcmp(overlay, "clip") ? Overlay::Clip
                                : Overlay::Volume;
                app.update(state);  // toma los valores iniciales antes de forzar
                app.overlays().force(o);
            }
        }
        if (booted) {
            // Con el Teensy conectado por UART, los gestos táctiles viajan como GF_CMD_UI_INPUT (0x45, theory/04 §5.2).
            Ctl c;
            while (input_poll(c)) {
                if (use_link) link.send_ui_input(c, state.shift);
                else app.handle(c, state);
            }
            app.update(state);
        }

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
