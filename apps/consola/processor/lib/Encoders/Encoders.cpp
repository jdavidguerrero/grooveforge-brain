#include "Encoders.h"
#include "MidiCommands.h"
#include "LiveController/LiveController.h"
#include <Arduino.h>

// Declare liveController as extern (defined in Hardware.cpp)
extern LiveController liveController;

Encoders::Encoders()
    : activeEncoders(NUM_ENCODERS_ACTIVE)
    , currentView(0)
    , shiftHeld(false)
    , onEncoderChange(nullptr)
{
    for (int i = 0; i < NUM_ENCODERS_MAX; i++) {
        encoders[i] = nullptr;
        oldPositions[i] = -999;
    }
}

void Encoders::begin() {
    int pins[] = ENCODER_PINS_ACTIVE;

    // Inicializar solo los encoders activos
    for (int i = 0; i < activeEncoders; i++) {
        encoders[i] = new Encoder(pins[i * 2], pins[i * 2 + 1]);
        oldPositions[i] = 0;
        Serial.printf("Encoder %d initialized (pins %d, %d)\n",
                     i + 1, pins[i * 2], pins[i * 2 + 1]);
    }

    Serial.printf("Encoders initialized: %d active of %d max\n",
                 activeEncoders, NUM_ENCODERS_MAX);
}

void Encoders::setActiveCount(uint8_t count) {
    if (count > NUM_ENCODERS_MAX) {
        count = NUM_ENCODERS_MAX;
    }

    // TODO: Si se aumenta el count, inicializar los nuevos encoders
    // Por ahora solo permitimos reducir
    if (count < activeEncoders) {
        for (int i = count; i < activeEncoders; i++) {
            if (encoders[i] != nullptr) {
                delete encoders[i];
                encoders[i] = nullptr;
            }
        }
    }

    activeEncoders = count;
    Serial.printf("Active encoders changed to: %d\n", activeEncoders);
}

void Encoders::read() {
    for (int i = 0; i < activeEncoders; i++) {
        if (encoders[i] == nullptr) continue;

        long newPosition = encoders[i]->read();
        if (newPosition != oldPositions[i]) {
            int delta = newPosition - oldPositions[i];

            // Aplicar control fino con Shift (incrementos 10x más pequeños)
            if (shiftHeld && abs(delta) > 1) {
                delta = delta / 10;
                if (delta == 0) delta = (newPosition > oldPositions[i]) ? 1 : -1;
            }

            sendEncoderChange(i, delta);
            oldPositions[i] = newPosition;
        }
    }
}

void Encoders::sendEncoderChange(uint8_t encoderIndex, int8_t delta) {
    // Llamar callback si está configurado
    if (onEncoderChange) {
        onEncoderChange(encoderIndex, delta);
        return;
    }

    // Comportamiento por defecto: enviar como relative encoder change
    // Valores: 1-63 = incrementos positivos, 65-127 = incrementos negativos
    uint8_t value = (delta > 0) ? 1 : 127;

    // Enviar como CMD_DEVICE_PARAMS (default - será mapeado según currentView)
    uint8_t payload[] = {encoderIndex, value};
    liveController.sendSysExToAbleton(CMD_DEVICE_PARAMS, payload, 2);

    Serial.printf("Encoder %d: delta=%d (value=%d)\n", encoderIndex, delta, value);
}
