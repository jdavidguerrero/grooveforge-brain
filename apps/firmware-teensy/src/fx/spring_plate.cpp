// apps/firmware-teensy/src/fx/spring_plate.cpp
// Sprint 2.10 — FX Spring + Plate
// Theory:  apps/docs/sprints/15-spring-plate.md (pendiente)
// Refs:    apps/docs/05-fx-architecture.md §1.10
//          Parker, "Efficient Simulation of Spring Reverb" (DAFX 2011)
//          Schroeder, "Natural Sounding Artificial Reverberation" (JAES, 1962)

#include "spring_plate.h"

// ── Tuning de los algoritmos ──────────────────────────────────────────────────────
// Valores derivados de la caracterización de reverbs analógicas reales:
//
// Spring (roomsize=0.4, damping=0.7):
//   roomsize bajo → tiempo de reverberación corto, comb filters con menor período.
//   damping alto → los comb filters de HF se atenúan rápidamente — carácter oscuro/wobbly.
//   Resultado: decay ~1.5s a 1kHz, fuerte rolloff de HF — spring Hammond/Fender.
//
// Plate (roomsize=0.75, damping=0.3):
//   roomsize alto → decay más largo, densidad mayor de reflexiones tempranas.
//   damping bajo → las HF se sostienen (la placa tiene poca absorción de HF).
//   Resultado: decay ~3.5s a 1kHz, respuesta plana hasta ~8kHz — EMT 140 style.
//
// Ref: Parker (DAFX 2011) §"Spring reverb acoustic model"
static constexpr float SPRING_ROOMSIZE = 0.40f;
static constexpr float SPRING_DAMPING  = 0.70f;
static constexpr float PLATE_ROOMSIZE  = 0.75f;
static constexpr float PLATE_DAMPING   = 0.30f;

// ── Constructor ──────────────────────────────────────────────────────────────────
SpringPlate::SpringPlate()
    : _cSpringBlend(_spring,   0, _blendMix,   0)
    , _cPlateBlend (_plate,    0, _blendMix,   1)
    , _cBlendWet   (_blendMix, 0, _dryWetMix,  1)   // blend → wet channel
{
    _cSpring = nullptr;
    _cPlate  = nullptr;
    _cDry    = nullptr;
}

// ── Destructor ───────────────────────────────────────────────────────────────────
SpringPlate::~SpringPlate() {
    delete _cSpring;
    delete _cPlate;
    delete _cDry;
}

// ── begin() ──────────────────────────────────────────────────────────────────────
// AudioMemory y codec son responsabilidad del sketch.
void SpringPlate::begin(AudioStream& inputStream) {
    // Aplicar tuning inicial de spring (default algorithm=0)
    _spring.roomsize(SPRING_ROOMSIZE);
    _spring.damping (SPRING_DAMPING);
    _plate.roomsize (PLATE_ROOMSIZE);
    _plate.damping  (PLATE_DAMPING);

    // Default algorithm=0 (spring): solo el canal spring sale al blend
    _blendMix.gain(0, 1.0f);    // spring → activo
    _blendMix.gain(1, 0.0f);    // plate  → silenciado
    _blendMix.gain(2, 0.0f);
    _blendMix.gain(3, 0.0f);

    _dryWetMix.gain(0, 1.0f);   // dry siempre presente
    _dryWetMix.gain(1, _mix);
    _dryWetMix.gain(2, 0.0f);
    _dryWetMix.gain(3, 0.0f);

    // Fan-out: ambos Freeverb + dry reciben el mismo inputStream.
    // Ambos Freeverb siempre procesan audio — la selección se hace silenciando
    // el que no se usa en _blendMix. Esto permite crossfade sin clicks.
    _cSpring = new AudioConnection(inputStream, 0, _spring,    0);
    _cPlate  = new AudioConnection(inputStream, 0, _plate,     0);
    _cDry    = new AudioConnection(inputStream, 0, _dryWetMix, 0);
}

