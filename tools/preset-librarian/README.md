# Bibliotecario de presets del Brain

Saca los presets del GrooveForge Brain a una librería de archivos JSON (uno por sonido, con ficha) y los sube de vuelta
por el USB. Diseño: `apps/docs/variants/studio/theory/11-studio-preset-librarian.md`.

```bash
# bajar todos los presets a ~/GROOVEFORGE/GrooveForge_Dataset_v1/patches/brain/<engine>/<nombre>.json
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py export

# subir archivos al final del banco de su engine (el "name" del JSON es el nombre en el Brain)
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py import ruta/al/preset.json

# reescribir los presets de un engine en este orden, hasta 64 = bancos A–D (lo que no esté en la lista sale del Brain)
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py bank juno_106 a.json b.json c.json

# listar lo que hay en el Brain
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py list

# presets de capa de HYBRID SYNTH (theory/12 §3d) → patches/brain/layers/<nombre>.json
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py layers
# subirlos al final (o reescribir los 32 en este orden con --reemplazar); el "name" del JSON renombra la capa
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py layer-import capa.json [--reemplazar]
```

- Usa el Python de PlatformIO porque ya trae `pyserial`. La librería va en `$GF_LIBRARY` o en el dataset de la paleta
  (`~/GROOVEFORGE/GrooveForge_Dataset_v1/patches`).
- En cada JSON se completan a mano `role` (bass · chords · pad · lead · pluck · perc · fx), `tags`, `key_range`,
  `pairs_with`, `notes` y `rating` (1–5). `origin` y `permissions` (`train`, `publish`) vienen puestos: los presets del
  Brain son propios o de fábrica del Brain. Un `export` posterior conserva toda la ficha y solo actualiza los parámetros.
- Los parámetros se editan en el Brain; el bloque `patch` es para leer y para la IA. Se sube siempre `raw`.
- Tests sin Brain: `python3 -m pytest tools/preset-librarian/tests`.
