# Temporada 1 — "Construyendo mi synth"

> Biblia de producción, versión 4 (2026-10-31). La Temporada 1 es el **GrooveForge Brain · Studio** (Actos 1 y 2) y,
> cuando el Brain está terminado, **GroovePush** (Acto 3): los dos instrumentos del live set. Un device a la vez, en
> orden (ADR-002, enmienda del 2026-10-31). Material de GroovePush: `groovepush/docs/content/BIBLIA_PRODUCCION_FINAL.md`.
> Decisiones de fondo: [`../adr/ADR-002-comunidad-primero-un-device.md`](../adr/ADR-002-comunidad-primero-un-device.md).
>
> **Guiones listos para grabar:** [`EP01_guion.md`](EP01_guion.md) · [`EP02_guion.md`](EP02_guion.md) ·
> [`EP03_guion.md`](EP03_guion.md). **Cámara, posiciones y ángulos:** [`GUIA_CAMARA.md`](GUIA_CAMARA.md).
> **Primera tanda:** [`PLAN_DOMINGO_1.md`](PLAN_DOMINGO_1.md).
> **Qué grabar mientras llegan las piezas:**
> [`PLAN_GRABACION_STANDBY.md`](PLAN_GRABACION_STANDBY.md).

---

## 1. La idea

**"Estoy construyendo mi propio synth para encontrar mi sonido: primero en mis producciones, después en mi live
set."**

Un productor que busca su sonido construye el instrumento para encontrarlo y lo usa en sus tracks. Nada se vende:
es el proceso, abierto. **La música es la prueba de que la ingeniería funciona.**

### Dos carriles que se alternan

| Carril | Qué es | Se graba con | Ejemplo |
|---|---|---|---|
| 🎹 **Estudio** | Diseño de sonido y producción con el Brain que ya suena | la protoboard + Ableton | del INIT al bajo del track |
| 🛠️ **Taller** | Cómo se diseña el instrumento: caja, esquemáticos, PCB | la pantalla (FreeCAD, KiCad, Claude Design) | el encoder que atravesaba el aluminio |

Alternarlos le da variedad al canal (sonido una semana, diseño la siguiente) y permite grabar **todo el Acto 1 sin
esperar piezas**.

### Dos actos

- **Acto 1 · Encontrar mi sonido y diseñar el instrumento (EP1-EP9):** todo grabable hoy.
- **Acto 2 · El Brain terminado (EP10-EP12):** cuando lleguen las placas: montaje, modo FX, primer ensayo en vivo.
  Termina con el Brain **terminado**: en su caja y en uso.
- **Acto 3 · GroovePush y el live set (EP13-EP17):** recién con el Brain terminado. Rearmar el controlador, la
  grilla, Ableton, la pantalla y la caja. **Final de temporada: el live set con mis dos instrumentos** y los tracks
  de la temporada.

### La fórmula de cada episodio

```
NECESIDAD (musical o física)  →  INGENIERÍA (cómo, y qué falló)  →  RESULTADO (sonido en un track / pieza diseñada)
```

---

## 2. Qué existe hoy

| Capacidad | Estado | Dónde |
|---|---|---|
| 6 engines inspirados en Model D, Juno-106, Prophet-5, OB-6, DX7 (4 op), ARP 2600 | ✅ código · ⚠️ **no todos probados a fondo en el banco** | `src/engines/`, sketch49 |
| Juno: saw + pulse + sub, PWM, HPF 0-3, chorus I / II estéreo, filtro con carácter IR3109 | ✅ | `theory/fidelidad-engines.md` §5, §9.3-9.4 |
| Banco de fábrica: 6 patches clásicos por engine + INIT | ✅ | commit `40ec7cb`, `src/ui/preset_bank.*` |
| Panel, cabeza PERFORM/PANEL, presets, SYNTH FX, MIDI con Ableton (piano roll, clock, CC out) | ✅ protoboard | sketch49, theory/04-08 |
| Enclosure: Claude Design → FreeCAD, cuña de 12°, aluminio 6061 2 mm + PETG 3 mm | ✅ diseño | `variants/studio/design/README.md`, `apps/design/3d-models/studio/studio-enclosure.FCStd` |
| Esquemáticos y PCB (portadora, PCB-A, PCB-K), DRC 0/0/0, fab | ✅ diseño | `apps/hardware/kicad/studio/` |
| Simulación de la etapa de entrada | ✅ | `apps/hardware/kicad/studio/sim/` |
| Modo FX con audio, Ghost Echo en el synth, TinyML en el sketch49 | ❌ | sketch49 línea 33, theory/05 §2 |
| PCBs físicas, caja física | ⏳ | auditoría 2026-10-22 |

