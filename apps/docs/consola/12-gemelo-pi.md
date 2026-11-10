# Consola: gemelo sin JUCE en el Pi 5

> **Fecha:** 2026-11-09. **Estado:** diseño y benchmark autorizado por Juan; decisión de hardware pendiente de medir en Pi. **Base:** ADR-006, revisión vigente; `produccion/ia-local-gemelo-digital.md` §3; `03-arquitectura-capas.md`. **No modifica** `00–06` ni el firmware.

## 1. Pregunta y decisión condicionada

¿Caben BASS, CHORDS, LEAD y PAD con margen en el Pi 5 (Debian 13, aarch64, Cortex-A76 ×4 a 2,4 GHz)? Si pasan la puerta, Juan propone mover los synths al Pi y usar **una Teensy para superficie, audio I/O y CV**, con **como máximo una parte Brain viva**. El benchmark solo decide si vale la pena construir el host; no demuestra latencia ni autoriza quitar la segunda Teensy todavía.

Se añade `gf_engine_bench` en `apps/brain-plugin/bench/`, junto a las bibliotecas que realmente usa el VST. `tools/gemelo-pi/` contiene solo la ejecución en el Pi. Así no se duplica DSP ni se convierte el shim histórico en un host. Un compilador C++17 y CMake bastan; sin JUCE, red, Python ni dispositivos de audio para renderizar.

## 2. Código reutilizado y procedencia

Rutas relativas a `apps/brain-plugin/`, sin cambiar su comportamiento:

| Biblioteca | Archivos compilados existentes |
|---|---|
| `gf_prophet_preview` | `src/prophet_preview.cpp`, `../firmware-teensy/src/engines/prophet_osc_core.cpp`, `../firmware-teensy/src/dsp/lfo.cpp` |
| `gf_prophet_preset` | `src/prophet_preset.cpp`, `src/dx7_preview.cpp`, `src/dx7_preset.cpp`, `src/juno_preview.cpp`, `src/juno_preset.cpp`, `src/ob6_preview.cpp`, `src/ob6_preset.cpp`, `src/model_d_preview.cpp`, `src/model_d_preset.cpp`, `src/arp2600_preview.cpp`, `src/arp2600_preset.cpp`, `src/hybrid_preview.cpp`, `../firmware-teensy/src/engines/dx7_core.cpp` |
| `gf_preset_codec` | `src/preset_codec.cpp`, `src/hybrid_codec.cpp`, `src/hybrid_router.cpp`, `../firmware-teensy/src/ui/layer_bank.cpp`, `../firmware-teensy/src/ui/preset_bank.cpp`, `../firmware-teensy/src/ui/page_params.cpp` |

También sus headers homónimos, `src/phase_chorus_preview.h`, `src/plate_reverb_preview.h`, `src/modal_reverb_preview.h`, `src/pjrc_white_noise.h`, `src/model_d_noise.h`, los headers de `../firmware-teensy/src/dsp/`: `analog_env.h`, `arp_core.h`, `chord_memory.h`, `drive_core.h`, `fast_math.h`, `juno_chorus_core.h`, `ladder_stage_core.h`, `lfo.h`, `modal_modes.h`, `note_stack.h`, `output_stage_core.h`, `phaser_core.h`, `pingpong_delay_core.h`, `polyblep_core.h`, `prophet_control_core.h`, `pump.h`, `sem_svf_core.h`, `tape_core.h`, `tape_modulation.h`, `voice_spread.h`, `zdf_ladder.h`; `engines/prophet_patch_apply.h`, `ui/{bank_file_storage,groove_macros,layers,preset_bank,layer_bank,page_params,studio_ui}.h` y `../bridge-protocol/include/{gf_params,protocol}.h`. El manifiesto SHA-256 acompaña cada build (también cubre fuentes no utilizadas del árbol DSP/UI para identificar el checkout); no hay una copia del firmware.

**Custom:** previews, ruteo, codecs, núcleos DSP y banco de presets son código del proyecto. El DX7 aquí es el motor **de cuatro operadores**, no el Yamaha original de seis. Las rutas de voz siguen parcialmente en cada `*_preview.cpp`: esto no completa la extracción del gemelo compartido del ADR.

