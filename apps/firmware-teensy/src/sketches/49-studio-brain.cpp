/**
 * 49-studio-brain.cpp — El Brain de Studio con los engines reales, el panel y la cabeza de la Pi.
 *
 * Es el sketch48 (panel + bridge + navegación del Teensy) con los 6 engines del sketch 28 en lugar de la voz
 * de prueba. El sketch 28 queda para la cabeza ESP32 (otra UI, otros pines); este es el de Studio.
 *
 *   teclado MIDI (USB host) → engine activo (MoogModelD · Juno106 · Prophet5 · OB6 · DX7Engine · ARP2600)
 *   → audio_graph (g_engine_mix → I2S → SGTL5000 → LINE OUT)
 *
 * Solo el engine activo procesa audio (set_dsp_enabled): con los 28 filtros MoogLadder4P encendidos a la vez
 * la ISR pasaba del 200 % y el sketch 28 se colgaba (moog_ladder.h).
 *
 * Panel (ADR-004): ENC L (10/11) · ENC R (13/14) · P1 (30/31) · P2 (34/35) · NAV (16/17, push 26) · B1–B4 (2–5)
 * · B5 MODE (40) · B6 SHIFT/HOLD (41) · ENC L push (12) bypass del filtro. En home (vistas) ENC L / R son cutoff /
 * resonancia; en las páginas de edición los 4 encoders editan la página. NAV push largo vuelve a home (theory/04 §3.1).
 * Navegación: studio::StudioUi (theory/04). P1/P2: studio::edit_encoder (theory/lfo-modulacion.md §6).
 * LFO: dsp::Lfo global a 1 kHz → LfoMod al engine activo (theory/lfo-modulacion.md §5).
 *
 * Bridge con la Pi (Serial1 921600, 02-bridge-protocol.md): HEARTBEAT, ENGINE_CHANGED, PARAM_CHANGED (filtro,
 * ENV y LFO del engine activo, gf_params.h), BEAT_DETECTED (BPM del MIDI clock), UI_STATE y NOTE_ON.
 *
 * Cadena de FX del synth (B4, variants/studio/theory/05-studio-synth-fx.md): BIT → TAPE → CHORUS → MODAL, fija,
 * una por lado (g_engine_bus → g_master_mix ch1 y g_engine_bus_r → g_master_mix_r ch1, para no perder el estéreo
 * del Juno). Cada FX se apaga con su setBypass(); las 4 encendidas ≈ 6 % de CPU por lado.
 *
 * Luces (theory/04 §8): una cadena WS2812 en el pin 28: anillo de 12 LEDs bajo NAV (sigue a NAV) y un LED por
 * tecla B1–B6 (destello al apretar sobre su nivel de estado). FastLED de Teensyduino.
 *
 * MIDI: el teclado del USB host y el computador por el USB de la Teensy (usbMIDI: piano roll de Ableton, clock)
 * tocan el mismo engine. El teclado se reenvía al computador (MIDI thru) para grabarlo; lo del computador no se
 * reenvía. En Ableton: External Instrument con MIDI To = Teensy MIDI y, al grabar, Monitor en Off (theory/04 §9).
 *
 * Fuera de esta prueba: el audio del modo FX (B5; la página navega, el motor sigue en el sketch 28), ML. Presets: LittleFS sobre el flash (theory/06).
 *
 *   cd apps/firmware-teensy
 *   pio run -e sketch49 -t upload
 *   pio device monitor -b 115200
 */
#include <Arduino.h>
#include <Audio.h>
#include <Bounce2.h>
#include <Encoder.h>
#include <FastLED.h>
#include <LittleFS.h>
#include <USBHost_t36.h>
#include <gf_params.h>
#include <protocol.h>

#include "../audio/audio_graph.h"
#include "../audio/output_stage.h"
#include "../audio/pump_stage.h"
#include "../dsp/arp_core.h"
#include "../dsp/chord_memory.h"
#include "../dsp/groove_clock.h"
#include "../dsp/pump.h"
#include "../dsp/lfo.h"
#include "../engines/arp2600.h"
#include "../engines/dx7.h"
#include "../engines/juno_106.h"
#include "../engines/moog_model_d.h"
#include "../engines/ob6.h"
#include "../engines/prophet_5.h"
#include "../fx/bit_sculpt.h"
#include "../fx/modal_reverb.h"
#include "../fx/phase_chorus.h"
#include "../fx/tape_saturate.h"
#include "../ui/detent.h"
#include "../ui/groove_macros.h"
#include "../ui/layers.h"
#include "../ui/layer_bank.h"
#include "../ui/page_params.h"
#include "../ui/preset_bank.h"
#include "../ui/preset_link.h"
#include "../ui/bank_file_storage.h"
#include "../ui/panel_leds.h"
#include "../ui/studio_ui.h"

MoogModelD moog;
Juno106 juno;
Prophet5 prophet;
OB6 ob6;
DX7Engine dx7;
ARP2600 arp;
// Globales para que existan antes de que arranque el DMA del I2S (mismo orden de canales que audio_graph.h)
AudioConnection c_moog(moog.getOutput(), 0, g_engine_mix_a, 0);
AudioConnection c_juno(juno.getOutput(), 0, g_engine_mix_a, 1);
AudioConnection c_prophet(prophet.getOutput(), 0, g_engine_mix_a, 2);
AudioConnection c_ob6(ob6.getOutput(), 0, g_engine_mix_a, 3);
AudioConnection c_dx7(dx7.getOutput(), 0, g_engine_mix_b, 0);
AudioConnection c_arp(arp.getOutput(), 0, g_engine_mix_b, 1);
// Lado derecho (fidelidad §9.3): los engines mono a los dos lados; el Juno manda su chorus en estéreo
AudioConnection c_moog_r(moog.getOutput(), 0, g_engine_mix_ra, 0);
AudioConnection c_juno_r(juno.getOutput(), Juno106::OUTPUT_PORT_R, g_engine_mix_ra, 1);
AudioConnection c_prophet_r(prophet.getOutput(), 0, g_engine_mix_ra, 2);
AudioConnection c_ob6_r(ob6.getOutput(), 0, g_engine_mix_ra, 3);
AudioConnection c_dx7_r(dx7.getOutput(), 0, g_engine_mix_rb, 0);
AudioConnection c_arp_r(arp.getOutput(), 0, g_engine_mix_rb, 1);

// Cadena de FX del synth: los 4 viven siempre en el grafo (apagar = bypass, sin recablear en vivo)
/// Una cadena de SYNTH FX (BIT → TAPE → CHORUS → MODAL → PUMP). Hay una por lado para no perder el estéreo del Juno.
struct SfxChain {
    AudioOutputStage out;                    // acople AC + saturación de salida (fidelidad §11.6)
    BitSculpt bit;
    TapeSaturate tape;
    PhaseChorus chorus;
    ModalReverb modal;
    AudioPumpStage pump;                     // al final: bombean también las colas (theory/09 de Studio §2)

    void begin(AudioStream& in, AudioMixer4& master) {
        new AudioConnection(in, 0, out, 0);
        bit.begin(out);
        tape.begin(bit.getOutputL());
        chorus.begin(tape.getOutputL());
        modal.begin(chorus.getOutputL());
        new AudioConnection(modal.getOutputL(), 0, pump, 0);
        new AudioConnection(pump, 0, master, 1);
    }

    void apply(const studio::Patch& p) {
        bit.setBits(gf_sfx_bits(p.sfx_p[GF_SFX_BIT][0]));
        bit.setSampleRate(gf_sfx_sample_rate(p.sfx_p[GF_SFX_BIT][1]));
        bit.setSculpt(p.sfx_p[GF_SFX_BIT][2]);
        tape.setDrive(gf_sfx_drive(p.sfx_p[GF_SFX_TAPE][0]));
        tape.setWow(p.sfx_p[GF_SFX_TAPE][1]);
        tape.setFlutter(p.sfx_p[GF_SFX_TAPE][2]);
        chorus.setDepth(p.sfx_p[GF_SFX_CHORUS][0]);
        chorus.setRate(gf_sfx_chorus_rate(p.sfx_p[GF_SFX_CHORUS][1]));
        chorus.setDrift(p.sfx_p[GF_SFX_CHORUS][2]);
        modal.setDecay(gf_sfx_decay(p.sfx_p[GF_SFX_MODAL][0]));
        modal.setMaterial(ModalReverb::Material(gf_index_from_norm(p.sfx_p[GF_SFX_MODAL][1], 4)));
        modal.setSize(ModalReverb::Size(gf_index_from_norm(p.sfx_p[GF_SFX_MODAL][2], 3)));
        // MIX antes que bypass: setMix() reescribe las ganancias del dry/wet
        bit.setMix(p.sfx_mix[GF_SFX_BIT]);
        tape.setMix(p.sfx_mix[GF_SFX_TAPE]);
        chorus.setMix(p.sfx_mix[GF_SFX_CHORUS]);
        modal.setMix(p.sfx_mix[GF_SFX_MODAL]);
        bit.setBypass(!p.sfx_on[GF_SFX_BIT]);
        tape.setBypass(!p.sfx_on[GF_SFX_TAPE]);
        chorus.setBypass(!p.sfx_on[GF_SFX_CHORUS]);
        modal.setBypass(!p.sfx_on[GF_SFX_MODAL]);
    }

    void update(float dt_ms) {
        tape.update(dt_ms);
        chorus.update(dt_ms);
        bit.update();
        modal.update();
    }
};
SfxChain sfx_l, sfx_r;

// Luces del panel (theory/04 §8)
constexpr uint8_t LED_PIN = 28;
constexpr uint8_t RING_OFFSET = 0;        // LED del anillo que queda arriba (ajustar al montaje)
constexpr uint16_t KEY_FLASH_MS = 400;
CRGB leds[studio::PANEL_LEDS];            // 0–11 anillo · 12–17 teclas B1…B6
CRGB* const ring = leds;
CRGB* const key_led = leds + studio::RING_LEDS;
uint32_t nav_flash_ms = 0;
uint32_t key_flash_ms[studio::KEY_LEDS] = {};

USBHost usb_host;
MIDIDevice midi(usb_host);

constexpr int ENC_PPD = 2;     ///< pulsos por detent: 2 = EC11 del prototipo (medido en el hardware, 2026-10-19)
studio::DetentCounter det_l(ENC_PPD), det_r(ENC_PPD), det_nav(ENC_PPD), det_p1(ENC_PPD), det_p2(ENC_PPD);
Encoder enc_l(10, 11), enc_r(6, 14), enc_nav(16, 17), enc_p1(30, 31), enc_p2(34, 35);
// B1–B4, B5 MODE, B6 SHIFT/HOLD, NAV push, ENC L push
constexpr uint8_t BTN_PINS[8] = {2, 3, 4, 5, 40, 41, 26, 12};
Bounce btn[8];
// Pulsadores de P1 y P2 (ADR-004: P1 SW = 32, P2 SW = 9)
constexpr uint8_t ENC_SW_PINS[2] = {32, 9};
Bounce enc_sw[2];
studio::StudioUi ui;

const char* const ENGINE_NAME[6] = {"MOOG MODEL D", "JUNO-106", "PROPHET-5", "OB-6", "DX7", "ARP 2600"};
// Resonancia de cada engine (descriptores del sketch 28); la cabeza la recibe normalizada 0–1
constexpr GF_Range RESO_RANGE[6] = {{0.1f, 5.0f}, {0.1f, 5.0f}, {0.1f, 5.0f}, {0.7f, 5.0f}, {0.7f, 3.0f}, {0.7f, 5.0f}};
// Cutoff de arranque de cada engine (descriptores del sketch 28)
constexpr float CUTOFF_DEFAULT[6] = {800, 1000, 1000, 2000, 15000, 2000};

float cutoff[6], resonance[6];
studio::Patch patches[GF_ENGINE_COUNT];  // uno por engine: al volver a un engine vuelve su LFO y su ENV
dsp::Lfo lfo;
float wheel = 0.0f;                      // CC 1, 0–1: suma vibrato como en los originales
float bend_st = 0.0f;                    // pitch bend en semitonos (±2, theory/07 §2)
float aftertouch = 0.0f;                 // presión de canal 0–1: cutoff +1 oct y vibrato
bool sustain_pedal = false;              // CC 64
uint8_t notes_held = 0;                  // teclas apretadas (retardo del LFO del Juno)
uint32_t lfo_delay_t0 = 0;               // nota nueva sin otras apretadas: arranca la rampa del LFO
uint32_t sustained[4] = {};              // notas soltadas con el pedal apretado (bit por nota)
constexpr float BEND_RANGE_ST = 2.0f;
constexpr uint8_t MIDI_OUT_CH = 1;
float bpm = 120.0f;                      // BPM del reloj común (MIDI · interno + TAP)
float master_vol_db = 0.0f;              // volumen de salida (DAC): 0 dB = el nivel de siempre; −60 a 0
uint32_t last_bpm_tx = 0;
float bpm_sent = 0.0f;
uint8_t moog_note = 255;                 // el Model D es monofónico: noteOff() sin nota
dsp::Drift mono_drift;                   // DRIFT del Model D (los poly lo llevan por voz adentro)
float mono_drift_pitch = 1.0f, mono_drift_cut = 1.0f;

