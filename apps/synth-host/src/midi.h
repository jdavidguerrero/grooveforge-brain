#pragma once
#include <cstddef>
#include <cstdint>

namespace host {
struct Midi {uint32_t frame=0;uint8_t status=0,a=0,b=0;};

/** @brief Decode one complete MIDI1 channel voice message; no running status or allocation. */
bool decode_midi1(const void* data,size_t size,uint32_t frame,Midi& event) noexcept;

/** @brief Consume one native-endian UMP packet; zero on truncation, status=0 if ignored/invalid. */
size_t decode_ump(const void* data,size_t size,uint32_t frame,Midi& event) noexcept;

/** @brief Existing engine routing: synth channels 1..4, drum channel 10; notes, CC and bend. */
bool routed_midi(const Midi& event) noexcept;
}