**PJRC oficial frente a adaptación:** `AudioStream`, I2S/USB, SGTL5000 y agenda por bloques del firmware son Teensy Audio Library oficial y **no se enlazan aquí**. `pjrc_white_noise.h` y `plate_reverb_preview.h` son adaptaciones portables de algoritmos PJRC (ver avisos en esos archivos); no equivalen a ejecutar los objetos oficiales con su aritmética y agenda. `tools/gemelo-baseline/shim/` es una referencia de pruebas separada a 44,1 kHz; no participa en este target.

## 3. Portabilidad y frecuencia

- Los tres targets estáticos usan C++ y libm, sin dependencia Apple, x86, Arduino ni JUCE en su ruta de build. El wrapper `src/plugin_processor.cpp` y la GUI quedan fuera. El arnés reproduce el bus `ProphetPreview` externo usado por el wrapper para DX7/Juno/OB-6/Model D/ARP; HYBRID tiene su propio bus compartido. Incluye un bloqueador DC de 5 Hz como el wrapper.
- **48.000 Hz** en todo el grafo del Pi, `prepare(48000)` por instancia; bloques 128 (2,667 ms) y 256 (5,333 ms). Firmware Teensy 4.x: **44.100 Hz nominales**, sin cambiarlo. No basta cambiar una constante: envolventes, filtros, delays, LFO y buffers reciben la frecuencia. Los ticks heredados que escalan 128/44100 se mantienen tal como están. No se compara el PCM de 48 kHz con el golden del shim a 44,1 kHz.
- Audio USB/cue de la Teensy seguirá en otro reloj. El host futuro necesitará resampling asíncrono o elegir un único reloj; este benchmark no lo resuelve.
- Release usa **`-O2 -fno-fast-math -ffp-contract=off`**, también para las bibliotecas que procesan audio. `GF_BENCH_OPT=O3` permite medir `-O3` explícitamente, sin cambiar los otros dos flags. Con `GF_BUILD_ENGINE_BENCH=ON` esta política también se aplica a las bibliotecas compartidas por el plugin (el benchmark se activa por defecto al deshabilitar VST3 y queda apagado por defecto en una configuración VST3 nueva); `OFF` conserva sus flags anteriores. No se usa `-ffast-math`: permite reasociar operaciones y asumir ausencia de NaN/Inf, cambia el sonido y puede invalidar la detección de fallos. `dsp/fast_math.h` es una aproximación explícita custom, distinta de ese flag.
- Linux aarch64: probar con CMake `-mcpu=cortex-a76`; fallback `-march=armv8.2-a+fp16+dotprod` si lo admite el compilador. Mac arm64: `-mcpu=apple-m1` si se admite; x86: flags comunes sin `-march=native`. Registrar selección, compilador, sistema, arquitectura y revisión en JSON. No trasladar instrucciones Apple al Pi.
- Denormales: en el hilo de render, activar **FPCR.FZ (bit 24)** en aarch64; en x86 SSE2, **MXCSR.FTZ (bit 15) y DAZ (bit 6)**. Se conserva y restaura el registro previo. FZ evita colas subnormales con costo impredecible; no habilita FZ16 porque el DSP es float32. Verificar con lectura del registro y una multiplicación subnormal. Todo hilo del host real debe configurar lo mismo.
- Semillas fijas y eventos por muestra permiten igualdad entre dos ejecuciones del **mismo binario**. Mac Clang/libSystem frente a Pi GCC/glibc puede cambiar `sin`, `pow`, redondeos y trayectorias de filtros recursivos. Desactivar FMA reduce una fuente de diferencias, no garantiza igualdad entre toolchains. SHA distinto entre máquinas es diagnóstico: comparar WAV alineados (RMS de error, pico, espectro y escucha), no declarar fidelidad por hash ni exigir igualdad binaria entre compiladores.

## 4. Método reproducible

Un hilo, reloj monotónico `std::chrono::steady_clock`, sin dormir entre bloques: mide rendimiento de render offline y costo de pared del trabajo DSP, **no uso del proceso muestreado por el SO**. Antes: construir y cargar fuera del tiempo medido, calentar 2 s de audio; después medir **al menos 20 s de audio por caso** (no 20 s de reloj). Se permite `--seconds` menor para diagnóstico, pero no pasa la puerta. No escribir WAV, JSON, logs ni calcular SHA dentro del intervalo cronometrado. Registrar cada bloque, incluidos eventos MIDI y mezcla; reservar buffers antes.

