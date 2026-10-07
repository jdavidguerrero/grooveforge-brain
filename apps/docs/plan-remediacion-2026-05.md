# 🛠️ GrooveForge Brain — Plan Integral de Remediación

> **Deriva de:** [`auditoria-tecnica-2026-05.md`](auditoria-tecnica-2026-05.md) (v2, commit `0ae14d1`).
> **Fecha:** 2026-05-29
> **Owner:** Juan Guerrero (GPROG)
> **Metodología:** spec-first · phase-gates · vertical slices · one thing at a time · educational-first (`CLAUDE.md`, `06-implementation-roadmap.md §0`).
> **Naturaleza:** plan de trabajo. **No modifica `00–06`**; donde el SSoT necesita cambios, los lista como *tareas* (no los aplica unilateralmente). Las contradicciones se **escalan al owner como decisiones**, no se resuelven aquí.

> ⏱️ **Realidad de tiempo (`06 §0.1`):** Brain es prioridad #2, 10–18 h/semana. Los estimados son **rangos honestos**, no compromisos. El reloj de la Fase A lo domina el *lead time de componentes*, no las horas de trabajo.

---

## 0. Cómo leer este plan

- **Severidad** viene del audit: P0 (bloqueante v1.0), P1 (alto), P2 (medio), P3 (bajo).
- Cada tarea: **Theory/Spec → Implementación → Demo/Gate** (sin demo audible/visible no se avanza, `06 §0.1`).
- **IDs `R-xN`** = tarea de remediación, fase `x`. Donde aplica, mapeo al sprint del roadmap existente.
- **Agente sugerido**: cuál de los subagentes del proyecto lo ejecuta mejor.
- 🔓 = **decisión del owner requerida** antes de empezar (Fase 0).

### El principio de secuenciación: retirar el riesgo existencial primero
El lado digital ya está sólido (audit v2 §11). **El único riesgo que puede invalidar el producto entero es el filtro analógico** — y tiene *lead time de compra*. Por eso:

```
Fase 0 (decisiones)  ─┬─▶ Fase A (FILTRO: validar la premisa) ──────────────▶ Fase E (PCB+enclosure) ─▶ Fase F (ship)
                      │        ▲ lead time de componentes (semanas)              ▲ requiere A + decisiones
                      └─▶ Fase B (integración digital) ─▶ Fase C (robustez+AI) ──┘
                                  └──── Fase D (docs+tests, transversal) ────┘
```

**Insight clave:** mientras la Fase A espera componentes (matched 2N3904, TL072, CD4066, charge pump…), las Fases B/C/D son trabajo de firmware que **no depende de ese hardware** → se hacen *en paralelo* aprovechando la espera. No es "una fase tras otra" en calendario; es "el filtro marca el camino crítico, el firmware llena los huecos de espera".

---

## Fase 0 — Decisiones del owner (gate de arranque)

Estas no son tareas de código: son **forks que bloquean trabajo aguas abajo**. Resolverlas en una sola sesión de decisión. Per `CLAUDE.md`, las presento; **decidís vos**.

| # | Decisión | Qué bloquea | Opciones / recomendación del audit |
|---|---|---|---|
| 🔓 D1 | **X1** — ¿el filtro analógico aplica a *todos* los engines o *solo* al Moog? | `setDigitalFilterEnabled` (R-B2), routing, schematic (R-A1) | El diseño eléctrico (loop externo al SGTL5000) lo permite para todos. Recomendación: "todos, con default ON/OFF por engine" y corregir `03 §6.3`. |
| 🔓 D2 | **X3** — reasignar el pin 14 (hoy ENC R-B *y* A0/Volume) | KiCad (R-E1), volume pot | Mover el volume pot a otro pin ADC libre. **Bloquea el PCB.** |
| 🔓 D3 | **X5** — V/oct: ¿target real ±10 o ±20 cents? | Spec de calibración (R-A5), marketing | Elegir UN número que sea spec *y* acceptance. |
| 🔓 D4 | **X8** — ¿LiPo + IP5306 o USB-C only? | Dimensionado del enclosure (R-E3), power | Conflicto arch (con batería) vs enclosure (sin). Define BODY_H. |
| 🔓 D5 | **Path de salida en Modo FX** (jack desde filtro vs desde SGTL5000) | Ruteo dual (R-E1), `theory/audio-routing-dual-mode.md §9` | Decisión abierta hace tiempo; cerrarla. |
| 🔓 D6 | **Framing de marketing** (6 arquitecturas / 12 FX / "AI") | Copy de pre-orden (R-F2) | Vender outcomes + "N al lanzamiento, M en roadmap". |

