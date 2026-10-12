#include "ui/engines.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "ui/painter.h"
#include "ui/theme.h"

using namespace gf;

namespace {

const EngineInfo ENGINES[ENGINE_COUNT] = {
    {"MODEL D", "MONO · 3 VCO", "MONO · 3 VCO · LADDER FILTER"},
    {"JUNO-106", "POLY 6 · CHORUS", "POLY 6 VOICES · DCO · BBD CHORUS"},
    {"PROPHET-5", "POLY 5 · 2 VCO", "POLY 5 VOICES · 2 VCO PER VOICE"},
    {"OB-6", "POLY 6 · DUAL SAW", "POLY 6 VOICES · DUAL SAW"},
    {"DX7", "FM · 4-OP", "FM · 4 OPERATORS · POLY 6"},
    {"ARP 2600", "SEMI-MODULAR", "SEMI-MODULAR · PATCH MEMORY"},
};

constexpr uint32_t BODY = 0x161616, SHADOW = 0x050505, FLOOR = 0x0D0F0E, DARK = 0x0F0F0F;
constexpr float VB_W = 280.0f, VB_H = 180.0f;

/// Port 1:1 de illo(): mismas primitivas, en coordenadas del viewBox de 280×180.
class Art {
public:
    Art(const Painter& p, float s, float dx, float dy) : _p(p), _s(s), _dx(dx), _dy(dy) {}

    void rect(float x, float y, float w, float h, uint32_t f, int stroke = -1, float sw = 0) const {
        _p.fill(X(x), Y(y), w * _s, h * _s, f);
        if (stroke >= 0 && sw > 0) {  // trazo SVG centrado en el borde
            const float t = std::max(1.0f, sw * _s), o = sw / 2;
            _p.fill(X(x - o), Y(y - o), (w + sw) * _s, t, uint32_t(stroke));
            _p.fill(X(x - o), Y(y + h + o) - t, (w + sw) * _s, t, uint32_t(stroke));
            _p.fill(X(x - o), Y(y - o), t, (h + sw) * _s, uint32_t(stroke));
            _p.fill(X(x + w + o) - t, Y(y - o), t, (h + sw) * _s, uint32_t(stroke));
        }
    }
    void circ(float cx, float cy, float r, uint32_t f, int stroke = -1, float sw = 0) const {
        const bool st = stroke >= 0 && sw > 0;
        const float rr = (r + (st ? sw / 2 : 0)) * _s;
        _p.circle(X(cx), Y(cy), rr, f, st ? uint32_t(stroke) : f, st ? std::max(1, int(std::lround(sw * _s))) : 0);
    }
    void line(float x1, float y1, float x2, float y2, uint32_t c, float sw = 1) const {
        _p.line(X(x1), Y(y1), X(x2), Y(y2), std::max(1, int(std::lround(sw * _s))), c);
    }
    void quad(float x0, float y0, float cx, float cy, float x1, float y1, uint32_t c, float sw) const {
        float px = x0, py = y0;
        for (int i = 1; i <= 12; ++i) {
            const float t = i / 12.0f, u = 1 - t;
            const float nx = u * u * x0 + 2 * u * t * cx + t * t * x1, ny = u * u * y0 + 2 * u * t * cy + t * t * y1;
            line(px, py, nx, ny, c, sw);
            px = nx; py = ny;
        }
    }
    void knob(float cx, float cy, float r) {
        circ(cx, cy, r, BG, TEAL, 1.5f);
        const float a = float(((_ki++) * 67) % 240 - 120) * float(M_PI) / 180.0f;
        line(cx, cy, cx + std::sin(a) * (r - 1.5f), cy - std::cos(a) * (r - 1.5f), TEAL_PLUS, 1.5f);
    }
    void keys(float x, float y, float w, float h, int cnt) const {
        const float kw = w / cnt;
        for (int i = 0; i < cnt; ++i) rect(x + i * kw, y, kw, h, GRAY, int(BG), 1);
        static const bool PAT[7] = {true, true, false, true, true, true, false};
        for (int i = 0; i < cnt - 1; ++i)
            if (PAT[i % 7]) rect(x + (i + 1) * kw - kw * 0.3f, y, kw * 0.6f, h * 0.6f, BG);
    }
    void body(float x, float y, float w, float h) const {
        rect(x, y, w, h, BODY, int(TEAL_DIM), 2);
        line(x + 2, y + 2, x + w - 2, y + 2, TEAL, 2);
    }
    void fader(float x, float y, float h, float p) const {
        line(x, y, x, y + h, TEAL_DIM, 2);
        rect(x - 3.5f, y + (h - 6) * (1 - p), 7, 6, TEAL_PLUS);
    }
    void jack(float cx, float cy) const { circ(cx, cy, 2.5f, BG, GRAY, 1); }
    void shadow(float x, float w) const { rect(x, 151, w, 5, SHADOW); }

private:
    float X(float x) const { return _dx + x * _s; }
    float Y(float y) const { return _dy + y * _s; }

