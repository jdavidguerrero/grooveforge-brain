# Gemelo OB-6: referencia digital

## Teoría y alcance

El OB-6 del Brain dispone de seis voces con dos VCO PolyBLEP, sub y ruido, un filtro SEM de dos polos con morph LP–notch–HP y BP, y envolventes RC. El filtro usa integración trapezoidal y su adaptador Teensy interpola coeficientes por bloque. X-MOD usa VCO2 a frecuencia de audio como fuente de VCO1; en SYNC, VCO2 es maestro y VCO1 esclavo. El orden de construcción de los objetos importa porque la Teensy Audio Library actualiza el maestro antes que el esclavo. Una implementación portable deberá preservar esa dirección y documentar cualquier cambio de agenda.

Esta etapa captura el grafo OB6 del firmware mediante el shim de pruebas, a 44.100 Hz y bloques de 128 muestras. Los ticks de control ocurren antes de cada bloque y no reflejan el jitter ni el reloj físico; el ruido usa una implementación escalar compatible con PJRC y semillas del harness. Se generan escenas de acorde, morph, BP, SYNC, X-MOD, ruido, robo de voz y glide con robo. Son configuraciones directas del engine: no incluyen GFP, GROOVE, FX Studio, codec ni hardware. Se verifican tamaño, señal, repetibilidad, release y procedencia; no son una prueba de equivalencia del VST.

Referencias: `theory/fase2-engines.md` §1; `theory/fidelidad-engines.md` §9 y §11.3; `src/engines/ob6.cpp`; Zavalishin, *The Art of VA Filter Design*, cap. 4; Simper, *The State Variable Filter*, Cytomic (2013).

## Piloto audible

El primer corte conecta un patch inicial de seis voces, dos VCO saw, sub, SEM LP y envolventes. Reutiliza `PolyBlepCore`, `sem_svf_core` y `AnalogEnv`; ofrece MIDI con sustain y pitch bend y los cuatro controles básicos del VST. Es una adaptación flotante: no reproduce todavía el mixer PCM16 ni la interpolación de coeficientes por bloques del adaptador de firmware. No incluye SYNC, X-MOD, GFP, ruido, morph/BP, LFO, GROOVE ni FX. Cada instancia mantiene su estado y la deriva de VCO avanza con cadencia temporal de 128 muestras a 44,1 kHz. La demo es una nota o acorde audible y se prueba estado, offsets, finiteza y liberación. No constituye un OB-6 completo ni fidelidad validada.

## Incremento GFP y filtro SEM

Se acepta GFP OB-6 (engine 3), se guarda el archivo completo en el estado del DAW y se aplica un subconjunto explícito: formas continuas de los dos VCO, mezcla, sub, ruido, intervalos, pulse widths, morph LP/notch/HP o BP, drive, envelopes y velocity. GRIT, POCKET y MOVE se aplican como valores efectivos iniciales según las funciones compartidas. Los cuatro controles generales siguen editables y se restauran en el proyecto. SYNC, X-MOD, glide, LFO, GROOVE y SYNTH FX todavía no se aplican; la interfaz indica la reproducción parcial. El ruido usa el generador PCM PJRC portable con semillas locales, sin garantizar la fase de la unidad física. La prueba verifica rechazo sin cambios, carga de fábrica, cambios audibles y reapertura; no se afirma fidelidad A/B.

## Incremento SYNC y X-MOD del GFP

VCO2 se renderiza primero y registra sus cruces de fase; VCO1 los consume para hard sync. Cuando SYNC está activo, el intervalo del preset mueve VCO1 y VCO2 mantiene la frecuencia de la nota. La señal VCO2 también modula VCO1 a frecuencia de audio para FREQ y PWM mediante `PolyBlepCore`, mientras FILTER ENV modula FREQ, PWM y morph del SEM a 1 kHz, según los destinos del GFP. Las profundidades GRIT se obtienen de `macro_patch`. El procesamiento por muestra respeta eventos MIDI del host, pero aún difiere de los bloques y la cuantización PCM16 del firmware: las referencias digitales y el hardware siguen siendo necesarios para validar fidelidad. Los tests comprueban dirección del sync, efectos audibles de destinos X-MOD, persistencia y salida finita; no certifican equivalencia completa.