**Gate Fase 0:** las 6 decisiones registradas en `00-master-strategy.md §10 Decision Log` (o donde corresponda). Sin D1/D2 no arrancan A ni E con confianza.

---

## Fase A — Validar la premisa (filtro analógico 2N3904)  · 🔴 P0

**Objetivo:** demostrar que el filtro suena, auto-oscila y es estable y reproducible — *antes* de gastar en PCB o abrir pre-órdenes. Es el experimento que puede matar o validar el producto.
**Agente:** Hardware Engineer (diseño) + Firmware Engineer (CV/jig).
**Lead time:** dominado por compra de componentes. Trabajo neto ~40–60 h.

| ID | Tarea | Theory/Spec primero | Demo/Gate | Sev |
|---|---|---|---|---|
| **R-A1** | **Diseñar el circuito faltante** (P0-2/P0-3): conversor exponencial V/oct (topología, rango de corriente Ie, compensación térmica), control de resonancia desde PWM/DAC del Teensy, y rail negativo (charge pump −5V). | Theory doc del ladder completo + schematic en papel. Actualiza `03-filter-design.md §6.2` (hoy "pendiente"). Refs: Moog US3475623, Electronotes/Hutchins, D'Angelo. | Schematic completo con valores + BOM actualizado (incluye charge pump ~$0.50). | P0 |
| **R-A2** | **Cerrar el matching jig** (`tools/matching-jig/` vacío): actualizar `03 §4` al circuito Teensy 12-bit + `analogReadAveraging(32)` + protocolo térmico (el sprint 03 ya lo corrigió, falta migrar al SSoT y ejecutar). | Update `03 §4`; sketch del jig. | Lote matcheado: ≥8 pares con ΔVbe<2mV. | P0 |
| **R-A3** | **Protoboard del filtro + CV de cutoff**: armar el ladder con pares matched; PWM/DAC del Teensy → conversor exp → corriente de cola. | — (sigue R-A1) | **Sweep 20Hz–20kHz audible**, sin clicks. | P0 |
| **R-A4** | **Resonancia + auto-oscilación**: cablear el feedback controlado por ENC R. | — | **Auto-oscila limpio al 80%** del rango, sin clipping. | P0 |
| **R-A5** | **Calibración + medición**: definir instrumentos (generador + interfaz de audio calibrada + REW), medir THD/SNR/headroom/V-oct. **Punto de decisión P0-3:** ¿5V alcanza o se confirma el rail negativo? | Update `03 §5` con setpoints + instrumentos + temperatura; resolver D3 (±10/±20). | Cumple acceptance `01 §5.3` (THD<0.5%, V/oct), o se ajusta el spec con datos reales y se documenta. | P0 |

**🎯 HITO FASE A:** el filtro analógico suena, auto-oscila, es estable y está medido. **La premisa de $599 queda validada.**
**🛑 Contingencia:** si R-A5 falla (ruido/inestabilidad/headroom irrecuperable a costo viable) → **decisión de producto antes de gastar en PCB**: rail negativo obligatorio, o pivote a un filtro analógico alternativo, o reposicionamiento. Mejor descubrirlo aquí que con 100 PCBs.

---

## Fase B — Cerrar la integración digital  · P1

**Objetivo:** convertir el firmware (hoy un sketch monolítico) en un producto integrado, y limpiar los bugs de audio de alto impacto. **Se hace en paralelo a la espera de componentes de la Fase A.**
**Agente:** Firmware Engineer + DSP Engineer.
**Esfuerzo:** ~20–30 h.

| ID | Tarea | Theory/Spec | Demo/Gate | Sev |
|---|---|---|---|---|
| **R-B1** | **Quick wins de audio** (bajo costo, alto impacto): release envelope desde nivel actual (6 engines, P1-5); normalizar ganancias de master mix (Juno/OB-6 1.5×, Prophet 1.25×, P1-6); Moog mixer default 1.20→1.0 (P2). | Comentar el *por qué* de cada fix. | A/B audible (sin saltos en staccato, sin clip con voces llenas) + smoke test no-NaN/clip. | P1 |
| **R-B2** | **`setDigitalFilterEnabled` por engine** (P1-4) + aplicar política D1. Bypass del `MoogLadder4P` cuando el analógico esté activo, para los engines que correspondan. | Depende de D1. Update `03 §6.3` (generalizar más allá del Moog). | Con "analógico activo" simulado (GPIO 25), el filtro digital se bypasea en los engines correctos. | P1 |
| **R-B3** | **Engine IDs**: añadir `GF_ENGINE_OB6/DX7/ARP2600` (0x03–0x05) a `protocol.h`, unificar wire format (hoy `0x10+idx` en sketch28 vs enum), `device_state` rango 0–5 (P1-7). | Update `02-bridge-protocol.md` §2.2. | Los 6 engines se anuncian y el ESP32 muestra la vista correcta para cada uno. | P1 |
| **R-B4** | **Entrypoint de producto**: extraer la integración de `sketch28` (~2.6K líneas) a `main.cpp`/`env:product` (6 engines + audio_graph + fx_manager + ml_engine + UI + bridge + MIDI). Mover crossfade y boot fuera de `delay()` bloqueante. | Theory: arquitectura del firmware de producto. | `env:product` compila y arranca; cambio de engine sin perder MIDI. | P1 |

