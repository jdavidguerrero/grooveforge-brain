// Sprint 5.5 — FX Processor (LINE_IN stereo → Reverb + Delay → LINE_OUT)
// Fuente de audio externa (jack 3.5mm) → SGTL5000 ADC → FX chain → SGTL5000 DAC
//
// Control principal: encoders físicos ALPS EC11
//   ENC L       → Dry/Wet mix (gira izq = dry, gira der = wet)
//   ENC L push  → Reset wet/dry a 0% (dry)
//   ENC R       → Parámetro activo del FX (definido por ENC NAV)
//   ENC R push  → Reset parámetro activo a su valor default
//   ENC NAV     → Cicla parámetro de ENC R: Room → Damping → Delay level → Delay ms
//
// Control secundario (MiniLab MIDI CC):
//   CC 74 → reverb room size | CC 71 → reverb damping
//   CC 93 → wet/dry          | CC 73 → delay level
//
// Grafo de audio:
//   LINE_IN L ──┬──▶ AudioEffectFreeverbStereo ──▶ mix_l ch0 (wet L)
//               │                               ──▶ mix_r ch0 (wet R)
//               ├──▶ mix_l ch1 (dry L)
//               └──▶ AudioEffectDelay ──▶ mix_l ch2 (delay)
//   LINE_IN R ──────▶ mix_r ch1 (dry R)
//
// Cross-ref: apps/docs/01-architecture.md §3.3 — pin mapping encoders
//            apps/docs/05-fx-architecture.md §2 — CPU budget

#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include "ui/encoders.h"
#include "usb/midi_host.h"

// ── Audio objects ─────────────────────────────────────────────────────────────
AudioInputI2S             line_in;
AudioEffectFreeverbStereo freeverb;
AudioEffectDelay          fx_delay;
AudioMixer4               mix_l;
AudioMixer4               mix_r;
AudioOutputI2S            line_out;
AudioControlSGTL5000      codec;

// ── Conexiones ────────────────────────────────────────────────────────────────
AudioConnection c1(line_in,  0, freeverb,  0);
AudioConnection c2(freeverb, 0, mix_l,     0);   // wet L
AudioConnection c3(freeverb, 1, mix_r,     0);   // wet R
AudioConnection c4(line_in,  0, mix_l,     1);   // dry L
AudioConnection c5(line_in,  1, mix_r,     1);   // dry R
AudioConnection c6(line_in,  0, fx_delay,  0);
AudioConnection c7(fx_delay, 0, mix_l,     2);   // delay
AudioConnection c8(mix_l,    0, line_out,  0);
AudioConnection c9(mix_r,    0, line_out,  1);

// ── Parámetros ────────────────────────────────────────────────────────────────
static float reverb_size   = 0.5f;   // default moderado — cola ~2s
static constexpr float REVERB_SIZE_MAX = 0.70f;  // Freeverb: 0.70 → cola ~4s, 0.85+ → cola infinita
static float reverb_damp   = 0.5f;
static float wet_dry       = 0.0f;   // arranca dry — ENC L sube el wet
static float delay_level   = 0.0f;   // delay off — ENC NAV → param 2, ENC R sube
static float delay_time_ms = 125.0f; // 125ms ≈ dieciseisavo a 120 BPM (slapback)

// Parámetro activo en ENC R (ENC NAV cicla)
// 0=Room  1=Damping  2=Delay level  3=Delay time
static uint8_t enc_r_param = 0;
static const char* PARAM_NAMES[] = { "Room size", "Damping", "Delay level", "Delay ms" };

static constexpr float STEP_FLOAT = 0.02f;
static constexpr float STEP_DELAY = 20.0f;

static Encoders encoders;
static MidiHost midi_host;

// ── Helpers ───────────────────────────────────────────────────────────────────
static inline float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static void apply_mix() {
    mix_l.gain(0, wet_dry);
    mix_l.gain(1, 1.0f - wet_dry);
    mix_l.gain(2, wet_dry * delay_level);
    mix_l.gain(3, 0.0f);
    mix_r.gain(0, wet_dry);
    mix_r.gain(1, 1.0f - wet_dry);
    mix_r.gain(2, 0.0f);
    mix_r.gain(3, 0.0f);
}

