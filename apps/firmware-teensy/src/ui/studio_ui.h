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
                    HYBRID = 13 };                 ///< home del modo HYBRID (theory/12 §3c)

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
                              HybridEnter,   ///< SHIFT + B2 sin capa: arg = capa a cargar (theory/12 §3c)
                              HybridExit,    ///< SHIFT + B2 en el modo
                              LayerLoad,     ///< NAV push en HYBRID: arg = capa
                              LayerFocus };  ///< P1 / P2 push en HYBRID: arg = 0 (A) o 1 (B)

struct Result {
    Action action = Action::None;
    uint8_t arg = 0;
};

constexpr uint8_t ENGINE_COUNT = 6;
constexpr uint8_t FAV_FX_SLOT = 2;               ///< B4 en FX: GHOST ECHO (como la cabeza)
constexpr uint8_t PRESET_ROWS_VISIBLE = 6;       ///< paso del swipe en PRESETS
constexpr uint8_t PRESET_SLOTS = 64;             ///< presets por engine: bancos A–D de 16 (theory/13)
constexpr uint8_t PRESET_BANK = 16;
constexpr uint8_t SFX_SLOTS = 4;                 ///< cadena de FX del synth (theory/05 de Studio)
constexpr uint8_t GROOVE_STOPS = 5;              ///< PUMP · ARP · CHORD · UNISON · MACRO
constexpr uint8_t HYBRID_LAYERS = 8;             ///< capas de la lista (layers.h)

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
    /// Modo HYBRID (theory/12 §3c): el sketch lo enciende al cargar una capa por CC 18, sin cambiar de página.
    void set_hybrid(bool on) {
        if (on == hybrid_) return;
        hybrid_ = on;
        if (on) last_view_ = HYBRID;
        else if (ui_.page == HYBRID) show(PERFORM);
        else last_view_ = PERFORM;
        dirty_ = true;
    }
    bool hybrid() const { return hybrid_; }
    uint8_t layer_cursor() const { return layer_cursor_; }
    void set_layer_cursor(uint8_t i) { layer_cursor_ = i < HYBRID_LAYERS ? i : 0; if (ui_.page == HYBRID) refresh_cursor(); }

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

    GF_UiState ui_{};
    uint8_t last_view_ = PERFORM;
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
    bool hybrid_ = false;                        ///< modo HYBRID: NAV largo y las teclas vuelven a la página HYBRID
    uint8_t layer_cursor_ = 0;                   ///< HYBRID: capa resaltada en la lista
    bool dirty_ = true;
};

}  // namespace studio
