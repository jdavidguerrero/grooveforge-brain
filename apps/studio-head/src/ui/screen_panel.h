#pragma once

#include <string>

#include "ui/bottom_bar.h"
#include "ui/screen.h"
#include "ui/top_bar.h"

/**
 * @brief Vista PANEL (theory/fidelidad-engines.md §11.7): el panel completo del engine en una pantalla.
 *
 * Cuarta vista de la home (PERFORM · AI · SPECTRUM · PANEL). Solo lectura: tarjetas por sección (paradas de OSC y de
 * MOD, ENV AMP y FILTER, FILTER, LFO, SYNTH FX), como la serigrafía del original. Se edita desde las páginas.
 */
class ScreenPanel : public Screen {
public:
    lv_obj_t* create() override;
    void update(const StudioState& s) override;

    static constexpr int CARDS = 12;   ///< grilla de 4 × 3

private:
    struct Card { lv_obj_t *box, *title, *body; std::string t, b; };
    void set_card(int i, const char* title, const std::string& body, bool accent);

    lv_obj_t* _scr = nullptr;
    TopBar _top;
    BottomBar _bottom;
    Card _cards[CARDS];
};
