# 🔍 GrooveForge Brain — Auditoría Técnica Integral (v2)

> **Tipo:** Auditoría independiente de implementación vs diseño.
> **Commit auditado:** `0ae14d1` ("feat(sprints 38-45): 3→6 engines + D'Angelo filter + Juno poly + Layer 1 AI completo + AI Mode consolidation"), `origin/main`, 2026-05-29.
> **Fecha:** 2026-05-29
> **Auditor:** Claude Code (rol: ingeniería de hardware/firmware para instrumentos musicales + músico/synth).
> **Método:** lectura de los 7 specs `00–06` como benchmark + 5 auditorías especializadas por subsistema (DSP, ML, firmware Teensy, firmware ESP32, hardware/specs) sobre el código actual + verificación manual de los hallazgos de mayor impacto.

> 📌 **Nota de versión.** La v1 de esta auditoría se corrió por error sobre un worktree desfasado (`e8a7a12`, 27-may), 1 commit / +13.9K líneas por detrás de `origin/main`. Esta v2 está hecha sobre el código real actual (`0ae14d1`) tras sincronizar. **Varios hallazgos P0/P1 de la v1 ya estaban resueltos** en el código real — esto se marca explícitamente como "✅ resuelto en 0ae14d1".

> ⚠️ **Verificabilidad:** sin hardware ni toolchain de compilación en el entorno de auditoría. Todo lo eléctrico/latencia/audio es análisis estático. Lo no verificable está en el Apéndice A.

> 🔁 **Per `CLAUDE.md`:** este documento **señala contradicciones, no las resuelve**. No es SSoT; no modifica `00–06`.

---

## Tabla de contenido

