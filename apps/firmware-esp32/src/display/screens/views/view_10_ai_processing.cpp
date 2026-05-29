/**
 * @file view_10_ai_processing.cpp
 * @brief Vista 10 — AI PROCESSING: Camelot Wheel hero visualization (Sprint 34 B-F)
 *
 * ## Dos modos de operación
 *
 * ### Modo overlay (durante hold 0→4s)
 *   Activado por `view_10_overlay_start()` desde `carousel_ai_overlay_start()`.
 *   La vista anterior permanece en pantalla; este módulo crea encima:
 *     - Un dimmer negro 40% opacidad (→ vista anterior al 60% visualmente)
 *     - El arco de progreso (200px, 6px grosor) que se llena 0→360°
 *     - Un dot central que "respira" (12→18px, 800ms period)
 *     - Label "ANALYZING..." que hace fade-in a t=1s
 *   Si el usuario suelta antes de 4s → `view_10_overlay_cancel()` → fade-out
 *   200ms + delete; la vista anterior reaparece intacta.
 *   Si se completa → `view_10_overlay_destroy_immediate()` (el carousel hace
 *   flash blanco + carousel_goto(VIEW_IDX_AI_PROC) para el modo full).
 *
 * ### Modo full (vista completa post-hold) — Sprint 34 B-F
 *   Construida por `view_10_create()` / destruida por `view_10_destroy()`.
 *
 *   Jerarquía visual (z-order inferior → superior):
 *     Capa A — Chromagram 12 segmentos Camelot (220×220, stroke 16px)
 *              + note labels blancos en r=88 (siempre legibles, opa fija 200)
 *              + tonic anchor dot en r=115
 *     Capa B — Core pulse (dot 5px, respira al BPM) — SIMPLIFICADO Batch F
 *     Capa C — Chord card hero (s_chord_card GF_FONT_HERO 36px)
 *              + key chip (s_key_chip GF_FONT_MICRO)
 *              + camelot chip (s_camelot_chip, color por zona Camelot)
 *              + bpm chip (s_bpm_chip GF_FONT_MICRO)
 *     Capa D — AI status line (s_ai_status_lbl, teal/gray según estado)
 *
 *   ELIMINADOS en Batch F:
 *     - Triada (s_triad_dot[], s_triad_line[]) — los colores Camelot son suficiente
 *     - Bar sweep (era confuso; se reemplaza por el core pulse)
 *     - Microcopy (reemplazado por AI status line)
 *     - Reference ring gris (los colores Camelot son la estructura visual)
 *     - s_bpm_label independiente (integrado en s_bpm_chip del chord card)
 *
 * Spec: apps/docs/sprints/34-camelot-hero-modes.md §Batches B-F
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"
#include <math.h>
#include <stdio.h>

/* ── Callbacks de animación compartidos (overlay + full) ──────────────────── */

/** exec_cb: opacidad de un lv_obj_t. */
static void opa_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
}

/**
 * exec_cb: tamaño del core (respiración) — recentra al cambiar de tamaño.
 * Funciona tanto si el padre es lv_scr_act() (modo full) como s_ov_dimmer
 * (modo overlay), porque lv_obj_center() centra dentro del padre.
 */
static void core_size_cb(void* obj, int32_t v) {
    lv_obj_t* d = static_cast<lv_obj_t*>(obj);
    lv_obj_set_size(d, v, v);
    lv_obj_set_style_radius(d, v / 2, 0);
    lv_obj_center(d);
}

/* ══════════════════════════════════════════════════════════════════════════
 * MODO OVERLAY — estado y funciones
 * ══════════════════════════════════════════════════════════════════════════ */

static lv_obj_t*   s_ov_dimmer      = nullptr;  ///< contenedor semi-opaco (40% negro)
static lv_obj_t*   s_ov_arc         = nullptr;  ///< arco de progreso 0→360°
static lv_obj_t*   s_ov_core        = nullptr;  ///< dot central que respira
static lv_obj_t*   s_ov_label       = nullptr;  ///< "ANALYZING..."
static lv_timer_t* s_ov_timer       = nullptr;  ///< timer 50ms actualiza arco
static lv_timer_t* s_ov_fadein_tmr  = nullptr;  ///< one-shot 1s para fade-in label

static void ov_progress_timer_cb(lv_timer_t* /*t*/) {
    if (!s_ov_arc) return;
    float p = bridge_get_ai_progress();
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    /* Arco desde 270° (top) en sentido horario: llena 0→360° con el progreso. */
    lv_arc_set_value(s_ov_arc, (int16_t)(p * 360.0f + 0.5f));
}

/**
 * @brief Inicia el overlay AI sobre la vista actual.
 *
 * Crea un dimmer + arco + dot + label sobre @p screen (lv_scr_act()).
 * No destruye ni toca los widgets de la vista anterior.
 *
 * @param screen  lv_scr_act() — los widgets del overlay serán hijos de él.
 */
