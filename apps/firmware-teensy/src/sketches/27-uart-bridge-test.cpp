/**
 * 27-uart-bridge-test.cpp — Prueba de integración UART Bridge Protocol Teensy↔ESP32.
 *
 * Sprint 6 / Bridge Protocol — valida BridgeMaster contra BridgeSlave del ESP32.
 *
 * Qué hace:
 *   - Heartbeat automático cada 1s via IntervalTimer → ESP32 responde ACK
 *   - ENC NAV gira → cambia engine_id (0-5, wraparound) → send_async ENGINE_CHANGED
 *   - ENC NAV push → send_and_wait GET_VERSION → imprime resultado o timeout
 *   - LED ring: Rojo tenue (sin conexión) | Teal tenue (conectado)
 *   - LED ring activo: posición del engine actual en Purple (igual que sketch26)
 *
 * Wire:
 *   Teensy Serial1 RX=pin0 ← ESP32 GPIO43(TX)
 *   Teensy Serial1 TX=pin1 → ESP32 GPIO44(RX)
 *   921600 baud 8N1
 *
 * Payload ENGINE_CHANGED (cmd 0x12, protocol.h):
 *   byte 0    : engine_id (uint8_t)
 *   bytes 1+  : nombre null-terminated (e.g. "MOOG MODEL D\0")
 *
 * Cómo flashear:
 *   cd apps/firmware-teensy
 *   pio run -e sketch -t upload
 *   pio device monitor -b 115200
 *
 * Cómo interpretar el monitor:
 *   [master] init ...         → BridgeMaster inicializado
 *   [HB]                      → heartbeat enviado (cada 1s)
 *   [HB] ESP32 alive          → primer ACK de heartbeat recibido
 *   [TX] ENGINE_CHANGED id=N  → cambio de engine enviado
 *   [TX] GET_VERSION → OK ... → versión recibida del ESP32
 *   [TX] GET_VERSION → TIMEOUT→ ESP32 no respondió en 100ms
 *   [master] ESP32 DOWN ...   → ESP32 no respondió heartbeat en 3s
 *
 * Ref: apps/docs/02-bridge-protocol.md, apps/docs/sprints/19-bridge-protocol.md
 */

#include <Arduino.h>

#include "bridge/bridge_master.h"
#include "ui/encoders.h"
#include "ui/leds.h"

/* ── Engines ────────────────────────────────────────────────────────────────── */

static const char* ENGINE_NAMES[] = {
    "MOOG MODEL D",
    "JUNO-106",
    "PROPHET-5",
    "OB-6",
    "DX7",
    "ARP 2600",
};
static constexpr uint8_t NUM_ENGINES = 6;

/* ── Colores (01-architecture.md §3.4) ─────────────────────────────────────── */

static const CRGB COLOR_TEAL   = CRGB(0x1D, 0x9E, 0x75);
static const CRGB COLOR_PURPLE = CRGB(0x53, 0x4A, 0xB7);
static const CRGB COLOR_RED    = CRGB(200,  0,    0   );
static const CRGB COLOR_OFF    = CRGB(0,    0,    0   );

/* ── Globals ─────────────────────────────────────────────────────────────────── */

static BridgeMaster bridge;
static Encoders     encoders;
static Leds         leds;

static uint8_t current_engine = 0;

/* IntervalTimer llama a un función global — no puede ser lambda ni método.
 * Usamos un flag para serializar el envío del heartbeat en loop(). */
static volatile bool hb_flag = false;

static IntervalTimer hb_timer;

static void hb_isr() {
    hb_flag = true;
}

/* ── Helpers ─────────────────────────────────────────────────────────────────── */

static void send_engine_changed(uint8_t engine_id) {
    const char* name = ENGINE_NAMES[engine_id];
    uint8_t name_len = (uint8_t)strlen(name) + 1;  // incluye '\0'

    uint8_t payload[1 + 64];                        // engine_id + nombre (max 63 chars + null)
    payload[0] = engine_id;
    memcpy(payload + 1, name, name_len);

    GF_Frame f;
    gf_frame_build(&f, GF_CMD_ENGINE_CHANGED, bridge.next_seq(), payload, 1 + name_len);
    bridge.send_async(f);

    Serial.printf("[TX] ENGINE_CHANGED id=%u name=\"%s\"\n", engine_id, name);
}

static void send_get_version() {
    GF_Frame f;
    gf_frame_build(&f, GF_CMD_GET_VERSION, bridge.next_seq(), nullptr, 0);
    bool ok = bridge.send_and_wait(f, 100);
    if (!ok) {
        Serial.println("[TX] GET_VERSION -> TIMEOUT (ESP32 no respondio en 100ms)");
    }
    /* Si ok=true, el handler de GF_CMD_VERSION ya imprimió la versión. */
}

