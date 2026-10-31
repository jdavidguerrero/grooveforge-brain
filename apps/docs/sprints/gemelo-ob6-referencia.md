# Gemelo OB-6: referencia digital

## Teoría y alcance

El OB-6 del Brain dispone de seis voces con dos VCO PolyBLEP, sub y ruido, un filtro SEM de dos polos con morph LP–notch–HP y BP, y envolventes RC. El filtro usa integración trapezoidal y su adaptador Teensy interpola coeficientes por bloque. X-MOD usa VCO2 a frecuencia de audio como fuente de VCO1; en SYNC, VCO2 es maestro y VCO1 esclavo. El orden de construcción de los objetos importa porque la Teensy Audio Library actualiza el maestro antes que el esclavo. Una implementación portable deberá preservar esa dirección y documentar cualquier cambio de agenda.

Esta etapa captura el grafo OB6 del firmware mediante el shim de pruebas, a 44.100 Hz y bloques de 128 muestras. Los ticks de control ocurren antes de cada bloque y no reflejan el jitter ni el reloj físico; el ruido usa una implementación escalar compatible con PJRC y semillas del harness. Se generan escenas de acorde, morph, BP, SYNC, X-MOD, ruido, robo de voz y glide con robo. Son configuraciones directas del engine: no incluyen GFP, GROOVE, FX Studio, codec ni hardware. Se verifican tamaño, señal, repetibilidad, release y procedencia; no son una prueba de equivalencia del VST.

Referencias: `theory/fase2-engines.md` §1; `theory/fidelidad-engines.md` §9 y §11.3; `src/engines/ob6.cpp`; Zavalishin, *The Art of VA Filter Design*, cap. 4; Simper, *The State Variable Filter*, Cytomic (2013).

## Piloto audible

El primer corte conecta un patch inicial de seis voces, dos VCO saw, sub, SEM LP y envolventes. Reutiliza `PolyBlepCore`, `sem_svf_core` y `AnalogEnv`; ofrece MIDI con sustain y pitch bend y los cuatro controles básicos del VST. Es una adaptación flotante: no reproduce todavía el mixer PCM16 ni la interpolación de coeficientes por bloques del adaptador de firmware. No incluye SYNC, X-MOD, GFP, ruido, morph/BP, LFO, GROOVE ni FX. Cada instancia mantiene su estado y la deriva de VCO avanza con cadencia temporal de 128 muestras a 44,1 kHz. La demo es una nota o acorde audible y se prueba estado, offsets, finiteza y liberación. No constituye un OB-6 completo ni fidelidad validada.
