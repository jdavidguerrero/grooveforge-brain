#pragma once

/**
 * @file bridge_handlers.h
 * @brief Handlers de comandos Bridge — ESP32-S3
 *
 * Registra los handlers específicos de display/engine en un BridgeSlave.
 * Llama a bridge_handlers_init() una vez en setup() antes de bridge.init().
 */

#include "bridge_slave.h"

/**
 * @brief Registra todos los handlers de comandos en el slave.
 *
 * Handlers en scope de Sprint 3.2:
 *   HEARTBEAT       → keepalive, marca conexión UP
 *   GET_VERSION     → responde con VERSION(1,0,0)
 *   ENGINE_CHANGED  → loguea engine_id + nombre
 *   PARAM_CHANGED   → loguea param_id + value
 *   SET_ENGINE      → ACK
 *   DISPLAY_UPDATE  → ACK (display es autónomo en carrusel por ahora)
 *   CLOUD_STATUS    → loguea connected + latency
 *   resto de CMDs   → ACK genérico (sin acción)
 */
void bridge_handlers_init(BridgeSlave& slave);

/** Nombre del engine activo (actualizado por ENGINE_CHANGED). */
const char* bridge_get_engine_name(void);

/** ID del engine activo (actualizado por ENGINE_CHANGED). */
uint8_t bridge_get_engine_id(void);

/** true si el ESP32 está conectado al cloud (actualizado por CLOUD_STATUS). */
bool bridge_get_cloud_connected(void);

/** Wet/dry del FX activo (0.0–1.0), actualizado por PARAM_CHANGED param_id=0xFF. */
float bridge_get_wet_dry(void);

/** Cursor en FX_SELECT (0-8), actualizado por PARAM_CHANGED param_id=0xFE. */
uint8_t bridge_get_fx_cursor(void);

/** Cursor en PARAM_EDIT (0-7), 0xFF = modo FX_MAIN (no editando param). */
uint8_t bridge_get_param_cursor(void);

/** Último valor de parámetro recibido, actualizado por PARAM_CHANGED real. */
float bridge_get_param_value(void);

/** Nombre del parámetro [fx_idx][param_idx] — tabla espejo del sketch 27. */
const char* bridge_get_param_name(uint8_t fx, uint8_t param);

/** Nivel de banda espectral [0-7] — actualizado por PARAM_CHANGED 0xE0-0xE7.
 *  0.0 = silencio, 1.0 = nivel máximo. */
float bridge_get_spectrum(uint8_t band);

/** Modo top-level activo: 0=FX, 1=SYNTH, 2=AI.
 *  Actualizado por PARAM_CHANGED param_id=0xFD. */
uint8_t bridge_get_top_mode(void);

/** Progreso del arm de AI mode (0.0–1.0). -1.0 = cancelado / inactivo.
 *  Actualizado por PARAM_CHANGED param_id=0xFB. */
float bridge_get_ai_progress(void);

/** Caché de valores de parámetros por FX. Actualizado por PARAM_CHANGED. */
float bridge_get_param_cached(uint8_t fx, uint8_t param);

/** Índice del sub-parámetro 1 (ENC L) para un FX dado. */
uint8_t bridge_get_sub1(uint8_t fx);

/** Índice del sub-parámetro 2 (ENC R) para un FX dado. */
uint8_t bridge_get_sub2(uint8_t fx);

/** Último param_id de un PARAM_CHANGED real (excluye especiales 0xF8-0xFF y spectrum 0xE0-0xE7). */
uint16_t bridge_get_last_real_param_id(void);

/** Timestamp (lv_tick_get()) del último PARAM_CHANGED real. */
uint32_t bridge_get_last_real_param_ms(void);

/** Nivel de navegación SYNTH: 0=ENGINE_LIST, 1=ENGINE_SUBHOME, 2=GROUP_VIEW. */
uint8_t bridge_get_synth_level(void);

/** Grupo activo en modo SYNTH: 0=OSC, 1=ENV, 2=FILTER, 3=LFO. */
uint8_t bridge_get_synth_group(void);

/** Cursor de sub-parámetro dentro del grupo activo. */
uint8_t bridge_get_synth_param(void);

/** Cursor en ENGINE_LIST (0-based), actualizado por PARAM_CHANGED param_id=0xFE en modo SYNTH. */
uint8_t bridge_get_engine_cursor(void);

/** Cutoff del filtro del engine activo (0.0–1.0). Actualizado por PARAM_CHANGED grupo FILTER param 0. */
float bridge_get_synth_cutoff(void);

/** Resonancia del filtro del engine activo (0.0–1.0). Actualizado por PARAM_CHANGED grupo FILTER param 1. */
float bridge_get_synth_resonance(void);

