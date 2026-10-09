#pragma once

#include "ui/bottom_bar.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief AI / HARMONY (UI v3, 05a): rueda Camelot, acorde, género, cromagrama de 12 bins,
 *        Scale Lock con el último snap, BPM y fase del compás en 16 pasos.
 *
 * Geometría de "GrooveForge Studio UI v3.dc.html": rueda G.wheel(160, 160, 152, 108, 64) en
 * (16, 48); columna derecha en x 360.
 */
class ScreenAi : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

private:
    struct Seg { lv_obj_t* arc; lv_obj_t* lbl; };
    void paint_wheel(uint8_t camelot, bool minor);

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;

    Seg _outer[12] = {};  // anillo B (mayor), índice = número Camelot − 1
    Seg _inner[12] = {};  // anillo A (menor)
    lv_obj_t *_key_code = nullptr, *_key_name = nullptr, *_key_conf = nullptr;
    lv_obj_t *_chord = nullptr, *_chord_conf = nullptr;
    lv_obj_t *_genre = nullptr, *_genre_conf = nullptr;
    lv_obj_t* _chroma[12] = {};
    lv_obj_t *_lock_pill = nullptr, *_lock_txt = nullptr, *_snap = nullptr;
    lv_obj_t* _bpm = nullptr;
    lv_obj_t* _cells[16] = {};

    int _last_key = -1;
    uint8_t _last_chroma_h[12] = {};
    int _last_step = -1;
    char _last_text[64] = {};
};
