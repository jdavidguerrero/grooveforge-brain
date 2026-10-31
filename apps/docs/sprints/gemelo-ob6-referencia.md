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
