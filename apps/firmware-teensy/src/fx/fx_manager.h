#pragma once

/**
 * @file fx_manager.h
 * @brief FX lifecycle manager — Sprint 36 Batch E.
 *
 * ## Problema que resuelve
 *
 * Los 9 FX implementados usan `AudioEffectDelay`, `AudioSynthKarplusStrong` y
 * otros objetos que reservan buffers de decenas de KB de heap (AudioEffectDelay
 * con 1.4 s @ 48 kHz = ~134 KB). Pre-alocar todos simultáneamente agotaría la
 * RAM2 del Teensy 4.1 (512 KB disponibles para heap).
 *
 * FxManager mantiene activo **solo el FX seleccionado** usando `new`/`delete`.
 * Cada FX tiene un destructor que elimina sus `AudioConnection*` dinámicas antes
 * de que se destruyan sus nodos de audio — esto es lo que hace el ciclo new/delete
 * seguro para el scheduler de la Teensy Audio Library.
 *
 * ## Audio graph cuando un FX está activo
 *
 * ```
 * g_engine_mix ──(FX begin)──→ [FX interno] ──→ g_master_mix ch1
 *      │                                               ↑
 *      └──(c_engineToMaster, siempre)──→ g_master_mix ch0 (dry)
 * ```
 *
 * `audio_graph::set_fx_wet(wet)` controla el balance ch0/ch1 del g_master_mix.
 *
 * ## Seguridad frente al audio ISR
 *
 * `AudioConnection::~AudioConnection()` llama `AudioStream::connect()` con
 * `__disable_irq()` / `__enable_irq()` → la remoción del grafo es atómica.
 * Destruir los AudioConnection antes de los nodos garantiza que el ISR no
 * intentará rutear datos a memoria liberada.
 *
 * Spec: apps/docs/sprints/36-audio-graph-multi-engine.md
 * Refs: apps/docs/05-fx-architecture.md — 9 FX implementados en Sprint 2.x
 */

#include <Audio.h>
#include <stdint.h>

class FxManager {
public:

    // ── FX IDs — orden del spec 05-fx-architecture.md §1 ─────────────────────────
    static constexpr uint8_t FX_CYMATIC_RESONATOR  = 0;   ///< §1.1
    static constexpr uint8_t FX_GRANULAR_CLOUD      = 1;   ///< §1.2
    static constexpr uint8_t FX_GHOST_ECHO          = 2;   ///< §1.3
    static constexpr uint8_t FX_TAPE_SATURATE       = 3;   ///< §1.5
    static constexpr uint8_t FX_BIT_SCULPT          = 4;   ///< §1.6
    static constexpr uint8_t FX_MODAL_REVERB        = 5;   ///< §1.7
    static constexpr uint8_t FX_PHASE_CHORUS        = 6;   ///< §1.8
    static constexpr uint8_t FX_SPRING_PLATE        = 7;   ///< §1.10
    static constexpr uint8_t FX_SUB_GENESIS         = 8;   ///< §1.12
    static constexpr uint8_t NUM_FX                 = 9;
    static constexpr uint8_t FX_NONE               = 0xFF;

    /**
     * @brief Desactiva el FX activo y libera su instancia.
     *
     * Secuencia segura:
     *   1. g_master_mix ch1 gain → 0 (silencia antes de desconectar)
     *   2. delete _c_out (AudioConnection FX output → g_master_mix)
     *   3. delete FX instance (destructor borra sus AudioConnection dinámicas,
     *      luego destruye sus nodos de audio)
     *
     * No-op si no hay FX activo.
     */
    void deactivate();

    /**
     * @brief Activa un FX por ID, conectándolo al stream de entrada especificado.
     *
     * Si hay un FX activo distinto, lo desactiva primero. Si el mismo ID ya
     * está activo, no hace nada.
     *
     * Secuencia de activación:
     *   1. deactivate() si había FX activo
     *   2. new FxClass() → constructor crea conexiones internas del grafo
     *   3. fx->begin(input) → crea AudioConnection* dinámicas desde el stream dado
     *   4. _c_out = new AudioConnection(fx->getOutputL(), 0, g_master_mix, 1)
     *   5. g_master_mix.gain(1, 0.7f) — abre el canal FX del master mix
     *
     * @param fx_id  Uno de FX_* o FX_NONE. No-op si fx_id ≥ NUM_FX.
     * @param input  Stream de entrada del FX. En FX Processor mode: g_audio_in.
     *               En FX-on-synth (futuro): g_engine_mix.
     */
    void activate(uint8_t fx_id, AudioStream& input);

    /** @brief Retorna el ID del FX activo o FX_NONE. */
    uint8_t active_fx() const { return _active_id; }

    /** @brief Retorna true si hay un FX activo. */
    bool is_active() const { return _active_id != FX_NONE; }

    /**
     * @brief Retorna el puntero raw a la instancia del FX activo.
     *
     * Uso exclusivo de apply_fx_param() en el sketch — el llamador debe
     * verificar que `active_fx() == FX_MANAGER_ID[fx_idx]` antes de castear.
     * No usar en producción sin esa guarda; el cast incorrecto es UB.
     *
     * @return void* puntero a la instancia activa, o nullptr si FX_NONE.
     */
    void* get_instance() const { return _instance; }

    /**
     * @brief Controla el wet/dry del FX activo mediante su mixer interno.
     *
     * Llama a `setMix(wet)` del FX activo. No toca `g_master_mix.gain(1)` —
     * ese canal queda fijo en 1.0f cuando el FX está activo, de modo que
     * el wet/dry es un crossfade puro sin efecto de volumen sobre el dry path.
     *
     * No-op si no hay FX activo.
     *
     * @param wet Nivel wet [0.0, 1.0].
     *            0.0 = solo dry (line-in pasa sin efecto).
     *            1.0 = solo wet (efecto al 100%, sin dry).
     */
    void set_wet(float wet);

    /**
     * @brief Avanza el estado interno del FX activo. Llamar desde loop().
     *
     * Solo TapeSaturate (§1.5) tiene un update() significativo (LFOs de Wow y
     * Flutter). Los demás FX tienen update() como no-op.
     *
     * @param dt_ms Tiempo transcurrido desde la última llamada, en milisegundos.
     */
    void update(float dt_ms);

private:
    uint8_t          _active_id = FX_NONE;
    void*            _instance  = nullptr;
    AudioConnection* _c_out     = nullptr;   ///< FX getOutputL() → g_master_mix ch1
};
