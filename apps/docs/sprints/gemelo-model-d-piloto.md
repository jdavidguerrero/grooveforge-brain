# Gemelo Model D: piloto monofónico

## Teoría y alcance

El Model D del Brain es un sintetizador monofónico: tres VCO PolyBLEP alimentan un ladder digital de cuatro polos y una envolvente de amplitud. Al tocar notas superpuestas se mantiene una pila de hasta ocho teclas, con prioridad a la última y retorno a la anterior al soltarla. La envolvente de filtro se calcula a 1 kHz y la del amplificador a frecuencia de audio; ambos contornos son RC con overshoot 0,3. El mixer normaliza la suma de niveles antes del ladder. El filtro usa el núcleo ZDF propio compartido con firmware, no es un modelo de la Teensy Audio Library oficial.

Este primer corte entrega un sonido inicial audible con tres sierras, detune fijo, ladder, envolventes, legato, sustain MIDI y pitch bend. Los cuatro controles generales del VST modifican cutoff, resonancia, ataque y release. El render es float portable a la frecuencia del DAW: no emula la cuantización del mixer de Teensy ni su agenda de bloques. No incluye aún importación GFP, las demás formas, ruido, glide, modulación, GROOVE, FX o HYBRID. La interfaz debe indicar claramente que es un piloto parcial.

## Validación y demo

Una nota debe sonar sin hardware; al soltar la última tecla debe caer a silencio. Con dos teclas, soltar la más reciente debe recuperar la anterior. Se comprueban sustain, salida finita, aislamiento de instancias, offsets MIDI y tasas 44,1, 48 y 96 kHz. Esto no verifica fidelidad A/B física ni rendimiento de Teensy.

Referencias: ADR-006; `theory/fase2-engines.md` §2; `theory/fidelidad-engines.md` §§9–11.2; `engines/moog_model_d.cpp`; Zavalishin, *The Art of VA Filter Design*, caps. 5–6; Stinchcombe, *Analysis of the Moog Transistor Ladder and Derivative Filters* (2008).

## Incremento: GFP parcial, osciladores y envelopes

Se acepta GFP Model D y se guardan sus bytes completos en el estado del DAW. El piloto aplica las seis formas por VCO mediante `PolyBlepCore`, la mezcla y afinación, pulse width, los envelopes, drive, velocity, DECAY y glide del patch. La pila mono mantiene el pitch anterior durante legato con glide y restaura la nota anterior al soltar la última. El GFP se identifica como parcial: ruido, VCO3 KBD/LO, modulación, LFO, GROOVE y SYNTH FX aún no se reproducen. La prueba cubre banco de fábrica, cambios audibles, límites, legato y restauración; no valida equivalencia con el Brain físico.

## Incremento: ruido blanco y rosa

Model D usa el `ColorNoiseCore` custom del firmware, no `AudioSynthNoiseWhite` oficial. El núcleo genera ruido blanco con xorshift y filtra una copia con tres polos para aproximar el espectro rosa; el GFP selecciona blanco/rosa y el nivel. El ruido entra al mixer con la ganancia ×0,6 del firmware y participa en su normalización antes del ladder. La instancia usa una semilla local reproducible; el orden global de construcción y la fase del ruido en el Brain físico pueden diferir. Los coeficientes rosa del firmware están definidos para 44,1 kHz; el incremento posterior de este documento describe su adaptación en el plugin. Las pruebas aíslan el ruido, comprueban nivel cero, diferencias entre colores, reproducibilidad y salida finita; no son una comparación A/B física. Referencia: `dsp/color_noise_core.h`, filtro de Paul Kellet, MusicDSP #76.

## Incremento: control del VCO 3 y del filtro

El GFP activa o desactiva el seguimiento de teclado del VCO 3: desactivado toma C3 como referencia fija; LO divide su frecuencia por 100. FILTER KBD multiplica el cutoff por `(frecuencia / C3)^(posición/3)`, y OVERDRIVE multiplica el drive por `1 + 3 × overdrive`, usando el límite del ladder portable compartido. El control sigue al pitch mono interno, antes de bend, como `MoogModelD::update()`. La prueba compara renders GFP con cada opción activada frente al mismo preset con la opción desactivada, sin reclamar equivalencia del grafo de hardware. Referencias: `engines/moog_model_d.cpp` y `theory/fidelidad-engines.md` §11.2.

## Incremento: MODULATION y LFO

La rueda determina la profundidad de MODULATION. El VCO 3 se calcula primero y modula por muestra la frecuencia de VCO 1 y 2; la parte de ruido de MOD MIX se actualiza a 1 kHz, igual que FILTER MOD. El LFO portable aplica los destinos GFP a pitch, cutoff y PWM; la presión de canal añade cutoff y vibrato, como el sketch Studio. La división sincronizada sigue el tempo del host. Las pruebas aíslan los destinos y verifican cambios audibles, salida finita y procesamiento MIDI; la temporización del firmware y la fase del LFO al desplazar el transporte quedan pendientes de medición. Referencias: `engines/moog_model_d.cpp` y `49-studio-brain.cpp::engine_mod`.

## Incremento: GROOVE y SYNTH FX del GFP

