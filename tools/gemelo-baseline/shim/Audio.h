#pragma once

#include "Arduino.h"
#include <algorithm>
#include <cstdint>
#include <vector>

constexpr int AUDIO_BLOCK_SAMPLES = 128;
constexpr float AUDIO_SAMPLE_RATE_EXACT = 44100.0f;
constexpr uint16_t WAVEFORM_SAWTOOTH = 1, WAVEFORM_SQUARE = 2, WAVEFORM_TRIANGLE = 3, WAVEFORM_PULSE = 5;

struct audio_block_t {
    int16_t data[AUDIO_BLOCK_SAMPLES]{};
    unsigned refs = 1;
};

class AudioConnection;

/** Minimal single-graph test harness for the Prophet firmware, not a production AudioStream. */
class AudioStream {
public:
    AudioStream(unsigned inputs, audio_block_t** queue);
    virtual ~AudioStream() = default;
    virtual void update() = 0;
    static void update_all();

protected:
    static audio_block_t* allocate() { return new audio_block_t; }
    static void release(audio_block_t* block);
    audio_block_t* receiveReadOnly(unsigned index);
    audio_block_t* receiveWritable(unsigned index);
    void transmit(audio_block_t* block, unsigned output = 0);

private:
    friend class AudioConnection;
    unsigned inputs_;
    audio_block_t** queue_;
    std::vector<AudioConnection*> connections_;
    static std::vector<AudioStream*> streams_;
};

class AudioConnection {
public:
    AudioConnection(AudioStream& source, unsigned output, AudioStream& destination, unsigned input);
private:
    friend class AudioStream;
    AudioStream& destination_;
    unsigned output_, input_;
};

class AudioMixer4 : public AudioStream {
public:
    AudioMixer4() : AudioStream(4, queue_) {}
    void gain(unsigned channel, float value);
    void update() override;
private:
    audio_block_t* queue_[4]{};
    int32_t multiplier_[4] = {65536, 65536, 65536, 65536};
};

/** Scalar PJRC-compatible reference noise; independent modulo formulation for test graphs. */
class AudioSynthNoiseWhite : public AudioStream {
public:
    AudioSynthNoiseWhite() : AudioStream(0, nullptr), seed_(next_seed_++) {}
    void amplitude(float n) { level_ = int32_t(std::clamp(n, 0.0f, 1.0f) * 65536.0f); }
    void update() override {
        if (!level_) return;
        auto* block = allocate();
        if (!block) return;
        for (auto& sample : block->data) {
            seed_ = uint32_t((uint64_t(seed_) * 16807u) % 2147483647u);
            int32_t signed_low = int32_t(seed_ & 0xffffu);
            if (signed_low >= 32768) signed_low -= 65536;
            sample = int16_t((int64_t(level_) * signed_low) >> 16);
        }
        transmit(block); release(block);
    }
private:
    inline static uint32_t next_seed_ = 1;
    uint32_t seed_;
    int32_t level_ = 0;
};