Matriz: Prophet-5 (1/4/5), DX7 (1/4/6), Juno (1/4/6), OB-6 (1/4/6), Model D (1), ARP 2600 (1/4), HYBRID (1/4/6 notas enviadas a ambas mitades, hasta once voces: Prophet A limita a cinco y roba una voz con seis notas; Juno B toca las seis). Factory preset índice 0 de `studio::factory_preset(engine, 0)`, con nombre registrado. HYBRID: factory layer índice **7, RETRO BRASS** (A Prophet-5, B Juno, MORPH 50 %, ganancias de fábrica); ambas mitades realmente se renderizan. Nombres de índice 0: **INIT** en Prophet/Juno/OB-6/Model D/ARP; **E.PIANO** en DX7. No se extrapola a las ocho capas ni a todo preset posible.

Dos variantes por caso: **dry**, sin SYNTH FX/GROOVE y sin chorus interno del Juno; **wet**, DRIVE + TAPE (wow/flutter) + PHASER + DELAY + PLATE y GROOVE/PUMP activos, valores fijos documentados en el arnés. Se conserva el patch de voz de fábrica, normalizando UNISON a uno y desactivando ARP/CHORD para que no alteren la polifonía solicitada. Release de voz/filtro y operadores DX7 limitado a 100 ms; patrón de acordes sostenidos 1,5 s y silencio 0,5 s, transpuesto cada 2 s, velocity fija por nota, pitch bend determinista. Se informa **notas MIDI sostenidas**, no un contador interno de envelopes. Wet incluye el chorus interno del preset Juno. Valores wet normalizados de SYNTH FX (`p0/p1/p2`, mix): DRIVE `.35/.55/0`, `.20`; TAPE `.30/.20/.15`, `.20`; PHASER `.50/.30/1`, `.25`; DELAY `.50/.35/.55`, `.20`; PLATE `.55/1/.50`, `.20`. Se pasan por los mapeos del preset, no son todos unidades físicas. PUMP: depth 35 %, shape/rate índice 0, offset 0 ms, release 180 ms. ARP/CHORD, MODAL y CHORUS alternativo no son un peor caso cubierto por esta matriz: deben medirse antes de congelar producto.

Escenario **4 partes**, instancias independientes en el mismo hilo y un solo cronómetro por bloque, sumadas con ganancia 0,25: BASS Model D ×1, CHORDS Juno ×6, LEAD OB-6 ×4, PAD HYBRID con seis notas (A ×5, B ×6). Dry y wet, ambos bloques. En el JSON, `held_notes=0` del caso `four-part` es un marcador de mezcla: las cantidades reales están en `parts` (1, 6, 4, 6). No sumar p99 de casos separados: se mide el bloque conjunto. `--threads 1` es obligatorio; no se implementa modo paralelo en esta entrega.

Métricas: `render_wall / audio_seconds` = RTF (menor es mejor); `% de un núcleo medio = 100 × RTF`; p99 por rango más cercano y máximo de `block_wall / (block_frames/48000) ×100`; también bloques sobre el deadline. No dividir por cuatro núcleos ni inferir que un núcleo A76 equivale a uno Apple. El rendimiento medio por sí solo no garantiza deadlines.

Un render nuevo de 3 s por engine, variante y bloque a máxima polifonía genera WAV estéreo PCM16 LE y SHA-256 **solo de sus bytes PCM**, con ganancia fija, pico/RMS y contador de no finitos. Misma duración, semillas y eventos siempre; independiente de `--seconds`. Archivo/hash no participa en el tiempo DSP. El escenario combinado también se puede inspeccionar en WAV. Se rechazan NaN/Inf y audio completamente nulo. Clipping se informa; no se oculta con normalización automática.

## 5. Puerta propuesta

**En el Pi**, ambos bloques, wet y dry, en **tres corridas consecutivas** con enfriamiento activo y gobernador/frecuencia registrados:

