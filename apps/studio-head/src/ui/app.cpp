#include "ui/app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "state/fx_catalog.h"
#include "studio_tick.h"

namespace {

constexpr uint8_t FAV_FX_SLOT = 2;  // B4 en FX: favorito (demo: GHOST ECHO; luego, configurable)
constexpr float VOL_MIN = -60.0f, VOL_MAX = 6.0f, IN_MIN = -12.0f, IN_MAX = 12.0f, POT_STEP = 0.5f;

bool is_view(Page p) { return p == Page::Perform || p == Page::Ai || p == Page::Spectrum; }

}  // namespace

Screen* App::screen(Page p) {
    switch (p) {
        case Page::Perform:  return &_perform;
        case Page::Ai:       return &_ai;
        case Page::Spectrum: return &_spectrum;
        case Page::Env:      return &_env;
        case Page::Fx:       return &_fx;
        case Page::Engine:   return &_engine;
        case Page::Presets:  return &_presets;
        case Page::Error:    return &_error;
        case Page::Osc:      return &_osc;
        case Page::Lfo:      return &_lfo;
        default:             return &_perform;
    }
}

void App::create() {
    for (int i = 0; i < int(Page::Count); ++i) _roots[i] = screen(Page(i))->create();
    _overlays.create();
}

void App::show(Page p) {
    _page = p;
    if (is_view(p)) _last_view = p;
    lv_scr_load(_roots[int(p)]);
}

// B1 / B2 / B3: abre la página de edición; la misma tecla otra vez vuelve a la última vista.
void App::toggle_page(Page p) {
    show(_page == p ? _last_view : p);
}

void App::load_engine(uint8_t id, StudioState& s) {
    s.engine = id;
    show(Page::Perform);
}

void App::handle(Ctl c, StudioState& s) {
    // VOL y la simulación del enlace funcionan siempre, también con el error en pantalla (08d).
    switch (c) {
        case Ctl::VolUp:   s.volume_db = std::min(VOL_MAX, s.volume_db + POT_STEP); return;
        case Ctl::VolDown: s.volume_db = std::max(VOL_MIN, s.volume_db - POT_STEP); return;
        case Ctl::LinkToggle:
            s.link = s.link == LinkState::Lost ? LinkState::Demo : LinkState::Lost;
            return;
        default: break;
    }
    if (_page == Page::Error) {
        if (c == Ctl::NavPush) s.link = LinkState::Demo;  // RETRY NOW (en el demo reconecta al toque)
        return;
    }

    switch (c) {
        case Ctl::InUp:   s.in_gain_db = std::min(IN_MAX, s.in_gain_db + POT_STEP); return;
        case Ctl::InDown: s.in_gain_db = std::max(IN_MIN, s.in_gain_db - POT_STEP); return;
        case Ctl::PadCycle:
            s.in_pad_db = s.in_pad_db == 0 ? -10 : s.in_pad_db == -10 ? -20 : 0;
            return;
        case Ctl::Mode:
            s.mode = s.mode == Mode::Fx ? Mode::Synth : Mode::Fx;
            s.shift = false;
            s.fx_hold = false;
            show(s.mode == Mode::Fx ? Page::Fx : _last_view);
            return;
        default: break;
    }
    if (s.mode == Mode::Fx) handle_fx(c, s);
    else handle_synth(c, s);
}

void App::handle_fx(Ctl c, StudioState& s) {
    switch (c) {
        case Ctl::NavNext:
        case Ctl::NavPrev:
            s.fx_slot = fx_next_slot(s.fx_slot, c == Ctl::NavNext ? 1 : -1);
            _fx.show_list();
            break;
        case Ctl::NavPush:    s.fx_on ^= uint16_t(1u << s.fx_slot); break;
        case Ctl::B1:         s.fx_route[s.fx_slot] = FxRoute::Insert; break;
        case Ctl::B2:         s.fx_route[s.fx_slot] = FxRoute::Send; break;
        case Ctl::B3:         s.fx_route[s.fx_slot] = FxRoute::Master; break;
        case Ctl::B4:         s.fx_slot = FAV_FX_SLOT; break;
        case Ctl::HoldOn:     s.fx_hold = true; break;
        case Ctl::HoldOff:    s.fx_hold = false; break;
        case Ctl::HoldToggle: s.fx_hold = !s.fx_hold; break;
        case Ctl::FxWave:      s.fx_view = FxView::Wave; break;
        case Ctl::FxSpectrum:  s.fx_view = FxView::Spectrum; break;
        case Ctl::FxSignature: s.fx_view = FxView::Signature; break;
        case Ctl::FxViewNext:  s.fx_view = FxView((int(s.fx_view) + 1) % 3); break;
        default: break;  // SHIFT y ENC L push no existen en FX
    }
}