## Incremento glide, UNISON, DRIFT y LFO

El piloto aplica el glide del GFP (`glide² × 1500 ms`) a 1 kHz sobre voces reutilizadas. UNISON asigna de 1 a 6 voces y distribuye la desafinación mediante `voice_spread.h`; DRIFT modifica pitch y cutoff por nota con el núcleo compartido. El LFO del GFP se evalúa a 1 kHz para cutoff, pitch y PWM; CC 1 añade vibrato y LFO SYNC sigue el BPM del host. La fase de seek/loop aún no se alinea. La compensación de UNISON ocurre al final del piloto mono, antes de una futura cadena FX: su posición definitiva debe compararse con el Brain. Se prueban respuesta audible, sustain, reproducibilidad y mapeo del GFP; no se declara equivalencia física.
# Incremento: SYNTH FX del GFP en OB-6

La señal mono del motor alimenta el bus estéreo portable que ya usa el piloto DX7. Se aplican los parámetros y macros efectivos del GFP con el mismo mapeo de drive, tape, chorus/phaser, delay y reverb que los demás motores. El bus mantiene memoria propia por instancia y se reinicia al cambiar de motor o preset; el tempo del host controla el delay. Como el OB-6 todavía no recibe la modulación de afinación del tape, esa parte del efecto no queda cubierta. Las pruebas comparan reproducción seca y procesada, restauración de estado y finiteza; no validan equivalencia con firmware ni hardware. Referencias de diseño: ADR-006 y `05-fx-architecture.md`.

## Incremento: afinación del tape

Wow y flutter producen una variación temporal de velocidad: al reproducir la cinta más rápido o más lento cambia la frecuencia percibida. El bus portable ya calcula esa variación como razón de frecuencia; el OB-6 la aplica a ambos osciladores y al sub, además del bend y LFO, tomando el valor del bus de la muestra anterior. El bus avanza su control a 1 kHz y devuelve razón unidad cuando tape está desactivado. La prueba del motor mide cruces para una razón conocida y verifica reset; la integración comprueba la restauración del preset. Esto no establece equivalencia con el circuito ni con hardware. Referencias: ADR-006 y `05-fx-architecture.md`.

## Incremento: GROOVE del GFP

El bus portable recibe las notas MIDI, administra sustain y genera las notas de ARP y CHORD antes de entregarlas al OB-6; PUMP modifica la señal en el bus según el transporte del host. El unísono y DRIFT permanecen en el sintetizador, evitando duplicar voces. La carga y restauración de GFP configuran el bus sin agregar todavía controles de edición para este motor. La prueba compara un preset con PUMP contra su equivalente desactivado y valida sustain y restauración; no sustituye una comparación A/B con el firmware o el hardware. El bus compartido conserva sus limitaciones existentes, incluidas las fuentes de PUMP no implementadas. Referencias: ADR-006 y `05-fx-architecture.md`.

## Incremento: edición GROOVE para OB-6

Los parámetros de PUMP, ARP y CHORD se exponen con identificadores propios del OB-6 para que el DAW pueda automatizarlos sin alterar Juno. La interfaz reutiliza la organización de GROOVE existente; la automatización se aplica al inicio de cada bloque y los cambios se guardan en el estado del proyecto. Al abrir proyectos anteriores, los valores iniciales se recuperan del GFP almacenado. La fuente de PUMP se conserva como dato, pero el bus utiliza el transporte del DAW. Las pruebas cubren importación, automatización audible y restauración; exportar las ediciones a GFP queda para otra etapa.

## Incremento: exportación GFP

El archivo exportado parte del GFP OB-6 cargado y conserva los campos aún no editables. Los controles comunes y GROOVE reemplazan los valores originales. GRIT y POCKET se incorporan en sus valores efectivos, incluida la sensibilidad a velocity; MOVE ya está incorporado en cutoff y resonancia. Los tres macros se ponen a cero para evitar aplicarlos dos veces al volver a importar. El LFO sincronizado al DAW no se exporta porque el estado de sincronía no tiene representación equivalente en este flujo. Se valida el contenido decodificado y la reproducción tras reimportar; la codificación GFP puede cuantizar los valores.

## Incremento: edición SYNTH FX

