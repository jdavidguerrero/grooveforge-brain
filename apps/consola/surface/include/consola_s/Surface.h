#pragma once
#include <stddef.h>
#include <stdint.h>
namespace surface {
/** Loop-only extension for bounded CDC responses (return true when handled). */
using CommandHandler = bool (*)(char*, char*, size_t);
/** Loop-only notification of a filtered MASTER/CUE/IN change. */
using PotHandler = void (*)(uint8_t, uint8_t);
/** Append space-prefixed diagnostics to the STATS line, without a newline. */
using StatsHandler = void (*)(char*, size_t);
/** Initialize shared buses, ADCs and the FSR timer; never waits for a USB host. */
void begin(CommandHandler commands = nullptr, PotHandler pots = nullptr, StatsHandler stats = nullptr);
/** Service bounded surface work and CDC; all I2C transactions run here or at boot. */
void poll();
/** True while common EN recovery owns the bus; outer codec/CV must defer I2C. */
bool i2c_paused();
/** Include outer audio/control work in the STATS loop maximum. */
void record_loop_duration(uint32_t duration_us);
}
