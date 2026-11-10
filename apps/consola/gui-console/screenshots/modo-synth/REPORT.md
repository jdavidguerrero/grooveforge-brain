# SYNTH — entrega de escritorio

2026-11-10 · checkout `grooveforge-codex-usb`, rama `codex/modo-synth`, base `7f106b0`.
Sin cambios a otros checkouts, a los specs 00–06 ni al doc19. Sin push.

## Decisiones y archivos

- [Spec21](../../../../docs/consola/21-modo-synth.md) escrita antes del código.
- `synth_surface.py`: decodificación de `superficie.py`; notas inmediatas, snapshot de
  parte/escala, releases al destino original, referencias entre fuentes, STOP/cierre/
  pérdida de entrada; controles de host por `SurfaceBatcher`. MIDI preferido cuando
  está expuesto; fallback socket independiente anunciado, ACK fuera del frame.
- `scale_keyboard.py`, `screens/SynthScale.qml`, `screens/ModeSelect.qml`: tercer modo,
  teclado in-key 4×8, raíz/escala/octava; 32 atajos físicos Mac, sin tocar notas en pantalla.
- `touch_controller.py`, `components/TouchTarget.qml` y los componentes/pantallas:
  callback común, foco por SHIFT+jog/clic, listas con drag/flick, foco de knobs,
  bool/tabs/algoritmos/bancos/partes/teclas de texto; modales excluyen controles detrás.
  Los servicios ausentes del catálogo legacy abren selección/detalle local rotulado.
- `main.py`: opciones de superficie/MIDI/CDC, fake y modo SYNTH. `surface_batch.py` y
  `synth_state.py`: integración sin llevar las notas al frame.
- `pi/run_synth.sh`: runner sin instalaciones, reutiliza `run_gui.sh` y la calibración
  real `invertx:rotate=0`, imagen QML90, socket XDG y puerto «GF Consola».
- `tests/test_synth_surface.py`, `tests/test_touch.py`: controles, transportes, lifecycle,
  macros en los ocho engines (dos filtros en HYBRID), tabla hardware y registro completo.

## Pruebas

Resultados: GUI **305 passed, 13 skipped** (62,60 s); tools/consola **450 passed** (2,17 s).
Salidas: [gui-pytest.txt](gui-pytest.txt) y [tools-pytest.txt](tools-pytest.txt).
GUI incluye compilación del host real y regresión de metadatos/FM ya existente.
Los skips corresponden al listener Unix restringido y a General Sans local ausente.
Las pruebas de latencia comprueban que el mensaje sale antes de drenar frames/ACK;
no miden latencia audible.

`uv run pytest` encontró primero una caché global bloqueada. Al mover la caché a
`/tmp` y probar offline, faltaba scipy1.18.1 para crear el entorno aislado. No hubo
descargas. La misma suite pasó mediante uv con el intérprete del sistema:

```sh
cd tools/consola
UV_CACHE_DIR=/tmp/gf-modo-synth-uv-cache uv run --no-project \
  --python /Library/Frameworks/Python.framework/Versions/3.14/bin/python3 python3 -m pytest -q
```

`bash -n` y `git diff --check`: sin errores. Un intérprete sustituto del runner
verificó argumentos y entorno sin abrir Qt/DRM: EGLFS, `invertx:rotate=0`,
`--rotate 90 --rotation-method qml`, socket XDG y MIDI GF Consola.

## Capturas y límites

[Galería](index.html), [manifest](manifest.json): seis PNG 1280×720, fake host de
escritorio, sin warnings QML. Reproducir con `tools/synth_mode_review.py` desde GUI.
ModeSelect/PARTS/FILTER/SCALE inspeccionadas visualmente; no hay audio en estas capturas.

Pendiente físico: Pi/PipeWire, latencia MIDI→sonido, touch real, xruns y compuerta
ADR-007. El formato LED de salida se emite, pero firmware18 no tiene su receptor;
no se afirma que los pads físicos cambien de color. MASTER usa el nodo de sala
`wpctl`; no existe `@GF MASTER`. HP/IN reutilizan CDC; IN respeta la dirección y
cuantización local del firmware para que ambos controles sean idempotentes.

Preguntas para Juan: FSR batería/acordes; formato/receptor LED; nodo fijo de sala;
**drag-to-edit en knobs** (propuesto, no implementado).

Git: el intento de commit del spec antes del código falló creando `.git/index.lock`
con `Operation not permitted`. El segundo intento de staging volvió a fallar por la misma restricción.
Documento y código quedan **sin commit**, sobre `7f106b0`. Mensajes previstos con el trailer solicitado:

```text
docs: define SYNTH mode and auxiliary touch contract

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>

feat: connect SYNTH surface and enable auxiliary touch across console

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
```
