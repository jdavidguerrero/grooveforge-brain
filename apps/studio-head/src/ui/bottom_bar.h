#pragma once

#include "lvgl.h"

/**
 * @brief Barra inferior de 800×96 (UI v3, componente BottomBar3). Etiquetas alineadas con el
 *        panel físico según la tabla del enclosure (claude-design-2026-10-09).
 *
 * Fila 1 (64 px): ENC L 0–104 · B1–B4 en segmentos de 148 px · ENC R 696–800.
 * Fila 2 (32 px, solo display): P1 0–200 · MODE 200–296 · NAV 296–504 · B6 504–600 · P2 600–800.
 *
 * Los setters comparan con lo último dibujado y solo tocan LVGL si cambió algo.
 */
class BottomBar {
public:
    void create(lv_obj_t* parent);

    void set_mode(bool fx, bool shift);
    void set_enc(bool right, const char* lbl, const char* val, const char* unit, float frac);
    /// ENC L / R en SYNTH: CUTOFF (escala log 20 Hz–20 kHz) y RES.
    void set_filter_encs(float cutoff_hz, float resonance);
    void set_keys(const char* const labels[4], int active);
    void set_param(int n, const char* lbl, const char* val, const char* unit, float frac);
    /// Control `k` de una página de edición (theory/04 §3.1): 0 = P1, 1 = P2, 2 = ENC L, 3 = ENC R.
    void set_page_control(int k, const char* lbl, const char* val, const char* unit, float frac) {
        if (k < 2) set_param(k + 1, lbl, val, unit, frac);
        else set_enc(k == 3, lbl, val, unit, frac);
    }
    void set_nav(const char* text);
    /// B6 en FX: HOLD apretado → celda HERO_WHITE con HELD.
    void set_hold(bool held);
    /// P1/P2 sin función (ENGINE SELECT, PRESETS): NO FUNCTION atenuado.
    void set_params_off(bool off);
    /// Valores en MONITOR_GRAY (FX en bypass: siguen editables, pero no suenan).
    void set_dim(bool dim);

private:
    struct Enc { lv_obj_t *tag, *lbl, *val, *unit, *track, *fill; char last[24]; int last_fw; };
    struct Key { lv_obj_t *cell, *bar, *lbl; };
    struct Param { lv_obj_t *cell, *tag, *lbl, *val, *unit, *track, *fill; char last[32]; int last_fw; };

    void build_enc(Enc& e, lv_obj_t* cell, bool right);
    void build_param(Param& p, lv_obj_t* cell, int n);
    void paint_params();
    void paint_b6();
    void paint_encs();

    lv_obj_t* _bar = nullptr;
    Enc _enc[2] = {};
    Key _keys[4] = {};
    Param _p[2] = {};
    lv_obj_t *_mode_pill = nullptr, *_mode_next = nullptr;
    lv_obj_t *_nav_val = nullptr;
    lv_obj_t *_b6_cell = nullptr, *_b6_lbl = nullptr, *_b6_state = nullptr;

    bool _fx = false, _shift = false, _hold = false, _p_off = false, _dim = false;
    int _last_mode = -1, _last_active = -2;
    char _last_keys[64] = {};
    char _last_nav[24] = {};
};
