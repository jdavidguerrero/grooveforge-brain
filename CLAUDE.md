# GrooveForge Brain — CLAUDE.md

> Reglas de trabajo para Claude Code en este monorepo.
> Leer SIEMPRE antes de proponer código, arquitectura o decisiones.

---

## Filosofía

1. **Educational-first** — cada implementación incluye teoría + código + razón de decisiones.
   No es "que funcione": es "que entiendas por qué funciona".
2. **Spec-first** — ninguna feature sin spec aprobado en `apps/docs/`. El spec precede al código.
3. **Phase gates** — cada fase tiene hito demostrable (audible/visible). Sin demo no se avanza.
4. **One thing at a time** — nunca dos features en paralelo. Una termina, se valida, siguiente.
5. **Vertical slices** — cada sprint entrega algo end-to-end (audio in → audio out).
6. **Time budget honest** — Brain es prioridad #2 (GroovePilot es #1). 10-18h/semana.

---

## Comportamiento de Claude Code

### Antes de proponer cualquier cosa

- Leer los specs relevantes en `apps/docs/` (tabla abajo).
- Cambio que afecta arquitectura → validar contra `apps/docs/01-architecture.md` antes de proponer.
- En ambigüedad → preguntar, no inventar.
- Contradicción entre specs → señalarla, NO resolverla solo.
- Specs `00-06.md` son SSoT inmutables. No modificar sin instrucción explícita.

### Para cada sprint / feature

- Theory doc primero (`apps/docs/sprints/` o `apps/docs/theory/`), después código.
- Explicar el "por qué", no solo el "qué".
- Citar referencias: libros, papers, datasheets (especialmente DSP y analog).
- Comentarios en código: solo cuando el WHY es no-obvio (constraint oculto, workaround de bug
  específico). No comentarios que repiten el nombre de la función.
- Doxygen en declaraciones públicas de headers `.h`.
- Marcar explícitamente: Teensy Audio Library oficial vs código custom.
- Sugerir alternativas con tradeoffs cuando haya decisiones de diseño.

### Anti-patterns

- ❌ Código sin theory doc previo
- ❌ Modificar `apps/docs/00-06.md` sin instrucción explícita
- ❌ Resolver contradicciones entre specs sin consultarlo
- ❌ Scope creep ("y si además…")
- ❌ Optimización prematura
- ❌ Feature que no aporta a ningún nivel del North Star

---

## Mapa de Specs (qué leer para qué pregunta)

| Pregunta | Doc |
|---|---|
| ¿Por qué $599? ¿North Star? ¿Posicionamiento? | `apps/docs/00-master-strategy.md` |
| ¿Hardware? ¿Pin mapping? ¿BOM? ¿Acceptance criteria? | `apps/docs/01-architecture.md` |
| ¿Cómo se comunican Teensy y ESP32? ¿Frame format? | `apps/docs/02-bridge-protocol.md` |
| ¿Filter analog? ¿Calibración? ¿Matching 2N3904? | `apps/docs/03-filter-design.md` |
| ¿Modelos TinyML? ¿Training? ¿Memory budget ML? | `apps/docs/04-ai-architecture.md` |
| ¿Qué efectos hay? ¿CPU FX? ¿Stack técnico FX? | `apps/docs/05-fx-architecture.md` |
| ¿Qué sprint sigue? ¿Hitos de fase? | `apps/docs/06-implementation-roadmap.md` |

### Jerarquía de autoridad en conflictos

1. `00-master-strategy.md` gana en estrategia/negocio
2. `01-architecture.md` gana en contratos técnicos
3. `02–05` son autoritativos en su subsistema
4. `06-implementation-roadmap.md` es living — ajustable sin violar los anteriores

---

## Constraints críticos

Resumen rápido. Verificar números exactos en los specs citados.

### Audio path (inmutable)

```
Teensy 4.1 → I2S nativo → SGTL5000 → 2N3904 ladder → SGTL5000 ADC → USB Audio
```

No proponer arquitectura alternativa sin revisar `01-architecture.md` §2.

### Latencia

| Target | Valor | Fuente |
|---|---|---|
| Audio latencia | **<1ms** determinística | `01-architecture.md` §5.2 |
| USB MIDI → audio out | **<5ms** p95 | `01-architecture.md` §5.2 |
| Bridge round-trip Teensy↔ESP32 | **<2ms** p95 | `02-bridge-protocol.md` §7.3 |
| Bridge command→ack | **<50ms** p95 | `01-architecture.md` §5.2 |

### CPU Teensy 4.1

