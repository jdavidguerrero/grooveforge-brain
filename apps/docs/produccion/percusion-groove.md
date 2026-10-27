# Percusión y top loops: mapa común, kits y biblioteca de patrones

> **Fecha:** 2026-10-27 · **Estado:** propuesta de trabajo para Juan · **Depende de:** `ia-local-refinamiento.md` §3 y §6,
> `protocolo-captura.md`.

La percusión colombiana y sudamericana puede ser el sello de Juan. Este documento define cómo trabajarla para que los
patrones se puedan variar, cambiar de instrumento y, más adelante, generar loops nuevos.

## 1. Dónde va en el laboratorio

Sesión B, **justo después del bajo**: el groove es kick + bajo + percusión y se arma junto.

1. Bajo → 2. **Groove kit** (kick de Kick 3 + percusión + top loops) → 3. Chords → plucks → lead → pad → percusión tonal del
Brain → textura.

## 2. Fuentes

| Fuente | Qué es | Qué trae | Cómo entra a Live |
|---|---|---|---|
| **Tribe Instruments**, South America Series | Plugin VST/AU (motor Tribe Core) | Colombian Percussion (Tambores de San Jacinto): bombo del Pacífico, tambora, alegre, llamador, maracón, guache, caja vallenata · Andean Percussion: bombo legüero, caja andina, tambores chamánicos, chajchas, panderetas · marimba de chonta y más | Instrumento por pista; **trae ritmos con MIDI** (San Jacinto; carnavalito, huayno, guabina…) |
| Percusión brasileña | Samples | (surdo, repique, caixa, agogô, ganzá… según la carpeta) | Drum Rack con el mapa común |
| Djembes y dun duns | Samples | Djembe (bass, tone, slap), dundunba / sangban / kenkeni y sus campanas | Drum Rack con el mapa común |
| 808 | Samples o Drum Rack | Congas, bongos, cowbell, rimshot, maracas | Drum Rack con el mapa común |
| Kick 3 | Plugin | Kicks (2–3 elegidos) | Su pista, fuera del mapa |

La licencia de Tribe no está en su página de producto (remite a una página de licencia aparte): **revisarla** antes de
samplear sus sonidos o usarlos para entrenar. Los MIDI de ritmos que trae Tribe son contenido de Tribe: sirven de
**referencia**, no son material propio.

## 3. El mapa común ("GF Perc Map")

Cada nota es un **rol rítmico**, no un instrumento: un patrón escrito sobre el mapa suena con cualquier kit. Notas en la
convención de Live (C1 = MIDI 36).

| Nota | Rol | Colombia (Tribe) | Brasil | África | 808 |
|---|---|---|---|---|---|
| C1 (36) | Grave abierto | Tambora (parche) · bombo del Pacífico | Surdo | Dundunba · djembe bass | — |
| C#1 (37) | Grave apagado | Tambora apagada | Surdo apagado | Dundun apagado | — |
| D1 (38) | Medio abierto | Alegre | Atabaque / timbal | Sangban · djembe tone | Conga low |
| D#1 (39) | Medio apagado / slap | Alegre tapado | — | — | — |
| E1 (40) | Agudo abierto | Llamador | Repique | Kenkeni | Conga high · bongo |
| F1 (41) | Agudo slap | Llamador seco | Repique (rim) | Djembe slap | Bongo high |
| F#1 (42) | Aro / madera | Tambora (aro) | Caixa (aro) | — | Rimshot |
| G1 (43) | Sacudido corto | Guache · maracón | Ganzá | Shekere | Maraca |
| G#1 (44) | Sacudido largo | Guache (arrastre) | Chocalho | — | — |
| A1 (45) | Raspado | Guacharaca | Reco-reco | — | — |
| A#1 (46) | Campana grave | Cencerro | Agogô (grave) | Campana del dundun | Cowbell |
| B1 (47) | Campana aguda | — | Agogô (aguda) | Gankogui | — |
| C2 (48) | Caja / tarola | Caja vallenata · caja andina | Caixa | — | Snare |
| C#2 (49) | Pandereta | Pandereta andina | Pandeiro | — | Tambourine |
| D2 (50) | Efecto | Tambores chamánicos | Cuíca | — | — |

