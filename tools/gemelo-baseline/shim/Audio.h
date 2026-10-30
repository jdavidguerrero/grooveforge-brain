#pragma once

#include "Arduino.h"
#include <algorithm>
#include <cstdint>
#include <vector>

constexpr int AUDIO_BLOCK_SAMPLES = 128;
constexpr float AUDIO_SAMPLE_RATE_EXACT = 44100.0f;
constexpr uint16_t WAVEFORM_SQUARE = 2, WAVEFORM_TRIANGLE = 3, WAVEFORM_PULSE = 5;

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

/** Silent placeholder: Juno reference scenarios require noise gain zero. */
class AudioSynthNoiseWhite : public AudioStream {
public:
    AudioSynthNoiseWhite() : AudioStream(0, nullptr) {}
    void amplitude(float) {}
    void update() override {
        auto* block = allocate();
        if (block) { transmit(block); release(block); }
    }
};
