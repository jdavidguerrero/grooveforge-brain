#include "Audio.h"
#include "engines/ob6.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static uint32_t virtual_ms = 0;
uint32_t millis() { return virtual_ms; }
class Capture : public AudioStream {
public:
    Capture() : AudioStream(1, queue_) {}
    void update() override {
        auto* b = receiveReadOnly(0);
        if (b) { std::memcpy(samples, b->data, sizeof(samples)); release(b); }
        else std::memset(samples, 0, sizeof(samples));
    }
    int16_t samples[AUDIO_BLOCK_SAMPLES]{};
private:
    audio_block_t* queue_[1]{};
};
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const int scenario = std::atoi(argv[1]);
    if (scenario < 1 || scenario > 8) return 2;
    OB6 ob6;
    Capture capture;
    AudioConnection output(ob6.getOutput(), 0, capture, 0);
    ob6.begin();
    if (scenario == 2) ob6.setFilterMorph(.5f);
    if (scenario == 3) ob6.setFilterBpMode(true);
    if (scenario == 4) { ob6.setVco2Interval(7); ob6.setSync(true); }
    if (scenario == 5) { ob6.setXMod(.4f, .7f, true, false, true); ob6.setVcoShape(0, .5f); }
    if (scenario == 6) ob6.setOscMix(.45f, .45f, .1f, .6f);
    if (scenario == 8) ob6.setGlide(180);
    constexpr uint8_t notes[] = {48, 55, 60, 64, 67, 72, 76};
    const int count = scenario >= 7 ? 6 : 3;
    for (int i = 0; i < count; ++i) ob6.noteOn(notes[i], .8f);
    for (unsigned block = 0; block < 690; ++block) {
        if (block == 172 && scenario >= 7) ob6.noteOn(notes[6], .8f);
        if (block == 345) for (auto note : notes) ob6.noteOff(note);
        const uint32_t target = uint32_t(uint64_t(block) * AUDIO_BLOCK_SAMPLES * 1000 / 44100);
        while (virtual_ms < target) { ++virtual_ms; ob6.update(); }
        AudioStream::update_all();
        if (std::fwrite(capture.samples, sizeof(int16_t), AUDIO_BLOCK_SAMPLES, stdout) != AUDIO_BLOCK_SAMPLES) return 1;
    }
}
