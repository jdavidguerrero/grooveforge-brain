#include "hybrid_router.h"
#include <array>
#include <cmath>
#include <cstdio>
struct Log { struct Event { int layer, note; bool on; }; std::array<Event, 40> events{}; int count = 0; };
static void on(void* ctx, uint8_t layer, uint8_t note, float) { auto& l = *static_cast<Log*>(ctx); l.events[l.count++] = {layer, note, true}; }
static void off(void* ctx, uint8_t layer, uint8_t note) { auto& l = *static_cast<Log*>(ctx); l.events[l.count++] = {layer, note, false}; }
int main() {
    gf::HybridRouter router; Log log;
    router.set_callbacks(&log, on, off); router.prepare(48000);
    router.configure(0, -6, -3, 10, 12);
    float left = 0, right = 0;
    router.mix(1, 1, 1, 1, left, right);
    if (std::abs(left - std::pow(10.0f, -.3f)) > 1e-5f || left != right) return 1;
    router.set_morph(1); router.mix(1, 1, 1, 1, left, right);
    if (std::abs(left - std::pow(10.0f, -.15f)) > 1e-5f) return 1;
    router.set_morph(.5f); router.mix(1, 1, 1, 1, left, right);
    if (std::abs(left - (std::pow(10.0f, -.3f) + std::pow(10.0f, -.15f))) > 1e-5f) return 1;
    router.note_on(60, 1);
    if (log.count != 1 || log.events[0].layer != 0 || log.events[0].note != 60) return 1;
    for (int i = 0; i < 480; ++i) router.tick();
    if (log.count != 1) return 1;
    router.tick();
    if (log.count != 2 || log.events[1].layer != 1 || log.events[1].note != 72) return 1;
    router.configure(.5f, 0, 0, 0, -12); router.note_off(60);
    if (log.count != 4 || log.events[3].note != 72 || log.events[3].on) return 1;
    router.configure(.5f, 0, 0, 10, 0); router.note_on(61, 1); router.note_off(61);
    for (int i = 0; i < 500; ++i) router.tick();
    if (log.count != 6) return 1;
    router.configure(.5f, 0, 0, 0, 12); router.note_on(120, 1); router.note_on(121, 1);
    if (log.count != 9 || log.events[7].note != 127 || log.events[7].layer != 1) return 1;
    router.note_off(120);
    if (log.count != 10 || log.events[9].layer != 0) return 1;
    router.note_off(121);
    if (log.count != 12 || log.events[11].note != 127 || log.events[11].on) return 1;
    router.note_on(64, 1); router.reset();
    if (log.count != 16 || log.events[14].on || log.events[15].on) return 1;
    return 0;
}
