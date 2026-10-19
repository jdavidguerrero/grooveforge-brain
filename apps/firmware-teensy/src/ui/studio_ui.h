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
                    SYNTH_FX = 10, PANEL = 11 };   ///< PANEL: 4.ª vista de la home (fidelidad §11.7)

/// Ids de control, compartidos con UI_INPUT (0x45). NAV lleva el paso del giro en `value`.
/// Key1–Key4 = B1–B4 del panel (Arduino ya define B1, B2… como literales binarios).
enum class Control : uint8_t {
    Nav = 0, NavPush, Key1, Key2, Key3, Key4, Mode, B6Down, B6Up, EncLPush,
    Pick0, Pick1, Pick2, Pick3, Pick4, Pick5,
    FxView0, FxView1, FxView2, FxViewNext, EngineCard, SwipeUp, SwipeDown,
    NavLong,                                     ///< NAV push ≥ 500 ms: vuelve a home (theory/04 §3.1)
    P1Push, P2Push,                              ///< pulsadores de P1 (pin 32) y P2 (pin 9), ADR-004
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
/// LfoShape lleva el paso de NAV como int8_t en `arg`; SfxToggle, el FX de la cadena del synth (0–3).
enum class Action : uint8_t { None, LoadEngine, LoadPreset, SavePreset, FxToggle, FilterBypass, LfoSync, LfoShape,
                              SfxToggle };

struct Result {
    Action action = Action::None;
    uint8_t arg = 0;
};

constexpr uint8_t ENGINE_COUNT = 6;
constexpr uint8_t FAV_FX_SLOT = 2;               ///< B4 en FX: GHOST ECHO (como la cabeza)
constexpr uint8_t PRESET_ROWS_VISIBLE = 6;       ///< paso del swipe en PRESETS
constexpr uint8_t SFX_SLOTS = 4;                 ///< cadena de FX del synth (theory/05 de Studio)

class StudioUi {
public:
    StudioUi();

    /// Procesa un control y devuelve la acción musical (si la hay). Marca el estado como cambiado.
    Result handle(Control c, int8_t value = 1);

    /// El engine cambió por otra vía (MIDI CC 0, preset): OSC y ENGINE SELECT lo usan.
    void set_engine(uint8_t engine);
    void set_preset_count(uint8_t n) { preset_count_ = n; if (preset_cursor_ > n) preset_cursor_ = n; }
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
    bool dirty_ = true;
};

}  // namespace studio