int32_t last_l = 0, last_r = 0, last_nav = 0, last_p1 = 0, last_p2 = 0;
uint32_t last_stats = 0, last_ui_tx = 0, last_ui_beat = 0;
bool host_seen = false;

studio::Patch& patch() { return patches[ui.engine()]; }

/// Logs de alta frecuencia (cada detent de un encoder): solo con un programa leyendo el puerto y lugar en el buffer.
/// Por el aislador USB, escribir sin lector frenaba el loop al girar P1 / P2 (2026-10-27).
bool log_ok() { return Serial.dtr() && Serial.availableForWrite() > 120; }

// GROOVE (variants/studio/theory/09): reloj común, PUMP, ARP y CHORD
dsp::GrooveClock gclock;
dsp::ArpCore arpeggio;                   // `arp` es el ARP 2600
dsp::ChordMemory chord;
uint8_t keys_down[dsp::ArpCore::MAX_NOTES], keys_n = 0;   // teclas apretadas de verdad (NAV push graba el acorde)
uint8_t chord_n[128], chord_notes[128][dsp::ChordMemory::MAX];   // lo que disparó cada tecla, para soltar lo mismo
struct StrumNote {                       // notas que esperan su momento: STRUM y el retraso del ataque de la capa B
    uint8_t eng, note, vel;
    uint32_t at_us;
};
StrumNote strum_q[16];
uint8_t strum_n = 0;
void groove_reset();

// Capas híbridas (variants/studio/theory/12): suenan A y B; ui.engine() es la capa en FOCO (la que se edita)
int8_t layer_idx = -1;                   // −1 sin capa · 0–31 preset de capa · LAYER_CUSTOM con un engine cambiado
constexpr int8_t LAYER_CUSTOM = 127;
uint8_t layer_a = 0, layer_b = 0xFF, layer_focus = 0;
float layer_gain_a_db = 0.0f, layer_gain_b_db = 0.0f;
int8_t layer_transpose = 0;
uint8_t layer_delay_ms = 0;
char layer_name[13] = "";
char layer_half_a[studio::PRESET_NAME_LEN + 1] = "", layer_half_b[studio::PRESET_NAME_LEN + 1] = "";   // para la cabeza
bool layered() { return layer_idx >= 0; }
void bridge_layer();
/// GROOVE (ARP / CHORD / PUMP / MORPH) y SYNTH FX son de la capa entera: los del patch de A.
studio::Patch& groove_patch() { return layered() ? patches[layer_a] : patch(); }
uint8_t groove_engine() { return layered() ? layer_a : ui.engine(); }
float layer_morph_applied = -1.0f;
dsp::Lfo lfo_b;                          // el LFO de la capa B, con su propio patch

// ── Presets (variants/studio/theory/06): un archivo por preset en LittleFS sobre el flash del programa ──────────

LittleFS_Program preset_fs;
bool preset_fs_ok = false;

/// Archivos enteros en el flash de presets; la escritura es atómica: temporal + renombrar (LittleFS reemplaza el
/// destino en un paso), así un corte a mitad deja el banco anterior entero.
class FlashFileBackend : public studio::FileBackend {
public:
    size_t read_file(const char* path, uint8_t* buf, size_t cap) override {
        if (!preset_fs_ok) return 0;
        File f = preset_fs.open(path, FILE_READ);
        if (!f) return 0;
        const size_t n = f.read(buf, cap);
        f.close();
        return n;
    }
    bool write_file(const char* path, const uint8_t* buf, size_t len) override {
        if (!preset_fs_ok) return false;
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%s.tmp", path);
        preset_fs.remove(tmp);                    // FILE_WRITE agrega al final: el temporal arranca vacío
        File f = preset_fs.open(tmp, FILE_WRITE);
        if (!f) {
            Serial.printf("[PRESET] no se pudo crear %s (usado %lu de %lu bytes)\n", tmp,
                          (unsigned long)preset_fs.usedSize(), (unsigned long)preset_fs.totalSize());
            return false;
        }
        const size_t n = f.write(buf, len);
        f.close();
        if (n != len || !preset_fs.rename(tmp, path)) {
            Serial.printf("[PRESET] no se pudo guardar %s (%u de %u bytes)\n", path, unsigned(n), unsigned(len));
            preset_fs.remove(tmp);
            return false;
        }
        return true;
    }
};

/// Un archivo por engine con sus 64 lugares (theory/13 rev. 3): con bloques de 64 KB, un archivo por preset llenaba
/// el área con ~23 presets.
FlashFileBackend preset_files;
DMAMEM uint8_t preset_bank_buf[studio::BankFileStorage::FILE_BYTES];   // ~34 KB en RAM2

studio::BankFileStorage preset_store(preset_files, preset_bank_buf);

/// Área de presets de 2 MB (theory/13). Sin migración: el área de 512 KB anterior resultó corrupta (sus metadatos
/// apuntaban fuera del flash y leerla colgó la Teensy, 2026-10-27); los presets de fábrica se regeneran solos.
constexpr uint32_t PRESET_FS_BYTES = 2u * 1024u * 1024u;

/// Monta el área y borra los archivos del formato anterior (un archivo por preset en /presets/<engine>/), que
/// ocupaban un bloque de 64 KB cada uno; los presets de fábrica se regeneran en el formato nuevo.
bool preset_fs_mount() {
    if (!preset_fs.begin(PRESET_FS_BYTES)) return false;
    if (!preset_fs.exists("/presets")) return true;
    unsigned n = 0;
    for (uint8_t e = 0; e <= 9; ++e)
        for (uint8_t i = 0; i <= 64; ++i) {
            char path[32];
            snprintf(path, sizeof(path), "/presets/%u/%02u.gfp", e, i);
            if (preset_fs.remove(path)) ++n;
            snprintf(path, sizeof(path), "/presets/%u/%02u.gfp.tmp", e, i);
            preset_fs.remove(path);
        }
    for (uint8_t e = 0; e <= 9; ++e) {
        char dir[16];
        snprintf(dir, sizeof(dir), "/presets/%u", e);
        preset_fs.rmdir(dir);
    }
    preset_fs.rmdir("/presets");
    Serial.printf("[PRESET] formato anterior borrado (%u archivos): se regeneran los de fábrica\n", n);
    return true;
}
studio::PresetBank presets(preset_store);

/// Presets de capa de HYBRID SYNTH (theory/12 §3d): un archivo con las dos mitades completas de cada capa.
DMAMEM uint8_t layer_bank_buf[studio::LayerBank::FILE_BYTES];          // ~35 KB en RAM2
studio::LayerBank layers(preset_files, layer_bank_buf);
uint8_t preset_loaded[GF_ENGINE_COUNT] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ── Engines ─────────────────────────────────────────────────────────────────────────────────────

void engine_note_on(uint8_t e, uint8_t note, float vel) {
    switch (e) {
        // El Model D y el Juno-106 originales no tienen velocity (fidelidad §11.6): suenan siempre a fondo
        // La respuesta a la velocity la decide cada preset (theory/10): 0 % en el Model D y el Juno por defecto
        case 0:
            mono_drift.amount = studio::macro_amount(patches[0], GF_MACRO_DRIFT);
            mono_drift_pitch = mono_drift.pitch();
            mono_drift_cut = mono_drift.cutoff();
            moog.noteOn(note, vel);
            moog_note = note;
            break;
        case 1: juno.noteOn(note, vel); break;
        case 2: prophet.noteOn(note, vel); break;
        case 3: ob6.noteOn(note, vel); break;
        case 4: dx7.noteOn(note, vel); break;
        case 5: arp.noteOn(note, vel); break;
    }
}

void engine_note_off(uint8_t e, uint8_t note) {
    switch (e) {
        case 0: moog.noteOff(note); break;
        case 1: juno.noteOff(note); break;
        case 2: prophet.noteOff(note); break;
        case 3: ob6.noteOff(note); break;
        case 4: dx7.noteOff(note); break;
        case 5: arp.noteOff(note); break;
    }
}

void all_notes_off() {
    groove_reset();
    memset(sustained, 0, sizeof(sustained));
    notes_held = 0;
    moog.noteOff();
    moog_note = 255;
    for (uint8_t n = 0; n <= 127; n++) {
        juno.noteOff(n);
        prophet.noteOff(n);
        ob6.noteOff(n);
        dx7.noteOff(n);
        arp.noteOff(n);
    }
}

// ── GROOVE: CHORD → ARP → engine (theory/09 de Studio §3–§4) ──────────────────────────────────────

void groove_reset() {
    arpeggio.clear();
    strum_n = 0;
    keys_n = 0;
    memset(chord_n, 0, sizeof(chord_n));
}

/// Toca `note` en el engine `e` ya o dentro de `delay_us` (a la cola).
void engine_note_at(uint8_t e, uint8_t note, uint8_t vel, uint32_t delay_us) {
    if (delay_us == 0 || strum_n == 16) { engine_note_on(e, note, vel / 127.0f); return; }
    strum_q[strum_n++] = {e, note, vel, micros() + delay_us};
}

/// Suelta `note` en `e`; si todavía esperaba en la cola, se cancela sin sonar.
void engine_note_release(uint8_t e, uint8_t note) {
    for (uint8_t i = 0; i < strum_n; ++i)
        if (strum_q[i].eng == e && strum_q[i].note == note) {
            strum_q[i] = strum_q[--strum_n];
            return;
        }
    engine_note_off(e, note);
}

/// El último paso antes de los engines: A siempre; con capa, también B (transpuesta y con su retraso de ataque).
void sound_on(uint8_t note, uint8_t vel, uint32_t delay_us) {
    if (!layered()) { engine_note_at(ui.engine(), note, vel, delay_us); return; }
    engine_note_at(layer_a, note, vel, delay_us);
    engine_note_at(layer_b, studio::layer_b_note(note, layer_transpose), vel, delay_us + layer_delay_ms * 1000u);
}

void sound_off(uint8_t note) {
    if (!layered()) { engine_note_release(ui.engine(), note); return; }
    engine_note_release(layer_a, note);
    engine_note_release(layer_b, studio::layer_b_note(note, layer_transpose));
}

void arp_note_on(uint8_t n, uint8_t v, void*) { sound_on(n, v, 0); }
void arp_note_off(uint8_t n, void*) { sound_off(n); }

/// Una nota ya pasada por CHORD: con ARP la toma el arpegiador; si no, va a los engines (con STRUM, a la cola).
void voice_on(uint8_t note, uint8_t vel, uint32_t delay_us) {
    if (groove_patch().groove_on[GF_GROOVE_ARP]) { arpeggio.note_on(note, vel); return; }
    sound_on(note, vel, delay_us);
}

void voice_off(uint8_t note) {
    if (groove_patch().groove_on[GF_GROOVE_ARP]) { arpeggio.note_off(note); return; }
    sound_off(note);
}

void play_on(uint8_t note, uint8_t vel) {
    const studio::Patch& p = groove_patch();
    if (!p.groove_on[GF_GROOVE_CHORD]) {
        chord_n[note] = 0;
        voice_on(note, vel, 0);
        return;
    }
    const float* g = p.groove[GF_GROOVE_CHORD];
    chord.set_type(dsp::ChordMemory::Type(uint8_t(g[GF_CHORD_TYPE])));
    chord.set_inversion(uint8_t(g[GF_CHORD_INV]));
    chord.set_key(uint8_t(g[GF_CHORD_KEY]));
    const uint32_t strum_us = uint32_t(g[GF_CHORD_STRUM] * 1000.0f);
    chord_n[note] = chord.notes(note, chord_notes[note]);
    for (uint8_t i = 0; i < chord_n[note]; ++i) voice_on(chord_notes[note][i], vel, strum_us * i);
}

void play_off(uint8_t note) {
    if (!chord_n[note]) { voice_off(note); return; }
    for (uint8_t i = 0; i < chord_n[note]; ++i) voice_off(chord_notes[note][i]);
    chord_n[note] = 0;
}

/// UNISON del engine `e` (el Model D no tiene: ya son 3 VCO en una voz). @return voces que suenan por nota.
uint8_t engine_unison(uint8_t e, uint8_t voices, float cents) {
    static const uint8_t POLY[GF_ENGINE_COUNT] = {1, JUNO_VOICES, PROPHET_VOICES, OB6_VOICES, DX7_VOICES, ARP_VOICES};
    if (e == 5 && patches[5].mod[0][0] > 0.5f) voices = 1;   // ARP 2600 en MONO: una voz
    const uint8_t n = voices < POLY[e] ? voices : POLY[e];
    switch (e) {
        case 1: juno.setUnison(n, cents); break;
        case 2: prophet.setUnison(n, cents); break;
        case 3: ob6.setUnison(n, cents); break;
        case 4: dx7.setUnison(n, cents); break;
        case 5: arp.setUnison(n, cents); break;
        default: break;
    }
    return n;
}

