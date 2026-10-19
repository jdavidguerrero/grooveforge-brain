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
bool is_view(uint8_t p) { return p == PERFORM || p == PANEL; }

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
        default:      ui_.cursor = 0; break;
    }
}

Result StudioUi::handle(Control c, int8_t v) {
    dirty_ = true;
    if (c == Control::NavLong) {                            // home = PERFORM, también desde PANEL (en FX, FX es su home)
        if (!ui_.mode) show(PERFORM);
        return {};
    }
    if (c == Control::Mode) {
        ui_.mode = ui_.mode ? 0 : 1;
        ui_.shift = 0;
        ui_.fx_hold = 0;
        show(ui_.mode ? FX : last_view_);
        return {};
    }
    return ui_.mode ? handle_fx(c, v) : handle_synth(c, v);
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
                show(PERFORM);
                return {Action::LoadEngine, engine_};
            case Control::Key4: show(PERFORM); break;
            case Control::Key1: show(OSC); break;
            case Control::Key2: show(ENV); break;
            case Control::Key3: show(LFO); break;
            default:
                if (c >= Control::Pick0 && c <= Control::Pick5) {
                    engine_ = engine_cursor_ = uint8_t(uint8_t(c) - uint8_t(Control::Pick0));
                    show(PERFORM);
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
                    if (preset_count_ < 16) ++preset_count_;
                    return {Action::SavePreset, uint8_t(preset_count_ - 1)};
                }
                return {Action::LoadPreset, uint8_t(preset_cursor_ - 1)};
            case Control::Key4:
                engine_cursor_ = engine_;
                show(ENGINE);
                break;
            case Control::Key1: show(OSC); break;
            case Control::Key2: show(ENV); break;
            case Control::Key3: show(LFO); break;
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
        case Control::Key3: toggle_page(LFO); break;
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
