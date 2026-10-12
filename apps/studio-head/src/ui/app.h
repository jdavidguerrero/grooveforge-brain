#pragma once

#include "state/studio_state.h"
#include "ui/input.h"
#include "ui/overlays.h"
#include "ui/screen_ai.h"
#include "ui/screen_engine.h"
#include "ui/screen_env.h"
#include "ui/screen_error.h"
#include "ui/screen_fx.h"
#include "ui/screen_lfo.h"
#include "ui/screen_osc.h"
#include "ui/screen_perform.h"
#include "ui/screen_presets.h"
#include "ui/screen_spectrum.h"

/// Páginas de la UI. Las tres primeras son las vistas que recorre NAV en SYNTH.
enum class Page : uint8_t { Perform, Ai, Spectrum, Env, Fx, Engine, Presets, Error, Osc, Lfo, Count };

/**
 * @brief Navegación de la UI v3 + FX Viz.
 *
 * SYNTH: NAV recorre PERFORM → AI → SPECTRUM; B1 abre OSC, B2 ENV y B3 LFO / MOD, y la misma
 * tecla vuelve a la última vista (en OSC, NAV elige el oscilador; en LFO, NAV push alterna SYNC).
 * B4 abre PRESETS, B4 otra vez ENGINE SELECT y B4 de nuevo vuelve a PERFORM. B5 MODE pasa a FX MAIN (y vuelve a la última
 * vista). En FX, NAV elige el slot, NAV push enciende/apaga, B1–B3 rutean, B4 salta al favorito y
 * B6 es HOLD. Si se pierde el enlace con el motor, la página de error reemplaza a cualquier otra.
 *
 * Todas las pantallas se crean una vez; solo se actualiza la que está a la vista.
 */
class App {
public:
    void create();
    void show(Page p);
    void handle(Ctl c, StudioState& s);
    void update(const StudioState& s);
    Page page() const { return _page; }
    Overlays& overlays() { return _overlays; }

private:
    Screen* screen(Page p);
    void handle_fx(Ctl c, StudioState& s);
    void handle_synth(Ctl c, StudioState& s);
    void load_engine(uint8_t id, StudioState& s);
    void toggle_page(Page p);

    ScreenPerform _perform;
    ScreenAi _ai;
    ScreenSpectrum _spectrum;
    ScreenEnv _env;
    ScreenFx _fx;
    ScreenEngine _engine;
    ScreenPresets _presets;
    ScreenError _error;
    ScreenOsc _osc;
    ScreenLfo _lfo;
    Overlays _overlays;
    lv_obj_t* _roots[int(Page::Count)] = {};
    Page _page = Page::Perform;
    Page _last_view = Page::Perform;
    Page _before_error = Page::Perform;
};