void App::handle_synth(Ctl c, StudioState& s) {
    switch (c) {
        case Ctl::EncLPush: s.filter_bypass = !s.filter_bypass; return;
        case Ctl::Shift:    s.shift = !s.shift; return;
        default: break;
    }

    if (_page == Page::Engine) {
        switch (c) {
            case Ctl::NavNext: _engine.move_cursor(1); break;
            case Ctl::NavPrev: _engine.move_cursor(-1); break;
            case Ctl::NavPush: load_engine(uint8_t(_engine.cursor()), s); break;
            case Ctl::B4:      show(Page::Perform); break;
            case Ctl::B1:      show(Page::Osc); break;
            case Ctl::B2:      show(Page::Env); break;
            case Ctl::B3:      show(Page::Lfo); break;
            default:
                if (c >= Ctl::Pick0 && c <= Ctl::Pick5) load_engine(uint8_t(int(c) - int(Ctl::Pick0)), s);
                break;
        }
        return;
    }

    if (_page == Page::Presets) {
        switch (c) {
            case Ctl::NavNext: _presets.move_cursor(1, s); break;
            case Ctl::NavPrev: _presets.move_cursor(-1, s); break;
            case Ctl::NavPush:
                if (_presets.cursor() == 0) {  // guardar en el próximo slot libre
                    if (s.preset_count < PRESET_MAX) {
                        std::snprintf(s.preset_names[s.preset_count], sizeof(s.preset_names[0]), "USER %02d", s.preset_count + 1);
                        s.preset_loaded = s.preset_count++;
                    }
                } else {
                    s.preset_loaded = uint8_t(_presets.cursor() - 1);
                }
                std::snprintf(s.preset, sizeof(s.preset), "A-%02d %s", s.preset_loaded + 1, s.preset_names[s.preset_loaded]);
                break;
            case Ctl::B4:
                _engine.set_cursor(s.engine);
                show(Page::Engine);
                break;
            case Ctl::B1: show(Page::Osc); break;
            case Ctl::B2: show(Page::Env); break;
            case Ctl::B3: show(Page::Lfo); break;
            default: break;
        }
        return;
    }

    if (_page == Page::Osc && (c == Ctl::NavNext || c == Ctl::NavPrev)) {
        _osc.move_selection(c == Ctl::NavNext ? 1 : -1, s);
        return;
    }
    if (_page == Page::Lfo && c == Ctl::NavPush) {
        s.lfo_sync = !s.lfo_sync;
        if (s.lfo_sync) {  // al activar SYNC, RATE salta a la división más cercana al BPM
            static const float BEATS[4] = {1.0f, 0.5f, 0.25f, 1.0f / 3.0f};
            float best = 1e9f;
            for (uint8_t i = 0; i < 4; ++i) {
                const float hz = s.bpm / 60.0f / BEATS[i];
                const float d = std::fabs(std::log(hz / s.lfo_rate_hz));
                if (d < best) { best = d; s.lfo_div = i; }
            }
        }
        return;
    }

    switch (c) {
        case Ctl::NavNext:
        case Ctl::NavPrev:
            if (is_view(_page)) {
                const int step = c == Ctl::NavNext ? 1 : 2;  // +1 o −1 módulo 3
                show(Page((int(_page) + step) % 3));
            }
            break;
        case Ctl::B1: toggle_page(Page::Osc); break;
        case Ctl::B2: toggle_page(Page::Env); break;
        case Ctl::B3: toggle_page(Page::Lfo); break;
        case Ctl::B4:
            show(Page::Presets);
            break;
        case Ctl::EngineCard:
            _engine.set_cursor(s.engine);
            show(Page::Engine);
            break;
        default:
            break;  // NAV push en las vistas: sin función (v3)
    }
}

void App::update(const StudioState& s) {
    if (s.link == LinkState::Lost && _page != Page::Error) {
        _before_error = _page;
        show(Page::Error);
    } else if (s.link != LinkState::Lost && _page == Page::Error) {
        show(_before_error);
    }
    screen(_page)->update(s);
    _overlays.update(s, studio_tick_ms());
}