/// A 1 kHz: BPM y posición del reloj, notas del STRUM, pasos del ARP y ganancia del PUMP.
void groove_tick(uint32_t now) {
    const studio::Patch& p = groove_patch();
    const float(&g)[GF_GROOVE_STOPS][GF_OSC_SLOTS] = p.groove;
    static const dsp::GrooveClock::Source SRC[3] = {dsp::GrooveClock::Source::Auto, dsp::GrooveClock::Source::Midi,
                                                    dsp::GrooveClock::Source::Internal};   // AUTO · MIDI · INT
    gclock.force_source(SRC[uint8_t(g[GF_GROOVE_PUMP][GF_PUMP_SOURCE]) % 3]);
    gclock.update(now);                  // re-fasa al cambiar de fuente (theory/09 §1.1)
    const double pos = gclock.position(now);
    const bool running = gclock.running(now);
    bpm = gclock.bpm(now);
    ui.set_clock(gclock.source(now) == dsp::GrooveClock::Source::Midi, running);   // MIDI / MIDI STOP / INT en la cabeza

    for (uint8_t i = 0; i < strum_n;) {
        if (int32_t(now - strum_q[i].at_us) >= 0) {
            engine_note_on(strum_q[i].eng, strum_q[i].note, strum_q[i].vel / 127.0f);
            strum_q[i] = strum_q[--strum_n];
        } else {
            ++i;
        }
    }

    const float* a = g[GF_GROOVE_ARP];
    arpeggio.set_mode(dsp::ArpCore::Mode(uint8_t(a[GF_ARP_MODE])));
    arpeggio.set_step_beats(GF_ARP_DIV_BEATS[uint8_t(a[GF_ARP_DIV])]);
    arpeggio.set_gate(a[GF_ARP_GATE] * 0.01f);
    arpeggio.set_octaves(uint8_t(a[GF_ARP_OCT]));
    arpeggio.set_swing(a[GF_ARP_SWING] * 0.01f);
    arpeggio.set_latch(a[GF_ARP_LATCH] > 0.5f);
    if (p.groove_on[GF_GROOVE_ARP]) {
        if (running) arpeggio.update(pos);
        else arpeggio.pause();
    }

    static float pump_out = 1.0f;
    float gain = 1.0f;
    if (p.groove_on[GF_GROOVE_PUMP] && running) {
        const float* q = g[GF_GROOVE_PUMP];
        dsp::PumpParams pp;
        pp.depth = q[GF_PUMP_DEPTH] * 0.01f;
        pp.shape = dsp::PumpShape(uint8_t(q[GF_PUMP_SHAPE]));
        pp.rate_beats = GF_PUMP_RATE_BEATS[uint8_t(q[GF_PUMP_RATE])];
        pp.offset_ms = q[GF_PUMP_OFFSET];
        pp.release_ms = q[GF_PUMP_RELEASE];
        gain = dsp::pump_gain(pp, pos, bpm);
        pump_out = gain;
    } else {
        pump_out += (1.0f - pump_out) * 0.05f;   // PUMP apagado o STOP: vuelve a 1 en ~20 ms, sin click
        gain = pump_out;
    }
    const float* u = g[GF_GROOVE_UNISON];
    const uint8_t uni = engine_unison(groove_engine(), p.groove_on[GF_GROOVE_UNISON] ? GF_UNISON_COUNT[uint8_t(u[GF_UNISON_VOICES])] : 1,
                                      u[GF_UNISON_DETUNE]);
    gain *= dsp::unison_gain(uni);   // N voces suman más fuerte: se compensa en la misma etapa del PUMP
    if (layered()) {                 // MORPH (CC 17, P1 en HYBRID) mueve el balance A / B de la capa
        const float morph = g[GF_GROOVE_MACRO][GF_MACRO_MORPH] * 0.01f;
        if (fabsf(morph - layer_morph_applied) > 0.001f) {
            studio::LayerDef d = studio::layer_def(0);
            d.gain_a_db = layer_gain_a_db;
            d.gain_b_db = layer_gain_b_db;
            float ga, gb;
            studio::layer_gains(d, morph, &ga, &gb);
            audio_graph::set_engine_gains(layer_a, ga, layer_b, gb);
            const bool changed = layer_morph_applied >= 0.0f;
            layer_morph_applied = morph;
            if (changed) bridge_layer();
        }
    }
    const float drift = studio::macro_amount(p, GF_MACRO_DRIFT);
    juno.setDrift(drift);
    prophet.setDrift(drift);
    ob6.setDrift(drift);
    dx7.setDrift(drift);
    arp.setDrift(drift);   // N voces suman más fuerte: se compensa en la misma etapa del PUMP
    sfx_l.pump.set_gain(gain);
    sfx_r.pump.set_gain(gain);
}

void engine_dsp(uint8_t e, bool on) {
    switch (e) {
        case 0: moog.set_dsp_enabled(on); break;
        case 1: juno.set_dsp_enabled(on); break;
        case 2: prophet.set_dsp_enabled(on); break;
        case 3: ob6.set_dsp_enabled(on); break;
        case 4: dx7.set_dsp_enabled(on); break;
        case 5: arp.set_dsp_enabled(on); break;
    }
}

void engine_filter(uint8_t e) {
    const float hz = (ui.state().flags & GF_UI_FLAG_FILTER_BYPASS) ? GF_CUTOFF_RANGE[e].hi : cutoff[e];
    const float res = gf_denorm(RESO_RANGE[e], studio::macro_resonance(gf_norm(RESO_RANGE[e], resonance[e]), patches[e]));
    switch (e) {
        case 0: moog.setFilterCutoff(hz); moog.setFilterResonance(res); break;
        case 1: juno.setFilterCutoff(hz); juno.setFilterResonance(res); break;
        case 2: prophet.setFilterCutoff(hz); prophet.setFilterResonance(res); break;
        case 3: ob6.setFilterCutoff(hz); ob6.setFilterResonance(res); break;
        case 4: dx7.setFilterCutoff(hz); dx7.setFilterResonance(res); break;
        case 5: arp.setFilterCutoff(hz); arp.setFilterResonance(res); break;
    }
}

/// Envelopes del amplificador y del filtro (la página ENV elige cuál con NAV), FLT ENV AMT, GLIDE y DRIVE del patch.
void engine_patch(uint8_t e) {
    const studio::Patch mp = studio::macro_patch(patches[e], e);   // con GRIT y POCKET
    const studio::Patch& p = mp;
    const float glide_ms = p.glide * p.glide * 1500.0f;
    const float drv = 0.5f + 3.5f * p.drive;
    float va, vf;
    studio::macro_vel_depths(p, &va, &vf);   // VEL → AMP / FILTER del preset + POCKET (theory/10)
    switch (e) {
        case 0:
            moog.setVCAAttack(p.attack_ms); moog.setVCADecay(p.decay_ms);
            moog.setVCASustain(p.sustain); moog.setVCARelease(p.release_ms);
            moog.setFilterAttack(p.f_attack_ms); moog.setFilterDecay(p.f_decay_ms);
            moog.setFilterSustain(p.f_sustain); moog.setFilterRelease(p.f_release_ms);
            moog.setFilterEnvAmount(p.env_amt);
            moog.setGlide(glide_ms);
            moog.setDrive(drv);
            moog.setVelocityDepth(va, vf);
            break;
#define POLY_PATCH(eng)                                                                                  \
    eng.setAmpEnv(p.attack_ms, p.decay_ms, p.sustain, p.release_ms);                                  \
    eng.setFilterEnv(p.f_attack_ms, p.f_decay_ms, p.f_sustain, p.f_release_ms);                       \
    eng.setFilterEnvAmount(p.env_amt);                                                                \
    eng.setGlide(glide_ms);                                                                           \
    eng.setDrive(drv);                                                                                \
    eng.setVelocityDepth(va, vf);
        case 1: POLY_PATCH(juno) break;
        case 2:
            POLY_PATCH(prophet)
            prophet.setGlide(fmaxf(glide_ms, p.osc[2][3] * 20.0f));
            break;
        case 3: POLY_PATCH(ob6) break;
        case 4: POLY_PATCH(dx7) break;
        case 5: POLY_PATCH(arp) break;
#undef POLY_PATCH
    }
}

/// Página OSC del patch → setters del engine (plantilla GF_OSC_SLOT; niveles en % de la UI → 0–1).
void engine_osc(uint8_t e) {
    const studio::Patch mp = studio::macro_patch(patches[e], e);   // con GRIT (POLY-MOD, X-MOD, FM)
    static const uint16_t W_JUNO[4] = {WAVEFORM_SAWTOOTH, WAVEFORM_SQUARE, WAVEFORM_PULSE, AudioSynthPolyBlep::WAVEFORM_SAW_PULSE};
    static const uint16_t W_ARP[5] = {WAVEFORM_SAWTOOTH, WAVEFORM_TRIANGLE, WAVEFORM_SQUARE, WAVEFORM_PULSE,
                                      WAVEFORM_SINE};
    const float (&o)[GF_OSC_STOPS][GF_OSC_SLOTS] = mp.osc;
    auto w = [&](const uint16_t* t, uint8_t n, float v) { return t[constrain(int(v), 0, n - 1)]; };
    switch (e) {
        case 0:
            for (uint8_t i = 0; i < 3; i++) moog.setWave(i, uint8_t(constrain(int(o[i][0]), 0, 5)));   // 6 formas
            moog.setDetune(o[1][1]);
            moog.setVco3Interval(o[2][1]);
            moog.setOscLevels(o[0][2] / 100.0f, o[1][2] / 100.0f, o[2][2] / 100.0f, o[3][0] / 100.0f);   // NOISE: 4.ª parada
            moog.setNoisePink(o[3][1] > 0.5f);
            moog.setPulseWidth(o[0][4] / 100.0f);
            moog.setVco3Detune(o[2][5]);
            {   // MODULATION: MOD MIX · OSC MOD · FILTER MOD · NOISE | VCO3 KBD · VCO3 LO · OVERDRIVE · FILTER KBD
                const float (&md)[GF_OSC_SLOTS] = mp.mod[0];
                moog.setModulation(md[0] / 100.0f, md[1] > 0.5f, md[2] > 0.5f);
                moog.setDecaySwitch(md[3] > 0.5f);
                moog.setVco3Control(md[4] > 0.5f, md[5] > 0.5f);
                moog.setOverdrive(md[6] / 100.0f);
                moog.setFilterKeyboard(uint8_t(lroundf(md[7])));
            }
            break;
        case 1:
            juno.setWaveform(w(W_JUNO, 4, o[0][0]));
            juno.setSubLevel(o[0][1] / 100.0f);
            juno.setNoiseLevel(o[0][2] / 100.0f);
            juno.setPulseWidth(o[0][3] / 100.0f);
            juno.setChorusMode(uint8_t(constrain(int(lroundf(o[0][4])), 0, 3)));
            juno.setHpf(uint8_t(constrain(int(lroundf(o[0][5])), 0, 3)));
            juno.setRange(uint8_t(constrain(int(lroundf(o[0][6])), 0, 2)));   // 16' / 8' / 4'
            juno.setVcaGate(o[0][7] > 0.5f);                                    // VCA ENV / GATE
            juno.setHiss(mp.mod[0][1] / 100.0f);                                // HISS del BBD (fase2 §3)
            break;
        case 2: {  // theory/prophet-5-completo.md §3.1: OSC A · OSC B · MIXER
            const studio::Patch& p = mp;
            prophet.setOscAFreq(o[0][0]);
            prophet.setOscAPw(o[0][1] / 100.0f);
            prophet.setOscAShape(o[0][2] > 0.5f, o[0][3] > 0.5f);
            prophet.setSync(o[0][4] > 0.5f);
            prophet.setOscBInterval(o[1][0]);
            prophet.setOscBFine(o[1][1]);
            prophet.setOscBPw(o[1][2] / 100.0f);
            prophet.setOscBKeyboard(o[1][3] > 0.5f);
            prophet.setOscBShape(o[1][4] > 0.5f, o[1][5] > 0.5f, o[1][6] > 0.5f);
            prophet.setOscBLoFreq(o[1][7] > 0.5f);
            prophet.setMixer(o[2][0] / 100.0f, o[2][1] / 100.0f, o[2][2] / 100.0f);
            prophet.setGlide(fmaxf(p.glide * p.glide * 1500.0f, o[2][3] * 20.0f)); // 0–100 → 0–2 s
            prophet.setFilterKeyboard(o[2][4] * 0.5f);                            // OFF / HALF / FULL
            const float (&m)[GF_OSC_SLOTS] = p.mod[0], (&wm)[GF_OSC_SLOTS] = p.mod[1];
            prophet.setPolyMod(m[0] / 100.0f, m[1] / 100.0f, m[2] > 0.5f, m[3] > 0.5f, m[4] > 0.5f);
            prophet.setWheelMod(wm[0] / 100.0f, wm[1] > 0.5f, wm[2] > 0.5f, wm[3] > 0.5f, wm[4] > 0.5f, wm[5] > 0.5f);
            break;
        }
        case 3: {
            ob6.setVcoShape(0, o[0][0] / 100.0f);           // SHAPE continua (fidelidad §11.3)
            ob6.setVcoShape(1, o[1][0] / 100.0f);
            ob6.setVco2Interval(o[1][1]);
            ob6.setOscMix(o[0][2] / 100.0f, o[1][2] / 100.0f, o[0][1] / 100.0f, o[0][3] / 100.0f);
            ob6.setPulseWidth(o[0][4] / 100.0f, o[1][4] / 100.0f);
            ob6.setVco2Detune(o[1][5]);
            ob6.setFilterMorph(o[0][6] / 100.0f);
            ob6.setFilterBpMode(o[0][7] > 0.5f);
            const float (&xm)[GF_OSC_SLOTS] = mp.mod[0];
            ob6.setXMod(xm[0] / 100.0f, xm[1] / 100.0f, xm[2] > 0.5f, xm[3] > 0.5f, xm[4] > 0.5f);
            ob6.setSync(xm[5] > 0.5f);
            break;
        }
        case 5:
            arp.setVco1Waveform(w(W_ARP, 5, o[0][0]));
            arp.setPulseWidth(o[0][1] / 100.0f);
            arp.setVco2Waveform(w(W_ARP, 5, o[1][0]));
            arp.setVco2Ratio(o[1][1]);
            arp.setRingMix(o[0][2] / 100.0f);
            arp.setNoiseLevel(o[0][3] / 100.0f);
            arp.setVco1Level(o[0][4] / 100.0f);
            arp.setVco2Level(o[1][4] / 100.0f);
            {   // PATCH (fidelidad §11.4): MONO · SYNC · VCO3 RATE · VCO3 → PITCH | VCO3 → CUTOFF · NOISE COLOR
                const float (&pa)[GF_OSC_SLOTS] = mp.mod[0];
                arp.setMono(pa[0] > 0.5f);
                arp.setSync(pa[1] > 0.5f);
                arp.setVco3(pa[2], pa[3] / 100.0f, pa[4] / 100.0f);
                arp.setNoiseColor(pa[5] / 100.0f);
            }
            break;
        case 4:   // DX7 de 4 operadores: RATIO · LEVEL · (ALGORITHM · FEEDBACK en OP 1) | A · D · S · R
            dx7.setAlgorithm(uint8_t(lroundf(o[0][2])));
            dx7.setFeedback(uint8_t(lroundf(o[0][3])));
            for (uint8_t k = 0; k < 4; k++) {
                dx7.setOp(k, o[k][0], o[k][1]);
                dx7.setOpEnv(k, o[k][4], o[k][5], o[k][6] / 100.0f, o[k][7]);
            }
            {   // MOD: WAVE / DETUNE · KBD / EG (fidelidad §11.1)
                const studio::Patch& p = mp;
                for (uint8_t k = 0; k < 4; k++)
                    dx7.setOpWave(k, uint8_t(constrain(int(lroundf(p.mod[0][k])) - 1, 0, 7)),
                                  int8_t(lroundf(p.mod[0][4 + k])));
                dx7.setKbdScaling(p.mod[1][0], uint8_t(lroundf(p.mod[1][1])), p.mod[1][2]);
            }
            break;
        default: break;
    }
}

