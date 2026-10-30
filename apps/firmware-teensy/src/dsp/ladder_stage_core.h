#pragma once

#include <cmath>

#include "zdf_ladder.h"

namespace dsp {

/**
 * @brief Cálculo del ladder con drive y protección, independiente de AudioStream.
 *
 * El adaptador conserva la cuantización del grafo de Teensy; el core opera en float.
 */
class LadderStageCore {
public:
    /** @brief Drive y compensación calculados fuera de la sección crítica. */
    struct DriveSetting { float drive; float compensation; };

    /** @brief Inicializa el core con la frecuencia de muestreo en Hz. */
    explicit LadderStageCore(float sample_rate) : sample_rate_(sample_rate) {}

    /** @brief Ajusta el cutoff en Hz usando la frecuencia de muestreo configurada. */
    void set_frequency(float hz) { ladder_.set_cutoff(hz, sample_rate_); }

    /** @brief Ajusta la resonancia en la convención de Teensy Audio. */
    void set_resonance(float q) {
        const float r = (q - 0.707f) / (5.0f - 0.707f);
        ladder_.set_k((r < 0.0f ? 0.0f : r > 1.0f ? 1.0f : r) * 4.4f);
    }

    /** @brief Selecciona el carácter del circuito del filtro. */
    void set_character(LadderCharacter character) { ladder_.set_character(character); }

    /** @brief Calcula drive y compensación de nivel para un valor solicitado. */
    static DriveSetting prepare_drive(float drive) {
        const float limited = drive < 0.5f ? 0.5f : drive > 4.0f ? 4.0f : drive;
        return {limited, 1.0f / std::sqrt(limited)};
    }

    /** @brief Aplica una configuración de drive previamente calculada. */
    void set_drive(DriveSetting setting) {
        drive_ = setting.drive;
        inverse_drive_compensation_ = setting.compensation;
    }

    /** @brief Borra el estado del filtro sin modificar sus parámetros. */
    void reset() { ladder_.reset(); }

    /** @brief Procesa una muestra normalizada y limita la salida a ±1. */
    float process_sample(float in) {
        const float y = ladder_.tick(in * drive_);
        if (!std::isfinite(y)) { reset(); return 0.0f; }
        const float out = y * inverse_drive_compensation_;
        return out > 1.0f ? 1.0f : out < -1.0f ? -1.0f : out;
    }

private:
    ZdfLadder ladder_;
    float sample_rate_;
    float drive_ = 1.0f;
    float inverse_drive_compensation_ = 1.0f;
};

}  // namespace dsp
