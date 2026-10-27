#pragma once

#include <Arduino.h>
#include <Audio.h>

/**
 * @file plate_reverb.h
 * @brief PLATE estéreo de la cadena del synth: AudioEffectFreeverbStereo (Teensy Audio Library, oficial) que se puede
 *        desactivar para no gastar CPU cuando no está elegida (theory/fase3-fx.md §2).
 */
class AudioPlateReverb : public AudioEffectFreeverbStereo {
public:
    /// false: la Audio Library deja de llamar a su update() (sin CPU); sus salidas quedan en silencio.
    void set_active(bool on) { active = on; }
};