/** true si el FX activo está en bypass (sin audio de efecto). Actualizado por param_id=0x00E8. */
bool bridge_get_fx_bypass(void);

/** true si Scale Lock está bypassed. Actualizado por PARAM_CHANGED param_id=0x00F2. */
bool bridge_get_scale_lock_bypass(void);

/** true si Beat Follower está bypassed. Actualizado por PARAM_CHANGED param_id=0x00F3. */
bool bridge_get_beat_follower_bypass(void);

/** Ordenamiento del chromagram en view_10. 0=cromático, 1=CoF (default).
 *  Cambia con PARAM_CHANGED param_id=0x00F0. */
uint8_t bridge_get_chromagram_ordering(void);

/**
 * @defgroup ml_results Resultados de inferencia ML (Sprint 32)
 *
 * Actualizados por GF_CMD_KEY_DETECTED (0x80), GF_CMD_CHORD_DETECTED (0x81)
 * y GF_CMD_BEAT_DETECTED (0x82). Antes de recibir el primer frame retornan
 * "---" / 0.
 *
 * Encoding de payload — ver `apps/docs/sprints/32-ml-engine-integration.md`:
 *   KEY_DETECTED:   payload[0]=key_idx (0-23), payload[1]=confidence
 *   CHORD_DETECTED: payload[0]=root (0-11),    payload[1]=quality (0-7)
 *   BEAT_DETECTED:  payload[0:1]=bpm×10 uint16 LE
 * @{
 */
/** Nombre de la tonalidad detectada, ej. "C MAJ" / "F# MIN" / "---". */
const char* bridge_get_ai_key_name(void);

/** Nombre del acorde detectado, ej. "Am7" / "C" / "---". */
const char* bridge_get_ai_chord_name(void);

/** BPM detectado (integer). 0 = desconocido. */
uint16_t bridge_get_ai_bpm(void);

/** Índice de la tonalidad detectada (0-23). 0-11=mayor, 12-23=menor.
 *  0xFF = ninguna detectada aún. */
uint8_t bridge_get_ai_key_idx(void);

/** Root del acorde detectado (0-11, donde 0=C). 0xFF = ninguno. */
uint8_t bridge_get_ai_chord_root(void);

/** Quality del acorde detectado (0=maj,1=min,2=7,3=m7,4=maj7,5=N). 0xFF = ninguno. */
uint8_t bridge_get_ai_chord_quality(void);
/** @} */

/**
 * @defgroup groove_state Estado live del modo AI (Sprint 33 — GROOVE_STATE 0x83)
 *
 * Actualizados por GF_CMD_GROOVE_STATE (0x83), que el Teensy envía @ 4Hz
 * mientras `g_in_ai_mode == true`. Cuando no hay frames recientes (>2s)
 * los getters retornan valores neutros (0 / sin snap).
 *
 * Payload del frame 0x83 (16 bytes):
 *   pitch_activity[12]   EMA tau=2s, 0-255 cada uno
 *   snap_event           rising-edge: 1 por un solo frame tras un snap
 *   snap_from, snap_to   MIDI notes 0-127 del evento snap
 *   beat_phase_256       0-255 dentro del bar actual (4 beats)
 *
 * Ver `apps/docs/sprints/33-ai-hero-viz.md §"Comando GROOVE_STATE 0x83"`.
 * @{
 */

/**
 * @brief Actividad acumulada de pitch class (0-11), normalizada 0-255.
 *        0=C, 1=C#, 2=D, ..., 11=B. EMA con tau=2s actualizada por el Teensy.
 * @param pc Pitch class 0-11 (idx fuera de rango → 0)
 * @return uint8_t 0-255 — proporcional al brillo del segmento en view_10
 */
uint8_t bridge_get_pitch_activity(uint8_t pc);

/**
 * @brief Información del último evento de snap (scale lock corrigiendo nota).
 *        Si nunca se ha producido snap o el último fue hace > 5s → retorna
 *        from=0, to=0, age_ms=0xFFFFFFFF.
 *
 * @param[out] from   MIDI note original que el usuario tocó (0-127)
 * @param[out] to     MIDI note resultante después del snap (0-127)
 * @param[out] age_ms Tiempo desde el evento en ms (0xFFFFFFFF si nunca)
 */
void bridge_get_last_snap(uint8_t* from, uint8_t* to, uint32_t* age_ms);

/**
 * @brief Estadísticas acumulativas de snap desde que se entró a AI mode.
 *        Se resetean cuando se reinicia AI mode (Teensy reinicia counters).
 *
 * @param[out] snapped Cuántas notas fueron cuantizadas por scale lock
 * @param[out] total   Cuántas notas se tocaron en total mientras AI mode activo
 */