| Contexto | Valor | Fuente |
|---|---|---|
| Engines @ 6 voces simultáneas | **~30%** | `01-architecture.md` §4.1 |
| 5 FX simultáneos (worst case) | **~45%** | `05-fx-architecture.md` §2 |
| CPU total en uso normal | **≤60%** | `01-architecture.md` §5.2 |

### Memoria Teensy 4.1 (1MB RAM, 8MB flash)

| Uso | Budget | Fuente |
|---|---|---|
| Audio Library + engines | ~400KB RAM | `04-ai-architecture.md` §1.2 |
| TinyML tensor arena | ~200KB RAM | `04-ai-architecture.md` §1.2 |
| TFLite models en flash | ~500KB flash | `04-ai-architecture.md` §1.2 |
| Inference latency | **<20ms p99** | `04-ai-architecture.md` §8 |

### Filter discreto 2N3904

- Cutoff: 20Hz–20kHz, 24dB/oct — `03-filter-design.md` §2.1
- Resonancia: 0 a auto-oscilación (threshold 80% pot) — `03-filter-design.md` §2.1
- V/oct tracking target: **±10 cents** / 5 octavas — `03-filter-design.md` §2.1
- GPIO Teensy: bypass CD4066 → **pin 25** (GUI vía Bridge Protocol) — `01-architecture.md` §3.3
- Pin 27 libre — TPDT eliminado del diseño

### Bridge Protocol frame

```
[CMD 1B][LEN 1B][SEQ 1B][PAYLOAD 0-255B][CRC8 1B]
```

CRC-8 poly `0x07`, little-endian, UART 921600 8N1 — `02-bridge-protocol.md` §1

---

## Convenciones de código

### Stack

| App | Lenguaje | Framework |
|---|---|---|
| `apps/firmware-teensy/` | C++17 | PlatformIO + Arduino + Teensyduino + Teensy Audio Library |
| `apps/firmware-esp32/` | C++17 | PlatformIO + Arduino + LVGL |
| `apps/bridge-protocol/` | C (headers) | Shared entre ambos firmwares |
| `apps/training/` | Python | uv + TensorFlow / TFLite Micro |
| Tooling | TypeScript | pnpm + Nx |

### Naming

- Funciones: `snake_case`
- Clases: `PascalCase`
- Constantes: `UPPER_CASE`
- Una clase = un par `.h` + `.cpp`
- `#pragma once` en todos los headers (no include guards manuales)

### Rutas de código

| Qué | Dónde |
|---|---|
| Synth engines | `apps/firmware-teensy/src/engines/` |
| Signature FX | `apps/firmware-teensy/src/fx/` |
| TinyML inference | `apps/firmware-teensy/src/ml/` |
| Bridge master | `apps/firmware-teensy/src/bridge/` |
| UI (encoders, buttons, LEDs) | `apps/firmware-teensy/src/ui/` |
| USB-A host MIDI | `apps/firmware-teensy/src/usb/` |
| Sketches de prueba | `apps/firmware-teensy/src/sketches/` |
| Bridge slave | `apps/firmware-esp32/src/bridge/` |
| Display LVGL | `apps/firmware-esp32/src/display/` |
| WiFi manager | `apps/firmware-esp32/src/wifi/` |
| Cloud client | `apps/firmware-esp32/src/cloud/` |
| Shared protocol header | `apps/bridge-protocol/include/` |
| Bridge protocol tests | `apps/bridge-protocol/test/` |
| TinyML training | `apps/training/` |
| Sprint docs | `apps/docs/sprints/` |
| Theory docs | `apps/docs/theory/` |
| Matching jig | `tools/matching-jig/` |
| Filter cal scripts | `tools/filter-cal/` |

---

## Testing

### Filosofía

- Los tests validan comportamiento observable, no implementación interna.
- No mockear el driver de audio o el codec para testear lógica de negocio — usar abstracciones.
- Cada sprint tiene al menos un test que valide el demo (audible/visible).
- Theory doc antes del test: qué estás validando, qué approach, qué NO cubre.
- Tests de regresión antes de pasar de fase.

### Firmware Teensy 4.1 — `apps/firmware-teensy/test/`

Framework: **PlatformIO Unity** (`unity.h`).

Tests nativos (sin hardware) corren en CI. Tests on-device requieren Teensy conectado.

