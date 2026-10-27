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

Escribe los kits (`kits/*.json`: Tribe Colombian, djembes/dununs, Brasil, 808) y 12 patrones de 2 compases en
`midi/perc/` sobre el GF Perc Map (C1 = 36), cada uno con su versión cuantizada `_q`, su ficha `.json` y la versión
para Tribe en `midi/perc/tribe_colombia/`. Volver a correrlo conserva lo que anotaste en `kits_probados` y `notas`.
Diseño: `apps/docs/produccion/percusion-groove.md` §7.