void engine_lfo(uint8_t e, const LfoMod& m) {
    switch (e) {
        case 0: moog.setLfoMod(m); break;
        case 1: juno.setLfoMod(m); break;
        case 2: prophet.setLfoMod(m); break;
        case 3: ob6.setLfoMod(m); break;
        case 4: dx7.setLfoMod(m); break;
        case 5: arp.setLfoMod(m); break;
    }
}

/// Lleva al motor las dos cadenas de FX (L y R) con el patch del engine `e` (rangos de gf_params.h).
void apply_sfx(uint8_t e) {
    sfx_l.apply(patches[e]);
    sfx_r.apply(patches[e]);
}

// ── Bridge (master) ────────────────────────────────────────────────────────────────────────────

uint8_t tx_seq = 0;
uint32_t last_hb = 0, last_engine_tx = 0, last_ack_ms = 0, acks = 0;
bool link_up = false;
uint8_t rx_buf[GF_PAYLOAD_MAX + GF_FRAME_MIN_SIZE];
size_t rx_len = 0;

void ui_control(studio::Control c, int8_t v = 1);

void bridge_send(GF_Cmd cmd, const uint8_t* payload, uint8_t len) {
    GF_Frame f;
    gf_frame_build(&f, cmd, tx_seq++, payload, len);
    uint8_t wire[GF_PAYLOAD_MAX + GF_FRAME_MIN_SIZE];
    Serial1.write(wire, gf_frame_serialize(&f, wire, sizeof(wire)));
}

void bridge_param(uint16_t id, float norm) {
    norm = constrain(norm, 0.0f, 1.0f);
    uint8_t p[6];
    memcpy(p, &id, 2);
    memcpy(p + 2, &norm, 4);
    bridge_send(GF_CMD_PARAM_CHANGED, p, 6);
}

/// Lo editado en el panel sale al computador por su CC (theory/07 §3) para que Ableton lo grabe.
uint32_t cc_out_ms[128] = {};            // último envío de cada CC (para no aplicar el eco del computador)

void cc_out(uint16_t id, float norm) {
    const uint8_t cc = studio::param_cc(id, ui.engine());
    if (cc == 0xFF) return;
    if (cc == 74) norm = gf_cutoff_cc_norm(ui.engine(), cutoff[ui.engine()]);   // CC del cutoff: logarítmico
    usbMIDI.sendControlChange(cc, uint8_t(lroundf(constrain(norm, 0.0f, 1.0f) * 127.0f)), MIDI_OUT_CH);
    cc_out_ms[cc] = millis();
}

void bridge_engine() {
    const uint8_t e = ui.engine();
    uint8_t p[24] = {e};
    const size_t n = strlen(ENGINE_NAME[e]);
    memcpy(p + 1, ENGINE_NAME[e], n + 1);
    bridge_send(GF_CMD_ENGINE_CHANGED, p, uint8_t(n + 2));
    last_engine_tx = millis();
}

void bridge_filter() {
    const uint8_t e = ui.engine();
    bridge_param(gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_CUTOFF), gf_norm(GF_CUTOFF_RANGE[e], cutoff[e]));
    bridge_param(gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_RESO), gf_norm(RESO_RANGE[e], resonance[e]));
}

void bridge_patch() {
    studio::ParamChange c[studio::PATCH_PARAM_COUNT];
    const uint8_t n = studio::patch_params(patch(), ui.engine(), c);
    for (uint8_t i = 0; i < n; ++i) bridge_param(c[i].id, c[i].norm);
}

/// Banco del engine activo a la cabeza: un PRESET_ENTRY (0x90, 02 §2.10.1) por preset.
void bridge_presets() {
    const uint8_t e = ui.engine(), n = presets.count(e);
    if (n == 0) {
        const uint8_t p[3] = {e, 0, 0};
        bridge_send(GF_CMD_PRESET_ENTRY, p, 3);
        return;
    }
    for (uint8_t i = 0; i < n; ++i) {
        uint8_t p[3 + studio::PRESET_NAME_LEN + 1] = {e, i, n};
        const char* name = presets.name(e, i);
        const size_t len = strnlen(name, studio::PRESET_NAME_LEN);
        memcpy(p + 3, name, len);
        bridge_send(GF_CMD_PRESET_ENTRY, p, uint8_t(3 + len + 1));
    }
}

/// PRESET_LOADED (0x17): [preset_id 2B, nombre].
void bridge_preset_loaded() {
    const uint8_t e = ui.engine(), i = preset_loaded[e];
    if (i == 0xFF) return;
    uint8_t p[2 + studio::PRESET_NAME_LEN + 1] = {i, 0};
    const char* name = presets.name(e, i);
    const size_t len = strnlen(name, studio::PRESET_NAME_LEN);
    memcpy(p + 2, name, len);
    bridge_send(GF_CMD_PRESET_LOADED, p, uint8_t(2 + len + 1));
}

/// Lleva un preset al engine `e`; si es el activo, también a las cadenas de FX y a la cabeza.
void apply_preset(uint8_t e, const studio::Preset& p) {
    // El preset trae sus encendidos de ARP / CHORD / UNISON: si cambian con notas sonando, quién suelta cada nota
    // cambia y alguna quedaría colgada (como al encenderlos con NAV push)
    if (e == ui.engine()) all_notes_off();
    patches[e] = p.patch;
    cutoff[e] = constrain(p.cutoff_hz, GF_CUTOFF_RANGE[e].lo, GF_CUTOFF_RANGE[e].hi);
    resonance[e] = gf_denorm(RESO_RANGE[e], p.reso_norm);
    engine_osc(e);
    engine_filter(e);
    engine_patch(e);
    if (e != ui.engine()) return;
    apply_sfx(groove_engine());                     // en capa, los SYNTH FX son los de A
    if (!link_up) return;                           // al conectar, bridge_all() manda todo
    bridge_filter();
    bridge_patch();
}

void layer_select(int8_t idx);
void activate_engine(uint8_t e);

void load_preset(uint8_t i) {
    const uint8_t e = ui.engine();                   // en capa: la mitad en foco, sin salir de la capa
    studio::Preset p;
    if (!presets.load(e, i, &p)) {
        Serial.printf("[PRESET] %u no se pudo leer\n", i);
        return;
    }
    preset_loaded[e] = i;
    apply_preset(e, p);
    bridge_preset_loaded();
    if (layered()) bridge_layer();
    Serial.printf("[PRESET] cargado %u %s\n", i, p.name);
}

// ── Capas híbridas (variants/studio/theory/12) ─────────────────────────────────────────────────────────

/// La cabeza ve la capa en foco como el engine de siempre (páginas, presets, filtro) y la capa en LAYER_STATE.
void focus_engine(uint8_t e) {
    ui.set_engine(e);
    ui.set_preset_count(presets.count(e));
    bridge_engine();
    bridge_filter();
    bridge_patch();
    bridge_presets();
    bridge_preset_loaded();
    bridge_layer();
}

/// Lista de presets de capa a la cabeza: un LAYER_ENTRY (0x47) por preset, como PRESET_ENTRY.
void bridge_layers() {
    const uint8_t n = layers.count();
    if (n == 0) {
        const uint8_t p[3] = {0, 0, 0};
        bridge_send(GF_CMD_LAYER_ENTRY, p, 3);
        return;
    }
    for (uint8_t i = 0; i < n; ++i) {
        uint8_t p[3 + studio::LAYER_NAME_LEN + 1] = {i, n, 0};   // [índice, cantidad, (reservado), nombre]
        const char* name = layers.name(i);
        const size_t len = strnlen(name, studio::LAYER_NAME_LEN);
        memcpy(p + 3, name, len);
        bridge_send(GF_CMD_LAYER_ENTRY, p, uint8_t(3 + len + 1));
    }
}

/// Sale de la capa: queda A sola en SYNTH.
void layer_off() {
    const bool was = layered();
    const uint8_t prev_a = layer_a, prev_b = layer_b;
    layer_idx = -1;
    layer_morph_applied = -1.0f;
    all_notes_off();
    if (was) {
        if (ui.engine() != prev_a) ui.set_engine(prev_a);
        if (prev_b != prev_a && prev_b != 0xFF) {
            engine_dsp(prev_b, false);
            engine_lfo(prev_b, LfoMod{});
        }
    }
    layer_b = 0xFF;
    layer_focus = 0;
    ui.set_focus(0);
    audio_graph::set_active_engine(ui.engine());
    ui.set_layer(false);
    ui.set_hybrid(false);
    if (was) focus_engine(ui.engine());
    Serial.println("[CAPA] OFF");
}

