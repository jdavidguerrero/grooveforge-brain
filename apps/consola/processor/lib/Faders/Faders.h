#pragma once

#include "shared/Config.h"
#include <stdint.h>

// Modos de parámetros para faders en Mix View
enum class FaderParamMode : uint8_t {
    VOLUME_PAN = 0,      // Faders 1-2: Volume, Faders 3-4: Pan
    SENDS_AB = 1,        // Faders 1-2: Send A, Faders 3-4: Send B
    SENDS_CD = 2,        // Faders 1-2: Send C, Faders 3-4: Cue
    MASTER_RETURNS = 3   // Modo especial: Returns + Master fijos
};

// Estado de pickup para cada fader
struct FaderPickupState {
    int physicalValue;      // Posición física actual del fader (0-127)
    int targetValue;        // Valor real del parámetro en Ableton (0-127)
    bool hasPickedUp;       // ¿Ya "enganchó" el valor target?
    int assignedTrackIndex; // Track absoluto asignado (0-N)
};

class Faders {
public:
    Faders();

    void begin();
    void read();

    // Track banking
    void setTrackBank(int bankOffset);  // 0, 4, 8, 12...
    int getTrackBank() const { return currentBankOffset; }

    // Parameter mode (para Mix View)
    void setParamMode(FaderParamMode mode);
    FaderParamMode getParamMode() const { return paramMode; }

    // Actualización de valores desde Ableton (para pickup check)
    void onTrackVolumeUpdate(int trackIndex, int volume);
    void onTrackParamUpdate(int trackIndex, uint8_t paramType, int value);

private:
    int oldValues[NUM_FADERS];
    int currentBankOffset;              // Offset del banco actual (0, 4, 8...)
    FaderParamMode paramMode;            // Modo de parámetros (Volume/Pan, Sends, etc.)
    FaderPickupState pickupStates[NUM_FADERS];

public:
    // Callbacks (moved after private members to match initialization order)
    void (*onVolumeChange)(int trackIndex, int volume);
    void (*onPickupStateChange)(int faderIndex, bool needsPickup);

private:

    bool checkPickup(int faderIndex, int physicalValue, int targetValue);
    void sendVolumeCommand(int trackIndex, int volume);
    void sendParamCommand(int trackIndex, uint8_t paramType, int value);
};
