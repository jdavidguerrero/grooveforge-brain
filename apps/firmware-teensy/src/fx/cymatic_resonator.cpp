// apps/firmware-teensy/src/fx/cymatic_resonator.cpp
// Sprint 2.8 — FX Cymatic Resonator
// Theory:  apps/docs/sprints/13-cymatic-resonator.md (pendiente)
// Refs:    apps/docs/05-fx-architecture.md §1.1
//          Chladni, "Entdeckungen über die Theorie des Klanges" (1787) — patrones modales
//          Zölzer, "DAFX: Digital Audio Effects" (2nd ed., Wiley, 2011), §3.4

#include "cymatic_resonator.h"

// ── Constructor ──────────────────────────────────────────────────────────────────
// Las conexiones estáticas se cablean en la init list: modos → modeMix, modeMix →
// dryWetMix(wet), dryWetMix → out L+R. Las conexiones desde inputStream se crean
// en begin() porque inputStream no se conoce hasta setup().
CymaticResonator::CymaticResonator()
    : _cMode0Mix(_mode[0], 0, _modeMix,    0)
    , _cMode1Mix(_mode[1], 0, _modeMix,    1)
    , _cMode2Mix(_mode[2], 0, _modeMix,    2)
    , _cMode3Mix(_mode[3], 0, _modeMix,    3)
    , _cMixWet  (_modeMix, 0, _dryWetMix,  1)   // modeMix → wet channel
    , _cOutL    (_dryWetMix, 0, _out,      0)   // L
    , _cOutR    (_dryWetMix, 0, _out,      1)   // R
{
    for (int i = 0; i < 4; i++) _cMode[i] = nullptr;
    _cDry = nullptr;
}

// ── begin() ──────────────────────────────────────────────────────────────────────
void CymaticResonator::begin(AudioStream& inputStream, float volume) {
    // 4 filtros BP + 2 mixers + out ≈ 7 objetos. AudioMemory(30) da margen amplio.
    AudioMemory(30);

    _codec.enable();
    _codec.volume(volume);

    // _modeMix: ganancias iniciales uniformes — _updateMixerGains() las ajusta
    // luego según _density. El 0.5f compensa la suma de hasta 4 modos en paralelo.
    // Sin normalización, 4 modos activos pueden sumar hasta 4× la amplitud individual.
    _modeMix.gain(0, 0.5f);
    _modeMix.gain(1, 0.5f);
    _modeMix.gain(2, 0.5f);
    _modeMix.gain(3, 0.5f);

    // _dryWetMix: dry siempre presente, wet controlado por _mix
    _dryWetMix.gain(0, 1.0f);     // ch0 = dry
    _dryWetMix.gain(1, _mix);     // ch1 = wet (resonancias)
    _dryWetMix.gain(2, 0.0f);
    _dryWetMix.gain(3, 0.0f);

    // Configurar filtros con tune=1.0, lfoMod=1.0 (LFO arranca en fase 0)
    _applyFilters(1.0f);
    _updateMixerGains();

    // Fan-out dinámico desde inputStream: 4 ramas hacia los modos + 1 al dry path.
    // La Teensy Audio Library soporta fan-out: el scheduler ejecuta inputStream una
    // vez y el buffer se distribuye a todos los destinos sin copias adicionales.
    for (int i = 0; i < 4; i++) {
        _cMode[i] = new AudioConnection(inputStream, 0, _mode[i], 0);
    }
    _cDry = new AudioConnection(inputStream, 0, _dryWetMix, 0);

    _lastUpdateMs = millis();
}

// ── setTune() ────────────────────────────────────────────────────────────────────
void CymaticResonator::setTune(float tune) {
    if (tune < 0.5f) tune = 0.5f;
    if (tune > 2.0f) tune = 2.0f;
    _tune = tune;
    _applyFilters(1.0f + 0.05f * sinf(_lfoPhase));
}

// ── setDensity() ─────────────────────────────────────────────────────────────────
void CymaticResonator::setDensity(uint8_t density) {
    if (density < 1) density = 1;
    if (density > 4) density = 4;
    _density = density;
    _updateMixerGains();
}

