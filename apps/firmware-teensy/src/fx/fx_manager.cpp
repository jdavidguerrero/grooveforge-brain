/**
 * @file fx_manager.cpp
 * @brief FX lifecycle manager — implementación.
 *
 * Ver fx_manager.h para diseño completo y justificación de seguridad.
 *
 * IMPORTANTE — AudioInterrupts() / noAudioInterrupts():
 *   No son necesarios aquí porque AudioConnection::~AudioConnection() y el
 *   constructor de AudioConnection usan __disable_irq() / __enable_irq()
 *   internamente. Agregar noAudioInterrupts() extra causaría latencia.
 */

#include "fx_manager.h"
#include "../audio/audio_graph.h"

// ── Includes de FX (compilados solo cuando FxManager está en el build) ────────
#include "cymatic_resonator.h"
#include "granular_cloud.h"
#include "ghost_echo.h"
#include "tape_saturate.h"
#include "bit_sculpt.h"
#include "modal_reverb.h"
#include "phase_chorus.h"
#include "spring_plate.h"
#include "sub_genesis.h"

#include <Arduino.h>

// ── deactivate() ─────────────────────────────────────────────────────────────────

void FxManager::deactivate() {
    if (_active_id == FX_NONE) return;

    // 1. Silenciar el canal FX del master mix ANTES de desconectar.
    //    Evita el click audible por discontinuidad de ganancia al retirar
    //    la conexión mientras el audio ISR está corriendo.
    g_master_mix.gain(1, 0.0f);

    // 2. Destruir la AudioConnection de salida (FX output → g_master_mix ch1).
    //    ~AudioConnection() es atómica (IRQ-protected) y remueve la conexión
    //    del grafo antes de retornar — el ISR no la usará más.
    delete _c_out;
    _c_out = nullptr;

    // 3. Destruir la instancia del FX:
    //    El destructor de cada FX elimina sus AudioConnection* dinámicas (_cIn,
    //    _cDry, etc.) antes de que los nodos de audio (AudioMixer4, AudioEffectDelay,
    //    etc.) sean destruidos. Orden garantizado por el estándar C++:
    //    cuerpo del destructor → miembros en orden inverso de declaración.
    // Salvar id e instancia ANTES de resetear los miembros.
    // Si reseteamos _instance primero, el switch haría delete nullptr (no-op).
    const uint8_t id   = _active_id;
    void* const   inst = _instance;
    _active_id = FX_NONE;
    _instance  = nullptr;

    switch (id) {
        case FX_CYMATIC_RESONATOR:  delete static_cast<CymaticResonator*>(inst);  break;
        case FX_GRANULAR_CLOUD:     delete static_cast<GranularCloud*>(inst);      break;
        case FX_GHOST_ECHO:         delete static_cast<GhostEcho*>(inst);          break;
        case FX_TAPE_SATURATE:      delete static_cast<TapeSaturate*>(inst);       break;
        case FX_BIT_SCULPT:         delete static_cast<BitSculpt*>(inst);          break;
        case FX_MODAL_REVERB:       delete static_cast<ModalReverb*>(inst);        break;
        case FX_PHASE_CHORUS:       delete static_cast<PhaseChorus*>(inst);        break;
        case FX_SPRING_PLATE:       delete static_cast<SpringPlate*>(inst);        break;
        case FX_SUB_GENESIS:        delete static_cast<SubGenesis*>(inst);         break;
        default: break;
    }

    Serial.printf("[FxManager] deactivated id=%u\n", (unsigned)id);
}

// ── activate() ───────────────────────────────────────────────────────────────────

// Macro interna: crea una instancia, llama begin() con el stream de entrada dado,
// conecta su salida al g_master_mix ch1 y almacena todo en los miembros privados.
// Se usa un bloque de código (no una función template) para evitar la necesidad de
// -frtti o excepciones en el build embedded (-fno-rtti -fno-exceptions).
#define FX_CREATE(TYPE, INPUT) \
    do { \
        TYPE* fx = new TYPE(); \
        fx->begin(INPUT); \
        _c_out = new AudioConnection(fx->getOutputL(), 0, g_master_mix, 1); \
        _instance = fx; \
    } while (0)

