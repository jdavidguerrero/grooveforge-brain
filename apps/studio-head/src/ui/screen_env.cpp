#include "ui/screen_env.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/theme.h"

using namespace gf;

namespace {

constexpr int GX = 16, GY = 48, GW = 768, GH = 208;
constexpr int X0 = 24, X1 = 744, Y_BASE = 184, Y_TOP = 24;
constexpr int SUSTAIN_W = 192;   // ancho fijo del tramo de sustain
constexpr int MIN_SEG = 24;
constexpr uint32_t AREA = 0x0F1E1A;

lv_color_t canvas_buf[GW * GH];

const char* const ENGINE_NAMES[6] = {"MODEL D", "JUNO-106", "PROPHET-5", "OB-6", "DX7", "ARP 2600"};
const char* const CELL_NAMES[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
const char* const CELL_TAGS[4] = {"P1", "P2", "SHIFT P1", "SHIFT P2"};

void fmt_ms(float ms, char* out, size_t n) {
    if (ms >= 1000.0f) std::snprintf(out, n, "%.1f", ms / 1000.0f);
    else std::snprintf(out, n, "%d", int(std::lround(ms)));
}

}  // namespace

lv_obj_t* ScreenEnv::create() {
    _scr = screen_root();
    _top.create(_scr, "ENV", -1);

    _canvas = lv_canvas_create(_scr);
    lv_canvas_set_buffer(_canvas, canvas_buf, GW, GH, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(_canvas, GX, GY);

    static const char* const SEG[4] = {"A", "D", "S", "R"};
    for (int i = 0; i < 4; ++i) {
        _seg_lbl[i] = label(_scr, T12B, MON, SEG[i]);
        lv_obj_set_y(_seg_lbl[i], 58);
    }

    for (int i = 0; i < 4; ++i) {
        Cell& c = _cells[i];
        c.box = rect(_scr, 16 + i * 194, 264, 186, 112, SURFACE);
        lv_obj_set_style_outline_color(c.box, lv_color_hex(WHITE), 0);
        lv_obj_set_style_outline_width(c.box, 2, 0);
        lv_obj_set_style_outline_pad(c.box, -2, 0);
        c.top = rect(c.box, 0, 0, 186, 4, LINE);
        c.lbl = label(c.box, T12B, MON, CELL_NAMES[i]);
        lv_obj_set_style_text_letter_space(c.lbl, 1, 0);
        lv_obj_set_pos(c.lbl, 16, 18);
        c.tag_box = lv_obj_create(c.box);
        lv_obj_remove_style_all(c.tag_box);
        lv_obj_set_size(c.tag_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(c.tag_box, 6, 0);
        lv_obj_set_style_pad_ver(c.tag_box, 2, 0);
        lv_obj_set_style_border_width(c.tag_box, 1, 0);
        lv_obj_set_style_bg_opa(c.tag_box, LV_OPA_COVER, 0);
        c.tag = label(c.tag_box, T12B, MON, CELL_TAGS[i]);
        lv_obj_align(c.tag_box, LV_ALIGN_TOP_RIGHT, -16, 16);
        c.val = label(c.box, T40, GRAY, "");
        lv_obj_set_pos(c.val, 16, 50);
        c.unit = label(c.box, T12, MON, "");
    }

    _bottom.create(_scr);
    return _scr;
}

void ScreenEnv::draw_graph(const StudioState& s) {
    const bool sh = s.shift;
    lv_canvas_fill_bg(_canvas, lv_color_hex(SURFACE), LV_OPA_COVER);

    // Reparto horizontal: sustain fijo; attack, decay y release proporcionales a su tiempo.
    const float a = std::max(s.env_attack_ms, 1.0f), d = std::max(s.env_decay_ms, 1.0f),
                r = std::max(s.env_release_ms, 1.0f);
    const float free_w = float(X1 - X0 - SUSTAIN_W - 3 * MIN_SEG);
    const float tot = a + d + r;
    const int xa = X0 + MIN_SEG + int(free_w * a / tot);
    const int xd = xa + MIN_SEG + int(free_w * d / tot);
    const int xs = xd + SUSTAIN_W;
    const int ys = Y_BASE - int(std::lround(std::clamp(s.env_sustain, 0.0f, 1.0f) * (Y_BASE - Y_TOP)));

    // Curva como polilínea: attack recto; decay y release con caída exponencial.
    constexpr int N = 16;
    lv_point_t pts[2 + N + 1 + N];
    int k = 0;
    pts[k++] = {X0, Y_BASE};
    pts[k++] = {lv_coord_t(xa), Y_TOP};
    for (int i = 1; i <= N; ++i) {
        const float t = float(i) / N, e = 1.0f - std::exp(-4.0f * t);
        pts[k++] = {lv_coord_t(xa + (xd - xa) * t), lv_coord_t(Y_TOP + (ys - Y_TOP) * e / (1.0f - std::exp(-4.0f)))};
    }
    pts[k++] = {lv_coord_t(xs), lv_coord_t(ys)};
    for (int i = 1; i <= N; ++i) {
        const float t = float(i) / N, e = 1.0f - std::exp(-4.0f * t);
        pts[k++] = {lv_coord_t(xs + (X1 - xs) * t), lv_coord_t(ys + (Y_BASE - ys) * e / (1.0f - std::exp(-4.0f)))};
    }

    // Relleno bajo la curva: trapecios convexos por tramo.
    lv_draw_rect_dsc_t area;
    lv_draw_rect_dsc_init(&area);
    area.bg_color = lv_color_hex(AREA);
    for (int i = 0; i + 1 < k; ++i) {
        const lv_point_t q[4] = {pts[i], pts[i + 1], {pts[i + 1].x, Y_BASE}, {pts[i].x, Y_BASE}};
        lv_canvas_draw_polygon(_canvas, q, 4, &area);
    }

    // Grilla: línea base y divisiones punteadas entre etapas.
    lv_draw_line_dsc_t grid;
    lv_draw_line_dsc_init(&grid);
    grid.color = lv_color_hex(LINE);
    grid.width = 1;
    const lv_point_t base[2] = {{X0, Y_BASE}, {X1, Y_BASE}};
    lv_canvas_draw_line(_canvas, base, 2, &grid);
    grid.dash_width = 4;
    grid.dash_gap = 4;
    for (int x : {xa, xd, xs}) {
        const lv_point_t v[2] = {{lv_coord_t(x), 16}, {lv_coord_t(x), 192}};
        lv_canvas_draw_line(_canvas, v, 2, &grid);
    }

    // Tramos: los que se editan en 4 px TEAL_PLUS, el resto en 2 px TEAL.
    lv_draw_line_dsc_t seg;
    lv_draw_line_dsc_init(&seg);
    auto stroke = [&](int from, int to, bool on) {
        seg.color = lv_color_hex(on ? TEAL_PLUS : TEAL);
        seg.width = on ? 4 : 2;
        lv_canvas_draw_line(_canvas, &pts[from], to - from + 1, &seg);
    };
    const int i_a = 1, i_d = 1 + N, i_s = i_d + 1;
    stroke(0, i_a, !sh);
    stroke(i_a, i_d, !sh);
    stroke(i_d, i_s, sh);
    stroke(i_s, k - 1, sh);

    // Nodos 8×8
    lv_draw_rect_dsc_t node;
    lv_draw_rect_dsc_init(&node);
    const lv_point_t nodes[5] = {pts[0], pts[i_a], pts[i_d], pts[i_s], pts[k - 1]};
    for (int i = 0; i < 5; ++i) {
        node.bg_color = lv_color_hex((sh ? i >= 2 : i <= 2) ? WHITE : TEAL);
        lv_canvas_draw_rect(_canvas, nodes[i].x - 4, nodes[i].y - 4, 8, 8, &node);
    }

    // Borde del recuadro
    lv_draw_rect_dsc_t border;
    lv_draw_rect_dsc_init(&border);
    border.bg_opa = LV_OPA_TRANSP;
    border.border_color = lv_color_hex(LINE);
    border.border_width = 1;
    lv_canvas_draw_rect(_canvas, 0, 0, GW, GH, &border);

    // Letras de etapa centradas en su tramo
    const int centers[4] = {(X0 + xa) / 2, (xa + xd) / 2, (xd + xs) / 2, (xs + X1) / 2};
    for (int i = 0; i < 4; ++i) {
        lv_obj_set_x(_seg_lbl[i], GX + centers[i] - 4);
        set_text_color(_seg_lbl[i], (sh ? i >= 2 : i < 2) ? TEAL_PLUS : MON);
    }
}

void ScreenEnv::paint_cell(Cell& c, bool on, bool nav, const char* val, const char* unit) {
    set_bg(c.box, on ? TEAL_DIM : SURFACE);
    set_bg(c.top, on ? TEAL_PLUS : LINE);
    lv_obj_set_style_outline_opa(c.box, nav ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    set_text_color(c.lbl, on ? WHITE : MON);
    set_bg(c.tag_box, on ? TEAL_PLUS : (on ? TEAL_PLUS : SURFACE));
    lv_obj_set_style_bg_opa(c.tag_box, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(c.tag_box, lv_color_hex(on ? TEAL_PLUS : MON), 0);
    set_text_color(c.tag, on ? BG : MON);
    lv_label_set_text(c.val, val);
    set_text_color(c.val, on ? WHITE : GRAY);
    lv_label_set_text(c.unit, unit);
    set_text_color(c.unit, on ? WHITE : MON);
    lv_obj_align_to(c.unit, c.val, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -6);
}

void ScreenEnv::update(const StudioState& s) {
    _top.update(s);
    if (s.engine != _last_engine && s.engine < 6) {
        _last_engine = s.engine;
        char t[24];
        std::snprintf(t, sizeof(t), "ENV \xC2\xB7 %s", ENGINE_NAMES[s.engine]);
        _top.set_page(t);
    }

    char a[12], d[12], sus[12], r[12];
    fmt_ms(s.env_attack_ms, a, sizeof(a));
    fmt_ms(s.env_decay_ms, d, sizeof(d));
    std::snprintf(sus, sizeof(sus), "%d", int(std::lround(s.env_sustain * 100.0f)));
    fmt_ms(s.env_release_ms, r, sizeof(r));
    const char* ua = s.env_attack_ms >= 1000.0f ? "S" : "MS";
    const char* ud = s.env_decay_ms >= 1000.0f ? "S" : "MS";
    const char* ur = s.env_release_ms >= 1000.0f ? "S" : "MS";

    char key[64];
    std::snprintf(key, sizeof(key), "%s|%s|%s|%s|%d", a, d, sus, r, s.shift);
    if (std::strcmp(key, _last_key) != 0) {
        std::snprintf(_last_key, sizeof(_last_key), "%s", key);
        draw_graph(s);
        const bool sh = s.shift;
        paint_cell(_cells[0], !sh, false, a, ua);
        paint_cell(_cells[1], !sh, !sh, d, ud);
        paint_cell(_cells[2], sh, false, sus, "%");
        paint_cell(_cells[3], sh, sh, r, ur);
    }

    _bottom.set_mode(s.mode == Mode::Fx, s.shift);
    static const char* const KEYS[4] = {"OSC", "ENV", "LFO / MOD", "PRESET"};
    _bottom.set_keys(KEYS, 1);
    char v[16];
    std::snprintf(v, sizeof(v), "%d", int(std::lround(s.cutoff_hz)));
    _bottom.set_enc(false, "CUTOFF", v, "HZ", std::clamp(std::log(s.cutoff_hz / 20.0f) / std::log(1000.0f), 0.0f, 1.0f));
    std::snprintf(v, sizeof(v), "%d", int(std::lround(s.resonance * 100.0f)));
    _bottom.set_enc(true, "RES", v, "%", s.resonance);
    if (s.shift) {
        _bottom.set_param(1, "SUSTAIN", sus, "%", s.env_sustain);
        _bottom.set_param(2, "RELEASE", r, ur, s.env_release_ms / 1200.0f);
    } else {
        _bottom.set_param(1, "ATTACK", a, ua, s.env_attack_ms / 200.0f);
        _bottom.set_param(2, "DECAY", d, ud, s.env_decay_ms / 1200.0f);
    }
    _bottom.set_nav("HIGHLIGHT");
}