Drive, tape, chorus/phaser, delay y reverb cuentan con parámetros independientes del OB-6 para activación, mezcla y tres parámetros por efecto. La interfaz comparte la distribución de Juno, pero cada engine conserva sus valores. Los cambios se aplican al inicio del bloque y se guardan en el estado del DAW y en la exportación GFP; los proyectos anteriores recuperan los valores del preset. Las pruebas comprueban importación, cambio audible, restauración y exportación.

## Incremento: osciladores y X-MOD editables

Los valores de los dos VCO y X-MOD se exponen en unidades GFP, con límites de `GF_OSC_SLOT` y `GF_MOD_SLOT`. El audio usa el patch almacenado como base y aplica las ediciones antes de calcular las macros, sin tocar los controles generales, GROOVE ni efectos. Cada parámetro se puede automatizar y se conserva en el estado y el GFP. Se prueba cambio audible, restauración y exportación; la fidelidad con hardware permanece por medir.

## Incremento: envolventes, LFO y voces editables

Los parámetros del envelope, filtro, velocity, LFO, glide, unísono y DRIFT se editan en las unidades GFP o del control correspondiente. Se aplican al patch antes de GRIT y POCKET, manteniendo sus relaciones con los controles del preset. La frecuencia de actualización es por bloque; el LFO sigue ejecutándose en la tasa de control portable de 1 kHz. Se guardan y automatizan por instancia y se exportan con las mismas reglas de macros del GFP. La prueba cubre edición, sonido y restauración, sin afirmar equivalencia A/B.

## Incremento: macros de producción OB-6

GRIT, POCKET y MOVE y sus tres profundidades se editan como parámetros independientes y se guardan en el proyecto. El GFP contiene valores base y macros; al importarlo el control general ya representa cutoff, resonancia y release efectivos. Para conservar ese comportamiento al automatizar, los cambios posteriores de macro se calculan respecto al valor original del GFP. Las demás rutas utilizan directamente `macro_patch`, `macro_vel_depths` y `move_sfx`. La exportación fija en el GFP los valores efectivos para evitar la doble aplicación al reimportar. Las pruebas comprueban cambios audibles, persistencia, migración de proyectos previos y exportación con comparación de audio. A diferencia de versiones anteriores del piloto, MOVE también afecta los efectos al importar un GFP: un proyecto guardado con MOVE distinto de cero puede sonar diferente. La equivalencia física queda pendiente.

## Incremento: diagnóstico frente al grafo digital

Se renderizan las ocho escenas del fixture OB-6 tanto en el piloto como en el grafo de firmware con shim. El informe registra RMS, error y correlación sin alinear ni aplicar un umbral de aprobación; también genera WAV para escucha y comprueba hashes y repetibilidad. Las escenas usan el mismo MIDI y valores iniciales, pero difieren la temporización de control, el mixer PCM16, el filtro y las semillas de ruido. El resultado permite priorizar discrepancias concretas; no cubre presets, efectos del VST, codec ni hardware físico y no certifica equivalencia sonora. El hito se valida generando el informe y comprobando sus entradas en una prueba nativa.

## Investigación: reloj de control del shim

El fixture actual adelanta los ticks de control al inicio de cada bloque de 128 muestras; el piloto avanza su control a 1 kHz a lo largo del audio. Para separar ese factor se compila una variante temporal que agrupa los ticks del envelope y glide al principio del bloque y se compara contra las mismas ocho escenas. No cambia el plugin ni pretende simular la agenda física del Brain. La diferencia entre informes permite estimar cuánto contribuye esa decisión a las discrepancias, sin probar causalidad para las etapas que no se han aislado. Se verifican tamaños, determinismo, procedencia y métricas sin umbral de aprobación.

En la ejecución arm64 del 31 de octubre de 2026, X-MOD pasó de correlación 0,8904 a 0,9994 al agrupar solo el control del filtro; agrupar solo glide dejó X-MOD en 0,8904. En glide con robo, agrupar solo glide elevó la correlación de 0,9323 a 0,9989; agrupar solo el filtro la dejó en 0,9324. Agrupar ambos produjo 0,9994 y 0,9990 respectivamente. El ruido permaneció en 0,9639, consistente con las distintas semillas. Estas son métricas de las escenas completas, no una medida perceptual; diferencias en cuantización y transitorios siguen presentes. Dado que el reloj real de llamadas a `update()` depende de la ejecución del firmware, estos datos no justifican adoptar sin medición física el reloj adelantado del fixture.

