# DX7 6-OP: validación del cambio

2026-11-10. Base `006684d`, rama `codex/dx7-6op`, árbol con cambios sin commit. Sin red, flash ni push.
Modelo, decisiones, APIs y límites: [dx7-6op.md](dx7-6op.md).

## Archivos y procedencia

| Área | Archivos principales |
|---|---|
| DSP compartido | `firmware-teensy/src/engines/dx7_6op_core.{h,cpp}`, `dx7_6op_algorithms.cpp` |
| Formatos y controles | `dx7_6op_sysex.{h,cpp}`, `dx7_6op_params.{h,cpp}`: 151 parámetros, bancos ≤8 |
| Adaptación MSFA | `engines/msfa/`: env, pitchenv, lfo, sin, exp2, freqlut, note_tables; LICENSE + NOTICE.md |
| Firmware | `engines/dx7_6op.h` (AudioStream custom), `49-studio-brain.cpp`, `audio_graph.*`, `platformio.ini` |
| Gemelo/VST | `brain-plugin/src/dx7_6op_preview.*`, `dx7_6op_view.h`, `preset_codec.*`, `plugin_processor.cpp` |
| Bench | `brain-plugin/bench/engine_bench.cpp`, CMake del bench y del plugin |
| Pruebas | `firmware-teensy/test/test_dx7_6op/test_main.cpp`; tests de codec/plugin; `brain-plugin/tests/dx7_6op/` |

MSFA **f67d41d313b7dc85f6fb99e79e515cc9d208cfff**, Google, Apache-2.0. Se conserva atribución por
archivo; no se usa Dexed ni se añaden patches ROM Yamaha. Los patches del oracle y del bench son propios.
El oracle compila fuentes originales en un ejecutable de prueba separado; sustituye sólo `synth.h`
por un shim portable que selecciona el kernel **escalar** original. No enlaza MSFA original al producto.

`dx7_core.*`, `dx7_preview.*` y `dx7_preset.*` de **FM 4-OP no tienen cambios**. Tampoco se modifican
los specs `00–06`. Los lectores y bytes GFP 2–5 siguen vigentes; el decoder con tag añade GFP 6/id 7.

## Builds y tests repetidos al terminar

| Verificación | Resultado |
|---|---|
| PlatformIO `run -e sketch49 -e consola -e consola_usb` | **3 SUCCESS**, sólo compilación |
| PlatformIO `test -e native` | **166/166** en 13 suites, incluidos FM 4-OP y los 9 tests nuevos |
| CMake limpio, `GF_BUILD_VST3=OFF` | Todas las bibliotecas y bench compilan; **16/16 CTest** |
| CMake con JUCE local **8.0.3**, VST3 y smoke | **17/17 CTest**, VST3 compila |
| UndefinedBehaviorSanitizer, `-fno-sanitize-recover=all` | Core/contratos pasan, sin diagnóstico |
| `git diff --check` | Pasa |

El nuevo test nativo valida las 32 rutas y sus extremos de feedback, tiempos de las cuatro etapas
contra valores congelados del MSFA original (rates 30/60/90/99), scaling lineal/exponencial y signo,
VCED↔VMEM, framing/checksum, GFP, cobertura de metadatos, notas/velocity extremas, 8–192 kHz,
independencia de instancia/bloque, ratio/fixed, transpose, velocity, sustain y edición en vivo.
El oracle también compara los valores del EG original y portado tick a tick a 44,1 kHz.

El smoke del plugin cubre import/export GFP 6, VCED, estado DAW con audio idéntico al restaurar,
editor de ocho controles, cambio de banco y escritura de parámetros DAW. Conserva los tests anteriores.
Se aisló su archivo de preferencias en un temporal único: la ruta fija anterior podía reutilizar
metadatos del navegador entre ejecuciones. El aislamiento evita que las ejecuciones compartan el mismo archivo de prueba.

PlatformIO no podía escribir su caché global. Se usó `PLATFORMIO_CORE_DIR=/tmp/gf-dx7-pio`, con copias
locales de las plataformas y enlaces de lectura a los paquetes ya instalados. Se ejecutó siempre
`~/.platformio/penv/bin/pio`, sin compilación ARM manual ni descargas.

Memoria final del linker (bytes, **no** medición de máximo de stack):

| Entorno | Flash código | RAM1 variables | RAM1 libre local | RAM2 variables |
|---|---:|---:|---:|---:|
| sketch49 | 292804 | 199072 | 30304 | 187744 |
| consola | 293764 | 199328 | 30048 | 187744 |
| consola_usb | 301640 | 203680 | 25696 | 187776 |

## Comparación golden

**42 renders:** 7 patches propios × notas MIDI 48/60/84 × 44100/48000 Hz; velocity 100, duración
1,5 s, key-up a 1 s. Mismo headroom de 0,125 en ambos motores. RMS/norma de error sobre toda la nota;
espectro de magnitud con ventana Hann de 8192 muestras desde 0,4 s (tramo sostenido, no ataque/release).
Resultados por nota: [CSV](../../brain-plugin/tests/dx7_6op/msfa-comparison.csv).

| Caso | 44,1 kHz: diferencia frente a MSFA original |
|---|---|
| 0 seno, 1 algoritmo 5, 2 algoritmo 16, 5 pitch EG + LFO | **PCM float idéntico**, ΔRMS 0 dB, error espectral 0 |
| 3 algoritmo 4, lazo multioperador | ΔRMS −0,003737…−0,000142 dB; error espectral ≤0,004081 % |
| 4 algoritmo 6, lazo multioperador | ΔRMS −0,065812…−0,041576 dB; error espectral ≤0,206546 % |
| 6 AMD/AMS 3, AMD 99 | ΔRMS −8,042804…−7,886426 dB; MSFA **no implementa** esa modulación |

