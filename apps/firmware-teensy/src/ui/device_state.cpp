/**
 * @file device_state.cpp
 * @brief Persistencia de estado en EEPROM — GrooveForge Brain (Sprint 35)
 *
 * Implementación usando EEPROM.h de Teensyduino.
 *
 * Detalles de EEPROM.put() en Teensyduino:
 *   - Escribe byte a byte usando EEPROM.update() internamente.
 *   - EEPROM.update() solo escribe si el byte cambió → wear-leveling básico.
 *   - Cada write consume un ciclo del EEPROM flash; read es gratis.
 *   - La dirección 0 está disponible (no reservada por el sistema).
 *
 * Validaciones en device_state_load():
 *   - magic == DEVICE_STATE_MAGIC (detecta EEPROM virgen o corrupta)
 *   - display_mode <= 1 (0=FX, 1=SYNTH — AI no se persiste)
 *   - active_engine <= 2 (0-2 son los únicos engines válidos)
 *   Si cualquier campo está fuera de rango → estado inválido → usar defaults.
 */

#include "device_state.h"
#include <EEPROM.h>
#include <Arduino.h>

void device_state_save(const DeviceState& s) {
    EEPROM.put(DEVICE_STATE_ADDR, s);
    Serial.printf("[STATE] saved — mode=%u engine=%u\n",
                  (unsigned)s.display_mode, (unsigned)s.active_engine);
}

bool device_state_load(DeviceState& s) {
    DeviceState tmp;
    EEPROM.get(DEVICE_STATE_ADDR, tmp);

    /* Validar magic byte */
    if (tmp.magic != DEVICE_STATE_MAGIC) {
        Serial.printf("[STATE] EEPROM virgen o magic inválido (got 0x%02X, expected 0x%02X)\n",
                      (unsigned)tmp.magic, (unsigned)DEVICE_STATE_MAGIC);
        return false;
    }

    /* Validar rangos */
    if (tmp.display_mode > 1) {
        Serial.printf("[STATE] display_mode fuera de rango (%u) — usando default\n",
                      (unsigned)tmp.display_mode);
        return false;
    }
    if (tmp.active_engine > 2) {
        Serial.printf("[STATE] active_engine fuera de rango (%u) — usando default\n",
                      (unsigned)tmp.active_engine);
        return false;
    }

    s = tmp;
    Serial.printf("[STATE] restored — mode=%u engine=%u\n",
                  (unsigned)s.display_mode, (unsigned)s.active_engine);
    return true;
}

DeviceState device_state_default() {
    DeviceState s = {};
    s.magic         = DEVICE_STATE_MAGIC;
    s.display_mode  = 1;  /* SYNTH */
    s.active_engine = 0;  /* Moog Model D */
    return s;
}
