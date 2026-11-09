#include "Faders.h"
#include "MidiCommands.h"
#include "LiveController/LiveController.h"
#include <Arduino.h>

// Declare liveController as extern (defined in Hardware.cpp)
extern LiveController liveController;

Faders::Faders()
    : currentBankOffset(0)
    , paramMode(FaderParamMode::VOLUME_PAN)
    , onVolumeChange(nullptr)
    , onPickupStateChange(nullptr)
{
    for (int i = 0; i < NUM_FADERS; i++) {
        oldValues[i] = -1;

        // Inicializar pickup state
        pickupStates[i].physicalValue = 0;
        pickupStates[i].targetValue = 0;
        pickupStates[i].hasPickedUp = false;
        pickupStates[i].assignedTrackIndex = i;  // Bank 0 por defecto
    }
}

void Faders::begin() {
    // Configure ADC resolution (Teensy 4.1 supports up to 12-bit)
    analogReadResolution(12);  // 0-4095
    analogReadAveraging(4);    // Average 4 samples for noise reduction

    int pins[] = FADER_PINS;
    for (int i = 0; i < NUM_FADERS; i++) {
        pinMode(pins[i], INPUT);
        oldValues[i] = -1;

        // Read initial value to check range
        int testRead = analogRead(pins[i]);
        Serial.printf("Fader %d initialized (pin A%d) - ADC test: %d/4095\n",
                     i + 1, pins[i] - A0, testRead);
    }

    Serial.printf("Faders initialized: %d faders in bank %d (tracks %d-%d)\n",
                 NUM_FADERS, currentBankOffset,
                 currentBankOffset, currentBankOffset + NUM_FADERS - 1);
}

void Faders::setTrackBank(int bankOffset) {
    if (bankOffset == currentBankOffset) return;

    Serial.printf("Fader bank change: %d → %d (tracks %d-%d)\n",
                 currentBankOffset, bankOffset,
                 bankOffset, bankOffset + NUM_FADERS - 1);

    currentBankOffset = bankOffset;

    // RESET pickup state para todos los faders
    for (int i = 0; i < NUM_FADERS; i++) {
        pickupStates[i].assignedTrackIndex = bankOffset + i;
        pickupStates[i].hasPickedUp = false;

        // Notificar GUI que necesita pickup
        if (onPickupStateChange) {
            onPickupStateChange(i, true);  // needsPickup = true
        }

        Serial.printf("  Fader %d → Track %d (needs pickup)\n",
                     i, pickupStates[i].assignedTrackIndex);
    }
}

void Faders::onTrackVolumeUpdate(int trackIndex, int volume) {
    // Actualizar target value si este track está en banco actual
    for (int i = 0; i < NUM_FADERS; i++) {
        if (pickupStates[i].assignedTrackIndex == trackIndex) {
            pickupStates[i].targetValue = volume;

            // Re-check pickup si cambió el target
            // (auto-pickup si el fader físico ya está cerca)
            if (!pickupStates[i].hasPickedUp) {
                int diff = abs(pickupStates[i].physicalValue - volume);
                if (diff <= FADER_PICKUP_THRESHOLD) {
                    pickupStates[i].hasPickedUp = true;

                    if (onPickupStateChange) {
                        onPickupStateChange(i, false);  // needsPickup = false
                    }

                    Serial.printf("AUTO-PICKUP: Fader %d caught track %d (diff=%d)\n",
                                 i, trackIndex, diff);
                }
            }

            break;
        }
    }
}

void Faders::read() {
    int pins[] = FADER_PINS;
    static unsigned long lastRangeCheck = 0;
    static int maxRawValues[NUM_FADERS] = {0};  // Track max values seen
    static unsigned long lastPickupLog[NUM_FADERS] = {0};  // Rate limit pickup logs

    for (int i = 0; i < NUM_FADERS; i++) {
        // Leer ADC 12-bit y convertir a 7-bit MIDI (0-127)
        int rawValue = analogRead(pins[i]);

        // Track maximum value seen (for diagnostic)
        if (rawValue > maxRawValues[i]) {
            maxRawValues[i] = rawValue;
        }

        // Print range diagnostic every 5 seconds
        if (millis() - lastRangeCheck > 5000) {
            if (i == 0) {  // Only print once per cycle
                for (int j = 0; j < NUM_FADERS; j++) {
                    Serial.printf("Fader %d max ADC seen: %d/4095 (%.1f%%)\n",
                                 j, maxRawValues[j],
                                 (maxRawValues[j] * 100.0f) / 4095.0f);
                }
                lastRangeCheck = millis();
            }
        }

        int value = rawValue >> 5;  // Divide por 32 (4096/128 ≈ 32)

        // Tolerancia para evitar ruido
        if (abs(value - oldValues[i]) <= 2) {
            continue;  // No hay cambio significativo
        }

        pickupStates[i].physicalValue = value;

        // === PICKUP MODE LOGIC ===
        if (!pickupStates[i].hasPickedUp) {
            int diff = abs(value - pickupStates[i].targetValue);

            if (diff <= FADER_PICKUP_THRESHOLD) {
                // ✅ PICKUP ACHIEVED!
                pickupStates[i].hasPickedUp = true;

                if (onPickupStateChange) {
                    onPickupStateChange(i, false);  // needsPickup = false
                }

                Serial.printf("PICKUP: Fader %d (track %d) diff=%d | phys=%d target=%d\n",
                             i, pickupStates[i].assignedTrackIndex, diff,
                             value, pickupStates[i].targetValue);
            } else {
                // ❌ AÚN NO - Solo actualizar oldValues, NO enviar MIDI
                oldValues[i] = value;

                // Opcional: log para debug (RATE LIMITED para evitar buffer overflow)
                if (diff > 10 && (millis() - lastPickupLog[i]) > 500) {
                    Serial.printf("Fader %d waiting pickup: phys=%d target=%d (diff=%d)\n",
                                 i, value, pickupStates[i].targetValue, diff);
                    lastPickupLog[i] = millis();
                }

                continue;  // CRITICAL: Skip MIDI send
            }
        }

        // Ya enganchó o está en tracking normal → Enviar MIDI
        int trackIndex = pickupStates[i].assignedTrackIndex;

        // ⚠️ IMPORTANTE: Los FADERS siempre controlan VOLUMEN
        // Solo los ENCODERS cambian su función según paramMode
        sendVolumeCommand(trackIndex, value);

        oldValues[i] = value;
    }
}