Los algoritmos 4/6 difieren intencionalmente porque MSFA omite sus lazos multioperador. La diferencia
con MSFA no acredita por sí sola fidelidad contra un DX7 físico.

A **48 kHz**, los casos comunes 0/1/2/5 difieren +0,046938…+0,133457 dB RMS, error de forma de onda
2,4–14,1 % y espectro sostenido idéntico en esos fixtures. MSFA original conserva el incremento de EG
de amplitud de 44,1 kHz; el port lo escala por 44100/fs para conservar duración en segundos. Los
algoritmos 4/6 mantienen error espectral sostenido ≤0,207119 %. AMD/AMS reduce RMS unos 7,8–8,0 dB;
es una implementación nueva aproximada pendiente de calibración física, no un resultado de igualdad.

Los `.f32` y WAV propios se generan en el directorio de build, sin incorporarlos al repo.

## CPU Mac

Mac **arm64**, AppleClang 17.0.0.17000404, Release **O2**, `-fno-fast-math -ffp-contract=off`,
`-mcpu=apple-m1`, 48 kHz, un hilo, FTZ verificado. Cada caso: 2 s de audio de calentamiento + 20 s
medidos; validación/WAV/hash fuera de la región cronometrada. No es un host de audio en tiempo real.

| 6 voces, todos los operadores | Bloque | Media de un núcleo | p99 | Máximo | Fuera de plazo |
|---|---:|---:|---:|---:|---:|
| Seco | 128 | 0,668 % | 1,427 % | 4,767 % | 0 |
| Con FX | 128 | 1,265 % | 2,289 % | 4,964 % | 0 |
| Seco | 256 | 0,657 % | 1,225 % | 4,234 % | 0 |
| Con FX | 256 | 1,270 % | 2,345 % | 7,830 % | 0 |

Matriz completa de 1/4/6 voces, sin NaN/Inf ni clipping: [JSON](../../brain-plugin/tests/dx7_6op/results/mac-bench.json).
Proveniencia: [SHA-256 de fuentes](../../brain-plugin/tests/dx7_6op/results/source-sha256.txt).
Los hashes PCM de 128/256 coinciden, tanto secos como con FX. El microbench del core, sin el bus de FX,
midió **0,500 %**, **17,4 ns/muestra/voz** para seis voces; es una estimación Mac, no ciclos Cortex-M7.
Las estimaciones Teensy (9–27 %) y Pi (2–7 %) del modelo siguen sin medir físicamente.

## Reproducir sin red

Desde la raíz (usar un directorio de build propio):

```sh
cmake -S apps/brain-plugin -B /tmp/dx7-six \
  -DGF_BUILD_VST3=OFF -DGF_BUILD_ENGINE_BENCH=ON -DCMAKE_BUILD_TYPE=Release \
  -DGF_MSFA_REFERENCE_DIR=/Users/juandavidguerrerocastro/GROOVEFORGE/refs/msfa/app/src/main/jni
cmake --build /tmp/dx7-six --parallel 4
ctest --test-dir /tmp/dx7-six --output-on-failure
/tmp/dx7-six/gf_engine_bench --engine dx7_6op --seconds 20 --threads 1 \
  --json /tmp/dx7-six-results/results.json
python3 apps/brain-plugin/tests/dx7_6op/analyze.py /tmp/dx7-six/dx7-6op-golden \
  --csv /tmp/dx7-six-results/spectra.csv --wav
```

Para VST3: `GF_BUILD_VST3=ON`, `GF_JUCE_DIR` al JUCE 8.0.3 local de GroovePilot. Para firmware:
`cd apps/firmware-teensy`, `~/.platformio/penv/bin/pio run -e sketch49 -e consola -e consola_usb` y
`~/.platformio/penv/bin/pio test -e native`. No añadir `upload`.
En Pi, `tools/gemelo-pi/run_on_pi.sh /ruta/resultados 20` incluye `dx7_6op` automáticamente (O2/O3).

## Límites y entrega Git

Implementados: seis operadores, rutas/feedback, curvas EG/output/scaling/velocity de MSFA,
ratio/fixed/coarse/fine/detune, pitch EG, LFO/seis ondas/delay/sync/PMD/PMS y AMD/AMS, transpose,
operator enable, SysEx y GFP. Edición conserva voces y EG; no añade ADSR/filtro propio al core.

**Pendientes físicos:** CPU/stack/latencia en Teensy, CPU/xruns reales en Pi, calibración AMD/AMS y
feedback 4/6 con hardware. No se emula DAC/ruido del DX7; hay seis voces y no 16; OSC SYNC off conserva
fase de ranura, sin simular el reloj libre mientras está inactiva. Se heredan aproximaciones de detune
y cuantización de MSFA. El Studio conserva sus seis engines/bancos previos; 6-OP usa selección serie
y GFP independiente. No se declara fidelidad bit a bit al Yamaha.

Contradicciones señaladas en el modelo: `01` §4.1 / `AudioSynthWaveformPWM`, ADR-005 vs ADR-008 y
«feedback siempre en OP6» frente a las rutas originales. No se editaron specs inmutables.

Git rechazó dos intentos de commit del documento con **`.git/index.lock: Operation not permitted`**.
Todo queda sin commit, como autoriza la tarea; no se intentó eludir el sandbox. Secuencia lógica prevista:

1. `docs: define DX7 six-operator model and integration contract`
2. `feat(dsp): add portable DX7 six-operator core and SysEx codec`
3. `feat(teensy): select DX7 six-operator engine alongside FM four-op`
4. `feat(preview): expose DX7 six-op presets, parameter banks and benchmark`
5. `test(dx7): compare MSFA renders and document validation`

Cada mensaje termina con una línea en blanco y:

```text
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
```
