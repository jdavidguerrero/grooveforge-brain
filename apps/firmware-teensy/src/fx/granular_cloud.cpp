// apps/firmware-teensy/src/fx/granular_cloud.cpp
// Sprint 2.9 — FX Granular Cloud
// Theory:  apps/docs/sprints/14-granular-cloud.md (pendiente)
// Refs:    apps/docs/05-fx-architecture.md §1.2
//          Roads, "Microsound" (MIT Press, 2001)
//          Truax, "Composing with Time-Shifted and Transposed Sound" (1988)

#include "granular_cloud.h"

// Buffer granular estático — definido aquí para que DMAMEM se aplique correctamente.
// DMAMEM coloca la variable en la sección de memoria accesible por DMA del Teensy 4.1
// (EXTMEM/DMAMEM ≈ 512KB, separada de los 1MB de RAM principal).
// AudioEffectGranular requiere que el buffer sea accesible por DMA para el I2S transfer.
DMAMEM int16_t GranularCloud::_granularBuffer[GranularCloud::GRANULAR_MEMORY_SIZE];

// ── Constructor ──────────────────────────────────────────────────────────────────
GranularCloud::GranularCloud()
    : _cGranWet(_granular,    0, _dryWetMix, 1)   // granular → wet channel
{
    _cIn  = nullptr;
    _cDry = nullptr;
}

// ── Destructor ───────────────────────────────────────────────────────────────────
GranularCloud::~GranularCloud() {
    delete _cIn;
    delete _cDry;
}

// ── begin() ──────────────────────────────────────────────────────────────────────
// AudioMemory y codec son responsabilidad del sketch.
void GranularCloud::begin(AudioStream& inputStream) {
    // Inicializar el granulizador con el buffer DMA y el tamaño en muestras.
    // AudioEffectGranular.begin(buffer, numSamples) — numSamples es int, no bytes.
    // 12800 muestras × (1/48000 s/muestra) = 0.267s de capacidad de buffer.
    _granular.begin(_granularBuffer, GRANULAR_MEMORY_SIZE);

    // Modo inicial: pitch shift passthrough con grain size de 100ms.
    // AudioEffectGranular API: begin() inicializa el buffer pero no arranca la granulación.
    // beginPitchShift(samples) activa el modo passthrough con pitch control via setSpeed().
    // beginFreeze(samples) alterna al modo freeze (captura y loop de granos).
    // No existe startPlayback() — la activación es siempre via beginPitchShift o beginFreeze.
    float grainSamples = _grainSize * (AUDIO_SAMPLE_RATE_EXACT / 1000.0f);
    _granular.beginPitchShift(grainSamples);
    _granular.setSpeed(_speed);

    _dryWetMix.gain(0, 1.0f);     // dry siempre presente
    _dryWetMix.gain(1, _mix);     // wet = granular output
    _dryWetMix.gain(2, 0.0f);
    _dryWetMix.gain(3, 0.0f);

    _cIn  = new AudioConnection(inputStream, 0, _granular,   0);
    _cDry = new AudioConnection(inputStream, 0, _dryWetMix,  0);
}

// ── setGrainSize() ───────────────────────────────────────────────────────────────
void GranularCloud::setGrainSize(float ms) {
    if (ms < 5.0f)   ms = 5.0f;
    if (ms > 250.0f) ms = 250.0f;
    _grainSize = ms;

    // AudioEffectGranular usa tamaño en muestras.
    // Conversión: muestras = ms × AUDIO_SAMPLE_RATE_EXACT / 1000.0f
    // Si está en freeze mode, re-aplicar beginFreeze() con el nuevo tamaño.
    if (_freeze) {
        _granular.beginFreeze(_grainSize * AUDIO_SAMPLE_RATE_EXACT / 1000.0f);
    }
    // En passthrough el grain size se aplica en el próximo startPlayback() implícito.
    // AudioEffectGranular no expone setGrainSize() separado del begin/freeze call —
    // el tamaño queda efectivo en el próximo cambio de modo.
}

// ── setSpeed() ───────────────────────────────────────────────────────────────────
void GranularCloud::setSpeed(float ratio) {
    if (ratio < 0.25f) ratio = 0.25f;
    if (ratio > 4.0f)  ratio = 4.0f;
    _speed = ratio;

    // Solo aplica en passthrough — en freeze el pitch está determinado por la
    // relación entre el grain size capturado y la velocidad de reproducción.
    if (!_freeze) {
        _granular.setSpeed(_speed);
    }
}

// ── setFreeze() ──────────────────────────────────────────────────────────────────
void GranularCloud::setFreeze(bool f) {
    _freeze = f;
    if (_freeze) {
        // beginFreeze() captura el contenido actual del buffer y lo congela.
        // El argumento es el grain size en muestras.
        _granular.beginFreeze(_grainSize * AUDIO_SAMPLE_RATE_EXACT / 1000.0f);
    } else {
        // Volver a passthrough — beginPitchShift() reactiva la granulización en tiempo real.
        float grainSamples = _grainSize * (AUDIO_SAMPLE_RATE_EXACT / 1000.0f);
        _granular.beginPitchShift(grainSamples);
        _granular.setSpeed(_speed);
    }
}

// ── setMix() ─────────────────────────────────────────────────────────────────────
void GranularCloud::setMix(float wet) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    _mix = wet;
    _dryWetMix.gain(0, _bypass ? 1.0f : 1.0f - _mix);
    _dryWetMix.gain(1, _bypass ? 0.0f : _mix);
}

// ── setBypass() ──────────────────────────────────────────────────────────────────
void GranularCloud::setBypass(bool bypass) {
    _bypass = bypass;
    _dryWetMix.gain(0, _bypass ? 1.0f : 1.0f - _mix);
    _dryWetMix.gain(1, _bypass ? 0.0f : _mix);
}

// ── update() ─────────────────────────────────────────────────────────────────────
// No-op en v1.0. En v2.0 integrará análisis espectral via AudioAnalyzeRMS para
// mapear amplitude del input a la densidad de granos — la "inteligencia" del Cloud.
// Ref: apps/docs/04-ai-architecture.md §"FX AI layer"
void GranularCloud::update() {
    // v2.0: amplitude → density mapping via TinyML
}