void Faders::sendVolumeCommand(int trackIndex, int volume) {
    // Callback si está configurado
    if (onVolumeChange) {
        onVolumeChange(trackIndex, volume);
        return;
    }

    // Convertir 7-bit (0-127) a 14-bit (0-16383) para mayor resolución
    int value14bit = volume * 129;  // 127 * 129 = 16383

    // Dividir en MSB (bits 7-13) y LSB (bits 0-6)
    uint8_t valueMsb = (value14bit >> 7) & 0x7F;
    uint8_t valueLsb = value14bit & 0x7F;

    // Enviar vía LiveController (no usar MIDI.sendSysEx directo)
    uint8_t payload[] = {(uint8_t)trackIndex, valueMsb, valueLsb};
    liveController.sendSysExToAbleton(CMD_MIXER_VOLUME, payload, 3);

    Serial.printf("Fader %d → Track %d: vol=%d (14bit=%d, MSB=0x%02X LSB=0x%02X)\n",
                 trackIndex % NUM_FADERS, trackIndex, volume, value14bit, valueMsb, valueLsb);
}

void Faders::setParamMode(FaderParamMode mode) {
    if (paramMode == mode) return;

    Serial.printf("Fader param mode change: %d → %d\n",
                 static_cast<int>(paramMode), static_cast<int>(mode));

    paramMode = mode;

    // RESET pickup para todos los faders (cambió qué parámetro controlan)
    for (int i = 0; i < NUM_FADERS; i++) {
        pickupStates[i].hasPickedUp = false;

        if (onPickupStateChange) {
            onPickupStateChange(i, true);  // needsPickup = true
        }
    }
}

void Faders::onTrackParamUpdate(int trackIndex, uint8_t paramType, int value) {
    // Actualizar target value para pickup check
    for (int i = 0; i < NUM_FADERS; i++) {
        if (pickupStates[i].assignedTrackIndex == trackIndex) {
            pickupStates[i].targetValue = value;

            // Auto-pickup si el fader físico ya está cerca
            if (!pickupStates[i].hasPickedUp) {
                int diff = abs(pickupStates[i].physicalValue - value);
                if (diff <= FADER_PICKUP_THRESHOLD) {
                    pickupStates[i].hasPickedUp = true;

                    if (onPickupStateChange) {
                        onPickupStateChange(i, false);
                    }

                    Serial.printf("AUTO-PICKUP: Fader %d param %d (track %d) diff=%d\n",
                                 i, paramType, trackIndex, diff);
                }
            }
            break;
        }
    }
}

void Faders::sendParamCommand(int trackIndex, uint8_t paramType, int value) {
    // Convertir 7-bit a 14-bit
    int value14bit = value * 129;
    uint8_t valueMsb = (value14bit >> 7) & 0x7F;
    uint8_t valueLsb = value14bit & 0x7F;

    // Determinar comando según paramType
    uint8_t command = CMD_MIXER_PAN;  // Default
    uint8_t sendIndex = 0;

    switch (paramType) {
        case 0: // PAN
            command = CMD_MIXER_PAN;
            break;
        case 1: // SEND A
        case 2: // SEND B
        case 3: // SEND C
        case 4: // SEND D (CUE)
            command = CMD_MIXER_SEND;
            sendIndex = paramType - 1;  // 0=A, 1=B, 2=C, 3=D
            break;
        default:
            Serial.printf("Unknown param type: %d\n", paramType);
            return;
    }

    // Payload: trackIndex, sendIndex (si aplica), MSB, LSB
    if (command == CMD_MIXER_SEND) {
        uint8_t payload[] = {(uint8_t)trackIndex, sendIndex, valueMsb, valueLsb};
        liveController.sendSysExToAbleton(command, payload, 4);

        Serial.printf("Fader → Track %d Send %c: %d (14bit=%d)\n",
                     trackIndex, 'A' + sendIndex, value, value14bit);
    } else {
        uint8_t payload[] = {(uint8_t)trackIndex, valueMsb, valueLsb};
        liveController.sendSysExToAbleton(command, payload, 3);

        Serial.printf("Fader → Track %d PAN: %d (14bit=%d)\n",
                     trackIndex, value, value14bit);
    }
}