Model D entrega su audio mono al bus estéreo portable por instancia. El bus aplica DRIVE, TAPE, chorus/phaser, delay y reverb, y usa el transporte del host para PUMP y ARP; CHORD alimenta la pila monofónica mediante el mismo ruteo MIDI. TAPE entrega su razón de afinación al motor en la muestra siguiente, como los otros pilotos. Los parámetros provienen del GFP y se restauran al abrir el proyecto. DRIFT y edición independiente de estos controles se tratan en incrementos posteriores. Las pruebas comparan presets secos y procesados, señal y restauración, sin afirmar equivalencia A/B física. Referencias: ADR-006 y `05-fx-architecture.md`.

## Incremento: DRIFT por nota

La macro DRIFT del GFP utiliza `dsp::Drift`, compartido con el firmware, para elegir una variación de afinación y cutoff por cada nueva nota. El estado pseudoaleatorio es local y reproducible por instancia; no se garantiza la fase ni las semillas de una unidad física. La prueba comprueba cambio audible y determinismo con el mismo preset.

## Incremento: exportación del GFP cargado

La exportación parte del GFP Model D importado y conserva sus parámetros que aún no tienen controles de edición. Los cuatro controles generales del VST reemplazan cutoff, resonancia y tiempos de ataque y release. GRIT, POCKET y MOVE se incorporan a los valores efectivos y luego se ponen en cero, evitando reaplicarlos al volver a importar; DRIFT se conserva. Se rechaza la exportación con LFO SYNC activo porque el GFP no guarda ese estado. Las pruebas verifican el tipo de engine, los valores modificados y la reimportación; no cubren la fidelidad al hardware.

## Incremento: edición OSC y MODULATION

Las tres formas y niveles de VCO, afinación, ruido y los ocho parámetros MODULATION tienen identificadores propios, visibles en la página OSC y automatizables desde el host. El patch GFP editado se aplica antes de sus macros y se preserva al guardar el proyecto o exportar. Los estados anteriores recuperan estos valores del GFP guardado. La prueba verifica cambios audibles, restauración, exportación y aislamiento frente a otros motores; las pruebas A/B se reservan para el final de la implementación completa.

## Incremento: edición ENV, LFO y DRIFT

Decay y sustain del amplificador, envolvente del filtro, cantidad de envelope, drive, sensibilidad a velocity, LFO, glide y DRIFT tienen parámetros propios de Model D. Sus valores se aplican antes de las macros y se guardan en el estado y en GFP; attack y release del amplificador siguen en los controles generales. Los proyectos anteriores restauran los valores de su GFP. La prueba comprueba edición audible y persistencia sin ejecutar pruebas A/B.

## Incremento: edición GROOVE y FX

PUMP, ARP, CHORD y la cadena de cinco efectos usan parámetros propios de Model D y comparten la organización visual existente. Los cambios se aplican al bus de ese motor y se conservan al guardar o exportar; la carga de un estado anterior inicializa estos parámetros desde su GFP. La fuente de PUMP se conserva, pero el plugin sigue el reloj del DAW. Se verifica aislamiento entre motores, cambios audibles y persistencia.

## Incremento: macros editables

GRIT, POCKET y MOVE y sus profundidades se automatizan por instancia. Se aplican al patch base y a los FX usando las funciones compartidas con firmware; los controles generales importados conservan su representación efectiva mediante el ajuste relativo a las macros originales. La exportación incorpora los valores efectivos y pone en cero las tres macros principales para evitar duplicarlas. Se verifican cambios de audio, estado y exportación; las pruebas A/B se reservan para el final de la implementación completa.

## Incremento: CC MIDI de macros

CC 14, 15, 16 y 19 controlan DRIFT, GRIT, POCKET y MOVE en el offset MIDI. Como en los otros pilotos, el DSP recibe el valor en el hilo de audio y la publicación a parámetros del DAW ocurre en el hilo de mensajes. El guardado y la exportación conservan valores pendientes; las pruebas cubren offset, restauración y exportación inmediata.

## Incremento: CC MIDI de filtro

CC 74 establece el cutoff base con la curva logarítmica del Model D y CC 71 la resonancia base. Las macros se aplican después de esos valores, conservándolos por separado cuando el resultado se satura. Los controles del DAW vuelven a tener prioridad si se editan posteriormente. El estado y la exportación preservan valores pendientes. La prueba cubre offsets, extremos, coexistencia con MOVE y restauración, sin pruebas A/B.

## Incremento: ruido rosa a la frecuencia del DAW

El filtro rosa de Kellet del firmware fija tres polos a 44,1 kHz. Para que sus tiempos de decaimiento no cambien con el DAW, el motor del plugin usa `p' = p^(44100 / fs)` para cada polo. Se ajusta cada ganancia de excitación por `sqrt((1-p'²)/(1-p²))`, manteniendo la varianza de cada sección ante ruido blanco ideal. A 44,1 kHz se conservan exactamente los coeficientes originales. Esta aproximación mantiene la escala temporal y la energía de los polos individuales; no garantiza un espectro idéntico a todas las frecuencias ni sustituye la validación A/B con el Brain. El código nuevo vive en el plugin y no modifica el firmware. Se prueban salida finita, reproducibilidad, silencio y energía en frecuencias de muestreo comunes.

Referencias: Julius O. Smith III, *Introduction to Digital Filters*, secciones sobre respuesta impulsional y transformación de polos; Paul Kellet, filtro rosa economy, MusicDSP #76; `dsp/color_noise_core.h`.
