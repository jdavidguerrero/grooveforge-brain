#pragma once

/**
 * @file audio_graph.h
 * @brief Backbone del audio graph del GrooveForge Brain — Sprint 38.
 *
 * ## Por qué este módulo existe
 *
 * El Teensy Audio Library impone tres restricciones inmutables:
 *   1. `AudioOutputI2S` es singleton de hardware (DMA del SAI hardcoded).
 *   2. `AudioControlSGTL5000` es singleton (registro I2C en 0x0A).
 *   3. `AudioConnection` se registra en el grafo global EN CONSTRUCCIÓN — no
 *      se puede re-rutear a otro destino tras instanciar.
 *
 * Antes de Sprint 36, `MoogModelD` poseía estos singletons internamente. Eso
 * impedía que `Juno106` y `Prophet5` produjeran audio simultáneamente (cada
 * uno necesitaría su propio `AudioOutputI2S`, lo cual rompe la regla 1).
 *
 * Sprint 38: se expande a 6 engines con topología de mixers en cascada.
 * AudioMixer4 tiene 4 canales máximos, por lo que se usan dos submixers
 * y un bus de combinación:
 *
 *   ```
 *   moog.getOutput()    ──ch0──┐
 *   juno.getOutput()    ──ch1──┤── g_engine_mix_a ──ch0──┐
 *   prophet.getOutput() ──ch2──┤                          ├── g_engine_bus ──ch0──→ g_master_mix ──→ g_i2s (L+R)
 *   ob6.getOutput()     ──ch3──┘                          │        ↑
 *                                                         │        │
 *   dx7.getOutput()     ──ch0──┐                          │        │
 *   arp.getOutput()     ──ch1──┴── g_engine_mix_b ──ch1──┘        │
 *                                                                   │
 *                              FxManager output ────────────────────┘ (Sprint 36 Batch E)
 *   ```
 *
 *   Engine switching:  helpers internos _submix_for() / _chan_for() despachan
 *                      al submixer correcto según engine_id.
 *   FX bypass:         FxManager no conecta nada → master_mix solo recibe engines.
 *   FX activo:         FxManager conecta su output a `g_master_mix` ch1.
 *
 * ## AudioMemory budget
 *
 * Cálculo de bloques (cada bloque = 512B, 128 samples × 4 bytes):
 *   - 5 mixers backbone (mix_a, mix_b, bus, master, audio_in path): ~5 bloques
 *   - AudioOutputI2S:          2 bloques (L+R double-buffered)
 *   - MoogModelD activo:      ~12 bloques (3 VCO + mixer + filter + env)
 *   - Juno106 (oscs corren):  ~6 bloques (DCO + sub + noise + chorus + dryWet)
 *   - Prophet5 (5 voces):     ~22 bloques (5 × 4 + master mixers)
 *   - OB6 (6 voces):          ~30 bloques (6 × (vco1+vco2+sub+noise+mix+filter+env) + masters)
 *   - Margen para FX activo:  ~15 bloques (worst-case Ghost Echo + buffer)
 *                             ─────────────────
 *                             ~92 bloques → AudioMemory(96) para ~5% margen
 *
 * `audio_graph::init()` llama `AudioMemory(96)` = 48KB.
 *
 * Spec: apps/docs/sprints/36-audio-graph-multi-engine.md
 *       apps/docs/sprints/38-ob6-engine.md
 */

#include <Audio.h>
#include <stdint.h>

// ── Singletons de hardware (definidos en audio_graph.cpp) ─────────────────────
extern AudioOutputI2S        g_i2s;
extern AudioControlSGTL5000  g_codec;

// ── Mixers backbone (definidos en audio_graph.cpp) ────────────────────────────
//   g_engine_mix_a: ch0=Moog, ch1=Juno, ch2=Prophet, ch3=OB6
//   g_engine_mix_b: ch0=DX7 (sprint 39), ch1=ARP2600 (sprint 40)
//   g_engine_bus:   ch0=mix_a, ch1=mix_b → g_master_mix ch0
//   g_master_mix:   ch0=engine bus (SYNTH mode), ch1=FX output, ch2=line-in direct (FX_SELECT)
//                   ch3=USB in (Mac → Teensy passthrough — off by default)
extern AudioMixer4           g_engine_mix_a;  ///< ch0=Moog, ch1=Juno, ch2=Prophet, ch3=OB6
extern AudioMixer4           g_engine_mix_b;  ///< ch0=DX7, ch1=ARP2600
extern AudioMixer4           g_engine_bus;    ///< ch0=mix_a, ch1=mix_b → g_master_mix ch0
extern AudioMixer4           g_master_mix;
extern AudioInputI2S         g_audio_in;   ///< ADC del SGTL5000 — fuente para FX Processor mode

