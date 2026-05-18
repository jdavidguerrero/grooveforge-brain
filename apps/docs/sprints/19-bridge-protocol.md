# Sprint 3.2 — UART Bridge Protocol Teensy ↔ ESP32-S3

> **Fase:** 3 — UI + Display
> **Estado:** CERRADO ✓ · Mayo 2026
> **Depende de:** Sprint 3.1 (`17-esp32-display.md` — ESP32-S3 + LVGL operativo)
> **Referencias:** `apps/docs/02-bridge-protocol.md` · `01-architecture.md §3.3, §5.2`

---

## Objetivo

Implementar el **protocolo binario UART** entre el Teensy 4.1 (master) y el ESP32-S3
(slave), de modo que cuando el Teensy cambie el engine activo, el ESP32 lo reciba y
pueda actualizar la UI.

Sin este protocolo, el carrusel de vistas (Sprint 3.4) muestra datos fijos — el
display no reacciona al hardware. Sprint 3.2 es el tubo de datos entre ambos procesadores.

**No requiere Teensy físico conectado.** El header compartido, el parser del slave y
los tests nativos son C++ puro verificable con `pio test -e native`. La demo usa un
script Python que inyecta frames vía USB-UART al ESP32.

**Criterio de pass:** `pio test -e native` pasa todos los tests de `test_bridge/`;
el monitor serial del ESP32 muestra el log correcto al recibir un frame `ENGINE_CHANGED`
enviado desde el script Python.

---

## Teoría

### 1. Por qué 921600 baud — y no un número redondo

La velocidad de UART no se elige libre: el transmisor genera el baud clock dividiendo
un oscillator de referencia por un divisor entero. El ESP32-S3 y el Teensy 4.1 usan
oscillators de **40 MHz** y **24 MHz** respectivamente, ambos con divisores que producen
921600 sin error de framing.

```
ESP32-S3:  40,000,000 / 4 / 10.85... ≈ no exacto — IDF usa PLL 80 MHz / 86 ≈ 930,232 baud
           en la práctica: el UART del ESP32-S3 acepta 921600 con error <1%, dentro del
           10% de tolerancia de UART standard.
Teensy 4.1: 600 MHz PLL / 651 ≈ 921,659 baud (error 0.006% — prácticamente exacto).
```

La razón real de 921600 vs 115200: **8× más throughput** con el mismo par de wires.
A 921600 baud, 1 frame máximo (4 bytes header + 255 payload + 1 CRC = 260 bytes × 10 bits)
tarda **2.82 ms**. Esto permite >340 frames/s sostenidos, más que suficiente para el
target de `<2ms p95 round-trip` del spec — el overhead de protocolo (header + CRC) no
domina la latencia.

A 115200, ese mismo frame tardría **22.6 ms** — inaceptable para el round-trip target.

**Referencia:** `02-bridge-protocol.md §1.3`, `01-architecture.md §5.2`.

### 2. CRC-8 poly 0x07 — por qué este checksum

Un checksum de 1 byte (paridad simple o suma) detecta errores de bit individuales pero
falla con errores de ráfaga (burst errors): si 2 bits consecutivos se corrompen, la suma
puede quedar igual. Un CRC detecta todos los bursts de hasta `n-1` bits donde `n` es el
grado del polinomio.

CRC-8 con polinomio `0x07` (`x⁸ + x² + x + 1`, Dallas/Maxim, llamado CRC-8/SMBUS):
- Detecta todos los errores de 1 bit en cualquier posición del frame
- Detecta todos los errores de ráfaga ≤7 bits
- Overhead: **1 byte** por frame — mínimo posible

Por qué no CRC-16: el frame tiene payload máximo de 255 bytes = 2040 bits. La probabilidad
de error no detectado con CRC-8 sobre 2040 bits es 1/256 ≈ 0.4%. CRC-16 lo reduciría a
1/65536, pero agrega 1 byte de overhead. A 921600 baud × 1000 frames/s = 8 KB/s de
overhead adicional. Para un link UART local (sin canal ruidoso como RF), CRC-8 es
el balance correcto: el UART ya detecta errores de framing via el stop bit.

**Algoritmo (spec §6):**
```cpp
uint8_t gf_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
        }
    }
    return crc;
}
```

**Vector canónico** (CRC-8/SMBUS, Wikipedia / Ross Williams "A Painless Guide to CRC"):
`gf_crc8("123456789", 9) == 0xF4`

**Referencia:** Williams, R. (1993) "A Painless Guide to CRC Error Detection Algorithms",
Version 3. Disponible en https://www.ross.net/crc/download/crc_v3.txt. La tabla §15
lista el check value 0xF4 para CRC-8/SMBUS con el input estándar "123456789".

### 3. State machine de recepción byte-a-byte