Los instrumentos de cada columna son ejemplos: el archivo de cada kit (§4) dice qué suena de verdad en cada rol.

## 4. Kits

Un **kit** dice qué toca cada rol en una fuente concreta (`GrooveForge_Dataset_v1/kits/<kit>.json`):

- **Samples (Brasil, djembes, 808):** un Drum Rack en Live con cada pad en la nota del mapa. No hace falta convertir nada:
  el patrón suena directo.
- **Tribe:** cada instrumento tiene su propio teclado (qué tecla toca qué golpe). El archivo del kit anota esas teclas y una
  herramienta convierte un patrón del mapa a las notas de Tribe (por pista: tambora, alegre, llamador, guache…). Para
  completarlo hay que leer el teclado de cada instrumento en Tribe Core.
- **Extraer los golpes de Tribe** a one-shots solo hace falta para un kit unificado (Tribe + djembes + 808 en un solo Drum
  Rack), y después de revisar la licencia. Tocar Tribe por MIDI conserva sus articulaciones, capas de velocity y
  variaciones.

## 5. Biblioteca de patrones (lo que después genera loops nuevos)

`GrooveForge_Dataset_v1/midi/perc/<patron>.mid` sobre el mapa común, cada uno con su ficha:

```json
{
  "patron": "cumbia_base_01",
  "estilo": "cumbia", "bpm": 124, "compases": 2,
  "roles": ["grave", "medio", "agudo", "sacudido_corto"],
  "swing": {"agudo": "+6 ms en contratiempos", "sacudido_corto": "56 %"},
  "densidad": "media",
  "origen": "propio",              // propio · ritmo tradicional (dominio público) · referencia (MIDI de Tribe u otra librería)
  "version_cuantizada": "cumbia_base_01_q.mid",
  "kits_probados": ["tribe_colombia", "brasil_samples"],
  "notas": ""
}
```

- **Microtiming por instrumento** (ia-local-refinamiento §2): patrones repetibles (por ejemplo, el llamador siempre un poco
  adelante), siempre con su versión cuantizada para comparar.
- **Top loop** = el patrón completo renderizado con un kit, en `renders/` junto a su MIDI y su ficha. Así cada loop se puede
  volver a generar con otro kit o con variaciones.
- **Generar loops nuevos, más adelante:** primero con **reglas** sobre tus patrones (densidad, acentos, pregunta y respuesta
  entre tambores, remates cada 8 compases, cambio de kit). Un modelo solo si supera a esas reglas (ia-local-refinamiento §8).

## 6. Sesión del groove kit

1. `gf_session.py new perc` con el kit o los patrones candidatos.
2. Elegir el kick (Kick 3) que va con el bajo elegido.
3. Partir de un ritmo de referencia (los MIDI de Tribe, un ritmo tradicional) y **hacerlo tuyo**: quitar, mover, acentuar,
   cambiar el kit. Lo que queda es un patrón `propio`.
4. Probar el mismo patrón con 2 kits (por ejemplo Tribe Colombia y djembes) y anotar la decisión.
5. Renderizar el top loop seco y en contexto (con kick y bajo), y guardar el MIDI con su ficha.

## 7. Kits reales y primera biblioteca de patrones (2026-10-27)

### Kits

