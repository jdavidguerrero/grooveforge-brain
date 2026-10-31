#include "Audio.h"
#include "engines/juno_106.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static uint32_t virtual_ms = 0;
uint32_t millis() { return virtual_ms; }

class Capture : public AudioStream {
public:
    Capture() : AudioStream(2, queue_) {}
    void update() override {
        for (unsigned ch = 0; ch < 2; ++ch) {
            auto* b = receiveReadOnly(ch);
            if (b) { std::memcpy(samples[ch], b->data, sizeof(samples[ch])); release(b); }
            else std::memset(samples[ch], 0, sizeof(samples[ch]));
        }
    }
    int16_t samples[2][AUDIO_BLOCK_SAMPLES]{};
private:
    audio_block_t* queue_[2]{};
};

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const int scenario = std::atoi(argv[1]);
    if (scenario < 1 || scenario > 8) return 2;
    Juno106 juno;
    Capture capture;
    AudioConnection left(juno.getOutput(), 0, capture, 0);
    AudioConnection right(juno.getOutput(), 1, capture, 1);
    juno.begin();
    juno.setNoiseLevel(scenario == 8 ? 0.6f : 0.0f);
    if (scenario == 2) { juno.setWaveform(WAVEFORM_PULSE); juno.setPulseWidth(0.35f); }
    if (scenario == 3) juno.setGlide(180);
    if (scenario == 5) juno.setHpf(3);
    if (scenario == 6) juno.setChorusMode(2);
    if (scenario == 7) juno.setChorusMode(3);
    const uint8_t notes[] = {48, 55, 60, 64, 67, 72, 76};
    const int count = scenario == 4 ? 6 : 3;
    for (int i = 0; i < count; ++i) juno.noteOn(notes[i], 0.8f);
    for (unsigned block = 0; block < 690; ++block) {
        if (block == 172 && (scenario == 3 || scenario == 4)) juno.noteOn(notes[6], 0.8f);
        if (block == 345) for (uint8_t note : notes) juno.noteOff(note);
        const uint32_t target = uint32_t(uint64_t(block) * AUDIO_BLOCK_SAMPLES * 1000 / 44100);
        while (virtual_ms < target) {
            ++virtual_ms;
            if (scenario == 2) { LfoMod mod; mod.pwm = 0.5f * std::sin(float(virtual_ms) * 0.0062831853f); juno.setLfoMod(mod); }
            juno.update();
        }
        AudioStream::update_all();
        for (unsigned i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
            for (unsigned ch = 0; ch < 2; ++ch)
                if (std::fwrite(&capture.samples[ch][i], sizeof(int16_t), 1, stdout) != 1) return 1;
    }
}