**Apertura** (`01-modelo-rampa-y-apertura.md` §3.3, a revisar según ADR-002): snippets de engines ≤ 20 líneas; nada
de pesos de ML. **Marcas:** "inspirado en el Juno-106", no "mi Juno-106".

**IA en esta temporada** (ADR-002, enmienda del 2026-10-22): solo **cameos** de GroovePilot como herramienta que
propone ideas MIDI; el músico elige, edita y decide, y se dice en cámara. Nunca es el centro del episodio.

---

## 3. La temporada

| Ep | Carril | Título de trabajo | Resultado | Grabable |
|---|---|---|---|---|
| 1 | 🎹 | Construyo mi synth para encontrar mi sonido | Bajo con el LFO al tempo de Ableton | ✅ hoy |
| 2 | 🎹 | Del INIT al acorde: encontrando mi sonido en el Juno | Los acordes del primer track, guardados como preset (grabado 2026-10-25) | ✅ grabado |
| 3 | 🛠️ | Diseñando la caja de mi synth | La caja en 3D, validada pieza por pieza | ✅ hoy |
| 4 | 🎹 | Los 6 motores: primera vuelta | El bajo del track (en el Model D) y un sonido útil de cada motor (y los que fallaron) | ✅ hoy |
| 5 | 🛠️ | Del circuito al esquemático | La etapa de entrada simulada antes de fabricar | ✅ hoy |
| 6 | 🎹 | El filtro es mi instrumento | El build-up del primer track | ✅ hoy |
| 7 | 🛠️ | Mi PCB la colocó un script | 3 placas con DRC 0/0/0 y la auditoría que frenó el pedido | ✅ hoy |
| 8 | 🎹 | Que Ableton lo grabe todo | Una toma con su automatización | ✅ hoy |
| 9 | 🎹 | Tocarlo como un instrumento | Un lead tocado con expresión | ✅ hoy |
| 10 | 🛠️ | Llegaron las placas | Montaje y primer encendido | ⏳ piezas |
| 11 | 🎹 | El modo FX: procesar en vivo | Un drop procesado en vivo | ⏳ audio del modo FX |
| 12 | 🎹 | El Brain terminado: primer ensayo en vivo | El Brain en su caja, tocando el primer track en vivo | ⏳ todo lo anterior |
| 13 | 🛠️ | Rearmo mi Push clone | GroovePush en protoboard: primera nota USB-MIDI en Ableton | ⏳ Brain terminado |
| 14 | 🛠️ | 32 pads con su propio cerebro | La grilla NeoTrellis M4 hablando con la Teensy por UART | ⏳ EP13 |
| 15 | 🎹 | Mi controlador maneja Ableton | Clips, escenas y mezcla desde GroovePush (Remote Script) | ⏳ EP14 |
| 16 | 🛠️ | La pantalla y la caja de GroovePush | La pantalla táctil de 7" y el controlador en su caja | ⏳ EP15 + piezas |
| 17 | 🎹 | Mi live set con mis instrumentos | **Final:** el set completo con el Brain y GroovePush | ⏳ todo lo anterior |

**El primer track** atraviesa el Acto 1: el EP2 pone los acordes, el EP4 el bajo y las capas, el EP6 el build-up,
el EP8 la automatización y el EP9 el lead. Al final del Acto 1 el track está terminado y se publica completo.

---

## 4. Fichas

### EP1 — Construyo mi synth para encontrar mi sonido
Guion: [`EP01_guion.md`](EP01_guion.md).

### EP2 — Del INIT al acorde: encontrando mi sonido en el Juno
Guion: [`EP02_guion.md`](EP02_guion.md). Se grabó solo con los acordes; la voz en off está en
[`VOZ_EN_OFF_EP01_EP02.md`](VOZ_EN_OFF_EP01_EP02.md) §3. El bajo pasa al EP4.

### EP3 — Diseñando la caja de mi synth
Guion: [`EP03_guion.md`](EP03_guion.md).

### EP4 — Los 6 motores: primera vuelta (12 min) · 🎹
- **La honestidad como formato:** no todos los motores están probados a fondo. Este episodio es esa prueba,
  grabada: cada motor tiene 90 s para dar un sonido útil para el track. El que falla, falla en cámara.
- **Hook:** el mismo acorde por los 6 motores, 1 s cada uno. VO: *"Programé seis synths. Algunos nunca los había
  tocado de verdad. Hoy los pruebo todos, y no todos van a pasar."*
