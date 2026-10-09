#pragma once

#include "state/studio_state.h"
#include "ui/input.h"
#include "ui/screen_ai.h"
#include "ui/screen_env.h"
#include "ui/screen_perform.h"
#include "ui/screen_spectrum.h"

/// Páginas de la UI. Las tres primeras son las vistas que recorre NAV; ENV se abre con B2.
enum class Page : uint8_t { Perform, Ai, Spectrum, Env, Count };

/**
 * @brief Navegación de la UI v3: NAV recorre PERFORM → AI → SPECTRUM; B1–B4 abren las páginas
 *        de edición (por ahora ENV en B2) y la misma tecla vuelve; B5 MODE alterna SYNTH ⇄ FX.
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

private:
    Screen* screen(Page p);

    ScreenPerform _perform;
    ScreenAi _ai;
    ScreenSpectrum _spectrum;
    ScreenEnv _env;
    lv_obj_t* _roots[int(Page::Count)] = {};
    Page _page = Page::Perform;
    Page _last_view = Page::Perform;
};