// ── MIDI CC (control secundario) ──────────────────────────────────────────────
static void on_cc(uint8_t ch, uint8_t cc, uint8_t val) {
    float norm = val / 127.0f;
    switch (cc) {
        case 74: reverb_size = norm > REVERB_SIZE_MAX ? REVERB_SIZE_MAX : norm;
                 freeverb.roomsize(reverb_size);
                 Serial.print(F("[MIDI] Room: ")); Serial.println(reverb_size, 2); break;
        case 71: reverb_damp = norm; freeverb.damping(reverb_damp);
                 Serial.print(F("[MIDI] Damp: ")); Serial.println(reverb_damp, 2); break;
        case 93: wet_dry = norm; apply_mix();
                 Serial.print(F("[MIDI] Wet: ")); Serial.println(wet_dry, 2); break;
        case 73: delay_level = norm; apply_mix();
                 Serial.print(F("[MIDI] Delay: ")); Serial.println(delay_level, 2); break;
    }
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println(F("=== Sprint 5.5 - FX Processor ==="));
    Serial.println(F("  ENC L       -> Wet/Dry"));
    Serial.println(F("  ENC L push  -> Reset dry (0%)"));
    Serial.println(F("  ENC NAV     -> Cicla: Room / Damping / Delay level / Delay ms"));
    Serial.println(F("  ENC R       -> Ajusta parametro activo"));
    Serial.println(F("  ENC R push  -> Reset parametro a default"));

    AudioMemory(40);

    Wire.begin();
    delay(10);
    bool ok = codec.enable();
    Serial.print(F("[FX] SGTL5000: "));
    Serial.println(ok ? F("OK") : F("FAIL"));

    codec.inputSelect(AUDIO_INPUT_LINEIN);
    codec.lineInLevel(5);
    codec.volume(0.8f);
    codec.lineOutLevel(13);

    freeverb.roomsize(reverb_size);
    freeverb.damping(reverb_damp);
    fx_delay.delay(0, delay_time_ms);
    apply_mix();

    encoders.init();
    midi_host.on_cc(on_cc);
    midi_host.init();

    Serial.print(F("[ENC R] -> ")); Serial.println(PARAM_NAMES[enc_r_param]);
    Serial.println(F("Listo. Sube ENC L para escuchar el reverb."));
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
    midi_host.poll();
    encoders.update();

    // ENC L → Wet/Dry
    int32_t dl = encoders.read_enc_l();
    if (dl != 0) {
        wet_dry = clamp01(wet_dry + dl * STEP_FLOAT);
        apply_mix();
        Serial.print(F("[ENC L] Wet/Dry: ")); Serial.println(wet_dry, 2);
    }
    if (encoders.sw_l_pressed()) {
        wet_dry = 0.0f;
        apply_mix();
        Serial.println(F("[ENC L push] -> dry 0%"));
    }

    // ENC NAV → cicla parámetro de ENC R
    int32_t dn = encoders.read_enc_nav();
    if (dn != 0) {
        enc_r_param = (enc_r_param + (dn > 0 ? 1 : 3)) % 4;
        Serial.print(F("[ENC NAV] ENC R -> ")); Serial.println(PARAM_NAMES[enc_r_param]);
    }

    // ENC R → parámetro activo
    int32_t dr = encoders.read_enc_r();
    if (dr != 0) {
        switch (enc_r_param) {
            case 0:
                reverb_size = clamp01(reverb_size + dr * STEP_FLOAT);
                if (reverb_size > REVERB_SIZE_MAX) reverb_size = REVERB_SIZE_MAX;
                freeverb.roomsize(reverb_size);
                Serial.print(F("[ENC R] Room: ")); Serial.println(reverb_size, 2);
                break;
            case 1:
                reverb_damp = clamp01(reverb_damp + dr * STEP_FLOAT);
                freeverb.damping(reverb_damp);
                Serial.print(F("[ENC R] Damp: ")); Serial.println(reverb_damp, 2);
                break;
            case 2:
                delay_level = clamp01(delay_level + dr * STEP_FLOAT);
                apply_mix();
                Serial.print(F("[ENC R] Delay level: ")); Serial.println(delay_level, 2);
                break;
            case 3:
                delay_time_ms = constrain(delay_time_ms + dr * STEP_DELAY, 10.0f, 1494.0f);
                fx_delay.delay(0, delay_time_ms);
                Serial.print(F("[ENC R] Delay ms: ")); Serial.println(delay_time_ms, 0);
                break;
        }
    }
    if (encoders.sw_r_pressed()) {
        switch (enc_r_param) {
            case 0: reverb_size = 0.5f; freeverb.roomsize(reverb_size); break;
            case 1: reverb_damp = 0.5f; freeverb.damping(reverb_damp);  break;
            case 2: delay_level = 0.0f; apply_mix(); break;
            case 3: delay_time_ms = 125.0f; fx_delay.delay(0, delay_time_ms); break;
        }
        Serial.print(F("[ENC R push] ")); Serial.print(PARAM_NAMES[enc_r_param]);
        Serial.println(F(" -> default"));
    }

    // Status cada 5s
    static uint32_t t_status = 0;
    uint32_t now = millis();
    if (now - t_status >= 5000) {
        t_status = now;
        Serial.print(F("CPU: ")); Serial.print(AudioProcessorUsage(), 1);
        Serial.print(F("% | Mem: ")); Serial.print(AudioMemoryUsageMax());
        Serial.print(F("/40 | Wet: ")); Serial.print(wet_dry, 2);
        Serial.print(F(" | Room: ")); Serial.print(reverb_size, 2);
        Serial.print(F(" | Delay: ")); Serial.print(delay_time_ms, 0);
        Serial.println(F("ms"));
    }
}
