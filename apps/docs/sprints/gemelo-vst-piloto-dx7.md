# Piloto DX7 para el gemelo VST

El siguiente corte reutiliza `Dx7VoiceCore` del firmware, implementado en C++ portable: cuatro operadores de
modulación de fase, ocho algoritmos y envolventes en dB (ADR-005 y `sprints/46-dx7-4-operadores.md`). El piloto
reproduce el E.PIANO de arranque de `DX7Engine::begin`, con seis voces, asignación rotativa, velocity, sustain y
pitch bend. Se suma una envolvente de amplitud y el filtro de tono CEM3320 del firmware. El preset de arranque es
una demostración: las páginas de operador GFP, modulación, GROOVE, efectos compartidos e HYBRID de DX7 aún no
están conectados. El selector del plugin conserva Prophet como valor inicial, por compatibilidad de sesiones.

El test escucha la salida MIDI en el procesador, verifica estado del selector y liberación de notas. Un test nativo
de DSP cubre seis voces y salida finita en 44,1/48/96 kHz. La cuantización del grafo Teensy, la fidelidad del
filtro y la comparación con hardware quedan pendientes. Referencias: ADR-005, `engines/dx7_core.*`, `engines/dx7.cpp`
y Chowning, «The Synthesis of Complex Audio Spectra by Means of Frequency Modulation», JAES, 1973.

## Incremento: importación GFP DX7

El decodificador compartido valida versión, engine y valores finitos antes de modificar el estado. El plugin guarda
los bytes GFP en el proyecto y aplica la página OSC de los cuatro operadores, algoritmo, feedback, ondas, detune,
keyboard scaling y envelopes según `engine_osc` del sketch 49, incluyendo GRIT y POCKET. También aplica filtro de
tono, velocity y envolvente de amplitud. La importación selecciona el engine del archivo; archivos inválidos no
alteran el estado. El motor DX7 sigue siendo una preescucha: GROOVE, LFO, glide y la cadena de FX todavía no están
conectados. Se comprueban migración, rechazo, restauración y cambio audible al variar el patch. La equivalencia del
grafo PCM y el costo por instancia quedan pendientes de medición y A/B.

## Incremento: UNISON, DRIFT, GLIDE y LFO

DX7 utiliza hasta seis voces por nota; `unison_ratio` reparte los cents y `unison_gain` compensa la suma. DRIFT usa
el mismo generador por nota del Brain. GLIDE interpola frecuencia a 1 kHz, mientras el LFO del firmware modula pitch
y amplitud según el preset y la rueda MIDI; la sincronización sigue el BPM del DAW. Las colas se liberan por nota,
incluso con varias voces y sustain. Se prueban GFP, audio, liberación, determinismo y división temporal. El reloj
físico, jitter y cuantización aún requieren A/B. Referencias: `engines/dx7.cpp`, `dsp/voice_spread.h`,
`dsp/lfo.h` y `variants/studio/theory/09-studio-groove.md`.

## Incremento: cadena de efectos DX7

La salida DX7 pasa por la misma cadena de SYNTH FX usada por Prophet. El GFP selecciona DRIVE, TAPE,
CHORUS/PHASER, DELAY y MODAL/PLATE, evitando duplicar el procesamiento. La cadena del piloto sigue siendo una
adaptación de escritorio: MODAL procesa en flotante y UNISON se
compensa antes de los efectos. Se prueban salida procesada, restauración del preset y estéreo. GROOVE de notas
continúa como incremento separado; el A/B físico y el costo de múltiples instancias quedan pendientes.

## Incremento: GROOVE de DX7

ARP, CHORD/STRUM y PUMP reutilizan la agenda y el reloj de muestras del bus SYNTH, conectando sus eventos a las
voces DX7 mediante callbacks sin asignaciones. UNISON y DRIFT permanecen en el propio DX7; su ganancia se aplica
antes de los efectos en este piloto. Una nota generada por el tick de ARP entra en DX7 a la muestra siguiente por el
orden del bus; esta diferencia y los acordes aprendidos fuera del GFP requieren A/B. Se comprueban carga GFP,
secuencia, sustain, note-off y ausencia de notas colgadas.

## Incremento: edición de operadores

El editor expone algoritmo y feedback globales, y por cada uno de los cuatro operadores ratio, nivel, ataque,
decay, sustain, release, onda y detune. Cada control tiene un ID estable para automatización y se guarda en el
estado del DAW; la importación GFP inicializa sus valores con las macros aplicadas. El audio toma los cambios al
inicio de cada bloque, sin asignaciones, y las envolventes de amplitud conservan el release más largo de las
portadoras. Se comprueban cambio audible por automatización, restauración del estado y funcionamiento del editor.
La edición no exporta GFP ni reemplaza los controles pendientes de GROOVE, FX y modulación general. Referencias:
`GF_FM_OP`, `GF_FM_WAVE`, `GF_FM_DETUNE`, `engine_osc` y ADR-005.