| Condición | Umbral |
|---|---|
| Escenario conjunto de 4 partes, un hilo | Media ≤50 % de un núcleo; p99 ≤70 %; máximo ≤90 %; **cero bloques >100 %** |
| Audio y repetibilidad local | Sin NaN/Inf, sin silencio inesperado; mismo SHA por fixture entre corridas |
| Validez de cada corrida | ≥20 s de audio por caso después de warm-up; sin throttling durante la prueba |

Si falla: no decidir todavía el cambio a una Teensy; revisar el caso que domina y repetir con una carga musical pactada. Si pasa: candidato para **host real**, no aprobación final de hardware. La reserva de medio núcleo es deliberada para sampler, mezcla, MIDI e I/O; no es una garantía de que esas otras cargas caben. Antes de cambiar placa: 30 minutos sin xruns con sampler/mezclador/GUI activos, latencia MIDI→salida medida y cue sin clics; decidir explícitamente el contrato de latencia.

## 6. Ejecución y evidencia

Desde la raíz del clon, en el Pi (CMake ya instalado):

```sh
bash tools/gemelo-pi/run_on_pi.sh
```

El script construye en `/tmp`, corre O2 y O3 tres veces cada uno, guarda JSON, tabla, WAV, hashes, flags y diagnóstico térmico antes/después en una carpeta de resultados. No instala ni descarga nada. `vcgencmd` ausente se registra como diagnóstico no disponible, no se inventa temperatura. Se leen también gobernador, frecuencia y temperatura de sysfs cuando están disponibles. Las lecturas antes/después no son telemetría continua; bits históricos de throttling ya activos no permiten excluir un episodio nuevo del mismo tipo. No se declara aprobada la térmica automáticamente. El script compara los hashes de las tres corridas por optimización; las seis matrices también permiten observar variación de timings. No compara O2 contra O3 como requisito binario. Uso manual:

```sh
cmake -S apps/brain-plugin -B /tmp/gf-gemelo-o2 -DGF_BUILD_VST3=OFF -DCMAKE_BUILD_TYPE=Release -DGF_BENCH_OPT=O2
cmake --build /tmp/gf-gemelo-o2 --parallel 4
ctest --test-dir /tmp/gf-gemelo-o2 --output-on-failure
/tmp/gf-gemelo-o2/gf_engine_bench --seconds 20 --threads 1 --json /tmp/gf-gemelo-results/results.json
```

Opciones: `--block 128|256` (omitir = ambos), `--engine prophet|dx7|juno|ob6|model-d|arp2600|hybrid|four-part` (omitir = todos). Los WAV se guardan junto al JSON. El JSON completo del Mac y la tabla se enlazan con la verificación al final; **todavía no hay resultado Pi**. El 0,8–3,6 % previo en M1 Pro (`ia-local-gemelo-digital.md` §3) usa otro arnés/presets y no es esta baseline.

## 7. Siguiente paso: diseño del host, sin implementarlo

Un proceso **C++**, cuatro instancias preasignadas, MIDI por canales (1 BASS, 2 CHORDS, 3 LEAD, 4 PAD), cola con timestamps y eventos en offsets de muestra; carga de presets y disco fuera del callback. **Nada de Python en la ruta de audio**. Aplicar FTZ en cada hilo DSP; no crear hilos, reservar memoria, bloquear ni escribir logs desde el callback.

Primera opción: cliente **JACK sobre PipeWire**, integra la mezcla y el ruteo existentes del Pi; fijar 48 kHz y quantum 128, admitir 256 para diagnóstico. Cliente PipeWire nativo ofrece control del grafo con mayor acoplamiento a esa API. **ALSA directo** reduce capas y permite un dispositivo exclusivo, pero obliga a implementar ruteo, recuperación, MIDI y convivencia con sala/cue; no elimina el problema de dos relojes. Medir ambas rutas si los xruns o latencia lo piden; no sumar buffers a ciegas.

## 8. Contradicciones y límites abiertos

