#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>

/**
 * @brief FX Ghost Echo — Sprint 2.11.
 *
 * Delay clásico con carácter de cinta magnética — un filtro LP en el path de
 * feedback oscurece los ecos progresivamente, imitando la pérdida de altas
 * frecuencias característica de las cintas magnetofónicas desgastadas.
 *
 * Cada eco repetido pasa por el LP filter antes de realimentarse al delay:
 *   eco 1 → pierde HF una vez (cinta levemente desgastada)
 *   eco 2 → pierde HF dos veces (cinta más oscura)
 *   eco n → decay exponencial de HF + amplitud → ecos que "desaparecen en la oscuridad"
 *
 * El parámetro _tapeLP controla el caracter del desgaste:
 *   12000 Hz → cinta nueva, casi sin coloración
 *    5000 Hz → cinta de estudio vintage, calida
 *    1000 Hz → cinta muy desgastada, los ecos son murmullo
 *
 * Arquitectura del feedback loop:
 *   El feedback loop es válido en Teensy Audio Library cuando hay un AudioEffectDelay
 *   en el path — introduce latencia de 1 bloque (128 samples ≈ 2.67ms) que el
 *   scheduler usa para romper la dependencia circular y ejecutarlo sin deadlock.
 *   Ref: Teensy Audio Library Design Tool — "Feedback loops" nota en AudioEffectDelay.
 *
 * Signal flow:
 *   inputStream ──→ _feedbackMix(ch0)
 *                       │
 *                       ↓
 *                   _delay ──(tap 0)──┬──→ _feedbackLP ──→ _feedbackMix(ch1) [feedback]
 *                                     └──→ _dryWetMix(ch1)               [wet out]
 *   inputStream ──────────────────────────────────────────→ _dryWetMix(ch0) [dry]
 *                            _dryWetMix ──→ _out L+R
 *
 * CPU estimado: ~6% (apps/docs/05-fx-architecture.md §1.3)
 * AudioMemory: begin() llama AudioMemory(30).
 *
 * Theory: apps/docs/sprints/16-ghost-echo.md (pendiente)
 * Refs: apps/docs/05-fx-architecture.md §1.3
 *       Zölzer, "DAFX: Digital Audio Effects" (2nd ed., Wiley, 2011), §2.2 (Delay)
 *       Schimmel et al., "Magnetic Tape Echo Model" (AES 2010)
 *       Dub delay technique: King Tubby, Lee "Scratch" Perry (1970s Jamaica)
 */
class GhostEcho {
public:

    GhostEcho();

    /**
     * @brief Conecta el stream de entrada e inicializa el codec SGTL5000.
     *
     * Crea 2 AudioConnections dinámicas: inputStream→_feedbackMix(ch0) e
     * inputStream→_dryWetMix(ch0). Llama AudioMemory(30) internamente.
     *
     * @param inputStream Stream de audio de entrada.
     * @param volume      Volumen del codec SGTL5000, 0.0–1.0 (default 0.5).
     */
    void begin(AudioStream& inputStream, float volume = 0.5f);

    /**
     * @brief Tiempo de delay en milisegundos.
     *
     * 375ms ≈ 1 beat a 160 BPM (3/8 note) — default orientado a sets de techno/house.
     * Rango máximo de AudioEffectDelay: 1.4 segundos (buffer de ~67200 muestras a 48kHz).
     * Ref: apps/docs/05-fx-architecture.md §1.3 — "64KB delay buffer"
     *
     * @param ms Tiempo de delay [10.0, 1400.0] ms.
     */
    void setDelayMs(float ms);

    /**
     * @brief Cantidad de señal realimentada al delay.
     *
     * Controla el gain del canal 1 de _feedbackMix (donde llega el feedback filtrado).
     * Clampeado a 0.95 máximo para evitar runaway acústico (feedback > 1.0 = oscilación).
     * A 0.95 con 375ms delay: los ecos son audibles por ~4-5 segundos antes de extinguirse.
     *
     * @param fb Feedback [0.0, 0.95].
     */
    void setFeedback(float fb);

    /**
     * @brief Frecuencia de corte del LP filter en el path de feedback.
     *
     * El LP filter colorea cada eco repetido oscureciéndolo progresivamente —
     * carácter de cinta magnética desgastada.
     * AudioFilterStateVariable port 0 = lowpass output.
     *
     * @param hz Frecuencia de corte [500.0, 12000.0] Hz.
     */
    void setTapeLP(float hz);

    /**
     * @brief Balance dry/wet.
     *
     * @param wet Nivel wet [0.0, 1.0].
     */
    void setMix(float wet);

    /**
     * @brief Activa o desactiva el bypass del FX.
     *
     * @param bypass true = bypass activo.
     */
    void setBypass(bool bypass);

    /**
     * @brief No-op en v1.0. Reservado para Markov chain en v2.0.
     *
     * En v2.0 (Fase 4): Ghost Echo aprenderá el ritmo del usuario via Markov chain
     * y generará ecos musicalmente coherentes con el groove en lugar de repeticiones
     * literales. Ref: apps/docs/04-ai-architecture.md, apps/docs/05-fx-architecture.md §1.3.
     */
    void update();

private:
    // ── Audio objects (Teensy Audio Library oficial) ──────────────────────────────
    AudioMixer4              _feedbackMix;    ///< ch0=nuevo audio, ch1=feedback filtrado
    AudioEffectDelay         _delay;          ///< delay multi-tap, usamos solo tap 0
    AudioFilterStateVariable _feedbackLP;     ///< LP en path de feedback (tape color)
    AudioMixer4              _dryWetMix;
    AudioOutputI2S           _out;
    AudioControlSGTL5000     _codec;

    // ── Conexiones estáticas (init list del constructor) ──────────────────────────
    // _feedbackMix(0) → _delay(0)          — el mix entra al delay
    // _delay(0) → _feedbackLP(0)           — tap 0 del delay al LP filter
    // _feedbackLP(port 0 = LP) → _feedbackMix(ch1)  — feedback filtrado cierra el loop
    // _delay(0) → _dryWetMix(ch1)          — fan-out: misma tap al wet out
    // _dryWetMix(0) → _out L+R
    //
    // Fan-out desde _delay tap 0: AudioConnection múltiple desde el mismo output port
    // es válido en Teensy Audio Library — el scheduler lo resuelve sin copias extras.
    AudioConnection _cFeedbackDelay;   ///< _feedbackMix → _delay
    AudioConnection _cDelayLP;         ///< _delay(tap0) → _feedbackLP
    AudioConnection _cLPFeedback;      ///< _feedbackLP(port0=LP) → _feedbackMix(ch1)
    AudioConnection _cDelayWet;        ///< _delay(tap0) → _dryWetMix(ch1) — fan-out
    AudioConnection _cOutL;
    AudioConnection _cOutR;

    // ── Conexiones dinámicas ──────────────────────────────────────────────────────
    AudioConnection* _cIn;    ///< inputStream → _feedbackMix(ch0)
    AudioConnection* _cDry;   ///< inputStream → _dryWetMix(ch0)

    // ── Estado de parámetros ──────────────────────────────────────────────────────
    float _delayMs  = 375.0f;    ///< tiempo de delay [10.0, 1400.0] ms
    float _feedback = 0.45f;     ///< gain de feedback [0.0, 0.95]
    float _tapeLP   = 5000.0f;   ///< cutoff del LP de cinta [500.0, 12000.0] Hz
    float _mix      = 0.5f;      ///< wet level [0.0, 1.0]
    bool  _bypass   = false;
};
