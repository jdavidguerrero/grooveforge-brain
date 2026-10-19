#include "ui/screen_env.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/engines.h"
#include "ui/theme.h"

using namespace gf;

namespace {

/// El envelope que muestra la página: NAV elige AMP (cursor 0) o FILTER (cursor 1).
struct EnvView { float a, d, s, r; bool filter; };
EnvView env_view(const StudioState& st) {
    const bool f = st.ui_cursor == 1;
    return f ? EnvView{st.env_attack_ms, st.env_decay_ms, st.env_sustain, st.env_release_ms, true}
             : EnvView{st.amp_attack_ms, st.amp_decay_ms, st.amp_sustain, st.amp_release_ms, false};
}


constexpr int GX = 16, GY = 48, GW = 768, GH = 208;
constexpr int X0 = 24, X1 = 744, Y_BASE = 184, Y_TOP = 24;
constexpr int SUSTAIN_W = 192;   // ancho fijo del tramo de sustain
constexpr int MIN_SEG = 24;
constexpr uint32_t AREA = 0x0F1E1A;

lv_color_t canvas_buf[GW * GH];

const char* const CELL_NAMES[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
const char* const CELL_TAGS[4] = {"P1", "P2", "ENC L", "ENC R"};

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
    const EnvView ev = env_view(s);
    const bool sh = s.shift;
    lv_canvas_fill_bg(_canvas, lv_color_hex(SURFACE), LV_OPA_COVER);

    // Reparto horizontal: sustain fijo; attack, decay y release proporcionales a su tiempo.
    const float a = std::max(ev.a, 1.0f), d = std::max(ev.d, 1.0f),
                r = std::max(ev.r, 1.0f);
    const float free_w = float(X1 - X0 - SUSTAIN_W - 3 * MIN_SEG);
    const float tot = a + d + r;
    const int xa = X0 + MIN_SEG + int(free_w * a / tot);
    const int xd = xa + MIN_SEG + int(free_w * d / tot);
    const int xs = xd + SUSTAIN_W;
    const int ys = Y_BASE - int(std::lround(std::clamp(ev.s, 0.0f, 1.0f) * (Y_BASE - Y_TOP)));

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
    const EnvView ev = env_view(s);
    _top.update(s);
    if ((s.engine != _last_engine || ev.filter != _last_filter) && s.engine < 6) {
        _last_engine = s.engine;
        _last_filter = ev.filter;
        char t[40];
        std::snprintf(t, sizeof(t), "ENV \xC2\xB7 %s \xC2\xB7 %s", ev.filter ? "FILTER" : "AMP", engine_info(s.engine).name);
        _top.set_page(t);
    }

    char a[12], d[12], sus[12], r[12];
    fmt_ms(ev.a, a, sizeof(a));
    fmt_ms(ev.d, d, sizeof(d));
    std::snprintf(sus, sizeof(sus), "%d", int(std::lround(ev.s * 100.0f)));
    fmt_ms(ev.r, r, sizeof(r));
    const char* ua = ev.a >= 1000.0f ? "S" : "MS";
    const char* ud = ev.d >= 1000.0f ? "S" : "MS";
    const char* ur = ev.r >= 1000.0f ? "S" : "MS";

    char key[64];
    std::snprintf(key, sizeof(key), "%s|%s|%s|%s|%d|%d", a, d, sus, r, s.shift, ev.filter);
    if (std::strcmp(key, _last_key) != 0) {
        std::snprintf(_last_key, sizeof(_last_key), "%s", key);
        draw_graph(s);
        const bool sh = s.shift;
        paint_cell(_cells[0], !sh, false, a, ua);       // los 4 encoders editan A · D · S · R; SHIFT: capa vacía
        paint_cell(_cells[1], !sh, false, d, ud);
        paint_cell(_cells[2], !sh, false, sus, "%");
        paint_cell(_cells[3], !sh, false, r, ur);
    }

    _bottom.set_mode(s.mode == Mode::Fx, s.shift);
    static const char* const KEYS[4] = {"OSC", "ENV", "LFO / MOD", "SYNTH FX"};
    _bottom.set_keys(KEYS, 1);
    _bottom.set_page_control(0, "ATTACK", a, ua, ev.a / 200.0f);
    _bottom.set_page_control(1, "DECAY", d, ud, ev.d / 1200.0f);
    _bottom.set_page_control(2, "SUSTAIN", sus, "%", ev.s);
    _bottom.set_page_control(3, "RELEASE", r, ur, ev.r / 1200.0f);
    _bottom.set_nav(ev.filter ? "AMP \xE2\x80\xB9 FILTER" : "AMP \xE2\x80\xBA FILTER");
}