- `03-arquitectura-capas.md` §§1,4 exige **dos Teensy**, synths en A y «nada del tiempo real depende de Linux». La decisión condicionada de Juan del 2026-11-09 cambia esas premisas si pasa la prueba. Se señala, no se reescribe esa arquitectura aquí.
- `01-architecture.md` §5.2 exige audio <1 ms determinístico y USB MIDI→audio <5 ms p95. Un quantum 128/48 kHz ya dura 2,667 ms; 256 dura 5,333 ms sin I/O. Migrar a Linux requiere revisar ese contrato con Juan; CPU baja no lo cumple por sí sola.
- ADR-006 conserva en el historial 44.117,6 Hz y extracción completa futura. Su **revisión vigente** confirma 44.100 Hz; el estado real de IA local §3 confirma rutas propias por preview. Los WAV diagnósticos PCM16 tampoco sustituyen la salida 24 bit prevista para `gf-render`. No se presenta este arnés como `gf-render` completo ni gemelo validado contra hardware.
- **Contexto fuera del working tree:** 06 y 08 se leyeron de las refs locales `origin/codex/teensy-s-e4` y `origin/codex/superficie-pi`; 10 de `codex/usb-audio-eval`, con `git show`, sin integrar sus cambios ni consultar otros clones. **La auditoría Fable (G10/G18/G19, §4.6) no existe en el árbol ni en el historial local de las refs disponibles**; solo está su prompt. Sus conclusiones no se pueden contrastar.
- `10-usb-audio-teensy.md` §2 (ref local citada) conserva la discrepancia entre la frecuencia atribuida a E3 y el framework 1.60; `05` actual y ADR-006 vigente dicen 44.100 nominal. Falta identificar el binario medido y su LRCLK físico. Su §6 también señala que 03 promete continuidad si falla el Pi, pero sala por Audio+/PipeWire ya depende del Pi. Migrar synths aumenta esa dependencia.
- `06`/`08` reservan MIDI USB canal 1 para mensajes crudos de superficie, incluidos CC 16–23 relativos; el host propuesto usa canales 1–4 para **notas musicales en otro puerto lógico**. No reenviar superficie cruda a un synth (colisionaría con macros/CC). El ruteo semántico futuro es contrato pendiente, no parte del benchmark.
- No hay medición física Pi, realtime scheduler, USB, resampling, sampler ni GUI en el benchmark. No se cambia firmware, no se da por resuelta la fidelidad Mac/Teensy ni se confirma la placa con una Teensy.

## Fuentes

