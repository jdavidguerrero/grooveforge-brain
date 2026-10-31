# Gemelo ARP 2600: primer núcleo portable

## Teoría y alcance

El firmware ARP suma VCO 1, VCO 2 y el producto de ambos (ring modulation) antes del filtro 4072 y del VCA. La multiplicación desplaza energía a suma y diferencia de frecuencias; una mezcla seca mantiene las fundamentales. El núcleo inicial reutiliza `PolyBlepCore`, `LadderStageCore` y `AnalogEnv` custom del proyecto, no la Teensy Audio Library oficial. Cada una de las cuatro voces actuales del firmware tiene su propio filtro y envolvente, con asignación rotativa y robo de la más antigua. Las envolventes tienen overshoot 0,6 y el filtro de control se actualiza a 1 kHz.

Esta primera entrega cubre dos osciladores, ring, filtro, envolventes y cuatro voces con MIDI básico y sustain; su prueba genera señal para demostrar el recorrido. Todavía no cubre GFP, ruido, sync, VCO 3, modo mono, efectos ni integración VST. La documentación de fidelidad describe como objetivo el ARP monofónico, mientras el firmware vigente también ofrece cuatro voces: este piloto sigue el comportamiento polifónico implementado sin decidir el alcance definitivo. La comparación A/B con hardware queda para el final.

Referencias: ADR-006; `theory/fidelidad-engines.md` §§9, 11.4; `engines/arp2600.cpp`; Välimäki y Huovilainen, *Antialiasing Oscillators in Subtractive Synthesis*, IEEE Signal Processing Magazine (2007); Smith, *Physical Audio Signal Processing*, ring modulation.

## Incremento: piloto seleccionable en VST

El piloto recibe notas MIDI y sustain con precisión de muestra y entrega su señal mono a ambas salidas del plugin. El selector y el control RING se guardan como parámetros del DAW; cutoff, resonancia, ataque y release usan los cuatro controles generales. Cambiar de motor reinicia voces para evitar notas retenidas. Esta integración inicial no incluye GFP, pitch bend, efectos, GROOVE ni la página PATCH y se identifica como piloto en la interfaz. La prueba verifica salida, offset MIDI, restauración de estado y aislamiento. No se realizan comparaciones A/B.

## Incremento: importación GFP parcial

El plugin reconoce GFP del engine 5 y aplica al núcleo portable las formas de VCO, ratio, anchos de pulso, niveles directos y ring, envolventes, drive y sensibilidad de velocity. El cutoff, resonancia y tiempos generales se inicializan desde el preset con las macros de firmware ya aplicadas. Los bytes GFP originales se conservan en el proyecto para restaurar el patch; el parámetro RING del DAW mantiene prioridad al editarlo. Este incremento todavía no interpreta ruido, PATCH (mono, sync y VCO3), LFO, glide, unison, GROOVE o FX, por lo que el estado visible indica GFP parcial. Se comprueban importación, rechazo de archivos corruptos, cambios audibles y restauración; no se realizan A/B.

## Incremento: ruido de color continuo

Cada voz añade ruido al mezclador antes del filtro, con nivel `OSC 1 / NOISE × 0,5`, color continuo blanco–rosa `PATCH / NOISE COLOR` y normalización conjunta con los dos VCO y ring, igual al ruteo del firmware. El generador usa el filtro custom de tres polos de Kellet con polos adaptados a la frecuencia del DAW; no usa la Teensy Audio Library oficial. Cada voz tiene estado y semilla propios. La prueba activa exclusivamente ruido, comprueba salida finita, silencio a nivel cero, respuesta al color y restauración desde GFP. El espectro y las semillas no sustituyen la comparación física A/B final.

## Incremento: hard sync VCO 1 → VCO 2