| Kit | Qué es | Cómo se toca el mapa |
|---|---|---|
| `tribe_colombia` | **Colombian Percussion** de Tribe: un instrumento con bombo, tambora, llamador, alegre, guache, maracón, caja y guacharaca en un solo teclado (C1 Bombo Open … D5 Guacharaca Longer) | `make_perc_patterns.py` convierte cada rol a la nota de Tribe. Las articulaciones L / R se alternan por semicorchea (par = L, impar = R); el guache usa Down en las pares y Up en las impares, como la mano |
| `djembe_dundun` | Samples: 6 djembe, 7 dununs (sangban, kenke, dundumba), 4 campanas, xeke y tchekere | Drum Rack con cada sample en la nota de su rol (el archivo del kit dice cuál) |
| `brasil_samples` | Samples: surdo, alfaia, zabumba, atabaque, rum/rumpi/le, timbal, caixa, repinique, tamborim, pandeiro, agogô, triángulo, ganzá, chocalho, caxixi, reco-reco, cuíca… | Drum Rack con el mapa |
| `808_perc` | Congas, bongos, rimshot, maracas, cowbell, claves, tambourine de la 808 | Drum Rack con el mapa |

**A verificar en Live (una vez):** que Tribe use la misma numeración que Live (C1 = MIDI 36). Se prueba tocando C1:
si suena el **Bombo Open**, está bien; si suena una octava corrida, se cambia `"octava"` en el archivo del kit.

### Patrones (`midi/perc/`)

Cada patrón son 2 compases a 124 BPM sobre el mapa común, con su versión **cuantizada** (`_q`) y la versión con
**microtiming** (swing por rol y adelantos / atrasos en ms, que es lo que hace sonar humano un top loop). Los ritmos
tradicionales son de dominio público y van como `origen: ritmo tradicional`; adaptados a 4/4 y 124 BPM son puntos de
partida para hacerlos tuyos (§6), no transcripciones.

| Patrón | Origen | Idea |
|---|---|---|
| `cumbia_base` | tradicional (Caribe colombiano) | llamador en los contratiempos, tambora con cáscara, alegre que dialoga, guache en semicorcheas |
| `puya_vallenata` | tradicional (vallenato) | caja vallenata y guacharaca rápida: la energía del raspado |
| `bullerengue` | tradicional (Caribe colombiano) | llamador en el pulso, frases de alegre, maracón |
| `mapale` | tradicional (Caribe colombiano) | alegre denso y tambora empujando: el más intenso |
| `kuku_afro` | tradicional (Guinea, adaptado a 4/4) | dunun, campana, sangban, kenke, djembe y xeke |
| `samba_reggae` | tradicional (Bahía) | surdo de dos golpes, timbal, caixa |
| `maracatu` | tradicional (Pernambuco) | alfaia, agogô en dos alturas y caixa redoblando |
| `baiao` | tradicional (Nordeste) | zabumba con bacalhau y triángulo |
| `afro_house_groove` | propio | shaker con swing, djembe pregunta y respuesta, dunun en los contratiempos, campana |
| `house_conga` | propio | shaker 16 con swing 56 %, congas, rim y pandereta: el top loop clásico de house |
| `organic_shaker_wood` | propio | sacudidos, raspado y aro de madera, sin graves: para encima de cualquier groove |
| `tribal_toms` | propio | toms medios y agudos con remate al final del compás 2: indie tech |

### Rev. 2: top loops por género y Drum Racks GF (2026-10-27)

Los ritmos de arriba quedan en `midi/perc/raices/` como **referencia**. Los top loops se arman **por género** en
`midi/perc/<genero>/`, pensados para ir encima del kick 4/4:

- **4 compases** en tres versiones: `intro` (solo sacudidos / campanas), `main` (con remate en el compás 4) y `peak`
  (main + una capa más de densidad). `main_q` = la main cuantizada.
- Los graves evitan las semicorcheas 1, 5, 9 y 13 (el kick de Kick 3). Excepción a propósito: `samba_house`
  (de Juan), con el surdo en 2 y 4 reforzando el kick.
- Cada patrón trae su **kit sugerido**.

