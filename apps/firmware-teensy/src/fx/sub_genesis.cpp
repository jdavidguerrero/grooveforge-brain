// apps/firmware-teensy/src/fx/sub_genesis.cpp
// Sprint 2.6 — FX Sub Genesis
// Theory:  apps/docs/sprints/11-sub-genesis.md
// Refs:    apps/docs/05-fx-architecture.md §1.12
//          Zölzer, "DAFX" (2011), §5.4 (Harmonic Exciter)
//          Boss OC-2 Service Manual (Roland Corporation, 1982)
//          Fastl & Zwicker, "Psychoacoustics" (3rd ed., Springer, 2007), Cap. 8

#include "sub_genesis.h"
#include <math.h>

// ── Constructor ──────────────────────────────────────────────────────────────────
// Conexiones estáticas que no dependen de inputStream — tiempo de vida = objeto.
// _cIn se crea en begin() porque inputStream es desconocido en compilación.
SubGenesis::SubGenesis()
    : _cSubOscSat(_subOsc,     0, _subSat,      0)   // subOsc out  → subSat in
    , _cSubSatLP (_subSat,     0, _subLP,        0)   // subSat out → subLP in
    , _cSubLPMix (_subLP,      0, _subMix,       0)   // subLP LP(out0) → subMix ch0
    , _cSubMixWet(_subMix,     0, _dryWetMix,    1)   // subMix out → dryWetMix ch1 (wet)
{}

// ── Destructor ───────────────────────────────────────────────────────────────────
SubGenesis::~SubGenesis() {
    delete _cIn;
}

// ── begin() ──────────────────────────────────────────────────────────────────────
// AudioMemory y codec son responsabilidad del sketch.
void SubGenesis::begin(AudioStream& inputStream) {
    // Sub oscillator: forma de onda y frecuencia inicial.
    // begin(wave) resetea el oscilador — frecuencia y amplitud deben setearse después.
    _subOsc.begin(_waveform);
    float subFreq = _rootFreq / (float)(1 << _octaves);   // rootFreq / 2^octaves
    _subOsc.frequency(subFreq);
    _subOsc.amplitude(_subLevel);   // nivel controlado aquí; _subMix.gain(0)=1.0 fijo

    // Tabla tanh para el waveshaper — debe generarse antes de que llegue audio.
    // shape() es thread-safe: aplica la tabla en el próximo bloque de audio (no mid-block).
    // Ref: apps/docs/sprints/11-sub-genesis.md §"AudioEffectWaveshaper"
    _buildSubSatTable(_drive);

    // LP post-saturación: Butterworth (Q=0.7 → máxima planura, sin pico de resonancia).
    // Enfoca el sub y elimina armónicos muy altos generados por la saturación.
    // Ref: apps/docs/sprints/11-sub-genesis.md §"AudioFilterStateVariable"
    _subLP.frequency(_cutoff);
    _subLP.resonance(0.7f);

    // _subMix: un solo canal activo (ch0 = LP del sub).
    // El nivel del sub se controla en _subOsc.amplitude(), no aquí — gain(0)=1.0 fijo.
    _subMix.gain(0, 1.0f);
    _subMix.gain(1, 0.0f);
    _subMix.gain(2, 0.0f);
    _subMix.gain(3, 0.0f);

    // dryWetMix: comportamiento ADITIVO — dry siempre al 100%, wet = sub sumado.
    // Diferencia clave vs otros FX: Mix no reemplaza el dry, solo controla cuánto
    // sub se suma sobre el chord. El chord nunca se atenúa.
    // Ref: apps/docs/sprints/11-sub-genesis.md §"Nota sobre el Mix parameter"
    _dryWetMix.gain(0, 1.0f);    // ch0: dry (input stream) — siempre 1.0
    _dryWetMix.gain(1, _mix);    // ch1: sub (wet) — nivel configurable
    _dryWetMix.gain(2, 0.0f);
    _dryWetMix.gain(3, 0.0f);

    // Única conexión dinámica: el dry path va directo al blend sin procesamiento.
    _cIn = new AudioConnection(inputStream, 0, _dryWetMix, 0);
}

