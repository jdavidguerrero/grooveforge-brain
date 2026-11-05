# Motor de asistencia

Primera entrega de `apps/docs/produccion/motor-asistencia-v2.md`. Teoría: `apps/docs/theory/motor-primera-entrega.md`.

Con Live abierto, `GFMotor` activo y un set nuevo creado desde el template:

```bash
cd tools/motor && uv run python -m gf_motor.servidor
```

Abre `http://127.0.0.1:8765`. Cerrar el motor no afecta a Live.

- **Eventos:** `$GF_DATASET/metadata/eventos/`. **Borradores y opciones:** `$GF_DATASET/metadata/motor/`.
- **Kick elegido:** `GrooveForge_Library/00 Elegidos/`, en el SSD. Se cambia con `GF_LIBRARY`.
- **Puente `GFMotor`:** es independiente del script de GroovePilot. Instálalo y actívalo una vez:

  ```bash
  cp -R tools/motor/remote_script/GFMotor "$HOME/Music/Ableton/User Library/Remote Scripts/"
  ```

  Reinicia Live y elige `GFMotor` en Preferencias › Link, Tempo & MIDI › Control Surface, sin entrada ni salida MIDI. Live solo lee el script al arrancar: tras cada cambio hay que reinstalarlo y reiniciar.
- **Tests:** `uv run pytest`. Usan un Live simulado y no tocan tu set.