| Género | Patrones |
|---|---|
| afro house | `djembe_sang` (Juan), `afro_house_groove`, `afro_dunun_roll` |
| organic house | `cumbia_afro` (Juan, Tribe), `organic_shaker_wood`, `organic_soft_conga` |
| melodic / progressive | `prog_rolling16`, `prog_offbeat_rim` |
| disco / house | `house_conga`, `samba_house` (Juan), `disco_tamb_cowbell` |
| indie tech | `tribal_toms`, `indie_metal_rim` |

**Drum Racks GF** (`tools/paleta-lab/make_gf_racks.py`): toma los racks de Juan (*Djembe Assembly*, *Brazilian
Percussion Rack*, *808 Core Kit*), pone cada sample en la nota de su rol y lo que no tiene rol de 51 hacia arriba, y
guarda racks nuevos en la User Library: **GF Djembe Dunun** (con slap, xeke, tchekere, long y bass con xeke, que no
estaban), **GF Brasil** (los 48 one-shots) y **GF 808 Perc**. Los racks de Juan no se modifican.

Herramientas: `tools/paleta-lab/make_perc_patterns.py` (patrones, fichas y versiones para Tribe en
`midi/perc/tribe_colombia/`) y `make_gf_racks.py`.

### Rev. 3: top loop y loop de percusión por separado, el rack en el nombre (2026-10-27)

En cada género (y en `raices/`) los patrones salen en dos carpetas, para mezclarlos aparte:

- **`top/`**: sacudidos, campanas, aros, agudos (slap, quemao, kenke, tamborim, conga aguda), raspado y pandereta. Vive
  arriba de ~300 Hz: filtro pasaaltos alto, bajo de nivel, ancho.
- **`perc/`**: graves y medios (dundunba, sangban, tambora, bombo, surdo, atabaque, congas bajas, toms), caja y efectos.
  Es la zona del kick y del bajo: pasaaltos ~80–100 Hz para dejarle el sub al bajo, sidechain al kick, afinar a la
  tonalidad. La dundunba y el bombo van acá, no en el top.

Nombre: `<patron>_<version>_<rack>.mid`, con `<version>` = `intro` · `main` · `peak` · `main_q` y `<rack>` =
`djembe` (GF Djembe Dunun) · `brazilian` (GF Brasil) · `808` (GF 808 Perc) · `tribe` (Tribe Colombian Percussion, ya
convertido a su teclado). Así top y perc de patrones distintos se combinan libremente.

### Idea para después (no ahora: primero la recolección)

`gf_toploops.py "<track>" --genero … --estructura "intro:16 groove:16 build:8 drop:32 break:16 …"`: top y perc del
largo del track, cada sección en su nivel (intro → `intro`, groove → `main`, build → `main` subiendo, drop → `peak`,
break → top `intro` o nada), remates cada 8 / 16 compases, A/B entre patrones del género, mutaciones con semilla y
varias propuestas para elegir. Las elecciones anotadas en las fichas son el dato para la IA local más adelante.

### Rev. 4: lo elegido genera más, y Tribe en todos los géneros (2026-10-27)

- **Favoritos:** Juan escuchó y eligió (sobre todo las versiones **peak**): los 13 patrones por género, con detalle de
  versión / parte / rack en varios (p. ej. `afro_dunun_roll` peak top y perc en djembe, `cumbia_afro` peak top en
  tribe, `organic_soft_conga` peak top en 808). Quedan en el campo `favoritos` de cada ficha, que el generador
  conserva.
- **Variaciones:** cada patrón con `favoritos` genera `_v2`, `_v3` y `_v4` (`mutate()`, semilla fija): mueve algún
  golpe una semicorchea, cambia acentos, suma o quita ghosts; mismos roles; ningún grave nuevo en los tiempos del kick.
- **Tribe en todos los géneros:** cada patrón sale también con `_tribe` (convertido al teclado de Colombian
  Percussion), y cada género tiene un patrón colombiano propio: `tambora_afro` (afro house), `bullerengue_organic`
  (organic), `guache_rolling` (melodic / progressive), `cumbia_disco` (disco / house), `mapale_tech` (indie tech).
