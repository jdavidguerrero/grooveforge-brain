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
#include "../ui/page_params.h"
#include "../ui/preset_bank.h"
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
/// Una cadena de SYNTH FX (BIT → TAPE → CHORUS → MODAL). Hay una por lado para no perder el estéreo del Juno.
struct SfxChain {
    AudioOutputStage out;                    // acople AC + saturación de salida (fidelidad §11.6)
    BitSculpt bit;
    TapeSaturate tape;
    PhaseChorus chorus;
    ModalReverb modal;

    void begin(AudioStream& in, AudioMixer4& master) {
        new AudioConnection(in, 0, out, 0);
        bit.begin(out);
        tape.begin(bit.getOutputL());
        chorus.begin(tape.getOutputL());
        modal.begin(chorus.getOutputL());
        new AudioConnection(modal.getOutputL(), 0, master, 1);
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
float bpm = 120.0f;                      // MIDI clock del USB host; sin clock, 120
uint32_t last_clock_us = 0, last_bpm_tx = 0;
float clock_us_avg = 0.0f;
uint8_t moog_note = 255;                 // el Model D es monofónico: noteOff() sin nota

int32_t last_l = 0, last_r = 0, last_nav = 0, last_p1 = 0, last_p2 = 0;
uint32_t last_stats = 0, last_ui_tx = 0, last_ui_beat = 0;
bool host_seen = false;

studio::Patch& patch() { return patches[ui.engine()]; }

// ── Presets (variants/studio/theory/06): un archivo por preset en LittleFS sobre el flash del programa ──────────

LittleFS_Program preset_fs;
bool preset_fs_ok = false;

class FlashPresetStorage : public studio::PresetStorage {
public:
    size_t read(uint8_t e, uint8_t i, uint8_t* buf, size_t cap) override {
        if (!preset_fs_ok) return 0;
        char path[32];
        path_of(e, i, path);
        File f = preset_fs.open(path, FILE_READ);
        if (!f) return 0;
        const size_t n = f.read(buf, cap);
        f.close();
        return n;
    }
    bool write(uint8_t e, uint8_t i, const uint8_t* buf, size_t len) override {
        if (!preset_fs_ok) return false;
        char dir[16], path[32];
        snprintf(dir, sizeof(dir), "/presets/%u", e);
        preset_fs.mkdir("/presets");
        preset_fs.mkdir(dir);
        path_of(e, i, path);
        preset_fs.remove(path);                   // FILE_WRITE agrega al final: se reescribe desde cero
        File f = preset_fs.open(path, FILE_WRITE);
        if (!f) return false;
        const size_t n = f.write(buf, len);
        f.close();
        return n == len;
    }

private:
    static void path_of(uint8_t e, uint8_t i, char* out) { snprintf(out, 32, "/presets/%u/%02u.gfp", e, i); }
};

FlashPresetStorage preset_store;
studio::PresetBank presets(preset_store);
uint8_t preset_loaded[GF_ENGINE_COUNT] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ── Engines ─────────────────────────────────────────────────────────────────────────────────────

void engine_note_on(uint8_t e, uint8_t note, float vel) {
    switch (e) {
        // El Model D y el Juno-106 originales no tienen velocity (fidelidad §11.6): suenan siempre a fondo
        case 0: moog.noteOn(note, 1.0f); moog_note = note; break;
        case 1: juno.noteOn(note, 1.0f); break;
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
    switch (e) {
        case 0: moog.setFilterCutoff(hz); moog.setFilterResonance(resonance[e]); break;
        case 1: juno.setFilterCutoff(hz); juno.setFilterResonance(resonance[e]); break;
        case 2: prophet.setFilterCutoff(hz); prophet.setFilterResonance(resonance[e]); break;
        case 3: ob6.setFilterCutoff(hz); ob6.setFilterResonance(resonance[e]); break;
        case 4: dx7.setFilterCutoff(hz); dx7.setFilterResonance(resonance[e]); break;
        case 5: arp.setFilterCutoff(hz); arp.setFilterResonance(resonance[e]); break;
    }
}

/// Envelopes del amplificador y del filtro (la página ENV elige cuál con NAV), FLT ENV AMT, GLIDE y DRIVE del patch.
void engine_patch(uint8_t e) {
    const studio::Patch& p = patches[e];
    const float glide_ms = p.glide * p.glide * 1500.0f;
    const float drv = 0.5f + 3.5f * p.drive;
    switch (e) {
        case 0:
            moog.setVCAAttack(p.attack_ms); moog.setVCADecay(p.decay_ms);
            moog.setVCASustain(p.sustain); moog.setVCARelease(p.release_ms);
            moog.setFilterAttack(p.f_attack_ms); moog.setFilterDecay(p.f_decay_ms);
            moog.setFilterSustain(p.f_sustain); moog.setFilterRelease(p.f_release_ms);
            moog.setFilterEnvAmount(p.env_amt);
            moog.setGlide(glide_ms);
            moog.setDrive(drv);
            break;
#define POLY_PATCH(eng)                                                                                  \
    eng.setAmpEnv(p.attack_ms, p.decay_ms, p.sustain, p.release_ms);                                  \
    eng.setFilterEnv(p.f_attack_ms, p.f_decay_ms, p.f_sustain, p.f_release_ms);                       \
    eng.setFilterEnvAmount(p.env_amt);                                                                \
    eng.setGlide(glide_ms);                                                                           \
    eng.setDrive(drv);
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
    static const uint16_t W4[4] = {WAVEFORM_SAWTOOTH, WAVEFORM_TRIANGLE, WAVEFORM_SQUARE, WAVEFORM_PULSE};
    static const uint16_t W_JUNO[4] = {WAVEFORM_SAWTOOTH, WAVEFORM_SQUARE, WAVEFORM_PULSE, AudioSynthPolyBlep::WAVEFORM_SAW_PULSE};
    static const uint16_t W_ARP[5] = {WAVEFORM_SAWTOOTH, WAVEFORM_TRIANGLE, WAVEFORM_SQUARE, WAVEFORM_PULSE,
                                      WAVEFORM_SINE};
    const float (&o)[GF_OSC_STOPS][GF_OSC_SLOTS] = patches[e].osc;
    auto w = [&](const uint16_t* t, uint8_t n, float v) { return t[constrain(int(v), 0, n - 1)]; };
    switch (e) {
        case 0:
            for (uint8_t i = 0; i < 3; i++) moog.setWaveform(i, w(W4, 4, o[i][0]));
            moog.setDetune(o[1][1]);
            moog.setVco3Interval(o[2][1]);
            moog.setOscLevels(o[0][2] / 100.0f, o[1][2] / 100.0f, o[2][2] / 100.0f, o[3][0] / 100.0f);   // NOISE: 4.ª parada
            moog.setNoisePink(o[3][1] > 0.5f);
            moog.setPulseWidth(o[0][4] / 100.0f);
            moog.setVco3Detune(o[2][5]);
            {   // MODULATION: MOD MIX · OSC MOD · FILTER MOD · NOISE | VCO3 KBD · VCO3 LO · OVERDRIVE · FILTER KBD
                const float (&md)[GF_OSC_SLOTS] = patches[0].mod[0];
                moog.setModulation(md[0] / 100.0f, md[1] > 0.5f, md[2] > 0.5f);
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
            break;
        case 2: {  // theory/prophet-5-completo.md §3.1: OSC A · OSC B · MIXER
            const studio::Patch& p = patches[2];
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
            const float (&xm)[GF_OSC_SLOTS] = patches[3].mod[0];
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
                const float (&pa)[GF_OSC_SLOTS] = patches[5].mod[0];
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
                const studio::Patch& p = patches[4];
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
    patches[e] = p.patch;
    cutoff[e] = constrain(p.cutoff_hz, GF_CUTOFF_RANGE[e].lo, GF_CUTOFF_RANGE[e].hi);
    resonance[e] = gf_denorm(RESO_RANGE[e], p.reso_norm);
    engine_osc(e);
    engine_filter(e);
    engine_patch(e);
    if (e != ui.engine()) return;
    apply_sfx(e);
    if (!link_up) return;                           // al conectar, bridge_all() manda todo
    bridge_filter();
    bridge_patch();
}

void load_preset(uint8_t i) {
    const uint8_t e = ui.engine();
    studio::Preset p;
    if (!presets.load(e, i, &p)) {
        Serial.printf("[PRESET] %u no se pudo leer\n", i);
        return;
    }
    preset_loaded[e] = i;
    apply_preset(e, p);
    bridge_preset_loaded();
    Serial.printf("[PRESET] cargado %u %s\n", i, p.name);
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
    const uint8_t p[3] = {uint8_t(x10 & 0xFF), uint8_t(x10 >> 8), 0};
    bridge_send(GF_CMD_BEAT_DETECTED, p, 3);
    last_bpm_tx = millis();
}

void bridge_ui_state() {
    uint8_t p[GF_UI_STATE_LEN];
    gf_ui_state_pack(&ui.state(), p);
    bridge_send(GF_CMD_UI_STATE, p, GF_UI_STATE_LEN);
    last_ui_tx = last_ui_beat = millis();
}

void bridge_all() {
    bridge_engine();
    bridge_filter();
    bridge_patch();
    bridge_bpm();
    bridge_presets();
    bridge_preset_loaded();
    bridge_ui_state();
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

/// LFO a 1 kHz: una evaluación y las cantidades del engine activo (theory/lfo-modulacion.md §5).
void mod_tick() {
    static uint32_t last_us = micros();
    const uint32_t now = micros();
    if (now - last_us < 1000) return;
    const float dt = (now - last_us) * 1e-6f;
    last_us = now;

    const studio::Patch& p = patch();
    const uint8_t e = ui.engine();
    lfo.set_shape(dsp::LfoShape(p.lfo_shape));
    lfo.set_rate_hz(p.lfo_rate_hz);
    lfo.set_sync(ui.state().flags & GF_UI_FLAG_LFO_SYNC, bpm, GF_LFO_DIV_BEATS[p.lfo_div]);
    const float v = lfo.tick(dt);

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
    m.pwm = amt(GF_DEST_PWM) * v;
    m.amp = 1.0f - amt(GF_DEST_AMP) * (1.0f + v) * 0.5f;
    m.lfo = v;
    engine_lfo(e, m);
}

/// P1 (0) o P2 (1) giró: edita lo que diga la página y avisa a la cabeza.
void page_encoder(uint8_t enc, int32_t steps) {
    studio::ParamChange c[1];
    if (!studio::edit_encoder(patch(), ui.state(), ui.engine(), enc, int8_t(constrain(steps, -8, 8)), c)) return;
    bridge_param(c[0].id, c[0].norm);
    cc_out(c[0].id, c[0].norm);
    engine_patch(ui.engine());
    if (ui.state().page == studio::SYNTH_FX) apply_sfx(ui.engine());
    if (ui.state().page == studio::OSC || ui.state().page == studio::LFO) engine_osc(ui.engine());   // OSC y MOD
    static const char* const ENC_NAME[4] = {"P1", "P2", "ENC L", "ENC R"};
    Serial.printf("[%s] param 0x%04X = %.3f\n", ENC_NAME[enc], c[0].id, c[0].norm);
}

/// Ejecuta en el sonido lo que pidió la navegación.
void run_action(studio::Result r) {
    switch (r.action) {
        case studio::Action::LoadEngine: activate_engine(r.arg); break;
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
        case studio::Action::None: break;
        default: Serial.printf("[UI] acción %u (arg %u) sin audio en este sketch\n", unsigned(r.action), r.arg); break;
    }
}

void ui_control(studio::Control c, int8_t v) {
    if (c == studio::Control::Nav) nav_flash_ms = millis();
    const uint8_t page_before = ui.state().page;
    run_action(ui.handle(c, v));
    const GF_UiState& u = ui.state();
    if (u.page == studio::PRESETS && page_before != studio::PRESETS) bridge_presets();   // theory/06 §5
    Serial.printf("[UI] modo %s · página %u · cursor %u · shift %u · hold %u · fx %u\n", u.mode ? "FX" : "SYNTH",
                  u.page, u.cursor, u.shift, u.fx_hold, u.fx_slot);
}

/// Recalcula las luces y las escribe solo si algo cambió (el WS2812 bloquea interrupciones mientras se escribe).
void leds_update() {
    const GF_UiState& u = ui.state();
    uint8_t pos, count, keys[studio::KEY_LEDS];
    studio::nav_position(u, ui.engine(), uint8_t(ui.preset_count() + 1), &pos, &count);
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
    uint32_t sig = pos | (count << 5) | (u.mode << 10) | (flash << 11);
    for (uint8_t i = 0; i < studio::KEY_LEDS; ++i) sig = sig * 31 + keys[i];
    if (sig == last_sig || (animating && now - last_show < 16)) return;   // animación a ~60 cuadros/s
    last_sig = sig;
    last_show = now;

    const CRGB base = u.mode ? CRGB(0x53, 0x4A, 0xB7) : CRGB(0x1D, 0x9E, 0x75);   // púrpura FX · teal SYNTH
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
    sustained[note >> 5] &= ~(1u << (note & 31));
    if (notes_held == 0) lfo_delay_t0 = millis();
    if (notes_held < 255) ++notes_held;
    engine_note_on(ui.engine(), note, vel / 127.0f);
    const uint8_t p[2] = {note, vel};
    bridge_send(GF_CMD_NOTE_ON, p, 2);
}

void note_off(byte ch, byte note, byte vel) {
    if (notes_held) --notes_held;
    if (sustain_pedal) { sustained[note >> 5] |= 1u << (note & 31); return; }
    engine_note_off(ui.engine(), note);
}

void pitch_bend(byte ch, int bend) { bend_st = BEND_RANGE_ST * bend / 8192.0f; }   // −8192…8191
void after_touch(byte ch, byte pressure) { aftertouch = pressure / 127.0f; }

void release_sustained() {
    for (uint8_t n = 0; n < 128; ++n)
        if (sustained[n >> 5] & (1u << (n & 31))) engine_note_off(ui.engine(), n);
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
    studio::ParamChange pc[1];
    if (studio::apply_cc(patch(), e, cc, val, pc)) {
        bridge_param(pc[0].id, pc[0].norm);
        engine_patch(e);
        engine_osc(e);
        apply_sfx(e);
        return;
    }
    if (cc == 74) { cutoff[e] = gf_cutoff_from_cc_norm(e, val / 127.0f); engine_filter(e); bridge_filter(); }
    if (cc == 71) { resonance[e] = gf_denorm(RESO_RANGE[e], val / 127.0f); engine_filter(e); bridge_filter(); }
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

/// MIDI clock (24 ppq): promedio móvil del intervalo → BPM para el SYNC del LFO.
void clock_tick() {
    const uint32_t now = micros();
    const uint32_t d = now - last_clock_us;
    last_clock_us = now;
    if (d > 100000) return;                               // primer pulso o reanudación: no promediar
    clock_us_avg = clock_us_avg == 0.0f ? d : clock_us_avg + 0.05f * (d - clock_us_avg);
    bpm = constrain(60e6f / (clock_us_avg * 24.0f), 30.0f, 300.0f);
}

// ── Arduino ─────────────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 1500) {
    }
    Serial.println("\n=== sketch49 — Studio Brain: engines reales + panel + cabeza Pi ===");
    if (CrashReport) Serial.print(CrashReport);

    const bool codec_ok = audio_graph::init(0.5f);   // AudioMemory + codec + mixers (Moog activo)
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
    preset_fs_ok = preset_fs.begin(512 * 1024);
    Serial.printf("[PRESET] LittleFS %s\n", preset_fs_ok ? "OK (512 KB)" : "NO DISPONIBLE: presets solo de fábrica");
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; e++) {
        presets.scan(e);
        studio::Preset p;
        if (presets.load(e, 0, &p)) {
            preset_loaded[e] = 0;
            apply_preset(e, p);
        }
    }
    ui.set_preset_count(presets.count(0));

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
    // MIDI del computador por el USB de la Teensy (piano roll de Ableton, clock de la sesión)
    usbMIDI.setHandleNoteOn(note_on);
    usbMIDI.setHandleNoteOff(note_off);
    usbMIDI.setHandleControlChange(usb_control_change);
    usbMIDI.setHandlePitchChange(pitch_bend);
    usbMIDI.setHandleAfterTouchChannel(after_touch);
    usbMIDI.setHandleClock(clock_tick);
    Serial.println("[host] USB host iniciado: conectar el teclado MIDI");
}

void loop() {
    usb_host.Task();
    midi.read();
    while (usbMIDI.read()) {
    }
    bridge_poll();
    if (millis() - last_hb >= 1000) { last_hb = millis(); bridge_send(GF_CMD_HEARTBEAT, nullptr, 0); }
    if (millis() - last_engine_tx >= 5000) bridge_engine();
    if (last_clock_us && millis() - last_bpm_tx >= 1000) bridge_bpm();   // con clock, el BPM 1 vez por segundo

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
    const bool enc_lr_filter = ui.state().mode || pg == studio::PERFORM || pg == studio::AI || pg == studio::SPECTRUM ||
                               pg == studio::PANEL ||
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
        cutoff[e] = constrain(cutoff[e] * powf(1.06f, accel * float(l - last_l)), cr.lo, cr.hi);
        engine_filter(e);
        bridge_filter();
        cc_out(gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_CUTOFF), gf_norm(cr, cutoff[e]));
        last_l = l;
    }
    if (r != last_r) {
        const GF_Range rr = RESO_RANGE[e];
        resonance[e] = constrain(resonance[e] + 0.05f * (rr.hi - rr.lo) * (r - last_r), rr.lo, rr.hi);
        engine_filter(e);
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
        Serial.printf("-- %s · cpu %.1f%% (máx %.1f%%) · memoria de audio %d (máx %d) · teclado %s · cabeza %s (%lu ACKs)\n",
                      ENGINE_NAME[e], AudioProcessorUsage(), AudioProcessorUsageMax(), AudioMemoryUsage(),
                      AudioMemoryUsageMax(), host_seen ? "sí" : "no", link_up ? "sí" : "no", (unsigned long)acks);
    }
}