- **Bloques:** (1) las reglas del juego: 90 s por motor, partir del banco de fábrica, un sonido para el track. (2)
  Model D (que da el **bajo del track**, el que no entró en el EP2), Prophet, OB-6, DX7, ARP (el Juno ya fue en el
  EP2). (3) Lo que falló y por qué, con el monitor serie o la
  tabla de `fidelidad-engines.md` §0 / §8. (4) Las capas que entran al track.
- **Clímax:** el loop del EP2 con las capas nuevas.
- **Shorts:** Mind-blow *"Mi Model D ignora la velocity, como el original"* (commit `27b0361`) · ASMR: el acorde por
  los 6 motores · Polémica: *"El motor que no pasó la prueba"* (el que haya fallado, con el porqué).
- **Ojo:** si un motor se cuelga o suena mal, no es un fracaso del episodio: anota el commit y el síntoma. Es
  material para un Short de bug y para arreglarlo.

### EP5 — Del circuito al esquemático (12 min) · 🛠️
- **Hook:** la simulación diciendo "−10.36 dB" y la frase: *"Esta parte de la placa no la voy a poder probar en
  protoboard, porque la ensambla una máquina en China. Así que la simulé."*
- **Bloques:** (1) qué hace la entrada: meter un sintetizador o un Eurorack caliente al codec sin romper nada. (2) El
  circuito por partes: pad 0/−10/−20 dB, protección BAT54S, centrado en VREF 1.65 V, seguidor MCP6004
  (`variants/studio/theory/03-studio-input-stage.md` §1). (3) KiCad: hojas, el conector como contrato
  (`portadora.kicad_sch`, `docs/portadora-schematic.pdf`). (4) La simulación (`sim/input_stage_sim.py`,
  `sim/results.md`): pad −10.36 / −20.33 dB, CV con 5.2 ms de asentamiento. (5) Lo que la simulación **no** dice
  (auditoría 2026-10-22 §Audio): no es THD real, ni ruido de la Pi, ni ESD.
- **Clímax:** correr la simulación en vivo y ver los 6 criterios pasar.
- **Shorts:** Mind-blow *"Simulé mi circuito porque no lo puedo probar"* · ASMR: el esquemático armándose en KiCad
  (time-lapse) · Polémica: *"La simulación dice THD 0.001 %. No te lo creas"*.

### EP6 — El filtro es mi instrumento (12 min) · 🎹
- **Hook:** build-up de 8 compases, la mano en ENC L abriendo el filtro hasta la auto-oscilación. VO: *"Este barrido
  tenía un problema: arriba de 2 kHz el filtro se desafinaba casi 20 semitonos."*
- **Bloques:** (1) el filtro como instrumento en la electrónica. (2) La resonancia corrida: −1913 ¢ a 5 kHz → ±5 ¢
  de 110 Hz a 12 kHz (`fidelidad-engines.md` §8, §9.4; snippet de `src/dsp/zdf_ladder.h`). (3) El cutoff que no
  bajaba de 83 Hz (commit `a3c588c`). (4) La aceleración del encoder (commit `625c304`).
- **Clímax:** el build-up y el drop del primer track.
- **Shorts:** Mind-blow *"Mi filtro desafinaba 19 semitonos al resonar"* · ASMR: auto-oscilación tocando una melodía,
  con el osciloscopio de kit en la salida · Polémica: *"Ableton me arruinaba el filtro (y no era su culpa)"*.

### EP7 — Mi PCB la colocó un script (14 min) · 🛠️
- **Hook:** time-lapse de `place_pcb_a.py` + `route_board.py` llenando la PCB-A, hasta "DRC: 0 violations". VO:
  *"Los componentes de esta placa los colocó un script, a partir de las coordenadas de la caja del episodio 3."*
- **Bloques:** (1) de la caja a la placa: la misma tabla de coordenadas del panel. (2) Colocar y rutear
  (`apps/hardware/kicad/studio/tools/`: `place_*.py`, `grid_route.py`, `finish_routing.py`, `clean_vias.py`,
  `add_gnd_pours.py`). (3) El contrato del cable: `set_idc_pinout.py` genera los dos lados del IDC. (4) Reglas de
  JLC y costo (`lecciones-para-groovepush.md` §1-§2). (5) La auditoría del 2026-10-22: F1 pin 13 con LED, F2 USB-C
  sin Rd, F3 corriente, F4 selector DP3T. **Cerrar con el estado real** del pedido el día de grabar.
- **Clímax:** el visor 3D de las 3 placas y la decisión del pedido.
- **Shorts:** Mind-blow *"Nunca le pongas INPUT_PULLUP a este pin"* · ASMR: time-lapse del router · Polémica:
  *"Mi PCB pasó DRC y estaba mal"*.