El patrón naive de leer un frame es `Serial.readBytes(buf, frame_size)` — bloqueante
hasta que llegan todos los bytes. En un firmware con LVGL, ese bloqueo puede durar
hasta `timeout_ms` (100ms por defecto). Durante ese tiempo `lv_task_handler()` no corre:
las animaciones se congelan, los timers del carrusel no avanzan.

La solución: una **state machine** que procesa **un byte por vez** en cada llamada a
`poll()`. `poll()` se llama desde `loop()` entre el `lv_task_handler()` y el `delay(5)`.
Si no hay bytes disponibles, retorna inmediatamente (~1µs). Si llegan bytes, los procesa
en la iteración del loop en que llegaron.

```
IDLE ─── 1 byte → CMD ──────────────────────────▶ GOT_CMD
GOT_CMD ── 1 byte → LEN ────────────────────────▶ GOT_LEN
GOT_LEN ── 1 byte → SEQ ────────────────────────▶ GOT_SEQ
GOT_SEQ ── (si LEN=0) ──────────────────────────▶ AWAIT_CRC
GOT_SEQ ── (si LEN>0, N bytes) ─────────────────▶ READING_PAYLOAD → AWAIT_CRC
AWAIT_CRC ── 1 byte → CRC; validar → dispatch ──▶ IDLE
```

Si CRC no coincide: `send_nack(seq, NACK_CRC_ERROR)` y vuelve a IDLE.
Si `CMD` es inválido: `send_nack(seq, NACK_INVALID_CMD)` y vuelve a IDLE.
Cualquier timeout parcial (frame incompleto que tarda >100ms): reset a IDLE.

Este patrón es el mismo que usan protocolos embebidos como MODBUS RTU y COBS-encoded
UARTs para coexistir con un task scheduler cooperativo.

### 4. Heartbeat como keepalive — por qué 1 Hz y 3s timeout

El Teensy envía `HEARTBEAT (0x00)` cada segundo. El ESP32 monitorea el tiempo desde
el último heartbeat recibido; si supera **3 segundos** (= 3 missed heartbeats), marca
la conexión como DOWN.

¿Por qué 3s y no 1s? Un heartbeat puede perderse por:
- Glitch de alimentación momentáneo que resetea el Teensy
- Buffer UART lleno (backpressure)
- Firmware actualización en vuelo

Un timeout de 1s causaría falsos positivos. 3s (≥3 missed heartbeats) es la heurística
de muchos protocolos industriales (Modbus, CANopen heartbeat) para distinguir entre
"ruido transitorio" y "dispositivo perdido".

El ESP32 no reintenta conexión activamente — espera que el Teensy envíe el próximo
heartbeat. Esto es intencional: el Teensy es el master del protocolo.

### 5. ACK/NACK y patrones async vs sync

El spec define dos patrones:

**Async (notificación):** `ENGINE_CHANGED`, `PARAM_CHANGED`, `NOTE_ON/OFF`. El sender
envía y no espera ACK. El receiver procesa en background. Adecuado cuando el dato
tiene valor efímero — si se pierde un `PARAM_CHANGED`, el siguiente lo sobreescribe.

**Sync (request-response):** `SET_ENGINE`, `SET_PARAM`, `GET_VERSION`. El sender espera
ACK (timeout 100ms UART) antes de considerar el comando enviado. Adecuado para acciones
de estado que deben confirmarse — "¿cambié el engine?" necesita confirmación.

En el ESP32 como slave, **todos los comandos reciben ACK** (o NACK si hay error).
Esto simplifica el master: siempre puede asumir que si recibe ACK el comando fue procesado.
Los comandos async como `ENGINE_CHANGED` aún reciben ACK — el master simplemente no
bloquea esperando.

**Referencia:** `02-bridge-protocol.md §3`, `§4.2`.

---

## Contradicciones de spec detectadas

| # | Contradicción | Resolución |
|---|---|---|
| 1 | El roadmap (06.md) lista Sprint 3.2 antes de 3.4, pero 3.4 se implementó primero (display disponible sin hardware de input) | Reorden pragmático — el carrusel sigue funcionando autónomo; este sprint agrega el canal de datos real sin romper nada |
| 2 | `01-architecture.md §3.3` dice ESP32 GPIO 43=TX / 44=RX; `02-bridge-protocol.md §1.3` no especifica los pines (solo baud y formato) | Usar §3.3 de architecture como autoritativo para pines. `Serial1.begin(921600, SERIAL_8N1, 44, 43)` (RX=44, TX=43) |

---

## Implementation

### Estructura de archivos

```
apps/bridge-protocol/include/protocol.h    ← header shared
apps/firmware-esp32/
├── platformio.ini                          ← -I ../../bridge-protocol/include en [env:native]
├── test/test_bridge/
│   ├── test_crc8.cpp
│   ├── test_serialization.cpp
│   └── test_error_handling.cpp
└── src/bridge/
    ├── bridge_slave.h/.cpp                 ← state machine, ACK/NACK, heartbeat
    └── bridge_handlers.h/.cpp             ← handlers específicos de display/engine
apps/firmware-teensy/src/bridge/
    ├── bridge_master.h/.cpp               ← skeleton vacío
apps/docs/sprints/19-bridge-protocol.md    ← este doc
tools/bridge-test/send_frame.py            ← script demo
```

