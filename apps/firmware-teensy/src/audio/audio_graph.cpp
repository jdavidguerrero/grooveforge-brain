/**
 * @file audio_graph.cpp
 * @brief Backbone del audio graph — implementación.
 *
 * Ver audio_graph.h para la documentación completa del diseño.
 */

#include "audio_graph.h"
#include <Wire.h>
#include <Arduino.h>

// ── Singletons de hardware ────────────────────────────────────────────────────
AudioOutputI2S        g_i2s;
AudioControlSGTL5000  g_codec;

// ── Mixers backbone ───────────────────────────────────────────────────────────
AudioMixer4           g_engine_mix_a;   // ch0=Moog, ch1=Juno, ch2=Prophet, ch3=OB6
AudioMixer4           g_engine_mix_b;   // ch0=DX7 (sprint 39), ch1=ARP2600 (sprint 40)
AudioMixer4           g_engine_bus;     // ch0=mix_a, ch1=mix_b → master
AudioMixer4           g_master_mix;

// ADC del SGTL5000 — entrada de línea para FX Processor mode.
// AudioInputI2S y AudioOutputI2S coexisten en Teensy 4.1 (I2S full-duplex).
// En SYNTH mode este objeto está conectado pero su canal en g_master_mix tiene
// gain=0. Solo se abre en FX_SELECT (monitoreo) y FX_MAIN (via FX chain).
AudioInputI2S         g_audio_in;

// USB audio in — recibe audio de Mac/PC cuando Teensy está en modo USB_MIDI_AUDIO_SERIAL.
// AudioInputUSB NO conflictúa con AudioOutputI2S (ambos comparten el SAI interrupt).
// El FIFO interno absorbe el clock drift de 0.04% (44100 vs 44117.647 Hz).
// Por defecto silenciado (ch3 gain=0). Activar con set_usb_passthrough(true).
AudioInputUSB         g_usb_in;

// ── Conexiones backbone (estáticas, viven todo el programa) ───────────────────
//   g_engine_mix_a → g_engine_bus(ch0): submix Moog/Juno/Prophet/OB6
//   g_engine_mix_b → g_engine_bus(ch1): submix DX7/ARP (sprints 39/40)
//   g_engine_bus   → g_master_mix(ch0): dry path SYNTH mode
//   g_audio_in     → g_master_mix(ch2): monitor directo FX_SELECT (gain=0 por defecto)
//   g_usb_in       → g_master_mix(ch3): USB passthrough (gain=0 por defecto — OFF)
//   g_master_mix   → g_i2s (L+R): salida final al codec
//
// FxManager conecta dinámicamente:
//   - g_audio_in → fx_input  (FX Processor mode — NOT g_engine_bus)
//   - fx_output  → g_master_mix(ch1)
static AudioConnection c_mixAToBus      (g_engine_mix_a, 0, g_engine_bus,  0);
static AudioConnection c_mixBToBus      (g_engine_mix_b, 0, g_engine_bus,  1);
static AudioConnection c_busToMaster    (g_engine_bus,   0, g_master_mix,  0);
static AudioConnection c_audioInToMaster(g_audio_in,     0, g_master_mix,  2);
static AudioConnection c_usbInToMaster  (g_usb_in,       0, g_master_mix,  3);
static AudioConnection c_masterToI2sL  (g_master_mix,   0, g_i2s, 0);
static AudioConnection c_masterToI2sR  (g_master_mix,   0, g_i2s, 1);

// ── Estado del módulo ─────────────────────────────────────────────────────────
static uint8_t s_active_engine    = audio_graph::ENGINE_MOOG;
static bool    s_usb_passthrough  = false;  ///< ch3 USB in — off por defecto

// AudioMemory: macro global, debe llamarse exactamente UNA vez antes de
// instanciar audio objects. Como los engines son globales (constructores
// corren antes de setup()), AudioMemory ya está provista por audio_graph::init()
// que se llama al inicio de setup().
//
// 80 bloques × 512B = 40KB. Budget total del firmware: 400KB (RAM2).
// Margen ~33% sobre el cálculo nominal de 60 bloques.

