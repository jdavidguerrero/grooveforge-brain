#include "hybrid_preview.h"
#include <cmath>
#include <cstdio>
#include <memory>
int main() {
    auto hybrid = std::make_unique<gf::HybridPreview>();
    for (double fs : {44100.0, 48000.0, 96000.0}) {
        hybrid->prepare(fs);
        for (uint8_t index = 0; index < studio::factory_layer_count(); ++index) {
            studio::LayerPreset layer;
            if (!studio::factory_layer(index, &layer) || !hybrid->load(layer)) return 1;
            hybrid->set_transport(120, false, 0, true);
            hybrid->note_on(60, .8f);
            double energy = 0;
            for (int i = 0; i < int(fs * .15); ++i) {
                float left, right; hybrid->process_frame(left, right);
                if (!std::isfinite(left) || !std::isfinite(right)) return 1;
                energy += left * left + right * right;
            }
            if (energy < .0001) { std::fprintf(stderr, "HYBRID layer %u silent at %.0f\n", unsigned(index), fs); return 1; }
            hybrid->note_off(60);
            for (float morph : {0.0f, 1.0f}) {
                if (!hybrid->load(layer)) return 1;
                hybrid->set_morph(morph); hybrid->note_on(60, .8f);
                double half_energy = 0;
                for (int i = 0; i < int(fs * .2); ++i) {
                    float left, right; hybrid->process_frame(left, right);
                    if (!std::isfinite(left + right)) return 1;
                    half_energy += left * left + right * right;
                }
                if (half_energy < .00001) { std::fprintf(stderr, "HYBRID layer %u side %.0f silent at %.0f\n", unsigned(index), morph, fs); return 1; }
                hybrid->note_off(60);
            }
        }
    }
    {
        gf::JunoPreview juno, reference;
        juno.prepare(48000); reference.prepare(48000);
        juno.note_on(60, 1); reference.note_on(60, 1); juno.set_tape_pitch(1.05f);
        double change = 0;
        for (int i = 0; i < 10000; ++i) {
            float l, r, a, b; juno.process_frame(l, r); reference.process_frame(a, b);
            if (!std::isfinite(l + r + a + b)) return 1;
            change += std::abs(l - a);
        }
        if (change < .01) return 1;
    }
    {
        gf::ProphetPreview prophet, reference;
        prophet.prepare(48000); reference.prepare(48000);
        prophet.note_on(60, 1); reference.note_on(60, 1); prophet.set_external_tape_pitch(1.05f);
        double change = 0;
        for (int i = 0; i < 10000; ++i) {
            const float a = prophet.process_sample(), b = reference.process_sample();
            if (!std::isfinite(a + b)) return 1;
            change += std::abs(a - b);
        }
        if (change < .01) return 1;
    }
    return 0;
}