/// Hace sonar el preset de capa `l` (índice `idx` del banco). A y B suenan; el foco arranca en A.
void layer_apply(const studio::LayerPreset& l, int8_t idx) {
    const bool was = layered();
    const uint8_t prev_b = layer_b;
    layer_idx = -1;
    layer_morph_applied = -1.0f;
    if (ui.engine() != l.eng_a) {
        ui.set_engine(l.eng_a);
        activate_engine(l.eng_a);                    // apaga el resto (incluida la B anterior)
    } else if (was && prev_b != 0xFF && prev_b != l.eng_b) {
        engine_dsp(prev_b, false);
        engine_lfo(prev_b, LfoMod{});
    }
    layer_a = l.eng_a;
    layer_b = l.eng_b;
    layer_focus = 0;
    ui.set_focus(0);
    layer_gain_a_db = l.gain_a_db;
    layer_gain_b_db = l.gain_b_db;
    layer_transpose = l.transpose_b;
    layer_delay_ms = l.delay_b_ms;
    snprintf(layer_name, sizeof(layer_name), "%s", l.name);
    snprintf(layer_half_a, sizeof(layer_half_a), "%s", l.a.name);
    snprintf(layer_half_b, sizeof(layer_half_b), "%s", l.b.name);
    preset_loaded[l.eng_a] = preset_loaded[l.eng_b] = 0xFF;   // las mitades viven en el preset de capa
    apply_preset(l.eng_a, l.a);                      // suelta las notas (apply_preset del engine activo)
    engine_dsp(l.eng_b, true);
    apply_preset(l.eng_b, l.b);
    engine_unison(l.eng_b, 1, 0.0f);
    for (uint8_t i = 0; i < GF_ENGINE_COUNT; ++i)    // de una capa a otra: lo que no suena, apagado (CPU)
        if (i != l.eng_a && i != l.eng_b) {
            engine_dsp(i, false);
            engine_lfo(i, LfoMod{});
        }
    patches[l.eng_a].groove[GF_GROOVE_MACRO][GF_MACRO_MORPH] = l.morph;
    layer_idx = idx;                                 // groove_tick aplica las ganancias de MORPH
    ui.set_layer(true);
    ui.set_hybrid(true);
    if (idx >= 0 && idx < int8_t(studio::LAYER_MAX)) ui.set_layer_cursor(uint8_t(idx));
    focus_engine(l.eng_a);
    Serial.printf("[CAPA] %d %s: A %s + B %s\n", idx + 1, l.name, ENGINE_NAME[l.eng_a], ENGINE_NAME[l.eng_b]);
}

/// Carga el preset de capa `idx` del banco, o sale de la capa (−1).
void layer_select(int8_t idx) {
    studio::LayerPreset l;
    if (idx < 0 || !layers.get(uint8_t(idx), &l)) {
        if (idx >= 0) Serial.printf("[CAPA] preset de capa %d no se pudo leer\n", idx);
        layer_off();
        return;
    }
    layer_apply(l, idx);
}

/// + CUSTOM (theory/12 §3d rev. 5): los dos engines elegidos, con sus mitades en INIT, para empezar a editar.
void layer_custom(uint8_t ea, uint8_t eb) {
    if (ea >= GF_ENGINE_COUNT || eb >= GF_ENGINE_COUNT || ea == eb) return;
    studio::LayerPreset l;
    snprintf(l.name, sizeof(l.name), "CUSTOM");
    l.eng_a = ea;
    l.eng_b = eb;
    const uint8_t half[2] = {ea, eb};
    studio::Preset* out[2] = {&l.a, &l.b};
    for (uint8_t k = 0; k < 2; ++k) {
        studio::patch_init(out[k]->patch, half[k]);
        out[k]->cutoff_hz = CUTOFF_DEFAULT[half[k]];
        out[k]->reso_norm = 0.15f;
        snprintf(out[k]->name, sizeof(out[k]->name), "INIT %c", k ? 'B' : 'A');
    }
    layer_apply(l, LAYER_CUSTOM);
}

/// SAVE en LAYER_PRESETS: guarda la capa tal como suena (las dos mitades, MORPH, retraso, transposición, mezcla).
void layer_save() {
    if (!layered()) {
        ui.set_layer_count(layers.count());
        Serial.println("[CAPA] no hay capa sonando: nada que guardar");
        return;
    }
    studio::LayerPreset l;
    l.eng_a = layer_a;
    l.eng_b = layer_b;
    const uint8_t half[2] = {layer_a, layer_b};
    studio::Preset* out[2] = {&l.a, &l.b};
    const char* names[2] = {layer_half_a, layer_half_b};
    for (uint8_t k = 0; k < 2; ++k) {
        const uint8_t e = half[k];
        out[k]->patch = patches[e];
        out[k]->cutoff_hz = cutoff[e];
        out[k]->reso_norm = gf_norm(RESO_RANGE[e], resonance[e]);
        snprintf(out[k]->name, sizeof(out[k]->name), "%s", names[k][0] ? names[k] : ENGINE_NAME[e]);
    }
    l.morph = uint8_t(lroundf(patches[layer_a].groove[GF_GROOVE_MACRO][GF_MACRO_MORPH]));
    l.delay_b_ms = layer_delay_ms;
    l.transpose_b = layer_transpose;
    l.gain_a_db = int8_t(lroundf(layer_gain_a_db));
    l.gain_b_db = int8_t(lroundf(layer_gain_b_db));
    const int i = layers.save_new(l);
    ui.set_layer_count(layers.count());              // StudioUi ya sumó uno: si falló, vuelve al real
    if (i < 0) {
        Serial.println("[CAPA] no se pudo guardar (banco de capas lleno o sin LittleFS)");
        return;
    }
    layer_idx = int8_t(i);
    snprintf(layer_name, sizeof(layer_name), "%s", layers.name(uint8_t(i)));
    ui.set_layer_cursor(uint8_t(i));
    bridge_layers();
    bridge_layer();
    Serial.printf("[CAPA] guardada %d %s\n", i, layer_name);
}

/// P1 / P2 push en HYBRID: la capa en foco pasa a ser la que editan las páginas, PRESETS y ENGINE SELECT.
void layer_set_focus(uint8_t f) {
    if (!layered()) return;
    layer_focus = f ? 1 : 0;
    ui.set_focus(layer_focus);
    focus_engine(layer_focus ? layer_b : layer_a);
    Serial.printf("[CAPA] foco en %c (%s)\n", layer_focus ? 'B' : 'A', ENGINE_NAME[ui.engine()]);
}

/// ENGINE SELECT en el modo HYBRID: cambia el engine de la capa en foco (capa CUSTOM).
void layer_replace_engine(uint8_t e) {
    const uint8_t other = layer_focus ? layer_a : layer_b, old = layer_focus ? layer_b : layer_a;
    if (e == other || e == old) {                    // no puede ser el de la otra capa
        ui.set_engine(old);
        focus_engine(old);
        return;
    }
    all_notes_off();
    engine_dsp(old, false);
    engine_lfo(old, LfoMod{});
    engine_dsp(e, true);
    if (layer_focus) layer_b = e;
    else {
        // MORPH vive en el patch de A: pasa al engine nuevo
        patches[e].groove[GF_GROOVE_MACRO][GF_MACRO_MORPH] = patches[old].groove[GF_GROOVE_MACRO][GF_MACRO_MORPH];
        layer_a = e;
    }
    studio::Preset p;
    if (presets.load(e, 0, &p)) {
        preset_loaded[e] = 0;
        apply_preset(e, p);
        snprintf(layer_focus ? layer_half_b : layer_half_a, sizeof(layer_half_a), "%s", p.name);
    } else {
        engine_osc(e);
        engine_filter(e);
        engine_patch(e);
    }
    engine_unison(layer_b, 1, 0.0f);
    layer_idx = LAYER_CUSTOM;
    layer_gain_a_db = layer_gain_b_db = 0.0f;
    snprintf(layer_name, sizeof(layer_name), "CUSTOM");
    layer_morph_applied = -1.0f;
    apply_sfx(layer_a);
    focus_engine(e);
    Serial.printf("[CAPA] CUSTOM: A %s + B %s\n", ENGINE_NAME[layer_a], ENGINE_NAME[layer_b]);
}

/// Con capa, el filtro de la otra mitad sigue al de la capa en foco: cutoff en proporción, resonancia con el mismo paso.
void layer_follow_filter(float cutoff_ratio, float reso_step_norm) {
    if (!layered()) return;
    const uint8_t b = ui.engine() == layer_a ? layer_b : layer_a;
    cutoff[b] = constrain(cutoff[b] * cutoff_ratio, GF_CUTOFF_RANGE[b].lo, GF_CUTOFF_RANGE[b].hi);
    const float rn = constrain(gf_norm(RESO_RANGE[b], resonance[b]) + reso_step_norm, 0.0f, 1.0f);
    resonance[b] = gf_denorm(RESO_RANGE[b], rn);
    engine_filter(b);
}

void save_preset() {
    const uint8_t e = ui.engine();
    studio::Preset p;
    p.patch = patches[e];
    p.cutoff_hz = cutoff[e];
    p.reso_norm = gf_norm(RESO_RANGE[e], resonance[e]);
    const int i = presets.save_new(e, p);
    ui.set_preset_count(presets.count(e));          // StudioUi ya sumó uno: si falló, vuelve al real
    if (i < 0) {
        Serial.println("[PRESET] no se pudo guardar (banco lleno o sin LittleFS)");
        return;
    }
    preset_loaded[e] = uint8_t(i);
    bridge_presets();
    bridge_preset_loaded();
    Serial.printf("[PRESET] guardado %d %s\n", i, presets.name(e, uint8_t(i)));
}

void bridge_bpm() {
    const uint16_t x10 = uint16_t(lroundf(bpm * 10.0f));
    const double pos = gclock.position(micros());
    const uint8_t phase = uint8_t((pos - floor(pos)) * 256.0);   // 02 §2.9: fase dentro del beat, 0–255
    const uint8_t p[3] = {uint8_t(x10 & 0xFF), uint8_t(x10 >> 8), phase};
    bridge_send(GF_CMD_BEAT_DETECTED, p, 3);
    last_bpm_tx = millis();
    bpm_sent = bpm;
}

/// Volumen de salida en el DAC del SGTL5000 (afecta LINE OUT y auriculares; pasos de 0.5 dB: n = 1 + dB / 96).
void apply_master_volume() { g_codec.dacVolume(1.0f + master_vol_db / 96.0f); }

void bridge_master_volume() { bridge_param(GF_PARAM_MASTER_VOL, gf_vol_norm(master_vol_db)); }

void bridge_ui_state() {
    uint8_t p[GF_UI_STATE_LEN];
    gf_ui_state_pack(&ui.state(), p);
    bridge_send(GF_CMD_UI_STATE, p, GF_UI_STATE_LEN);
    last_ui_tx = last_ui_beat = millis();
}

/// LAYER_STATE (0x46, theory/12 §3c): la capa, el foco, MORPH y los presets de las dos mitades.
void bridge_layer() {
    GF_LayerState l{};
    l.layer = layer_idx;
    l.cursor = ui.layer_cursor();
    l.engine_a = layer_a;
    l.engine_b = layer_b == 0xFF ? layer_a : layer_b;
    l.focus = layer_focus;
    l.morph = uint8_t(lroundf(patches[layer_a].groove[GF_GROOVE_MACRO][GF_MACRO_MORPH]));
    l.delay_b_ms = layer_delay_ms;
    l.transpose_b = layer_transpose;
    snprintf(l.name, sizeof(l.name), "%s", layered() ? layer_name : "");
    snprintf(l.preset_a, sizeof(l.preset_a), "%s", layered() ? layer_half_a : "");
    snprintf(l.preset_b, sizeof(l.preset_b), "%s", layered() ? layer_half_b : "");
    uint8_t p[GF_LAYER_STATE_LEN];
    gf_layer_state_pack(&l, p);
    bridge_send(GF_CMD_LAYER_STATE, p, GF_LAYER_STATE_LEN);
}

void bridge_all() {
    bridge_engine();
    bridge_filter();
    bridge_patch();
    bridge_bpm();
    bridge_master_volume();
    bridge_presets();
    bridge_preset_loaded();
    bridge_ui_state();
    bridge_layers();
    bridge_layer();
}

/// Lee las respuestas de la cabeza: cuenta ACKs y procesa gestos táctiles UI_INPUT (0x45).
void bridge_poll() {
    while (Serial1.available()) {
        if (rx_len == sizeof(rx_buf)) rx_len = 0;
        rx_buf[rx_len++] = Serial1.read();
        while (rx_len >= GF_FRAME_MIN_SIZE) {
            const size_t total = GF_FRAME_MIN_SIZE + rx_buf[1];
            if (rx_len < total) break;
            if (gf_crc8(rx_buf, total - 1) == rx_buf[total - 1]) {
                if (rx_buf[0] == GF_CMD_ACK || rx_buf[0] == GF_CMD_VERSION) {
                    acks++;
                    last_ack_ms = millis();
                } else if (rx_buf[0] == GF_CMD_UI_INPUT && rx_buf[1] >= 1) {
                    acks++;
                    last_ack_ms = millis();
                    const uint8_t acked = rx_buf[2];                 // 02 §2.1: ACK lleva el SEQ recibido
                    bridge_send(GF_CMD_ACK, &acked, 1);
                    ui_control(studio::Control(rx_buf[3]), rx_buf[1] >= 2 ? int8_t(rx_buf[4]) : 1);
                }
                memmove(rx_buf, rx_buf + total, rx_len - total);
                rx_len -= total;
            } else {
                memmove(rx_buf, rx_buf + 1, --rx_len);   // resync de a un byte
            }
        }
    }
    const bool up = last_ack_ms && millis() - last_ack_ms < 3000;
    if (up != link_up) {
        link_up = up;
        Serial.printf("[bridge] cabeza %s\n", up ? "CONECTADA (ACKs)" : "sin respuesta");
        if (up) bridge_all();
    }
}

// ── Sonido ──────────────────────────────────────────────────────────────────────────────────────

