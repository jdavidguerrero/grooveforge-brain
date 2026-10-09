#include "ui/app.h"

namespace {

bool is_view(Page p) { return p == Page::Perform || p == Page::Ai || p == Page::Spectrum; }

}  // namespace

Screen* App::screen(Page p) {
    switch (p) {
        case Page::Perform:  return &_perform;
        case Page::Ai:       return &_ai;
        case Page::Spectrum: return &_spectrum;
        case Page::Env:      return &_env;
        default:             return &_perform;
    }
}

void App::create() {
    for (int i = 0; i < int(Page::Count); ++i) _roots[i] = screen(Page(i))->create();
}

void App::show(Page p) {
    _page = p;
    if (is_view(p)) _last_view = p;
    lv_scr_load(_roots[int(p)]);
}

void App::handle(Ctl c, StudioState& s) {
    switch (c) {
        case Ctl::NavNext:
        case Ctl::NavPrev:
            if (is_view(_page)) {
                const int step = c == Ctl::NavNext ? 1 : 2;  // +1 o −1 módulo 3
                show(Page((int(_page) + step) % 3));
            }
            break;
        case Ctl::B2:
            show(_page == Page::Env ? _last_view : Page::Env);
            break;
        case Ctl::Mode:
            s.mode = s.mode == Mode::Fx ? Mode::Synth : Mode::Fx;
            break;
        case Ctl::Shift:
            s.shift = !s.shift;
            break;
        default:
            break;  // B1, B3, B4 y NAV push: páginas pendientes (OSC, LFO, PRESETS, FX MAIN)
    }
}

void App::update(const StudioState& s) {
    screen(_page)->update(s);
}
