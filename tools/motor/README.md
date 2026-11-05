# Motor de asistencia

Primera entrega de `apps/docs/produccion/motor-asistencia-v2.md`. Teoría: `apps/docs/theory/motor-primera-entrega.md`.

Con Live abierto y un set nuevo creado desde el template:

```bash
cd tools/motor && uv run python -m gf_motor.servidor
```

Abre `http://127.0.0.1:8765`. Cerrar el motor no afecta a Live.

- **Eventos:** `$GF_DATASET/metadata/eventos/`. **Borradores y opciones:** `$GF_DATASET/metadata/motor/`.
- **Kick elegido:** `GrooveForge_Library/00 Elegidos/`, en el SSD. Se cambia con `GF_LIBRARY`.
- **Tests:** `uv run pytest`. Usan un Live simulado y no tocan tu set.
