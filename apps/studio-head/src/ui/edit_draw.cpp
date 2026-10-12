#include "ui/edit_draw.h"

#include <algorithm>
#include <cmath>

#include "ui/theme.h"

using namespace gf;

namespace {

float noise_at(int i) {
    const double s = std::sin(i * 12.9898 + 4.1) * 43758.5453;
    return float(s - std::floor(s));
}

int font_px(const lv_font_t* f) {
    if (f == T12 || f == T12B) return 12;
    if (f == T16 || f == T16R) return 16;
    if (f == T20) return 20;
    if (f == T28) return 28;
    if (f == T40) return 40;
    return 64;
}

}  // namespace

float wave_value(Wave w, float p) {
    static const float SH[8] = {0.6f, -0.35f, 0.9f, -0.7f, 0.15f, -0.9f, 0.45f, -0.1f};
    if (w == Wave::SampleHold) return SH[((int(std::floor(p)) % 8) + 8) % 8];
    const float ph = p - std::floor(p);
    switch (w) {
        case Wave::Saw:   return 2 * ph - 1;
        case Wave::Tri:   return ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph;
        case Wave::Sqr:   return ph < 0.5f ? 1.0f : -1.0f;
        case Wave::Pulse: return ph < 0.25f ? 1.0f : -1.0f;
        default:          return std::sin(2 * float(M_PI) * ph);
    }
}

void draw_trace(const Painter& p, Wave w, int x0, int cy, int width, int amp, float cyc, int off,
                uint32_t color, int th, int st) {
    int py = INT32_MIN;
    for (int x = 0; x < width; x += st) {
        const float v = w == Wave::Noise ? noise_at(x * 7 + cy) * 2 - 1 : wave_value(w, (x - off) / (width / cyc));
        const int y = int(std::lround(cy - v * amp));
        const int a = py == INT32_MIN ? y : std::min(py, y), b = py == INT32_MIN ? y : std::max(py, y);
        p.fill(x0 + x, a - (th >> 1), st, b - a + th, color);
        py = y;
    }
}

void draw_wave_icon(const Painter& p, Wave w, int x, int y, uint32_t color) {
    const float cyc = w == Wave::SampleHold ? 4 : (w == Wave::Tri || w == Wave::Sine) ? 1 : 2;
    draw_trace(p, w, x, y + 8, 24, 5, cyc, 0, color, 2, 1);
}

void draw_box(const Painter& p, int x, int y, int w, int h, uint32_t bc, uint32_t fc, int b) {
    p.fill(x, y, w, h, bc);
    if (fc != bc) p.fill(x + b, y + b, w - 2 * b, h - 2 * b, fc);
}

void text_top(const Painter& p, int x, int y_top, const char* s, uint32_t color, const lv_font_t* font,
              lv_text_align_t align) {
    // Con line-height = tamaño, la línea base de IBM Plex Mono cae a 0.875 em del borde superior
    // (ascent 1.025 em − medio interlineado de 0.15 em).
    p.text(x, y_top + std::lround(font_px(font) * 0.875f), s, color, font, align);
}
