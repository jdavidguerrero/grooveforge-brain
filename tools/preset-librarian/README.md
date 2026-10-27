# Bibliotecario de presets del Brain

Saca los presets del GrooveForge Brain a una librería de archivos JSON (uno por sonido, con ficha) y los sube de vuelta
por el USB. Diseño: `apps/docs/variants/studio/theory/11-studio-preset-librarian.md`.

```bash
# bajar todos los presets a ~/GROOVEFORGE/sound-library/brain/<engine>/<nombre>.json
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py export

# subir archivos al final del banco de su engine (el "name" del JSON es el nombre en el Brain)
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py import ruta/al/preset.json

# reescribir los presets de un engine en este orden, hasta 64 = bancos A–D (lo que no esté en la lista sale del Brain)
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py bank juno_106 a.json b.json c.json

# listar lo que hay en el Brain
~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py list
```

- Usa el Python de PlatformIO porque ya trae `pyserial`. La librería va en `$GF_LIBRARY` o `~/GROOVEFORGE/sound-library`.
- En cada JSON se completan a mano `role` (bass · chords · pad · lead · pluck · perc · fx), `tags`, `key_range`,
  `pairs_with` y `notes`. Un `export` posterior conserva esa ficha y solo actualiza los parámetros.
- Los parámetros se editan en el Brain; el bloque `patch` es para leer y para la IA. Se sube siempre `raw`.
- Tests sin Brain: `python3 -m pytest tools/preset-librarian/tests`.
