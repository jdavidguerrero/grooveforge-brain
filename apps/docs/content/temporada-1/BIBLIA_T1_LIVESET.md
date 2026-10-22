# Temporada 1 — "Construyendo mi synth"

> Biblia de producción, versión 2 (2026-10-22). Reemplaza el enfoque de
> `groovepush/docs/content/BIBLIA_PRODUCCION_FINAL.md`: la Temporada 1 es **solo el GrooveForge Brain · Studio**.
> El material de GroovePush (Push clone, PCBs del panel) pasa a la Temporada 2.
> Guion completo del piloto: [`EP01_guion.md`](EP01_guion.md).
> Decisiones de fondo: [`../adr/ADR-002-comunidad-primero-un-device.md`](../adr/ADR-002-comunidad-primero-un-device.md)
> (comunidad primero, sin monetizar; un device a la vez; GroovePilot en segundo plano; sin IA en esta temporada).

---

## 1. La idea en una frase

**"Estoy construyendo mi propio synth para encontrar mi sonido: primero en mis producciones, después en mi live set."**

No es un tutorial de electrónica ni un review de un synth. Es un productor que busca su sonido, construye el
instrumento para encontrarlo y lo usa en sus tracks, episodio tras episodio. **La música es la prueba de que la
ingeniería funciona.** Nada se vende: es el proceso, abierto.

**Dos actos:**
- **Acto 1 · Encontrar mi sonido (EP1-EP6):** el Brain en el estudio. Cada episodio deja un sonido, un preset o una
  toma en un track.
- **Acto 2 · Llevarlo al escenario (EP7-EP9):** el modo FX, la caja y el live set. Termina con el Brain **terminado**:
  en su enclosure y en uso.

### La fórmula de cada episodio

```
NECESIDAD musical       →  INGENIERÍA (cómo lo construí, qué falló)  →  USO en un track / en el set
"necesito que el bajo       "el filtro desafinaba al resonar;           "este es el drop con
 abra en el drop"            así lo medí y lo arreglé"                   ese barrido"
```

- El **hook** muestra el resultado musical o el problema sonando.
- El **clímax** es siempre musical: un loop, una toma, un fragmento de track hecho con el Brain.
- Cada episodio deja una pieza de **mi sonido** en un track (un sonido, una toma, un preset); en el Acto 2, del set.

### La regla que ordena la temporada

El final es el **Brain terminado** (en su caja, sin protoboard) y un **live set tocado con él**. Cada episodio tiene su **deuda de ingeniería**: lo que hay que
terminar antes de grabarlo. Así, el calendario de publicación es también el plan de trabajo del Brain
(`01-modelo-rampa-y-apertura.md` §1: "el contenido es el deadline").

---

## 2. Qué existe hoy (lo que se puede mostrar)

Fuente: `apps/firmware-teensy/src/sketches/49-studio-brain.cpp` y `variants/studio/theory/04-08`.

| Capacidad | Estado | Dónde |
|---|---|---|
| 6 engines (Model D, Juno-106, Prophet-5, OB-6, DX7 de 4 operadores, ARP 2600) | ✅ | `src/engines/`, sketch49 |
| Panel: ENC L cutoff · ENC R resonancia · P1/P2 · NAV con anillo de 12 LEDs · B1-B6 | ✅ protoboard | ADR-004, theory/04 |
| Cabeza Pi 3B + 5": PERFORM con onda y filtro animados, PANEL, kiosko | ✅ | `apps/studio-head`, theory/08 |
| Teclado por USB host + MIDI thru al computador para grabarlo | ✅ | sketch49, theory/04 §9 |
| Ableton → Brain: piano roll y clock (LFO sincronizado al BPM) | ✅ | sketch49 `clock_tick()` |
| Brain → Ableton: cada perilla sale por CC y se graba como automatización; eco ignorado 150 ms | ✅ | theory/07 |
| Pitch bend, aftertouch → cutoff, sustain, velocity fiel por engine | ✅ | theory/07, commit `27b0361` |
| Cadena SYNTH FX: BIT → TAPE → CHORUS → MODAL (~6 % de CPU) | ✅ | theory/05 |
| Presets: 16 por engine, LittleFS, banco de fábrica | ✅ | theory/06, commit `40ec7cb` |
| **Modo FX (B5) con audio**: procesar el master o la entrada en vivo, estilo RMX-1000 | ❌ la página navega, el audio no está en el sketch49 | sketch49 línea 33 |
| Ghost Echo en la cadena del synth | ❌ no entra en AudioMemory (130 vs 96 bloques) | theory/05 §2 |
| TinyML (key/chord/beat) en el sketch49 | ❌ | sketch49 línea 33 |
| PCB y enclosure físicos | ❌ pedido retenido | `apps/hardware/kicad/studio/docs/auditoria-fabricacion-2026-10-22.md` |

