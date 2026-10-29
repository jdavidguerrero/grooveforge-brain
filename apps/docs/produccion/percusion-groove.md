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

**Carpetas por versión y variación (rev. 5):** `midi/perc/<genero>/<top|perc>/<intro|main|peak|main_q>/<v1…v4>/`
(`v1` = el patrón original, `v2`–`v4` = las variaciones de los elegidos); `favoritos/` sigue la misma estructura. Las
raíces quedan en `raices/<top|perc>/`.

### Rev. 6: ronda 2 de escucha (2026-10-27)

Juan filtró las variaciones: **37 elecciones** (`favoritos` con `"ronda": 2` en la ficha de cada variación), copiadas
a `midi/perc/seleccion/` con la misma estructura. Lo que dicen:

- **Tribe gana en todos los géneros** (34 de 37): la percusión colombiana encaja también en afro, progressive e indie.
- **Peak** casi siempre; en afro house, además, el **perc cuantizado** (`main_q`): el cuerpo de tambores recto y el
  top con swing.
- Cobertura: afro house 14, organic 10, indie tech 8, melodic / progressive 4 (sin perc), disco / house 1 (sin perc).

### Rev. 7: más Tribe en indie tech, el lado oscuro; el 808 latino queda pendiente (2026-10-27)

Juan confirma la idea de fondo: **los patrones son roles** (graves, medios, agudos, sacudidos, campanas…) y el rack solo
cambia el acento (africano, brasileño, colombiano). Tribe es su sello: suena natural (alterna manos) y es marca
colombiana. Quiere ver cómo encaja en **indie tech**, porque es más oscuro.

Lo oscuro del indie tech en percusión: casi recto (swing 50–51 %), hipnótico (polirritmos que se repiten), tonos
**apagados** y graves más que abiertos y brillantes, pocos golpes y bien puestos, el brillo solo en el peak. Junto a
`mapale_tech` (que es más festivo), tres patrones en Tribe:

| Patrón | Idea |
|---|---|
| `tambora_hipnotica` | cáscara de tambora cada 3 semicorcheas (un ciclo de 3 sobre el 4/4 que se corre), tambora apagada, maracón suave a contratiempo; peak con guacharaca en los backbeats |
| `quemao_oscuro` | alegre en bajoneo (apagado) en 2 compases, quemaos secos y escasos, caja fondo en 2 y 4, guacharaca; peak con guache en corcheas |
| `bombo_cascara_drone` | el aro del bombo en semicorcheas como un pulso de madera continuo, tambora abierta y alegre escasos, latigazo en el remate; peak con caja quemao a contratiempo |

Mismas reglas: graves fuera de las semicorcheas del kick, versiones intro / main / peak / main_q, top y perc aparte.
A Juan le gustaron los tres: quedan en `favoritos` (ronda 1) y generan sus variaciones `_v2`–`_v4`.

**Pendiente:** cambiar el GF 808 Perc por un rack **GF Latin** con samples reales (tumba, congas, bongó, güiro,
maracas, cencerro, clave, timbal) cuando Juan los consiga, con el mismo mapa de roles: todos los patrones salen con ese
sabor sin reescribirlos.

### Rev. 8: fills a partir de las raíces, para afro y organic (2026-10-27)

A Juan le gustaron las raíces **colombianas** (cumbia, puya, bullerengue, mapalé) y **brasileñas** (samba reggae,
maracatu, baião), y `mapale_tech` (a favoritos). Las raíces quedan marcadas en su ficha como fuente de fills.

**Por qué salen de las raíces:** cada raíz es una frase tradicional de 2 compases que ya termina en su remate (el
compás 2 cierra con el redoble: `XXXX`, `Xxxx`…). El final de una frase es justamente lo que hace un fill.

**Para qué géneros:** afro house y organic house, donde la percusión es protagonista y un fill tribal es parte del
track. En melodic / progressive, indie tech y disco el fill es otra cosa (más sutil, seco o de congas): después, si
hace falta.

**Tres tipos, cada uno en largos que caen justo antes del cambio de sección:**

| Tipo | Largos | Qué es |
|---|---|---|
| **remate** | 1 tiempo · 2 tiempos · 1 compás · 2 compases | la cola de la frase (sus últimas semicorcheas): cierra cada 8 / 16 compases |
| **entrada** | 1 compás · 2 compases | el pickup al drop: la cola de la frase en crescendo (la velocity sube un escalón cada mitad), los graves solo en la segunda mitad y un redoble `xxXX` en el último tiempo del tambor que lleva la frase; termina antes del 1 |
| **break** | 1 compás · 2 compases | la llamada sin graves, desde el comienzo de la frase: para cuando se cae el kick |

