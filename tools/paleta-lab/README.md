# Laboratorio de paletas

Herramientas del laboratorio de identidad (`apps/docs/produccion/ia-local-refinamiento.md` §3 y `protocolo-captura.md`).

```bash
# al empezar una sesión: rol, hasta 3 candidatos y la ganancia de la interfaz
python3 tools/paleta-lab/gf_session.py new bass "JD RUN FUNKBASS" "GF POCKET BASS" "HY WOODBASS B" --ganancia "11 h"

# mientras escuchas: A contra B, cuál y por qué (a, b o el nombre)
python3 tools/paleta-lab/gf_session.py decide "JD RUN FUNKBASS" "GF POCKET BASS" a "las ghost notes se sienten" --contexto "afro 124"

# lo que descartas también se guarda
python3 tools/paleta-lab/gf_session.py reject "HY WOODBASS B" "el golpe de madera tapa la tambora"

# origen y permisos de un sample externo
python3 tools/paleta-lab/gf_session.py permiso "LOOP TRIBE 03" tribe-instruments --no-entrenar

# resumen
python3 tools/paleta-lab/gf_session.py show

# al terminar: los presets tal como sonaron
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py export
```

Las fichas van en `~/GROOVEFORGE/GrooveForge_Dataset_v1/metadata/sesiones/` (o `$GF_DATASET`). Tests:
`python3 -m pytest tools/paleta-lab/tests`.

## Percusión y top loops (`make_perc_patterns.py`)

```bash
python3 tools/paleta-lab/make_perc_patterns.py            # 124 BPM
```

Escribe los kits (`kits/*.json`) y los patrones en `midi/perc/<genero>/top/` y `.../perc/` (y `raices/`), con el rack
en el nombre: `<patron>_<intro|main|peak|main_q>_<djembe|brazilian|808|tribe>.mid`. Volver a correrlo conserva lo que
anotaste en `kits_probados` y `notas`. Los Drum Racks GF salen de `make_gf_racks.py`.
Diseño: `apps/docs/produccion/percusion-groove.md` §7.

Lo que elijas (campo `favoritos` de la ficha) genera variaciones `_v2…_v4` y se copia a `midi/perc/favoritos/`.

## Sesión de paleta por ADN (`make_adn_clips.py`)

Patrones para tocar (no para medir) sobre una sola progresión, clips de las 8 capas híbridas (CC 18 + MORPH) y tres
contextos de percusión que cruzan estilos, en `GrooveForge_Dataset_v1/midi/adn/` con un README para armar
`GF_Paleta_Lab.als`. Plan: `apps/docs/produccion/sesion-paleta-adn.md`.

```bash
python3 tools/paleta-lab/make_adn_clips.py            # La menor, 124 BPM
```

`fill_lab_set.py "<ruta>/GF_Paleta_Lab.als"` carga esos clips en el set que Juan creó en Live (escenas 1–6 =
contextos A–F; desde la 7, un patrón del Brain por escena; tempo 124). Copia el original a `.als.bak`. Los CC de las capas
no van en el .als: se arrastra el `.mid` de `capas/` o se elige la capa en el Brain.

## Lab de leads (`make_lead_lab.py`)

12 escalas en 4 familias, cada una con su progresión (La como centro, 124 BPM), × 4 rítmicas (corta, mixta A, mixta
B, larga), más un clip de acordes por escala, en `midi/adn/lead/lab/<familia>/`. El nombre lleva los dialectos
esperados: `hungara_menor__mixta_a__melodic_organic.mid`. Diseño: `apps/docs/produccion/lab-leads.md`.

```bash
python3 tools/paleta-lab/make_lead_lab.py            # La, 124 BPM
```

Para escucharlos en Live (una vez), sobre la sesión de siempre:

1. Abre `GF_Paleta_Lab_clips.als` y agrega una pista MIDI llamada **ACORDES** con un instrumento de Live suave (por
   ejemplo Drift o Electric), a volumen bajo: es la referencia armónica, no parte del sonido.
2. *Save Live Set As…* → `GF_Lead_Lab.als`, en la misma carpeta. Cierra ese set (abre otro) antes del paso 3.
3. `python3 tools/paleta-lab/fill_lab_set.py --leads "<ruta>/GF_Lead_Lab.als"` y vuelve a abrirlo.

`--leads` **agrega** 48 escenas debajo de las que ya hay (no toca nada de arriba): en cada una el kick, el lead en
BRAIN MIDI y los acordes de su escala en ACORDES. Se corre una sola vez por set. El Brain toca un sonido a la vez:
mientras suena el lead no suena el bajo del Brain.