1. [Resumen ejecutivo](#1-resumen-ejecutivo)
2. [Qué cambió entre la v1 y la v2 (sprints 38-45)](#2-qué-cambió)
3. [Veredicto: ¿vale cada centavo de los $599?](#3-veredicto)
4. [Estado de implementación vs diseño](#4-estado-de-implementación-vs-diseño)
5. [Hallazgos críticos consolidados](#5-hallazgos-críticos-consolidados)
6. [Auditoría por subsistema](#6-auditoría-por-subsistema)
7. [Registro de contradicciones de specs (re-verificado)](#7-registro-de-contradicciones-de-specs)
8. [Integridad de marketing](#8-integridad-de-marketing)
9. [Registro de riesgos](#9-registro-de-riesgos)
10. [Tests y CI](#10-tests-y-ci)
11. [Lo que está bien hecho](#11-lo-que-está-bien-hecho)
12. [Hoja de ruta de remediación](#12-hoja-de-ruta-de-remediación)
13. [Apéndice A — No verificado](#apéndice-a--no-verificado)

---

## 1. Resumen ejecutivo

En los últimos días el proyecto dio un **salto de categoría en el lado digital** (de 3 a 6 engines, filtro VA real, grafo de audio compartido, AI Layer 1 orquestada) que **resolvió el bloqueante arquitectónico #1** de la auditoría previa. Al mismo tiempo, **el riesgo central no se movió**: el filtro analógico discreto — lo que justifica el precio — sigue siendo un diseño en papel con huecos eléctricos bloqueantes, sin prototipo. Y un patrón de fondo se agravó: **la documentación (`00–06`, `CLAUDE.md`) quedó aún más por detrás del código.**

### El estado en una frase
GrooveForge ya es **un firmware digital sorprendentemente completo y bien arquitecturado** (6 engines + filtro D'Angelo + 9 FX + AI Layer 1 + UI funcional con datos reales), todavía **sin integrar en un único entrypoint de producto**, montado sobre un **diseño de hardware analógico incompleto** y una **base documental desincronizada**.

### Semáforo por subsistema (actualizado a 0ae14d1)

| Subsistema | Estado | Δ vs v1 | Veredicto corto |
|---|---|---|---|
| **Arquitectura de audio (audio_graph)** | 🟢 | ⬆️ **resuelto** | Singletons `g_i2s`/`g_codec` compartidos; los 6 engines ya **no** instancian I/O propia. Era el P0 #1. |
| **Synth engines (6)** | 🟢🟡 | ⬆️ | Set completo (Moog/Juno/Prophet/OB-6/DX7/ARP). Juno ahora polifónico. Bugs de envelope/ganancia persisten; DX7 no es FM real, OB-6 con filtro equivocado. |
| **Filtro digital (MoogLadder4P D'Angelo)** | 🟢🟡 | 🆕 | VA 4-polo real reemplaza el placeholder SVF — sonido boutique **ahora creíble**. Falta oversampling; coef. de cutoff a verificar. |
| **Signature FX (9) + fx_manager** | 🟢🟡 | ⬆️ | Gestor con new/delete correcto. ModalReverb decay y PhaseChorus aún no cumplen carácter del spec. |
| **TinyML Layer 1 + ml_engine + Fase 6** | 🟡 | ⬆️ | Orquestador + auto-harmonize/arp/groove/velocity. **CMSIS-NN aún off**, aún no corre en device, reality gap sin medir. |
| **Bridge Protocol** | 🟢🟡 | = | Núcleo correcto. **Sin retry/recovery/resync/multiframe** (igual que v1). Engine IDs incompletos en el protocolo. |
| **Firmware Teensy (integración)** | 🟡 | ⬆️ | sketch28 integra todo (monolito ~2.6K líneas); `main.cpp` (env de producción) sigue siendo solo-UI. No hay entrypoint de producto en CI. |
| **Firmware ESP32 / UI (28 vistas)** | 🟢🟡 | ⬆️ | Resultados ML (0x80-0x83) **ya se consumen** en vistas reales. Boot ~6.2s (peor que el spec <2s), engine-ID latente para engines 3-5. |
| **Filtro analógico 2N3904 (HW)** | 🔴 | = | **Corazón del precio, diseño incompleto.** Conversor exp y control de resonancia "pendientes"; headroom 5V insuficiente; KiCad/jig/cal vacíos. |
| **Enclosure** | 🟡 | = | (Sin cambios materiales en este commit; la evolución real está en una rama sin pushear.) |
| **Tests / CI** | 🔴 | = | `firmware-teensy/test/` vacío; CI Teensy corre nada; cero tests de engines/FX/ML/Fase 6. |
| **Docs / SSoT** | 🔴 | ⬇️ **peor** | El código avanzó (sprints 38-45) pero `00–06`/`CLAUDE.md` no. Drift agravado. |

🟢 sólido · 🟡 funcional con gaps · 🔴 bloqueante o crítico · ⬆️ mejoró · ⬇️ empeoró · = igual

### Los puntos que más importan ahora

1. **✅ RESUELTO (era P0 #1): los engines ya son componibles.** `audio_graph.cpp` expone `g_i2s`/`g_codec`/`g_audio_in` como singletons; los 6 engines fueron refactorizados para no instanciar su propio I2S (verificado: `moog_model_d.h:202` "movidos a audio_graph.cpp"). `AudioMemory(96)` centralizado. Excelente corrección.

2. **🔴 P0 (sin cambio): el filtro analógico sigue siendo un diseño incompleto.** `03-filter-design.md §6.2` sigue marcando como *"pendiente de definir"* el **conversor exponencial (cutoff/V-oct)** y el **control de resonancia**. A **5V unipolar el headroom (~2 Vpp) es insuficiente** para el nivel de 1 Vrms del propio spec → clipping; falta rail negativo en el BOM. `tools/matching-jig`, `tools/filter-cal` y KiCad siguen vacíos. **Es el mayor riesgo del proyecto y no se movió.**

3. **🆕 P1 ELEVADO — el doble ladder ahora afecta a TODOS los engines.** Antes el problema "filtro digital + analógico en serie = 48 dB/oct" era solo del Moog. Ahora `MoogLadder4P` corre en **las voces de los 6 engines** y el filtro analógico está "disponible para todos" (X1). `setDigitalFilterEnabled()` (que `03 §6.3` exige) **sigue sin existir en ningún archivo**. Cuando el hardware analógico se conecte, todos los engines tendrán doble filtrado destructivo.

4. **🆕 P1 — bug latente de engine ID para los 3 engines nuevos.** `protocol.h` solo define IDs `0x00–0x02`; sketch28 envía `0x10 + engine` (0x10–0x15) (`28-synth-navigator.cpp:1075`); el ESP32 hace `(eid>=0x10)?eid-0x10:0`. Funciona por accidente para Moog/Juno/Prophet, pero **OB-6/DX7/ARP no se mapean bien** y el enum del protocolo está incompleto.

5. **🟡 P1 (sin cambio): la capa de robustez del Bridge sigue faltando.** No hay retry (3×), ni recovery (`GET_VERSION` post-heartbeat), ni resync de framing, ni multiframe (0xF0–0xF2); NACK 0x06–0x08 sin manejar. Una reconexión del ESP32 deja el display congelado (`carousel_resume()` nunca se llama).

6. **🟡 P1 (sin cambio): no hay entrypoint de producto ni tests.** `main.cpp` (el env `teensy41` de producción) sigue siendo solo-UI; el integrador real es `sketch28` (monolito ~2.6K líneas). `firmware-teensy/test/` sigue vacío y el CI no compila el env de producción ni corre nada en Teensy.

7. **🟢 La integridad de marketing de la AI está bien cuidada en el código.** Fase 6 (auto-harmonize, smart-arp, groove, velocity) está documentada honestamente como **determinística/Markov/estadística, no redes neuronales** (`auto_harmonize.h:9-15` dice literalmente "Por qué es determinístico (no ML)"). El problema es que `04-ai-architecture.md §1.3` aún les asigna presupuestos fantasma (80KB/12ms) como si fueran modelos TFLite.

8. **🔴 El throughline: drift de documentación.** El código corrió a sprints 38-45; los specs `00–06` y `CLAUDE.md` no se actualizaron. Tan real es el problema que **esta misma auditoría tuvo que rehacerse** porque el worktree estaba desfasado. El "spec-first" del proyecto hoy es aspiracional.

---

## 2. Qué cambió

Para que quede claro qué resolvió `sprints 38-45` respecto a la auditoría v1:

| Hallazgo v1 | Estado en 0ae14d1 | Evidencia |
|---|---|---|
| P0-1 engines no componibles (cada uno con su `AudioOutputI2S`) | ✅ **RESUELTO** | `audio_graph.{h,cpp}` singletons; 6 engines sin I/O propia (`moog_model_d.h:202`) |
| D1 VCF placeholder SVF 2-polo | ✅ **RESUELTO** | `MoogLadder4P` D'Angelo & Välimäki 4-polo en todos los engines |
| "Solo 3 engines" | ✅ **6 engines** | arp2600, dx7, ob6 añadidos |
| Juno monofónico (P1-1) | ✅ **6 voces** | `juno_106.cpp` voice stealing FIFO |
| AudioMemory en begin() (C4) | ✅ **RESUELTO** | centralizado en `audio_graph::init(96)` |
| ML sin orquestador / no corre | 🟡 **Parcial** | `ml_engine` existe + Fase 6; pero aún no en device, CMSIS-NN off |
| FX sin gestor | ✅ **fx_manager** | new/delete con destructor correcto |
| ESP32 ML en `on_generic` | ✅ **consumido** | handlers 0x80-0x83 + 5 vistas nuevas con datos reales |
| CMSIS-NN excluido (P1-2) | ❌ **sigue** | `library.json` |
| Bridge sin retry/recovery (P1-3) | ❌ **sigue** | `bridge_master.cpp` |
| setDigitalFilterEnabled (P1-4) | ❌ **sigue (elevado)** | ahora afecta a los 6 engines |
| Filtro analógico incompleto (P0-2/3) | ❌ **sigue** | `03-filter-design.md §6.2` |
| Bug release envelope (P1-5) | ❌ **sigue** | en los 6 engines |
| Pin 14 conflicto (P1-8) | ❌ **sigue** | `01-architecture.md §3.3` |
| Boot ESP32 lento | ❌ **peor (~6.2s)** | delay 2000 + anim 2200 + splash 2000 |

---

## 3. Veredicto: ¿vale cada centavo de los $599?

| Pilar del valor | Estado actual | Riesgo |
|---|---|---|
| **Filtro analógico discreto "Moog real"** | Diseño eléctrico incompleto, sin prototipo, headroom comprometido | 🔴 **Alto** — sin cambios; es el diferenciador #1 y el menos maduro |
| **6 arquitecturas clásicas** | ✅ Las 6 implementadas (con un filtro VA D'Angelo creíble) | 🟢 **Bajo** — la promesa "6 arquitecturas" ahora se cumple en software |
| **AI integrada por capas** | Layer 1 orquestada + Fase 6, honesta en código; falta correr/validar en device | 🟡 **Medio** — vender outcomes, no conteo de modelos |
| **12 FX boutique** | 9 implementados (3 son v1.1) | 🟡 **Medio** — entregable, comunicar "9 al lanzamiento" |

**Conclusión (sin cambio respecto a la v1, y es lo más importante):** el lado digital ya empieza a *demostrar* el valor de $599 — 6 emulaciones con un ladder VA real es un argumento fuerte. Pero **el pilar más caro de justificar (el filtro analógico) sigue sin existir y con huecos de diseño.** La recomendación se mantiene y se vuelve más urgente porque todo lo demás ya avanzó: **correr ya un spike de validación del filtro analógico en protoboard** (definir el conversor exp + resonancia + rail negativo, armar, medir que suena/auto-oscila/es estable). No abrir pre-órdenes ni fijar fecha hasta tenerlo. Es el único experimento que puede invalidar la premisa entera, y hoy está casi al final del roadmap (Fase 9).

---

## 4. Estado de implementación vs diseño

| Área | Spec promete | Código (0ae14d1) | Δ |
|---|---|---|---|
| Engines | 6 (v1.0=3, resto updates) | **6 implementados** | ✅ adelantado al plan |
| Polifonía | Moog 6 / Juno 8 / Prophet 5 | Moog mono, Juno **6**, Prophet 5, OB-6 6, DX7 6, ARP 4 | Juno 6≠8 (6 es auténtico); Moog mono por diseño |
| Filtro digital | ladder real 24dB/oct | **MoogLadder4P (D'Angelo) 4-polo** en todos | ✅ resuelto |
| Filtro analógico | ladder 2N3904 controlable | Nada físico; diseño con huecos | 🔴 incompleto |
| FX | 12 signature | 9 + fx_manager (3 son v1.1) | conteo inconsistente entre docs (X6) |
| TinyML | Scale Lock+Chord+Beat corriendo + Fase 6 | 3 modelos + ml_engine + Fase 6; **no corre en device aún** | drift: roadmap aún dice "no vendoreado" |
| Bridge | frame+CRC+retry+recovery+multiframe | frame+CRC ✅; resto ✗ | robustez incompleta |
| Display UI | vistas + datos del Teensy | **28 vistas + datos reales incl. ML** | 🟢 funcional |
| WiFi/Cloud/DAW | Layers 2/3 | vacíos | gap esperado (Fases 7/8) |
| Enclosure | 5 piezas + base | solo body PETG exportado | resto en scripts/rama sin pushear |
| KiCad / jig / cal | PCB + herramientas | vacíos | gap (Fase 9) |

---

## 5. Hallazgos críticos consolidados

**P0** bloqueante v1.0 · **P1** alto · **P2** medio · **P3** bajo/mejora. (✅ = ya resuelto en 0ae14d1, se lista para trazabilidad.)

### P0 — Bloqueantes
| ID | Hallazgo | Evidencia | Acción |
|---|---|---|---|
| ✅ **P0-1** | ~~Engines no componibles~~ **RESUELTO** | `audio_graph`, `moog_model_d.h:202` | — (terminar: extraer legacy de Moog/Prophet si queda; crear entrypoint de producto) |
| **P0-2** | Filtro analógico: conversor exponencial (cutoff/V-oct) y control de resonancia **sin definir** | `03-filter-design.md §6.2` | Diseñar ambos circuitos antes del schematic. |
| **P0-3** | Filtro a 5V unipolar: headroom ~2 Vpp < 2.83 Vpp (1 Vrms); sin rail negativo en BOM | `03-filter-design.md §2.1/§9`; `01 §3.2` | Charge pump −5V (~$0.50) o re-escalar nivel + reamplificar. |

### P1 — Alto
| ID | Hallazgo | Evidencia | Acción |
|---|---|---|---|
| **P1-2** | CMSIS-NN excluido → kernels de referencia 3–4× más lentos; <20ms p99 en riesgo | `library.json` (`-<.../cmsis_nn/>`) | Incluir cmsis_nn + flags FPU; medir en device. |
| **P1-3** | Bridge sin retry/recovery/resync/multiframe; NACK 0x06–0x08 sin manejar | `bridge_master.cpp:111-147,78-85` | Implementar las 4 piezas del spec `02 §4`. |
| **P1-4** | `setDigitalFilterEnabled()` no existe — **ahora el doble-ladder afecta a los 6 engines** (MoogLadder4P en todas las voces + analógico "para todos") | `03 §6.3`; ningún archivo lo implementa | Implementar bypass del filtro digital por engine; definir política (X1). |
| **P1-5** | Bug envelope: release calcula desde `sustain`, no desde nivel actual → salto audible, en los **6 engines** | `moog_model_d.cpp:270`, `juno_106.cpp:423`, `prophet_5.cpp:395`, `ob6.cpp:409`, `dx7.cpp:366`, `arp2600.cpp:358` | Snapshot del nivel al entrar a Release. |
| **P1-6** | Clipping por suma de voces: Juno y OB-6 hasta **1.5×**, Prophet **1.25×** con todas las voces | `juno_106.cpp:151-152`, `ob6.cpp:155-159`, `prophet_5.cpp:132-135` | Normalizar ganancias del master mix por nº de voces. |
| **P1-7** | Engine ID: protocolo define 0x00–0x02 pero el código emite 0x10–0x15; OB-6/DX7/ARP no mapean en ESP32 | `protocol.h:134-136`, `28-synth-navigator.cpp:1075`, `view_24_filter.cpp:135` | Añadir IDs 0x03–0x05 al enum y unificar wire format. |
| **P1-8** | Pin 14 doble función (ENC R-B y A0/Volume) | `01 §3.3` líneas 304 y 313 | Reasignar volume pot **antes del PCB**. |
| **P1-9** | Sin entrypoint de producto: `main.cpp` solo-UI; integrador = sketch28 monolito. CI no compila `env:teensy41` ni corre tests | `main.cpp`, `platformio.ini`, `ci.yml` | Crear `env:product`; mover la integración fuera del sketch. |
| **P1-10** | Boot ESP32 ~6.2s (delay 2000 + anim 2200 + splash 2000) vs `<2s` spec | `main.cpp:33`, `screen_boot.cpp:32`, `carousel.cpp:116` | Quitar `delay(2000)`, recortar splash. |
| **P1-11** | Drift de documentación agravado (specs/CLAUDE.md no reflejan sprints 38-45) | `06 §4`, `04 §1.3`, `CLAUDE.md` | Pase de sincronización doc↔código. |

### P2 — Medio (selección)
INT8: cuantización de input parece truncar en vez de redondear (verificar — el comentario dice `round` pero el cast podría truncar) en key/chord/beat → posible −1-2% accuracy; `setDigitalFilterEnabled` doc; Moog mixer default suma 1.20 (`moog_model_d.cpp:53-55`); ModalReverb decay hardcoded a 1.0 (`modal_reverb.cpp:261-263`, infra T60 existe sin usar); PhaseChorus modula nivel wet no el delay (`phase_chorus.cpp:141-149`); **DX7 usa frequencyModulation (no phase-mod) → no es FM auténtico + aliasing en agudos**; **OB-6 usa MoogLadder4P en vez de un SVF tipo Curtis → carácter equivocado**; device_state valida `active_engine` 0-2 pero hay 6 engines (restore falla para 3-5); crossfade de engine y `delay(500)` de boot bloquean `loop()`/`bridge.poll()`; `Serial.flush()` masivo (57×) en bridge_handlers; `carousel_resume()` nunca se invoca.

### P3 — Bajo/mejora
Coeficiente `g` del MoogLadder4P a verificar contra el paper (posible factor 2× en calibración de cutoff, `moog_ladder.cpp:37`); sin oversampling en el ladder (aliasing en auto-oscilación); groove swing 0.17 da 1.41:1 no el 2:1 documentado (`groove.cpp:53`); glide clamp con overshoot de 1 step; cross-mod Prophet con `-=1.0f` en vez de `fmodf`; `micro_error_reporter.h` incluido sin usar (puede romper builds nuevos de TFLM); LFO "solo almacenamiento" (la UI edita pero no suena) en Moog/OB6/DX7/ARP; `new AudioConnection` sin manejo de OOM en fx_manager; statics locales del intérprete TFLite (frágil).

---

## 6. Auditoría por subsistema

### 6.1 Arquitectura de audio + engines (DSP)
**🟢 Salto de categoría.** `audio_graph` resuelve el singleton I2S (P0-1). `MoogLadder4P` (D'Angelo & Välimäki ICASSP 2013) es un VA 4-polo real con saturación `tanh` por etapa y auto-oscilación a k=4, con protección `isfinite`+reset — el sonido boutique ya es **técnicamente creíble**. Los 6 engines siguen el patrón de voice-stealing FIFO.

**Pendientes:** P1-5 (release), P1-6 (clipping de voces), P2 (Moog mixer 1.20, DX7 no-FM, OB-6 filtro). **A verificar:** coef. `g` del ladder (`moog_ladder.cpp:37` usa `4·fs·A·VT`; revisar contra ecs. 13-14 del paper — si el factor correcto es `2·`, el cutoff efectivo está corrido y el tracking V/oct no coincidirá con el analógico) y ausencia de oversampling (aliasing en auto-oscilación cerca de Nyquist). DX7 con `frequencyModulation` no reproduce la descomposición de Bessel del FM real; OB-6 con ladder no replica el SVF Curtis. El crossfade de engine usa `delay()` bloqueante 20ms.

### 6.2 Signature FX + fx_manager
**🟢🟡** `fx_manager` orquesta con new/delete y destructor que libera las `AudioConnection` antes que los nodos (correcto). 9 FX usables; 3 (Spectral Smear, Pitch Mosaic, Glitch Stutter) son v1.1. **Pendientes:** ModalReverb no aplica el T60 por material (el diferenciador "guadua vs cristal" no es audible); PhaseChorus modula nivel wet en vez del delay (no es el chorus CE-2 del spec); fx_manager no maneja OOM en `new` (GhostEcho usa ~134KB).

### 6.3 TinyML Layer 1 + ml_engine + Fase 6
**🟡** `ml_engine` corre key→chord→beat secuencialmente cada 500ms; resultados enviados al ESP32 y consumidos en vistas. Memoria holgada: 3 modelos = **53.4 KB flash** / arenas 56 KB de 200 KB. Fase 6 (auto-harmonize, smart-arp, groove, velocity) implementada y **honestamente documentada como no-ML** (bien para integridad de marketing). **Pendientes:** CMSIS-NN off (P1-2); cuantización de input a verificar (¿trunca en vez de redondear?); las 3 formas distintas del histograma (KS sintético / duración Lakh / conteo firmware) divergen → reality gap sin medir; `04 §1.3` tiene presupuestos fantasma (Auto-Harm 80KB/12ms, Velocity 8KB/3ms) para features que son determinísticas; groove swing 1.41:1 ≠ 2:1 documentado; cero tests de Fase 6; aún no corre en device.

### 6.4 Firmware Teensy (integración + Bridge)
**🟡** sketch28 (~2.6K líneas) cablea los 6 engines + audio_graph + fx_manager + ml_engine + UI + bridge + MIDI con orden de setup correcto (AudioMemory→Wire→codec→engines). Pero es un **monolito** y el env de producción (`main.cpp`) sigue siendo solo-UI. **Bridge:** núcleo conforme (frame, CRC-8 0x07, state machine, heartbeat) pero **falta retry, recovery `GET_VERSION`, resync de framing, multiframe**; NACK 0x06-0x08 sin manejar; engine IDs incompletos (P1-7); param_id namespace (0x00E0-0x00FF) no documentado en el spec. `device_state` valida engine 0-2 (restore falla para 3-5). `delay(500)` en setup bloquea `bridge.poll`.

### 6.5 Firmware ESP32 / UI (28 vistas)
**🟢🟡** Los 4 comandos ML (0x80-0x83) tienen handlers dedicados y **5 vistas nuevas** (filter, ai_models, karaoke, ai_feed, ai_rack) muestran **datos reales** del Teensy. Memoria bien gestionada (1 vista viva, canvases en PSRAM, pool LVGL 96KB, sin leak). **Pendientes:** boot ~6.2s (P1-10); `carousel_resume()` nunca se llama (display congela si el Teensy reinicia); engine-ID latente para engines 3-5; backlight GPIO2 (código) vs GPIO18 (spec) y UART GPIO16/17 vs GPIO43/44 (spec) — el código funciona en el módulo Waveshare real, el spec parece desactualizado; `Serial.flush()` masivo.

### 6.6 Hardware analógico (filtro + power) — sin cambios, todo vigente
**🔴** Re-verificado contra los docs actuales: P0-2 (conversor exp + resonancia "pendiente", `03 §6.2`) y P0-3 (headroom 5V, sin rail negativo, `03 §9`) **siguen vigentes palabra por palabra**. El jig de matching del spec canónico `03 §4.2` sigue describiendo el Arduino con ADC 10-bit incorrecto (el sprint 03 lo corrigió con Teensy 12-bit, pero el SSoT no se actualizó). `tools/matching-jig`, `tools/filter-cal` y KiCad siguen vacíos. El nuevo `MoogLadder4P` **agrava X1**: ahora hay filtro digital en todos los engines + analógico "para todos" → doble-ladder generalizado sin bypass.

---

## 7. Registro de contradicciones de specs

> Re-verificadas contra los docs de `0ae14d1`. Se señalan, **no se resuelven**.

| # | Contradicción | Estado | Nota |
|---|---|---|---|
| **X1** | Filtro "para todos los engines" (03 §6.1) vs "solo Moog" (03 §6.3) | **SIGUE (agravada)** | El MoogLadder4P en todos los engines la vuelve más urgente. |
| **X2** | TPDT bypass presente en 03 §1.1/§2.2/§3.3 vs eliminado en 01 §3.1 | **SIGUE** | El BOM del filtro aún lista el TPDT ($0.50). |
| **X3** | Pin 14 doble función (ENC R-B y A0/Volume) | **SIGUE** | Resolver antes del PCB. |
| **X4** | GPIO 27 "libre" vs 74HC4053 | ✅ **RESUELTA en specs** | `01 §3.3` lo asigna; solo `CLAUDE.md` quedó stale. |
| **X5** | V/oct ±10 cents (diseño) vs ±20 cents (acceptance) | **SIGUE** | Spec vs criterio de QA, dentro del mismo `01`. |
| **X6** | Conteo FX: 12 (05) / 11 (06) / 9 (`fx_manager.h:56`) | **SIGUE** | Tres números; código y roadmap coinciden en 9 implementados. |
| **X7** | Engine ID 0x00-0x02 (protocol.h) vs 0x10+idx (sketch/ESP32) | **PARCIAL** | Enum incompleto (faltan OB-6/DX7/ARP); wire usa 0x10-0x15. |
| **X10** | Fechas v1.0=Q3 2027 vs v1.1=Q1 2027 (05 §4) | **SIGUE** | v1.1 antes que v1.0. |
| **X11** | "6 Kailh switches" (01 §5.1) vs 4 botones (01 §3.4) | **SIGUE** | Acceptance criteria stale. |
| **X12** | Genre Fingerprint en Layer 1 (04 §0.1/§1.3) vs removido (06) | **PARCIAL** | 06 y CLAUDE.md coherentes; `04` aún lo lista en Layer 1. |

**Nuevas (drift de sprints 38-45):** `04 §1.3` lista presupuestos TFLite (80KB/12ms, 8KB/3ms) para features que son determinísticas; tamaños de modelo reales (key 19.4KB) ≠ spec (12KB); `prophet_5.h` y `audio_graph.h` documentan AudioMemory(72/80) cuando el real es 96; `ob6.h` doc dice SVF pero usa MoogLadder4P.

---

## 8. Integridad de marketing

| Claim | Realidad (0ae14d1) | Acción |
|---|---|---|
| "Seis arquitecturas clásicas" | ✅ **6 engines implementados** | Defendible. (Validar el carácter de DX7/OB-6 — ver §6.1.) |
| "12 signature FX" | 9 implementados (3 = v1.1) | "9 al lanzamiento, 12 en la hoja de ruta". |
| "AI 3-layer / TinyML" | Layer 1 corre en código; varios features son reglas/Markov (honesto en código) | Vender **outcomes**, no conteo. Corregir `04 §1.3` (presupuestos fantasma). |
| "Filtro Moog real (1970)" | Diseño incompleto, sin prototipo | **No prometer carácter sonoro sin medición.** |
| "V/oct ±10 cents" | Acceptance dice ±20c (X5) | Publicar el número que QA garantiza. |

El código y los sprint docs 37/42/43/44 son **explícitamente honestos** sobre qué es ML y qué no — esa disciplina es correcta y debe mantenerse. El único desajuste es el SSoT `04` que quedó atrás.

---

## 9. Registro de riesgos

| Riesgo | Prob. | Impacto | Mitigación |
|---|---|---|---|
| Filtro analógico no suena/no calibra en serie | Media | **Crítico** | Spike de protoboard **ya**; es la premisa del precio. |
| Doble-ladder (digital+analógico) destruye el sonido al integrar HW | Alta (si no se atiende) | Alto | Implementar `setDigitalFilterEnabled` por engine + decidir X1. |
| Drift doc → decisiones sobre specs obsoletos | Alta | Medio | Pase de sincronización + disciplina por sprint. |
| Costo real de manufactura > BOM (matching/cal/labor) | Alta | Medio | Re-costear con labor y rechazo; margen 82% es bruto sobre BOM. |
| Sin tests → regresiones silenciosas al integrar | Alta | Medio | Tests nativos de bridge/engines/Fase 6 + `env:teensy41` en CI. |
| Reality gap TinyML (accuracy real ≪ sintética) | Media | Medio | Medir con MIDI real; mezclar Lakh en training. |
| Burnout (solo founder, Brain = prioridad #2) | Media | Alto | Respetar gates de pausa; recortar scope antes que velocidad. |

---

## 10. Tests y CI

**Existe (🟢):** 30 tests de Bridge Protocol (CRC vector `0xF4`, serialización, overflow, SEQ rollover, NACK) — pero **solo en `firmware-esp32/test/`** — y 92 tests de ML en Python (sobre datos sintéticos).

**Gaps (🔴):** `firmware-teensy/test/` vacío → `pio test -e native` del Teensy **pasa trivialmente**; cero tests de engines/FX/UI/ml_engine/Fase 6 (los vectores del sprint 37 no se implementaron); CI no compila `env:teensy41` (producción); sin test de accuracy sobre música real.

---

## 11. Lo que está bien hecho

- **El refactor a `audio_graph`** (singletons compartidos) — resolvió limpiamente el bloqueante #1, con documentación del *por qué* en el header.
- **`MoogLadder4P`** — elegir el modelo D'Angelo & Välimäki (referencia académica sólida) sobre un SVF genérico es la decisión correcta para el carácter boutique.
- **Honestidad de la AI** — documentar Fase 6 como determinística/Markov y no como "modelos de IA" es madurez de producto poco común.
- **UI del ESP32** — 28 vistas con datos reales (incl. ML), memoria planificada en PSRAM, sin leak. Trabajo de UI embebida maduro.
- **Núcleo del Bridge + tests de protocolo** — la parte más crítica para CI, rigurosa.
- **Convenciones** — `snake_case`/`PascalCase`/`#pragma once`/Doxygen, sin `String` de Arduino ni `new` en paths críticos, ISR-safety correcta.
- **El roadmap** sigue siendo la mejor pieza documental (razona tradeoffs y reality gap con honestidad).

---

## 12. Hoja de ruta de remediación

Ordenada por dependencias.

### Fase A — Validar la premisa (antes que nada)
1. **Spike del filtro analógico en protoboard:** definir P0-2 (conversor exp + control de resonancia) y P0-3 (rail negativo), armar con transistores matcheados (cerrar el jig con el circuito Teensy ya corregido), demostrar que suena, auto-oscila y es estable.
2. Actualizar `03-filter-design.md` (jig 12-bit, quitar TPDT, circuitos de control).

### Fase B — Cerrar la integración digital (ya casi está)
3. **`setDigitalFilterEnabled` por engine** + decidir X1 (qué engines usan el analógico) — antes de conectar HW.
4. **Entrypoint de producto:** mover la integración de sketch28 a `main.cpp`/`env:product`; añadirlo al CI.
5. Fixes de audio de alto impacto y bajo costo: release envelope (6 engines), clipping de voces (Juno/OB-6/Prophet), Moog mixer 1.20.
6. Engine IDs (añadir 0x03-0x05 al protocolo y unificar wire) + `device_state` rango 0-5.

### Fase C — Robustez y AI
7. Bridge: retry + recovery `GET_VERSION` + resync + multiframe; `carousel_resume()` en heartbeat-loss.
8. CMSIS-NN on + arenas medidas en device + latencia <20ms verificada.
9. Cuantización (verificar round vs trunc) + medir reality gap con música real.
10. Boot ESP32 <2s (quitar `delay(2000)`).

### Fase D — Higiene de proceso (transversal, mantener)
11. **Pase de sincronización doc↔código** + resolver X1-X12 y los drifts nuevos con el owner.
12. Tests nativos del lado Teensy (bridge, engines, Fase 6) + `env:teensy41` en CI.
13. Resolver pin 14 antes del PCB; documentar el namespace de param_id.

### Fase E — Hardware físico y enclosure
14. KiCad (main + filter + panel); resolver batería (rama enclosure sin pushear) y ubicar jacks FX IN; exportar las 5 piezas; fijación mecánica del bracket; bottom cover; acceso a trimmers; re-costear BOM con labor.

### Fase F — Solo entonces: pre-órdenes y fecha
15. Prototipo end-to-end medido contra `01 §5`; comunicar claims con números reales.

---

## Apéndice A — No verificado (requiere hardware)

- **Audio/filtro:** todo lo del filtro analógico (no existe); aliasing/auto-oscilación reales del MoogLadder4P; corrimiento de cutoff por el coef. `g`; THD+N/SNR del path; clipping audible con voces llenas; oversampling.
- **Bridge/MIDI:** latencias <2ms/<5ms, throughput >1000 fps, parser ante ruido real, recovery físico.
- **TinyML:** latencia <20ms en device, accuracy en música real, arena real post-`AllocateTensors()`, si CMSIS-NN compila con la toolchain de Teensy.
- **ESP32:** FPS con 28 vistas, heap pico, boot real, PSRAM efectiva.
- **Integración:** CPU% con 6 engines + ml_engine + fx_manager; clicks en crossfade; `device_state` restore para engines 3-5.
- **Enclosure:** tolerancias, warping, retención del bracket, guadua en serie.

---

*Fin de la Auditoría Técnica Integral v2 — GrooveForge Brain · commit `0ae14d1` · 2026-05-29*
*Documento de análisis, no SSoT. No modifica `00–06`. Señala contradicciones para decisión del owner.*
