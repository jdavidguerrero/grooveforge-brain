# ADR-004: Panel de Studio — 5 encoders y 6 botones cableados al Teensy

**Status:** Accepted — Revisión 1 (2026-10-07): pot IN y pad de entrada · Revisión 2 (2026-10-09): posición de IN/VOL según el enclosure validado · Revisión 3 (2026-10-15): prototipo sin LEDs ni PCB-K
**Date:** 2026-10-07
**Deciders:** Juan Guerrero (owner)
**Relacionados:** ADR-002 (variante Studio), ADR-003 (add-on del ladder)
**Afecta specs:** `01-architecture.md` §3.3 (pin map) y §3.4 (controles). No se editan hasta
autorización explícita.

---

## Context

- El panel base es 3 encoders + 4 botones + volumen ("Hardware UI Spec — CERRADO, Panel v5").
- En Studio, el modo SYNTH se puede tocar desde el controlador MIDI, pero el **modo FX en vivo**
  (inspirado en la Roland RMX-1000) necesita más control simultáneo.
- Con el panel base aparecían dos fricciones: editar parámetros solo con NAV (seleccionar, apretar,
  girar), y el doble push de NAV, que obliga a esperar ~350 ms en cada push simple.
- El PCB viene después del Studio head: el panel tiene que quedar definido ahora.

## Decision

1. Studio suma **2 encoders (P1, P2) y 2 botones (B5, B6)**, **cableados al Teensy**, no a la Pi. Si
   la cabeza se reinicia, los controles siguen funcionando (el audio nunca depende de la cabeza).
2. **Un solo firmware** que soporta 5 encoders y 6 botones. En el comercial esos pines quedan sin
   poblar (con pull-up no generan eventos). Sin `#ifdef` de variante. El panel comercial no cambia.
3. **Mapeo:**

| Control | Modo FX | Modo SYNTH |
|---|---|---|
| ENC L | Dry/Wet | Cutoff |
| ENC R | Parámetro 1 del FX | Resonance |
| P1 | Parámetro 2 del FX | 1er parámetro de la página |
| P2 | Parámetro 3 del FX | 2º parámetro de la página |
| NAV | recorrer FX; push = on/off inmediato | vistas y cursor; push = acción inmediata |
| B1–B3 | INSERT / SEND / MASTER | OSC / ENV / LFO |
| B4 | FX favorito | PRESET / ENGINE |
| B5 | MODE (SYNTH⇄FX) | MODE |
| B6 | HOLD: mantener = FX encendido, soltar = apagado | SHIFT: P1/P2 pasan al par secundario |

4. **Cambio de modo en Studio:** botón MODE. La detección de doble push de NAV se desactiva por
   configuración en Studio (push inmediato); el comercial la conserva. Esto reemplaza, **solo para
   Studio**, la decisión de doble push tomada antes el mismo día.
5. **Pines** (todos de borde en la Teensy 4.1, libres según `01` §3.3):

| Función | Pin(es) |
|---|---|
| P1: A / B / SW | 30 / 31 / 32 |
| P2: A / B / SW | 34 / 35 / 9 |
| B5 MODE | 40 |
| B6 HOLD/SHIFT | 41 |
| Pot de volumen | **22 (A8)** — sale del 14 |
| CV de cutoff para el add-on | 29 (PWM) |
| CV 2 opcional | 33 (PWM) |
| **Pot de ganancia de entrada IN** (rev. 1) | **24 (A10)** |
| **Lectura del switch de pad** (rev. 1, opcional) | **25 (A11)** con divisor resistivo; el pin queda libre por ADR-003 |

6. **Cierra D2 / X3:** el pot de volumen pasa del pin 14 (que también era ENC R-B) al pin 22. Como el
   firmware es uno solo, la corrección aplica también a la base.
7. **LEDs:** los dos keycaps nuevos se suman a la cadena WS2812 del pin 28 (18 LEDs en total).
8. **Entrada de audio (revisión 1):**
   - **Pot IN**, simétrico al VOL. **Rev. 2:** IN y VOL van a los costados de la pantalla, en (15, 96) y
     (165, 96) mm; en los extremos de la fila 2 ninguna etiqueta quedaba a ≤ 25 mm de su control. Es un
     control, no un pot en el camino del audio: el Teensy lo lee y fija la ganancia analógica del
     codec (`lineInLevel`, 16 pasos de ~1.5 dB entre 3.12 y 0.24 Vpp de fondo de escala) más una
     ganancia digital fina para suavizar los pasos.
   - **Switch de pad físico** de 3 posiciones (0 / −10 / −20 dB) en el **panel trasero**, junto a las
     entradas, con protección de sobrevoltaje antes del codec. Motivo: el SGTL5000 acepta como
     máximo 3.12 Vpp, y una fuente de +4 dBu o un mixer de DJ caliente lo superan.
   - La UI muestra un medidor de entrada con clip en FX MAIN.
   - En el comercial, sin pot IN, la ganancia queda en un valor por defecto ajustable por menú.

## Options Considered

| Opción | Resultado | Motivo |
|---|---|---|
| Panel base sin cambios | ❌ | el modo FX en vivo queda corto y persisten las dos fricciones |
| Controles extra cableados a la Pi | ❌ | mueren si la cabeza se reinicia: inaceptable en vivo |
| **Controles extra en el Teensy** | ✅ | robustos y con latencia directa; mismo firmware para ambas variantes |
| 4 encoders de página tipo Push | ❌ por ahora | más pines y más ancho de panel; 2 + SHIFT cubre los casos |

