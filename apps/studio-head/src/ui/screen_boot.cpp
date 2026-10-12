#include "ui/screen_boot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/screen.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr uint32_t OFF = 0x1C1C1C;
constexpr int BASE_Y = 200;
const char WORD[] = "GROOVEFORGE";

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float ease(float v) { return 1 - std::pow(1 - clamp01(v), 3.0f); }

void show(lv_obj_t* o, bool on) {
    if (on == !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

lv_obj_t* BootSplash::create() {
    _scr = screen_root();

    _line = rect(_scr, 400, BASE_Y, 0, 2, TEAL_PLUS);
    for (int i = 0; i < 32; ++i) {
        _bars[i] = rect(_scr, 18 + i * 24, BASE_Y, 20, 0, TEAL);
        _bar_c[i] = TEAL;
    }

    // Marca: 11 celdas de 26 px desde x 257; la caja de línea del diseño es de 40 px.
    const int ly = 216 - (T40->line_height - 40) / 2;
    for (int i = 0; i < 11; ++i) {
        const char ch[2] = {WORD[i], '\0'};
        _letters[i] = label(_scr, T40, WHITE, ch);
        lv_obj_set_width(_letters[i], 26);
        lv_obj_set_style_text_align(_letters[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(_letters[i], 257 + i * 26, ly);
        lv_obj_add_flag(_letters[i], LV_OBJ_FLAG_HIDDEN);
    }
    _cursor = rect(_scr, 258, 216, 24, 40, TEAL_PLUS);
    lv_obj_add_flag(_cursor, LV_OBJ_FLAG_HIDDEN);

    _sub = label(_scr, T16R, TEAL_PLUS, "BRAIN · STUDIO");
    lv_obj_set_style_text_letter_space(_sub, 4, 0);
    lv_obj_align(_sub, LV_ALIGN_TOP_MID, 0, 268 - (T16R->line_height - 16) / 2);
    lv_obj_set_style_text_opa(_sub, LV_OPA_TRANSP, 0);

    // Fila de engines: la miniatura se "enciende" bajando un velo BG de opacidad 85 % a 0.
    _row = lv_obj_create(_scr);
    lv_obj_remove_style_all(_row);
    lv_obj_clear_flag(_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(_row, 20, 296);
    lv_obj_set_size(_row, 6 * 128, 110);
    for (int i = 0; i < ENGINE_COUNT; ++i) {
        Thumb& t = _thumbs[i];
        const int x = i * 128;
        t.art = engine_art_create(_row, x, 0, 120, 80, uint8_t(i));
        t.veil = rect(_row, x, 0, 120, 80, BG);
        lv_obj_set_style_bg_opa(t.veil, LV_OPA_COVER * 85 / 100, 0);
        t.bar = rect(_row, x, 84, 120, 4, LINE);
        t.name = label(_row, T12, MON, engine_info(uint8_t(i)).name);
        lv_obj_set_pos(t.name, x, 94 - (T12->line_height - 12) / 2);
        _thumb_state[i] = -1;
    }
    lv_obj_add_flag(_row, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < 32; ++i) {
        _segs[i] = rect(_scr, 18 + i * 24, 420, 20, 8, OFF);
        _seg_c[i] = OFF;
        lv_obj_add_flag(_segs[i], LV_OBJ_FLAG_HIDDEN);
    }
    _status = label(_scr, T12, GRAY, "");
    lv_obj_set_style_text_letter_space(_status, 1, 0);
    lv_obj_set_pos(_status, 18, 444 - (T12->line_height - 12) / 2);
    _pct = label(_scr, T16, WHITE, "");
    lv_obj_align(_pct, LV_ALIGN_TOP_RIGHT, -18, 440 - (T16->line_height - 16) / 2);

    _fade = rect(_scr, 0, 0, 800, 480, BG);
    lv_obj_set_style_bg_opa(_fade, LV_OPA_TRANSP, 0);
    return _scr;
}

void BootSplash::update(uint32_t ms) {
    const float t = ms / 1000.0f;
    const bool flash = t >= 4.6f && t < 4.75f;

    // 0.0–0.4 s: la línea base crece desde el centro.
    const int lw = int(std::lround(768 * ease(t / 0.4f) / 2)) * 2;
    if (lw != _line_w) {
        _line_w = lw;
        lv_obj_set_x(_line, 400 - lw / 2);
        lv_obj_set_width(_line, lw);
    }

    // 0.4–1.4 s: barrido con pico resonante de izquierda a derecha; después, barras en reposo.
    // 4.6–5.1 s: READY, pulso desde el centro con 2 frames de destello blanco.
    const float front = (t - 0.4f) / 1.0f * 36 - 2;
    for (int i = 0; i < 32; ++i) {
        const float idle = 8 + 30 * std::exp(-std::pow((i - 8) / 10.0f, 2.0f)) + 6 * (0.5f + 0.5f * std::sin(i * 1.9f + t * 5));
        float h = 0;
        uint32_t c = TEAL;
        if (t >= 0.4f && t < 1.4f) {
            if (i <= front) {
                const float pk = 110 * std::exp(-std::pow(i - front, 2.0f) / 3);
                h = idle + pk;
                c = std::fabs(i - front) < 1 ? WHITE : pk > 20 ? TEAL_PLUS : TEAL;
            }
        } else if (t >= 1.4f) {
            h = idle;
            if (t >= 4.6f && t < 5.1f) {
                const float k = std::sin(float(M_PI) * (t - 4.6f) / 0.5f) * std::max(0.0f, 1 - std::fabs(i - 15.5f) / 18);
                h += 100 * k;
                c = flash ? WHITE : k > 0.2f ? TEAL_PLUS : TEAL;
            }
        }
        const int hi = std::min(150, int(std::lround(h / 2)) * 2);
        if (hi != _bar_h[i]) {
            _bar_h[i] = int16_t(hi);
            lv_obj_set_y(_bars[i], BASE_Y - hi);
            lv_obj_set_height(_bars[i], hi);
        }
        if (c != _bar_c[i]) {
            _bar_c[i] = c;
            set_bg(_bars[i], c);
        }
    }

    // 1.0–1.7 s: la marca se tipea, una letra cada 60 ms, con cursor de bloque.
    const int n = std::clamp(int(std::floor((t - 1.0f) / 0.06f)), 0, 11);
    if (n != _letters_n) {
        _letters_n = n;
        for (int i = 0; i < 11; ++i) show(_letters[i], i < n);
    }
    const bool cur_on = t >= 0.95f && n < 11 && int(std::floor(t * 10)) % 2 == 0;
    const int cur_at = cur_on ? n : -1;
    if (cur_at != _cursor_at) {
        _cursor_at = cur_at;
        show(_cursor, cur_on);
        if (cur_on) lv_obj_set_x(_cursor, 257 + n * 26 + 1);
    }
    const int sub_opa = int(clamp01((t - 1.75f) / 0.25f) * 255);
    if (sub_opa != _sub_opa) {
        _sub_opa = sub_opa;
        lv_obj_set_style_text_opa(_sub, lv_opa_t(sub_opa), 0);
    }

    // 1.8–4.6 s: engines que se encienden uno por uno y barra de carga.
    show(_row, t >= 1.8f);
    const bool blink = int(std::floor(t * 12)) % 2 == 0;
    for (int i = 0; i < ENGINE_COUNT; ++i) {
        const float s0 = 2.0f + i * 0.35f;
        const float k = clamp01((t - s0) / 0.35f);
        const int st = t < s0 ? 0 : t < s0 + 0.35f ? 1 : 2;
        const int veil = st == 0 ? 85 : st == 1 ? int(85 * (1 - k)) : 0;
        const uint32_t bc = st == 0 ? LINE : st == 1 ? (blink ? TEAL_PLUS : TEAL) : flash ? WHITE : TEAL;
        const int key = veil * 4 + (bc == TEAL_PLUS) * 2 + (bc == WHITE);
        if (key == _thumb_state[i] && st != 1) continue;
        _thumb_state[i] = key;
        Thumb& th = _thumbs[i];
        lv_obj_set_style_bg_opa(th.veil, lv_opa_t(LV_OPA_COVER * veil / 100), 0);
        set_bg(th.bar, bc);
        set_text_color(th.name, st == 0 ? MON : st == 1 ? WHITE : GRAY);
    }

    const float p = t < 1.8f ? 0 : t < 2.0f ? 0.08f * (t - 1.8f) / 0.2f : t < 4.1f ? 0.08f + 0.72f * (t - 2.0f) / 2.1f
                  : t < 4.6f ? 0.8f + 0.2f * (t - 4.1f) / 0.5f : 1.0f;
    const int lit = int(std::floor(p * 32));
    for (int i = 0; i < 32; ++i) {
        show(_segs[i], t >= 1.6f);
        const uint32_t c = flash ? WHITE : i < lit ? TEAL_PLUS : (i == lit && p > 0 && p < 1) ? WHITE : OFF;
        if (c != _seg_c[i]) {
            _seg_c[i] = c;
            set_bg(_segs[i], c);
        }
    }

    char st[48] = "";
    const int ei = std::clamp(int(std::floor((t - 2.0f) / 0.35f)), 0, ENGINE_COUNT - 1);
    if (t < 1.8f) st[0] = '\0';
    else if (t < 2.0f) std::snprintf(st, sizeof(st), "LINKING AUDIO ENGINE");
    else if (t < 4.1f) std::snprintf(st, sizeof(st), "LOADING ENGINE %d/6 · %s", ei + 1, engine_info(uint8_t(ei)).name);
    else if (t < 4.35f) std::snprintf(st, sizeof(st), "AI MODELS · KEY · CHORD · GENRE");
    else if (t < 4.6f) std::snprintf(st, sizeof(st), "FX RACK · 12 SLOTS");
    else std::snprintf(st, sizeof(st), "READY");
    if (std::strcmp(st, _last_status) != 0) {
        std::snprintf(_last_status, sizeof(_last_status), "%s", st);
        lv_label_set_text(_status, st);
        set_text_color(_status, t >= 4.6f ? TEAL_PLUS : GRAY);
    }
    char pc[16] = "";
    if (t >= 1.8f) std::snprintf(pc, sizeof(pc), "%d %%", int(std::lround(p * 100)));
    if (std::strcmp(pc, _last_pct) != 0) {
        std::snprintf(_last_pct, sizeof(_last_pct), "%s", pc);
        lv_label_set_text(_pct, pc);
        set_text_color(_pct, t >= 4.6f ? TEAL_PLUS : WHITE);
        lv_obj_align(_pct, LV_ALIGN_TOP_RIGHT, -18, 440 - (T16->line_height - 16) / 2);
    }

    // 5.7–6.1 s: fundido a BG antes de PERFORM.
    const int fade = t < 5.7f ? 0 : int(clamp01((t - 5.7f) / 0.4f) * 255);
    if (fade != _fade_opa) {
        _fade_opa = fade;
        lv_obj_set_style_bg_opa(_fade, lv_opa_t(fade), 0);
    }
}
