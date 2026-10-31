#include "juno_preview.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string_view scenario(argv[1]);
    if (scenario != "chord" && scenario != "pwm" && scenario != "glide" && scenario != "voice_steal" &&
        scenario != "hpf" && scenario != "chorus_ii" && scenario != "chorus_i_ii" && scenario != "white_noise" &&
        scenario != "glide_voice_steal" && scenario != "pwm_initialized") return 2;
    const bool noise = scenario == "white_noise";
    const bool pwm = scenario == "pwm" || scenario == "pwm_initialized";
    gf::JunoPreview juno;
    juno.prepare(44100);
    juno.set_parameters(1200, .707f, 10, 300);
    juno.set_patch(200, .5f, 10, 200, .5f, 300, .5f, 1.1f, .65f, .5f);
    juno.set_oscillator(pwm ? dsp::WAVE_PULSE : dsp::WAVE_SAW_AND_PULSE, .3f, noise ? .6f : 0.0f,
                        pwm ? .35f : .5f, scenario == "chorus_ii" ? 2 : scenario == "chorus_i_ii" ? 3 : 1,
                        scenario == "hpf" ? 3 : 1, 1, false, 0);
    if (pwm) juno.set_lfo(1.0f, dsp::LfoShape::Sine, 0, 0, .5f, .5f, 0);
    if (scenario == "glide" || scenario == "glide_voice_steal") juno.set_voice_modulation(180, 1, 0, 0);
    constexpr uint8_t notes[] = {48, 55, 60, 64, 67, 72, 76};
    const int count = (scenario == "voice_steal" || scenario == "glide_voice_steal") ? 6 : 3;
    for (int i = 0; i < count; ++i) juno.note_on(notes[i], .8f);
    for (int block = 0; block < 690; ++block) {
        if (block == 172 && (scenario == "glide" || scenario == "voice_steal" || scenario == "glide_voice_steal")) juno.note_on(notes[6], .8f);
        if (block == 345) for (auto note : notes) juno.note_off(note);
        for (int sample = 0; sample < 128; ++sample) {
            float left, right;
            juno.process_frame(left, right);
            for (float v : {left, right}) {
                if (!std::isfinite(v)) return 1;
                const int16_t pcm = int16_t(std::clamp(int(v * 32767.0f), -32768, 32767));
                const uint8_t bytes[] = {uint8_t(uint16_t(pcm)), uint8_t(uint16_t(pcm) >> 8)};
                if (std::fwrite(bytes, 1, 2, stdout) != 2) return 1;
            }
        }
    }
}
