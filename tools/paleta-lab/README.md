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