`--dialectos <ficha.json>` agrega la ronda de dialectos: los clips de `lab_leads.clips_que_gustan` de la ficha sobre la
percusión de cada dialecto (afro, organic, melodic, indie, disco), un bloque por dialecto (`lab-leads.md` §10).

## Presets JD LEAD (`make_jd_leads.py`)

Deriva JD LEAD MELODIC (de GF MELODIC LEAD), JD LEAD BODY (de JD RUN LEAD BODY) y la capa JD DESERT LD (de DESERT
LEAD) con los cambios contra la estridencia de `apps/docs/produccion/lab-leads.md` §5. Lee la librería (exportarla
antes) y escribe los JSON; no toca el Brain.

```bash
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py export && \
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py layers
~/.platformio/penv/bin/python tools/paleta-lab/make_jd_leads.py
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py import <lib>/ob_6/jd-lead-melodic.json <lib>/prophet_5/jd-lead-body.json
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py layer-import <lib>/layers/jd-desert-ld.json
```

## Dosis de Tribe y ronda con kits (`make_tribe_dosis.py`)

Un elemento de Tribe, suave, por dialecto (`apps/docs/produccion/sesion-paleta-adn.md` §1d) en
`midi/perc/dosis_tribe/<dialecto>_tribe.mid`. `fill_lab_set.py --dialectos <ficha> --kits` arma la ronda de dialectos
con un kit por dialecto (pistas KIT DJEMBE, KIT BRASIL y KIT 808 con los racks GF) y la dosis en PERC.

```bash
python3 tools/paleta-lab/make_tribe_dosis.py
```

## Dosis de color (`make_lead_lab.py` + `fill_lab_set.py --dosis`)

`make_lead_lab.py` escribe también `midi/adn/lead/lab/dosis/<escala>__<rítmica>__{remate,de_paso}.mid`: la frase
sin notas de color y una sola nota de color, al cierre (remate) o de adorno (de paso) (`lab-leads.md` §4, rev. 4).
Las pruebas están en `DOSE_TESTS` (escala, rítmica, dialecto). Para escucharlas, sobre una copia nueva del set con
kits (con Live cerrado o sin ese archivo abierto):

```bash
cp GF_Lead_Dialectos_kits.als GF_Lead_Dosis.als
python3 tools/paleta-lab/fill_lab_set.py --dosis GF_Lead_Dosis.als
```

Reemplaza la ronda de dialectos por las escenas de dosis (completa · remate · de paso por prueba, con el kit del
dialecto y la dosis de Tribe) y quita las escenas vacías de abajo.

## Lab de acordes (`make_chord_lab.py` + `fill_lab_set.py --acordes`)

Tres progresiones (menor con V prestado, kumoi, lidia) × seis órdenes (bloque, Alberti, arpegio, arco, 1-5-8,
décimas) × cuatro ritmos (corcheas, tresillo, contratiempo, respiro), con los acordes conducidos a la inversión más
cercana (`apps/docs/produccion/lab-acordes.md`). Escribe `midi/adn/chords/lab/<progresion>__<orden>__<ritmo>.mid`.

```bash
python3 tools/paleta-lab/make_chord_lab.py
cp GF_Lead_Dialectos_kits.als GF_Acordes_Lab.als
python3 tools/paleta-lab/fill_lab_set.py --acordes GF_Acordes_Lab.als
```

El set queda con el kit afro y la dosis de Tribe, sin lead; los acordes van en ACORDES (el Electric de Live).

## Presets JD KEYS (`make_jd_keys.py`)

Pianos eléctricos parecidos al Electric de Live (`apps/docs/produccion/lab-acordes.md` §8): JD KEYS JUNO (de PLUCK
KEYS), JD KEYS PROPHET (de WARM KEYS) y la capa JD KEYS CAPA (JD KEYS JUNO + E.PIANO reducido al golpe). Lee la
librería y escribe los JSON; no toca el Brain.

```bash
~/.platformio/penv/bin/python tools/paleta-lab/make_jd_keys.py
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py import <lib>/juno_106/jd-keys-juno.json <lib>/prophet_5/jd-keys-prophet.json
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py layer-import <lib>/layers/jd-keys-capa.json
```

## Snapshot del laboratorio (`gf_snapshot.py`)

Congela MIDI, presets, kits, fichas, los `.als` y las tomas de Live en `GrooveForge_Dataset_v1/snapshots/<fecha>/` con
un `manifest.json` (SHA-256 por archivo, formato real de los WAV, commit del repo, firmware declarado). Con `--brain`
exporta además los presets **del Brain** y anota qué difiere de la librería. Un snapshot no se edita. Ver
`apps/docs/produccion/protocolo-captura.md` §6.

    python3 tools/paleta-lab/gf_snapshot.py --brain --firmware "<lo que se sabe>"