| Área | Qué testear | Env |
|---|---|---|
| Bridge Protocol | Frame serialization roundtrip, CRC-8 vectors, payload >255 rechazado, SEQ 255→0 | native |
| Engines | Parámetros dentro de rango, output no NaN/Inf, polyphony sin crash | native (mock audio) |
| FX | Parámetros dentro de rango, bypass path sin artifacts | native |
| TinyML | Tensor arena ≤200KB, inference <20ms, output dentro de rango | teensy41 |
| UI | Encoder debounce logic, button state machine, LED mapping | native |

```bash
cd apps/firmware-teensy
pio test -e native            # CI-friendly, sin hardware
pio test -e teensy41          # on-device, Teensy conectado
```

### Firmware ESP32-S3 — `apps/firmware-esp32/test/`

Framework: **PlatformIO Unity**.

| Área | Qué testear | Env |
|---|---|---|
| Bridge Slave | Frame parsing, ACK/NACK correctos, timeout handling | native |
| WiFi Manager | State machine (connecting/connected/disconnected) — mock network | native |
| Cloud Client | Request serialization, response parsing — mock HTTP | native |
| Display | State updates sin out-of-bounds en buffer | native |

```bash
cd apps/firmware-esp32
pio test -e native
pio test -e esp32s3           # on-device
```

### Bridge Protocol — `apps/bridge-protocol/test/`

Tests del protocolo compartidos entre ambos firmwares. Se incluyen en ambas suites via symlink o
inclusión directa.

- CRC-8 contra test vectors conocidos (polynomial 0x07)
- Serialization roundtrip para todos los CMD IDs del spec
- Multi-frame transfer (payloads >255 bytes)
- Error handling: todos los NACK reasons, timeout recovery, heartbeat loss

### ML / Training — `apps/training/`

Framework: **pytest**.

| Área | Target | Fuente |
|---|---|---|
| Accuracy del modelo | >85% precision/recall en validation set | `04-ai-architecture.md` §8 |
| Delta de quantización | Float32 vs int8 accuracy drop <5% | `04-ai-architecture.md` §1.5 |
| Memory footprint | Modelo TFLite ≤500KB total (todos los modelos) | `04-ai-architecture.md` §1.2 |
| Inference latency | Simulada en CPU equivalente <20ms p99 | `04-ai-architecture.md` §8 |

```bash
cd apps/training
uv run pytest                          # toda la suite
uv run pytest models/key_detector/    # modelo específico
```

### Tests de integración (hardware físico)

No corren en CI. Requieren Teensy + ESP32 conectados y configurados.

| Escenario | Criterio de pass | Fuente |
|---|---|---|
| UART loopback Teensy↔ESP32 | 1000 frames sin error, throughput >1000 frames/s | `02-bridge-protocol.md` §7.3 |
| MIDI in → audio out latencia | <5ms p95, medido con analizador de audio | `01-architecture.md` §5.2 |
| Connection recovery | Heartbeat miss → recovery en <5s | `02-bridge-protocol.md` §4.3 |
| Audio quality | THD+N <0.1% @ 1kHz, -12dBFS | `01-architecture.md` §5.3 |
| High-throughput stress | Notes + display updates simultáneos sin xruns | `02-bridge-protocol.md` §7.2 |

### Filter calibration (jig físico)

Procedimiento definido en `03-filter-design.md` §5. Pass/fail:

- Sweep 20Hz–20kHz sin clicks ni discontinuidades
- Auto-oscilación limpia a 80% resonance (sin clipping)
- Bypass transition sin click audible
- THD <0.5% @ 1kHz, baja resonancia

Tiempo estimado por unidad: 5-7 minutos.

### CI/CD — `.github/workflows/`

Lo que corre automáticamente en cada PR:

| Check | Comando |
|---|---|
| Build Teensy (sin flash) | `pio run -e sketch` |
| Build ESP32 (sin flash) | `pio run -e esp32s3` |
| Tests nativos Teensy | `pio test -e native` (desde `firmware-teensy/`) |
| Tests nativos ESP32 | `pio test -e native` (desde `firmware-esp32/`) |
| Tests ML | `uv run pytest apps/training/` |

Lo que **no** corre en CI (requiere hardware):

- Tests on-device (`pio test -e teensy41`, `-e esp32s3`)
- Tests de integración UART
- Tests de latencia audio
- Calibración del filter

### Reglas

- No mockear hardware real en tests on-device — si necesita mock, es un test native.
- Cada nuevo engine o FX tiene al menos un smoke test (parámetros → output no crashea).
- Los tests de protocolo (Bridge) son los más críticos para CI — corren en cada PR sin excepción.
- Si un test on-device pasa pero el native falla: el native tiene razón (el on-device puede enmascarar bugs de memoria).