// ── setRootFreq() ────────────────────────────────────────────────────────────────
void SubGenesis::setRootFreq(float hz) {
    if (hz < 20.0f)   hz = 20.0f;
    if (hz > 1000.0f) hz = 1000.0f;
    _rootFreq = hz;
    float subFreq = _rootFreq / (float)(1 << _octaves);
    _subOsc.frequency(subFreq);
}

// ── setOctaves() ─────────────────────────────────────────────────────────────────
void SubGenesis::setOctaves(uint8_t oct) {
    if (oct < 1) oct = 1;
    if (oct > 2) oct = 2;
    _octaves = oct;
    // Multiplicar por 0.5 en cada paso evita pow(2, octaves) con floating point.
    // Para octaves ∈ {1, 2}, el shift entero (1 << oct) es exacto y eficiente.
    float subFreq = _rootFreq / (float)(1 << _octaves);
    _subOsc.frequency(subFreq);
}

// ── setSubLevel() ────────────────────────────────────────────────────────────────
void SubGenesis::setSubLevel(float level) {
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    _subLevel = level;
    if (!_bypass) {
        _subOsc.amplitude(_subLevel);
    }
}

// ── setDrive() ───────────────────────────────────────────────────────────────────
void SubGenesis::setDrive(float drive) {
    if (drive < 1.0f) drive = 1.0f;
    if (drive > 8.0f) drive = 8.0f;
    _drive = drive;
    _buildSubSatTable(_drive);
}

// ── setCutoff() ──────────────────────────────────────────────────────────────────
void SubGenesis::setCutoff(float hz) {
    if (hz < 40.0f)  hz = 40.0f;
    if (hz > 200.0f) hz = 200.0f;
    _cutoff = hz;
    _subLP.frequency(_cutoff);
}

// ── setMix() ─────────────────────────────────────────────────────────────────────
// Dry siempre al 100% (gain ch0 = 1.0 fijo). Mix controla el nivel del sub sumado.
// Comportamiento aditivo: el chord nunca se atenúa al subir el sub.
void SubGenesis::setMix(float wet) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    _mix = wet;
    if (!_bypass) {
        _dryWetMix.gain(1, _mix);
    }
}

// ── setBypass() ──────────────────────────────────────────────────────────────────
void SubGenesis::setBypass(bool bypass) {
    _bypass = bypass;
    if (_bypass) {
        // Silenciar el sub: apagar oscilador + silenciar canal wet.
        // El dry permanece al 100% — señal completamente limpia.
        _subOsc.amplitude(0.0f);
        _dryWetMix.gain(1, 0.0f);
    } else {
        // Restaurar: oscilador al nivel configurado, sub al nivel _mix.
        _subOsc.amplitude(_subLevel);
        _dryWetMix.gain(1, _mix);
    }
}

// ── setWaveform() ────────────────────────────────────────────────────────────────
void SubGenesis::setWaveform(uint16_t wave) {
    _waveform = wave;
    // begin(wave) puede resetear la frecuencia interna del oscilador.
    // Recalcular y reaplicar después del begin() para garantizar la frecuencia correcta.
    _subOsc.begin(_waveform);
    float subFreq = _rootFreq / (float)(1 << _octaves);
    _subOsc.frequency(subFreq);
    _subOsc.amplitude(_bypass ? 0.0f : _subLevel);
}

// ── _buildSubSatTable() ──────────────────────────────────────────────────────────
// Genera tabla tanh normalizada: satTable[i] = tanhf(drive * x) / tanhf(drive)
// donde x ∈ [-1.0, 1.0]. La normalización garantiza que la ganancia máxima de
// salida sea ≈ 1.0 independientemente del drive (sin compresión de nivel).
// Ref: apps/docs/sprints/11-sub-genesis.md §"AudioEffectWaveshaper"
void SubGenesis::_buildSubSatTable(float drive) {
    float norm = tanhf(drive);
    // norm mínimo para evitar división por cero a drive muy bajo (drive≥1.0 → norm≥0.76)
    if (norm < 0.001f) norm = 0.001f;
    for (int i = 0; i < 257; i++) {
        float x = (float)i / 128.0f - 1.0f;   // x ∈ [-1.0, 1.0]
        _subSatTable[i] = tanhf(drive * x) / norm;
    }
    _subSat.shape(_subSatTable, 257);
}