/// Cambia de engine: apaga notas, enciende los filtros del nuevo, crossfade y apaga los demás.
void activate_engine(uint8_t e) {
    all_notes_off();
    engine_dsp(e, true);
    audio_graph::set_active_engine(e);
    for (uint8_t i = 0; i < GF_ENGINE_COUNT; i++)
        if (i != e) {
            engine_dsp(i, false);
            engine_lfo(i, LfoMod{});
        }
    engine_filter(e);
    engine_patch(e);
    engine_osc(e);
    apply_sfx(e);
    ui.set_preset_count(presets.count(e));
    bridge_engine();
    bridge_filter();
    bridge_patch();
    bridge_presets();
    bridge_preset_loaded();
    Serial.printf("[ENGINE] %s\n", ENGINE_NAME[e]);
}

/// Modulación de un engine con su propio patch y su LFO (theory/lfo-modulacion.md §5).
LfoMod engine_mod(uint8_t e, const studio::Patch& p, dsp::Lfo& l, float dt) {
    l.set_shape(dsp::LfoShape(p.lfo_shape));
    l.set_rate_hz(p.lfo_rate_hz);
    l.set_sync(ui.state().flags & GF_UI_FLAG_LFO_SYNC, bpm, GF_LFO_DIV_BEATS[p.lfo_div]);
    const float v = l.tick(dt);

    // LFO DELAY del Juno (fidelidad §11.5): la profundidad sube en rampa tras una nota nueva
    float ramp = 1.0f;
    if (e == 1 && p.mod[0][0] > 1.0f) ramp = constrain((millis() - lfo_delay_t0) / p.mod[0][0], 0.0f, 1.0f);
    auto amt = [&](uint8_t d) { return gf_lfo_dest_available(e, d) ? p.lfo_amt[d] * ramp : 0.0f; };
    LfoMod m;
    m.cutoff_mul = exp2f(amt(GF_DEST_CUTOFF) * GF_LFO_CUT_OCT * v + aftertouch);
    // En el Prophet (WHEEL-MOD) y el Model D (MODULATION) la rueda hace lo del original, no un vibrato genérico
    const float wheel_vib = (e == 2 || e == 0) ? 0.0f : 0.3f * wheel;
    const float wf_st = p.sfx_on[GF_SFX_TAPE] ? sfx_l.tape.getWowFlutterMod() * 0.5f : 0.0f;
    const float vib = fminf(1.0f, amt(GF_DEST_PITCH) + wheel_vib + 0.3f * aftertouch);
    m.pitch_ratio = exp2f((gf_lfo_pitch_st(vib) * v + wf_st + bend_st) / 12.0f);
    if (e == 0) {   // DRIFT del Model D: desafinación y cutoff elegidos en cada nota
        m.pitch_ratio *= mono_drift_pitch;
        m.cutoff_mul *= mono_drift_cut;
    }
    m.pwm = amt(GF_DEST_PWM) * v;
    m.amp = 1.0f - amt(GF_DEST_AMP) * (1.0f + v) * 0.5f;
    m.lfo = v;
    return m;
}

/// LFO a 1 kHz: una evaluación por engine que suena (A y, con capa, B).
void mod_tick() {
    static uint32_t last_us = micros();
    const uint32_t now = micros();
    if (now - last_us < 1000) return;
    const float dt = (now - last_us) * 1e-6f;
    last_us = now;
    groove_tick(now);
    if (!layered()) {
        engine_lfo(ui.engine(), engine_mod(ui.engine(), patch(), lfo, dt));
        return;
    }
    engine_lfo(layer_a, engine_mod(layer_a, patches[layer_a], lfo, dt));
    engine_lfo(layer_b, engine_mod(layer_b, patches[layer_b], lfo_b, dt));
}

/// P1 (0) o P2 (1) giró: edita lo que diga la página y avisa a la cabeza.
/// Home de HYBRID SYNTH y sus presets (theory/12 §3d): P1 MORPH · P2 retraso del ataque de B · SHIFT + P2 transposición de B.
void hybrid_encoder(uint8_t enc, int32_t steps) {
    if (!layered() || enc > 1) return;
    if (enc == 0) {
        float& m = patches[layer_a].groove[GF_GROOVE_MACRO][GF_MACRO_MORPH];
        m = constrain(m + 2.0f * steps, 0.0f, 100.0f);
        const uint16_t id = gf_groove_param_id(layer_a, GF_GROOVE_MACRO, GF_MACRO_MORPH);
        bridge_param(id, m * 0.01f);
        cc_out(id, m * 0.01f);
    } else if (ui.state().shift) {
        layer_transpose = int8_t(constrain(layer_transpose + (steps > 0 ? 12 : -12), -24, 24));
        all_notes_off();                             // las notas de B que suenan quedarían en la altura vieja
    } else {
        layer_delay_ms = uint8_t(constrain(int(layer_delay_ms) + int(steps), 0, 35));
    }
    bridge_layer();
}

void page_encoder(uint8_t enc, int32_t steps) {
    if (ui.state().page == studio::HYBRID || ui.state().page == studio::LAYER_PRESETS) {
        hybrid_encoder(enc, steps);
        return;
    }
    studio::ParamChange c[1];
    if (!studio::edit_encoder(patch(), ui.state(), ui.engine(), enc, int8_t(constrain(steps, -8, 8)), c)) return;
    bridge_param(c[0].id, c[0].norm);
    cc_out(c[0].id, c[0].norm);
    engine_patch(ui.engine());
    if (ui.state().page == studio::SYNTH_FX) apply_sfx(groove_engine());
    if (ui.state().page == studio::OSC || ui.state().page == studio::LFO) engine_osc(ui.engine());   // OSC y MOD
    if (ui.state().page == studio::GROOVE && ui.state().cursor == GF_GROOVE_MACRO) {   // GRIT / POCKET
        engine_osc(ui.engine());
        engine_filter(ui.engine());
    }
    static const char* const ENC_NAME[4] = {"P1", "P2", "ENC L", "ENC R"};
    if (log_ok()) Serial.printf("[%s] param 0x%04X = %.3f\n", ENC_NAME[enc], c[0].id, c[0].norm);
}

/// Ejecuta en el sonido lo que pidió la navegación.
void run_action(studio::Result r) {
    switch (r.action) {
        case studio::Action::LoadEngine:
            if (layered()) layer_replace_engine(r.arg);   // en HYBRID cambia el engine de la capa en foco
            else activate_engine(r.arg);
            break;
        case studio::Action::HybridEnter:             // MODE → HYBRID SYNTH: entra con el último preset de capa
            if (layers.count()) layer_select(int8_t(r.arg));
            bridge_layers();
            break;
        case studio::Action::HybridExit: layer_off(); break;
        case studio::Action::LayerLoad: layer_select(int8_t(r.arg)); break;
        case studio::Action::LayerSave: layer_save(); break;
        case studio::Action::LayerCustom: layer_custom(r.arg & 0x0F, r.arg >> 4); break;
        case studio::Action::LayerFocus: layer_set_focus(r.arg); break;
        case studio::Action::FilterBypass:
            engine_filter(ui.engine());
            Serial.printf("[UI] bypass del filtro %s\n", r.arg ? "ON" : "OFF");
            break;
        case studio::Action::LfoShape: {
            studio::ParamChange c[1];
            // P1 push recorre las formas en círculo: después de S&H vuelve a SINE
            const int8_t step = patch().lfo_shape + 1 >= int(GF_LFO_SHAPES) ? int8_t(-patch().lfo_shape) : 1;
            studio::step_lfo_shape(patch(), ui.engine(), step, c);
            bridge_param(c[0].id, c[0].norm);
            Serial.printf("[UI] forma del LFO %u\n", patch().lfo_shape);
            break;
        }
        case studio::Action::SfxToggle: {
            studio::ParamChange c[1];
            if (studio::toggle_sfx(patch(), ui.engine(), r.arg, c)) bridge_param(c[0].id, c[0].norm);
            apply_sfx(ui.engine());
            Serial.printf("[UI] FX %u del synth %s\n", r.arg, patch().sfx_on[r.arg] ? "ON" : "OFF");
            break;
        }
        case studio::Action::LoadPreset: load_preset(r.arg); break;
        case studio::Action::SavePreset: save_preset(); break;
        case studio::Action::LfoSync:
            Serial.printf("[UI] LFO SYNC %s (%.1f BPM)\n", r.arg ? "ON" : "OFF", bpm);
            break;
        case studio::Action::Tap:
            gclock.tap(micros());
            bpm = gclock.bpm(micros());
            bridge_bpm();
            Serial.printf("[GROOVE] TAP · %.1f BPM\n", bpm);
            break;
        case studio::Action::GroovePush: {
            if (r.arg == GF_GROOVE_CHORD && keys_n >= 2) {   // con el acorde apretado, NAV push lo graba
                chord.learn(keys_down, keys_n);
                Serial.printf("[GROOVE] acorde grabado (%u notas)\n", keys_n);
                break;
            }
            studio::ParamChange c[1];
            if (!studio::toggle_groove(patch(), ui.engine(), r.arg, c)) break;
            bridge_param(c[0].id, c[0].norm);
            if (r.arg != GF_GROOVE_PUMP) all_notes_off();   // ARP / CHORD / UNISON cambian quién suelta cada nota
            Serial.printf("[GROOVE] parada %u %s\n", r.arg, patch().groove_on[r.arg] ? "ON" : "OFF");
            break;
        }
        case studio::Action::VolStep:
            master_vol_db = constrain(master_vol_db + GF_VOL_DB_STEP * int8_t(r.arg), GF_VOL_DB_MIN, 0.0f);
            apply_master_volume();
            bridge_master_volume();
            Serial.printf("[VOL] %.0f dB\n", master_vol_db);
            break;
        case studio::Action::None: break;
        default: Serial.printf("[UI] acción %u (arg %u) sin audio en este sketch\n", unsigned(r.action), r.arg); break;
    }
}

void ui_control(studio::Control c, int8_t v) {
    if (c == studio::Control::Nav) nav_flash_ms = millis();
    const uint8_t page_before = ui.state().page;
    run_action(ui.handle(c, v));
    const GF_UiState& u = ui.state();
    // SYNTH FX y GROOVE son de la capa entera (los de A): abrirlos pasa el foco a A
    if (layered() && layer_focus && (u.page == studio::SYNTH_FX || u.page == studio::GROOVE)) layer_set_focus(0);
    if (u.page == studio::HYBRID && page_before != studio::HYBRID) bridge_layer();
    if (u.page == studio::LAYER_PRESETS && page_before != studio::LAYER_PRESETS) bridge_layers();
    if (u.page == studio::PRESETS && page_before != studio::PRESETS) bridge_presets();   // theory/06 §5
    static const char* const MODE_NAME[3] = {"SYNTH", "FX", "HYBRID SYNTH"};
    if (log_ok()) Serial.printf("[UI] modo %s · página %u · cursor %u · shift %u · hold %u · fx %u\n", MODE_NAME[u.mode % 3],
                  u.page, u.cursor, u.shift, u.fx_hold, u.fx_slot);
}

/// Recalcula las luces y las escribe solo si algo cambió (el WS2812 bloquea interrupciones mientras se escribe).
void leds_update() {
    const GF_UiState& u = ui.state();
    uint8_t pos, count, keys[studio::KEY_LEDS];
    const uint8_t rows = u.page == studio::HYBRID          ? uint8_t(layers.count() + 1)   // + CUSTOM
                         : u.page == studio::LAYER_PRESETS ? uint8_t(layers.count() + 1)
                                                           : uint8_t(ui.preset_count() + 1);
    studio::nav_position(u, ui.engine(), rows, &pos, &count);
    studio::key_levels(u, keys);
    const uint32_t now = millis();
    const bool flash = now - nav_flash_ms < 300;
    bool animating = false;
    for (uint8_t i = 0; i < studio::KEY_LEDS; ++i) {      // destello que se apaga en KEY_FLASH_MS
        const uint32_t dt = now - key_flash_ms[i];
        if (key_flash_ms[i] && dt < KEY_FLASH_MS) {
            keys[i] = max(keys[i], uint8_t(255 - 255 * dt / KEY_FLASH_MS));
            animating = true;
        }
    }

    static uint32_t last_sig = 0xFFFFFFFF, last_show = 0;
    uint32_t sig = pos | (count << 6) | (u.mode << 12) | (flash << 14);
    for (uint8_t i = 0; i < studio::KEY_LEDS; ++i) sig = sig * 31 + keys[i];
    if (sig == last_sig || (animating && now - last_show < 16)) return;   // animación a ~60 cuadros/s
    last_sig = sig;
    last_show = now;

    // teal SYNTH · púrpura FX · cyan HYBRID SYNTH (theory/12 §3d). Cyan y no ámbar: R+G+B 290 contra 437, menos
    // corriente por LED; por el aislador USB el destello de NAV en ámbar hundía la alimentación (2026-10-27)
    const CRGB base = u.mode == studio::MODE_FX       ? CRGB(0x53, 0x4A, 0xB7)
                      : u.mode == studio::MODE_HYBRID ? CRGB(0x10, 0x7A, 0xA8)
                                                      : CRGB(0x1D, 0x9E, 0x75);
    const studio::RingArc arc = studio::ring_arc(pos, count);
    fill_solid(ring, studio::RING_LEDS, CRGB::Black);
    for (uint8_t i = 0; i < arc.len; ++i) {
        CRGB c = base;
        c.nscale8(flash ? 255 : 90);
        ring[(arc.first + i + RING_OFFSET) % studio::RING_LEDS] = c;
    }
    for (uint8_t i = 0; i < studio::KEY_LEDS; ++i) {
        CRGB c = (i == 4) ? base : CRGB(0x1D, 0x9E, 0x75);
        c.nscale8(keys[i]);
        key_led[i] = c;
    }
    FastLED.show();
}