**Política de apertura** (`01-modelo-rampa-y-apertura.md` §3.3): del código de los engines, snippets de ≤ 20 líneas y
fórmulas, nunca un `.cpp` completo. De los modelos de ML, nada de pesos, dataset ni receta. FX, enclosure, protocolo y
errores: abiertos.

---

## 3. La temporada (9 episodios)

| Ep | Título de trabajo | Necesidad musical | Ingeniería | Clímax musical | Deuda antes de grabar |
|---|---|---|---|---|---|
| 1 | Construyo mi synth para encontrar mi sonido | "Quiero un sonido propio y tocarlo con las manos, no con el mouse" | El banco, la arquitectura, la regla del audio | Ableton con el beat + el Brain tocando el bajo con el LFO al tempo | Probar en banco el clock → LFO SYNC ✅ |
| 2 | 6 synths: ¿cuál es mi sonido? | Encontrar el sonido de cada parte de mi track | Engines, fidelidad medida, tabla honesta | Un loop de 8 compases: bajo Model D, pad Juno, lead Prophet | — |
| 3 | El filtro es mi instrumento | El barrido del drop | Ladder ZDF: −1913 ¢ → ±5 ¢; cutoff log; eco del DAW | Un build-up con el barrido del filtro | — |
| 4 | Que Ableton lo grabe todo | Grabar una toma con su automatización | USB-MIDI, CC map, MIDI thru, clock | Toma grabada; Ableton la reproduce sola, las perillas "se mueven" en la pantalla | — |
| 5 | Tocarlo como un instrumento | Que responda como un synth de verdad | Detents con histéresis, aceleración, aftertouch, sustain, velocity por engine | Una toma tocada a mano, con expresión | — |
| 6 | Mi sonido no se borra | Llegar al show con mis sonidos listos | Presets en LittleFS (355 B), SYNTH FX (~6 % CPU), por qué Ghost Echo no entra | El banco de presets del set; apagar, encender, cargar | — |
| 7 | El modo FX: procesar en vivo | Meter el master del set por los efectos del Brain | Modo FX estilo RMX-1000, capas INSERT/SEND/MASTER, HOLD, CPU | Un drop con Granular / Spring sobre el set | **Audio del modo FX (B5) en el sketch49** |
| 8 | De protoboard a instrumento | No puedo llevar una protoboard al escenario | PCBs, auditoría (pin 13, USB-C, corriente), enclosure | Abrir la caja de JLC / primer encendido | **Cerrar F1-F4, pedir, recibir** |
| 9 | Mi primer live set con el Brain | El show | Todo junto: set de 3 tracks, rutina, qué falló | Set en vivo (completo en video aparte) | EP7 + EP8 + los 3 tracks |

**Orden de publicación:** 1 → 2 → 3 → 4 → 5 → 6 se pueden grabar con lo que existe hoy. Entre el 6 y el 7 queda el
tiempo para construir el modo FX y cerrar la auditoría. Si la PCB se atrasa, el EP9 se toca **en la protoboard**:
también es una historia (y muy auténtica).

---

## 4. Fichas

Para cada episodio: hook (0-15 s), bloques con archivos en pantalla, clímax, 3 Shorts (Mind-blow · Build/ASMR ·
Polémica). El EP1 tiene guion palabra por palabra en [`EP01_guion.md`](EP01_guion.md).

### EP1 — Construyo mi synth para encontrar mi sonido
Ver `EP01_guion.md`.

### EP2 — 6 synths: ¿cuál es mi sonido? (13 min)
- **Hook:** el mismo acorde por los 6 engines, 1 s cada uno, con el nombre en pantalla. VO: *"Seis synths clásicos en
  una placa de 30 dólares. Hoy elijo cuáles van en mi track, y te digo cuál todavía no suena como el original."*
