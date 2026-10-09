# ADR-004: Panel de Studio — 5 encoders y 6 botones cableados al Teensy

**Status:** Accepted — Revisión 1 (2026-10-07): pot IN y pad de entrada · Revisión 2 (2026-10-09): posición de IN/VOL según el enclosure validado
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

## Action Items

1. [ ] Firmware: 5 encoders + 6 botones, MODE, HOLD/SHIFT, doble push configurable.
2. [ ] Mapa de parámetros 2 y 3 para los 9 FX (y el par de cada página en SYNTH).
3. [ ] Iteración de UI en Claude Design (`variants/studio/design/claude-design-prompt-v2-controles.md`).
4. [ ] Validación del enclosure en Claude Design (`variants/studio/design/claude-design-prompt-enclosure.md`).
5. [x] Con autorización: actualizar `01` §3.3 y §3.4 (2026-10-07).
6. [ ] (rev. 1) Firmware: pot IN → `lineInLevel` + ganancia fina + medidor de entrada; lectura del pad.
7. [ ] (rev. 1) Theory doc del PCB: pad 0/−10/−20 dB y protección de la entrada.
