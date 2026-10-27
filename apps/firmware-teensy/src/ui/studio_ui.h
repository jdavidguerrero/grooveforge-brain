#pragma once

#include <stdint.h>

#include <protocol.h>

/**
 * @file studio_ui.h
 * @brief Máquina de páginas del panel de Studio: el Teensy es el dueño de la navegación
 *        (apps/docs/variants/studio/theory/04-studio-ui-state.md).
 *
 * Lógica pura, sin hardware: recibe controles (físicos o UI_INPUT de la cabeza), actualiza el estado de
 * UI que se manda entero en UI_STATE (0x44) y devuelve la acción musical que el sketch debe ejecutar.
 * El comportamiento es el de `App::handle` de apps/studio-head (UI v3 + ADR-004), portado tal cual.
 */

namespace studio {

/// Páginas: mismo enum que `Page` de la cabeza (7 = error, lo decide la cabeza y nunca viaja).
enum Page : uint8_t { PERFORM = 0, AI = 1, SPECTRUM = 2, ENV = 3, FX = 4, ENGINE = 5, PRESETS = 6, OSC = 8, LFO = 9,
                    SYNTH_FX = 10, PANEL = 11,     ///< PANEL: 4.ª vista de la home (fidelidad §11.7)
                    GROOVE = 12,                   ///< SHIFT + B3 (theory/09 de Studio §7)
                    HYBRID = 13,                   ///< home del modo HYBRID SYNTH (theory/12 §3d)
                    LAYER_PRESETS = 14 };          ///< presets de capa (B4 en HYBRID SYNTH)

/// Ids de control, compartidos con UI_INPUT (0x45). NAV lleva el paso del giro en `value`.
/// Key1–Key4 = B1–B4 del panel (Arduino ya define B1, B2… como literales binarios).
enum class Control : uint8_t {
    Nav = 0, NavPush, Key1, Key2, Key3, Key4, Mode, B6Down, B6Up, EncLPush,
    Pick0, Pick1, Pick2, Pick3, Pick4, Pick5,
    FxView0, FxView1, FxView2, FxViewNext, EngineCard, SwipeUp, SwipeDown,
    NavLong,                                     ///< NAV push ≥ 500 ms: vuelve a home (theory/04 §3.1)
    P1Push, P2Push,                              ///< pulsadores de P1 (pin 32) y P2 (pin 9), ADR-004
    VolUp, VolDown,                              ///< volumen de salida desde la cabeza (táctil) mientras no hay pot VOL
};

/// Tarjetas de la vista PANEL en el orden en que NAV las recorre (la cabeza dibuja el mismo orden).
enum class PanelCard : uint8_t { Osc, Mod, EnvAmp, EnvFilter, Filter, Lfo, SynthFx };
struct PanelItem {
    PanelCard card;
    uint8_t stop;                                ///< parada de OSC o de MOD
};
/// Cantidad de tarjetas de PANEL del engine y la tarjeta `i`.
uint8_t panel_card_count(uint8_t engine);
PanelItem panel_card(uint8_t engine, uint8_t i);

/// Lo que el sketch tiene que hacer en el sonido después de un control.
/// LfoShape lleva el paso de NAV como int8_t en `arg`; SfxToggle, el FX de la cadena del synth (0–3);
/// GroovePush, la parada de GROOVE (el sketch decide: en CHORD con notas apretadas graba el acorde, si no enciende /
/// apaga); Tap, B4 en GROOVE.
enum class Action : uint8_t { None, LoadEngine, LoadPreset, SavePreset, FxToggle, FilterBypass, LfoSync, LfoShape,
                              SfxToggle, GroovePush, Tap, VolStep,
                              HybridEnter,   ///< MODE entra a HYBRID SYNTH: arg = preset de capa (theory/12 §3d)
                              HybridExit,    ///< MODE sale de HYBRID SYNTH (queda A sola en SYNTH)
                              LayerLoad,     ///< NAV push en la home o en LAYER_PRESETS: arg = preset de capa
                              LayerSave,     ///< SAVE en LAYER_PRESETS: arg = lugar nuevo
                              LayerFocus,    ///< B1 / B2 / B3 otra vez, P1 / P2 push: arg = 0 (A) o 1 (B)
                              LayerCustom }; ///< CUSTOM elegido: arg = engine A | engine B << 4

struct Result {
    Action action = Action::None;
    uint8_t arg = 0;
};

constexpr uint8_t ENGINE_COUNT = 6;
constexpr uint8_t FAV_FX_SLOT = 2;               ///< B4 en FX: GHOST ECHO (como la cabeza)
constexpr uint8_t PRESET_ROWS_VISIBLE = 6;       ///< paso del swipe en PRESETS
constexpr uint8_t PRESET_SLOTS = 64;             ///< presets por engine: bancos A–D de 16 (theory/13)
constexpr uint8_t PRESET_BANK = 16;
constexpr uint8_t SFX_SLOTS = 5;                 ///< cadena de FX del synth (theory/fase3-fx.md §2)
constexpr uint8_t GROOVE_STOPS = 5;              ///< PUMP · ARP · CHORD · UNISON · MACRO
constexpr uint8_t LAYER_SLOTS = 32;              ///< presets de capa: bancos A y B de 16 (layer_bank.h)
constexpr uint8_t MODE_SYNTH = 0, MODE_FX = 1, MODE_HYBRID = 2;

class StudioUi {
public:
    StudioUi();

    /// Procesa un control y devuelve la acción musical (si la hay). Marca el estado como cambiado.
    Result handle(Control c, int8_t value = 1);