### EP8 — Que Ableton lo grabe todo (12 min) · 🎹
- **Hook:** Ableton reproduce una toma; nadie toca el Brain, pero el cutoff se mueve solo en la pantalla.
- **Bloques:** External Instrument (theory/04 §9), mapa de CC (theory/07 §3), MIDI thru, clock → LFO SYNC, el eco y
  los 150 ms.
- **Clímax:** 16 compases grabados con automatización, editados en Ableton.
- **Shorts:** *"Mi synth graba su propia automatización"* · perilla + curva en pantalla partida · *"El MIDI de 7
  bits no alcanza para un filtro"*.

### EP9 — Tocarlo como un instrumento (12 min) · 🎹
- **Hook:** un clic, el valor no cambia; otro clic, cambia. VO: *"Mi synth se comía un clic de cada dos."*
- **Bloques:** 2 pulsos por detent (commit `625c304`), histéresis (`src/ui/detent.h`), aftertouch → cutoff, pitch
  bend, sustain (theory/07 §2), velocity fiel por motor, anillo de LEDs.
- **Clímax:** el lead del primer track tocado a mano. **Cierre del Acto 1: el track completo.**
- **Shorts:** *"15 líneas que arreglan cualquier encoder barato"* · macro del encoder y el anillo · *"Mi Model D
  ignora la velocity a propósito"*.

### EP10 — Llegaron las placas · ⏳
Unboxing, inspección, continuidad sin módulos, primer encendido con límite de corriente, montaje en la caja
(orden de la auditoría §"Orden de cierre", punto 5). Clímax: el Brain en su caja tocando el primer track.

### EP11 — El modo FX: procesar en vivo · ⏳ requiere el audio del modo FX en el sketch49
Inspiración RMX-1000 (ADR-004), capas INSERT / SEND / MASTER, CPU, la entrada de audio. Clímax: un drop procesado.

### EP12 — El Brain terminado: primer ensayo en vivo · ⏳
La rutina con el Brain solo, qué falló en el ensayo, el primer track tocado en vivo. Cierra "Brain terminado"
(ADR-002 §2.1) y abre el Acto 3: *"Para el set me falta algo: controlar Ableton con mis manos."*

### Acto 3 — GroovePush y el live set (EP13-EP17) · ⏳ con el Brain terminado
Fichas a escribir cuando empiece el acto, a partir de `groovepush/docs/content/BIBLIA_PRODUCCION_FINAL.md` (su EP1
"Desarmé mi Push clone para construir un synth" es el gancho natural del EP13: lo desarmé para el Brain, ahora lo
rearmo para el set). Contratos técnicos: `groovepush/CLAUDE.md` (performance MIDI por USB, < 5 ms; nunca por la red).
- **EP13 · Rearmo mi Push clone:** del controlador desarmado a la primera nota USB-MIDI (ROADMAP de GroovePush,
  bloque 1).
- **EP14 · 32 pads con su propio cerebro:** la NeoTrellis M4, el UART a 1 Mbps, el protocolo binario con checksum.
- **EP15 · Mi controlador maneja Ableton:** el Remote Script, clips y escenas, por qué el MIDI de la actuación no
  pasa por la red.
- **EP16 · La pantalla y la caja:** la Raspberry Pi 5 con la pantalla de 7", la GUI, el enclosure.
- **EP17 · Mi live set con mis instrumentos (final):** el set completo, qué falló en los ensayos, los tracks de la
  temporada. El set entero, como video aparte. **Después de este episodio se revisa el modelo de negocio (ADR-004).**

---

## 5. En todos los episodios

- **Intro de 15 s máximo**, sin logo ni saludo. Resultado o problema en el segundo 0.
- **Segmento "el track" (≈ 1 min, en los 🎹):** dónde entra lo de hoy en el track.
- **Cierre:** qué falta y qué viene, con el estado real del repo.
- **CTA único** (sin venta, ADR-002): "déjame tu correo para seguir el proceso" o "comenta qué sonido quieres que
  construya".
- **Audio:** LINE OUT → interfaz → Ableton, 48 kHz / 24 bit, en paralelo con todas las tomas 🎹.
- **Pantalla:** ruta del archivo visible, fuente ≥ 20 pt.

## 6. GroovePush

Acto 3 de esta temporada (EP13-EP17), después del Brain terminado. Material ya escrito:
`groovepush/docs/content/BIBLIA_PRODUCCION_FINAL.md`. Si las placas del Brain se atrasan mucho, se puede adelantar
**solo contenido de diseño ya hecho** de GroovePush (carril Taller, como el EP3/EP5/EP7 del Brain), sin empezar a
construirlo: eso lo decide Juan.