void bridge_get_snap_stats(uint16_t* snapped, uint16_t* total);

/**
 * @brief Fase dentro del compás actual (bar = 4 beats).
 *        Útil para animar el "bar sweep" del chromagram en sync con el tempo
 *        detectado por el BeatFollower del Teensy. 0=downbeat, 255=just before
 *        next downbeat.
 *
 * @return uint8_t 0-255 (interpolar entre frames para fluidez)
 */
uint8_t bridge_get_beat_phase(void);

/** @} */

/**
 * @defgroup note_state Estado de nota MIDI activa (Sprint 31)
 *
 * Capturado por los handlers NOTE_ON / NOTE_OFF. Permite a view_02 animar
 * el osciloscopio en función de la tecla presionada:
 *   - bridge_get_note_midi()     → número MIDI de la nota (0-127)
 *   - bridge_get_note_velocity() → velocidad normalizada [0,1]
 *   - bridge_get_note_active()   → true mientras la nota está presionada
 * @{
 */
/** Número MIDI de la última nota ON (0-127). Default: 69 (A4). */
uint8_t bridge_get_note_midi(void);
/** Velocidad de la última nota ON, normalizada [0.0, 1.0]. */
float   bridge_get_note_velocity(void);
/** true si hay una nota presionada en este momento. */
bool    bridge_get_note_active(void);
/** @} */

/**
 * @brief Valor cacheado de un parámetro synth (normalizado 0.0–1.0).
 *
 * Indexado por engine (0=Moog, 1=Juno, 2=Prophet), grupo (0=OSC, 1=ENV,
 * 2=FILTER, 3=LFO) y param dentro del grupo (0-3).
 * Actualizado por cada PARAM_CHANGED con param_id byte-alto 0x10-0x12.
 * Retorna 0.0f si el param aún no ha sido enviado por el Teensy.
 */
float bridge_get_synth_param_cached(uint8_t engine, uint8_t group, uint8_t pidx);

/**
 * @defgroup manual_scale_override Override manual de escala (Sprint 34 Batch J)
 *
 * Actualizado por PARAM_CHANGED param_id=0x00EF (Teensy → ESP32).
 * Protocolo:
 *   value < 0      → AUTO mode (detección AI activa, sin override)
 *   value 0-23     → MANUAL mode con key_idx fijo (0-11 major, 12-23 minor)
 *
 * Nota: se usa 0x00EF (no 0x00F6) porque 0x00F6 está ocupado para SYNTH group nav.
 * @{
 */
/** true si el usuario fijó la escala manualmente desde view_16. */
bool bridge_get_scale_manual_mode(void);

/** Key index manual activo (0-23). 0xFF cuando manual_mode=false.
 *  Mismo encoding que bridge_get_ai_key_idx(): 0-11 major, 12-23 minor. */
uint8_t bridge_get_scale_manual_key(void);
/** @} */

/**
 * @defgroup ai_rack_state Estado de la AI Rack (Sprint 45)
 *
 * Actualizados por PARAM_CHANGED con IDs 0x00E9-0x00EE (únicos libres del
 * rango especial byte-alto=0 / byte-bajo 0xE9-0xEE).
 *
 * Scale Lock on/off → bridge_get_scale_lock_bypass() (0x00F2, invertido para ON=bypass false)
 * Beat FX on/off   → bridge_get_beat_follower_bypass() (0x00F3, invertido igual)
 * @{
 */

/** Fila activa en la AI Rack (0-5). Actualizado por param_id=0x00E9. */
uint8_t bridge_get_ai_rack_cursor(void);

/** true si Auto-Harmonize está habilitado. Actualizado por param_id=0x00EA. */
bool bridge_get_auto_harm_enabled(void);

/** Intervalo de armonía activo: 0=THIRD, 1=SIXTH. Actualizado por param_id=0x00EB. */
uint8_t bridge_get_auto_harm_interval(void);

/** Modo del arpeggiador (0=UP,1=DOWN,2=UP_DOWN,3=RANDOM,4=SMART). Actualizado por 0x00EC. */
uint8_t bridge_get_arp_mode(void);

/** División rítmica del arp (0=QUARTER,1=EIGHTH,2=SIXTEENTH). Actualizado por 0x00ED. */
uint8_t bridge_get_arp_division(void);

/** Estilo del Groove Humanizer (0=OFF,1=HUMAN,2=SWING). Actualizado por 0x00EE (parte entera). */
uint8_t bridge_get_groove_style(void);

/** Intensidad del Groove Humanizer (0.0-0.99). Actualizado por 0x00EE (parte fraccionaria). */
float bridge_get_groove_amount(void);

/** @} */