- **Bloques:** (1) el track del episodio: tempo, tonalidad, qué necesita cada parte. (2) La tabla de fidelidad
  (`theory/fidelidad-engines.md` §0, estimación de 2026-10-16) y lo que se arregló después (§9.4). (3) Por qué cada
  engine suena como suena: polyBLEP −23 → −70 dB de aliasing (§8); envelopes RC (`src/dsp/analog_env.h`); un filtro
  por chip (§4). (4) Diseño de sonido en la protoboard: bajo en Model D, pad en Juno con chorus estéreo, lead en
  Prophet.
- **Clímax:** los 8 compases con las tres capas, grabados en Ableton.
- **Shorts:** Mind-blow *"Mi Juno no tenía estéreo"* (chorus mono → dos BBD simulados, correlación 0.78, §9.4) ·
  ASMR: el acorde por los 6 engines · Polémica: *"¿Para qué un Prophet de miles de dólares?"*: lo que mi Prophet hace y
  lo que no (tabla §0).

### EP3 — El filtro es mi instrumento (12 min)
- **Hook:** build-up de 8 compases, la mano en ENC L abriendo el filtro hasta que auto-oscila. VO: *"Este barrido
  tenía un problema: arriba de 2 kHz el filtro se desafinaba casi 20 semitonos."*
- **Bloques:** (1) por qué en música electrónica el filtro es un instrumento. (2) La resonancia corrida: −859 ¢ a
  1.76 kHz, −1913 ¢ a 5 kHz → ±5 ¢ de 110 Hz a 12 kHz con el ladder ZDF (`fidelidad-engines.md` §8, §9.4; snippet de
  `src/dsp/zdf_ladder.h`). (3) El cutoff que no bajaba de 83 Hz: CC 74 lineal y el eco de Ableton (commit `a3c588c`).
  (4) La aceleración del encoder del cutoff (commit `625c304`).
- **Clímax:** el build-up y el drop del track del EP2 con el barrido grabado.
- **Shorts:** Mind-blow *"Mi filtro desafinaba 19 semitonos al resonar"* · ASMR: auto-oscilación tocando una
  melodía, con el osciloscopio de kit en la salida · Polémica: *"Ableton me arruinaba el filtro (y no era culpa de
  Ableton)"*: el eco del MIDI.

### EP4 — Que Ableton lo grabe todo (12 min)
- **Hook:** Ableton reproduce una toma; nadie toca el Brain, pero el cutoff de la pantalla se mueve solo. VO: *"No lo
  estoy tocando. Ableton está tocando mis perillas."*
- **Bloques:** (1) External Instrument, MIDI To = Teensy MIDI, Monitor en Off al grabar (theory/04 §9). (2) El mapa
  de CC (theory/07 §3): cada perilla sale por su CC. (3) MIDI thru: el teclado del USB host se graba en Ableton. (4)
  Clock: 24 ppq → BPM → LFO SYNC (`clock_tick()` en sketch49). (5) El eco y los 150 ms.
- **Clímax:** grabar 16 compases tocando y moviendo perillas; reproducir; editar la automatización en Ableton.
- **Shorts:** Mind-blow *"Mi synth graba su propia automatización"* · ASMR: perilla girando + la curva dibujándose en
  Ableton (pantalla partida) · Polémica: *"El MIDI de 7 bits no alcanza para un filtro"* (128 pasos, la escala log).

### EP5 — Tocarlo como un instrumento (12 min)
- **Hook:** cenital: un clic, el valor no cambia; otro clic, cambia. VO: *"Mi synth se comía un clic de cada dos. Así
  no se toca en vivo."*
- **Bloques:** (1) 2 pulsos por detent y el divisor de 4 (commit `625c304`). (2) El temblor en reposo y la histéresis:
  `src/ui/detent.h` completo (abierto). (3) Aftertouch → cutoff (+1 octava) y vibrato; pitch bend ±2; sustain que
  guarda las notas (theory/07 §2). (4) Velocity fiel: el Model D y el Juno-106 la ignoran, como los originales
  (commit `27b0361`). (5) Las luces: anillo de 12 bajo NAV, un LED por tecla, un solo `write` por frame.