    /// El engine cambió por otra vía (MIDI CC 0, preset): OSC y ENGINE SELECT lo usan.
    void set_engine(uint8_t engine);
    /// Estado del reloj común para la cabeza (flags CLOCK_MIDI / CLOCK_RUN). Marca cambio solo si cambió.
    void set_clock(bool midi, bool running) {
        const uint8_t f = uint8_t((ui_.flags & ~(GF_UI_FLAG_CLOCK_MIDI | GF_UI_FLAG_CLOCK_RUN)) |
                                  (midi ? GF_UI_FLAG_CLOCK_MIDI : 0) | (running ? GF_UI_FLAG_CLOCK_RUN : 0));
        if (f != ui_.flags) {
            ui_.flags = f;
            dirty_ = true;
        }
    }
    /// HYBRID SYNTH (theory/12 §3d): el sketch lo enciende al cargar un preset de capa por CC 18 y lo apaga con CC 18 = 0.
    void set_hybrid(bool on) {
        if (on == hybrid()) return;
        ui_.shift = ui_.fx_hold = 0;
        if (on) {
            if (ui_.mode == MODE_SYNTH) synth_view_ = last_view_;
            ui_.mode = MODE_HYBRID;
            show(HYBRID);
        } else {
            ui_.mode = MODE_SYNTH;
            last_view_ = synth_view_;
            show(synth_view_);
        }
        dirty_ = true;
    }
    bool hybrid() const { return ui_.mode == MODE_HYBRID; }
    uint8_t layer_cursor() const { return layer_cursor_; }
    void set_layer_cursor(uint8_t i) { layer_cursor_ = i < layer_count_ ? i : 0; refresh_cursor(); }
    /// Preset de capa para entrar al modo (la fila + CUSTOM del final no es un preset).
    uint8_t layer_to_load() const { return layer_cursor_ < layer_count_ ? layer_cursor_ : 0; }
    void set_layer_count(uint8_t n) {
        layer_count_ = n > LAYER_SLOTS ? LAYER_SLOTS : n;
        if (layer_cursor_ > layer_count_) layer_cursor_ = layer_count_;   // = layer_count_: la fila + CUSTOM
        if (layer_row_ > layer_count_) layer_row_ = layer_count_;
        refresh_cursor();
        dirty_ = true;
    }
    uint8_t layer_count() const { return layer_count_; }
    /// Capa en foco (flag FOCUS_B): la que editan B1 / B2 y ENGINE SELECT.
    void set_focus(uint8_t f) {
        const uint8_t fl = uint8_t(f ? ui_.flags | GF_UI_FLAG_FOCUS_B : ui_.flags & ~GF_UI_FLAG_FOCUS_B);
        if (fl != ui_.flags) {
            ui_.flags = fl;
            dirty_ = true;
        }
    }
    uint8_t focus() const { return ui_.flags & GF_UI_FLAG_FOCUS_B ? 1 : 0; }
    /// 0 = sin CUSTOM · 1 eligiendo el engine de A · 2 el de B (theory/12 §3d rev. 5).
    uint8_t custom_step() const { return custom_step_; }

    /// Capa híbrida sonando (flag LAYER para la cabeza).
    void set_layer(bool on) {
        const uint8_t f = uint8_t(on ? ui_.flags | GF_UI_FLAG_LAYER : ui_.flags & ~GF_UI_FLAG_LAYER);
        if (f != ui_.flags) {
            ui_.flags = f;
            dirty_ = true;
        }
    }
    void set_preset_count(uint8_t n) { preset_count_ = n; if (preset_cursor_ > n) preset_cursor_ = n; refresh_cursor(); }
    uint8_t preset_count() const { return preset_count_; }

    const GF_UiState& state() const { return ui_; }
    uint8_t engine() const { return engine_; }

    /// true una vez por cada cambio pendiente de enviar.
    bool take_dirty() {
        bool d = dirty_;
        dirty_ = false;
        return d;
    }

private:
    void show(uint8_t page);
    void toggle_page(uint8_t page);
    void refresh_cursor();
    Result handle_fx(Control c, int8_t v);
    Result handle_synth(Control c, int8_t v);
    Result handle_hybrid(Control c, int8_t v);
    Result focus_to(uint8_t f);
    Result custom_pick(uint8_t engine);
    void custom_cancel();

    GF_UiState ui_{};
    uint8_t last_view_ = PERFORM;
    uint8_t synth_view_ = PERFORM;               ///< vista de SYNTH al pasar a FX: vuelve al salir de HYBRID SYNTH
    uint8_t engine_ = 0;
    uint8_t engine_cursor_ = 0;
    uint8_t preset_cursor_ = 0;
    uint8_t preset_count_ = 8;
    uint8_t sfx_cursor_ = 0;
    uint8_t env_sel_ = 0;                        ///< ENV: 0 AMP · 1 FILTER (lo elige NAV)
    uint8_t osc_sel_[ENGINE_COUNT] = {};
    uint8_t lfo_stop_[ENGINE_COUNT] = {};         ///< LFO / MOD: 0 = LFO, 1… = paradas de MOD del engine
    uint8_t panel_cursor_ = 0;                   ///< PANEL: tarjeta (panel_card)
    uint8_t groove_stop_ = 0;                    ///< GROOVE: parada (común a todos los engines)
    uint8_t layer_cursor_ = 0;                   ///< HYBRID: preset de capa resaltado en la home
    uint8_t layer_row_ = 0;                      ///< LAYER_PRESETS: fila (0 = SAVE)
    uint8_t layer_count_ = 8;
    uint8_t custom_step_ = 0, custom_a_ = 0, custom_focus_ = 0;
    bool dirty_ = true;
};

}  // namespace studio