// ── MIDI ────────────────────────────────────────────────────────────────────────────────────────

void note_off(byte ch, byte note, byte vel);

void note_on(byte ch, byte note, byte vel) {
    if (vel == 0) { note_off(ch, note, 0); return; }
    if (sustained[note >> 5] & (1u << (note & 31))) {   // sostenida por el pedal: se suelta lo que disparó antes
        sustained[note >> 5] &= ~(1u << (note & 31));
        play_off(note);
    }
    if (notes_held == 0) lfo_delay_t0 = millis();
    if (notes_held < 255) ++notes_held;
    if (keys_n < dsp::ArpCore::MAX_NOTES) keys_down[keys_n++] = note;
    play_on(note, vel);
    const uint8_t p[2] = {note, vel};
    bridge_send(GF_CMD_NOTE_ON, p, 2);
}

void note_off(byte ch, byte note, byte vel) {
    if (notes_held) --notes_held;
    for (uint8_t i = 0; i < keys_n; ++i)
        if (keys_down[i] == note) {
            keys_down[i] = keys_down[--keys_n];
            break;
        }
    if (sustain_pedal) { sustained[note >> 5] |= 1u << (note & 31); return; }
    play_off(note);
}

void pitch_bend(byte ch, int bend) { bend_st = BEND_RANGE_ST * bend / 8192.0f; }   // −8192…8191
void after_touch(byte ch, byte pressure) { aftertouch = pressure / 127.0f; }

void release_sustained() {
    for (uint8_t n = 0; n < 128; ++n)
        if (sustained[n >> 5] & (1u << (n & 31))) play_off(n);
    memset(sustained, 0, sizeof(sustained));
}

void control_change(byte ch, byte cc, byte val) {
    const uint8_t e = ui.engine();
    if (cc == 64) {
        sustain_pedal = val >= 64;
        if (!sustain_pedal) release_sustained();
        return;
    }
    if (cc == 123) { all_notes_off(); return; }
    if (cc == 18) {
        Serial.printf("[MIDI] CC 18 = %u (preset de capa)\n", val);                                  // CC 18: preset de capa desde Live (0 = sale a SYNTH, theory/12 §3d)
        const int8_t idx = int8_t(studio::layer_from_cc(val, layers.count())) - 1;
        if (idx != layer_idx) layer_select(idx);
        return;
    }
    studio::ParamChange pc[1];
    const uint8_t ce = (layered() && cc >= 14 && cc <= 17) ? layer_a : e;   // las macros (y MORPH) son de A
    if (studio::apply_cc(patches[ce], ce, cc, val, pc)) {
        bridge_param(pc[0].id, pc[0].norm);
        engine_patch(ce);
        engine_osc(ce);
        engine_filter(ce);  // GRIT (CC 15) mueve la resonancia
        apply_sfx(groove_engine());
        return;
    }
    if (cc == 74) {
        const float old = cutoff[e];
        cutoff[e] = gf_cutoff_from_cc_norm(e, val / 127.0f);
        engine_filter(e);
        layer_follow_filter(cutoff[e] / old, 0.0f);
        bridge_filter();
    }
    if (cc == 71) {
        const float old = gf_norm(RESO_RANGE[e], resonance[e]);
        resonance[e] = gf_denorm(RESO_RANGE[e], val / 127.0f);
        engine_filter(e);
        layer_follow_filter(1.0f, val / 127.0f - old);
        bridge_filter();
    }
    if (cc == 1) {
        wheel = val / 127.0f;
        prophet.setWheel(wheel);
        moog.setWheel(wheel);                       // MODULATION del Model D (fidelidad §11.2)
    }
}

// Teclado del USB host: suena en el Brain y además se reenvía al computador por el USB de la Teensy (MIDI thru),
// para grabarlo en Ableton. Lo que llega del computador (usbMIDI) solo suena: no se reenvía, así no hay lazos.
void host_note_on(byte ch, byte note, byte vel) {
    note_on(ch, note, vel);
    usbMIDI.sendNoteOn(note, vel, ch);
}

void host_note_off(byte ch, byte note, byte vel) {
    note_off(ch, note, vel);
    usbMIDI.sendNoteOff(note, vel, ch);
}

// CC del computador: si es el eco de lo que el Brain acaba de mandar (Ableton con monitoreo de MIDI, theory/07 §3),
// se ignora. Si no, aplicarlo cuantiza el valor a 7 bits y frena la perilla (el cutoff quedaba en 83 Hz).
void usb_control_change(byte ch, byte cc, byte val) {
    if (cc < 128 && cc_out_ms[cc] && millis() - cc_out_ms[cc] < 150) return;
    control_change(ch, cc, val);
}

void host_control_change(byte ch, byte cc, byte val) {
    control_change(ch, cc, val);
    usbMIDI.sendControlChange(cc, val, ch);
}

void host_pitch_bend(byte ch, int bend) {
    pitch_bend(ch, bend);
    usbMIDI.sendPitchBend(bend, ch);
}

void host_after_touch(byte ch, byte pressure) {
    after_touch(ch, pressure);
    usbMIDI.sendAfterTouch(pressure, ch);
}

/// Transporte MIDI (24 ppq) → reloj común: BPM y posición para LFO SYNC, PUMP y ARP.
void clock_tick() { gclock.midi_clock(micros()); }
void clock_start() { gclock.midi_start(); }
void clock_continue() { gclock.midi_continue(); }
void clock_stop() { gclock.midi_stop(); }
void clock_song_position(uint16_t sixteenths) { gclock.midi_song_position(sixteenths); }

// ── Arduino ─────────────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 1500) {
    }
    Serial.println("\n=== sketch49 — Studio Brain: engines reales + panel + cabeza Pi ===");
    if (CrashReport) Serial.print(CrashReport);

    const bool codec_ok = audio_graph::init(0.5f);   // AudioMemory + codec + mixers (Moog activo)
    g_codec.dacVolumeRamp();             // cambios de VOL sin zipper
    apply_master_volume();
    Serial.printf("[codec] SGTL5000: %s\n", codec_ok ? "OK" : "FALLA (revisar I2C 18/19 y 3.3 V)");
    moog.begin();
    juno.begin();
    prophet.begin();
    ob6.begin();
    dx7.begin();
    arp.begin();

    // Cadena de FX: engines → BIT → TAPE → CHORUS → MODAL → master ch1; el camino directo (ch0) queda mudo
    sfx_l.begin(g_engine_bus, g_master_mix);
    sfx_r.begin(g_engine_bus_r, g_master_mix_r);
    audio_graph::set_master_gain(0, 0.0f);   // el camino directo queda mudo: todo pasa por las cadenas
    audio_graph::set_master_gain(1, 1.0f);
    apply_sfx(0);

    // PWM solo se oye en ondas de pulso (como en los originales): patch_init deja un oscilador en PULSE en el
    // Juno, el Prophet, el OB-6 y el ARP para que el LFO → PWM se escuche.
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; e++) {
        studio::patch_init(patches[e], e);
        engine_osc(e);
        cutoff[e] = CUTOFF_DEFAULT[e];
        resonance[e] = RESO_RANGE[e].lo + 0.15f * (RESO_RANGE[e].hi - RESO_RANGE[e].lo);
        engine_filter(e);
        engine_patch(e);
        if (e) engine_dsp(e, false);
    }

    // Presets: banco de fábrica en el primer arranque; cada engine arranca con su preset 1 (INIT / E.PIANO)
    preset_fs_ok = preset_fs_mount();
    Serial.printf("[PRESET] LittleFS %s\n", preset_fs_ok ? "OK (2 MB)" : "NO DISPONIBLE: presets solo de fábrica");
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; e++) {
        presets.read_bank(e);   // solo lee: los de fábrica que falten se escriben después, desde el loop
        studio::Preset p;
        if (presets.load(e, 0, &p)) {
            preset_loaded[e] = 0;
            apply_preset(e, p);
        }
    }
    ui.set_preset_count(presets.count(0));
    layers.load();                                   // sin archivo: las 8 capas de fábrica (una escritura)
    ui.set_layer_count(layers.count());
    Serial.printf("[CAPA] %u presets de capa\n", layers.count());
    Serial.printf("[PRESET] bancos: %u %u %u %u %u %u · LittleFS usado %lu de %lu bytes\n", presets.count(0),
                  presets.count(1), presets.count(2), presets.count(3), presets.count(4), presets.count(5),
                  (unsigned long)preset_fs.usedSize(), (unsigned long)preset_fs.totalSize());

    Serial.println("[test] La 440 Hz durante 0.6 s (Moog) por LINE OUT");
    moog.noteOn(69, 1.0f);
    for (int i = 0; i < 12; i++) { delay(50); moog.update(); }
    moog.noteOff();
    Serial.printf("[test] cpu=%.1f%% (>0 = interrupción de audio corriendo)\n", AudioProcessorUsageMax());

    for (int i = 0; i < 8; i++) btn[i].attach(BTN_PINS[i], INPUT_PULLUP), btn[i].interval(5);
    for (int i = 0; i < 2; i++) enc_sw[i].attach(ENC_SW_PINS[i], INPUT_PULLUP), enc_sw[i].interval(5);

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, studio::PANEL_LEDS);
    FastLED.setBrightness(80);
    // Tope de consumo de los 18 LEDs (theory/02 §7 y §10J F3): el presupuesto de la fuente USB-C es de 3 A.
    // Es un presupuesto de software, no una protección: la primera validación de alimentación se hace con los LEDs
    // apagados (build_flags = -DSTUDIO_LEDS_OFF, theory/02 §10K D01) y se encienden después de medir.
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 300);
#ifdef STUDIO_LEDS_OFF
    FastLED.setBrightness(0);
    Serial.println("[LED] apagados (STUDIO_LEDS_OFF): validación de alimentación");
#endif

    Serial1.begin(921600);                  // bridge con la Pi: TX1 (1) → GPIO15, RX1 (0) ← GPIO14
    usb_host.begin();
    midi.setHandleNoteOn(host_note_on);
    midi.setHandleNoteOff(host_note_off);
    midi.setHandleControlChange(host_control_change);
    midi.setHandlePitchChange(host_pitch_bend);
    midi.setHandleAfterTouchChannel(host_after_touch);
    midi.setHandleClock(clock_tick);
    midi.setHandleStart(clock_start);
    midi.setHandleContinue(clock_continue);
    midi.setHandleStop(clock_stop);
    midi.setHandleSongPosition(clock_song_position);
    // MIDI del computador por el USB de la Teensy (piano roll de Ableton, clock de la sesión)
    usbMIDI.setHandleNoteOn(note_on);
    usbMIDI.setHandleNoteOff(note_off);
    usbMIDI.setHandleControlChange(usb_control_change);
    usbMIDI.setHandlePitchChange(pitch_bend);
    usbMIDI.setHandleAfterTouchChannel(after_touch);
    usbMIDI.setHandleClock(clock_tick);
    usbMIDI.setHandleStart(clock_start);
    usbMIDI.setHandleContinue(clock_continue);
    usbMIDI.setHandleStop(clock_stop);
    usbMIDI.setHandleSongPosition(clock_song_position);
    arpeggio.set_callbacks(arp_note_on, arp_note_off, nullptr);
    Serial.println("[host] USB host iniciado: conectar el teclado MIDI");
}

// ── Bibliotecario de presets por el USB serie (variants/studio/theory/11) ──────────────────────────────────

/// Después de cambiar un banco desde la Mac: si es el del engine activo, la cabeza recibe la lista nueva.
void librarian_bank_changed(uint8_t e) {
    if (e != ui.engine()) return;
    ui.set_preset_count(presets.count(e));
    if (preset_loaded[e] != 0xFF && preset_loaded[e] >= presets.count(e)) preset_loaded[e] = 0xFF;
    bridge_presets();
    bridge_preset_loaded();
}

