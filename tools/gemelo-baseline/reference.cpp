#include "Audio.h"
#include "engines/prophet_5.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

static uint32_t virtual_ms = 0;
uint32_t millis() { return virtual_ms; }

class Capture : public AudioStream {
public:
    Capture() : AudioStream(1, queue_) {}
    void update() override {
        auto* block = receiveReadOnly(0);
        if (block) { std::memcpy(samples, block->data, sizeof(samples)); release(block); }
        else std::memset(samples, 0, sizeof(samples));
    }
    int16_t samples[AUDIO_BLOCK_SAMPLES]{};
private:
    audio_block_t* queue_[1]{};
};

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: reference CASE(1-6)\n"); return 2; }
    const int scenario = std::atoi(argv[1]);
    if (scenario < 1 || scenario > 6) return 2;
    Prophet5 prophet;
    Capture capture;
    AudioConnection connection(prophet.getOutput(), 0, capture, 0);
    prophet.begin();
    if (scenario == 2) { prophet.setOscAShape(false, true); prophet.setOscAPw(0.35f); }
    if (scenario == 3) prophet.setPolyMod(0.6f, 0.45f, true, false, true);
    if (scenario == 4) prophet.setGlide(180.0f);
    if (scenario == 5) prophet.setFilterResonance(4.9f);
    const uint8_t notes[] = {48, 55, 60, 64, 67, 72};
    const int count = scenario == 6 ? 5 : 3;
    for (int i = 0; i < count; ++i) prophet.noteOn(notes[i], 0.8f);

    // MIDI and control occur on 128-sample boundaries, matching the historical graph reference.
    for (unsigned block = 0; block < 690; ++block) {
        if (block == 172 && (scenario == 4 || scenario == 6)) prophet.noteOn(notes[5], 0.8f);
        if (block == 345) {
            for (int i = 0; i < count; ++i) prophet.noteOff(notes[i]);
            prophet.noteOff(notes[5]);
        }
        const uint32_t target_ms = uint32_t(uint64_t(block) * AUDIO_BLOCK_SAMPLES * 1000 / 44100);
        while (virtual_ms < target_ms) {
            ++virtual_ms;
            if (scenario == 2) {
                LfoMod mod;
                mod.pwm = 0.5f * std::sin(float(virtual_ms) * 0.0062831853f);
                prophet.setLfoMod(mod);
            }
            prophet.update();
        }
        AudioStream::update_all();
        if (std::fwrite(capture.samples, sizeof(int16_t), AUDIO_BLOCK_SAMPLES, stdout) != AUDIO_BLOCK_SAMPLES) return 1;
    }
    return 0;
}