**🎯 HITO FASE B:** firmware de producto integrado; los 6 engines suenan limpio; listo para recibir el filtro analógico de la Fase A.

---

## Fase C — Robustez de comunicación y AI  · P1

**Objetivo:** que el Bridge sobreviva al mundo real y que la AI corra en device dentro de budget. **También paraleliza con la Fase A.**
**Agente:** Firmware Engineer + ML Engineer.
**Esfuerzo:** ~30–40 h.

| ID | Tarea | Theory/Spec | Demo/Gate | Sev |
|---|---|---|---|---|
| **R-C1** | **Robustez del Bridge** (P1-3): retry 3×, recovery `GET_VERSION` + re-anuncio de estado, framing resync (timeout en `GOT_CMD` + re-sync deslizante), multiframe 0xF0–0xF2, NACK 0x06–0x08, `carousel_resume()` en heartbeat-loss. | `02 §4` (ya está especificado). | Loopback con ruido inyectado: el parser se recupera; desconectar/reconectar ESP32 → estado converge. | P1 |
| **R-C2** | **CMSIS-NN on** (P1-2): incluir `cmsis_nn/` en `library.json`, flags FPU de Teensy; mover/medir arenas. | Skill `tinyml-quantization`. | Latencia por Serial: 3 modelos <20 ms p99 en device. | P1 |
| **R-C3** | **Calidad TinyML**: verificar cuantización (round vs trunc en key/chord/beat), unificar el histograma feature (3 variantes → 1), medir reality gap con MIDI real, calibrar umbral de confianza. | Update métricas en `04 §8`. | Accuracy en música real medida y reportada honestamente (no solo sintética). | P1/P2 |
| **R-C4** | **Boot ESP32 <2s** (P1-10): quitar `delay(2000)`, recortar splash/anim. | `01 §5.1`. | Medición power-on → vista operativa <2 s. | P1 |

**🎯 HITO FASE C:** comunicación robusta ante fallos; AI corre en device dentro de latencia/memoria; boot en spec.

---

## Fase D — Higiene de proceso (transversal) · P1

**Objetivo:** restaurar el "spec-first" — que el SSoT vuelva a reflejar la realidad — y que el CI valide el producto. **Corre en paralelo con B y C** (no es una fase secuencial; es disciplina continua).
**Agente:** Documentation Writer + Firmware Engineer.
**Esfuerzo:** ~12–18 h.

| ID | Tarea | Demo/Gate | Sev |
|---|---|---|---|
| **R-D1** | **Pase de sincronización doc↔código:** actualizar `00–06` + `CLAUDE.md` al estado real (6 engines, `audio_graph`, `MoogLadder4P`, Fase 6 *determinística* no-NN, tamaños de modelo reales, pin 27→74HC4053, jig 12-bit, quitar TPDT de `03`, presupuestos fantasma de `04 §1.3`). Resolver/registrar X1–X12 con las decisiones de la Fase 0. | Diff de specs revisado; X1–X12 con estado RESUELTA/decidida. | P1 |
| **R-D2** | **Tests + CI**: tests nativos del lado Teensy (bridge CRC/roundtrip, engine param-range no-NaN, vectores Fase 6 del sprint 37); `env:product`/`teensy41` en CI. | `pio test -e native` (Teensy) corre tests reales; CI compila el producto. | P1 |
| **R-D3** | **Documentar el namespace `param_id`** (0x00E0–0x00FF) en `02-bridge-protocol.md`. | El contrato Teensy↔ESP32 está en el spec, no solo en comentarios. | P2 |

