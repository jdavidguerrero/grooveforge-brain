#pragma once
#include "params.h"
#include <array>
namespace host {
/** @brief Adapter contract: all methods except construction/destruction run without allocation. */
class Engine {
public:
    Config config;
    const std::vector<Param>& params;
    explicit Engine(const Config& c):config(c),params(catalog(c.engine)) {}
    virtual ~Engine()=default;
    /** @brief Apply current POD patch without resetting active voices. */
    virtual void apply() noexcept=0;
    /** @brief Render a stereo frame. */
    virtual void frame(float& l,float& r) noexcept=0;
    /** @brief MIDI note, normalized velocity (zero releases). */
    virtual void note(uint8_t n,float velocity) noexcept=0;
    /** @brief MIDI sustain. */
    virtual void sustain(bool down) noexcept=0;
    /** @brief MIDI pitch bend, 0..16383. */
    virtual void bend(int value) noexcept=0;
};
/** @brief Constructs and prepares an engine on control. */
std::unique_ptr<Engine> make_engine(const Config&);
}
