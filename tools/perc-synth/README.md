# perc-synth

Percusión orgánica por síntesis modal, fuera de tiempo real. Teoría y criterio de prueba:
`apps/docs/theory/percusion-organica-dsp.md`.

```bash
uv run pytest
uv run python -m gf_perc demo --out ~/GROOVEFORGE/GrooveForge_Dataset_v1/renders/perc-synth/v0.1
uv run python -m gf_perc variantes --instrumento clave -n 100 --out /ruta/de/salida
```

`seed_timbre` fija el instrumento y `seed_golpe` la ejecución. Cada WAV queda en `manifiesto.jsonl` con sus parámetros.

## Kits tipo 808

```bash
uv run python -m gf_perc kits --out ~/GROOVEFORGE/GrooveForge_Dataset_v1/renders/perc-synth/v0.2
```

Genera `808_referencia`, `808_madera`, `808_piel` y `808_mixto`. Cada WAV empieza con su nota del GF Perc Map. Teoría:
`apps/docs/theory/percusion-kits-808-organica.md`.