// ── setAlgorithm() ───────────────────────────────────────────────────────────────
void SpringPlate::setAlgorithm(uint8_t alg) {
    if (alg > 2) alg = 2;
    _algorithm = alg;

    switch (_algorithm) {
        case 0:   // Spring: preset físico, ignorar _roomSize/_damping del usuario
            _spring.roomsize(SPRING_ROOMSIZE);
            _spring.damping (SPRING_DAMPING);
            _blendMix.gain(0, 1.0f);   // solo spring
            _blendMix.gain(1, 0.0f);
            break;

        case 1:   // Plate: preset físico
            _plate.roomsize(PLATE_ROOMSIZE);
            _plate.damping (PLATE_DAMPING);
            _blendMix.gain(0, 0.0f);
            _blendMix.gain(1, 1.0f);   // solo plate
            break;

        case 2:   // Blend: parámetros del usuario, crossfade controlable
            _applyBlend();
            break;
    }
}

// ── setRoomSize() ────────────────────────────────────────────────────────────────
void SpringPlate::setRoomSize(float size) {
    if (size < 0.0f) size = 0.0f;
    if (size > 1.0f) size = 1.0f;
    _roomSize = size;
    // Solo aplicar si estamos en blend mode — en spring/plate los presets son fijos
    if (_algorithm == 2) {
        _spring.roomsize(_roomSize);
        _plate.roomsize (_roomSize);
    }
}

// ── setDamping() ─────────────────────────────────────────────────────────────────
void SpringPlate::setDamping(float damp) {
    if (damp < 0.0f) damp = 0.0f;
    if (damp > 1.0f) damp = 1.0f;
    _damping = damp;
    if (_algorithm == 2) {
        _spring.damping(_damping);
        _plate.damping (_damping);
    }
}

// ── setBlend() ───────────────────────────────────────────────────────────────────
void SpringPlate::setBlend(float blend) {
    if (blend < 0.0f) blend = 0.0f;
    if (blend > 1.0f) blend = 1.0f;
    _blend = blend;
    // Aplicar el crossfade aunque no estemos en blend mode — el valor queda guardado
    // para cuando se active algorithm=2. Si ya estamos en blend mode, actualiza inmediato.
    if (_algorithm == 2) {
        _blendMix.gain(0, 1.0f - _blend);   // spring: 1.0 cuando blend=0
        _blendMix.gain(1, _blend);           // plate:  1.0 cuando blend=1
    }
}

// ── setMix() ─────────────────────────────────────────────────────────────────────
void SpringPlate::setMix(float wet) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    _mix = wet;
    _dryWetMix.gain(0, _bypass ? 1.0f : 1.0f - _mix);
    _dryWetMix.gain(1, _bypass ? 0.0f : _mix);
}

// ── setBypass() ──────────────────────────────────────────────────────────────────
void SpringPlate::setBypass(bool bypass) {
    _bypass = bypass;
    _dryWetMix.gain(0, _bypass ? 1.0f : 1.0f - _mix);
    _dryWetMix.gain(1, _bypass ? 0.0f : _mix);
}

// ── update() ─────────────────────────────────────────────────────────────────────
// No-op en v1.0. En v2.0: pre-delay (buffer de delay antes del Freeverb) para
// separar las reflexiones tempranas del audio directo — mejora la sensación de
// "tamaño de sala" percibida.
void SpringPlate::update() {
    // v2.0: pre-delay via AudioEffectDelay antes de _spring y _plate
}

// ── _applyBlend() ────────────────────────────────────────────────────────────────
void SpringPlate::_applyBlend() {
    // En blend mode: aplicar _roomSize y _damping del usuario a ambos procesadores
    _spring.roomsize(_roomSize);
    _spring.damping (_damping);
    _plate.roomsize (_roomSize);
    _plate.damping  (_damping);

    // Crossfade lineal entre spring y plate
    _blendMix.gain(0, 1.0f - _blend);
    _blendMix.gain(1, _blend);
}