static void update_ring_leds() {
    bool alive = bridge.is_esp32_alive();

    /* Idle color: Teal tenue (conectado) o Rojo tenue (sin conexión) */
    CRGB idle_dim;
    if (alive) {
        idle_dim = CRGB(COLOR_TEAL.r / 12, COLOR_TEAL.g / 12, COLOR_TEAL.b / 12);
    } else {
        idle_dim = CRGB(COLOR_RED.r / 8, COLOR_RED.g / 8, COLOR_RED.b / 8);
    }

    /* Engine actual mapeado a posición 0-11 del ring */
    uint8_t ring_pos = (current_engine * NUM_RING) / NUM_ENGINES;

    leds.set_ring(ring_pos, COLOR_PURPLE, idle_dim);
    leds.show();
}

/* ── Handlers recibidos del ESP32 ────────────────────────────────────────────── */

static void on_version(const GF_Frame* f, void* /*ctx*/) {
    /* Payload VERSION (cmd 0x04): cadena de versión null-terminated */
    if (f->len > 0) {
        /* Asegurar null-termination antes de imprimir */
        char version_str[GF_PAYLOAD_MAX + 1];
        memcpy(version_str, f->payload, f->len);
        version_str[f->len] = '\0';
        Serial.printf("[TX] GET_VERSION -> OK  version=\"%s\"\n", version_str);
    } else {
        Serial.println("[TX] GET_VERSION -> OK  (payload vacio)");
    }
}

/* ── setup ───────────────────────────────────────────────────────────────────── */

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}

    encoders.init();
    leds.init();

    /* Registrar handler de VERSION antes de init() (init registra ACK interno) */
    bridge.on_command(GF_CMD_VERSION, on_version, nullptr);
    bridge.init();

    /* LED de arranque: ring todo rojo tenue (sin conexión hasta primer HB ACK) */
    CRGB idle_dim = CRGB(COLOR_RED.r / 8, COLOR_RED.g / 8, COLOR_RED.b / 8);
    for (uint8_t i = 0; i < NUM_RING; i++) leds.set(i, idle_dim.r, idle_dim.g, idle_dim.b);
    leds.show();

    /* IntervalTimer: heartbeat cada 1000ms — corre en ISR context, solo setea flag */
    hb_timer.begin(hb_isr, 1000000);  // microsegundos

    Serial.println("\n=== Sprint Bridge Protocol — UART Bridge Test (sketch27) ===");
    Serial.println("Wire: Teensy RX=pin0 <- ESP32 GPIO43  |  TX=pin1 -> ESP32 GPIO44");
    Serial.println("Baud: 921600 8N1");
    Serial.printf("Engines (%u): ", NUM_ENGINES);
    for (uint8_t i = 0; i < NUM_ENGINES; i++) {
        Serial.printf("[%u]%s ", i, ENGINE_NAMES[i]);
    }
    Serial.println();
    Serial.println("ENC NAV gira  → cambia engine (0-5) + send ENGINE_CHANGED");
    Serial.println("ENC NAV push  → send GET_VERSION (bloqueante 100ms timeout)");
    Serial.println("LED ring:  Rojo tenue=sin conexion  |  Teal tenue=conectado");
    Serial.println("LED ring:  Purple=posicion engine actual");
    Serial.println("HB cada 1s via IntervalTimer → ESP32 debe responder ACK");
    Serial.println("Si ESP32 no responde en 3s → DOWN (ring vuelve a Rojo)");
    Serial.println("--------------------------------------------------------------");
}

/* ── loop ────────────────────────────────────────────────────────────────────── */

void loop() {
    bridge.poll();
    encoders.update();

    /* ── Heartbeat (flag seteado por ISR) */
    if (hb_flag) {
        hb_flag = false;
        bridge.send_heartbeat();
        Serial.printf("[HB] seq=%u  esp32_alive=%s\n",
                      bridge.tx_seq() - 1,
                      bridge.is_esp32_alive() ? "YES" : "NO");
    }

    /* ── ENC NAV gira → cambia engine */
    int32_t dn = encoders.read_enc_nav();
    if (dn != 0) {
        /* Wraparound 0-5 en ambas direcciones */
        int32_t next = (int32_t)current_engine + dn;
        next = ((next % NUM_ENGINES) + NUM_ENGINES) % NUM_ENGINES;
        current_engine = (uint8_t)next;

        send_engine_changed(current_engine);
        update_ring_leds();
    }

    /* ── ENC NAV push → GET_VERSION */
    if (encoders.sw_nav_pressed()) {
        Serial.println("[TX] GET_VERSION (send_and_wait, timeout=100ms)");
        send_get_version();
    }

    /* ── Actualizar ring si cambió el estado de conexión
     *    Se re-dibuja en cada ciclo solo cuando cambia _esp32_alive para
     *    reflejar transiciones UP/DOWN sin bloquear el loop. */
    static bool last_alive = false;
    bool alive_now = bridge.is_esp32_alive();
    if (alive_now != last_alive) {
        last_alive = alive_now;
        update_ring_leds();
        if (alive_now) {
            Serial.println("[HB] ESP32 alive — conexion UP");
        }
    }

    delay(5);  // 200Hz poll — consistente con sketch26
}
