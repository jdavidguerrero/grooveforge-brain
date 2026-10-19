# Studio · Presets reales (guardar y cargar el patch)

> **Fecha:** 2026-10-16 · **Estado:** APROBADO por Juan el 2026-10-18 ("haz las 6 implementaciones para la experiencia
> de synth físico"), incluida la enmienda al `02` (§5, aplicada en `02` §2.10.1) · **Implementado:** `ui/preset_bank.*`,
> sketch49, `bridge_link.cpp` de la cabeza
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
| Cabecera | firma `GFP`, versión de formato (1), engine, nombre (16 caracteres) | 21 |
| ENV | A D S R del amplificador y del filtro | 32 |
| FILTER / PERFORM | FLT ENV AMT, GLIDE, DRIVE, cutoff (Hz), resonancia (normalizada) | 20 |
| LFO | RATE, forma, división, 4 cantidades | 22 |
| OSC | 4 paradas × 8 slots (`GF_OSC_SLOT`, incluye los 4 operadores del DX7) | 128 |
| MOD | 2 paradas × 8 slots (`GF_MOD_SLOT`: POLY-MOD, WHEEL-MOD, X-MOD) | 64 |
| SYNTH FX | 4 encendidos, 4 MIX, 12 parámetros | 68 |
| **Total** | `PRESET_BYTES` | **355 B** |

(Actualizado al implementar: la propuesta original contaba 16 slots de OSC; desde la página OSC de 8 slots y las
paradas de MOD el patch creció. 16 presets × 6 engines × 355 B ≈ 34 KB de los 512 KB reservados.)

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

Renombrar y borrar, volver al banco de fábrica desde el panel (hoy: borrar `/presets` del LittleFS), presets del
modo FX y exportar a SD o al computador.

## 8. Banco de fábrica (agregado 2026-10-18)

Si el banco de un engine está vacío al arrancar se escriben 6 patches clásicos (en vez de un solo INIT) y cada engine
arranca con el preset 1. Son puntos de partida del carácter de cada máquina, no copias de patches de fábrica
originales (nombres genéricos):

| Engine | Presets |
|---|---|
| MODEL D | INIT · FAT BASS (3 VCO, VCO 3 −12) · 70S LEAD (glide) · BRASS · FLUTE (tri + ruido + vibrato) · PLUCK |
| JUNO-106 | INIT · STRINGS (chorus II, HPF) · PWM PAD (LFO → PWM) · SUB BASS · BRASS STAB · PLUCK KEYS |
| PROPHET-5 | INIT · SYNC LEAD (POLY-MOD ENV → FREQ A) · POLY BRASS · POLYMOD BELL (OSC B → FREQ A) · STRING PAD · WARM KEYS |
| OB-6 | INIT · JUMP BRASS · BP PAD (pasa banda) · XMOD LEAD · SAW STRINGS · PULSE BASS |
| DX7 | E.PIANO (alg 5) · FM BASS (alg 1, FB 5) · BELLS · BRASS FM (alg 3, FB 6) · MARIMBA · ORGAN (alg 8, drawbars 16'/8'/4'/2 2/3') |
| ARP 2600 | INIT · RING BELL (ring mod) · SQUARE BASS · SEQ LEAD · S&H BURBLE (LFO S&H → cutoff) · PERC BLIP |

Test `test_presets`: cada valor de fábrica cae dentro del rango y de los discretos de la plantilla del engine.
