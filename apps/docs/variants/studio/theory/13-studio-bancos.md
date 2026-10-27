# Studio · Más espacio para presets: 2 MB y bancos A–D (64 por engine)

> **Fecha:** 2026-10-27 · **Estado:** APROBADO por Juan ("revisemos lo de la propuesta para el espacio de los
> presets") · **Depende de:** theory/06 (presets) · theory/11 (bibliotecario y robustez).

## 1. Problema

- El área de presets era de **512 KB** dentro del flash de la Teensy (le sobran ~7.8 MB), con un archivo por preset y
  un bloque de 4 KB por archivo: entran ~100 presets en total, y con los de fábrica (clásicos, JD, GF, HY) ya hay ~70.
- El tope de **16 presets por engine** venía de la lista de la cabeza, no del hardware.

## 2. Diseño

**Área de 2 MB con migración:** `LittleFS_Program` ubica el área en los últimos N bytes del flash, así que el área
nueva de 2 MB **contiene** a la vieja en su final. Al arrancar:

1. Se monta el área de 2 MB. Si ya tiene `/presets`, listo (migrada o en uso).
2. Si está vacía (primer arranque con este firmware, la librería la formatea: solo escribe sus dos primeros bloques,
   lejos del área vieja), se monta el área vieja de 512 KB y **todos sus archivos se copian a la RAM** (~55 KB) antes
   de escribir nada en la nueva.
3. Se escriben en la nueva (escritura atómica de theory/11). Si el área vieja no existía, no hay nada que copiar.

Si se corta la corriente a mitad, el próximo arranque ve `/presets` en la nueva y sigue con lo que alcanzó a copiar; lo
que falte vuelve como preset de fábrica. Nunca se vuelve a montar el área vieja cuando la nueva ya tiene datos (montar
una área inválida la formatea y podría pisar bloques de la nueva).

**Rev. 2 (2026-10-27, en el hardware):** el área vieja de 512 KB estaba **corrupta** (sus metadatos apuntaban fuera
del flash: leerla en la migración colgó la Teensy con un bus fault en `0x608B0000`; explica también el banco que
quedaba en 2 presets). Se quitó la migración: el área de 2 MB arranca limpia y se regeneran los de fábrica. Para
dejarla limpia de una vez: `@GF FORMAT YES` (`gf_presets.py format --si`), que borra todo y regenera los de fábrica.

**Rev. 3 (2026-10-27): la causa raíz.** En la Teensy 4.1 `LittleFS_Program` usa **bloques de 64 KB**
(`SECTOR_SIZE 65536`) y cada archivo ocupa al menos uno: con un archivo por preset, cada uno gastaba 64 KB. El área de
512 KB (8 bloques) admitía ~2 presets y terminó corrupta al quedarse sin bloques; la de 2 MB (32 bloques) se llenaba con
~23. Ahora hay **un archivo por engine** (`/bank<e>.gfb`, ~34 KB con los 65 lugares, entra en un bloque;
`ui/bank_file_storage.h`): el banco se carga en RAM, las escrituras lo cambian ahí y se guarda **una vez por banco**
(un borrado de bloque en lugar de uno por preset, que además frenaba el USB y el audio). Al arrancar se borran los
archivos del formato anterior (`/presets/`) y se regeneran los de fábrica.

**Bancos A–D:** 64 presets por engine, guardados en orden (`00.gfp` … `63.gfp`, la anotación de fábrica pasa a `64`).
Un banco son 16 lugares seguidos: A = 1–16, B = 17–32, C = 33–48, D = 49–64.

| Control | En PRESETS |
|---|---|
| NAV | Recorre SAVE y los presets de todos los bancos |
| **B1 / B2 / B3 / B4** | **Salta al banco A / B / C / D** (antes abrían OSC / ENV / LFO / ENGINE; esas siguen con NAV largo y SHIFT + NAV push) |
| NAV push | Carga (o SAVE en la fila 0) |

- La cabeza nombra cada preset por banco: `A-01` … `D-16`.
- El bibliotecario acepta hasta 64 por engine (`bank` reescribe los 64 en el orden dado). Así se reorganiza: por
  ejemplo A = clásicos, B = JD, C = GF / HY, D = tus sonidos.

## 3. Cómo se prueba

| Qué | Cómo |
|---|---|
| Banco de 64, anotación de fábrica en el lugar 64, saltos de banco en PRESETS | test native |
| Migración desde el área de 512 KB con tus presets reales | en el hardware (log `[PRESET]` al arrancar) |
| El bibliotecario exporta los 64 y `bank` reescribe un banco largo | en el hardware |

## 4. Qué no cubre

Un archivo por banco (5× menos espacio, no hace falta con 2 MB) y la microSD de la Teensy.
