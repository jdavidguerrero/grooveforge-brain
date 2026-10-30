#include "Audio.h"

#include <cassert>
#include <cstring>

std::vector<AudioStream*> AudioStream::streams_;

AudioStream::AudioStream(unsigned inputs, audio_block_t** queue) : inputs_(inputs), queue_(queue) {
    for (unsigned i = 0; i < inputs; ++i) queue[i] = nullptr;
    streams_.push_back(this);
}

void AudioStream::update_all() {
    for (auto* stream : streams_) stream->update();
}

void AudioStream::release(audio_block_t* block) {
    if (--block->refs == 0) delete block;
}

audio_block_t* AudioStream::receiveReadOnly(unsigned index) {
    if (index >= inputs_) return nullptr;
    auto* block = queue_[index];
    queue_[index] = nullptr;
    return block;
}

audio_block_t* AudioStream::receiveWritable(unsigned index) {
    auto* block = receiveReadOnly(index);
    if (!block || block->refs == 1) return block;
    auto* copy = allocate();
    std::memcpy(copy->data, block->data, sizeof(copy->data));
    release(block);
    return copy;
}

void AudioStream::transmit(audio_block_t* block, unsigned output) {
    for (auto* c : connections_) {
        if (c->output_ != output) continue;
        auto& slot = c->destination_.queue_[c->input_];
        if (!slot) { slot = block; ++block->refs; }
    }
}

AudioConnection::AudioConnection(AudioStream& source, unsigned output, AudioStream& destination, unsigned input)
    : destination_(destination), output_(output), input_(input) {
    assert(input < destination.inputs_);
    source.connections_.push_back(this);
}

void AudioMixer4::gain(unsigned channel, float value) {
    if (channel < 4) multiplier_[channel] = int32_t(std::clamp(value, -32767.0f, 32767.0f) * 65536.0f);
}

void AudioMixer4::update() {
    // Scalar equivalent of PJRC Audio/mixer.cpp (ARM Q16 gain and saturated addition).
    audio_block_t* out = nullptr;
    for (unsigned ch = 0; ch < 4; ++ch) {
        audio_block_t* in = out ? receiveReadOnly(ch) : receiveWritable(ch);
        if (!in) continue;
        if (!out) {
            out = in;
            if (multiplier_[ch] == 65536) continue;
            for (auto& sample : out->data) {
                const int32_t v = int32_t((int64_t(sample) * multiplier_[ch]) >> 16);
                sample = int16_t(std::clamp(v, -32768, 32767));
            }
        } else {
            for (unsigned i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
                const int32_t scaled = std::clamp(int32_t((int64_t(in->data[i]) * multiplier_[ch]) >> 16), -32768, 32767);
                out->data[i] = int16_t(std::clamp(int32_t(out->data[i]) + scaled, -32768, 32767));
            }
            release(in);
        }
    }
    if (out) { transmit(out); release(out); }
}
