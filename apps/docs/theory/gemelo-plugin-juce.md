# Gemelo del Brain · DSP compartido y plugin JUCE

> Fecha: 2026-10-29 · Estado: teoría para revisión, previa a implementación.
> Rama: `feat/gemelo-dsp` · Base documental: ADR-006.
> Juan confirmó JUCE y pidió reorganizar la GUI actual para escritorio, con Serum como referencia de organización.
> Spec asociado: [gemelo-plugin-v1.md](../specs/gemelo-plugin-v1.md).

## 1. Qué estamos construyendo y por qué

El instrumento debe sonar en varias pistas del DAW sin grabar el Brain por partes. El motor de asistencia necesita
el mismo instrumento sin interfaz para producir audiciones reproducibles. Un único motor C++ puede servir a ambos
y al firmware: cambian la entrada de eventos y el destino de las muestras, no la síntesis.

Esto es un port/refactor de C++, no una traducción a otro lenguaje. JUCE envuelve el instrumento para el DAW y dibuja
su editor. No sustituye nuestros osciladores, filtros o efectos por los suyos.

La revisión previa del ADR había pausado el plugin y elegido un shim. La instrucción actual de Juan retoma el plugin
con JUCE. El shim conserva valor como referencia del grafo anterior; el destino vuelve a ser un núcleo por instancia
con frecuencia parametrizable. No se debe convertir el grafo global de AudioStream en un singleton del VST.

## 2. Corrección verificada: Teensy 4.1 = 44.100 Hz nominales

Verificado el 2026-10-29 en el framework Teensyduino instalado:

| Fuente | Evidencia |
|---|---|
| `cores/teensy4/AudioStream.h:57–61` | Si no hay override, `AUDIO_SAMPLE_RATE_EXACT` vale `44100.0f` |
| `cores/teensy3/AudioStream.h:58–65` | El default de Teensy 3.x es `44117.64706` |
| `libraries/Audio/output_i2s.cpp`, rama `__IMXRT1062__`, `config_i2s` | El PLL de I2S se configura a partir de `AUDIO_SAMPLE_RATE_EXACT` |
| `apps/firmware-teensy/platformio.ini` | No define un override de esa constante |

Por tanto, nuestra referencia digital inicial es **44.100 Hz**, no 44.117,6 Hz. La frecuencia física exacta y su
desviación requieren medir el reloj; leer el código confirma la configuración nominal, no una medición del cristal.
Los comentarios de `audio_graph.h/.cpp` que describen 44.117,6 Hz son heredados y no establecen el contrato.

48 kHz/24 bit sigue siendo el formato de entrega del dataset propuesto en ADR-006. No significa que el Brain opere
a 48 kHz ni que cambiar la cabecera de un WAV sea remuestrear.

## 3. Tres tasas distintas

1. **Audio:** una muestra cada `1 / fs` segundos. Osciladores, filtros y VCA avanzan con esta tasa.
2. **Control:** actualizaciones de LFO, glide, macros y envolventes de control. Deben tener un reloj explícito.
3. **GUI:** dibuja una instantánea reciente del estado. Su velocidad nunca determina cómo suena el instrumento.

Un ataque de 10 ms equivale a 441 muestras a 44,1 kHz y a 480 a 48 kHz. Mantener un contador fijo de 441 muestras
cambiaría su duración al cambiar el host. Un oscilador usa `incremento = frecuencia / fs`; un filtro TPT emplea
coeficientes que dependen de `tan(pi * cutoff / fs)`. Delay, chorus y reverb necesitan revisar también longitudes
de líneas, interpolación y polos; pasar `fs` solo al oscilador no basta.

Se propone procesar a la frecuencia del host: 44,1/48/96 kHz. Frecuencia interna fija más SRC es una alternativa
si la validación la justifica: conserva una referencia única, pero agrega filtros de remuestreo, latencia y pruebas.
No se implementan dos caminos durante el piloto.

