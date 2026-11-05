# GrooveForge Library

> **Fecha:** 2026-11-05. **Estado:** primera versión. **Decisión de Juan:** tener one-shots, MIDI y tops a la mano, organizados para Live y para alimentar la IA.
> **Código:** `tools/library/make_library.py`. **No modifica** `00–06`.

## 1. Dos carpetas, dos trabajos

| Carpeta | Para qué | Quién escribe |
|---|---|---|
| `GrooveForge_Dataset_v1/` | Memoria técnica: snapshots, registros, presets, fichas | Los scripts del dataset |
| `GrooveForge_Library/` | Lo que se usa al producir; se agrega a Places en Live | El generador, el motor y Juan |

El dataset no se reorganiza: sus scripts dependen de esas rutas y sus snapshots son registros históricos. La Library se genera **copiando** desde él y se puede regenerar.

## 2. Estructura

```text
GrooveForge_Library/
├── 00 Elegidos/          lo que Juan elige en el motor
├── 01 One Shots/         <Instrumento>/<Género o Material>/  y  Modales/<Instrumento>/
├── 02 Kits/              kits 808 por material
├── 03 Drums MIDI/        Tops · Perc · Fills · Raices · Favoritos · Seleccion · Dosis
├── 04 Melodic MIDI/      Bass · Chords · Leads · Plucks · Pads · Capas · Builds
├── 05 Loops/
├── 06 Racks/
├── 07 Contexto/          contextos, estudios y clips de prueba
├── 08 Audiciones/        todas las variantes en fila; no se usan para entrenar
└── _catalogo/            catalogo.jsonl, resumen.json
```

**Instrumento antes que género:** al producir se busca «un kick», y el género se ve en la subcarpeta y en el nombre del archivo.

**Excluido:** `snapshots/` (copias), `renders/perc-synth/v0.2` (reemplazado por los kits de v0.3), `patches/` (van al Brain con el bibliotecario) y las fichas JSON, que se enlazan desde el catálogo.

## 3. Catálogo para la IA

`_catalogo/catalogo.jsonl` tiene una línea por archivo, con rutas relativas para que siga válido en el SSD:

| Campo | Para qué |
|---|---|
| `id`, `ruta`, `origen` | Identidad por contenido y procedencia en el dataset |
| `tipo`, `rol`, `genero`, `material`, `rack`, `seccion`, `version` | Filtrar y buscar; lo que no se conoce queda en `null` |
| `tonalidad`, `bpm`, `sintesis` | Parámetros reales del generador, sin estimar |
| `midi` | Tempo del archivo, número de notas, rango y duración en tiempos |
| `grupo` | Evita fuga de datos: un mismo patrón nunca se reparte entre entrenamiento y prueba |
| `autoria` | Todo es `generado`; una toma tocada por Juan llevará `tocado` |
| `decision_juan` | `favorito` (ronda 1) o `seleccion` (ronda 2); si no, `null` |
| `uso_ia` | `entrenable`, `solo_catalogo` o `excluido`, con su motivo |

**Lo que esto no enseña por sí solo.** Casi todo sale de reglas o de síntesis, así que describe los generadores y no el gusto de Juan. Su criterio está en `decision_juan` y en las ediciones que registra el motor
(`motor-asistencia-v2.md` §6). Separar `autoria` y `decision_juan` impide tratar mil variantes generadas como mil preferencias (`ia-local-gemelo-digital.md` §7, abundancia falsa).

**Racks:** referencian samples de terceros, así que son `excluido`. Hay que revisar las licencias antes de entrenar con audio de ellos. El MIDI hecho para Tribe es propio: no contiene audio de Tribe.

## 4. Reglas del generador

1. Solo lee el dataset, nunca lo cambia.
2. No borra nada de la Library.
3. **Un archivo modificado por Juan no se sobrescribe:** si difiere de lo que el catálogo anterior registró, se conserva y se avisa.
4. Si dos orígenes con distinto contenido caen en la misma ruta, ninguno se pierde: el segundo lleva un sufijo con su hash.
5. Es repetible: una segunda corrida sin cambios no copia nada.

**Ubicación:** `/Volumes/GROOVE_SSD/GrooveForge_Library`, para compartirla con el Mac mini; se cambia con `GF_LIBRARY`. El dataset sigue en el Mac como fuente, y por ahora la Library se regenera desde este computador.