Lectura local, sin red en esta entrega: [ADR-006](../adr/ADR-006-plugin-gemelo.md#revisión-vigente-de-juan-2026-10-29-retomar-plugin-con-juce-y-gui-de-escritorio), [estado real](../produccion/ia-local-gemelo-digital.md#3-estado-real), [CMake del plugin](../../brain-plugin/CMakeLists.txt), [README del plugin](../../brain-plugin/README.md), [baseline y shim](../../../tools/gemelo-baseline/README.md), [arquitectura de capas](03-arquitectura-capas.md), [E3 y relojes](05-e3-teensy-a.md); estilo y contexto 06/08/10 leídos con `git show` en las refs locales indicadas en §8 (archivos no integrados en esta rama).

Referencias técnicas primarias para la decisión (enlaces de consulta, no verificados en línea por la restricción sin red): Arm Architecture Reference Manual A-profile, registro [FPCR](https://developer.arm.com/documentation/ddi0601/latest/AArch64-Registers/FPCR--Floating-point-Control-Register); [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html), MXCSR, FTZ/DAZ; [GCC, opciones de optimización](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html) y [AArch64](https://gcc.gnu.org/onlinedocs/gcc/AArch64-Options.html); [PJRC Audio](https://www.pjrc.com/teensy/td_libs_Audio.html); [PipeWire](https://docs.pipewire.org/), [JACK callback](https://jackaudio.org/api/group__ClientCallbacks.html), [ALSA PCM](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm.html); [NIST FIPS 180-4](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf) (SHA-256); C++17 `steady_clock` (reloj monotónico, no reloj de CPU).

## 9. Baseline Mac verificada (2026-11-09)

**MacBook Pro, MacBookPro18,3; Apple M1 Pro, 8 núcleos (6 performance + 2 efficiency), 16 GB; macOS 27.0.1 (26A434), Darwin arm64.** Apple Clang 17.0.0, CMake 3.30.3. Proceso ordinario de escritorio, sin afinidad ni prioridad RT; no se identifica qué tipo de núcleo ejecutó cada bloque. Son porcentajes de **un** núcleo por tiempo de pared, no porcentaje total de los ocho.

Carga final `factory0-retrobrass7-v2`: HYBRID con seis notas y ambos motores saturados. La revisión exploratoria anterior con cinco notas se descartó como baseline. Cada corrida final contiene 76 casos ×20 s = **1.520 s de audio medido**, más warm-up y fixtures. La tabla siguiente es **O2, corrida 1, máxima polifonía**; cada celda wet expresa **media / p99 / máximo**, en %. Los JSON conservan también 1 y 4 notas.

| Engine | Notas | 128 dry media | 128 wet media/p99/máx | 256 dry media | 256 wet media/p99/máx |
|---|---:|---:|---:|---:|---:|
| Prophet-5 | 5 | 6.26 | 4.51 / 13.70 / 1312.33 | 4.42 | 4.18 / 11.95 / 330.83 |
| DX7 (4 operadores) | 6 | 2.62 | 3.04 / 6.57 / 23.85 | 4.33 | 3.24 / 11.55 / 106.99 |
| Juno | 6 | 3.86 | 6.38 / 42.30 / 568.58 | 3.94 | 4.44 / 8.77 / 25.57 |
| OB-6 | 6 | 2.37 | 2.95 / 6.17 / 160.62 | 2.89 | 2.83 / 4.29 / 14.48 |
| Model D | 1 | 0.87 | 1.46 / 2.38 / 7.13 | 0.86 | 1.49 / 2.69 / 5.38 |
| ARP 2600 | 4 | 1.87 | 2.46 / 4.54 / 22.47 | 1.84 | 2.45 / 4.12 / 12.02 |
| HYBRID A5+B6 | 6 | 6.49 | 7.13 / 12.58 / 41.88 | 6.94 | 10.69 / 62.93 / 1018.78 |
| 4 partes | 1+6+4+(5+6) | 13.67 | 19.86 / 88.81 / 1469.43 | 14.69 | 23.69 / 158.68 / 3212.20 |

No se elimina ningún outlier. La puerta incluye máximo y bloques fuera del deadline, así que las medias bajas no bastan para aprobar esta sesión Mac. Las pausas del SO son parte del tiempo de pared observado; no se atribuyen automáticamente al DSP. **No hay conversión M1→A76 ni dictamen Pi.**

**O3, misma carga, corrida 1** (media de un núcleo, %; no es un experimento con afinidad/frecuencia controladas):

| Engine | 128 dry | 128 wet | 256 dry | 256 wet |
|---|---:|---:|---:|---:|
| Prophet-5 ×5 | 2.01 | 2.56 | 1.93 | 2.49 |
| DX7 ×6 | 2.38 | 2.92 | 2.42 | 2.92 |
| Juno ×6 | 2.24 | 2.95 | 2.24 | 2.99 |
| OB-6 ×6 | 2.20 | 2.78 | 2.24 | 2.84 |
| Model D ×1 | 0.87 | 2.13 | 0.85 | 1.44 |
| ARP 2600 ×4 | 1.79 | 2.42 | 1.77 | 2.38 |
| HYBRID A5+B6 | 3.92 | 4.64 | 3.91 | 4.68 |
| 4 partes | 9.45 | 11.95 | 9.30 | 12.30 |

**Escenario conjunto wet, tres repeticiones**. Los rangos muestran mínimo–máximo entre corridas; no sustituyen a los valores por bloque de cada JSON:

| Build / bloque | Media % | p99 % | Máximo % | Bloques fuera de plazo por corrida |
|---|---:|---:|---:|---|
| O2 / 128 | 15.59–19.86 | 21.13–88.81 | 42.35–1469.43 | 67, 0, 8 |
| O2 / 256 | 15.94–23.69 | 24.59–158.68 | 67.99–3212.20 | 68, 4, 0 |
| O3 / 128 | 11.87–11.95 | 17.59–18.36 | 51.78–103.26 | 1, 0, 0 |
| O3 / 256 | 11.99–12.30 | 17.27–20.63 | 56.06–940.46 | 1, 0, 1 |

**Resultado:** ninguna de las seis corridas Mac pasa la puerta provisional completa. Se conserva el fallo por picos; no se escoge la mejor repetición. O3 merece medirse en el Pi por sus resultados observados aquí, pero no se atribuye toda la diferencia al optimizador ni se cambia O2 como baseline por defecto.

**Evidencia íntegra adjunta al documento:**

| Build | JSON de corridas completas | Tablas | Tests / PCM SHA-256 / fuentes |
|---|---|---|---|
| O2 | [1](mediciones/gemelo-pi/mac-o2-run-1.json), [2](mediciones/gemelo-pi/mac-o2-run-2.json), [3](mediciones/gemelo-pi/mac-o2-run-3.json) | [1](mediciones/gemelo-pi/mac-o2-run-1.txt), [2](mediciones/gemelo-pi/mac-o2-run-2.txt), [3](mediciones/gemelo-pi/mac-o2-run-3.txt) | [ctest](mediciones/gemelo-pi/mac-o2-ctest.txt), [PCM](mediciones/gemelo-pi/mac-o2-pcm-sha256.txt), [fuentes](mediciones/gemelo-pi/mac-o2-source-sha256.txt) |
| O3 | [1](mediciones/gemelo-pi/mac-o3-run-1.json), [2](mediciones/gemelo-pi/mac-o3-run-2.json), [3](mediciones/gemelo-pi/mac-o3-run-3.json) | [1](mediciones/gemelo-pi/mac-o3-run-1.txt), [2](mediciones/gemelo-pi/mac-o3-run-2.txt), [3](mediciones/gemelo-pi/mac-o3-run-3.txt) | [ctest](mediciones/gemelo-pi/mac-o3-ctest.txt), [PCM](mediciones/gemelo-pi/mac-o3-pcm-sha256.txt), [fuentes](mediciones/gemelo-pi/mac-o3-source-sha256.txt) |

[Verificación machine-readable](mediciones/gemelo-pi/verification.json): **456 casos**, **9.120 s de audio medido**, 192 WAV comprobados con SHA-256 independiente (`hashlib` sobre el data chunk leído con `wave`); metadatos 48 kHz/PCM16/estéreo/144.000 frames, métricas JSON y manifiestos de fuentes válidos. **Sin NaN/Inf, sin clipping ni silencio inesperado.** Los 32 fixtures son idénticos en tres repeticiones por build, entre bloques 128/256 y, en este Mac, también entre O2/O3. Eso no promete igualdad Clang/libSystem frente a GCC/glibc. Los vectores SHA-256 vacío/`abc`/un millón de `a` y FTZ input/output pasan; ctest **13/13 en O2 y O3** (11 pruebas existentes + 2 del arnés).

Se verificaron los flags efectivos de las **23 unidades de compilación** del arnés y sus bibliotecas por build; `-O2`/`-O3` es la última optimización del comando, con `-fno-fast-math -ffp-contract=off -mcpu=apple-m1`. El `-O3` inicial de `CMAKE_CXX_FLAGS_RELEASE` queda sobrescrito por `-O2` en esa variante. Comprobación adicional del arnés con `-Wall -Wextra -Wpedantic`: sin diagnósticos. CLI rechaza threads distintos de 1, bloques inválidos, engine desconocido, segundos cero/negativos/NaN.

Comando ejecutado en este Mac: `bash tools/gemelo-pi/run_on_pi.sh /tmp/gf-gemelo-pi-mac-v2 20`. Los WAV y logs completos quedan en `/tmp/gf-gemelo-pi-mac-v2`; builds en `/tmp/gf-gemelo-pi-build.wOJDzU` (O2/O3). Se guardan JSON/tablas/hashes en el repo y se evitan ~106 MiB de WAV binarios versionados; el script los regenera en cada máquina. No hay `vcgencmd` en macOS: no se declara validación térmica Pi.

**Entrega Git:** documento escrito antes de implementar; intento de `git add` rechazado al crear `.git/index.lock` (`Operation not permitted`). Todo queda **sin commits** en `codex/gemelo-pi`, conforme a la excepción solicitada. Sin push, instalaciones, red, cambios de firmware ni modificaciones a los specs `00–06`. No se implementa el host de audio.
