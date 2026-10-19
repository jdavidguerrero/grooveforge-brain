# Studio · Estado de UI en el Teensy y su paso a la cabeza

> **Fecha:** 2026-10-15 · **Estado:** APROBADO por Juan el 2026-10-15 (navegación en el Teensy y enmienda al `02` §2.5)
> **Depende de:** ADR-002 (Studio, punto 4 navegación y 5 touch), ADR-004 (panel), theory/01 (cabeza),
> `02-bridge-protocol.md` (enmienda propuesta en §5).

## 1. El problema

La cabeza de la Pi ya navega sola: `apps/studio-head/src/ui/app.cpp` (`App::handle`) implementa toda la
navegación de la UI v3 con teclado y touch. Pero en el hardware los controles físicos están cableados al
Teensy, y el bridge no tiene ningún mensaje que lleve la navegación: hoy, girar NAV o apretar B1 no le
llega a la pantalla. Hay que decidir quién es el dueño de "en qué página estoy y qué edita cada control".

## 2. Decisión: el dueño es el Teensy

| | Teensy dueño (elegida) | Cabeza dueña |
|---|---|---|
| Si la Pi se cuelga | P1/P2, botones y engine siguen funcionando | P1/P2 dejan de editar (dependen del mapeo de página) |
| Principio rector de theory/01 §1 ("el audio nunca depende de la cabeza") | se cumple | se rompe |
| Coherencia con la cabeza comercial (ESP32) | igual: el Teensy decide y la cabeza muestra | distinta |
| Trabajo | portar la máquina de páginas al firmware | reenviar eventos (poco) |

**Regla:** la cabeza es una vista. Dibuja `StudioState`, que llega entero por el bridge: valores musicales
(PARAM_CHANGED, ENGINE_CHANGED, ya implementados) y ahora también el **estado de UI** (§4). La cabeza no
navega por su cuenta con los controles físicos.

El touch (ADR-002 punto 5, "atajo secundario") pasa a ser **un control más del Teensy**: la cabeza manda
el gesto (§5.2) y el Teensy lo procesa igual que un botón. Así hay una sola máquina de estados.

## 3. La máquina de páginas (se porta tal cual)

Fuente de verdad del comportamiento: `App::handle` / `handle_synth` / `handle_fx` de la cabeza, que ya
siguen la UI v3 y ADR-004. Se traslada a `apps/firmware-teensy/src/ui/studio_ui.{h,cpp}` como lógica pura
(sin hardware, testeable en `native`):

- **SYNTH:** NAV gira PERFORM → AI → SPECTRUM → PANEL (PANEL agregada el 2026-10-19, fidelidad §11.7). B1 OSC, B2 ENV, B3 LFO / MOD (la misma tecla vuelve a la
  última vista). B4 PRESETS → ENGINE SELECT → PERFORM. En OSC, NAV elige oscilador; en LFO, NAV push
  alterna SYNC; en ENGINE/PRESETS, NAV mueve el cursor y NAV push carga. B6 = SHIFT (momentáneo en el
  hardware). ENC L push = bypass del filtro.
- **FX:** B5 MODE alterna SYNTH ⇄ FX y vuelve a la última vista. NAV elige slot, NAV push lo enciende,
  B1–B3 rutean (INSERT/SEND/MASTER), B4 salta al favorito, B6 = HOLD (momentáneo).
- **Error de enlace:** la página de error la decide la cabeza (es su propio estado de conexión), no viaja.

Lo musical que hoy simula la cabeza en `App` (cargar engine o preset, encender un FX) pasa a ser una
acción del Teensy: cambia el sonido y después avisa por PARAM_CHANGED / ENGINE_CHANGED como siempre.

### 3.1 Cuatro encoders por página y home (aprobado por Juan, 2026-10-16)