bool audio_graph::init(float volume) {
    // 1. AudioMemory: el pool debe existir antes de que los audio objects pidan bloques.
    // Sprint 38: OB6 (6 voces) agrega ~30 bloques al presupuesto.
    // Ver header §AudioMemory budget para el cálculo detallado.
    AudioMemory(96);

    // 2. Wire.begin() explícito antes de codec.enable():
    //    USBHost_t36 corre constructores globales antes de setup() y puede dejar
    //    el bus I2C sin inicializar. Forzamos init aquí para garantizar bus listo.
    Wire.begin();
    delay(10);

    // 3. Codec SGTL5000 — enable es lo primero que toca I2C
    bool codec_ok = g_codec.enable();
    Serial.print(F("[audio_graph] SGTL5000 enable: "));
    Serial.println(codec_ok ? F("OK") : F("FAIL — revisar SDA=18 SCL=19 y 3.3V"));

    g_codec.volume(volume);        // jack 3.5mm headphones
    g_codec.lineOutLevel(13);      // 3.16Vpp = nivel estándar -10dBV línea

    // ADC line-in para FX Processor mode.
    // lineInLevel(5) = 0 dBu — referencia nominal para salidas de mezcladora DJ.
    // Rango SGTL5000: 0 (máx sensibilidad, ~0.24 Vrms) a 15 (mínima, ~3.2 Vrms).
    // Una mezcladora DJ output típica es ~1 Vrms (≈+2 dBu) → nivel 5 es correcto.
    g_codec.inputSelect(AUDIO_INPUT_LINEIN);
    g_codec.lineInLevel(5);

    // 4. Gains iniciales — SYNTH mode por defecto:
    //    engine_mix_a/b: TODOS en 0 — set_active_engine() enciende el canal correcto.
    //    engine_bus:     ch0=1 y ch1=1 siempre activos (bus suma ambos submixers).
    //                    ch2/ch3 = 0 (AudioMixer4 tiene 4 entradas, solo usamos 2).
    //    master_mix:     ch0 (engine bus) = 1.0 (SYNTH mode activo)
    //                    ch1 (FX output) = 0.0
    //                    ch2 (line-in direct) = 0.0 — off hasta FX_SELECT
    //                    ch3 = 0.0 (USB passthrough — off por defecto)
    for (uint8_t i = 0; i < 4; i++) g_engine_mix_a.gain(i, 0.0f);
    for (uint8_t i = 0; i < 4; i++) g_engine_mix_b.gain(i, 0.0f);
    g_engine_bus.gain(0, 1.0f);   // mix_a siempre pasa por el bus
    g_engine_bus.gain(1, 1.0f);   // mix_b siempre pasa por el bus
    g_engine_bus.gain(2, 0.0f);
    g_engine_bus.gain(3, 0.0f);
    g_master_mix.gain(0, 1.0f);
    g_master_mix.gain(1, 0.0f);
    g_master_mix.gain(2, 0.0f);  // line-in direct — off por defecto (SYNTH mode)
    g_master_mix.gain(3, 0.0f);

    // 5. Engine inicial: Moog activo (matches default de DeviceState).
    //    Esto se sobrescribe luego desde main.cpp cuando EEPROM restore corre.
    g_engine_mix_a.gain(ENGINE_MOOG, ENGINE_ACTIVE_GAIN);
    s_active_engine = ENGINE_MOOG;

    Serial.printf("[audio_graph] init OK — AudioMemory=96, active=%u, vol=%.2f\n",
                  (unsigned)s_active_engine, (double)volume);
    return codec_ok;
}

// ── Helpers de routing por submixer ──────────────────────────────────────────
//
// engine_id 0-3 → g_engine_mix_a (canales 0-3)
// engine_id 4-5 → g_engine_mix_b (canales 0-1)
//
// Estos helpers evitan duplicar la lógica de dispatching en set_active_engine()
// y en cualquier futuro código que necesite acceso por engine_id.

static AudioMixer4* _submix_for(uint8_t id) {
    return (id < 4) ? &g_engine_mix_a : &g_engine_mix_b;
}

static uint8_t _chan_for(uint8_t id) {
    return (id < 4) ? id : (uint8_t)(id - 4);
}

void audio_graph::set_active_engine(uint8_t engine_id) {
    if (engine_id >= NUM_ENGINES) return;
    if (engine_id == s_active_engine) return;

    const uint8_t prev     = s_active_engine;
    AudioMixer4*  prev_mix = _submix_for(prev);
    uint8_t       prev_ch  = _chan_for(prev);
    AudioMixer4*  new_mix  = _submix_for(engine_id);
    uint8_t       new_ch   = _chan_for(engine_id);

    // Crossfade 20ms (4 pasos × 5ms) entre el engine previo y el nuevo.
    // Evita clicks por discontinuidad de DC al hacer hard-cut de gain.
    // Solo tocamos los dos canales involucrados — los demás permanecen en 0.
    const uint8_t STEPS   = 4;
    const uint8_t STEP_MS = 5;
    for (uint8_t step = 1; step <= STEPS; step++) {
        float t = (float)step / (float)STEPS;
        prev_mix->gain(prev_ch, ENGINE_ACTIVE_GAIN * (1.0f - t));
        new_mix->gain(new_ch,   ENGINE_ACTIVE_GAIN * t);
        delay(STEP_MS);
    }
    s_active_engine = engine_id;
    Serial.printf("[audio_graph] active engine -> %u\n", (unsigned)engine_id);
}

uint8_t audio_graph::get_active_engine() {
    return s_active_engine;
}

void audio_graph::set_fx_wet(float wet) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    // FX Processor mode: ch0 (engines) ya fue silenciado por set_fx_select_mode().
    // Solo controlamos ch1 (FX output) y ch2 (direct line-in monitor).
    // ch2 se desactiva porque el FX maneja su propio dry/wet internamente.
    g_master_mix.gain(1, wet);
    g_master_mix.gain(2, 0.0f);  // desactivar bypass directo cuando hay FX activo
}

void audio_graph::set_synth_mode() {
    g_master_mix.gain(0, 1.0f);   // engines dry — activo
    g_master_mix.gain(1, 0.0f);   // FX — off
    g_master_mix.gain(2, 0.0f);   // line-in direct — off
    g_master_mix.gain(3, s_usb_passthrough ? 1.0f : 0.0f);  // USB: preservar estado
    Serial.println(F("[audio_graph] SYNTH mode — engines activos, line-in off"));
}

void audio_graph::set_fx_select_mode() {
    g_master_mix.gain(0, 0.0f);   // engines — silenciados
    g_master_mix.gain(1, 0.0f);   // FX — aún no hay FX activo
    g_master_mix.gain(2, 1.0f);   // line-in direct — monitoreo sin FX
    g_master_mix.gain(3, s_usb_passthrough ? 1.0f : 0.0f);  // USB: preservar estado
    Serial.println(F("[audio_graph] FX_SELECT mode — line-in direct monitor ON"));
}

void audio_graph::set_usb_passthrough(bool enable) {
    s_usb_passthrough = enable;
    g_master_mix.gain(3, enable ? 1.0f : 0.0f);
    Serial.printf("[audio_graph] USB passthrough %s\n", enable ? "ON" : "OFF");
}

bool audio_graph::get_usb_passthrough() {
    return s_usb_passthrough;
}

void audio_graph::set_volume(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    g_codec.volume(v);
}
