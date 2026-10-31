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

## Incremento editor OSC y automatización

La página OSC expone nueve parámetros estables del host: WAVE (0 saw, 1 square, 2 pulse, 3 saw+pulse), SUB, NOISE, PW, CHORUS (0 off, 1 I, 2 II, 3 I+II), HPF (0 boost, 1 flat, 2 225 Hz, 3 720 Hz), RANGE (0 16', 1 8', 2 4'), VCA (0 env, 1 gate) y HISS. Sus unidades y límites corresponden a los slots del firmware. Importar GFP actualiza estos controles; exportar incorpora sus valores editados. En sesiones anteriores a esta versión se inicializan desde el GFP guardado y las sesiones nuevas conservan sus valores automatizados. El audio aplica cambios entre bloques, sin prometer automatización a resolución de muestra.

## Incremento editor ENV/FILTER

La página ENV expone decay/sustain de amplitud, ADSR de filtro, cantidad de envolvente, drive y sensibilidad de velocity de amplitud y filtro. Attack y release de amplitud siguen en la página principal. Se editan valores efectivos después de GRIT y POCKET; al exportar se incorporan en el GFP con esas macros a cero, igual que los controles generales. IDs estables y estado del host permiten automatización por bloque. La migración de sesiones previas obtiene los valores efectivos del GFP, o los valores iniciales si no había preset. Se prueban respuesta audible, restauración y exportación. Las rutas de LFO, GROOVE y FX todavía necesitan su propio editor.

## Incremento editor LFO

La página LFO permite automatizar RATE (0,05–20 Hz), SHAPE (seno, triángulo, sierra, cuadrada y sample & hold), DIV (1/4, 1/8, 1/16, 1/8T), profundidades de cutoff, pitch y PWM, y DELAY (0–3000 ms). Se reutilizan el reloj de 1 ms y el LFO compartido; los cambios llegan al comienzo del bloque. La sincronización se guarda en el estado del DAW y sigue sin representarse en GFP, así que la exportación requiere desactivarla. Los proyectos anteriores migran desde el GFP, y se prueban la respuesta audible, la migración y la exportación.

## Incremento controles de voz GROOVE

La página GROOVE expone glide en milisegundos, encendido de UNISON, selección 0–3 (1/2/3/6 voces), desafinación de 0–50 cents y DRIFT 0–100 %. Al importar GFP se convierte glide desde su valor normalizado cuadrático; al exportar se aplica la inversa. Estos controles se guardan en el estado del DAW y se migran desde el GFP en sesiones anteriores. PUMP, ARP y CHORD todavía requieren su editor. Las pruebas verifican asignación, efecto audible, migración, persistencia y roundtrip del preset.

## Incremento editor PUMP / ARP / CHORD

Los controles editan las tres paradas GROOVE con el mismo catálogo y unidades del firmware. El estado se guarda en el DAW y exporta a GFP; sesiones anteriores migran desde el GFP. El scheduler y PUMP siguen siendo los del piloto compartido; la memoria de acordes aprendidos en hardware no existe en GFP. Las pruebas verifican importación, automatización observable, persistencia y exportación, sin afirmar equivalencia física.

## Incremento editor SYNTH FX

La página FX expone encendido, mezcla y tres parámetros por ranura: DRIVE, TAPE, CHORUS/PHASER, DELAY y REVERB. Se usan los IDs estables del host y el mapeo de la cadena SYNTH común; las cargas antiguas migran desde GFP y la exportación incorpora los valores editados. La actualización se aplica cuando cambia un control, sin reiniciar las voces. Se validan cambios audibles, persistencia, exportación y compatibilidad de Prophet/DX7. El delay, PLATE y MODAL del piloto y la compensación de UNISON aún requieren comparación A/B con el Brain.

## Incremento macros de Juno

GRIT, POCKET y MOVE trabajan como desplazamientos sobre los valores editables del host; GRIT RES, GRIT MOD y POCKET VEL ajustan sus profundidades. Al importar GFP, los valores efectivos se cargan como base y las posiciones de GRIT/POCKET/MOVE comienzan en cero, evitando aplicar dos veces el preset. Al exportar se incorporan los valores efectivos y se dejan esas tres macros en cero; sus posiciones independientes no se conservan en el GFP exportado. El estado del DAW sí conserva posiciones y valores base. MOVE afecta cutoff, resonancia y los FX activos; se usan las funciones compartidas del firmware. La actualización es por bloque y la fidelidad física permanece pendiente.

## Fuente de reloj de PUMP en el VST

El SOURCE del Brain es una selección del reloj común: AUTO, MIDI o INTERNO. La spec del plugin establece que el host gobierna BPM, posición y transporte; el piloto todavía no recibe MIDI Clock ni tiene un selector de reloj interno independiente. El editor conserva el valor SOURCE del GFP (0 AUTO, 1 MIDI, 2 INT), lo automatiza y lo exporta sin alterarlo, pero el audio del plugin sigue siempre el reloj del DAW. El texto de ayuda del control comunica esa limitación. No se declara equivalencia de las transiciones MIDI/INT hasta implementar un contrato de reloj compatible con la spec y probarlo; tampoco se añade un control que prometa cambiar la fuente de audio sin hacerlo. La migración de la versión anterior importa solo SOURCE para preservar los demás valores automatizados de PUMP.

## Incremento ruido blanco PJRC

`AudioSynthNoiseWhite` oficial usa Park–Miller–Carta (16807, módulo 2³¹−1) y extrae el entero de 16 bits bajo de cada estado. La ganancia de la instancia en Juno es 1 y la mezcla posterior multiplica ese PCM por NOISE × 0,5 y velocity. El piloto sustituye xorshift por una adaptación portable de ese generador con seis semillas independientes y aritmética PCM16. El shim de referencia usa una formulación modular independiente y permite una escena con ruido; se validan vectores y se conservan las siete referencias anteriores. En hardware las semillas dependen del orden global de construcción y de cuántos bloques hayan corrido: no se garantiza fase de ruido idéntica. El mezclador y ladder del piloto siguen usando float y no se declara equivalencia bit a bit del render completo.

Fuente: Paul Stoffregen/PJRC, Teensy Audio Library `synth_whitenoise.cpp` y `synth_whitenoise.h`, licencia incluida en `tools/gemelo-baseline/PJRC-LICENSE.txt`; S. K. Park y K. W. Miller, «Random Number Generators: Good Ones Are Hard to Find», Communications of the ACM 31(10), 1988.