- **Pulsadores de P1 y P2** (pines 32 y 9, ADR-004; agregados 2026-10-19): en la parada LFO, P1 push = siguiente
  forma de onda (en círculo). Siguen disponibles SHIFT + P1.
- **Home = las vistas** (PERFORM, AI, SPECTRUM, PANEL): ENC L / R son CUTOFF / RES, el filtro global de todos los engines.
- **En las páginas de edición** (OSC, ENV, LFO, SYNTH FX) los **4 encoders son controles de la página**: P1, P2,
  ENC L, ENC R. Con SHIFT, otros 4 (capa reservada para las paradas por engine; hoy vacía).
  Antes eran 2 directos (P1 / P2) + 2 con SHIFT, y ENC L / R siempre eran el filtro.
- **Volver a home:** la misma tecla de la página (como antes) o **NAV push largo (≥ 500 ms) desde cualquier página**.
  El push corto conserva su función en cada página (cargar, SYNC, encender FX) y se dispara al soltar.
- Tradeoff aceptado: en una página de edición no se barre el filtro sin volver a home (un gesto).
- El modo FX no cambia: sus 4 encoders ya eran del FX (`05` §1.13).
- El control nuevo `NAV_LONG` se agrega al final de los ids de §5.2 para no mover los existentes.

## 4. Qué es "estado de UI"

Lo que la cabeza necesita para dibujar la página correcta y que no sale de los valores musicales:

| Campo | Tipo | Notas |
|---|---|---|
| `mode` | u8 | 0 SYNTH, 1 FX |
| `page` | u8 | mismo enum que `Page` de la cabeza: 0 PERFORM, 1 AI, 2 SPECTRUM, 3 ENV, 4 FX, 5 ENGINE, 6 PRESETS, 8 OSC, 9 LFO, 10 SYNTH FX, 11 PANEL (7 = error, nunca se manda) |
| `shift` | u8 | 0/1 |
| `fx_hold` | u8 | 0/1 |
| `cursor` | u8 | ENGINE: tile 0–5 · PRESETS: fila (0 = SAVE) · OSC: oscilador 0–3 |
| `fx_slot` | u8 | 0–11 |
| `fx_view` | u8 | 0 ONDA, 1 ESPECTRO, 2 FIRMA |
| `flags` | u8 | bit0 filter_bypass, bit1 lfo_sync |
| `fx_on` | u16 LE | bit n = slot n encendido |
| `fx_route` | 12 × u8 | 0 INSERT, 1 SEND, 2 MASTER |

Son **22 bytes**. Se manda **entero** (no deltas): idempotente, "el último valor gana" (theory/01 §3.2) y
la reconexión converge con el primer frame.

## 5. Enmienda propuesta al `02` (rango libre 0x44–0x4F de UI)

### 5.1 `0x44 UI_STATE` (Teensy → cabeza)

- Payload: los 22 bytes de §4, en ese orden, little-endian.
- **Cuándo:** en cada cambio, como máximo cada 20 ms (coalescido: si cambia varias veces en 20 ms va el
  último), y además una vez por segundo junto al HEARTBEAT para converger tras un corte.
- La cabeza contesta ACK como a todo frame. LEN distinto de 22 → NACK INVALID_LEN.

### 5.2 `0x45 UI_INPUT` (cabeza → Teensy)

- Payload: `[control: u8, value: i8]`. `control` usa los mismos ids de §3 (NAV, NAV_PUSH, B1–B6, MODE,
  ENC_L_PUSH, PICK_n, FX_VIEW_n, SWIPE…); `value` es el paso del giro o 1/0 de apretar/soltar.
- El Teensy lo trata igual que el control físico y contesta ACK. **Fase 2**: primero va §5.1.

Enmienda aplicada al `02` §2.5 el 2026-10-15 (instrucción explícita de Juan). En `protocol.h` van como
`GF_CMD_UI_STATE` y `GF_CMD_UI_INPUT`, con las funciones de empaquetado.

## 6. Cómo se prueba