Cuando PATCH/SYNC está activo, cada vuelta del VCO 1 reinicia la fase del VCO 2. Se reutiliza el registro de eventos y la corrección BLEP del `PolyBlepCore` compartido para conservar la fracción de muestra del cruce. La activación procede del GFP y se restaura con él; desactivado, los osciladores corren de forma independiente. La prueba comprueba salida finita, que SYNC produzca un cambio audible y que el estado se restaure; la fidelidad física queda para la comparación final. Referencias: `engines/arp2600.cpp::setSync` y `dsp/polyblep_core.h`.

## Incremento: VCO 3 como modulador

El VCO 3 del firmware funciona a tasa de control como un triángulo común a las voces. La posición de su fase avanza a 1 kHz y sus destinos multiplican la frecuencia de VCO 1 y 2 por `2^(profundidad × triángulo)` y el cutoff por `2^(4 × profundidad × triángulo)`. El GFP proporciona frecuencia y cantidades independientes para ambos destinos; las cantidades cero desactivan su efecto. En este incremento el VCO 3 es un modulador, no una fuente de audio. Las pruebas verifican cada destino por separado y la restauración del proyecto, sin ejecutar A/B físico. Referencia: `engines/arp2600.cpp::update()`.

## Incremento: pitch bend MIDI

El pitch bend MIDI usa el rango fijo compartido por los pilotos existentes (±2 semitonos), aplicado a las frecuencias de los dos VCO en el offset de muestra del evento. El centro restaura la afinación y al reiniciar o cambiar de motor se limpia su estado. El bend es una decisión del adaptador VST; su rango y fidelidad con el flujo MIDI del Brain requieren validación posterior. Se comprueba comportamiento observable y precisión de offset sin A/B físico.

## Incremento: MONO y glide

El GFP selecciona MONO mediante PATCH y define glide con la curva del firmware `glide_ms = glide² × 1500`. En mono se emplea `dsp::NoteStack` custom (ocho notas, prioridad a la última): si hay legato y glide activo, el pitch se desplaza sin redisparar las envolventes; al soltar la nota superior vuelve a la anterior. Sin glide, cada nota nueva redispara las envolventes. El pedal retiene las liberaciones hasta levantarse. En modo polifónico la transición de frecuencia sigue el comportamiento del firmware al reutilizar una voz activa. El cálculo exponencial se actualiza a 1 kHz. Las pruebas comprueban secuencias de notas, pedal, salida y restauración; no son A/B con el Brain. Referencias: `engines/arp2600.cpp::noteOn/noteOff/update` y `dsp/note_stack.h`.

## Incremento: LFO y expresión MIDI

El LFO custom compartido se evalúa a 1 kHz y aplica los destinos GFP disponibles para ARP: pitch, cutoff y PWM. CC1 añade vibrato; la presión de canal añade vibrato y apertura de cutoff, como `engine_mod` del firmware. En modo SYNC sigue el tempo del DAW y la división del GFP; libre usa la frecuencia del preset. Se reinicia al cambiar de motor o preset, y todavía no sincroniza su fase al desplazar o repetir el transporte. Se comprueban destino, controles MIDI y persistencia sin A/B físico. Referencias: `theory/lfo-modulacion.md`, `engines/arp2600.cpp::update` y `49-studio-brain.cpp::engine_mod`.

## Incremento: UNISON y DRIFT del GFP

UNISON asigna hasta cuatro voces del ARP a cada nota, repartiendo su desafinación simétricamente con `dsp::unison_ratio`; la salida se compensa con `1/√N`, como la ganancia del PUMP en firmware. En MONO se fuerza una sola voz. DRIFT usa `dsp::Drift` custom para elegir por nota y voz variaciones reproducibles de pitch y cutoff; las diferencias fijas entre voice cards permanecen aparte. La liberación y sustain afectan todas las voces de la nota, y el estado se recupera desde el GFP. El plugin aún no interpreta los demás controles GROOVE ni ofrece edición de UNISON/DRIFT. Las pruebas cubren la asignación, liberación, saturación a cuatro voces y determinismo, sin A/B físico. Referencias: `engines/arp2600.cpp::noteOn`, `49-studio-brain.cpp::groove_tick`, `dsp/voice_spread.h` y `variants/studio/theory/09-studio-groove.md`.