// AudioInputUSB: recibe audio de Mac/PC via USB (Teensy aparece como audio interface).
// NO conflictúa con AudioOutputI2S — comparten el mismo interrupt del SAI.
// La diferencia de clock (44100 Hz USB vs 44117.647 Hz I2S) se absorbe en el FIFO interno.
extern AudioInputUSB         g_usb_in;     ///< USB audio in (Mac passthrough, off by default)

namespace audio_graph {

/**
 * Engine IDs — mapean engine_id a submixer (mix_a o mix_b) y canal.
 *   IDs 0-3 → g_engine_mix_a (canales 0-3)
 *   IDs 4-5 → g_engine_mix_b (canales 0-1)
 */
static constexpr uint8_t ENGINE_MOOG    = 0;
static constexpr uint8_t ENGINE_JUNO    = 1;
static constexpr uint8_t ENGINE_PROPHET = 2;
static constexpr uint8_t ENGINE_OB6     = 3;
static constexpr uint8_t ENGINE_DX7     = 4;
static constexpr uint8_t ENGINE_ARP     = 5;
static constexpr uint8_t NUM_ENGINES    = 6;

/** Gain nominal de un engine activo en g_engine_mix. 0.7 evita clipping al sumar. */
static constexpr float ENGINE_ACTIVE_GAIN = 0.7f;

/**
 * @brief Inicializa el audio path completo.
 *
 * Debe llamarse desde setup() ANTES de instanciar/llamar begin() de cualquier
 * engine. Hace:
 *   1. AudioMemory(80)
 *   2. Wire.begin() + delay para estabilizar I2C
 *   3. g_codec.enable()
 *   4. g_codec.volume(volume) + lineOutLevel(13)
 *   5. Gains iniciales: engine_mix todos en 0 (silencio), master_mix ch0=1 ch1=0
 *
 * @param volume Volumen headphones [0.0, 1.0]. Default 0.5.
 * @return true si el codec respondió por I2C, false si no (revisar SDA=18/SCL=19).
 */
bool init(float volume = 0.5f);

/**
 * @brief Selecciona el engine activo (silencia los otros vía mixer gain).
 *
 * Crossfade de 20ms (4 pasos × 5ms) entre el engine previo y el nuevo para
 * evitar clicks por discontinuidad de DC. Si los dos engines tenían notas
 * activas, el crossfade superpone audio durante la transición.
 *
 * @param engine_id  ENGINE_MOOG | ENGINE_JUNO | ENGINE_PROPHET | ENGINE_OB6 | ENGINE_DX7 | ENGINE_ARP.
 */
void set_active_engine(uint8_t engine_id);

/**
 * @brief Retorna el engine activo actual.
 */
uint8_t get_active_engine();

/**
 * @brief Ajusta el wet del FX activo en FX Processor mode.
 *
 * Solo para FX Processor mode. ch0 (engines) y ch2 (line-in direct) los
 * gestiona set_synth_mode() / set_fx_select_mode().
 *
 *   wet=0.0 → FX silenciado, ch2 off (usar bypass toggle para monitor directo)
 *   wet=1.0 → FX al máximo en ch1, ch2=0
 *
 * @param wet Nivel wet [0.0, 1.0].
 */
void set_fx_wet(float wet);

/**
 * @brief Routing para SYNTH mode: engines → master, line-in y FX silenciados.
 * ch0=1.0 (engines), ch1=0 (FX), ch2=0 (line-in direct).
 * ch3 (USB passthrough) se preserva: si estaba ON sigue ON.
 */
void set_synth_mode();

/**
 * @brief Routing para FX_SELECT: line-in pasa directo al master para monitoreo.
 * ch0=0 (no engines), ch1=0 (no FX), ch2=1.0 (line-in directo).
 * ch3 (USB passthrough) se preserva: si estaba ON sigue ON.
 * Llamar cuando se entra a FX mode sin un FX activo.
 */
void set_fx_select_mode();

/**
 * @brief Activa/desactiva el passthrough USB audio → headphones (ch3 del master mix).
 *
 * Cuando ON: Mac → USB → AudioInputUSB → g_master_mix ch3 → I2S → SGTL5000 → headphones.
 * Persiste a través de cambios de modo (SYNTH/FX) — ch3 no se toca en los mode switches.
 * Activar con el combo secreto HOME held + NAV push.
 *
 * @param enable true = ON (ch3 gain=1.0), false = OFF (ch3 gain=0.0).
 */
void set_usb_passthrough(bool enable);

/**
 * @brief Retorna el estado actual del USB passthrough.
 */
bool get_usb_passthrough();

/**
 * @brief Volume master del codec (afecta jack 3.5mm headphones).
 *
 * @param v Volumen [0.0, 1.0].
 */
void set_volume(float v);

}  // namespace audio_graph
