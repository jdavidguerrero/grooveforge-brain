# Referencia previa del Prophet

Este harness ejecuta el código actual de `Prophet5`, su oscilador, ladder y VCA, con un sustituto **de pruebas** de
AudioStream y AudioMixer4. No es el núcleo portable ni representa todavía el patch completo de Studio, sus efectos,
el codec o la salida física. Sus bloques de 128 muestras y reloj determinista permiten comparar el refactor con el
comportamiento digital anterior; el jitter del `loop()` del hardware no está modelado.

## Generar y escuchar

Desde la raíz de la rama, con Python 3 y compilador C++17:

```sh
python3 tools/gemelo-baseline/run.py --out /tmp/gf-prophet-reference
```

La carpeta debe estar vacía. Se generan seis `.raw` PCM16 mono y seis WAV a **44.100 Hz**, más `manifest.json`.
Cada caso se ejecuta dos veces y se verifica que sea reproducible y tenga ataque/release; el script primero prueba
mezcla Q16, saturación y copia de bloques compartidos. El manifiesto guarda hashes de fuentes y resultados, compilador,
arquitectura y commit. La referencia guardada está en `apps/firmware-teensy/test/golden/prophet5/`.

```sh
python3 tools/gemelo-baseline/run.py --out /tmp/gf-prophet-comparison \
  --compare apps/firmware-teensy/test/golden/prophet5
```

La comparación comprueba cinco casos con RMS relativo ≤−80 dB y error pico ≤−60 dBFS. Para resonancia exige por ahora
igualdad byte a byte y falla si hay diferencias; falta implementar su comparación por envolvente/espectro según el
spec. No se declara completa la puerta Prophet con este script. El siguiente paso es conectar el núcleo extraído a un
runner que emita la misma secuencia y comparar ambos.

Los casos son acorde saw, pulse con PWM, POLY-MOD, glide, alta resonancia y robo de voz. Son parámetros programados
del engine, no importaciones GFP. La semilla es la del engine actual y los eventos se sitúan en bordes de bloque.
Para reproducir exactamente los hashes usar el mismo compilador/arquitectura indicados por el manifiesto; otras
plataformas se comparan numéricamente. No usar este shim para el VST ni varias instancias simultáneas.

`shim/audio.cpp` reproduce la semántica escalar relevante de `Audio/mixer.cpp` de PJRC; ver `PJRC-LICENSE.txt`.

## Referencia Juno-106

```sh
python3 tools/gemelo-baseline/run_juno.py --out /tmp/gf-juno-reference
```

Genera diez WAV estéreo y PCM16 con manifiesto a 44.100 Hz. Los siete casos originales se conservan en
`apps/firmware-teensy/test/golden/juno106/`; la escena adicional de ruido se conserva en
`apps/firmware-teensy/test/golden/juno106-noise/` y los dos casos recientes en `apps/firmware-teensy/test/golden/juno106-extra/`. El shim usa una implementación escalar PJRC de ruido con semillas
del harness, no comprueba las semillas ni la fase de ruido de una unidad física. Esta referencia no valida FX Studio,
ejecución en DAW ni fidelidad del hardware. Se puede verificar cualquiera de las dos referencias:

```sh
python3 tools/gemelo-baseline/run_juno.py --out /tmp/gf-juno-check \
  --compare apps/firmware-teensy/test/golden/juno106
``` Véase
`apps/docs/sprints/gemelo-juno-referencia.md`.

## Comparar el piloto Juno con el grafo de firmware

```sh
cmake --build /tmp/gf-brain-vst-build --target gf_juno_preview_reference
python3 tools/gemelo-baseline/compare_juno_preview.py \
  --renderer /tmp/gf-brain-vst-build/gf_juno_preview_reference --out /tmp/gf-juno-preview-check
```

El informe y los WAV comparan ocho escenas deterministas de `JunoPreview` con el shim, sin alinear señal ni aplicar
umbrales de aprobación. No incluye el wrapper JUCE, la cadena completa de FX ni el hardware físico. La correlación
alta no descarta errores transitorios: revisar también la posición y el valor del pico.

Para aislar el efecto de los ticks adelantados del shim en la envolvente de filtro y el glide:

```sh
python3 tools/gemelo-baseline/experiment_juno_block_clock.py --out /tmp/gf-juno-block-experiment
```

El experimento compila una copia temporal con una agenda distinta; nunca modifica ni representa el VST. Su informe
incluye las limitaciones y hashes de la variante. No es una prueba con el Brain físico.
