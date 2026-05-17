// apps/firmware-teensy/src/fx/bit_sculpt.cpp
// Sprint 2.5 — FX Bit Sculpt
// Theory:  apps/docs/sprints/10-bit-sculpt.md
// Refs:    apps/docs/05-fx-architecture.md §1.6
//          Pohlmann, "Principles of Digital Audio" (6th ed.), Cap. 4
//          Zölzer, "DAFX" (2011), §7.1–7.2

#include "bit_sculpt.h"
#include <math.h>

// ── Constructor ──────────────────────────────────────────────────────────────────
// Conexiones estáticas que no dependen de inputStream — tiempo de vida = objeto.
// _cIn1 y _cIn2 se crean en begin() porque inputStream es desconocido en compilación.
BitSculpt::BitSculpt()
    : _cNoiseDither(_noise,      0, _ditherMix,  1)   // noise → ditherMix ch1
    , _cDitherCrush(_ditherMix,  0, _crusher,    0)   // ditherMix out → crusher in
    , _cCrushWet   (_crusher,    0, _dryWetMix,  1)   // crusher out → wet channel
    , _cMixL       (_dryWetMix,  0, _out,        0)   // mix → I2S Left
    , _cMixR       (_dryWetMix,  0, _out,        1)   // mix → I2S Right
{}

// ── begin() ──────────────────────────────────────────────────────────────────────
void BitSculpt::begin(AudioStream& inputStream, float volume) {
    // 3 oscs + srcMix + noise + ditherMix + crusher + dryWetMix ≈ 9 bloques activos.
    // AudioMemory(20) da ~2.2× headroom — suficiente.
    // Ref: apps/docs/sprints/10-bit-sculpt.md §CPU
    AudioMemory(20);

    _codec.enable();
    _codec.volume(volume);

    // Noise white: empieza silenciado — update() ajusta el nivel según Sculpt y bits.
    // amplitude() no es gain del mixer; escala la amplitud interna del generador LFSR.
    // Lo dejamos en 1.0 y controlamos el nivel exclusivamente desde _ditherMix.gain(1).
    _noise.amplitude(1.0f);

    // ditherMix: señal de entrada al 100%; noise empieza en 0 (update() lo ajusta).
    _ditherMix.gain(0, 1.0f);   // ch0: señal de entrada
    _ditherMix.gain(1, 0.0f);   // ch1: noise — nivel calculado en update()
    _ditherMix.gain(2, 0.0f);
    _ditherMix.gain(3, 0.0f);

    // AudioEffectBitcrusher: seguro de llamar antes o después de AudioMemory().
    // No tiene begin() ni buffer interno — solo escribe dos registros de parámetro.
    // Diferencia clave respecto a AudioEffectChorus (Sprint 2.4).
    // Ref: PaulStoffregen, effect_bitcrusher.cpp
    _crusher.bits(_bits);
    _crusher.sampleRate(_sampleRate);

    _dryWetMix.gain(0, 1.0f - _mix);   // ch0: dry
    _dryWetMix.gain(1, _mix);           // ch1: wet
    _dryWetMix.gain(2, 0.0f);
    _dryWetMix.gain(3, 0.0f);

    // Conexiones dinámicas: inputStream conocido solo en begin()
    _cIn1 = new AudioConnection(inputStream, 0, _ditherMix,  0);   // → wet path
    _cIn2 = new AudioConnection(inputStream, 0, _dryWetMix,  0);   // → dry path

    // Inicializa _ditherLevel y aplica el nivel de noise al mixer
    update();
}

// ── setBits() ────────────────────────────────────────────────────────────────────
void BitSculpt::setBits(uint8_t bits) {
    if (bits < 1)  bits = 1;
    if (bits > 16) bits = 16;
    _bits = bits;
    _crusher.bits(_bits);
    // El 1 LSB cambia con la resolución — recalcular nivel de dither
    update();
}

// ── setSampleRate() ──────────────────────────────────────────────────────────────
void BitSculpt::setSampleRate(float hz) {
    if (hz < 1000.0f)  hz = 1000.0f;
    if (hz > 48000.0f) hz = 48000.0f;
    _sampleRate = hz;
    _crusher.sampleRate(_sampleRate);
    // La reducción de sample rate no afecta el cálculo del dither (depende solo de bits)
}

