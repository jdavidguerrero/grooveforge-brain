// apps/firmware-teensy/src/fx/ghost_echo.cpp
// Sprint 2.11 — FX Ghost Echo
// Theory:  apps/docs/sprints/16-ghost-echo.md (pendiente)
// Refs:    apps/docs/05-fx-architecture.md §1.3
//          Zölzer, "DAFX: Digital Audio Effects" (2nd ed., Wiley, 2011), §2.2
//          Schimmel et al., "Magnetic Tape Echo Model" (AES 2010)

#include "ghost_echo.h"

// ── Constructor ──────────────────────────────────────────────────────────────────
// El feedback loop Teensy Audio Library-válido:
//   _feedbackMix → _delay → _feedbackLP → _feedbackMix (cierre de loop)
//   El AudioEffectDelay introduce 1 bloque de latencia (128 samples = 2.67ms a 48kHz)
//   que el scheduler usa para detectar y resolver el ciclo de dependencias.
//
// Fan-out desde _delay tap 0: tanto _feedbackLP como _dryWetMix(ch1) se conectan
// al mismo output port 0 del _delay. La Teensy Audio Library resuelve el fan-out
// implícitamente — el scheduler llama a _delay.update() una vez y distribuye el
// buffer a todos los destinos registrados.
GhostEcho::GhostEcho()
    : _cFeedbackDelay(_feedbackMix, 0, _delay,       0)
    , _cDelayLP      (_delay,       0, _feedbackLP,   0)
    , _cLPFeedback   (_feedbackLP,  0, _feedbackMix,  1)   // port 0 = LP output
    , _cDelayWet     (_delay,       0, _dryWetMix,    1)   // fan-out: tap 0 al wet
    , _cOutL         (_dryWetMix,   0, _out,          0)
    , _cOutR         (_dryWetMix,   0, _out,          1)
{
    _cIn  = nullptr;
    _cDry = nullptr;
}

// ── begin() ──────────────────────────────────────────────────────────────────────
void GhostEcho::begin(AudioStream& inputStream, float volume) {
    AudioMemory(30);

    _codec.enable();
    _codec.volume(volume);

    // _feedbackMix: ch0 = señal nueva (gain 1.0), ch1 = feedback filtrado (gain = _feedback)
    // El feedback NO va directamente al delay — va al mixer que suma con la señal nueva.
    // Esto permite que la señal original siempre llegue limpia al delay, mientras que
    // el feedback llega atenuado y filtrado — comportamiento de delay de cinta real.
    _feedbackMix.gain(0, 1.0f);         // señal nueva pasa limpia
    _feedbackMix.gain(1, _feedback);    // feedback filtrado, atenuado
    _feedbackMix.gain(2, 0.0f);
    _feedbackMix.gain(3, 0.0f);

    // Configurar el delay: tap 0 con el tiempo inicial
    _delay.delay(0, _delayMs);

    // LP filter del feedback: Q=0.7 (Butterworth, sin pico resonante) para
    // coloración suave. Q alto añadiría un pico que hace el feedback inestable
    // y sonaría "electrónico" en lugar de "cinta".
    // AudioFilterStateVariable.resonance() acepta Q (no R — cuidado con la API):
    //   Q = 0.5 → sobre-amortiguado (sin oscilación, rolloff suave)
    //   Q = 0.7 → Butterworth (respuesta plana máxima, sin pico)
    //   Q > 1.0 → sub-amortiguado (pico resonante)
    _feedbackLP.frequency(_tapeLP);
    _feedbackLP.resonance(0.7f);    // Butterworth: Q=0.7, sin pico resonante

    // _dryWetMix: dry siempre presente, wet = ecos del delay
    _dryWetMix.gain(0, 1.0f);
    _dryWetMix.gain(1, _mix);
    _dryWetMix.gain(2, 0.0f);
    _dryWetMix.gain(3, 0.0f);

    // inputStream → _feedbackMix(ch0): la señal nueva entra al mixer del feedback loop
    _cIn  = new AudioConnection(inputStream, 0, _feedbackMix, 0);
    // inputStream → _dryWetMix(ch0): dry path directo al output
    _cDry = new AudioConnection(inputStream, 0, _dryWetMix,  0);
}

// ── setDelayMs() ─────────────────────────────────────────────────────────────────
void GhostEcho::setDelayMs(float ms) {
    if (ms < 10.0f)    ms = 10.0f;
    if (ms > 1400.0f)  ms = 1400.0f;
    _delayMs = ms;
    // AudioEffectDelay.delay(tap, ms) — primer argumento es índice de tap (0-based)
    _delay.delay(0, _delayMs);
}

// ── setFeedback() ────────────────────────────────────────────────────────────────
void GhostEcho::setFeedback(float fb) {
    if (fb < 0.0f)  fb = 0.0f;
    if (fb > 0.95f) fb = 0.95f;   // cap a 0.95 — previene runaway acústico
    _feedback = fb;
    // El feedback controla el gain del ch1 del _feedbackMix (donde llega el eco filtrado)
    // No se llama gain() en el _delay — el delay no tiene control de feedback directo.
    _feedbackMix.gain(1, _feedback);
}

// ── setTapeLP() ──────────────────────────────────────────────────────────────────
void GhostEcho::setTapeLP(float hz) {
    if (hz < 500.0f)   hz = 500.0f;
    if (hz > 12000.0f) hz = 12000.0f;
    _tapeLP = hz;
    _feedbackLP.frequency(_tapeLP);
}

// ── setMix() ─────────────────────────────────────────────────────────────────────
void GhostEcho::setMix(float wet) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    _mix = wet;
    if (!_bypass) {
        _dryWetMix.gain(1, _mix);
    }
}

// ── setBypass() ──────────────────────────────────────────────────────────────────
void GhostEcho::setBypass(bool bypass) {
    _bypass = bypass;
    _dryWetMix.gain(1, _bypass ? 0.0f : _mix);
    // Nota: el delay buffer NO se limpia en bypass — los ecos acumulados persisten.
    // Comportamiento intencional: toggle de bypass mid-performance sin silencio abrupto.
}

// ── update() ─────────────────────────────────────────────────────────────────────
// No-op en v1.0. En v2.0: Markov chain orden 2-3 para generar timing coherente de ecos
// con el groove del usuario. Ref: apps/docs/05-fx-architecture.md §1.3 "Ghost amount".
void GhostEcho::update() {
    // v2.0: Markov chain rhythm detection + intelligent echo timing
}
