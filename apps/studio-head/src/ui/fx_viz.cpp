#include "ui/fx_viz.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "state/fx_catalog.h"
#include "ui/theme.h"

using namespace gf;

namespace {

constexpr float TWO_PI = 6.2831853f;

/// Colores de la WET y de los indicadores vivos según el estado (FX Viz §A).
struct Pal {
    uint32_t pb;    ///< WET principal (PURPLE_BRIGHT; PURPLE_DIM en bypass)
    uint32_t p;     ///< WET secundaria (PURPLE)
    uint32_t live;  ///< punto / cabezal (HERO_WHITE; MONITOR_GRAY en bypass)
};

Pal palette(VizState st) {
    return st == VizState::Bypass ? Pal{PURPLE_DIM, PURPLE_DIM, MON} : Pal{PURPLE_BRIGHT, PURPLE, WHITE};
}

struct LegendItem { uint32_t c; const char* l; bool line; };

/// Leyenda de la franja superior (y 0–24): muestras de 10×10 o 14×2 y texto en baseline 17.
void legend(const Painter& p, std::initializer_list<LegendItem> items, const char* right) {
    int px = 8;
    for (const LegendItem& it : items) {
        if (it.line) p.fill(px, 11, 14, 2, it.c);
        else p.fill(px, 7, 10, 10, it.c);
        px += it.line ? 20 : 16;
        p.text(px, 17, it.l, GRAY, T12);
        px += text_width(it.l, T12) + 16;
    }
    if (right) p.text(VIZ_W - 8, 17, right, MON, T12, LV_TEXT_ALIGN_RIGHT);
}

void db_text(float v, char* out, size_t n) {
    const float d = 20 * std::log10(std::max(v, 1e-4f));
    std::snprintf(out, n, "%s%.1f", d < 0 ? "−" : "+", double(std::fabs(d)));
}

const char* note_name(float f, char* out, size_t n) {
    static const char* const NM[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int m = int(std::lround(12 * std::log2(f / 440.0f))) + 69;
    std::snprintf(out, n, "%s%d", NM[((m % 12) + 12) % 12], m / 12 - 1);
    return out;
}

float band_of(float f) { return 31.0f * std::log(f / 20.0f) / std::log(1000.0f); }

// ── C1 · Osciloscopio dual ───────────────────────────────────────────────────

void draw_scope(const Painter& p, const StudioState& s, VizState st) {
    constexpr int CY = 136, A = 104;
    constexpr float PITCH = 720.0f / FX_COLS;
    const int wide = std::max(2, int(std::lround(PITCH - 1)));
    const int thin = std::max(1, int(std::lround(wide * 0.6f)));
    const int off = (wide - thin) / 2;
    const bool byp = st == VizState::Bypass;

    p.fill(4, CY, 720, 1, LINE);
    p.fill(4, CY - A, 720, 1, LINE);
    p.fill(4, CY + A, 720, 1, LINE);
    for (int i = 1; i < 4; ++i) p.fill(4 + i * 180, 24, 1, 224, LINE);

    const FxStream& f = s.fx;
    for (int i = 0; i < FX_COLS; ++i) {
        const float d0 = f.dry_min[i] / 127.0f, d1 = f.dry_max[i] / 127.0f;
        const float w0 = f.wet_min[i] / 127.0f, w1 = f.wet_max[i] / 127.0f;
        const float x = 4 + i * PITCH;
        if (byp) {  // DRY encima (lo que se oye), WET detrás como vista previa
            p.fill(x, CY - w1 * A, wide, std::max(2.0f, (w1 - w0) * A), PURPLE_DIM);
            p.fill(x + off, CY - d1 * A, thin, std::max(2.0f, (d1 - d0) * A), MON);
        } else {
            p.fill(x, CY - d1 * A, wide, std::max(2.0f, (d1 - d0) * A), MON);
            p.fill(x + off, CY - w1 * A, thin, std::max(2.0f, (w1 - w0) * A), PURPLE_BRIGHT);
        }
    }
    char right[48];
    std::snprintf(right, sizeof(right), "42.7 MS · %d COL · %s", FX_COLS, f.free_run ? "FREE" : "TRIG ↑ DRY");
    legend(p, {{MON, "DRY", false}, {byp ? PURPLE_DIM : PURPLE_BRIGHT, byp ? "WET · BYPASS" : "WET", false}}, right);
}

// ── C2 · Espectro dual ───────────────────────────────────────────────────────

void draw_spec(const Painter& p, const StudioState& s, VizState st) {
    constexpr int X0 = 12;
    const bool byp = st == VizState::Bypass;
    const uint32_t wc = byp ? PURPLE_DIM : PURPLE_BRIGHT;
    auto y = [](float db) { return 28 + (-db / 60) * 188; };
    float D[FX_BANDS], W[FX_BANDS];
    for (int i = 0; i < FX_BANDS; ++i) {
        D[i] = s.fx.spec_dry[i] / 4.0f - 60;
        W[i] = s.fx.spec_wet[i] / 4.0f - 60;
    }

    for (int db : {-20, -40}) {
        p.fill(X0, y(db), 704, 1, LINE);
        char t[24];
        std::snprintf(t, sizeof(t), "−%d DB", -db);
        p.text(X0 + 2, y(db) - 4, t, MON, T12);
    }
    p.fill(X0, 216, 704, 1, LINE);

    for (int i = 0; i < FX_BANDS; ++i) {
        const float yw = y(W[i]);
        p.fill(X0 + i * 22 + 3, yw, 16, 216 - yw, wc);
    }
    // Contorno escalonado de la DRY con un halo BG de 1 px para que se lea sobre las barras.
    for (int i = 0; i < FX_BANDS; ++i) {
        const float yd = y(D[i]);
        p.fill(X0 + i * 22, yd - 1, 22, 4, BG);
        if (i) {
            const float yp = y(D[i - 1]);
            p.fill(X0 + i * 22 - 1, std::min(yp, yd) - 1, 4, std::fabs(yp - yd) + 4, BG);
        }
    }
    for (int i = 0; i < FX_BANDS; ++i) {
        const float yd = y(D[i]);
        p.fill(X0 + i * 22, yd, 22, 2, MON);
        if (i) {
            const float yp = y(D[i - 1]);
            p.fill(X0 + i * 22, std::min(yp, yd), 2, std::fabs(yp - yd) + 2, MON);
        }
    }
    // Fila ∆: la WET agrega (≥ +3 dB) o quita (≤ −3 dB) en cada banda.
    for (int i = 0; i < FX_BANDS; ++i) {
        const float dd = W[i] - D[i];
        const int bx = X0 + i * 22 + 3;
        if (!byp && dd >= 3) p.fill(bx, 224, 16, 10, PURPLE_BRIGHT);
        else if (!byp && dd <= -3) p.fill(bx, 224, 16, 10, MON);
        else p.fill(bx, 228, 16, 2, LINE);
    }
    static const struct { float f; const char* l; } FREQ[] = {{20, "20"}, {100, "100"}, {1000, "1K"}, {10000, "10K"}, {20000, "20K"}};
    for (const auto& q : FREQ) {
        const float cx = X0 + band_of(q.f) * 22 + 11;
        p.text(std::clamp(cx, 20.0f, float(VIZ_W - 18)), 250, q.l, MON, T12, LV_TEXT_ALIGN_CENTER);
    }
    legend(p, {{MON, "DRY", true}, {wc, "WET", false}, {byp ? LINE : PURPLE_BRIGHT, "∆ ADDS", false},
               {byp ? LINE : MON, "∆ REMOVES", false}},
           "32 BANDS · 0 TO −60 DB · ∆ = ±3 DB");
}

// ── §C · Firmas ──────────────────────────────────────────────────────────────

void axis_db(const Painter& p, float top, float bot, float x0, float x1) {
    for (int db : {0, -20, -40, -60}) {
        const float y = top + (-db / 60.0f) * (bot - top);
        p.fill(x0, y, x1 - x0, 1, LINE);
        char t[16];
        if (db) std::snprintf(t, sizeof(t), "−%d", -db);
        else std::snprintf(t, sizeof(t), "0");
        p.text(x0 - 8, y + 4, t, MON, T12, LV_TEXT_ALIGN_RIGHT);
    }
}

/// Ghost Echo: repeticiones cada TIME con su nivel según FEEDBACK y la recta de caída a −60 dB.
void sig_ghost(const Painter& p, const float* val, const Pal& c, uint32_t since_attack) {
    constexpr float X0 = 56, X1 = 712, TOP = 36, BOT = 220, WIN = 3000;
    const float s = (X1 - X0) / WIN;
    const float time = std::max(10.0f, val[2]);
    const float fb = std::max(0.001f, val[1] / 100.0f);
    const float fb_db = 20 * std::log10(fb);
    auto y = [&](float db) { return TOP + (-db / 60) * (BOT - TOP); };
    axis_db(p, TOP, BOT, X0, X1);

    const float pt = std::min(WIN, float(since_attack));
    for (int px = 0; px < X1 - X0; px += 6) {
        const float db = fb_db * (px / s) / time;
        if (db < -60) break;
        p.fill(X0 + px, y(db) - 1, 2, 2, MON);
    }
    int cur = -1;
    for (int n = 0; n * time <= WIN; ++n)
        if (n * time <= pt) cur = n;
    const int label_every = std::max(1, int(std::ceil(40.0f / (time * s))));
    for (int n = 0; n * time <= WIN; ++n) {
        const float ms = n * time, db = std::max(-60.0f, n * fb_db), cx = X0 + ms * s, yt = y(db);
        p.fill(cx - 4, yt, 8, BOT - yt, n == 0 ? MON : ms <= pt ? c.pb : c.p);
        if (n == cur && n > 0) p.fill(cx - 4, yt - 4, 8, 2, c.live);
        if (n % label_every == 0) {
            char t[8];
            std::snprintf(t, sizeof(t), "%d", int(ms));
            p.text(std::min(cx, X1 - 20), 240, t, MON, T12, LV_TEXT_ALIGN_CENTER);
        }
    }
    p.fill(X0 + pt * s - 1, TOP, 2, BOT - TOP, c.live);
    p.text(VIZ_W - 4, 240, "MS", MON, T12, LV_TEXT_ALIGN_RIGHT);

    char right[80];
    const float to60 = 60.0f / std::max(0.01f, -fb_db) * time / 1000.0f;
    std::snprintf(right, sizeof(right), "TIME %d MS · FB %d %% · −%.1f DB/REP · −60 DB IN %.1f S", int(time),
                  int(std::lround(val[1])), double(-fb_db), double(to60));
    legend(p, {{MON, "DRY", false}, {c.pb, "ECHO PLAYED", false}, {c.p, "UPCOMING", false}}, right);
}

/// Tape / Bit: curva de transferencia contra la identidad y el nivel actual (FX Viz C3).
void sig_curve(const Painter& p, FxSig sig, const float* val, const Pal& c, float peak) {
    constexpr float BX = 40, BY = 12, BW = 480, BH = 232, CX = BX + BW / 2, CY = BY + BH / 2;
    const float drive_k = 0.5f + val[1] * 0.49f;
    const float levels = std::pow(2.0f, std::max(1.0f, val[1]) - 1.0f);
    auto f = [&](float v) {
        return sig == FxSig::Bit ? std::round(v * levels) / levels : std::tanh(drive_k * v) / std::tanh(drive_k);
    };

    p.frame(int(BX), int(BY), int(BW), int(BH), 1, LINE);
    p.fill(BX, CY, BW, 1, LINE);
    p.fill(CX, BY, 1, BH, LINE);
    for (int i = 0; i < BW; i += 6) p.fill(BX + i, CY - ((i - BW / 2) / (BW / 2)) * (BH / 2) - 1, 2, 2, MON);
    int prev = -1;
    for (int i = 0; i < BW; ++i) {
        const float v = (i / (BW - 1)) * 2 - 1;
        const int py = int(std::lround(CY - std::clamp(f(v), -1.0f, 1.0f) * (BH / 2 - 2)));
        if (prev < 0) prev = py;
        p.fill(BX + i - (i ? 1 : 0), std::min(prev, py) - 1, 2, std::abs(py - prev) + 2, c.pb);
        prev = py;
    }
    const float lv = std::clamp(peak, 0.0f, 0.98f);
    const float px = CX + lv * BW / 2, py = CY - f(lv) * (BH / 2 - 2);
    p.fill(px, py, 1, CY - py, MON);
    p.fill(CX, py, px - CX, 1, MON);
    p.fill(px - 5, py - 5, 10, 10, c.live);
    p.text(BX + 8, BY + 18, "OUT", MON, T12);
    p.text(BX + BW - 8, CY - 8, "IN", MON, T12, LV_TEXT_ALIGN_RIGHT);

    constexpr float OX = 552;
    char t[40];
    p.text(OX, 40, "IN", GRAY, T12B);
    db_text(lv, t, sizeof(t));
    p.text(OX, 78, t, WHITE, T28);
    p.text(OX + 96, 78, "DBFS", MON, T12);
    p.text(OX, 122, "OUT", GRAY, T12B);
    db_text(std::fabs(f(lv)), t, sizeof(t));
    p.text(OX, 160, t, WHITE, T28);
    p.text(OX + 96, 160, "DBFS", MON, T12);
    p.fill(OX, 194, 10, 10, c.live);
    p.text(OX + 16, 203, "CURRENT LEVEL", GRAY, T12);
    if (sig == FxSig::Bit) std::snprintf(t, sizeof(t), "BITS %d · ENC R", int(val[1]));
    else std::snprintf(t, sizeof(t), "DRIVE %.1f DB · ENC R", double(val[1]));
    p.text(OX, 236, t, MON, T12);
}

/// Cymatic: patrón de Chladni de una placa cuadrada con 8 modos (FX Viz C4).
void sig_cymatic(const Painter& p, const StudioState& s, VizState st) {
    constexpr int N = 58, NN = N * N;
    static const int MODES[8][2] = {{1, 2}, {1, 3}, {2, 3}, {1, 4}, {3, 4}, {2, 5}, {1, 5}, {3, 5}};
    static float* basis = nullptr;  // 8 × 58 × 58, se calcula una vez
    if (!basis) {
        basis = new float[8 * NN];
        for (int k = 0; k < 8; ++k) {
            const int n = MODES[k][0], m = MODES[k][1];
            const float sg = k % 2 ? 1.0f : -1.0f;
            for (int j = 0; j < N; ++j)
                for (int i = 0; i < N; ++i) {
                    const float u = (i + 0.5f) / N, v = (j + 0.5f) / N;
                    basis[k * NN + j * N + i] = std::cos(n * float(M_PI) * u) * std::cos(m * float(M_PI) * v)
                                              + sg * std::cos(m * float(M_PI) * u) * std::cos(n * float(M_PI) * v);
                }
        }
    }
    float a[8];
    for (int k = 0; k < 8; ++k) a[k] = s.fx.mode_amp[k] / 255.0f;
    static float field[NN];
    float mx = 1e-6f;
    for (int i = 0; i < NN; ++i) {
        float v = 0;
        for (int k = 0; k < 8; ++k) v += a[k] * basis[k * NN + i];
        field[i] = v;
        mx = std::max(mx, std::fabs(v));
    }
    const uint32_t sand1 = st == VizState::Bypass ? PURPLE_DIM : PURPLE_BRIGHT;
    const uint32_t sand2 = st == VizState::Bypass ? SURFACE_2 : PURPLE;
    p.fill(12, 12, N * 4, N * 4, SURFACE);
    for (int i = 0; i < NN; ++i) {
        const float r = std::fabs(field[i]) / mx;
        if (r < 0.06f) p.fill(12 + (i % N) * 4, 12 + (i / N) * 4, 4, 4, sand1);
        else if (r < 0.14f) p.fill(12 + (i % N) * 4, 12 + (i / N) * 4, 4, 4, sand2);
    }
    p.text(272, 28, "MODES · AMPLITUDE", GRAY, T12B);
    p.text(VIZ_W - 8, 28, "SAND = NODES", MON, T12, LV_TEXT_ALIGN_RIGHT);
    for (int k = 0; k < 8; ++k) {
        const int y = 44 + k * 25;
        char t[16];
        std::snprintf(t, sizeof(t), "M%d %d,%d", k + 1, MODES[k][0], MODES[k][1]);
        p.text(272, y + 10, t, GRAY, T12);
        p.fill(368, y, 340, 10, PURPLE_DIM);
        p.fill(368, y, std::lround(340 * a[k]), 10, st == VizState::Bypass ? MON : PURPLE_BRIGHT);
    }
}

/// Granular: cada grano es un rect (x = posición, ancho = tamaño, fila = pitch).
void sig_granular(const Painter& p, const StudioState& s, const Pal& c, VizState st) {
    constexpr float AX = 56, AW = 656, S = AW / 2000;
    auto y = [](float pitch) { return 124 - pitch * 7.5f; };
    for (int pitch : {12, 0, -12}) {
        p.fill(AX, y(pitch), AW, 1, LINE);
        char t[16];
        std::snprintf(t, sizeof(t), "%s%d", pitch > 0 ? "+" : pitch < 0 ? "−" : "", std::abs(pitch));
        p.text(AX - 8, y(pitch) + 4, t, MON, T12, LV_TEXT_ALIGN_RIGHT);
    }
    const bool byp = st == VizState::Bypass;
    for (int j = 0; j < s.fx.grain_count; ++j) {
        const FxGrain& g = s.fx.grains[j];
        const float am = g.amp / 255.0f;
        const float x = AX + g.pos_ms * S, w = g.size_ms * S;
        const uint32_t col = byp ? PURPLE_DIM : am > 0.66f ? PURPLE_BRIGHT : am > 0.33f ? PURPLE : PURPLE_DIM;
        p.fill(x, y(g.pitch_st) - 4, std::min(w, AX + AW - x), 8, col);
    }
    p.fill(AX + s.fx.write_head_ms * S - 1, 28, 2, 192, c.live);
    for (int ms = 0; ms <= 2000; ms += 500) {
        char t[8];
        std::snprintf(t, sizeof(t), "%d", ms);
        p.text(AX + ms * S, 240, t, MON, T12, LV_TEXT_ALIGN_CENTER);
    }
    p.text(AX + AW + 16, 240, "MS", MON, T12);
    legend(p, {{c.pb, "GRAIN", false}, {c.live, "WRITE HEAD", false}}, "X = POSITION · WIDTH = SIZE · Y = PITCH (ST)");
}

/// Modal Reverb / Spring + Plate: envolvente de caída con la marca RT60 y el cabezal.
void sig_verb(const Painter& p, bool spring, const float* val, const Pal& c, uint32_t since_attack) {
    constexpr float AX = 56, TOP = 36, BOT = 220, AW = 660;
    const float dec = std::max(0.1f, val[1]);
    const float pre = 0.02f;  // pre-delay fuera del panel (05 §1.13)
    const float dur = 1.5f * dec + pre;
    const float pt = since_attack / 1000.0f;

    p.fill(AX, BOT, AW, 1, LINE);
    p.fill(AX, TOP, AW, 1, LINE);
    p.text(AX - 8, TOP + 4, "0 DB", MON, T12, LV_TEXT_ALIGN_RIGHT);
    for (int col = 0; col < 110; ++col) {
        const float tm = (col + 0.5f) / 110 * dur;
        float e = tm < pre ? 0 : std::exp(-6.91f * (tm - pre) / dec);
        if (!spring)  // batido de los modos
            e = std::min(1.0f, e * (1 + 0.12f * std::cos(TWO_PI * tm * 2.3f)) * (1 + 0.08f * std::cos(TWO_PI * tm * 5.7f)));
        const float h = (BOT - TOP) * e * (spring ? 0.8f : 1.0f);
        p.fill(AX + col * 6, BOT - h, 5, h, tm <= pt ? c.p : PURPLE_DIM);
        if (!spring && h > 2) p.fill(AX + col * 6, BOT - h, 5, 2, c.pb);
    }
    if (spring) {  // rebotes del resorte cada 28 ms, decaen más rápido que la placa
        for (float tm = pre; tm < dur; tm += 0.028f) {
            const float es = std::exp(-tm * 3.2f) * (0.6f + 0.4f * std::sin(tm * 90));
            const float h = (BOT - TOP) * es;
            p.fill(AX + tm / dur * AW, BOT - h, 2, h, tm <= pt ? c.pb : c.p);
        }
    }
    const float rx = AX + (pre + dec) / dur * AW;
    for (float yy = TOP; yy < BOT; yy += 6) p.fill(rx, yy, 2, 2, c.live);
    char t[48];
    std::snprintf(t, sizeof(t), "RT60 %.1f S", double(dec));
    p.text(rx + 8, TOP + 16, t, c.live, T12);
    if (pt < dur) p.fill(AX + pt / dur * AW - 1, TOP, 2, BOT - TOP, c.live);
    const float step = dur <= 4 ? 0.5f : dur <= 8 ? 1.0f : 2.0f;
    for (float sx = 0; sx <= dur + 1e-4f; sx += step) {
        if (sx) std::snprintf(t, sizeof(t), "%.1f", double(sx));
        else std::snprintf(t, sizeof(t), "0");
        p.text(AX + sx / dur * AW, 240, t, MON, T12, LV_TEXT_ALIGN_CENTER);
    }
    p.text(AX + 676, 240, "S", MON, T12);
    if (spring) {
        std::snprintf(t, sizeof(t), "DECAY %.1f S · BOUNCE 28 MS", double(dec));
        legend(p, {{c.p, "PLATE", false}, {c.pb, "SPRING", false}}, t);
    } else {
        std::snprintf(t, sizeof(t), "DECAY %.1f S · PRE %d MS", double(dec), int(std::lround(pre * 1000)));
        legend(p, {{c.p, "TAIL PLAYED", false}, {PURPLE_DIM, "UPCOMING", false}}, t);
    }
}

/// Phase Chorus: retardo de las 3 voces en un ciclo del LFO.
void sig_chorus(const Painter& p, const StudioState& s, const float* val, const Pal& c) {
    constexpr float AX = 56, AW = 656, TOP = 36, BOT = 220;
    const float depth = val[1] / 100.0f * 8.0f;  // DEPTH (ENC R) 50 % = ±4 ms
    auto y = [](float ms) { return BOT - (ms - 5) / 10 * (BOT - TOP); };
    const uint32_t col[3] = {c.pb, c.live, c.p};
    for (int ms : {5, 10, 15}) {
        p.fill(AX, y(ms), AW, 1, LINE);
        char t[8];
        std::snprintf(t, sizeof(t), "%d", ms);
        p.text(AX - 8, y(ms) + 4, t, MON, T12, LV_TEXT_ALIGN_RIGHT);
    }
    for (int v = 0; v < 3; ++v)
        for (int px = 0; px < AW; px += 4) {
            const float ms = 10 + depth * std::sin(TWO_PI * (px / AW + v / 3.0f));
            p.fill(AX + px, y(ms) - 1, 2, 2, col[v]);
        }
    const float ph = s.fx.lfo_phase / 65536.0f, x = AX + ph * AW;
    p.fill(x, TOP, 1, BOT - TOP, MON);
    for (int v = 0; v < 3; ++v) {
        const float ms = 10 + depth * std::sin(TWO_PI * (ph + v / 3.0f));
        p.fill(x - 5, y(ms) - 5, 10, 10, col[v]);
    }
    static const char* const DEG[5] = {"0°", "90°", "180°", "270°", "360°"};
    for (int i = 0; i < 5; ++i) p.text(AX + i * AW / 4, 240, DEG[i], MON, T12, LV_TEXT_ALIGN_CENTER);
    char t[80];
    std::snprintf(t, sizeof(t), "DELAY (MS) OVER ONE LFO CYCLE · RATE %.1f HZ · ±%.0f MS", double(val[2]), double(depth));
    legend(p, {{c.pb, "V1", false}, {c.live, "V2", false}, {c.p, "V3", false}}, t);
}

/// Sub Genesis: fundamental y 2 armónicos de la entrada, y las suboctavas generadas.
void sig_sub(const Painter& p, const StudioState& s, const float* val, const Pal& c) {
    constexpr float AX = 56, AW = 656, TOP = 36, BOT = 204;
    auto X = [](float f) { return AX + std::log(f / 20) / std::log(40.0f) * AW; };
    const float f0 = std::max(25.0f, s.fx.f0_hz);
    const float env = s.fx.in_env;
    const float sub = std::clamp(val[1] / 6.0f, 0.0f, 1.3f);  // SUB LVL 6 dB = alto del diseño
    const bool two = val[2] >= 1.5f;
    p.fill(AX, BOT, AW, 1, LINE);
    struct Bar { float f; uint32_t c; float w, l; bool lab; };
    const Bar bars[5] = {{f0, MON, 16, 1, true}, {f0 * 2, MON, 8, 0.45f, false}, {f0 * 3, MON, 8, 0.3f, false},
                         {f0 / 2, c.pb, 24, 0.7f * sub, true}, {f0 / 4, c.p, 24, two ? 0.4f * sub : 0, two}};
    char t[24];
    for (const Bar& b : bars) {
        if (b.l <= 0) continue;
        const float h = (BOT - TOP) * std::min(1.0f, b.l * env), cx = X(b.f);
        p.fill(cx - b.w / 2, BOT - h, b.w, h, b.c);
        if (!b.lab) continue;
        p.text(cx, 224, note_name(b.f, t, sizeof(t)), WHITE, T12B, LV_TEXT_ALIGN_CENTER);
        std::snprintf(t, sizeof(t), b.f < 100 ? "%.1f HZ" : "%.0f HZ", double(b.f));
        p.text(cx, 242, t, MON, T12, LV_TEXT_ALIGN_CENTER);
    }
    char nm[16], right[48];
    std::snprintf(right, sizeof(right), "TRACK %s · 20–800 HZ LOG", note_name(f0, nm, sizeof(nm)));
    if (two) legend(p, {{MON, "INPUT", false}, {c.pb, "SUB −1 OCT", false}, {c.p, "SUB −2 OCT", false}}, right);
    else legend(p, {{MON, "INPUT", false}, {c.pb, "SUB −1 OCT", false}}, right);
}

void draw_sig(const Painter& p, const StudioState& s, VizState st, uint32_t now_ms) {
    const FxInfo& fx = FX_CATALOG[s.fx_slot];
    const float* val = s.fx_val[s.fx_slot];
    const Pal c = palette(st);
    const uint32_t since = now_ms - s.fx.attack_ms;
    switch (fx.sig) {
        case FxSig::Ghost:    sig_ghost(p, val, c, since); break;
        case FxSig::Tape:
        case FxSig::Bit:      sig_curve(p, fx.sig, val, c, s.fx.in_peak); break;
        case FxSig::Cymatic:  sig_cymatic(p, s, st); break;
        case FxSig::Granular: sig_granular(p, s, c, st); break;
        case FxSig::Modal:    sig_verb(p, false, val, c, since); break;
        case FxSig::Spring:   sig_verb(p, true, val, c, since); break;
        case FxSig::Chorus:   sig_chorus(p, s, val, c); break;
        case FxSig::Sub:      sig_sub(p, s, val, c); break;
        default: break;
    }
}

}  // namespace

void viz_draw(const Painter& p, const StudioState& s, VizState st, uint32_t now_ms) {
    p.fill(0, 0, VIZ_W, VIZ_H, BG);
    switch (s.fx_view) {
        case FxView::Wave:      draw_scope(p, s, st); break;
        case FxView::Spectrum:  draw_spec(p, s, st); break;
        case FxView::Signature: draw_sig(p, s, st, now_ms); break;
    }
    if (st == VizState::Hold) p.frame(0, 0, VIZ_W, VIZ_H, 2, WHITE);
    else p.frame(0, 0, VIZ_W, VIZ_H, 1, LINE);
}

void meter_draw(const Painter& p, const MeterView& m) {
    p.fill(0, 0, METER_W, METER_H, BG);
    p.text(12, 13, "IN", GRAY, T12B, LV_TEXT_ALIGN_CENTER);
    for (int j = 0; j < 2; ++j) {
        const int bx = j * 14;
        p.fill(bx, 20, 10, 8, m.clip[j] ? RED : LINE);
        p.fill(bx, 32, 10, 272, SURFACE_2);
        p.fill(bx, 304 - m.level[j], 10, m.level[j], GRAY);
        p.fill(bx, 304 - std::min(270, m.peak[j] + 2), 10, 2, WHITE);
    }
    char t[8];
    if (m.pad_db) std::snprintf(t, sizeof(t), "−%d", -m.pad_db);
    else std::snprintf(t, sizeof(t), "0");
    p.text(12, 324, t, MON, T12, LV_TEXT_ALIGN_CENTER);
}
