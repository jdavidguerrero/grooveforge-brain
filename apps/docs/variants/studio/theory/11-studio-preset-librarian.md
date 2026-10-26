# Studio · Bibliotecario de presets (Brain ↔ Mac por USB)

> **Fecha:** 2026-10-26 · **Estado:** APROBADO por Juan ("implementa todo lo necesario para arrancar con la generación
> de presets … el tema de guardado me parece") · **Depende de:** theory/06 (formato y banco) · theory/10 (formato 4).

## 1. Para qué

Hoy los presets viven solo en el flash de la Teensy, se llaman `USER nn` y no tienen ficha. Para construir la paleta
de sonidos hace falta:

- **Sacarlos** del Brain a archivos legibles en la Mac, uno por sonido, con nombre y ficha (rol, tags, registro, con
  qué combina, notas).
- **Subirlos** de vuelta, ponerles nombre y reordenar un banco.
- Que esa carpeta sea la **librería maestra**: con historial en git, la lee después la IA local (Mac mini) y la
  puede leer la Pi.

## 2. Transporte: comandos de texto por el USB serie

El USB serie de la Teensy ya existe (es el log). Los comandos son líneas que empiezan con `@GF `; las respuestas
también, así el script ignora el resto del log.

| Mac → Teensy | Teensy → Mac | Qué hace |
|---|---|---|
| `@GF HELLO` | `@GF HELLO <formato> <engines> <máx por banco>` | Versión del formato y límites |
| `@GF EXPORT` | `@GF BEGIN <total>`, una línea `@GF P <engine> <índice> <hex>` por preset, `@GF END` | Todos los bancos, en binario del formato actual |
| `@GF PUT <engine> <índice> <hex>` | `@GF OK PUT <engine> <índice>` o `@GF ERR <motivo>` | Valida (`preset_decode`), escribe en el lugar `índice` (reemplaza o agrega al final, sin dejar huecos) |
| `@GF TRUNC <engine> <n>` | `@GF OK TRUNC <engine> <n>` | Deja el banco con `n` presets (borra del `n` en adelante) |

- El binario viaja en **hexadecimal** (≈ 1.3 KB por preset): sin escapes y fácil de depurar.
- Después de `PUT` o `TRUNC` el Brain vuelve a leer el banco y manda la lista a la cabeza (`PRESET_ENTRY`).
- Un preset de un formato viejo se migra al leerlo (theory/10); el Brain siempre exporta el formato actual.
- Motivos de error: `FORMATO`, `ENGINE`, `INDICE` (deja hueco o pasa de 16), `ESCRITURA`.

## 3. La librería en la Mac

`tools/preset-librarian/gf_presets.py` (Python 3, `pyserial`). Un archivo JSON por sonido:

```
<librería>/brain/<engine>/<nombre>.json
{
  "name": "JD RUN FUNKBASS",          ← máx. 16 caracteres; es el nombre que se ve en el Brain
  "engine": "model_d",
  "role": "bass",                     ← bass · chords · pad · lead · pluck · perc · fx
  "tags": ["funk", "ghost-notes"],
  "key_range": "C1–C3",
  "pairs_with": ["kick redondo", "tambora"],
  "notes": "",
  "format": 4,
  "patch": { … todos los parámetros con nombre … },
  "raw": "<hex del binario>"          ← lo que se sube; "patch" es para leer y para la IA
}
```

- **export:** baja todos los bancos. Si ya existe un archivo con ese nombre, **conserva la ficha** (role, tags,
  notas) y solo actualiza `patch` y `raw`.
- **import:** sube archivos al final del banco de su engine. Si el `name` del JSON cambió, el binario se reescribe con
  ese nombre.
- **bank:** reescribe un banco completo en el orden dado (PUT 0…n−1 y TRUNC n).
- `patch` se decodifica del binario con el mismo orden que `visit()` de `preset_bank.cpp`. Para editar parámetros se
  usa el Brain; el JSON es para leer y fichar (y para la IA). Subir siempre usa `raw`.

## 3b. Robustez del flash de presets (rev. 2, 2026-10-26)

- **Escritura atómica:** cada preset se escribe en `NN.gfp.tmp` y se renombra a `NN.gfp` (LittleFS reemplaza el
  destino en un paso). Si se corta la corriente a mitad, queda el preset anterior entero.
- **Presets de fábrica fuera del arranque:** al arrancar solo se lee el banco (`read_bank`); los de fábrica que falten
  se escriben **de a uno cada 25 ms desde el loop** (`seed_one`). El audio, el USB y la cabeza no esperan al flash, y
  si se interrumpe, el próximo arranque sigue donde quedó. La anotación de ofrecidos se escribe al terminar el banco.
- **Diagnóstico:** el log de arranque muestra el tamaño de cada banco y el espacio usado; `@GF FS` (o
  `gf_presets.py fs`) lo da a pedido, y una escritura fallida queda en el log con el espacio que quedaba.

## 4. Cómo se prueba

| Qué | Cómo |
|---|---|
| Hex ida y vuelta, parseo de comandos, PUT / TRUNC sobre el banco (huecos, formato, límites) | test native |
| Decodificar un `raw` exportado y volver a codificarlo da los mismos bytes; renombrar | test del script (pytest) |
| Exportar, renombrar en la Mac, subir y verlo en la pantalla con el nombre nuevo | en el hardware |

## 5. Qué no cubre

Editar parámetros desde la Mac, sincronizar automática o por Wi-Fi, y los sonidos de Tribe Instruments, Kick 3,
Splice o Loopcloud: esos van en la etapa de organizar la librería para la IA local.