void librarian_command(char* line) {
    static char hex[2 * studio::LAYER_BYTES + 1];      // el preset de capa es el binario más largo
    static uint8_t bin[studio::LAYER_BYTES];
    const studio::LinkCmd c = studio::parse_link_cmd(line);
    switch (c.kind) {
        case studio::LinkCmd::Hello:
            Serial.printf("@GF HELLO %u %u %u %u %u\n", studio::PRESET_FORMAT, GF_ENGINE_COUNT, studio::PRESET_MAX,
                          studio::LAYER_FORMAT, studio::LAYER_MAX);
            break;
        case studio::LinkCmd::Export: {
            unsigned total = 0;
            for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e) total += presets.count(e);
            Serial.printf("@GF BEGIN %u\n", total);
            for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e)
                for (uint8_t i = 0; i < presets.count(e); ++i) {
                    studio::Preset p;   // se lee migrado y sale siempre en el formato actual
                    if (!presets.load(e, i, &p) || !studio::preset_encode(p, e, bin, studio::PRESET_BYTES)) continue;
                    studio::hex_encode(bin, studio::PRESET_BYTES, hex);
                    Serial.printf("@GF P %u %u %s\n", e, i, hex);
                }
            Serial.println("@GF END");
            break;
        }
        case studio::LinkCmd::Put: {
            const size_t n = studio::hex_decode(c.hex, bin, studio::PRESET_BYTES);
            const studio::PutResult r = n ? presets.put(c.engine, c.index, bin, n) : studio::PutResult::Format;
            static const char* const ERR[] = {"", "FORMATO", "ENGINE", "INDICE", "ESCRITURA"};
            if (r == studio::PutResult::Ok) {
                Serial.printf("@GF OK PUT %u %u\n", c.engine, c.index);
                librarian_bank_changed(c.engine);
            } else {
                Serial.printf("@GF ERR %s\n", ERR[uint8_t(r)]);
            }
            break;
        }
        case studio::LinkCmd::Trunc:
            if (c.engine < GF_ENGINE_COUNT && presets.truncate(c.engine, c.index)) {
                Serial.printf("@GF OK TRUNC %u %u\n", c.engine, c.index);
                librarian_bank_changed(c.engine);
            } else {
                Serial.println("@GF ERR ESCRITURA");
            }
            break;
        case studio::LinkCmd::LayerExport:
            Serial.printf("@GF BEGIN %u\n", layers.count());
            for (uint8_t i = 0; i < layers.count(); ++i) {
                const size_t n = layers.raw(i, bin, sizeof(bin));
                if (!n) continue;
                studio::hex_encode(bin, n, hex);
                Serial.printf("@GF L %u %s\n", i, hex);
            }
            Serial.println("@GF END");
            break;
        case studio::LinkCmd::LayerPut: {
            const size_t n = studio::hex_decode(c.hex, bin, sizeof(bin));
            const studio::PutResult r = n ? layers.put(c.index, bin, n) : studio::PutResult::Format;
            static const char* const ERR[] = {"", "FORMATO", "ENGINE", "INDICE", "ESCRITURA"};
            if (r == studio::PutResult::Ok) {
                layers.flush();
                ui.set_layer_count(layers.count());
                bridge_layers();
                Serial.printf("@GF OK LPUT %u\n", c.index);
            } else {
                Serial.printf("@GF ERR %s\n", ERR[uint8_t(r)]);
            }
            break;
        }
        case studio::LinkCmd::LayerTrunc:
            if (layers.truncate(c.index)) {
                layers.flush();
                ui.set_layer_count(layers.count());
                bridge_layers();
                Serial.printf("@GF OK LTRUNC %u\n", c.index);
            } else {
                Serial.println("@GF ERR ESCRITURA");
            }
            break;
        case studio::LinkCmd::Fs:   // diagnóstico: espacio del flash de presets y tamaño de cada banco
            Serial.printf("@GF FS %lu %lu %u %u %u %u %u %u\n", (unsigned long)preset_fs.usedSize(),
                          (unsigned long)preset_fs.totalSize(), presets.count(0), presets.count(1), presets.count(2),
                          presets.count(3), presets.count(4), presets.count(5));
            break;
        case studio::LinkCmd::Format:   // borra el área de presets y vuelve a escribir los de fábrica (pide "YES")
            if (preset_fs_ok && preset_fs.quickFormat()) {
                preset_store.reset();               // el banco en RAM ya no existe en el flash
                all_notes_off();
                for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e) {
                    presets.read_bank(e);           // vacío: los de fábrica se escriben desde el loop
                    preset_loaded[e] = 0xFF;
                }
                librarian_bank_changed(ui.engine());
                layers.reset();                     // vuelven las capas de fábrica
                ui.set_layer_count(layers.count());
                bridge_layers();
                Serial.println("@GF OK FORMAT");
            } else {
                Serial.println("@GF ERR ESCRITURA");
            }
            break;
        case studio::LinkCmd::None: break;
    }
}

/// Lee líneas del USB serie sin bloquear; una línea demasiado larga se descarta entera.
void librarian_poll() {
    static char line[2 * studio::LAYER_BYTES + 64];
    static size_t len = 0;
    static bool overflow = false;
    while (Serial.available()) {
        const char ch = char(Serial.read());
        if (ch == '\n') {
            line[len] = '\0';
            if (!overflow) librarian_command(line);
            len = 0;
            overflow = false;
        } else if (len + 1 < sizeof(line)) {
            line[len++] = ch;
        } else {
            overflow = true;
        }
    }
}

/// Presets de fábrica que faltan: uno cada 25 ms desde el loop, para que el arranque (audio, USB, cabeza) no espere al
/// flash. Si un banco estaba vacío, su primer preset se carga apenas existe (como antes en el arranque).
void preset_seed_tick() {
    static uint32_t last = 0;
    if (millis() - last < 25) return;
    last = millis();
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e) {
        if (!presets.seeding(e)) continue;
        if (presets.seed_one(e)) {
            if (preset_loaded[e] == 0xFF && presets.count(e) == 1) {
                studio::Preset p;
                if (presets.load(e, 0, &p)) {
                    preset_loaded[e] = 0;
                    apply_preset(e, p);
                }
            }
            if (e == ui.engine()) {
                ui.set_preset_count(presets.count(e));
                if (!presets.seeding(e)) bridge_presets();   // la lista completa a la cabeza al terminar el banco
            }
        }
        return;   // un engine por vez
    }
}

void loop() {
    usb_host.Task();
    librarian_poll();
    preset_seed_tick();
    midi.read();
    while (usbMIDI.read()) {
    }
    bridge_poll();
    if (millis() - last_hb >= 1000) { last_hb = millis(); bridge_send(GF_CMD_HEARTBEAT, nullptr, 0); }
    if (millis() - last_engine_tx >= 5000) bridge_engine();
    // BPM del reloj común a la cabeza: 1 vez por segundo y al cambiar (como mucho cada 100 ms)
    if (millis() - last_bpm_tx >= 1000 || (fabsf(bpm - bpm_sent) >= 0.5f && millis() - last_bpm_tx >= 100)) bridge_bpm();

    mod_tick();
    {
        static uint32_t last_fx_ms = millis();
        const uint32_t now = millis();
        sfx_l.update(float(now - last_fx_ms));
        sfx_r.update(float(now - last_fx_ms));
        last_fx_ms = now;
    }
    moog.update();                          // solo el activo suena, pero todos avanzan sus envelopes
    juno.update();
    prophet.update();
    ob6.update();
    dx7.update();
    arp.update();

    if (bool(midi) != host_seen) {
        host_seen = bool(midi);
        Serial.printf("[host] teclado MIDI %s\n", host_seen ? "conectado" : "desconectado");
    }

    const uint8_t e = ui.engine();
    // Detents con histéresis (ui/detent.h): el vaivén de ±1 pulso del contacto no mueve el valor
    const int32_t l = det_l.update(enc_l.read()), r = det_r.update(enc_r.read()), n = det_nav.update(enc_nav.read());
    const int32_t p1 = det_p1.update(enc_p1.read()), p2 = det_p2.update(enc_p2.read());
    // Home (vistas) y modo FX: ENC L / R = filtro. Páginas de edición: ENC L / R = 3.er y 4.º control de la página.
    const uint8_t pg = ui.state().page;
    const bool enc_lr_filter = ui.state().mode == studio::MODE_FX || pg == studio::PERFORM || pg == studio::AI || pg == studio::SPECTRUM ||
                               pg == studio::PANEL || pg == studio::HYBRID || pg == studio::LAYER_PRESETS ||
                               pg == studio::ENGINE || pg == studio::PRESETS;   // páginas de selección: sin controles propios
    if (l != last_l && !enc_lr_filter) { page_encoder(2, l - last_l); last_l = l; }
    if (r != last_r && !enc_lr_filter) { page_encoder(3, r - last_r); last_r = r; }
    if (l != last_l) {
        const GF_Range cr = GF_CUTOFF_RANGE[e];
        // Aceleración: detents seguidos (< 40 ms) en el mismo sentido avanzan 4×, < 90 ms 2×; al cambiar de sentido
        // no acelera (un pulso de rebote en contra no salta 26 %). Despacio sigue siendo fino (6 % por detent).
        static uint32_t last_cut_ms = 0;
        static int last_cut_dir = 0;
        const int dir = l > last_l ? 1 : -1;
        const uint32_t dt_cut = millis() - last_cut_ms;
        last_cut_ms = millis();
        const float accel = dir != last_cut_dir ? 1.0f : dt_cut < 40 ? 4.0f : dt_cut < 90 ? 2.0f : 1.0f;
        last_cut_dir = dir;
        const float old_cut = cutoff[e];
        cutoff[e] = constrain(cutoff[e] * powf(1.06f, accel * float(l - last_l)), cr.lo, cr.hi);
        engine_filter(e);
        layer_follow_filter(cutoff[e] / old_cut, 0.0f);
        bridge_filter();
        cc_out(gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_CUTOFF), gf_norm(cr, cutoff[e]));
        last_l = l;
    }
    if (r != last_r) {
        const GF_Range rr = RESO_RANGE[e];
        const float old_res = gf_norm(rr, resonance[e]);
        resonance[e] = constrain(resonance[e] + 0.05f * (rr.hi - rr.lo) * (r - last_r), rr.lo, rr.hi);
        engine_filter(e);
        layer_follow_filter(1.0f, gf_norm(rr, resonance[e]) - old_res);
        bridge_filter();
        cc_out(gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_RESO), gf_norm(rr, resonance[e]));
        last_r = r;
    }
    if (p1 != last_p1) { page_encoder(0, p1 - last_p1); last_p1 = p1; }
    if (p2 != last_p2) { page_encoder(1, p2 - last_p2); last_p2 = p2; }
    if (n != last_nav) {
        ui_control(studio::Control::Nav, int8_t(constrain(n - last_nav, -8, 8)));
        last_nav = n;
    }
    using studio::Control;
    static const Control PRESS[8] = {Control::Key1, Control::Key2, Control::Key3, Control::Key4, Control::Mode,
                                     Control::B6Down, Control::NavPush, Control::EncLPush};
    static uint32_t nav_down_ms = 0;
    static bool nav_long_done = false;
    for (int i = 0; i < 8; i++) {
        btn[i].update();
        if (i == 6) {                       // NAV push: corto al soltar, largo (≥ 500 ms) = home
            if (btn[i].fell()) { nav_down_ms = millis(); nav_long_done = false; }
            if (!btn[i].read() && nav_down_ms && !nav_long_done && millis() - nav_down_ms >= 500) {
                nav_long_done = true;
                ui_control(Control::NavLong);
            }
            if (btn[i].rose()) {
                if (!nav_long_done && nav_down_ms) ui_control(Control::NavPush);
                nav_down_ms = 0;
            }
            continue;
        }
        if (btn[i].fell()) {
            if (i < int(studio::KEY_LEDS)) key_flash_ms[i] = millis();   // B1–B4, B5, B6: su LED destella
            ui_control(PRESS[i]);
        }
        if (i == 5 && btn[i].rose()) ui_control(Control::B6Up);   // SHIFT / HOLD momentáneos
    }
    for (int i = 0; i < 2; i++) {
        enc_sw[i].update();
        if (enc_sw[i].fell()) ui_control(i ? Control::P2Push : Control::P1Push);
    }
    leds_update();
    // UI_STATE: en cada cambio (como máximo cada 20 ms, coalescido) y 1 Hz para converger (theory/04 §5.1)
    if ((millis() - last_ui_tx >= 20 && ui.take_dirty()) || millis() - last_ui_beat >= 1000) bridge_ui_state();

    if (millis() - last_stats > 5000) {
        last_stats = millis();
        // máx = pico de CPU del audio en estos 5 s (se reinicia): un pico cerca de 100 % pierde un bloque = click
        Serial.printf("-- %s · cpu %.1f%% (máx 5 s %.1f%%) · memoria de audio %d (máx %d) · teclado %s · cabeza %s (%lu ACKs)\n",
                      ENGINE_NAME[e], AudioProcessorUsage(), AudioProcessorUsageMax(), AudioMemoryUsage(),
                      AudioMemoryUsageMax(), host_seen ? "sí" : "no", link_up ? "sí" : "no", (unsigned long)acks);
        AudioProcessorUsageMaxReset();
    }
}
