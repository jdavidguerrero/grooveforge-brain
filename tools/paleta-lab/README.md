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
