# Gemelo Juno-106: referencia digital inicial

## Teoría y alcance

El Juno del Brain combina seis voces rotativas: DCO PolyBLEP, suboscilador y ruido, ladder con carácter IR3109, envolventes y un chorus estéreo BBD con HPF. El oscilador y el chorus ya usan núcleos DSP propios; la asignación de voces y el grafo de mezcla permanecen en `Juno106`. Su reutilización exige conservar ganancias, cuantización PCM16 y orden de actualización del firmware. Una nueva implementación independiente no demuestra fidelidad (ADR-006).

Este primer corte captura el grafo actual a 44.100 Hz en un shim determinista, sin cambiar firmware. El shim sustituye el generador de ruido por silencio: los casos fijan su ganancia en cero y no validan ruido. El reloj de control avanza por milisegundos antes de cada bloque de 128 muestras; no reproduce el jitter de hardware. Capturamos ambos canales después del chorus y antes de los efectos Studio. No es un engine de plugin ni una prueba A/B física.

## Demo y validación

Se renderizan acordes, PWM, glide, robo de voz, HPF y modos de chorus a WAV estéreo. Para cada caso se comprueba tamaño, reproducibilidad, señal y caída tras note-off. Se guardan hashes y procedencia; no se aplican umbrales de equivalencia al plugin hasta definir un comparador con el mismo recorrido y eventos. El chorus incluye búfer de 512 muestras: su comportamiento a frecuencias altas requiere revisión específica antes de habilitarlo en el host.

Referencias: ADR-006; `engines/juno_106.cpp`, `audio/juno_chorus.h`, `dsp/juno_chorus_core.h`; Välimäki y Huovilainen, «Oscillator and Filter Algorithms for Virtual Analog Synthesis», Computer Music Journal 30(2), 2006.

## Piloto inicial en el VST

El primer incremento conecta un patch fijo de seis voces al host: DCO saw+pulse, sub, filtro IR3109, envolventes y chorus estéreo del núcleo compartido. Se admiten notas, sustain y pitch bend. La mezcla y el ritmo de llamadas al oscilador son aproximaciones flotantes; no sustituyen la comparación PCM con el grafo de referencia. El piloto aún no importa GFP Juno, no implementa ruido, LFO/PWM, glide, GROOVE ni SYNTH FX y no demuestra fidelidad A/B. El chorus inicialmente conservaba el búfer fijo del firmware: originalmente se desactivaba por encima de 95 kHz; el incremento posterior amplía el búfer para frecuencias altas.

## Incremento GFP y patch de Juno

El archivo GFP de engine 1 usa el decoder de Studio y se conserva completo en el estado del DAW. Se aplican los campos OSC (forma, sub, ruido, PW, chorus, HPF, rango y VCA gate), hiss, envolventes, sensibilidad a velocity, drive y el cutoff/resonancia con las macros GRIT, POCKET y MOVE. El control general de release está limitado a 10.000 ms, mientras Juno admite hasta 12.000 ms; valores mayores quedan recortados en el piloto. El ruido portátil usa un generador determinista propio y no reproduce todavía la cuantización o semilla del ruido PJRC. Los cuatro controles generales del editor representan valores efectivos del preset y son automatizables; el resto del patch se conserva en el GFP, pero aún no es editable desde el panel. La carga de un GFP inválido debe conservar el estado anterior; guardar y restaurar una sesión debe conservar sus bytes y selección.

La carga no implica fidelidad completa: el resto de GROOVE, SYNTH FX y exportación de Juno siguen pendientes; glide, UNISON y DRIFT se incorporan en el incremento siguiente. Comparar contra la referencia requiere cubrir primero el ruido real PJRC y las diferencias de cuantización y actualización por bloque.

## Incremento glide, UNISON y DRIFT

El glide del firmware actualiza la frecuencia una vez por milisegundo con `1-exp(-1/glide_ms)`; una voz libre comienza directamente en la nota y una voz reutilizada interpola desde su frecuencia anterior. El valor GFP se convierte con `glide² × 1500 ms`. UNISON asigna 1, 2, 3 o 6 voces por nota, con razones simétricas del núcleo `voice_spread.h` y ganancia `1/√N` aplicada después del chorus en este piloto. DRIFT reutiliza el generador determinista del firmware para pitch y cutoff por nota. La compensación del Brain está después de SYNTH FX; aquí no hay todavía esa cadena, por lo que el orden completo sigue pendiente.

Las pruebas verifican que la asignación múltiple produce audio diferente, que note-off y sustain liberan todas las voces, y que DRIFT es reproducible por instancia y altera el resultado. Glide se compara entre notas consecutivas que reutilizan la misma voz; esto verifica el comportamiento audible sin afirmar equivalencia de muestras con el grafo PCM del hardware.

## Incremento LFO y PWM

Se reutiliza `dsp::Lfo` a 1 kHz con las formas, velocidad y división del GFP. CUTOFF aplica ±4 octavas, PITCH usa la curva cuadrática del firmware y la rueda MIDI suma vibrato; PWM desplaza el ancho del DCO. El LFO corre libre y su profundidad crece desde cero durante LFO DELAY cuando llega la primera nota de un grupo. El sync sigue el BPM del host; la fase no se realinea al cambiar posición o repetir un loop. Las pruebas comparan renders con y sin modulación y comprueban persistencia del preset y eventos MIDI. La equivalencia de timing frente al loop físico permanece pendiente.

## Incremento GROOVE y bus estéreo

Juno entrega dos canales tras su chorus. El bus portátil debe procesarlos con estado independiente para DRIVE, TAPE, chorus/phaser y MODAL, conservando el intercambio estéreo del delay. PLATE del piloto utiliza una suma mono para su entrada húmeda y una salida estéreo; esto es una aproximación existente que requiere comparación con el firmware. PUMP, ARP y CHORD reutilizan el scheduler compartido y envían eventos a Juno. La compensación UNISON se conserva en la salida de Juno; ubicarla tras los efectos queda pendiente de A/B y de refactor del bus. Se validan salida estéreo, cambio audible con el preset, aislamiento de instancias y compatibilidad de Prophet/DX7.

## Incremento exportación GFP

La exportación Juno parte del GFP importado o un patch INIT, actualiza los cuatro controles generales y conserva los campos que todavía no tienen editor. GRIT, POCKET y MOVE se incorporan a los valores efectivos y se ponen a cero para evitar aplicarlos dos veces al recargar; DRIFT permanece como macro. El GFP no registra LFO SYNC, por lo que la exportación se rechaza mientras está activado. Se verifican el formato, la conservación de parámetros y la recarga audible. La GUI completa de todos los parámetros permanece pendiente.

## Chorus a frecuencias altas del host

El búfer circular del firmware tiene 512 muestras: no alcanza para el retardo máximo del modo II a 96 kHz. El núcleo compartido pasa a aceptar un tamaño como parámetro de compilación; el alias del firmware mantiene 512 muestras. En el plugin se conserva ese mismo núcleo y tamaño hasta 95 kHz, y se usa una instancia de 4096 muestras por encima. Así se preserva la aritmética anterior a 44,1 kHz y se habilitan los mismos retardos temporales hasta 384 kHz sin asignación en el hilo de audio. La prueba comprueba salida estéreo, niveles finitos y reproducibilidad a 44,1/48/96/192/384 kHz; no sustituye la comparación de frecuencia y sonido frente al hardware.
