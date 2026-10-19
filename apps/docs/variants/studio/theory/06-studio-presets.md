# Studio · Presets reales (guardar y cargar el patch)

> **Fecha:** 2026-10-16 · **Estado:** PROPUESTO, espera aprobación de Juan (incluye una enmienda al `02`, §5)
> **Depende de:** theory/04 (navegación en el Teensy: `LoadPreset` / `SavePreset` ya existen), theory/05 (SYNTH FX),
> theory/lfo-modulacion.md (`studio::Patch`, `gf_params.h`), `02-bridge-protocol.md` §2.2 y §2.10.

## 1. El problema

La página PRESETS navega, pero no guarda ni carga nada: la lista son nombres de ejemplo de la cabeza y el sketch49
ignora `LoadPreset` / `SavePreset`. Un preset tiene que guardar **todo lo que se edita desde el panel** y sobrevivir
al apagado.

## 2. Qué guarda un preset

El `studio::Patch` completo del engine más el filtro de ENC L / R:

| Bloque | Contenido | Bytes |
|---|---|---|
| OSC | 16 slots (`GF_OSC_SLOT`) | 64 |
| ENV | A D S R del amplificador y del filtro | 32 |
| FILTER | cutoff, resonancia, FLT ENV AMT | 12 |
| LFO | RATE, forma, división, 4 cantidades | 26 |
| SYNTH FX | 4 encendidos, 4 MIX, 12 parámetros | 68 |
| Cabecera | versión de formato, engine, nombre (16 caracteres) | 20 |
| **Total** | | **~222 B** |

No guarda la navegación (página, SHIFT) ni el modo FX (B5): un preset es un sonido, no un estado del panel.

## 3. Dónde se guarda

| Opción | Capacidad | Tradeoff |
|---|---|---|
| EEPROM emulada del Teensy 4.1 | 4284 B: ~19 presets en total | no alcanza para un banco por engine |
| **LittleFS sobre el flash del programa (`LittleFS_Program`, Teensyduino)** | lo que se reserve: 512 KB = miles de presets | **elegida**: sin hardware extra, sobrevive a reflasheos si la partición no se toca |
| Tarjeta SD del Teensy 4.1 | GB | requiere tarjeta puesta; útil para exportar después |

**Banco:** 16 presets por engine (el tope que ya usa la cabeza, `PRESET_MAX`), un archivo por preset:
`/presets/<engine>/<nn>.gfp` (binario, little-endian, con versión de formato para poder migrar).
Si un banco está vacío al arrancar se crea `INIT` con los valores de `patch_init()`.

## 4. Comportamiento

| Acción | En la página PRESETS | Resultado |
|---|---|---|
| NAV | mueve el cursor (fila 0 = SAVE) | — |
| NAV push sobre SAVE | guarda el sonido actual en el próximo lugar libre, como `USER nn` | aparece en la lista y queda cargado |
| NAV push sobre un preset | lo carga | el sonido cambia y la cabeza recibe todos los parámetros |
| B4 | ENGINE SELECT (como hoy) | — |

Renombrar y borrar quedan para la fase 2 (necesitan teclado en pantalla o UI_INPUT).

## 5. Bridge: qué ya existe y qué falta (enmienda al `02`)

- **Ya existe** `0x17 PRESET_LOADED [preset_id: 2B, name: string]` (`02` §2.2): el Teensy lo manda al cargar o
  guardar. La cabeza actualiza el preset cargado y el nombre de la barra superior. Después van los
  PARAM_CHANGED del patch completo (`patch_params`), como al cambiar de engine.
- **Falta:** la lista de nombres del banco. `02` §2.10 ya la anota como "dato sin comando" ("Presets: banco del
  engine, cargado, guardar · al entrar a la página"). Propuesta:

| CMD | Nombre | Payload | Cuándo |
|---|---|---|---|
| **0x90** | PRESET_ENTRY | `[engine: 1B, index: 1B, count: 1B, name: string ≤16]` | uno por preset al entrar a PRESETS, al cambiar de engine y al guardar |

Un frame por entrada (≤ 23 B) en vez de un frame grande: no hace falta multi-frame y la cabeza va llenando la lista
aunque se pierda uno (el siguiente ingreso a la página lo repone). Usa el rango libre 0x90–0x9F que ya propone
`02` §2.10. **Necesita tu instrucción explícita para aplicarse al `02`.**

## 6. Cómo se prueba

| Qué | Cómo | Dónde |
|---|---|---|
| Serialización del preset (roundtrip, versión, tamaño) | tests Unity con un buffer en memoria | `pio test -e native` |
| Banco: SAVE en el próximo libre, tope 16, INIT si está vacío | tests Unity con un almacenamiento falso | `pio test -e native` |
| Sobrevive al apagado | guardar, desenchufar, cargar: suena igual | protoboard |
| La cabeza muestra la lista real | entrar a PRESETS en cada engine | Pi 3B |

## 7. Qué no cubre

Renombrar y borrar, presets de fábrica por engine (más allá de INIT), presets del modo FX, exportar a SD o al
computador, y los presets del DX7 de operadores (su página OSC no existe todavía).