**🎯 HITO FASE D:** SSoT confiable + CI que valida el firmware de producto. (Sin esto, futuras auditorías/decisiones vuelven a correr sobre specs obsoletos — la causa raíz de que esta auditoría se rehiciera.)

---

## Fase E — Hardware físico y enclosure · (post-A)

**Objetivo:** del prototipo validado al producto fabricable. **Requiere Fase A completa + decisiones D2 (pin 14) y D4 (batería).**
**Agente:** Hardware Engineer + Industrial Designer.
**Lead time:** dominado por fab (JLCPCB 4–6 semanas) + CNC/guadua.

| ID | Tarea | Depende de | Demo/Gate |
|---|---|---|---|
| **R-E1** | KiCad schematic (main 4-layer + filter sub-block + panel PCB). | Fase A, D2, D4, D5 | Schematic completo, ERC limpio. |
| **R-E2** | PCB layout: star ground, LC filter audio rail, separación analóg/digital. | R-E1 | DRC limpio; gerbers a fab. |
| **R-E3** | Enclosure: ubicar jacks FX IN, exportar las 5 piezas CAD, fijación mecánica del bracket (no VHB), bottom cover, acceso a trimmers del filtro, decisión guadua vs MOSO. | D4 | 5 piezas exportadas (STEP/STL); ensamblaje seco encaja. |
| **R-E4** | Re-costear BOM con labor de matching/calibración + rechazo; definir instrumentos de cal de línea. | Fase A | BOM realista + margen neto (no solo bruto). |

**🎯 HITO FASE E:** 5 prototipos completos producidos y ensamblados.

---

## Fase F — Pre-órdenes y shipping · (post-E)

| ID | Tarea | Gate |
|---|---|---|
| **R-F1** | Prototipo end-to-end medido contra **todos** los acceptance criteria `01 §5`. | Checklist `01 §5.1–5.3` pasa. |
| **R-F2** | Comunicar claims con números reales (D6); **abrir pre-órdenes** (recién aquí). | Copy revisado contra realidad medida. |
| **R-F3** | Beta 10 units (PTDJA) → batch 1 JLCPCB SMT. | Feedback beta incorporado. |

**🎯 HITO FASE F:** producto confiable, medido, y vendido con claims defendibles.

---

## Resumen de secuenciación y esfuerzo

| Fase | Camino | Esfuerzo neto | Calendario (10–18h/sem) | Bloquea a |
|---|---|---|---|---|
| 0 — Decisiones | — | 1 sesión | inmediato | A, B, E |
| **A — Filtro** 🔴 | **crítico** | ~40–60 h | dominado por compras (semanas) | E, F |
| B — Integración | paralelo a A | ~20–30 h | 2–3 sem | C, E |
| C — Robustez+AI | paralelo a A | ~30–40 h | 3–4 sem | F |
| D — Docs+Tests | transversal | ~12–18 h | continuo | (calidad de todo) |
| E — HW+enclosure | post-A | (fab-bound) | 6–10 sem | F |
| F — Ship | post-E | (proceso) | meses | — |

**Camino crítico = Fase 0 → A → E → F.** B/C/D se ejecutan durante las esperas de A y E, así que **no alargan el calendario** — lo aprovechan.

---

## Qué hacer esta semana (arranque concreto)

1. **Sesión de decisión (Fase 0):** cerrar D1–D6. ~2–3 h. Desbloquea todo.
2. **Encargar componentes del filtro** (R-A1 BOM): matched 2N3904 en cantidad, TL072, CD4066, charge pump, caps polystyrene 1nF, trimmers Bourns. Arranca el reloj de lead time.
3. **Mientras llegan:** R-B1 (quick wins de audio — son cambios chicos, A/B audible inmediato y mantienen momentum) y empezar R-D1 (sync de specs, barato y reduce el riesgo de seguir construyendo sobre docs obsoletos).

---

## Notas de gobernanza

- **Un theory doc antes de cada tarea con DSP/analog/ML** (`CLAUDE.md`): R-A1, R-A5, R-B2, R-C2/3 lo requieren.
- **Las decisiones de la Fase 0 y el resultado de R-A5 pueden cambiar `00–06`** — eso es trabajo explícito (R-D1), no edición silenciosa.
- **Gate de pausa cada 30 días** (`01 §8` riesgo de burnout): respetarlo; recortar scope antes que velocidad.
- Este plan es *living*: se ajusta tras cada hito, como el `06-implementation-roadmap.md`.

---

*Fin del Plan Integral de Remediación — GrooveForge Brain · 2026-05-29*
*Deriva del audit v2 (`0ae14d1`). Plan de trabajo, no SSoT.*
