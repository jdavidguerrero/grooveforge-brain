# Consola: GUI del synth (31–45)

2026-11-10 · ADR-007/008 · contrato [14](14-synth-host-contrato.md) sin modificaciones.

## Alcance y transporte

PARTS 31/31b; plantilla FILTER 32/32b/32c; OSC 33; ENV 34/34b;
LFO 35; FM 36/36b (4 y 6 operadores); GROOVE 37; FX 38; MACROS 39;
HYBRID 40/41; PRESETS 42/42b; SETTINGS 43/44/45. Se conservan los IDs
legacy del registro; nombres Synth* identifican las pantallas nuevas.
Chrome 56/568/96, columnas x=8+i·160, ancho 152; texto inglés y fuentes locales.

`synth_client.py` usa QLocalSocket y JSON UTF-8 por líneas, IDs correlacionados,
timeout de 500 ms y reconexión exponencial limitada. Los callbacks encolan mensajes;
el mismo `afterAnimating` que drena SurfaceBatcher aplica el lote synth una vez
por cuadro. No hay audio ni DSP en Qt. Al conectar: hello, subscribe, get_state;
después list_params y get_params por parte. El audio sigue si se cierra la GUI.

`main.py --synth-socket /ruta/gf-synth.sock --screen SynthParts` conecta al host.
`--fake-synth` lanza el fake en otro proceso; `--preview --screen SynthDx7Focus`
usa un snapshot local explícito. F3=Parts, F4=Audio, PageUp/Down=página;
clic 3=banco, SHIFT=fino. Girar no dispara la soft key. Ediciones del mismo
parámetro se acumulan y envían una vez por cuadro; Guardar vacía primero la cola.

## Metadatos y DX7 6-OP real

`list_params(engine)` es la única fuente de controles, nombres, unidades, rangos,
defaults, opciones y bancos. La GUI no completa parámetros ausentes. Los encoders
usan `step`; SHIFT multiplica por 0,1 solamente continuos/bipolares. Los enums
normalizados del bus también respetan su step fraccionario: índice de etiqueta =
`round((value-min)/step)`. Huecos de bancos cortos quedan deshabilitados.

El host registra **dx7_6op**, distinto de **dx7** (FM 4-OP). Los 151 descriptores de
`firmware-teensy/src/engines/dx7_6op_params.cpp` se comparten con el editor JUCE.
IDs reales: `op1.r1`, `op6.l4`, `op3.coarse`, `algorithm`, `feedback`, `pitch.r1`,
`lfo.wave`; nunca se envían los antiguos IDs provisionales. Agrupación:

| Página | Bancos (máximo ocho controles) |
|---|---|
| OPS | Los seis `opN.level` |
| OP 1 … OP 6 | 1: mode/coarse/fine/detune/velocity/ams/on (7); 2: R1–R4/L1–L4 (8); 3: breakpoint/left_depth/right_depth/left_curve/right_curve/rate_scaling (6) |
| ALGO/FB | algorithm, feedback, osc_sync, transpose (4) |
| PITCH EG | R1–R4/L1–L4 (8) |
| LFO | speed/delay/pmd/amd/sync/wave/pms (7) |

El host añade GLOBAL (cutoff/resonance del filtro de bus), GROOVE, MACROS y cinco
páginas FX, para **207 controles** en total; esos 56 controles no forman parte
del VCED Yamaha ni de los 151 del editor 6-OP del VST.

`synth_state.py` vincula los IDs semánticos conocidos únicamente cuando existen
en los metadatos recibidos. Cuenta los operadores a partir de esos descriptores.
`FmPanel.qml` muestra **seis columnas simultáneas**, con frecuencia ratio/fixed,
nivel y enable reales; pulsar columna o nodo abre el operador. Tres botones
seleccionan sus bancos. `FmEnvelope.qml` dibuja las cuatro etapas desde R/L:
sustain en L3 y release a L4. La anchura temporal es esquemática, no milisegundos
ni osciloscopio. `KeyScaling.qml` dibuja los 128 valores de la fórmula entera del
core (breakpoint+17, signo, curva y profundidad por lado); una prueba compara
cuatro curvas contra valores generados por el C++ compilado.

`algorithm` usa valores **0–31**, etiquetas **1–32**. Los botones convierten a
las unidades del descriptor. `fm_algorithms.py` decodifica los flags exportados
del core, incluyendo retornos multioperador en 4/6. El diagrama representa
modulador→portadora, salidas y extremos de feedback reales.

