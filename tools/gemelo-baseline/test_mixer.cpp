#include "Audio.h"

#include <cassert>
#include <cstdio>

uint32_t millis() { return 0; }

class Source : public AudioStream {
public:
    explicit Source(int16_t value) : AudioStream(0, nullptr), value_(value) {}
    void update() override {
        auto* block = allocate();
        for (auto& sample : block->data) sample = value_;
        transmit(block);
        release(block);
    }
private:
    int16_t value_;
};

class Sink : public AudioStream {
public:
    Sink() : AudioStream(1, queue_) {}
    void update() override {
        auto* block = receiveReadOnly(0);
        assert(block);
        value = block->data[0];
        release(block);
    }
    int16_t value = 0;
private:
    audio_block_t* queue_[1]{};
};

int main() {
    Source positive(30000), negative(-30001);
    AudioMixer4 mixer;
    Sink mixed, original;
    AudioConnection a(positive, 0, mixer, 0), b(negative, 0, mixer, 1);
    AudioConnection c(mixer, 0, mixed, 0), d(positive, 0, original, 0);
    mixer.gain(0, 1.0f);
    mixer.gain(1, 0.25f);
    AudioStream::update_all();
    assert(mixed.value == 22499 && original.value == 30000); // Q16 floor and copy-on-write.
    mixer.gain(0, 2.0f);
    mixer.gain(1, 0.0f);
    AudioStream::update_all();
    assert(mixed.value == 32767 && original.value == 30000);
    mixer.gain(0, -2.0f);
    AudioStream::update_all();
    assert(mixed.value == -32768 && original.value == 30000);
    std::puts("mixer Q16, saturation and shared-block copy: OK");
}