## 4. Reloj de muestras y bloques variables

En hardware, algunos `update()` leen `millis()` y las notas retrasadas usan `micros()`. El render offline puede
calcular diez segundos de música en mucho menos tiempo real. Esos relojes no pueden gobernar el núcleo portable.

Usaremos posición absoluta de muestras, eventos con offset dentro del bloque y posición musical del host. El
control del piloto tendrá ticks de 1 ms mediante acumulador fraccional anclado a muestras, independiente del tamaño
de callback. A 44,1 kHz los intervalos no pueden ser todos de 44 muestras: el acumulador distribuye 44/45 sin deriva.
Esta cadencia es una referencia determinista; no reproduce el jitter del `loop()` físico.

El procesamiento se divide en los límites de eventos y ticks de control. El orden para un mismo instante debe ser
estable y documentado: controles restaurados/automatización, MIDI en orden recibido, cálculo de control y audio
posterior. No se reordena arbitrariamente un note-off frente a un note-on. Los eventos retrasados internos deben
conservar también el orden y cancelarse si la nota se suelta antes de dispararse.

El shim golden puede mantener bloques de 128 y eventos aplicados al siguiente borde, como referencia histórica.
El núcleo del plugin puede aplicar notas en su offset exacto. Para comparar ambos se alinean los eventos a bordes de
128; una prueba distinta verifica la precisión temporal del plugin. No confundir mejora de resolución con regresión.

JUCE entrega MIDI con offsets; no se presume que todas las automatizaciones VST3 lleguen con esa misma resolución
a través de la abstracción elegida. El wrapper documentará la granularidad efectiva y suavizará parámetros continuos
en el núcleo. No se promete automatización sample-accurate de todos los parámetros sin comprobarla.

## 5. Capas del software

```text
libs/gf-dsp            osciladores, filtros, voces, engines, etapas y FX
        ↑
libs/gf-instrument     patches, Hybrid, macros, GROOVE, scheduler y presets
        ↑
        ├── Teensy: adaptadores AudioStream / entradas físicas
        ├── gf-render: MIDI + snapshot → WAV + manifiesto
        └── brain-plugin: AudioProcessor JUCE + editor JUCE
```

Las rutas son propuestas para implementación, todavía no creadas. `gf-dsp` y `gf-instrument` son C++17 sin Arduino,
LVGL ni JUCE. No se copian engines a una segunda implementación. Un shim de pruebas puede portar semántica de PJRC,
pero no gobierna el estado del plugin ni su arquitectura.

Contrato conceptual: `prepare(sample_rate, max_block)`, `reset(seed)`, carga de estado preparada fuera del callback y
`process(outputs, frames, events, transport)`. `max_block` expresa capacidad; `frames` puede cambiar en cada llamada.
También se define cómo procesar eventos en llamadas de cero muestras y cómo dividir callbacks mayores que la
capacidad reservada. Repreparar por cambio de frecuencia ocurre fuera del procesamiento normal.

Cada instancia posee voces, RNG, cola, líneas de delay y parámetros. Las tablas inmutables sí pueden compartirse.
Memoria acotada y reservada antes del audio; sin disco, logging, locks o asignaciones en `process`. La publicación de
un patch utiliza un intercambio preparado; tampoco se destruyen buffers grandes en el hilo de audio.

## 6. Qué sonido conservar

El inventario anterior sigue siendo útil: núcleos puros en `src/dsp/`, `prophet_osc_core`, `dx7_core`, decodificadores
GFP/GFL nativos y lógica de macros. Las cifras de líneas y los 154 tests del theory anterior son una foto histórica,
no una ejecución de pruebas de este documento.

