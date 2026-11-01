#pragma once
#include "hybrid_router.h"
#include "ui/layer_bank.h"
#include "model_d_preview.h"
#include "juno_preview.h"
#include "prophet_preview.h"
#include "ob6_preview.h"
#include "dx7_preview.h"
#include "arp2600_preview.h"
#include <array>
#include <vector>
namespace gf {
/** @brief Piloto portable de dos engines, un ruteador y un bus SYNTH compartido. */
class HybridPreview {
public:
    /** @brief Reserva memorias y prepara el motor fuera del hilo de audio. */
    void prepare(double sample_rate);
    /** @brief Carga una capa válida antes de reproducir. */
    bool load(const studio::LayerPreset& layer);
    /** @brief Limpia voces y eventos pendientes. */
    void reset();
    /** @brief Devuelve el engine de una mitad cargada. */
    uint8_t engine_id(unsigned side) const { return side == 0 ? layer_.eng_a : layer_.eng_b; }
    /** @brief Dispara una nota MIDI. */
    void note_on(uint8_t note, float velocity);
    /** @brief Libera una nota MIDI. */
    void note_off(uint8_t note);
    /** @brief Controla el pedal de sustain. */
    void set_sustain(bool down);
    /** @brief Aplica pitch bend a las dos mitades. */
    void set_pitch_bend(int value);
    /** @brief Ajusta rueda, presión y sincronización de ambos LFO. */
    void set_expression(float wheel, float pressure, float bpm, bool sync);
    /** @brief Ajusta retraso y transposición sin interrumpir notas activas. */
    void set_layer_controls(float morph, uint8_t delay_ms, int8_t transpose, float gain_a_db, float gain_b_db);
    /** @brief Aplica cutoff, resonancia normalizada y envolvente básica de una mitad. */
    void set_half_controls(unsigned side, float cutoff, float resonance, float attack, float release);
    /** @brief Aplica los slots OSC/MOD normalizados de una mitad. */
    void set_half_osc(unsigned side, const std::array<float, 48>& values);
    /** @brief Aplica envolventes, LFO y parámetros de voz de una mitad. */
    void set_half_voice(unsigned side, const std::array<float, 20>& values);
    /** @brief Ajusta GROOVE y SYNTH FX compartidos a partir de los valores de A. */
    void set_bus(const std::array<float, 21>& groove, const std::array<float, 25>& effects);
    /** @brief Ajusta DRIFT, GRIT, POCKET, MOVE y sus profundidades normalizadas. */
    void set_macros(const std::array<float, 7>& values);
    /** @brief Ajusta MOVE de la mitad B sin modificar los FX compartidos. */
    void set_b_move(float value);
    /** @brief Ajusta el balance de las dos mitades. */
    void set_morph(float value);
    /** @brief Actualiza tempo y posición del host. */
    void set_transport(float bpm, bool has_position, double beats, bool running);
    /** @brief Produce una muestra estéreo. */
    void process_frame(float& left, float& right);
private:
    struct Half {
        uint8_t engine = 0;
        studio::Preset preset;
        std::array<float, 4> last_controls{};
        ModelDPreview model; JunoPreview juno; ProphetPreview prophet; Ob6Preview ob6; Dx7Preview dx7; Arp2600Preview arp;
        void prepare(double fs);
        void configure(const studio::Preset& preset, bool second);
        void reset();
        void note_on(uint8_t note, float velocity);
        void note_off(uint8_t note);
        void controls(float cutoff, float resonance, float attack, float release);
        void update_patch(const studio::Preset& updated);
        void pitch_bend(int value);
        void expression(float wheel, float pressure, float bpm, bool sync);
        void process(float& left, float& right, float tape_pitch);
    };
    static void route_on(void* context, uint8_t side, uint8_t note, float velocity);
    static void route_off(void* context, uint8_t side, uint8_t note);
    static void groove_on(void* context, uint8_t note, float velocity);
    static void groove_off(void* context, uint8_t note);
    std::array<Half, 2> halves_;
    HybridRouter router_;
    ProphetPreview bus_;
    studio::LayerPreset layer_;
    std::vector<int16_t> delay_l_, delay_r_, plate_;
    double fs_ = 44100;
    float b_unison_gain_ = 1, last_b_move_ = -1;
    void refresh_half(unsigned side);
    void update_unison_gain();
    void apply_bus_fx();
    std::array<float, 7> last_macros_{};
    std::array<float, 21> last_groove_{};
    std::array<float, 25> last_effects_{};
    std::array<std::array<float, 20>, 2> last_voice_{};
    std::array<std::array<float, 48>, 2> last_osc_{};
    bool loaded_ = false;
};
}