Medición local de orientación: render de dos segundos con seis notas en `Dx7Preview` (sin wrapper JUCE ni FX),
compilación Release en este Mac: 0,051 s a 44,1 kHz, 0,046 s a 48 kHz y 0,090 s a 96 kHz. No mide picos de
callback, exportación, ni varias instancias en Live; esas pruebas siguen pendientes. Los estados DX7 guardados antes
de incorporar los nuevos controles recuperan sus valores desde el GFP guardado.

## Incremento: controles de voz y LFO

La página VOICE expone cutoff, resonancia, drive, envolvente global, velocity, glide y los parámetros de LFO
(velocidad, forma, profundidad de pitch/amplitud y división). Cada parámetro tiene ID propio para automatización;
el GFP inicializa valores efectivos después de las macros, y el estado antiguo se migra desde el GFP guardado. Los
cambios se aplican al comienzo del bloque de audio. Se verifican modificación audible, restauración y migración.
Los controles de GROOVE y FX y la exportación GFP requieren incrementos separados.

La misma página añade UNISON (activación, número y detune) y DRIFT con IDs propios. Cambiar el número de voces
reinicia las notas para evitar notas colgadas, como el cambio de ruteo en Studio; los estados anteriores se migran
desde su GFP. Se verifica que el cambio modifique el audio y se restaure al reabrir.

## Incremento: exportación GFP DX7

La exportación parte del GFP DX7 cargado o de un patch INIT, reemplaza operadores y controles de voz por los valores
efectivos del DAW y escribe GFP v5 con el serializador del Brain. GRIT, POCKET y MOVE se fijan en cero en el archivo:
sus efectos sobre los parámetros editables se incorporan a esos valores para evitar aplicarlos dos veces al reabrir.
El archivo conserva GROOVE y FX del GFP de origen y la macro DRIFT editable. El estado de LFO SYNC no está en GFP;
si está activo se rechaza la exportación para no perderlo silenciosamente. También se rechaza si el GFP cargado es de
otro engine. Se prueba la importación del archivo exportado y el cambio audible y se explicitan las diferencias por
macros y cuantización en la UI. El resultado se confirma contra hardware durante el A/B posterior.

## Incremento: edición de SYNTH FX

La página FX muestra encendido, mezcla y tres parámetros para DRIVE, TAPE, CHORUS/PHASER, DELAY y REVERB. Cada
valor normalizado tiene un ID de automatización estable; el GFP inicializa los valores, el estado anterior se migra
desde su archivo y la exportación escribe los ajustes actuales. El bus usa los mismos mapeos del sketch 49. Al
volver a encender el delay se limpian sus líneas, como en el firmware. Se prueban cambio audible, persistencia y
reimportación. El significado de TYPE y TIME corresponde a los pasos discretos del Brain; su fidelidad y el coste
del borrado de las colas quedan para la validación posterior.

## Incremento: edición de GROOVE

La página GROOVE expone los encendidos y parámetros de PUMP, ARP y CHORD que afectan la ruta del DAW; el reloj
proviene del transporte del host, por lo que no se presenta la selección física de fuente MIDI/AUTO/INT. Los
parámetros se automatizan y se guardan/exportan con GFP. Los acordes aprendidos en la memoria del Brain no viajan en
el formato; el modo memoria usa su acorde inicial. La prueba cubre activación desde el host, persistencia y exportación.

## Incremento: macros GRIT, POCKET y MOVE

Se exponen como desplazamientos automatizables sobre los controles efectivos del plugin, con profundidades GRIT
RES, GRIT MOD y POCKET VEL. Los GFP cargados inicializan las posiciones de macro en cero porque su efecto ya se
incorporó a los controles efectivos; así no se duplica. El código reutiliza las fórmulas del Brain y aplica MOVE
a los efectos activados. La exportación incorpora el resultado a los valores GFP y deja estas macros en cero: reproduce
la posición actual, pero no conserva las posiciones de macro como controles independientes. Se prueba cambio de
audio, persistencia, cero neutro y exportación/reimportación. La automatización MIDI CC de estas macros queda fuera
de este incremento.

## Incremento: MIDI CC de macros

El Brain asigna CC 14 a DRIFT, 15 a GRIT, 16 a POCKET y 19 a MOVE. El plugin los aplica en el offset del evento
MIDI y entrega los cambios a los parámetros del DAW mediante un buzón atómico y un temporizador del hilo de mensajes;
el callback de audio no llama a la interfaz ni a notificaciones del host. El estado y la exportación conservan el
último valor incluso si aún no se ha entregado al DAW. Se prueban el offset, el sonido, el estado y GFP. Los CC de
MORPH y selección de capa corresponden a HYBRID y permanecen fuera del piloto DX7.