| Qué | Cómo | Dónde |
|---|---|---|
| Máquina de páginas | tests Unity con secuencias de controles → página/cursor esperados (copiados del comportamiento de `App`) | `pio test -e native` |
| Serialización de UI_STATE | roundtrip 22 bytes + LEN inválido | native, `apps/bridge-protocol/test/` |
| E2E | girar NAV / apretar B1–B6 en el panel → la pantalla de la Pi cambia en < 50 ms | protoboard, Pi 3B |
| Pi caída | matar studio-head: P1/P2 siguen editando el sonido | protoboard |

## 7. Qué no cubre

- Los parámetros que editan P1/P2 en cada página (están en la UI v3 y ADR-004; el firmware ya los tiene
  para PERFORM y FILTER).
- El orden de integración en el sketch 28 / `main.cpp` (depende de la corrección del cuelgue, aún sin
  commit).
- Espectro y flujos de FX Viz (siguen pendientes de la enmienda de rango 0x90–0xEF de theory/01 §12).

## 8. Luces del panel (agregado 2026-10-16)

**Una sola cadena WS2812 en el pin 28** (decisión de Juan, 2026-10-16): el anillo de 12 LEDs bajo NAV (LEDs 0–11)
y un WS2812 suelto por tecla B1…B6 (LEDs 12–17). Reemplaza a los APA102 en los pines 36/37 de ADR-004 rev. 3 y
vuelve a los 18 LEDs del pin 28 de `01` §3.3; el pin 28 pasa por el buffer 74AHCT1G125 de la portadora, así que
los datos llegan a 5 V. Las luces salen del mismo estado de UI que se manda a la cabeza: no hay otra lógica.

- **Anillo (solo NAV):** muestra la posición de lo que NAV elige en la página visible, como un arco de los 12 LEDs:
  vista (3), ENGINE (6), SYNTH FX (4), OSC (osciladores del engine), ENV (AMP / FILTER), LFO (5 formas),
  PRESETS (filas) y, en FX, el slot (12, un LED por slot). Al girar NAV el arco brilla 300 ms y vuelve a su nivel.
  Color: teal en SYNTH, púrpura en FX.
- **Teclas:** cada una anima su LED al apretarla (destello que se apaga en 400 ms) sobre un nivel de estado:
  B1–B4 tenues y la de la página abierta brillante; B5 MODE teal o púrpura según el modo; B6 encendida mientras
  SHIFT / HOLD está apretado.
- Se escribe a los LEDs solo cuando algo cambia o mientras dura una animación (60 cuadros por segundo): el WS2812
  bloquea interrupciones ~30 µs por LED, ~540 µs por cuadro con 18 LEDs.

## 9. MIDI por USB y thru (agregado 2026-10-18)

- El Brain toca lo que llega por **dos entradas**: el teclado del **USB host** y el **computador por el USB de la
  Teensy** (`usbMIDI`, el firmware es `USB_MIDI_AUDIO_SERIAL`): piano roll de Ableton y clock de la sesión (SYNC del
  LFO).
- **Thru:** el teclado del USB host se reenvía al computador para grabarlo. Lo que llega del computador **no** se
  reenvía (evita lazos).
- **Conexión recomendada con Ableton:** teclado → Brain → PC. En Ableton, track MIDI con *External Instrument*:
  *MIDI To* = Teensy MIDI, *Audio From* = la entrada de la interfaz donde entra la LINE OUT. Al grabar, **Monitor
  en Off** (si no, Ableton le devuelve al Brain las notas que este le mandó y suenan dobles); al reproducir, el
  piano roll toca el Brain. Alternativa: teclado → PC y Ableton manda todo al Brain (sin riesgo de notas dobles,
  pero el Brain no suena sin el computador).
- Audio de vuelta por el mismo USB (la Teensy como interfaz): fase siguiente, a verificar con el I2S en el hardware.
