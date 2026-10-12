#pragma once

#include <cstdint>

#include "lvgl.h"
#include "state/studio_state.h"

/// Overlays que se pueden forzar (snapshots y pruebas).
enum class Overlay : uint8_t { Volume, Input, Bypass, ModeBanner, Clip };

/**
 * @brief Overlays de la UI v3 sobre lv_layer_top(): aparecen solos cuando cambia el valor que
 *        muestran, sin que ninguna pantalla los llame.
 *
 * - VOL (08a) e IN (08f): cualquier movimiento del pot; se ocultan 1.5 s después del último.
 * - FILTER BYPASS (08b): toast de 1.5 s al activarlo.
 * - MODE (08c): banner de 600 ms en cada cambio SYNTH ⇄ FX.
 * - INPUT CLIPPING (04d): mientras haya clip y 2 s después de la última muestra recortada.
 *
 * Con el enlace perdido solo queda VOL (08d: "VOL stays active").
 */
class Overlays {
public:
    void create();
    void update(const StudioState& s, uint32_t now);
    void force(Overlay o);

private:
    void build_volume(lv_obj_t* top);
    void build_input(lv_obj_t* top);
    void build_toast(lv_obj_t* top);
    void build_banner(lv_obj_t* top);
    void build_clip(lv_obj_t* top);
    void paint_volume(const StudioState& s);
    void paint_input(const StudioState& s, uint32_t now);
    void paint_banner(bool fx);
    static void show(lv_obj_t* o, bool on);

    lv_obj_t *_vol = nullptr, *_vol_val = nullptr;
    lv_obj_t* _vol_seg[32] = {};
    lv_obj_t *_in = nullptr, *_in_val = nullptr, *_in_pad = nullptr;
    lv_obj_t* _in_seg[2][31] = {};
    uint32_t _in_seg_c[2][31] = {};
    lv_obj_t* _toast = nullptr;
    lv_obj_t *_banner = nullptr, *_banner_big = nullptr, *_banner_hint = nullptr;
    lv_obj_t *_scrim = nullptr, *_clip = nullptr, *_clip_detail = nullptr;

    bool _init = false;
    float _last_vol = 0, _last_gain = 0;
    int _last_pad = 0;
    bool _last_bypass = false;
    Mode _last_mode = Mode::Synth;
    uint32_t _vol_until = 0, _in_until = 0, _toast_until = 0, _banner_until = 0;
    uint32_t _clip_ch_until[2] = {};
    float _in_peak[2] = {};
    uint32_t _last_ms = 0;
    int _vol_lit = -1;
    char _last_in_key[32] = {};
};
