#include "ob6_preview.h"
#include <cmath>
#include <cstdio>

static bool run(double fs) {
    gf::Ob6Preview a, b;
    a.prepare(fs); b.prepare(fs);
    a.set_sustain(true);
    for (uint8_t n : {48, 55, 60, 64, 67, 72, 76}) a.note_on(n, .8f);
    for (uint8_t n : {48, 55, 60, 64, 67, 72, 76}) a.note_off(n);
    double energy = 0;
    for (int i = 0; i < int(fs * .25); ++i) {
        const float x = a.process_sample(), silent = b.process_sample();
        if (!std::isfinite(x) || silent != 0) return false;
        energy += x*x;
    }
    if (energy < .01) return false;
    a.set_sustain(false);
    for (int i = 0; i < int(fs * 5); ++i) if (!std::isfinite(a.process_sample())) return false;
    return std::abs(a.process_sample()) < 1e-5f;
}
int main() {
    for (double fs : {44100.0, 48000.0, 96000.0})
        if (!run(fs)) { std::fprintf(stderr, "OB-6 pilot failed at %.0f Hz\n", fs); return 1; }
}