| Tipo | Ejemplos | Trabajo |
|---|---|---|
| Custom puro | PolyBLEP, ZDF ladder, envelopes, chorus BBD, FM 4-op | Reutilizar y hacer explícitos fs, reloj y semilla |
| Custom con AudioStream | wrappers de filtro, VCA y efectos | Separar buffer/IO del cálculo de muestras |
| Teensy Audio Library oficial | mixers Q16, ruido, biquads, chorus, waveshaper, Freeverb estéreo | Inventariar uso, portar/aislar semántica con atribución y comparar |
| Pegamento de Studio | preset → engine, `layer_apply`, macros, modulación, SFX y notas | Extraer a instrumento compartido |

No se presupone que quitar las cuantizaciones intermedias sea inaudible. El grafo actual hace conversiones int16 y
sumas saturadas por etapas. Sumar en float y recortar solo al final puede diferir incluso sin ruido de cuantización:
`clip(clip(a+b)+c)` no equivale siempre a `clip(a+b+c)`. El piloto conserva escala, orden de suma y saturación relevantes.
Cualquier simplificación posterior necesita evidencia y una decisión explícita, no la suposición de que float es mejor.

El Prophet piloto tiene que incluir voces, filtro CEM3320, VCA, envolvente del filtro, POLY-MOD, glide y asignación de
voces; su núcleo de osciladores por sí solo no es el instrumento. El DX7 es el de ADR-005, cuatro operadores y ocho
algoritmos, no una emulación nueva de seis operadores.

## 7. Hybrid y SYNTH FX

Referencia: `ui/layers.h`, `ui/layer_bank.h` y `sketches/49-studio-brain.cpp`.

La capa suma dos engines distintos con ganancias base, transposición y retraso de B. Se conserva la fórmula actual
de `layer_gains`, incluidos sus límites: sustituirla por un crossfade de libro cambiaría la mezcla central. Se
conserva también `ENGINE_ACTIVE_GAIN = 0.7` y la topología de saturaciones que se mida en el grafo.

La cadena Studio suma A/B y aplica etapa de salida → DRIVE → TAPE → CHORUS/PHASER → DELAY → REVERB → PUMP.
Los SYNTH FX y GROOVE compartidos toman la configuración de A. La GUI debe explicarlo para evitar editar B esperando
oír una segunda reverb. El estéreo del Juno y el ping-pong no pueden colapsarse a mono.

El modo FX Processor sobre LINE_IN está fuera del plugin v1. Los efectos integrados de los presets sí están dentro.
POCKET es una macro de articulación/velocity; no debe confundirse con una humanización aleatoria genérica.

## 8. GUI de escritorio: conservar identidad, cambiar navegación

La GUI actual usa LVGL a 800×480 y está organizada para botones/encoders. Copiarla píxel a píxel al DAW conservaría
restricciones del panel. Se trasladan a JUCE los tokens, nombres, diagramas y agrupaciones útiles; los controles se
reimplementan para ratón, teclado y automatización.

Serum es la referencia organizativa dada por Juan: ver módulos relacionados simultáneamente, edición directa y
zona de modulación disponible. No implica añadir wavetables, una nueva matriz libre o copiar su apariencia.

Se conservan fondo `#0A0A0A`, superficies `#141414/#1C1C1C`, texto `#F2F2F2`, SYNTH teal `#1D9E75`, HYBRID cyan
`#1E90C0`, geometría y jerarquía de la familia Studio. IBM Plex Mono se reutiliza si sus archivos/licencia lo permiten.
La GUI no usa las variables globales mutables de `theme.h`: dos instancias en modos diferentes deben ser independientes.

Se propone A/B arriba, síntesis en el centro, envolventes/LFO en una franja inferior y macros siempre visibles. La
separación A/B significa engines completos, no OSC A/OSC B de un único engine. Los osciladores internos conservan sus
nombres propios. Detalles de layout y aceptación en el spec.

## 9. Reproducibilidad, estado y audiciones