FM 4-OP conserva sus IDs reales: `osc.0.2` algoritmo **1–8**, `osc.0.3` feedback,
`osc.N.0/1` ratio/nivel, `osc.N.4…7` ADSR y `mod.0.N` wave/detune. El host los
agrupa en OPS/OP 1…4/ALGO-FB; su DSP y codecs GFP 2–5 no cambian. Los demás
motores usan sus IDs de host, incluidos `a.`/`b.` y `layer.morph` para HYBRID.
Los gráficos genéricos muestran valores del banco recibido cuando no hay un
binding especializado; no inventan controles ni curvas de oscilador por engine.

## Fake y prevención de divergencias

`synth_catalog.py` ya no contiene tablas de parámetros escritas en Python.
`fake_synth_host.py` carga **synth-metadata.json**, exportado por el ejecutable
C++ `gf-export-metadata`, que usa el mismo `params_json()` que `list_params`.
Incluye los ocho engines, parámetros enteros y flags de algoritmos. Se elige una
exportación compilada en lugar de parsear C++ con regex: así se resuelven defaults,
macros y agrupaciones exactamente como en el host. El JSON se versiona para usar
la GUI sin toolchain C++ en el Pi; no sustituye list_params en producción.

```sh
cmake -S apps/synth-host -B /tmp/gf-host-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/gf-host-build -j4
cp /tmp/gf-host-build/synth-metadata.json apps/consola/gui-console/synth-metadata.json
ctest --test-dir /tmp/gf-host-build --output-on-failure
python3 -m pytest apps/consola/gui-console/tests -q
```

CTest `metadata_snapshot` falla ante diferencias entre exportación compilada y
snapshot. Pytest vuelve a compilar el host/exportador en un directorio temporal,
compara **todos los metadatos**, las 32 rutas y curvas de scaling, y construye
cada página/banco sobre el host C++ real. Para reutilizar un build verificado:
`GF_SYNTH_HOST_BUILD=/tmp/gf-host-build python3 -m pytest …`.

El fake sirve comandos/eventos y presets en memoria; no genera audio, importa
SysEx ni persiste archivos. El host real descubre `.syx`/`.gfp` en sus directorios:
la GUI usa list_presets/load_preset sin añadir comandos al contrato.

## Estado, límites y validación

Sin respuesta en 500 ms: NOT RESPONDING, últimos valores al 45 %, edición bloqueada,
Retry. LOADING PRESET dura hasta la respuesta; errores conservan el estado anterior.
La respuesta de set_engine actualiza la parte antes de pedir los valores nuevos,
incluso si se vuelve a seleccionar el mismo engine. Los tests esperan IDs del
catálogo nuevo y final de peticiones pendientes, sin depender de sleeps de carga.

AF_UNIX bind está bloqueado en este sandbox. Doce tests de listener/broadcast por
ruta quedan skipped; los round-trips del fake se prueban por pipes. Las pruebas
nuevas usan **QLocalSocket con socketpairs Unix heredados** hacia el host C++:
mismo parser, cliente, cambios al inicio del bloque y audio offline. No certifican
el listener por ruta, PipeWire, latencia ni hardware Pi. Un skip adicional es
General Sans (descarga local ignorada por Git); la tipografía de fallback se prueba.

Capturas 1280×720 del **host real**, sin warnings QML:
[overview](../../consola/gui-console/screenshots/dx7-6op-overview.png) y
[operator focus](../../consola/gui-console/screenshots/dx7-6op-operator-focus.png).
Reproducir con `python3 apps/consola/gui-console/tools/real_synth_review.py
/tmp/gf-host-build/gf-synth-host --screenshots apps/consola/gui-console/screenshots`.

Settings conserva muestras rotuladas para USB/red/disco/temperatura; acciones sin
contrato quedan deshabilitadas. CUE, cambio al próximo compás, favoritos editables,
swap/link de mitades y configuración de sistema siguen fuera del contrato 14.
Las descripciones de mitades HYBRID son muestras de diseño; sólo sus controles de
host se editan. Faltan certificación de rendimiento Pi y pruebas físicas.

Resultado final: **204 passed, 13 skipped** (42,45 s), cero warnings QML. Salida:
[pytest](../../consola/gui-console/screenshots/dx7-wiring-pytest.txt);
[procedencia de capturas](../../consola/gui-console/screenshots/dx7-wiring-manifest.json).
