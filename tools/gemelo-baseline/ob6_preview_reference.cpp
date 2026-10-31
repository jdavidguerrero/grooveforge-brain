#include "ob6_preview.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string_view>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string_view name(argv[1]);
    if (name != "chord" && name != "notch" && name != "bandpass" && name != "sync" &&
        name != "xmod" && name != "white_noise" && name != "voice_steal" && name != "glide_voice_steal") return 2;
    gf::Ob6Preview ob6;
    ob6.prepare(44100);
    if (name != "chord" && name != "voice_steal") {
        gf::Ob6SoundPatch patch;
        if (name == "notch") patch.morph = .5f;
        if (name == "bandpass") patch.bandpass = true;
        if (name == "sync") { patch.interval = 7; patch.sync = true; }
        if (name == "xmod") { patch.xmod_env = .4f; patch.xmod_vco2 = .7f; patch.xmod_freq = true; patch.xmod_pw = true; patch.shape1 = .5f; }
        if (name == "white_noise") patch.noise = .6f;
        if (name == "glide_voice_steal") patch.glide_ms = 180;
        ob6.set_sound_patch(patch);
    }
    constexpr uint8_t notes[] = {48, 55, 60, 64, 67, 72, 76};
    const bool steal = name == "voice_steal" || name == "glide_voice_steal";
    for (int i = 0; i < (steal ? 6 : 3); ++i) ob6.note_on(notes[i], .8f);
    for (int block = 0; block < 690; ++block) {
        if (block == 172 && steal) ob6.note_on(notes[6], .8f);
        if (block == 345) for (auto note : notes) ob6.note_off(note);
        for (int sample = 0; sample < 128; ++sample) {
            const float value = ob6.process_sample();
            if (!std::isfinite(value)) return 1;
            const auto pcm = uint16_t(int16_t(std::clamp(int(value * 32767.0f), -32768, 32767)));
            const uint8_t bytes[] = {uint8_t(pcm), uint8_t(pcm >> 8)};
            if (std::fwrite(bytes, 1, 2, stdout) != 2) return 1;
        }
    }
}