Un preset por nombre o slot no identifica el sonido. El estado del DAW guarda las mitades completas, sus cambios,
modo, macros, FX, configuración de GROOVE y versión del esquema. Un banco embebido fija el significado de CC 18 para
esa sesión. Reordenar el banco externo no debe cambiar una canción guardada.

Una semilla explícita sirve para audiciones; el modo interactivo conserva variación natural determinista a partir de
su estado inicial y eventos. Reabrir el proyecto restaura patch/configuración, no promete continuar exactamente la
fase de una reverb a mitad de una nota. Repetir desde un reset y origen temporal definidos sí debe ser reproducible.

El hash se exige idéntico dentro del mismo build/entorno. Entre ARM y escritorio pueden cambiar FMA, redondeos o
funciones matemáticas: se comparan con tolerancias. Nunca etiquetar audio de software como captura física.

## 10. Pruebas que dan evidencia

Antes de implementar tests se fija qué comparan:

- Golden antes/después a 44,1 kHz, misma semilla, escala, eventos y ticks de control; casos saw, PWM, POLY-MOD,
  glide, resonancia y robo de voz. El shim histórico requiere incorporar su fuente y procedencia: la prueba scratch
  del theory anterior no cuenta como baseline disponible en CI.
- Partición 1/17/64/127/128/257/512 muestras y secuencia variable: el núcleo recibe idénticos eventos absolutos y
  debe producir el mismo resultado dentro de la tolerancia fijada. Incluir offsets cero/final de bloque.
- Tiempo/afinación a 44,1/48/96 kHz. La duración de cada nota se mide en segundos y la afinación en cents. No se
  exige igualdad muestra a muestra entre frecuencias diferentes.
- Dos instancias: modificar o reiniciar B no cambia el render de A. Probar también modos GUI distintos.
- Hybrid: MORPH 0/50/100, ganancias, transposición límite, nota soltada antes del retraso, CC 18 y restauración.
- DAW: estado, automatización, cierre del editor, seek/loop/stop, Freeze y exportación con colas.
- Teensy: build real de Studio (`sketch49`), tests nativos y CPU antes/después; el objetivo sigue ≤60% en uso normal.

El spec fija umbrales iniciales del piloto. Si fallan, se investiga antes de cambiarlos. Las pruebas de pitch y
envolvente se aíslan de drift, FX y filtros auto-oscilantes; la validación de esos comportamientos usa casos separados.

La comparación con SGTL5000/salida analógica usa nivel, envolvente, espectro y A/B/X. Un null digital controlado ayuda
a encontrar regresiones; un null contra hardware analógico no es el criterio de equivalencia del ADR.

## 11. Referencias

- PJRC: [Audio Library](https://github.com/PaulStoffregen/Audio), y
  [AudioStream Teensy 4](https://github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h).
  Para reproducibilidad del piloto se fijarán versiones/hashes de los archivos instalados, no solo enlaces a master.
- JUCE: [AudioProcessor](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html),
  [CMake API](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md),
  [licencia JUCE 8](https://juce.com/legal/juce-8-licence/). Fuentes oficiales consultadas el 2026-10-29.
- Välimäki y Huovilainen, “Antialiasing Oscillators in Subtractive Synthesis”, IEEE Signal Processing Magazine, 2007.
- Zavalishin, *The Art of VA Filter Design*, 2018: coeficientes TPT/ZDF dependientes de fs.
- Widrow y Kollár, *Quantization Noise*, Cambridge University Press, 2008: contexto para cuantización; no demuestra
  por sí solo equivalencia perceptual de un grafo con saturaciones.
- Fuentes del proyecto: ADR-003/005/006; `theory/gemelo-fase1-nucleo-dsp.md` (histórico),
  `variants/studio/theory/12-studio-capas.md`, `produccion/motor-asistencia.md`, `ui/groove_macros.h`,
  `ui/layer_bank.h`, `sketches/49-studio-brain.cpp`, `apps/studio-head/src/ui/theme.h`.