void view_10_overlay_start(lv_obj_t* screen) {
    /* ── 1. Dimmer container ────────────────────────────────────────────── */
    /* 40% negro opaco = vista anterior percibida al ~60% de brillo.
     * No es lo mismo que LV_OPA_60 en la vista, pero el efecto visual es
     * equivalente sin necesidad de un container intermedio por vista. */
    s_ov_dimmer = lv_obj_create(screen);
    lv_obj_set_size(s_ov_dimmer, 240, 240);
    lv_obj_set_pos(s_ov_dimmer, 0, 0);
    lv_obj_set_style_bg_color(s_ov_dimmer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ov_dimmer, LV_OPA_TRANSP, 0);   /* empieza transparente */
    lv_obj_set_style_border_width(s_ov_dimmer, 0, 0);
    lv_obj_set_style_radius(s_ov_dimmer, 0, 0);
    lv_obj_set_style_pad_all(s_ov_dimmer, 0, 0);
    lv_obj_clear_flag(s_ov_dimmer, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Fade-in del dimmer en 100ms hasta 40% opacidad (102/255). */
    lv_anim_t da;
    lv_anim_init(&da);
    lv_anim_set_var(&da, s_ov_dimmer);
    lv_anim_set_exec_cb(&da, opa_cb);
    lv_anim_set_values(&da, 0, 102);    /* 0 → LV_OPA_40 */
    lv_anim_set_time(&da, 100);
    lv_anim_start(&da);

    /* ── 2. Arco de progreso (200×200, centrado, 6px grosor) ───────────── */
    /* 200px (radio 100) en vez de los 220px del full-view para que quepa con
     * margen en el dimmer 240×240. La diferencia visual es mínima. */
    s_ov_arc = lv_arc_create(s_ov_dimmer);
    lv_obj_set_size(s_ov_arc, 200, 200);
    lv_obj_center(s_ov_arc);
    lv_arc_set_rotation(s_ov_arc, 270);         /* empieza desde las 12h */
    lv_arc_set_bg_angles(s_ov_arc, 0, 360);
    lv_arc_set_range(s_ov_arc, 0, 360);
    lv_arc_set_value(s_ov_arc, 0);
    lv_obj_remove_style(s_ov_arc, nullptr, LV_PART_KNOB);
    lv_obj_set_style_arc_color(s_ov_arc, GF_COLOR_TEAL_DIM,  LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_ov_arc, GF_COLOR_TEAL_PLUS, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_ov_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_ov_arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_ov_arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(s_ov_arc, LV_OBJ_FLAG_CLICKABLE);

    /* ── 3. Dot central que respira (12→18px, 800ms) ───────────────────── */
    s_ov_core = gf_dot(s_ov_dimmer, 12, GF_COLOR_TEAL_PLUS);
    lv_obj_center(s_ov_core);

    lv_anim_t ca;
    lv_anim_init(&ca);
    lv_anim_set_var(&ca, s_ov_core);
    lv_anim_set_exec_cb(&ca, core_size_cb);
    lv_anim_set_values(&ca, 12, 18);
    lv_anim_set_time(&ca, 800);
    lv_anim_set_playback_time(&ca, 800);
    lv_anim_set_repeat_count(&ca, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&ca);

    /* ── 4. Label "ANALYZING..." — invisible, fade-in a t=1s ───────────── */
    s_ov_label = lv_label_create(s_ov_dimmer);
    lv_label_set_text(s_ov_label, "ANALYZING...");
    lv_obj_set_style_text_font(s_ov_label, GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(s_ov_label, GF_COLOR_TEAL_PLUS, 0);
    lv_obj_set_style_opa(s_ov_label, LV_OPA_TRANSP, 0);
    lv_obj_align(s_ov_label, LV_ALIGN_CENTER, 0, 28);

    s_ov_fadein_tmr = lv_timer_create([](lv_timer_t* t) {
        lv_timer_del(t);
        s_ov_fadein_tmr = nullptr;
        if (!s_ov_label) return;
        lv_anim_t la;
        lv_anim_init(&la);
        lv_anim_set_var(&la, s_ov_label);
        lv_anim_set_exec_cb(&la, opa_cb);
        lv_anim_set_values(&la, 0, LV_OPA_COVER);
        lv_anim_set_time(&la, 300);
        lv_anim_start(&la);
    }, 1000, nullptr);

    /* ── 5. Timer de actualización del arco (50ms = 20fps) ─────────────── */
    s_ov_timer = lv_timer_create(ov_progress_timer_cb, 50, nullptr);
}

/**
 * @brief Cancela el overlay (usuario soltó ENC NAV antes de 4s).
 *
 * Anima el dimmer a OPA_TRANSP en 200ms y luego lo elimina.
 * La vista anterior queda intacta y visible.
 */
void view_10_overlay_cancel(void) {
    if (!s_ov_dimmer) return;

    /* Detener timers activos del overlay. */
    if (s_ov_timer) {
        lv_timer_del(s_ov_timer);
        s_ov_timer = nullptr;
    }
    if (s_ov_fadein_tmr) {
        lv_timer_del(s_ov_fadein_tmr);
        s_ov_fadein_tmr = nullptr;
    }

    /* Cancelar animaciones pendientes sobre objetos del overlay. */
    if (s_ov_core)   lv_anim_del(s_ov_core,   core_size_cb);
    if (s_ov_label)  lv_anim_del(s_ov_label,   opa_cb);
    lv_anim_del(s_ov_dimmer, opa_cb);

    /* Guardar el puntero antes de nulificar — el ready_cb lo necesita. */
    lv_obj_t* dimmer = s_ov_dimmer;
    s_ov_dimmer = nullptr;
    s_ov_arc    = nullptr;
    s_ov_core   = nullptr;
    s_ov_label  = nullptr;

    /* Fade-out del dimmer (y todos sus hijos) en 200ms, luego delete. */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, dimmer);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_values(&a, 102, 0);
    lv_anim_set_time(&a, 200);
    lv_anim_set_ready_cb(&a, [](lv_anim_t* a) {
        lv_obj_del(static_cast<lv_obj_t*>(a->var));
    });
    lv_anim_start(&a);
}

/**
 * @brief Destruye el overlay inmediatamente sin animación de fade.
 *
 * Usado cuando carousel_goto() interrumpe el hold (ej. PANIC, timeout).
 * carousel_goto() llama lv_obj_clean() justo después — por eso es seguro
 * simplemente matar el timer y dejar que lv_obj_clean lo elimine,
 * pero hacemos lv_obj_del explícito para cancelar animaciones pendientes
 * antes de que el pool global las dispare contra un objeto eliminado.
 */
void view_10_overlay_destroy_immediate(void) {
    if (s_ov_timer) {
        lv_timer_del(s_ov_timer);
        s_ov_timer = nullptr;
    }
    if (s_ov_fadein_tmr) {
        lv_timer_del(s_ov_fadein_tmr);
        s_ov_fadein_tmr = nullptr;
    }
    if (s_ov_core)   lv_anim_del(s_ov_core,   core_size_cb);
    if (s_ov_label)  lv_anim_del(s_ov_label,   opa_cb);
    if (s_ov_dimmer) {
        lv_anim_del(s_ov_dimmer, opa_cb);
        lv_obj_del(s_ov_dimmer);
        s_ov_dimmer = nullptr;
    }
    s_ov_arc   = nullptr;
    s_ov_core  = nullptr;
    s_ov_label = nullptr;
}

/* ══════════════════════════════════════════════════════════════════════════
 * MODO FULL — Camelot hero visualization (Sprint 34 B-F)
 *
 * Capa A — Chromagram 12 segmentos, cada uno con su color Camelot fijo.
 *           Opacidad: min_opa por diatonismo + boost por actividad real (pow 0.6).
 *           Note labels BLANCOS r=88, opa fija 200 (siempre legibles).
 *           Tonic anchor dot en r=115.
 *
 * Capa B — Core pulse (dot 5px, respira infinito).
 *
 * Capa C — Chord card hero al centro:
 *   s_chord_card   → chord > key > "—" (GF_FONT_HERO 36px, blanco)
 *   s_key_chip     → "in A major" / "" (GF_FONT_MICRO, gray, y=+25)
 *   s_camelot_chip → "11A"/"11B"/"—" (GF_FONT_MICRO, color Camelot, y=+42)
 *   s_bpm_chip     → "120 BPM"/"—" (GF_FONT_MICRO, gray, y=+62)
 *
 * Capa D — AI status line (GF_FONT_MICRO, teal/gray, y=+85):
 *   SNAP (2s) > CHORD > KEY LOCKED > DETECTING > LISTENING > IDLE
 *
 * ELIMINADO (Batch F):
 *   Triada, bar sweep wedge, microcopy, reference ring gris, s_bpm_label.
 *
 * ══════════════════════════════════════════════════════════════════════════ */

/* Tablas de ordenamiento del chromagram */

/** Posición visual p → pitch class. CoF clockwise desde C @ top.
 *  p=0→C, p=1→G, p=2→D, p=3→A, p=4→E, p=5→B, p=6→F#, p=7→C#, p=8→G#, p=9→D#, p=10→A#, p=11→F */
static const uint8_t COF_POS_TO_PC[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };

/** Pitch class → posición visual CoF (tabla inversa de COF_POS_TO_PC). */
static const uint8_t PC_TO_COF_POS[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };

/** Texto de nota para cada posición visual en modo CoF */
static const char* const NOTE_LABELS_COF[12] = {
    "C","G","D","A","E","B","F#","C#","G#","D#","A#","F"
};

/** Texto de nota para cada posición visual en modo cromático (posición == pitch class) */
static const char* const NOTE_LABELS_CHROMATIC[12] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

/** Notas short para el status line (pitch class → nombre). */
static const char* const NOTES[12] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

/**
 * Retorna el pitch class del segmento en posición visual p,
 * según el ordenamiento activo (CoF=1, cromático=0).
 */
static uint8_t position_to_pc(uint8_t p) {
    return (bridge_get_chromagram_ordering() == 1) ? COF_POS_TO_PC[p] : p;
}

/* Capa A — chromagram */
static lv_obj_t*   s_chroma_seg[12]  = {};      ///< 12 arc wedges, outer ring (220×220)
static lv_obj_t*   s_note_lbl[12]    = {};      ///< 12 note labels en r=88
static lv_obj_t*   s_tonic_anchor    = nullptr; ///< dot 5px fuera del ring (r=115)

/* Capa B — core pulse */
static lv_obj_t*   s_core_pulse      = nullptr; ///< dot 5px que respira

/* Capa C — chord card hero */
static lv_obj_t*   s_chord_card      = nullptr; ///< chord/key hero (GF_FONT_HERO 36px)
static lv_obj_t*   s_key_chip        = nullptr; ///< "in A major" (GF_FONT_MICRO, gray)
static lv_obj_t*   s_bpm_chip        = nullptr; ///< "120 BPM" (GF_FONT_MICRO, gray) — top of screen

/* Capa D — AI status */
static lv_obj_t*   s_ai_status_lbl   = nullptr; ///< priority state machine

/* Idle tracking */
static uint32_t    s_last_active_ms  = 0;

/* Snap arrow ephemeral */
static lv_obj_t*   s_snap_arrow      = nullptr;
static uint32_t    s_last_rendered_snap_age = 0xFFFFFFFF;

/* Flash tracking para key/chord events */
static uint32_t    s_key_flash_ms    = 0;  ///< timestamp del flash de key lock
static uint32_t    s_chord_flash_ms  = 0;  ///< timestamp del flash de chord detection
static uint8_t     s_prev_key_idx    = 0xFF;
static uint8_t     s_prev_chord_root = 0xFF;

static lv_timer_t* s_prog_timer      = nullptr;

/* ── Helpers privados ─────────────────────────────────────────────────── */

/* Intervalos de escala mayor y menor natural (en semitonos desde root) */
static const uint8_t MAJOR_INTERVALS[7] = {0, 2, 4, 5, 7, 9, 11};
static const uint8_t MINOR_INTERVALS[7] = {0, 2, 3, 5, 7, 8, 10};

/**
 * Retorna true si pitch class `pc` (0-11) es diatónico en `key_idx` (0-23).
 * key_idx 0-11 = mayor (C MAJ..B MAJ), key_idx 12-23 = menor (C MIN..B MIN).
 */
static bool note_is_in_key(uint8_t pc, uint8_t key_idx) {
    if (key_idx == 0xFF || pc >= 12) return false;
    bool is_minor   = (key_idx >= 12);
    uint8_t root    = is_minor ? (key_idx - 12) : key_idx;
    const uint8_t* intervals = is_minor ? MINOR_INTERVALS : MAJOR_INTERVALS;
    for (int i = 0; i < 7; i++) {
        if (((root + intervals[i]) % 12) == pc) return true;
    }
    return false;
}

/**
 * Llena `out[3]` con los intervalos de la triada base del quality dado.
 * Usado para los flashes de chord (Batch D).
 * Si quality == 5 (N) o 0xFF, out[0] = 0xFF (flash oculto).
 */
static void chord_intervals(uint8_t qual, uint8_t out[3]) {
    switch (qual) {
        case 0: out[0]=0; out[1]=4; out[2]=7; break;  /* maj */
        case 1: out[0]=0; out[1]=3; out[2]=7; break;  /* min */
        case 2: out[0]=0; out[1]=4; out[2]=7; break;  /* 7 — triada base */
        case 3: out[0]=0; out[1]=3; out[2]=7; break;  /* m7 — triada base */
        case 4: out[0]=0; out[1]=4; out[2]=7; break;  /* maj7 — triada base */
        default: out[0]=0xFF; out[1]=0xFF; out[2]=0xFF; break;
    }
}

/* ── Timer hero (50ms) ────────────────────────────────────────────────── */

static void ai_hero_timer_cb(lv_timer_t* /*t*/) {
    uint8_t key_idx    = bridge_get_ai_key_idx();
    uint8_t chord_root = bridge_get_ai_chord_root();
    uint8_t chord_qual = bridge_get_ai_chord_quality();
    uint16_t bpm       = bridge_get_ai_bpm();
    uint32_t now       = lv_tick_get();
    uint8_t ordering   = bridge_get_chromagram_ordering();

    const char* chord  = bridge_get_ai_chord_name();
    const char* key    = bridge_get_ai_key_name();

    /* Actividad total para idle detection */
    uint32_t sum_activity = 0;
    for (int i = 0; i < 12; i++) sum_activity += bridge_get_pitch_activity((uint8_t)i);
    if (sum_activity > 100) s_last_active_ms = now;
    bool idle = ((now - s_last_active_ms) > 5000);

    /* Detectar cambios de key y chord para disparar flashes */
    if (key_idx != 0xFF && key_idx != s_prev_key_idx) {
        s_key_flash_ms = now;
    }
    s_prev_key_idx = key_idx;

    if (chord_root != 0xFF && chord_root != s_prev_chord_root) {
        s_chord_flash_ms = now;
    }
    s_prev_chord_root = chord_root;

    /* Selección de tabla de labels según ordenamiento activo */
    const char* const* labels = (ordering == 1) ? NOTE_LABELS_COF : NOTE_LABELS_CHROMATIC;

    /* ── Capa A: chromagram Camelot ─────────────────────────────────── */

    /* Calcular si hay flash activo de chord (200ms) */
    bool chord_flash_active = (now - s_chord_flash_ms) < 200;
    uint8_t chord_flash_pcs[3] = {0xFF, 0xFF, 0xFF};
    if (chord_flash_active && chord_root != 0xFF) {
        uint8_t ivs[3];
        chord_intervals(chord_qual, ivs);
        if (ivs[0] != 0xFF) {
            chord_flash_pcs[0] = (chord_root + ivs[0]) % 12;
            chord_flash_pcs[1] = (chord_root + ivs[1]) % 12;
            chord_flash_pcs[2] = (chord_root + ivs[2]) % 12;
        }
    }

    /* Calcular si hay flash activo de key lock (400ms) */
    bool key_flash_active = (now - s_key_flash_ms) < 400;

    for (int p = 0; p < 12; p++) {
        /* p = posición visual; pc = pitch class correspondiente */
        uint8_t pc = position_to_pc((uint8_t)p);

        /* Color Camelot fijo para el segmento: en modo CoF la posición visual
         * p coincide directamente con la posición Camelot. En modo cromático,
         * p == pc y hay que buscar la posición CoF para el color. */
        uint8_t camelot_pos = (ordering == 1) ? (uint8_t)p : PC_TO_COF_POS[pc];
        lv_color_t seg_color = gf_camelot_color(camelot_pos);

        /* Opacidad base por diatonismo + actividad real
         *
         * Baselines más bajos que antes porque el color Camelot ya diferencia
         * visualmente incluso a baja opacidad.
         *   idle / sin key: 80 (todos tenues, paleta completa visible)
         *   diatónico:      150 (siempre visible en color saturado)
         *   cromático:       60 (visible pero más tenue)
         * Boost diatónico +60 adicional durante key_flash (400ms post-lock).
         */
        uint8_t min_opa;
        if (key_idx == 0xFF) {
            min_opa = 80;
        } else {
            bool is_diatonic = note_is_in_key(pc, key_idx);
            min_opa = is_diatonic ? 150 : 60;
            if (is_diatonic && key_flash_active) min_opa = (uint8_t)((uint16_t)min_opa + 60 > 255 ? 255 : min_opa + 60);
        }

        uint8_t act  = bridge_get_pitch_activity(pc);
        float norm   = (float)act / 255.0f;
        float boost  = powf(norm, 0.6f);
        uint8_t seg_opa = (uint8_t)(min_opa + (uint32_t)(255 - min_opa) * boost);

        /* Flash chord: los 3 pcs de la triada suben a 255 durante 200ms */
        if (chord_flash_active) {
            for (int j = 0; j < 3; j++) {
                if (chord_flash_pcs[j] == pc) { seg_opa = 255; break; }
            }
        }

        if (s_chroma_seg[p]) {
            lv_obj_set_style_arc_color(s_chroma_seg[p], seg_color, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(s_chroma_seg[p], seg_color, LV_PART_MAIN);
            lv_obj_set_style_opa(s_chroma_seg[p], seg_opa, 0);
        }

        /* Note labels: reactivos al estado de nota activa.
         *
         * bridge_get_note_active() + bridge_get_note_midi() dan respuesta
         * INSTANTÁNEA (event-driven NOTE_ON/OFF), sin depender del EMA lento.
         *   - Nota presionada:   opa=255, color Camelot de ese pitch class
         *   - Diatónica en reposo: opa=160, blanco
         *   - No diatónica:        opa=80, blanco
         *   - Sin key detectada:   opa=120, blanco (igual para todas)
         */
        if (s_note_lbl[p]) {
            lv_label_set_text_static(s_note_lbl[p], labels[p]);
            bool is_active_note = bridge_get_note_active() &&
                                  ((bridge_get_note_midi() % 12) == pc);
            if (is_active_note) {
                /* Color Camelot de la nota presionada + full brightness */
                uint8_t note_cof = (ordering == 1) ? (uint8_t)p : PC_TO_COF_POS[pc];
                lv_obj_set_style_text_color(s_note_lbl[p], gf_camelot_color(note_cof), 0);
                lv_obj_set_style_opa(s_note_lbl[p], 255, 0);
            } else {
                lv_obj_set_style_text_color(s_note_lbl[p], lv_color_white(), 0);
                uint8_t lbl_opa;
                if (key_idx == 0xFF) {
                    lbl_opa = 120;   /* sin key: todas iguales, legibles */
                } else {
                    bool is_diat = note_is_in_key(pc, key_idx);
                    lbl_opa = is_diat ? 200 : 80;  /* diatónica bright, cromática dim */
                }
                lv_obj_set_style_opa(s_note_lbl[p], lbl_opa, 0);
            }
        }
    }

    /* Tonic anchor — visible solo cuando hay key */
    if (s_tonic_anchor) {
        if (key_idx == 0xFF) {
            lv_obj_set_style_opa(s_tonic_anchor, LV_OPA_TRANSP, 0);
        } else {
            uint8_t tonic_pc = key_idx % 12;
            uint8_t vis_pos = tonic_pc;
            if (ordering == 1) {
                for (uint8_t p = 0; p < 12; p++) {
                    if (COF_POS_TO_PC[p] == tonic_pc) { vis_pos = p; break; }
                }
            }
            float angle_rad = (270.0f + vis_pos * 30.0f) * (float)M_PI / 180.0f;
            lv_coord_t dx = (lv_coord_t)lroundf(115.0f * cosf(angle_rad));
            lv_coord_t dy = (lv_coord_t)lroundf(115.0f * sinf(angle_rad));
            lv_obj_align(s_tonic_anchor, LV_ALIGN_CENTER, dx, dy);
            lv_obj_set_style_opa(s_tonic_anchor, LV_OPA_COVER, 0);
        }
    }

    /* ── Snap arrow ephemeral 500ms ──────────────────────────────────── */
    {
        uint8_t snap_from, snap_to;
        uint32_t snap_age_ms;
        bridge_get_last_snap(&snap_from, &snap_to, &snap_age_ms);

        bool is_new_snap = (snap_age_ms < 200) &&
                           (snap_age_ms < s_last_rendered_snap_age);
        s_last_rendered_snap_age = (snap_age_ms == 0xFFFFFFFF) ? 0xFFFFFFFF : snap_age_ms;

        if (is_new_snap) {
            uint8_t from_pc = snap_from % 12;
            uint8_t to_pc   = snap_to   % 12;
            uint8_t from_vis = from_pc, to_vis = to_pc;
            if (ordering == 1) {
                for (uint8_t p = 0; p < 12; p++) {
                    if (COF_POS_TO_PC[p] == from_pc) { from_vis = p; break; }
                }
                for (uint8_t p = 0; p < 12; p++) {
                    if (COF_POS_TO_PC[p] == to_pc) { to_vis = p; break; }
                }
            }
            /* r=78: entre note labels (r=88) y centro */
            float from_rad = ((270 + from_vis * 30) % 360) * (float)M_PI / 180.0f;
            float to_rad   = ((270 + to_vis   * 30) % 360) * (float)M_PI / 180.0f;
            lv_coord_t fx = 120 + (lv_coord_t)(78.0f * cosf(from_rad));
            lv_coord_t fy = 120 + (lv_coord_t)(78.0f * sinf(from_rad));
            lv_coord_t tx = 120 + (lv_coord_t)(78.0f * cosf(to_rad));
            lv_coord_t ty = 120 + (lv_coord_t)(78.0f * sinf(to_rad));

            if (s_snap_arrow) {
                lv_anim_del(s_snap_arrow, nullptr);
                lv_obj_del(s_snap_arrow);
                s_snap_arrow = nullptr;
            }

            static lv_point_t arrow_pts[2];
            arrow_pts[0].x = fx; arrow_pts[0].y = fy;
            arrow_pts[1].x = tx; arrow_pts[1].y = ty;

            s_snap_arrow = lv_line_create(lv_scr_act());
            lv_line_set_points(s_snap_arrow, arrow_pts, 2);
            lv_obj_set_style_line_color(s_snap_arrow, GF_COLOR_TEAL_PLUS, 0);
            lv_obj_set_style_line_width(s_snap_arrow, 3, 0);
            lv_obj_set_style_line_opa(s_snap_arrow, LV_OPA_COVER, 0);

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, s_snap_arrow);
            lv_anim_set_exec_cb(&a, [](void* v, int32_t opa) {
                lv_obj_set_style_line_opa((lv_obj_t*)v, (lv_opa_t)opa, 0);
            });
            lv_anim_set_values(&a, 255, 0);
            lv_anim_set_time(&a, 500);
            lv_anim_set_ready_cb(&a, [](lv_anim_t* an) {
                lv_obj_del((lv_obj_t*)an->var);
                s_snap_arrow = nullptr;
            });
            lv_anim_start(&a);

            /* Flash del segmento snap_from con rojo (boost visual 100ms) */
            uint8_t from_camelot_pos = (ordering == 1) ? from_vis : PC_TO_COF_POS[from_pc];
            (void)from_camelot_pos;  /* usado indirectamente por seg_color en próximo tick */
            if (s_chroma_seg[from_vis]) {
                lv_obj_set_style_arc_color(s_chroma_seg[from_vis], GF_COLOR_RED, LV_PART_INDICATOR);
                lv_obj_set_style_arc_color(s_chroma_seg[from_vis], GF_COLOR_RED, LV_PART_MAIN);
                lv_obj_set_style_opa(s_chroma_seg[from_vis], 255, 0);
            }
        }
    }

    /* ── Capa C: Chord card hero ─────────────────────────────────────── */

    /* Chord card: prioridad chord > key > "—" */
    if (s_chord_card) {
        if (!idle && chord[0] != '-') {
            lv_label_set_text(s_chord_card, chord);
        } else if (key[0] != '-') {
            lv_label_set_text(s_chord_card, key);
        } else {
            lv_label_set_text(s_chord_card, "\xe2\x80\x94");  /* em dash */
        }
    }

    /* Key chip: contexto del chord card */
    if (s_key_chip) {
        if (!idle && chord[0] != '-' && key[0] != '-') {
            static char key_buf[20];
            snprintf(key_buf, sizeof(key_buf), "in %s", key);
            lv_label_set_text(s_key_chip, key_buf);
        } else {
            lv_label_set_text(s_key_chip, "");
        }
    }

    /* BPM chip — top of screen */
    if (s_bpm_chip) {
        if (idle || bpm == 0) {
            lv_label_set_text(s_bpm_chip, "\xe2\x80\x94");  /* em dash */
        } else {
            static char bpm_buf[12];
            snprintf(bpm_buf, sizeof(bpm_buf), "%u BPM", (unsigned)bpm);
            lv_label_set_text(s_bpm_chip, bpm_buf);
        }
    }

    /* ── Capa D: AI status line (priority state machine) ───────────── */
    if (s_ai_status_lbl) {
        uint8_t snap_from_n, snap_to_n;
        uint32_t snap_age;
        bridge_get_last_snap(&snap_from_n, &snap_to_n, &snap_age);

        static char status_buf[48];
        const char* status_str = "";
        lv_color_t  status_col = GF_COLOR_GRAY;

        if (snap_age < 2000) {
            /* SNAP tiene la prioridad más alta — visible 2s post-evento */
            snprintf(status_buf, sizeof(status_buf), "> SNAP %s->%s",
                     NOTES[snap_from_n % 12], NOTES[snap_to_n % 12]);
            status_str = status_buf;
            status_col = GF_COLOR_TEAL_PLUS;
        } else if (!idle && chord[0] != '-') {
            snprintf(status_buf, sizeof(status_buf), "> CHORD %s", chord);
            status_str = status_buf;
            status_col = GF_COLOR_TEAL_PLUS;
        } else if (key[0] != '-') {
            snprintf(status_buf, sizeof(status_buf), "> KEY LOCKED %s", key);
            status_str = status_buf;
            status_col = GF_COLOR_TEAL_PLUS;
        } else if (sum_activity > 0) {
            status_str = "* DETECTING KEY...";
            status_col = GF_COLOR_GRAY;
        } else if (idle) {
            status_str = "o Play notes to engage AI";
            status_col = GF_COLOR_GRAY;
        }
        /* else: sin actividad pero no idle todavía → vacío */

        lv_label_set_text(s_ai_status_lbl, status_str);
        lv_obj_set_style_text_color(s_ai_status_lbl, status_col, 0);
    }
}

void view_10_create(lv_obj_t* parent) {
    /* Limpiar estado de un rebuild anterior. */
    for (int i = 0; i < 12; i++) { s_chroma_seg[i] = nullptr; s_note_lbl[i] = nullptr; }
    s_tonic_anchor   = nullptr;
    s_core_pulse     = nullptr;
    s_chord_card     = nullptr;
    s_key_chip       = nullptr;
    s_bpm_chip       = nullptr;
    s_ai_status_lbl  = nullptr;
    s_snap_arrow     = nullptr;
    s_prog_timer     = nullptr;
    s_last_active_ms            = lv_tick_get();
    s_last_rendered_snap_age    = 0xFFFFFFFF;
    s_key_flash_ms              = 0;
    s_chord_flash_ms            = 0;
    s_prev_key_idx              = bridge_get_ai_key_idx();
    s_prev_chord_root           = bridge_get_ai_chord_root();

    gf_screen_bg(parent);
    /* Sin gf_arc_title ni gf_mode_pill — vista pura, identidad por chromagram */

    /* ═══════════════════════════════════════════════════════════════════
     * Capa A — Chromagram 12 segmentos Camelot (outer ring, 220×220, stroke 16px)
     *
     * Posición visual p → posición angular: C en top (270°), +30° por posición.
     * Color: gf_camelot_color(p) en modo CoF (posición visual == posición Camelot).
     * El color se actualiza en el timer junto con la opacidad — en create() se
     * inicializa con el color base; en el primer tick se aplica el color correcto
     * según ordering (que puede ser cromático).
     * Span 24°, gap 6° entre segmentos.
     * ═══════════════════════════════════════════════════════════════════ */
    for (int p = 0; p < 12; p++) {
        lv_obj_t* seg = lv_arc_create(parent);
        lv_obj_set_size(seg, 220, 220);
        lv_obj_center(seg);
        lv_arc_set_rotation(seg, 0);

        int center_deg = (270 + p * 30) % 360;
        int seg_start  = (center_deg - 12 + 360) % 360;
        int seg_end    = (center_deg + 12) % 360;

        lv_arc_set_bg_angles(seg,  (uint16_t)seg_start, (uint16_t)seg_end);
        lv_arc_set_angles(seg,     (uint16_t)seg_start, (uint16_t)seg_end);
        lv_arc_set_value(seg, 100);
        lv_arc_set_range(seg, 0, 100);
        lv_obj_remove_style(seg, nullptr, LV_PART_KNOB);
        lv_obj_set_style_arc_width(seg, 16, LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(seg, 16, LV_PART_MAIN);

        /* Color Camelot inicial (en modo CoF pos == índice Camelot) */
        lv_color_t c = gf_camelot_color((uint8_t)p);
        lv_obj_set_style_arc_color(seg, c, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(seg, c, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(seg, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_clear_flag(seg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(seg, 80, 0);  /* idle baseline */
        s_chroma_seg[p] = seg;
    }

    /* ═══════════════════════════════════════════════════════════════════
     * Note labels — 12 lv_label en r=88, color BLANCO fijo opa=200
     *
     * La opacidad está desacoplada del segmento para garantizar legibilidad
     * independientemente del color Camelot del fondo (algunos son oscuros).
     * ═══════════════════════════════════════════════════════════════════ */
    for (int p = 0; p < 12; p++) {
        lv_obj_t* lbl = lv_label_create(parent);
        /* GF_FONT_MICRO (8px) — verificado que cabe en el segmento de 18px stroke */
        lv_obj_set_style_text_font(lbl, GF_FONT_MICRO, 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_label_set_text(lbl, "C");
        int center_deg = (270 + p * 30) % 360;
        float rad = (float)center_deg * (float)M_PI / 180.0f;
        lv_coord_t x = (lv_coord_t)(88.0f * cosf(rad) + 0.5f);
        lv_coord_t y = (lv_coord_t)(88.0f * sinf(rad) + 0.5f);
        lv_obj_align(lbl, LV_ALIGN_CENTER, x, y);
        lv_obj_set_style_opa(lbl, 200, 0);
        s_note_lbl[p] = lbl;
    }

    /* Tonic anchor — dot 5px en r=115 sobre el segmento tónico */
    s_tonic_anchor = gf_dot(parent, 5, GF_COLOR_WHITE);
    lv_obj_center(s_tonic_anchor);
    lv_obj_set_style_opa(s_tonic_anchor, LV_OPA_TRANSP, 0);

    /* ═══════════════════════════════════════════════════════════════════
     * Capa B — Core pulse (Batch F: simplificado a 5px, más sutil)
     *
     * Eliminamos el bar sweep (confuso) y la triada (Batch F).
     * El core pulse se mantiene como único indicador de vida del BPM.
     * ═══════════════════════════════════════════════════════════════════ */
    s_core_pulse = gf_dot(parent, 5, GF_COLOR_TEAL_PLUS);
    lv_obj_center(s_core_pulse);
    lv_anim_t ca;
    lv_anim_init(&ca);
    lv_anim_set_var(&ca, s_core_pulse);
    lv_anim_set_exec_cb(&ca, core_size_cb);
    lv_anim_set_values(&ca, 5, 8);   /* más pequeño que antes (era 8→14) */
    lv_anim_set_time(&ca, 700);
    lv_anim_set_playback_time(&ca, 700);
    lv_anim_set_repeat_count(&ca, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&ca);

    /* ═══════════════════════════════════════════════════════════════════
     * BPM chip — top of screen (fuera del área del chord card central)
     *
     * Posicionado en el top-center del display (y≈18 desde el borde),
     * dentro del hueco superior del chromagram ring.
     * Sprint 34 Polish: movido desde el centro (era y=+62) al top
     * para liberar espacio en el card central y ser más escaneable.
     * ═══════════════════════════════════════════════════════════════════ */
    s_bpm_chip = lv_label_create(parent);
    lv_obj_set_style_text_font(s_bpm_chip, GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(s_bpm_chip, GF_COLOR_GRAY, 0);
    lv_obj_set_style_text_align(s_bpm_chip, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_bpm_chip, "\xe2\x80\x94");
    lv_obj_align(s_bpm_chip, LV_ALIGN_TOP_MID, 0, 18);

    /* ═══════════════════════════════════════════════════════════════════
     * Capa C — Chord card hero al centro (Batches C + E, Polish Sprint 34)
     *
     * Layout vertical centrado. Sin Camelot chip (removido en Polish —
     * la rueda de colores ya comunica la información armónica visualmente).
     *   s_chord_card   → y=−12 (GF_FONT_HERO 36px, blanco)
     *   s_key_chip     → y=+22 (GF_FONT_MICRO, gray)
     * ═══════════════════════════════════════════════════════════════════ */
    s_chord_card = gf_hero_label(parent, "\xe2\x80\x94", GF_FONT_HERO, GF_COLOR_WHITE, -12);

    s_key_chip = gf_hero_label(parent, "", GF_FONT_MICRO, GF_COLOR_GRAY, 22);

    /* ═══════════════════════════════════════════════════════════════════
     * Capa D — AI status line (Batch D, Polish: subida desde y=+85 a +48)
     *
     * Más cerca del chord card. Teal = evento activo, gray = idle.
     * Caracteres ASCII (> *) en lugar de UTF-8 — ibm_plex_mono_8 puede
     * no incluir esos glifos Unicode.
     * ═══════════════════════════════════════════════════════════════════ */
    s_ai_status_lbl = gf_hero_label(parent, "", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS, 48);

    /* Hero timer 50ms */
    s_prog_timer = lv_timer_create(ai_hero_timer_cb, 50, nullptr);
}

void view_10_destroy(void) {
    if (s_prog_timer) {
        lv_timer_del(s_prog_timer);
        s_prog_timer = nullptr;
    }
    if (s_core_pulse) lv_anim_del(s_core_pulse, core_size_cb);

    /* snap_arrow puede vivir fuera de la jerarquía del carousel */
    if (s_snap_arrow) {
        lv_anim_del(s_snap_arrow, nullptr);
        lv_obj_del(s_snap_arrow);
        s_snap_arrow = nullptr;
    }

    /* Nulificar punteros — el carousel limpia los widgets con lv_obj_clean */
    for (int i = 0; i < 12; i++) { s_chroma_seg[i] = nullptr; s_note_lbl[i] = nullptr; }
    s_tonic_anchor  = nullptr;
    s_core_pulse    = nullptr;
    s_chord_card    = nullptr;
    s_key_chip      = nullptr;
    s_bpm_chip      = nullptr;
    s_ai_status_lbl = nullptr;
}