- Cada fill arranca en el tiempo 0 del clip, así el clip mide lo que el fill.
- Racks: el de la raíz y Tribe; en afro house también djembe.
- Top y perc por separado, como el resto. Con swing y microtiming de la raíz.
- Carpetas: `midi/perc/<afro_house|organic_house>/fills/<remate|entrada|break>/<top|perc>/`, nombre
  `<raiz>_<tipo>_<largo>_<rack>.mid` con largo `1beat` · `2beat` · `1bar` · `2bar`. Una ficha por raíz en `fills/`
  (con `favoritos`, para filtrar escuchando).

### Rev. 9: roll acelerado de 8 compases para el break (2026-10-29)

Juan usa siempre riser + snare roll en las transiciones (regla declarada, `motor-asistencia.md` §8) y pidió el roll con
instrumentos nativos y de **8 compases que aceleren**: negras → corcheas → semicorcheas → fusas, dos compases cada una
(8 · 16 · 32 · 64 golpes). Es la figura clásica del *build* de house: la densidad sube al doble cada dos compases, así
la tensión crece sin cambiar de tempo.

- **Voz principal:** el rol `caja` — en Tribe, **Caja Fondo L / R alternando por golpe** (las manos alternan de verdad:
  en un roll continuo la paridad de semicorchea de `to_tribe` no sirve, con fusas dos golpes caen en la misma); en el
  kit brasileño, la **caixa**.
- **Peso:** desde el compás 5, el rol `grave_abierto` en negras (tambora / surdo), que empuja hacia la vuelta del groove.
- **Dinámica:** crescendo lineal de velocity 40 → 120 a lo largo de los 8 compases, con acento en cada tiempo.
- Archivos: `midi/adn/estudio/roll_acelerado_{tribe,brasil}.mid` (`tools/paleta-lab/make_estudio_clips.py`). Es
  humanización programada (auditoría §7): un patrón del generador, no una interpretación tocada.

### Rev. 10: base nueva para el caso 2 (est-indie) (2026-10-29)

Juan quiere que el caso 2 arme su **propia base**: patrones nuevos y no los kits del lab. El orden sigue su ADN:
percusión, después bajo y después melodías. Se mantienen las reglas de la Rev. 7 para el indie tech:
- casi recto (swing 50 %);
- hipnótico;
- tonos apagados;
- pocos golpes;
- brillo solo en el peak;
- graves fuera de las semicorcheas del kick (1, 5, 9, 13).

Todo va en Tribe. Hay tres candidatos y cada uno prueba **una idea rítmica que todavía no está en la librería**:

| Patrón | Idea | Lo nuevo frente a la Rev. 7 |
|---|---|---|
| `quintillo_cascara` | Cáscara de tambora cada **5** semicorcheas. Tambora apagada en la "a" del 2 y del 4. Alegre bajoneo escaso | Ciclo de 5 sobre 16: tarda 5 compases en volver al mismo lugar. Es más largo y menos obvio que el de 3 de `tambora_hipnotica` |
| `llamador_seco` | El llamador hace su papel de la cumbia, **el golpe a contratiempo**, pero en bajoneo (apagado). Tambora apagada sincopada. Un solo quemao cada 2 compases | La raíz cumbiera del llamador, reducida a un pulso oscuro |
| `bullerengue_tech` | Alegre bajoneo en **3-3-2** (el tresillo que en el bullerengue sentao viene del 6/8). Tambora apagada en las síncopas. Caja fondo solo en el 3 del compás 2 y del 4 (medio tiempo) | El 3-3-2 en la percusión media y un backbeat a medio tiempo, que en indie da peso sin acelerar |

El peak agrega guacharaca o guache. El compás 4 trae remate. La audición compara solo el **main**, con el kick 4x4
como único fondo, porque todavía no hay bajo ni acordes elegidos. Archivos:
`midi/adn/estudio_indie/perc_<patrón>_{main,peak}_tribe.mid`, generados por `tools/paleta-lab/make_estudio_clips.py`.

Referencias: el llamador como marcador del contratiempo en la cumbia y el 6/8 del bullerengue, en Convers &
Ochoa, *Gaita Colombiana: método de aprendizaje* (Ministerio de Cultura, 2007), y Ochoa, *Músicas locales en tiempos
de globalización* (2003). La polirritmia de ciclos impares sobre 4/4 está en Toussaint, *The Geometry of Musical
Rhythm* (2013, cap. sobre ritmos euclidianos).