    const Painter& _p;
    float _s, _dx, _dy;
    int _ki = 0;
};

float pr(int i) {
    static const float P[8] = {0.3f, 0.7f, 0.5f, 0.85f, 0.4f, 0.6f, 0.2f, 0.75f};
    return P[i % 8];
}

void draw_engine(Art& a, uint8_t id) {
    a.rect(0, 0, 280, 180, BG);
    a.rect(0, 151, 280, 29, FLOOR);
    a.line(0, 151, 280, 151, TEAL_DIM, 1);

    switch (id) {
        case 0:  // Model D
            a.shadow(36, 208);
            a.rect(36, 44, 12, 107, TEAL_DIM);
            a.rect(232, 44, 12, 107, TEAL_DIM);
            a.body(48, 44, 184, 58);
            a.rect(48, 102, 184, 6, DARK);
            a.body(48, 108, 184, 43);
            a.keys(56, 116, 168, 35, 21);
            for (int r = 0; r < 3; ++r)
                for (int i = 0; i < 7; ++i) a.knob(66 + i * 25, 58 + r * 15, 5.5f);
            a.circ(226, 96, 2, TEAL_PLUS);
            break;
        case 1:  // Juno-106
            a.shadow(16, 248);
            a.body(16, 72, 248, 79);
            a.line(18, 110, 262, 110, TEAL_DIM, 1);
            for (int i = 0; i < 16; ++i) a.fader(30 + i * 12, 80, 24, pr(i));
            for (int i = 0; i < 6; ++i)
                a.rect(222 + (i % 2) * 18, 80 + (i / 2) * 9, 14, 6, i == 1 || i == 2 ? TEAL_PLUS : TEAL_DIM);
            a.keys(24, 116, 232, 35, 36);
            break;
        case 2:  // Prophet-5
            a.shadow(20, 240);
            a.rect(20, 76, 10, 75, TEAL_DIM);
            a.rect(250, 76, 10, 75, TEAL_DIM);
            a.body(30, 76, 220, 75);
            for (int r = 0; r < 2; ++r)
                for (int i = 0; i < 11; ++i) a.knob(45 + i * 19, 88 + r * 14, 5.5f);
            for (int i = 0; i < 12; ++i) a.rect(45 + i * 16.5f, 111, 10, 4, i == 3 ? TEAL_PLUS : TEAL);
            a.keys(36, 119, 208, 32, 29);
            break;
        case 3:  // OB-6
            a.shadow(28, 224);
            a.rect(40, 145, 16, 6, TEAL_DIM);
            a.rect(224, 145, 16, 6, TEAL_DIM);
            a.body(28, 60, 224, 85);
            for (float x0 : {40.0f, 64.0f}) {  // 'M40 76 l8 -8 v8 l8 -8 v8': dos dientes de sierra
                a.line(x0, 76, x0 + 8, 68, TEAL_PLUS, 2);
                a.line(x0 + 8, 68, x0 + 8, 76, TEAL_PLUS, 2);
                a.line(x0 + 8, 76, x0 + 16, 68, TEAL_PLUS, 2);
                a.line(x0 + 16, 68, x0 + 16, 76, TEAL_PLUS, 2);
            }
            for (int r = 0; r < 2; ++r)
                for (int i = 0; i < 6; ++i) a.knob(52 + i * 35, 94 + r * 24, 9.5f);
            for (int i = 0; i < 9; ++i) a.rect(44 + i * 22, 133, 14, 6, i == 0 ? TEAL_PLUS : TEAL_DIM);
            break;
        case 4:  // DX7
            a.shadow(16, 248);
            a.body(16, 92, 248, 59);
            a.rect(28, 100, 52, 14, TEAL_DIM);
            a.line(32, 105, 66, 105, TEAL_PLUS, 1.5f);
            a.line(32, 110, 56, 110, TEAL_PLUS, 1.5f);
            for (int r = 0; r < 2; ++r)
                for (int i = 0; i < 16; ++i)
                    a.rect(88 + i * 10.4f, 99 + r * 9, 8, 5,
                           r == 0 ? (i % 4 == 3 ? PURPLE_BRIGHT : TEAL_DIM) : (i % 4 == 3 ? TEAL_PLUS : TEAL));
            a.keys(24, 120, 232, 31, 32);
            break;
        case 5:  // ARP 2600
            a.shadow(36, 208);
            a.body(76, 8, 128, 104);
            for (int r = 0; r < 2; ++r)
                for (int i = 0; i < 8; ++i) a.fader(90 + i * 14.5f, 18 + r * 26, 20, pr(i + r * 3));
            for (int i = 0; i < 9; ++i) { a.jack(92 + i * 12, 70); a.jack(92 + i * 12, 80); }
            a.circ(96, 98, 8, DARK, TEAL_DIM, 1.5f);
            a.circ(184, 98, 8, DARK, TEAL_DIM, 1.5f);
            a.quad(104, 70, 124, 104, 152, 80, PURPLE_BRIGHT, 2);
            a.quad(128, 70, 160, 98, 188, 70, TEAL_PLUS, 2);
            a.line(100, 112, 100, 124, TEAL_DIM, 3);
            a.line(180, 112, 180, 124, TEAL_DIM, 3);
            a.body(36, 124, 208, 27);
            a.keys(44, 129, 192, 22, 24);
            break;
        default:
            break;
    }
}

void on_draw(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    const uint8_t id = uint8_t(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(obj)));
    const float w = lv_obj_get_width(obj), h = lv_obj_get_height(obj);
    const float s = std::max(w / VB_W, h / VB_H);
    const Painter p = Painter::from(e);
    Art a(p, s, (w - VB_W * s) / 2, (h - VB_H * s) / 2);
    draw_engine(a, id);
}

}  // namespace

const EngineInfo& engine_info(uint8_t id) {
    return ENGINES[id < ENGINE_COUNT ? id : 0];
}

lv_obj_t* engine_art_create(lv_obj_t* parent, int x, int y, int w, int h, uint8_t engine) {
    lv_obj_t* o = draw_area(parent, x, y, w, h, on_draw, nullptr);
    lv_obj_set_user_data(o, reinterpret_cast<void*>(uintptr_t(engine)));
    return o;
}

void engine_art_set(lv_obj_t* art, uint8_t engine) {
    if (uintptr_t(lv_obj_get_user_data(art)) == engine) return;
    lv_obj_set_user_data(art, reinterpret_cast<void*>(uintptr_t(engine)));
    lv_obj_invalidate(art);
}