## Incremento: presión de canal MIDI

En el sketch Studio, la presión de canal normalizada suma hasta una octava al cutoff y hasta 0,3 a la profundidad de vibrato, antes de limitar esta última a 1. El piloto OB-6 aplica la misma relación en su tick de control de 1 kHz; el evento MIDI se recibe en su offset del bloque y el cambio se escucha desde el siguiente tick, sin afectar muestras anteriores. No se interpreta presión polifónica como presión de canal. Se prueba señal, offset e independencia de instancias; no se mide latencia del teclado ni equivalencia física. Referencia: `49-studio-brain.cpp`, `engine_mod` y `after_touch`.

## Incremento: CC MIDI de macros OB-6

Como en Studio, CC 14 controla DRIFT, CC 15 GRIT, CC 16 POCKET y CC 19 MOVE. El audio aplica cada mensaje en su posición MIDI; la publicación al parámetro del DAW se hace desde el hilo de mensajes. El estado y el GFP incluyen el valor pendiente aunque se guarden antes de esa publicación. Se comprueban offset, aislamiento entre instancias, restauración y exportación. DRIFT afecta notas nuevas; las demás macros afectan el sonido activo. Referencias: `49-studio-brain.cpp`, `control_change`; `ui/page_params.cpp`, `apply_cc`.

## Incremento: validación del editor OB-6

El selector de página debe recorrer voz, osciladores, envolventes/LFO, GROOVE, FX y macros sin mostrar controles de otra página. Las páginas compartidas con Juno deben enlazar sus controles al engine activo sin modificar el otro. Una prueba de componentes JUCE recorre páginas y modifica controles vinculados; no sustituye la inspección visual en un DAW ni verifica accesibilidad o disposición a otras escalas.

CC 74 y 71 siguen pendientes: Studio los aplica a cutoff y resonancia base antes de las macros, mientras los controles comunes actuales del VST guardan valores efectivos al importar un GFP. Con MOVE o GRIT activos, escribir directamente el valor MIDI en esos controles cambiaría el resultado y, en los extremos, el recorte impide recuperar el valor base. Su implementación requiere conservar el valor base y migrar el estado existente; la prueba de componentes no cubre esa conversión.

## Incremento: CC de filtro OB-6 y compatibilidad de estado

CC 74 usa `gf_cutoff_from_cc_norm` para convertir la posición MIDI en frecuencia base logarítmica; CC 71 establece la resonancia base lineal del rango OB-6. GRIT y MOVE se aplican después, como en `control_change` y `engine_filter` de Studio. El plugin conserva el valor base recibido por MIDI por separado del control compartido, que continúa representando la referencia anterior de los proyectos existentes; así la saturación no destruye el valor MIDI original. Si el usuario o la automatización cambian después el control compartido, este recupera la prioridad. El valor base y los mensajes aún pendientes se guardan en el estado del DAW y se consideran al exportar GFP. La prueba cubre offsets, macros, saturación, guardado/restauración y prioridad del control; no verifica la unidad física.

## Incremento: independencia del tamaño de bloque del host

Un DAW puede entregar los mismos eventos MIDI con bloques de distinto tamaño. La agenda de nota, presión y CC debe depender del offset temporal de cada evento y del reloj interno del motor, no del siguiente límite de bloque del host. La prueba reproduce la misma secuencia a 44,1, 48 y 96 kHz con bloques de 128 y 32 muestras y compara ambos canales; no cubre automatización de parámetros que el host entregue solo una vez por bloque ni los relojes del hardware.

## Incremento: legibilidad del editor

Las capturas locales de las seis páginas mostraron valores con hasta ocho decimales que exceden las cajas y encabezados de FX asociados solo al primer efecto. Se presenta una cantidad compacta de decimales sin modificar la precisión del parámetro ni el texto aceptado al editar, y las columnas compartidas se nombran P1–P3, con el nombre real de cada control en su ayuda contextual. La prueba verifica formato, entrada numérica y enlaces de parámetros; la revisión en un DAW y a otras escalas sigue pendiente.