## Consequences

**Más fácil:** FX en vivo con 4 perillas simultáneas y efecto momentáneo; edición directa de
parámetros por página; push de NAV sin retardo en Studio; el conflicto del pin 14 queda resuelto.

**Más difícil:** cada FX necesita definir sus parámetros 2 y 3 (hoy solo tiene el principal); el
diseño de UI necesita una iteración; el panel es más ancho que el comercial.

**A revisitar:**
- Pin 25 (bypass del CD4066) queda libre por ADR-003.
- Pin 27 (74HC4053, ruteo Synth/FX): revisar si sigue haciendo falta junto con la decisión D5.
- Ajustar las acciones del mapeo con el uso real (el owner lo va a ir revisando tocando).

## Revisión 3 (2026-10-15): prototipo económico sin LEDs

**Decisión del owner** para bajar el costo del primer prototipo (objetivo: US$80–100 en PCBs con envío):

- **Sin LEDs en el prototipo.** El punto 7 de la decisión (18 WS2812/SK6812 en la cadena del pin 28) queda
  diferido. El buffer 74AHCT1G125 y la línea LED_DIN siguen en la portadora y en PCB-A.
- **Sin PCB-K.** Los 6 Choc V2 se clipsan en el rebaje del aluminio como antes y se cablean con 7 hilos
  (B1–B6 + GND) a **J1 de PCB-A**, que pasa de JST-SH a un **header 1×10 acodado** con el mismo pinout
  (B4, B6, B3, GND, +5V, GND, LED_DIN, B2, B5, B1): +5V y LED_DIN quedan libres para un anillo o tira de
  LEDs cableada más adelante. El diseño de PCB-K queda en el repo, no se pide.
- **Pines del Teensy sin cambios:** el firmware y `01` §3.3 no cambian por esto; el pin 28 queda sin carga.
- **Fabricación:** portadora con ensamble solo de SMD; PCB-A sin ensamble; todos los THT a mano
  (`theory/02` §10F).

**Luces cableadas (2026-10-15, mismo día):** el owner tiene un anillo WS2812 de 12 LEDs (Ø34/Ø49.5) y
APA102/SK9822 5050 sueltos.
- **NAV:** el anillo va bajo el aluminio, sobre un **difusor de PC esmerilado Ø44 × 2 mm** que hace de guía de
  luz y asoma por el agujero de Ø32 (los LEDs del anillo quedan en Ø42, bajo el aluminio). Se conecta a J1 de
  PCB-A (+5V, LED_DIN, GND): pin 28 por el buffer de la portadora, sin cambios de placa.
- **Teclas:** un APA102 por Choc en un **soporte impreso** encajado en el poste central, bajo la ventana de LED.
  Cadena cableada a mano (5V, GND, datos, reloj). Datos y reloj desde **pines 36 y 37** de la Teensy, con
  hilos soldados a J2.21/J2.20 de la portadora (sin conexión en la placa); FastLED por software SPI.
  Nivel de 3.3 V contra V_IH de ~3.5 V del APA102 a 5 V: suele funcionar con cables cortos y reloj lento.

**Cambio (2026-10-16, Juan):** las teclas usan WS2812 sueltos en la misma cadena del anillo (pin 28, LEDs 12–17)
en lugar de APA102 en los pines 36/37: un solo pin, datos a 5 V por el buffer de la portadora. Detalle en
`variants/studio/theory/04-studio-ui-state.md` §8.

**Cambio de mapeo (2026-10-16, Juan):** ENC L / R son CUTOFF / RES solo en las vistas (home). En OSC, ENV, LFO y
SYNTH FX los 4 encoders (P1, P2, ENC L, ENC R) editan la página, y SHIFT da otros 4. NAV push largo vuelve a home.
B4 = SYNTH FX y SHIFT + B4 = PRESETS (theory/05 de Studio). Detalle en theory/04 §3.1.

`01` §3.3 sigue listando los 18 LEDs: no se edita sin autorización. Si el producto final vuelve a llevar
LEDs, PCB-K está lista para pedirse.

## Action Items

1. [ ] Firmware: 5 encoders + 6 botones, MODE, HOLD/SHIFT, doble push configurable. (Pines validables con
   `sketches/46-studio-controls-test.cpp`, 2026-10-12; falta integrarlo en `ui/encoders` y `ui/buttons`.)
2. [x] Mapa de parámetros 2 y 3 para los FX: `05` §1.13 (2026-10-12). El par de cada página en SYNTH está
   en la GUI (OSC: plantilla por engine; ENV; LFO).
3. [x] Iteración de UI en Claude Design (UI v3, 2026-10-09; OSC/LFO, 2026-10-12).
4. [x] Validación del enclosure en Claude Design (2026-10-09). Reparto de PCBs simplificado a 4 placas
   (`01` §3.6, 2026-10-12).
5. [x] Con autorización: actualizar `01` §3.3 y §3.4 (2026-10-07).
6. [ ] (rev. 1) Firmware: pot IN → `lineInLevel` + ganancia fina + medidor de entrada; lectura del pad.
7. [ ] (rev. 1) Theory doc del PCB: pad 0/−10/−20 dB y protección de la entrada.
