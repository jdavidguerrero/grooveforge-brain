#pragma once
#include <stdint.h>

/** Conserva transiciones parciales y produce CC relativo de 7 bits. */
class RelativeEncoder {
public:
    /** Añade delta de cuadratura; devuelve 0 si aún no completa un detent. */
    uint8_t update(int32_t delta);
    /** Codifica detents en complemento a dos MIDI; satura a ±63. */
    static uint8_t encode(int32_t detents);
private:
    int64_t remainder_ = 0;
};
