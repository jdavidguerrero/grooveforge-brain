#include "studio_ui.h"

#include <gf_params.h>

namespace studio {

namespace {

// Slots de FX implementados (los demás se saltan al girar NAV, como fx_next_slot de la cabeza)
constexpr bool FX_AVAILABLE[GF_UI_FX_SLOTS] = {true, true, true, false, true, true, true, true, false, true, false, true};
// Ruteo por defecto de cada FX (05 §1; capa del FX en el catálogo de la cabeza)
constexpr uint8_t FX_ROUTE_DEFAULT[GF_UI_FX_SLOTS] = {0, 0, 1, 0, 2, 0, 1, 0, 0, 2, 0, 2};

// Home de theory/08 de Studio: PERFORM y PANEL. AI y SPECTRUM siguen en el enum (ids del protocolo) pero fuera de la
// rotación hasta que lleguen los datos de GroovePilot y el comando de espectro.
bool is_view(uint8_t p) { return p == PERFORM || p == PANEL || p == HYBRID; }

/// Vistas de la home en el orden de NAV.
constexpr int N_VIEWS = 2;
constexpr uint8_t VIEWS[N_VIEWS] = {PERFORM, PANEL};
uint8_t next_view(uint8_t p, int dir) {
    int i = 0;
    while (i < N_VIEWS && VIEWS[i] != p) ++i;
    return VIEWS[(i % N_VIEWS + dir + N_VIEWS) % N_VIEWS];
}

uint8_t next_fx_slot(uint8_t slot, int dir) {
    int i = slot;
    for (uint8_t k = 0; k < GF_UI_FX_SLOTS; ++k) {
        i = (i + dir + GF_UI_FX_SLOTS) % GF_UI_FX_SLOTS;
        if (FX_AVAILABLE[i]) return uint8_t(i);
    }
    return slot;
}

}  // namespace

uint8_t panel_card_count(uint8_t e) {
    return e < ENGINE_COUNT ? uint8_t(GF_OSC_STOP_COUNT[e] + GF_MOD_STOPS[e] + 5) : 0;
}

PanelItem panel_card(uint8_t e, uint8_t i) {
    const uint8_t no = GF_OSC_STOP_COUNT[e], nm = GF_MOD_STOPS[e];
    if (i < no) return {PanelCard::Osc, i};
    if (i < no + nm) return {PanelCard::Mod, uint8_t(i - no)};
    static const PanelCard REST[5] = {PanelCard::EnvAmp, PanelCard::EnvFilter, PanelCard::Filter, PanelCard::Lfo,
                                      PanelCard::SynthFx};
    const int k = i - no - nm;
    return {REST[k < 0 ? 0 : k > 4 ? 4 : k], 0};
}

StudioUi::StudioUi() {
    ui_.fx_slot = FAV_FX_SLOT;
    ui_.fx_on = (1u << 1) | (1u << 2) | (1u << 6);   // mismo arranque que la cabeza
    for (uint8_t i = 0; i < GF_UI_FX_SLOTS; ++i) ui_.fx_route[i] = FX_ROUTE_DEFAULT[i];
}

void StudioUi::set_engine(uint8_t engine) {
    if (engine >= ENGINE_COUNT || engine == engine_) return;
    engine_ = engine;
    if (panel_cursor_ >= panel_card_count(engine_)) panel_cursor_ = 0;
    refresh_cursor();
    dirty_ = true;
}

void StudioUi::show(uint8_t page) {
    ui_.page = page;
    if (is_view(page)) last_view_ = page;
    refresh_cursor();
}

void StudioUi::toggle_page(uint8_t page) { show(ui_.page == page ? last_view_ : page); }

void StudioUi::refresh_cursor() {
    switch (ui_.page) {
        case ENGINE:  ui_.cursor = engine_cursor_; break;
        case PRESETS: ui_.cursor = preset_cursor_; break;
        case OSC:     ui_.cursor = osc_sel_[engine_]; break;
        case LFO:     ui_.cursor = lfo_stop_[engine_]; break;
        case SYNTH_FX: ui_.cursor = sfx_cursor_; break;
        case ENV:     ui_.cursor = env_sel_; break;
        case PANEL:   ui_.cursor = panel_cursor_; break;
        case GROOVE:  ui_.cursor = groove_stop_; break;
        case HYBRID:  ui_.cursor = layer_cursor_; break;
        case LAYER_PRESETS: ui_.cursor = layer_row_; break;
        default:      ui_.cursor = 0; break;
    }
}

Result StudioUi::handle(Control c, int8_t v) {
    dirty_ = true;
    if (c == Control::NavLong) {                            // home: PERFORM en SYNTH, HYBRID en HYBRID SYNTH; en FX, FX
        custom_cancel();
        if (ui_.mode == MODE_SYNTH) show(PERFORM);
        else if (ui_.mode == MODE_HYBRID) show(HYBRID);
        return {};
    }
    if (c == Control::VolUp || c == Control::VolDown)        // en todos los modos, en cualquier página
        return {Action::VolStep, uint8_t(int8_t(c == Control::VolUp ? 1 : -1))};
    if (c == Control::Mode) {                               // SYNTH → FX → HYBRID SYNTH → SYNTH (theory/12 §3d)
        custom_cancel();
        ui_.shift = 0;
        ui_.fx_hold = 0;
        if (ui_.mode == MODE_SYNTH) {
            keep_synth_page();
            ui_.mode = MODE_FX;
            show(FX);
            return {};
        }
        if (ui_.mode == MODE_FX) {
            ui_.mode = MODE_HYBRID;
            show(hybrid_page_);                             // la página de HYBRID SYNTH donde lo dejaste
            return {Action::HybridEnter, layer_to_load()};
        }
        keep_hybrid_page();
        ui_.mode = MODE_SYNTH;
        restore_synth_page();                               // la página de SYNTH que tenías al salir
        return {Action::HybridExit, 0};
    }
    if (ui_.mode == MODE_FX) return handle_fx(c, v);
    return ui_.mode == MODE_HYBRID ? handle_hybrid(c, v) : handle_synth(c, v);
}

Result StudioUi::focus_to(uint8_t f) {
    if (f == focus()) return {};
    set_focus(f);
    return {Action::LayerFocus, f};
}

void StudioUi::custom_cancel() {
    if (!custom_step_) return;
    custom_step_ = 0;
    ui_.flags = uint8_t(ui_.flags & ~GF_UI_FLAG_PICK);
    set_focus(custom_focus_);
}

// ENGINE SELECT de CUSTOM: primero A, después B (distinto de A); al elegir B se arma la capa y se abre OSC de A.
Result StudioUi::custom_pick(uint8_t e) {
    if (e >= ENGINE_COUNT) return {};
    if (custom_step_ == 1) {
        custom_a_ = e;
        custom_step_ = 2;
        set_focus(1);
        engine_cursor_ = uint8_t((e + 1) % ENGINE_COUNT);
        refresh_cursor();
        return {};
    }
    if (e == custom_a_) return {};                          // B no puede repetir el engine de A
    custom_step_ = 0;
    ui_.flags = uint8_t(ui_.flags & ~GF_UI_FLAG_PICK);
    set_focus(0);
    show(OSC);
    return {Action::LayerCustom, uint8_t(custom_a_ | (e << 4))};
}

Result StudioUi::handle_hybrid(Control c, int8_t v) {
    const uint8_t pg = ui_.page;
    if (custom_step_ && pg == ENGINE) {                     // eligiendo los engines de CUSTOM
        switch (c) {
            case Control::Nav:
                engine_cursor_ = uint8_t((engine_cursor_ + v % ENGINE_COUNT + ENGINE_COUNT) % ENGINE_COUNT);
                refresh_cursor();
                return {};
            case Control::NavPush: return custom_pick(engine_cursor_);
            default:
                if (c >= Control::Pick0 && c <= Control::Pick5)
                    return custom_pick(uint8_t(uint8_t(c) - uint8_t(Control::Pick0)));
                if (c == Control::B6Down || c == Control::B6Up) break;
                custom_cancel();                            // cualquier otra tecla cancela y sigue su camino
                break;
        }
    }
    switch (c) {
        case Control::P1Push:                               // en la parada LFO sigue cambiando la forma
            if (pg == LFO && lfo_stop_[engine_] == 0) break;
            return focus_to(0);
        case Control::P2Push: return focus_to(1);
        default: break;
    }
    // PRESETS del engine de la mitad en foco (theory/12 §3d rev. 6): B1–B4 bancos como en SYNTH; SHIFT + B4 vuelve a
    // los presets de capa. Load / SAVE ya van a la mitad en foco (el sketch usa ui.engine()).
    if (pg == PRESETS && c >= Control::Key1 && c <= Control::Key4) {
        if (c == Control::Key4 && ui_.shift) {
            show(LAYER_PRESETS);
            return {};
        }
        if (!ui_.shift) return handle_synth(c, v);
    }
    if (pg == LAYER_PRESETS && c == Control::Key3 && !ui_.shift) {   // B3: PRESETS de la mitad en foco
        show(PRESETS);
        return {};
    }
    switch (c) {
        case Control::Key1:
        case Control::Key2:
        case Control::Key3: {
            if (pg == LAYER_PRESETS && !ui_.shift) {        // B1 / B2: banco A / B de presets de capa
                const int row = 1 + int(uint8_t(c) - uint8_t(Control::Key1)) * PRESET_BANK;
                layer_row_ = uint8_t(row > layer_count_ ? layer_count_ : row);
                refresh_cursor();
                return {};
            }
            if (c == Control::Key3 && ui_.shift) {          // SHIFT + B3: GROOVE (de la capa entera) · otra vez: home
                show(pg == GROOVE ? HYBRID : GROOVE);
                return focus_to(0);
            }
            // Páginas de SYNTH (theory/12 §3d rev. 5): la misma tecla otra vez cambia a la otra capa
            const uint8_t target = c == Control::Key1 ? OSC : c == Control::Key2 ? ENV : LFO;
            if (pg == target) return focus_to(uint8_t(focus() ^ 1));
            show(target);
            return {};
        }
        case Control::Key4:
            if (ui_.shift) {                                // SHIFT + B4: presets de capa · otra vez: home
                if (pg == LAYER_PRESETS) {
                    show(HYBRID);
                } else {
                    layer_row_ = uint8_t(layer_cursor_ < layer_count_ ? layer_cursor_ + 1 : 0);
                    show(LAYER_PRESETS);
                }
                return {};
            }
            show(pg == SYNTH_FX ? HYBRID : SYNTH_FX);       // B4: SYNTH FX (de la capa entera) · otra vez: home
            return focus_to(0);
        case Control::EngineCard:
            engine_cursor_ = engine_;
            show(ENGINE);
            return {};
        default: break;
    }
    if (pg == HYBRID) {
        if (c == Control::Nav) {                            // los presets de capa y, al final, + CUSTOM
            const int i = int(layer_cursor_) + v, n = layer_count_;
            layer_cursor_ = uint8_t(i < 0 ? 0 : i > n ? n : i);
            refresh_cursor();
            return {};
        }
        if (c == Control::NavPush && !ui_.shift) {
            if (layer_cursor_ < layer_count_) return {Action::LayerLoad, layer_cursor_};
            custom_step_ = 1;                               // + CUSTOM: ENGINE SELECT de A, después el de B
            custom_focus_ = focus();
            ui_.flags = uint8_t(ui_.flags | GF_UI_FLAG_PICK);
            set_focus(0);
            engine_cursor_ = engine_;
            show(ENGINE);
            return {};
        }
    }
    if (pg == LAYER_PRESETS && !(ui_.shift && c == Control::NavPush)) {
        const int rows = layer_count_ + 1;                  // la fila 0 es SAVE
        auto move = [&](int d) {
            const int r = layer_row_ + d;
            layer_row_ = uint8_t(r < 0 ? 0 : r >= rows ? rows - 1 : r);
            refresh_cursor();
        };
        switch (c) {
            case Control::Nav: move(v); return {};
            case Control::SwipeUp: move(PRESET_ROWS_VISIBLE); return {};
            case Control::SwipeDown: move(-PRESET_ROWS_VISIBLE); return {};
            case Control::NavPush:
                if (layer_row_ == 0) {
                    if (layer_count_ >= LAYER_SLOTS) return {};
                    ++layer_count_;
                    layer_cursor_ = uint8_t(layer_count_ - 1);
                    return {Action::LayerSave, layer_cursor_};
                }
                layer_cursor_ = uint8_t(layer_row_ - 1);
                return {Action::LayerLoad, layer_cursor_};
            default: break;
        }
        if (c >= Control::Pick0 && c <= Control::Pick5) return {};
    }
    return handle_synth(c, v);                              // páginas de edición, ENGINE SELECT, SHIFT, ENC L push
}

Result StudioUi::handle_fx(Control c, int8_t v) {
    switch (c) {
        case Control::Nav:
            for (int n = v > 0 ? v : -v; n > 0; --n) ui_.fx_slot = next_fx_slot(ui_.fx_slot, v > 0 ? 1 : -1);
            break;
        case Control::NavPush:
            ui_.fx_on ^= uint16_t(1u << ui_.fx_slot);
            return {Action::FxToggle, ui_.fx_slot};
        case Control::Key1: ui_.fx_route[ui_.fx_slot] = 0; break;
        case Control::Key2: ui_.fx_route[ui_.fx_slot] = 1; break;
        case Control::Key3: ui_.fx_route[ui_.fx_slot] = 2; break;
        case Control::Key4: ui_.fx_slot = FAV_FX_SLOT; break;
        case Control::B6Down: ui_.fx_hold = 1; break;       // HOLD momentáneo (estilo RMX-1000)
        case Control::B6Up:   ui_.fx_hold = 0; break;
        case Control::FxView0: case Control::FxView1: case Control::FxView2:
            ui_.fx_view = uint8_t(uint8_t(c) - uint8_t(Control::FxView0));
            break;
        case Control::FxViewNext: ui_.fx_view = uint8_t((ui_.fx_view + 1) % 3); break;
        default: break;                                     // SHIFT y ENC L push no existen en FX
    }
    return {};
}

Result StudioUi::handle_synth(Control c, int8_t v) {
    switch (c) {
        case Control::EncLPush:
            ui_.flags ^= GF_UI_FLAG_FILTER_BYPASS;
            return {Action::FilterBypass, uint8_t(ui_.flags & GF_UI_FLAG_FILTER_BYPASS ? 1 : 0)};
        case Control::B6Down: ui_.shift = 1; return {};      // SHIFT momentáneo en el hardware
        case Control::B6Up:   ui_.shift = 0; return {};
        case Control::NavPush:                              // SHIFT + NAV push: ENGINE SELECT desde cualquier página
            if (ui_.shift) {
                engine_cursor_ = engine_;
                show(ENGINE);
                return {};
            }
            break;
        default: break;
    }

    if (ui_.page == ENGINE) {
        switch (c) {
            case Control::Nav:
                engine_cursor_ = uint8_t((engine_cursor_ + v % ENGINE_COUNT + ENGINE_COUNT) % ENGINE_COUNT);
                refresh_cursor();
                break;
            case Control::NavPush:
                engine_ = engine_cursor_;
                show(hybrid() ? HYBRID : PERFORM);
                return {Action::LoadEngine, engine_};
            case Control::Key4: show(PERFORM); break;
            case Control::Key1: show(OSC); break;
            case Control::Key2: show(ENV); break;
            case Control::Key3: show(LFO); break;
            default:
                if (c >= Control::Pick0 && c <= Control::Pick5) {
                    engine_ = engine_cursor_ = uint8_t(uint8_t(c) - uint8_t(Control::Pick0));
                    show(hybrid() ? HYBRID : PERFORM);
                    return {Action::LoadEngine, engine_};
                }
                break;
        }
        return {};
    }

    if (ui_.page == PRESETS) {
        const int rows = preset_count_ + 1;                 // la fila 0 es SAVE
        auto move = [&](int d) {
            int p = preset_cursor_ + d;
            preset_cursor_ = uint8_t(p < 0 ? 0 : p >= rows ? rows - 1 : p);
            refresh_cursor();
        };
        switch (c) {
            case Control::Nav: move(v); break;
            case Control::SwipeUp: move(PRESET_ROWS_VISIBLE); break;
            case Control::SwipeDown: move(-PRESET_ROWS_VISIBLE); break;
            case Control::NavPush:
                if (preset_cursor_ == 0) {
                    if (preset_count_ < PRESET_SLOTS) ++preset_count_;
                    return {Action::SavePreset, uint8_t(preset_count_ - 1)};
                }
                return {Action::LoadPreset, uint8_t(preset_cursor_ - 1)};
            case Control::Key4:
                if (ui_.shift) {                            // SHIFT + B4 otra vez: vuelve a la vista de antes
                    show(last_view_);
                    break;
                }
                [[fallthrough]];
            case Control::Key1: case Control::Key2: case Control::Key3: {
                // B1–B4 saltan al banco A–D (theory/13): al primer preset del banco (fila 1 + 16·banco)
                const int bank = int(uint8_t(c) - uint8_t(Control::Key1));
                move(1 + bank * PRESET_BANK - preset_cursor_);
                break;
            }
            default: break;
        }
        return {};
    }

    if (ui_.page == PERFORM && c == Control::NavPush) {     // NAV push en PERFORM: entra a PANEL (theory/08 §4)
        show(PANEL);
        return {};
    }
    if (ui_.page == PANEL && c == Control::Nav) {       // NAV recorre las tarjetas; pasados los bordes, la vista vecina
        const int i = int(panel_cursor_) + v, n = panel_card_count(engine_);
        if (i < 0) {
            show(next_view(PANEL, -1));
        } else if (i >= n) {
            show(next_view(PANEL, 1));
        } else {
            panel_cursor_ = uint8_t(i);
            refresh_cursor();
        }
        return {};
    }
    if (ui_.page == PANEL && c == Control::NavPush) {   // abre la página de la tarjeta con su parada elegida
        const PanelItem it = panel_card(engine_, panel_cursor_);
        switch (it.card) {
            case PanelCard::Osc: osc_sel_[engine_] = it.stop; show(OSC); break;
            case PanelCard::Mod: lfo_stop_[engine_] = uint8_t(it.stop + 1); show(LFO); break;
            case PanelCard::EnvAmp: env_sel_ = 0; show(ENV); break;
            case PanelCard::EnvFilter: env_sel_ = 1; show(ENV); break;
            case PanelCard::Filter: show(PERFORM); break;     // cutoff / res / FLT ENV AMT viven en la home
            case PanelCard::Lfo: lfo_stop_[engine_] = 0; show(LFO); break;
            case PanelCard::SynthFx: show(SYNTH_FX); break;
        }
        return {};
    }
    if (ui_.page == LFO && c == Control::P1Push && lfo_stop_[engine_] == 0) {   // P1 push: siguiente forma del LFO
        return {Action::LfoShape, 1};
    }

    if (ui_.page == OSC && c == Control::Nav) {
        const uint8_t n = GF_OSC_STOP_COUNT[engine_];
        if (n > 1) {
            osc_sel_[engine_] = uint8_t((osc_sel_[engine_] + v % n + n) % n);
            refresh_cursor();
        }
        return {};
    }
    if (ui_.page == ENV && c == Control::Nav) {                        // NAV elige AMP o FILTER
        env_sel_ = v > 0 ? 1 : 0;
        refresh_cursor();
        return {};
    }
    if (ui_.page == SYNTH_FX && c == Control::Nav) {                   // NAV elige el FX de la cadena
        const int s = int(sfx_cursor_) + v;
        sfx_cursor_ = uint8_t(s < 0 ? 0 : s >= SFX_SLOTS ? SFX_SLOTS - 1 : s);
        refresh_cursor();
        return {};
    }
    if (ui_.page == SYNTH_FX && c == Control::NavPush) return {Action::SfxToggle, sfx_cursor_};
    if (ui_.page == GROOVE) {
        if (c == Control::Nav) {                                       // NAV elige la parada
            const int s = int(groove_stop_) + v;
            groove_stop_ = uint8_t(s < 0 ? 0 : s >= GROOVE_STOPS ? GROOVE_STOPS - 1 : s);
            refresh_cursor();
            return {};
        }
        if (c == Control::NavPush) return {Action::GroovePush, groove_stop_};
        if (c == Control::Key4 && !ui_.shift) return {Action::Tap, 0};   // B4 = TAP (SHIFT + B4 sigue en PRESETS)
    }
    if (ui_.page == LFO && c == Control::Nav) {                        // NAV elige la parada: LFO y las de MOD
        const int s = int(lfo_stop_[engine_]) + v, n = 1 + GF_MOD_STOPS[engine_];
        lfo_stop_[engine_] = uint8_t(s < 0 ? 0 : s >= n ? n - 1 : s);
        refresh_cursor();
        return {};
    }
    if (ui_.page == LFO && c == Control::NavPush && lfo_stop_[engine_] == 0) {   // SYNC solo en la parada LFO
        ui_.flags ^= GF_UI_FLAG_LFO_SYNC;
        return {Action::LfoSync, uint8_t(ui_.flags & GF_UI_FLAG_LFO_SYNC ? 1 : 0)};
    }

    switch (c) {
        case Control::Nav:
            if (is_view(ui_.page)) {
                for (int n = v > 0 ? v : -v; n > 0; --n) show(next_view(ui_.page, v > 0 ? 1 : -1));
            }
            break;
        case Control::Key1: toggle_page(OSC); break;
        case Control::Key2: toggle_page(ENV); break;
        case Control::Key3:                                 // B3: LFO / MOD · SHIFT + B3: GROOVE
            toggle_page(ui_.shift ? GROOVE : LFO);
            break;
        case Control::Key4:                                 // B4: SYNTH FX · SHIFT + B4: PRESETS
            if (ui_.shift) show(PRESETS);
            else toggle_page(SYNTH_FX);
            break;
        case Control::EngineCard:
            engine_cursor_ = engine_;
            show(ENGINE);
            break;
        default: break;                                     // NAV push en las vistas: sin función (v3)
    }
    return {};
}

}  // namespace studio
