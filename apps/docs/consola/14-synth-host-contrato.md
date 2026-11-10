# Consola: contrato del host de synths (`gf-synth-host`)

> **Fecha:** 2026-11-10. **Estado:** contrato para implementar en paralelo el host (C++) y la GUI del synth (QML +
> PySide6). **Base:** ADR-008 §1, §8; ADR-007; `12-gemelo-pi.md`; diseño `design/claude-design-synth/`.
> Si la implementación necesita cambiar algo de aquí, se cambia este doc primero y se avisa a la otra parte.

## 1. Qué es

Un proceso en **C++17**, sin interfaz, que toca **cuatro partes** de synth con el código del gemelo (las mismas
bibliotecas sin JUCE de `apps/brain-plugin`), como cliente de **PipeWire** (o JACK vía `pipewire-jack`) en el grafo
del Audio+ a 48 kHz. Una salida estéreo por parte (`part1_L/R` … `part4_L/R`) y una mezcla `mix_L/R`.

- **MIDI:** un puerto de entrada; **canal 1–4 = parte 1–4**. Note on/off, velocity, pitch bend, sustain (CC 64) y los
  CC que ya entiende el VST (14 DRIFT, 15 GRIT, 16 POCKET, 19 MOVE, 71 reso, 74 cutoff).
- **Control:** socket Unix `$XDG_RUNTIME_DIR/gf-synth.sock`, **JSON por líneas** (UTF-8, una petición o evento por
  línea). Varios clientes a la vez (GUI, núcleo, herramientas de prueba).
- **Tiempo real:** el hilo de audio no toma locks ni reserva memoria; los cambios del socket pasan por una cola sin
  locks y se aplican al inicio del bloque. Flush-to-zero activado.

## 2. Partes

`part` es 1–4. Cada parte tiene: `engine` (`prophet`, `dx7`, `dx7_6op`, `juno`, `ob6`, `model_d`, `arp2600`,
`hybrid`), `preset` (nombre), `role` (texto: `BASS`, `CHORDS`, `LEAD`, `PAD`), `level_db`, `mute`, `modified` (bool).

## 3. Peticiones (cliente → host)

Toda petición lleva `"id"` (entero del cliente) y el host responde `{"id":…, "ok":true, …}` o
`{"id":…, "ok":false, "error":"…"}`.

| `cmd` | Campos | Respuesta |
|---|---|---|
| `hello` | `client` | `version`, `sample_rate`, `block`, `engines` (lista) |
| `get_state` | — | `parts`: lista de las 4 partes (§2) |
| `list_params` | `engine` | `pages`: `[{name, banks:[[param×≤8], …]}]`; cada `param` = `{id, name, unit, min, max, default, step, kind: "cont"|"bipolar"|"enum"|"bool", options?}`. **Única fuente** de los metadatos para la GUI |
| `get_params` | `part`, `page?` | `values`: `{param_id: value}` |
| `set_param` | `part`, `param`, `value` | eco del valor aplicado (recortado al rango) |
| `set_engine` | `part`, `engine` | carga los valores por defecto del engine |
| `list_presets` | `engine?`, `role?`, `tag?` | `presets`: `[{name, engine, tags, favorite}]` |
| `load_preset` | `part`, `name` | estado de la parte |
| `save_preset` | `part`, `name` | ok / error si existe |
| `set_level` / `set_mute` | `part`, `db` / `mute` | estado de la parte |
| `note` | `part`, `note`, `velocity` (0 = off) | — (para pruebas sin MIDI) |
| `subscribe` | `topics`: `["meters","state","cpu"]` | — |

## 4. Eventos (host → clientes suscritos)

| `event` | Contenido | Frecuencia |
|---|---|---|
| `meters` | `parts: [{peak_db, rms_db}]×4`, `mix` | 30 Hz |
| `state` | la parte que cambió (§2) | al cambiar |
| `param` | `part`, `param`, `value` (cambios hechos por MIDI CC o por otro cliente) | al cambiar, agrupados por bloque |
| `cpu` | `mean_pct`, `max_pct` (de un núcleo, ventana de 1 s), `xruns` | 1 Hz |

## 5. Lo que la GUI hace con esto

- PARTS (31): `get_state` + `meters`.
- Plantilla de parámetros (32–39): `list_params(engine)` define páginas y bancos; los 8 encoders mandan `set_param`
  relativo convertido a absoluto en la GUI según `step`/`kind`; los gráficos (curva de filtro, ADSR, algoritmo FM)
  se dibujan desde los valores.
- PRESETS (42): `list_presets(role=…)`, `load_preset`, `save_preset`.
- Estados del diseño: «BRAIN NOT RESPONDING» (32b) si el socket no responde en 500 ms; «LOADING PRESET» (32c) entre
  `load_preset` y su respuesta.

## 6. Fuera de este contrato

El núcleo de la consola (estado del set, frases, ACCEPT), el sampler, la parte del Brain físico (Teensy) y la
grabación. La parte del Brain físico aparece en PARTS por el estado USB de la Teensy, no por este host.

## 7. Extensión DRUM v1 (2026-11-10)

D1/D2/D4 de19§12 incorporan sampler, secuenciador y transporte al mismo host.
Contrato aditivo completo: [22§5](22-modo-drum.md#5-extensión-del-contrato14).
`hello` conserva version0.1.0 y añade drum_version1/capacidades; comandos synth
sin cambios. Se agregan cue_L/R; mix_L/R incluye ROOM del rack. Canal10 MIDI
notas0..15 dispara pads0..15, sin reasignar canales1..4. Índices DRUM0-based.
Carga/preview/edición/clock/jobs y recuperación detenida se definen en22; §6 se
mantiene como alcance original de synth, ampliado por esta sección.
