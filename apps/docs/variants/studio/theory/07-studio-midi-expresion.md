# Studio · Expresión MIDI y mapa de CC

> **Fecha:** 2026-10-18 · **Estado:** APROBADO por Juan ("haz las 6 implementaciones para la experiencia de synth
> físico") · **Depende de:** theory/04 §9 (MIDI por USB y thru), theory/lfo-modulacion.md, `gf_params.h`.

## 1. Por qué

Un synth físico se toca con las ruedas, la presión de la tecla y perillas para todo. El Brain tiene 4 encoders y
páginas; un controlador MIDI con perillas (el teclado, el clon de Push) o la automatización de Ableton pueden hacer de
panel completo si cada parámetro tiene su CC.

## 2. Expresión

| Fuente | Destino | Detalle |
|---|---|---|
| **Pitch bend** | afinación de todas las voces | ±2 semitonos (rango de los originales por defecto), se suma al vibrato del LFO |
| **Mod wheel (CC 1)** | vibrato (o WHEEL-MOD en el Prophet) | ya implementado (theory/lfo-modulacion.md §5) |
| **Aftertouch de canal** | cutoff (+1 octava a fondo) y vibrato (+30 % de la cantidad PITCH) | como el Prophet-5 rev 4 y el OB-6 (aftertouch → filtro / LFO) |
| **Velocity** | brillo del envelope del filtro (Juno, OB-6, ARP, Prophet) e índice de modulación (DX7) | ya en los engines; el Model D original no tiene velocity |

## 3. Mapa de CC (por engine, el mismo número en todos)

Entrada absoluta 0–127 → valor normalizado del parámetro (`gf_params.h`). Lo que se edita desde el panel **sale por
el mismo CC** al computador (`usbMIDI`), así Ableton graba la automatización y la reproduce.

| CC | Parámetro | CC | Parámetro |
|---|---|---|---|
| 1 | mod wheel | 74 / 71 | cutoff / resonancia |
| 73 / 75 / 70 / 72 | ENV AMP: attack / decay / sustain / release | 102–105 | ENV FILTER: A / D / S / R |
| 106 | FLT ENV AMT | 76 / 80 / 81 | LFO: rate / forma / división |
| 77 / 78 / 79 | LFO → cutoff / pitch / 3.er destino | 20–27 | OSC parada 1, slots 1–8 |
| 28–35 | OSC parada 2, slots 1–8 | 36–43 | OSC parada 3, slots 1–8 |
| 46–53 | OSC parada 4 (OP 4 del DX7) | 54–61 | MOD parada 1 (POLY-MOD, X-MOD…) |
| 110–117 | MOD parada 2 (WHEEL-MOD…) | 91 / 93 / 94 / 95 | SYNTH FX MIX: reverb / chorus / tape / bit |

Los slots sin uso en el engine activo ignoran su CC. Los CC 64 (sustain pedal) y 123 (all notes off) siguen el
estándar MIDI.

## 4. Implementación

- `studio::apply_cc()` y `studio::param_cc()` en `page_params` (lógica pura, tests native): CC → cambio del patch +
  `ParamChange`, y `param_id` → CC para la salida.
- Sketch49: pitch bend y aftertouch del USB host y de `usbMIDI` entran a `mod_tick`; los CC pasan por `apply_cc`;
  `page_encoder` manda el CC del parámetro editado.
- Sustain pedal: las notas soltadas con el pedal apretado se guardan y se sueltan al levantarlo.

## 5. Qué no cubre

Rango de pitch bend editable, NRPN de 14 bits, MPE, MIDI learn (el mapa es fijo).