- **Clímax:** una toma de lead tocada con aftertouch y pitch bend sobre el loop.
- **Shorts:** Mind-blow *"15 líneas que arreglan cualquier encoder barato"* · ASMR: macro del encoder y el anillo de
  LEDs siguiendo el giro · Polémica: *"Mi Model D ignora la velocity a propósito"*.

### EP6 — Mi sonido no se borra (11 min)
- **Hook:** apagar el Brain a mitad de una nota, encenderlo, cargar el preset: mismo sonido. VO: *"Si mi synth olvida
  el sonido, no hay show."*
- **Bloques:** (1) Qué guarda un preset: 355 bytes (theory/06 §2). (2) EEPROM (~19 presets) vs LittleFS vs SD:
  por qué LittleFS. (3) La cadena SYNTH FX: orden de pedalboard, ~6 % de CPU, `setBypass` para no tener clicks
  (theory/05 §3). (4) Por qué Ghost Echo no entra: 375 ms ≈ 130 bloques y el pool es de 96 (§2).
- **Clímax:** el banco de presets del live set: uno por track, nombrados y probados.
- **Shorts:** Mind-blow *"Un sonido entero cabe en 355 bytes"* · ASMR: recorrer los presets del set con NAV, la cabeza
  cambiando · Polémica: *"El delay que no le cabe a mi synth"*.

### EP7 — El modo FX: procesar en vivo (13 min) · ⏳ requiere implementar el audio del modo FX
- **Hook:** el master del set entra por el Brain; HOLD apretado, el Granular congela el drop; se suelta y vuelve.
- **Bloques:** (1) la inspiración: Roland RMX-1000 y el uso en vivo (ADR-004, tabla de controles en modo FX). (2)
  Capas INSERT / SEND / MASTER (`05-fx-architecture.md`). (3) Presupuesto de CPU: Granular 8-18 % estimado vs ≤ 60 %
  total. (4) La entrada: pad 0/−10/−20 dB, VREF 1.65 V (`variants/studio/theory/03-studio-input-stage.md`).
- **Clímax:** un drop del set procesado en vivo.
- **Deuda:** el motor del modo FX en el sketch49 (hoy vive en el sketch 28) y la entrada de audio montada.

### EP8 — De protoboard a instrumento (14 min) · ⏳ requiere el pedido
- **Hook:** el carrito de JLC; se cierra la pestaña. VO: *"DRC cero errores, y no pagué."* Después, la caja que llega.
- **Bloques:** (1) las 3 placas de Studio y el enclosure. (2) La auditoría del 2026-10-22: F1 el pin 13 con su LED,
  F2 USB-C sin Rd, F3 la cuenta de corriente, F4 el selector DP3T. (3) Cómo se cerró cada uno. (4) Primer encendido
  con límite de corriente.
- **Clímax:** el Brain en su caja tocando el loop del EP2.
- **Deuda:** cerrar F1-F4, pedir, recibir, montar.

### EP9 — Mi primer live set con el Brain (15 min + set completo aparte) · ⏳
- **Hook:** 15 s del set en vivo, plano general y la mano en el filtro.
- **Bloques:** la rutina (presets, clock, modo FX), qué falló en el ensayo, el set.
- **Clímax:** el set. Publicar el set completo como video aparte.

---

## 5. Lo que se repite en todos los episodios

- **Intro de 15 s máximo**, sin logo ni saludo. Resultado o problema en el segundo 0.
- **Segmento fijo "el track" (≈ 1 min, al final):** dónde va lo de hoy en el track/set. Construye la serie.
- **Cierre:** qué falta y qué viene, con el estado real del repo.
- **CTA único** (nada de venta, ADR-002): "déjame tu correo para seguir el proceso" o "comenta qué sonido quieres
  que construya".
- **Grabación de audio:** LINE OUT del Brain → interfaz → Ableton, 48 kHz / 24 bit, en paralelo con todas las tomas.
- **Pantalla:** ruta del archivo visible, fuente ≥ 20 pt.

## 6. Material de GroovePush

Queda para la Temporada 2 (el Push clone como controlador del set y su hermano del Brain). Los episodios y Shorts ya
escritos están en `groovepush/docs/content/BIBLIA_PRODUCCION_FINAL.md` (EP1, EP6 y parte del EP7).