void FxManager::activate(uint8_t fx_id, AudioStream& input) {
    if (fx_id >= NUM_FX)       return;    // no implementado (IDs 9-11 = future FX)
    if (fx_id == _active_id)   return;    // ya activo — no-op

    // Desactivar el FX previo si lo hay
    deactivate();

    // Crear e instanciar el nuevo FX con el stream de entrada especificado.
    // En FX Processor mode el caller pasa g_audio_in (SGTL5000 line-in ADC).
    switch (fx_id) {
        case FX_CYMATIC_RESONATOR:  FX_CREATE(CymaticResonator, input);  break;
        case FX_GRANULAR_CLOUD:     FX_CREATE(GranularCloud,    input);   break;
        case FX_GHOST_ECHO:         FX_CREATE(GhostEcho,        input);   break;
        case FX_TAPE_SATURATE:      FX_CREATE(TapeSaturate,     input);   break;
        case FX_BIT_SCULPT:         FX_CREATE(BitSculpt,        input);   break;
        case FX_MODAL_REVERB:       FX_CREATE(ModalReverb,      input);   break;
        case FX_PHASE_CHORUS:       FX_CREATE(PhaseChorus,      input);   break;
        case FX_SPRING_PLATE:       FX_CREATE(SpringPlate,      input);   break;
        case FX_SUB_GENESIS:        FX_CREATE(SubGenesis,       input);   break;
        default: return;   // por si acaso — debería estar cubierto por fx_id >= NUM_FX
    }

    _active_id = fx_id;

    // Canal FX del master mix fijo en 1.0. El wet/dry real se controla internamente
    // vía set_wet() → setMix() de cada FX (crossfade puro, sin efecto de volumen).
    g_master_mix.gain(1, 1.0f);

    Serial.printf("[FxManager] activated fx_id=%u input=%s\n",
                  (unsigned)fx_id,
                  &input == (AudioStream*)&g_audio_in ? "line-in" : "engine-mix");
}

#undef FX_CREATE

// ── set_wet() ────────────────────────────────────────────────────────────────────

void FxManager::set_wet(float wet) {
    if (_active_id == FX_NONE || _instance == nullptr) return;
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    switch (_active_id) {
        case FX_CYMATIC_RESONATOR:  static_cast<CymaticResonator*>(_instance)->setMix(wet);  break;
        case FX_GRANULAR_CLOUD:     static_cast<GranularCloud*>(_instance)->setMix(wet);      break;
        case FX_GHOST_ECHO:         static_cast<GhostEcho*>(_instance)->setMix(wet);          break;
        case FX_TAPE_SATURATE:      static_cast<TapeSaturate*>(_instance)->setMix(wet);       break;
        case FX_BIT_SCULPT:         static_cast<BitSculpt*>(_instance)->setMix(wet);          break;
        case FX_MODAL_REVERB:       static_cast<ModalReverb*>(_instance)->setMix(wet);        break;
        case FX_PHASE_CHORUS:       static_cast<PhaseChorus*>(_instance)->setMix(wet);        break;
        case FX_SPRING_PLATE:       static_cast<SpringPlate*>(_instance)->setMix(wet);        break;
        case FX_SUB_GENESIS:        static_cast<SubGenesis*>(_instance)->setMix(wet);         break;
        default: break;
    }
}

// ── update() ─────────────────────────────────────────────────────────────────────

void FxManager::update(float dt_ms) {
    if (_active_id == FX_NONE || _instance == nullptr) return;

    // TapeSaturate (§1.5): LFOs de Wow (0.5 Hz) y Flutter (6 Hz) necesitan update().
    // El resto de FX tienen update() como no-op — no se llaman para ahorrar el branch.
    if (_active_id == FX_TAPE_SATURATE) {
        static_cast<TapeSaturate*>(_instance)->update(dt_ms);
    }
    // GhostEcho::update() es no-op en v1.0 (Markov chain v2.0).
    // Los demás FX no declaran update() con dt_ms.
}