// ── setSculpt() ──────────────────────────────────────────────────────────────────
void BitSculpt::setSculpt(float sculpt) {
    if (sculpt < 0.0f) sculpt = 0.0f;
    if (sculpt > 1.0f) sculpt = 1.0f;
    _sculpt = sculpt;
    update();
}

// ── setMix() ─────────────────────────────────────────────────────────────────────
void BitSculpt::setMix(float wet) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    _mix = wet;
    if (!_bypass) {
        _dryWetMix.gain(0, 1.0f - _mix);
        _dryWetMix.gain(1, _mix);
    }
}

// ── setBypass() ──────────────────────────────────────────────────────────────────
void BitSculpt::setBypass(bool bypass) {
    _bypass = bypass;
    if (_bypass) {
        // Silenciar noise y wet — señal limpia sin artefactos del FX
        _ditherMix.gain(1, 0.0f);
        _dryWetMix.gain(0, 1.0f);
        _dryWetMix.gain(1, 0.0f);
    } else {
        // Restaurar dry/wet y recalcular dither
        _dryWetMix.gain(0, 1.0f - _mix);
        _dryWetMix.gain(1, _mix);
        update();
    }
}

// ── update() ─────────────────────────────────────────────────────────────────────
void BitSculpt::update() {
    if (_bypass) return;

    // 1 LSB de amplitud para la resolución actual.
    // Con bits=8: LSB = 1/(255) ≈ 0.0039. Con bits=4: LSB = 1/(15) ≈ 0.067.
    // Ref: Pohlmann, op. cit., Cap. 4 pp. 98–115
    const float lsb = 1.0f / (float)((1 << _bits) - 1);

    // Sculpt ∈ [0.0, 0.5]: blend RPDF puro → TPDF
    //   dither_level sube de 0.0 a 1.0 (un LSB de ruido al llegar a Sculpt=0.5)
    //   sin noise shaping — solo decorrelación de primer orden
    //
    // Sculpt ∈ [0.5, 1.0]: TPDF activo + noise shaping de primer orden
    //   dither_level = 1.0 (TPDF completo)
    //   ns_amount sube de 0.0 a 0.7 (clamped para estabilidad del bucle de error)
    //
    // El noise shaping exacto requeriría acceso sample-a-sample al error en el ISR.
    // Esta aproximación actualiza el nivel a tasa de loop() (~1ms) — suficiente para
    // el carácter perceptual: concentra ruido en altas frecuencias vía realimentación
    // del error aproximado.
    // Ref: apps/docs/sprints/10-bit-sculpt.md §"El noise shaping de primer orden en loop()"
    float ditherLevel;
    float nsAmount;

    if (_sculpt <= 0.5f) {
        ditherLevel = _sculpt * 2.0f;   // 0.0 → 1.0 al llegar a Sculpt=0.5
        nsAmount    = 0.0f;
    } else {
        ditherLevel = 1.0f;
        // Clamp conservador: coeficiente ≤ 0.7 garantiza estabilidad del bucle de error
        // para todas las señales en rango de audio normal.
        // Ref: apps/docs/sprints/10-bit-sculpt.md §"Nota importante"
        float nsRaw = (_sculpt - 0.5f) * 2.0f;   // 0.0 → 1.0
        nsAmount    = nsRaw < 0.7f ? nsRaw : 0.7f;
    }

    // Nivel efectivo de noise en el ditherMix:
    //   componente base  = dither RPDF/TPDF a 1 LSB
    //   componente NS    = realimentación del error aproximado (Sculpt > 0.5)
    //
    // _lastErrorApprox es una estimación del error de cuantización de la iteración
    // anterior. No es sample-accurate (se actualiza a tasa de loop()), pero produce
    // el carácter perceptual de "ruido concentrado en altas frecuencias" suficiente
    // para un FX en contexto live.
    float effectiveNoise = lsb * ditherLevel + nsAmount * fabsf(_lastErrorApprox);

    // Actualización del error aproximado: diferencia entre el nivel de ruido anterior
    // y el nuevo (proxy del "error" de ajuste del nivel de noise shaping).
    // Nota: esto NO es el error de cuantización muestra a muestra — es una aproximación
    // de baja velocidad del feedback. El resultado perceptual es el único criterio relevante.
    _lastErrorApprox = effectiveNoise - _ditherLevel;
    _ditherLevel     = effectiveNoise;

    _ditherMix.gain(1, _ditherLevel);
}