### Frame wire format

```
Bytes en el cable (orden): CMD | LEN | SEQ | PAYLOAD[0..LEN-1] | CRC8
CRC8 cubre: CMD + LEN + SEQ + PAYLOAD[0..LEN-1] (no el CRC mismo)
```

### Comandos implementados en este sprint

Los 11 comandos listados en el plan (`0x00–0x14`, `0x40`, `0x50`). El resto de comandos
(FX, MIDI, Cloud, DAW, Pogo, ML) reciben ACK genérico sin handler específico.

### Cambios en `lv_conf.h` / `platformio.ini`

Ninguno en `lv_conf.h`. En `platformio.ini`: agregar
`-I ../../bridge-protocol/include` solo al `[env:native]` (el `[env:esp32s3]` compila
`bridge_slave.cpp` que incluye `protocol.h` via ruta relativa desde `src/`).

---

## Demo / criterios de aceptación

1. `pio test -e native` desde `apps/firmware-esp32/` → `test_bridge/test_crc8`,
   `test_serialization`, `test_error_handling` pasan todos.
2. `pio run -e esp32s3` → compila sin warnings nuevos.
3. Script Python `tools/bridge-test/send_frame.py --cmd ENGINE_CHANGED --engine 0 --name "MOOG MODEL D"`:
   - El monitor serial del ESP32 muestra: `[bridge] ENGINE_CHANGED — id=0 name=MOOG MODEL D`
4. Script envía HEARTBEAT cada 1s durante 5s, luego se detiene:
   - A los 3s: `[bridge] connection DOWN — heartbeat timeout`

---

## Learnings

### `${PROJECT_DIR}` en PlatformIO native resuelve al workspace root, no al project dir

El env `[env:native]` necesita la ruta de include como `-I ../bridge-protocol/include`
(relativa a donde corre el compilador, que es `apps/firmware-esp32/`). El intento con
`${PROJECT_DIR}/../../bridge-protocol/include` resolvió mal porque PlatformIO toma
el workspace root del monorepo Nx como `PROJECT_DIR`. La regla: usar rutas relativas
simples en `build_flags` de native.

El mismo include (`-I ../bridge-protocol/include`) aplica también al env `esp32s3`
para que `bridge_slave.cpp` / `bridge_handlers.cpp` encuentren `protocol.h`.

### PlatformIO Unity: un directorio = un binario (no múltiples `main()`)

Poner `test_crc8.cpp`, `test_serialization.cpp` y `test_error_handling.cpp` en el
mismo directorio `test_bridge/` causó "duplicate symbol _main". PlatformIO compila
todo un directorio en un único ejecutable. Solución: un subdirectorio por suite:
`test_bridge_crc8/`, `test_bridge_serialization/`, `test_bridge_error/`.

### `uint8_t` como parámetro de `payload_len` trunca 256 → 0 silenciosamente

`gf_frame_build(f, cmd, seq, buf, 256)` con `uint8_t payload_len` pasa la validación
`if (payload_len > 255)` porque 256 & 0xFF = 0. Cambiar a `size_t` expone correctamente
el overflow. Regla general: usar tipos más anchos que el dominio cuando el test de
overflow importa.

### `millis()` no disponible en headers que no incluyen `Arduino.h`

El método `mark_heartbeat()` tenía implementación inline en `bridge_slave.h` y usaba
`millis()`. El compilador no encontró `millis` al compilar `bridge_handlers.cpp` (que
no incluía `Arduino.h` directamente). Solución: declarar en el header, implementar en
el `.cpp` donde `Arduino.h` ya está incluido. Regla: no usar símbolos del runtime
Arduino en implementaciones inline de headers compartidos.

### Separación handler / slave via `void* ctx` es limpia para este caso

El patrón `on_command(cmd, handler_fn, &slave)` donde el contexto es el propio slave
funciona bien porque el slave vive en `main.cpp` con lifetime de la aplicación. El
handler recibe `(const GF_Frame*, void*)` y hace `static_cast<BridgeSlave*>(ctx)`.
Alternativa con `std::function` sería más ergonómica pero agrega 24KB de flash por
el heap de std::function — inaceptable en el ESP32 con el heap apretado de LVGL.

**Sprint 3.2 — CERRADO · Mayo 2026**

*30/30 tests nativos · 15.3% flash / 48.7% RAM · build limpio.*
*Demo: `python3 tools/bridge-test/send_frame.py --cmd ENGINE_CHANGED --engine 0 --name "MOOG MODEL D"`*

---

*Sprint 3.2 — GrooveForge Brain · Juan Guerrero (GPROG)*
