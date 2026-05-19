#pragma once
// beat_sync.h — Sincronización de FX al BPM detectado por Beat Follower ML.
//
// BeatSync convierte el BPM del Beat Follower a tiempos de delay musicales
// (quarter, dotted eighth, etc.) con suavizado exponencial para evitar
// saltos audibles. Incluye confidence gate: solo actualiza si el Beat
// Follower está seguro.
//
// El suavizado EMA (alpha=0.1) introduce una constante de tiempo de ~10 beats
// para converger al nuevo tempo. A 120 BPM esto son ~5 segundos — tiempo
// suficiente para absorber errores de detección de single-beat, pero
// suficientemente rápido para seguir cambios de tempo reales.
//
// Cross-ref: apps/docs/05-fx-architecture.md §2 — CPU budget FX
//            apps/docs/sprints/28-beat-synced-fx.md

#include "ml/beat_follower.h"

/**
 * @brief Subdivisiones musicales soportadas para delay y sync de LFO.
 *
 * Los factores son multiplicadores del beat period (60000/BPM ms):
 *   HALF          = 2.0  → blanca
 *   QUARTER       = 1.0  → negra
 *   DOTTED_EIGHTH = 0.75 → corchea con punto ("el delay de U2")
 *   EIGHTH        = 0.5  → corchea
 *   SIXTEENTH     = 0.25 → semicorchea
 */
enum class Subdivision : uint8_t {
    HALF          = 0,
    QUARTER       = 1,
    DOTTED_EIGHTH = 2,
    EIGHTH        = 3,
    SIXTEENTH     = 4,
};

/**
 * @brief Conversor BPM → tiempo de subdivisión musical en ms.
 *
 * Función pura sin estado. Útil para casos donde no se necesita
 * el suavizado EMA de BeatSync (e.g., síncronización de display).
 *
 * @param bpm BPM actual (debe ser > 0).
 * @param sub Subdivisión musical deseada.
 * @return Tiempo en ms, clampeado a MAX_DELAY_MS (1490ms).
 *         Retorna 0.0f si bpm < 1.0f.
 */
float subdivision_ms(float bpm, Subdivision sub);

/**
 * @brief Sincronizador de FX al BPM detectado por BeatFollower.
 *
 * Responsabilidades:
 *   1. Confidence gate — descarta resultados del BeatFollower con baja confianza.
 *   2. Suavizado EMA — evita saltos audibles en cambios de tempo (alpha=0.1).
 *   3. Conversión BPM → ms para cualquier subdivisión musical.
 *
 * Uso típico:
 * @code
 *   BeatSync sync;
 *   // En loop(), cada vez que BeatFollower infiere:
 *   sync.update(br, 0.7f);
 *   // Para obtener el periodo de quarter note:
 *   float ms = sync.delay_ms(Subdivision::QUARTER);
 * @endcode
 */
class BeatSync {
public:
    BeatSync();

    /**
     * @brief Actualiza el BPM suavizado con un nuevo resultado del Beat Follower.
     *
     * Solo actualiza si br.valid && br.confidence >= min_confidence.
     * En la primera lectura válida no aplica EMA — adopta el BPM directamente
     * para evitar el ramp-up desde 120 BPM cuando el tempo real es muy diferente.
     *
     * @param br             Resultado de BeatFollower::infer().
     * @param min_confidence Umbral de confianza [0.0, 1.0]. Default 0.7.
     */
    void update(const BeatResult& br, float min_confidence = 0.7f);

    /**
     * @brief Tiempo en ms para la subdivisión pedida.
     *
     * @param sub Subdivisión musical. Default: DOTTED_EIGHTH.
     * @return Tiempo en ms, o 0.0f si !has_bpm().
     */
    float delay_ms(Subdivision sub = Subdivision::DOTTED_EIGHTH) const;

    /** @brief True si hay BPM activo (al menos una lectura válida recibida). */
    bool has_bpm() const { return _has_bpm; }

    /** @brief BPM suavizado actual (0.0 si sin datos). */
    float bpm() const { return _bpm_smooth; }

private:
    float _bpm_smooth;
    bool  _has_bpm;

    // EMA alpha=0.1: tau ≈ 10 beats (~5s a 120 BPM).
    // Elegido para absorber detecciones espurias sin reaccionar lento
    // a cambios de tempo deliberados (ej: DJ drops de 120→140 BPM).
    static constexpr float EMA_ALPHA = 0.1f;

    // Límite de AudioEffectDelay de la Teensy Audio Library: 1500ms.
    // Usamos 1490ms para margen contra off-by-one en el cálculo de buffer.
    static constexpr float MAX_DELAY_MS = 1490.0f;
};