// ── setResonance() ───────────────────────────────────────────────────────────────
void CymaticResonator::setResonance(float q) {
    if (q < 10.0f) q = 10.0f;
    if (q > 80.0f) q = 80.0f;
    _resonance = q;
    _applyFilters(1.0f + 0.05f * sinf(_lfoPhase));
}

// ── setLfoRate() ─────────────────────────────────────────────────────────────────
void CymaticResonator::setLfoRate(float hz) {
    if (hz < 0.1f) hz = 0.1f;
    if (hz > 3.0f) hz = 3.0f;
    _lfoRate = hz;
}

// ── setMix() ─────────────────────────────────────────────────────────────────────
void CymaticResonator::setMix(float wet) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    _mix = wet;
    if (!_bypass) {
        _dryWetMix.gain(1, _mix);
    }
}

// ── setBypass() ──────────────────────────────────────────────────────────────────
void CymaticResonator::setBypass(bool bypass) {
    _bypass = bypass;
    _dryWetMix.gain(1, _bypass ? 0.0f : _mix);
}

// ── update() ─────────────────────────────────────────────────────────────────────
// Avanza el LFO y aplica la modulación de tune a los filtros activos.
// Llamar desde loop() — se ejecuta en el thread de control, no en el audio ISR.
void CymaticResonator::update() {
    if (_bypass) return;

    uint32_t now = millis();
    float dt_ms  = (float)(now - _lastUpdateMs);
    _lastUpdateMs = now;

    // Avanzar fase del LFO: dPhase = 2π × f × dt_s
    // Usamos 2.0f * 3.14159265f en lugar de TWO_PI — la macro de Teensyduino puede
    // redefinirse y romper el build en algunos contextos de compilación cruzada.
    _lfoPhase += (2.0f * 3.14159265f) * _lfoRate * (dt_ms / 1000.0f);

    // Wrap de fase: evitar drift numérico en sesiones largas (32-bit float pierde
    // precisión acumulando 2π repetidamente — wrap mantiene el rango cerca de 0)
    if (_lfoPhase >= (2.0f * 3.14159265f)) {
        _lfoPhase -= (2.0f * 3.14159265f);
    }

    // Modulación ±5% en tune — crea el movimiento "cristalino" característico
    float lfoMod = 1.0f + 0.05f * sinf(_lfoPhase);
    _applyFilters(lfoMod);
}

// ── _applyFilters() ──────────────────────────────────────────────────────────────
// Actualiza los coeficientes biquad de cada modo activo con la frecuencia central
// modulada por el LFO. Solo actualiza los modos activos (_density) para minimizar
// el trabajo en loop() — los modos silenciados igualmente consumen CPU en el audio
// thread, pero evitamos la llamada a setBandpass() que regenera coeficientes.
void CymaticResonator::_applyFilters(float lfoMod) {
    for (int i = 0; i < (int)_density; i++) {
        float freq = RATIOS[i] * BASE_FREQ * _tune * lfoMod;
        // Clamp: AudioFilterBiquad no valida el rango de frecuencia internamente.
        // El límite superior es AUDIO_SAMPLE_RATE_EXACT/2 (Nyquist) ≈ 24kHz.
        if (freq < 20.0f)    freq = 20.0f;
        if (freq > 20000.0f) freq = 20000.0f;
        _mode[i].setBandpass(0, freq, _resonance);
    }
}

// ── _updateMixerGains() ──────────────────────────────────────────────────────────
// Activa modos 0..(density-1) con ganancia 0.5 (normalizada para evitar clipping)
// y silencia los modos density..3. El 0.5 asegura que la suma máxima de 4 modos
// no supere la unidad en la entrada del _dryWetMix.
void CymaticResonator::_updateMixerGains() {
    for (int i = 0; i < 4; i++) {
        // 0.5f fijo en lugar de 1/density: evita cambios bruscos de volumen al
        // cambiar density — el oyente percibe más "contenido", no más "volumen".
        _modeMix.gain(i, (i < (int)_density) ? 0.5f : 0.0f);
    }
}
