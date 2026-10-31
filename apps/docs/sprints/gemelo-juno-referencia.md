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

## Cadencia de deriva del DCO

En `PolyBlepCore` del Brain el generador de deriva y su filtro avanzan una vez por llamada de 128 muestras, no una vez por muestra. El piloto llamaba al núcleo para cada muestra y aceleraba ese proceso 128 veces a 44,1 kHz. El núcleo expone una opción para mantener la deriva entre llamadas; su valor por defecto conserva el comportamiento del firmware. El piloto avanza la deriva cada 128 muestras a 44,1 kHz, usando un acumulador temporal para mantener ese intervalo en otras frecuencias y sin depender del tamaño del buffer del host. El oscilador sigue respondiendo a cambios de MIDI y PWM por muestra, por lo que el resultado no reproduce todas las decisiones de bloque del firmware. Se compara el núcleo por muestras contra renders de 128 muestras con parámetros constantes y se repiten referencias y compilaciones existentes. La sincronización con el bloque físico y la equivalencia completa requieren A/B.

La prueba del wrapper también detectó que `set_parameters` volvía a escribir el cutoff cuando el acumulador coincidía con cero al comienzo de un callback; eso dependía del tamaño del bloque del host. Se eliminó esa escritura repetida: el filtro se actualiza por el tick de control. La prueba compara 32 y 128 muestras por callback a 48 kHz, con el mismo MIDI y sin cambios de parámetros entre bloques.

## Comparador digital Juno: referencia de firmware y piloto

Antes del A/B físico se compara un render aislado de `JunoPreview` con las escenas `chord` y `white_noise` capturadas del grafo firmware a 44,1 kHz. Se fijan notas, velocity, parámetros iniciales, duración y eventos a los mismos bordes de 128 muestras. Se generan WAV estéreo PCM16 y un informe con RMS de señal y error, error pico y correlación por canal, además de hashes y procedencia. El resultado es diagnóstico: no hay umbral de aprobación calibrado y no se compensan fase, ganancia ni retardo. El shim del firmware aplica el control antes de cada bloque, mientras el piloto usa ticks temporales dentro del bloque; la salida del piloto es float y no reproduce toda la cuantización de los mixers. No se incluyen presets Studio, SYNTH FX, codec ni hardware.

Resultado local inicial (arm64, 44.100 Hz, comparación sin alineación): en el canal izquierdo, `chord` da correlación 0,999922 y error RMS −65,44 dBFS; `white_noise`, 0,999905 y −64,72 dBFS. Los picos son −31,56 y −31,73 dBFS, respectivamente, ambos cerca de 11,6 ms. Esto es una observación de dos escenarios, no un pass ni una medida del plugin completo. Investigar el transitorio y ampliar los casos antes de inferir fidelidad general.

## Investigación del transitorio de 11,6 ms

El pico se concentra en el ataque y el error disminuye después de ~14,5 ms. Un experimento aislado que reproduce en el piloto la agenda del harness —aplicar de golpe antes de cada bloque todos los ticks de 1 ms que le corresponden— reduce el error RMS izquierdo de `chord` de −65,44 a −83,56 dBFS y el pico de −31,56 a −56,68 dBFS; en `white_noise` el RMS pasa de −64,72 a −83,21 dBFS. Esto indica que la diferencia dominante en estos dos casos es la agenda de la envolvente de filtro de la referencia. Esa agenda anticipa hasta casi 128 muestras y es una simplificación del shim, no una medición del loop de la Teensy. No se incorpora al plugin: el spec exige eventos en su offset y ticks de 1 ms independientes del bloque del host.

Sí se adapta la envolvente de filtro del piloto para que avance exactamente a 1 kHz mediante `AnalogEnv::update`, igual que el engine, en lugar de evaluarla por muestra y leerla a 1 kHz. La VCA sigue avanzando por muestra. Con la agenda temporal del plugin el pico de los casos medidos no desaparece; es necesario mejorar el método de referencia y medir el hardware antes de atribuirlo a una diferencia audible real.

## Ampliación del corpus digital

El comparador incorpora las ocho escenas existentes: acorde, PWM, glide, robo de voz, HPF, chorus II, chorus I+II y ruido. En PWM, el firmware del harness recibe una senoide de 1 Hz con profundidad 0,5; el piloto usa su LFO seno con los mismos valores, pero el primero actualiza antes de cada bloque y el segundo a 1 kHz durante el audio. En glide y robo de voz se reproducen las mismas notas y bordes de bloque del harness. La escena glide original introduce la nota nueva en una voz libre, por lo que no demuestra por sí sola un deslizamiento en voz reutilizada; se conserva como referencia histórica y se necesita otra escena para cubrir ese caso. Los resultados siguen siendo diagnósticos, sin umbrales ni comparación física.

Resultado local del corpus ampliado (canal izquierdo, sin alineación): error RMS acorde −65,51 dBFS, PWM −53,14,
glide −63,86, robo de voz −60,71, HPF −68,89, chorus II −65,52, chorus I+II −66,45 y ruido −64,80 dBFS. PWM
presenta un pico de −23,85 dBFS en la muestra 134: el harness configura `setPulseWidth(0.35)` como valor de control,
pero el DCO del firmware conserva 0,5 durante el primer bloque y recibe 0,35 en el siguiente `update`; el piloto usa
0,35 desde la primera muestra. Es una diferencia identificada en la preparación del escenario, no una medición del
hardware. Los resultados no constituyen criterios de aprobación ni cubren el plugin completo.

## Casos adicionales: glide en voz reutilizada y PWM inicializado

Se conservan las ocho referencias históricas y se agregan dos sin alterar sus hashes. `glide_voice_steal` ocupa las seis voces, activa glide de 180 ms y dispara una séptima nota para reutilizar la voz más antigua. `pwm_initialized` aplica un tick de control de preparación antes del primer bloque del firmware, de modo que el DCO comience con el ancho de pulso 0,35 como el piloto; la senoide y los ticks posteriores siguen ejecutándose con agendas diferentes. Este tick artificial prepara el escenario y no modela el arranque físico ni demuestra igualdad de señales. Los criterios siguen siendo diagnósticos y se prueban integridad, duración y reproducibilidad.

## Diagnóstico reproducible del glide con voz reutilizada

La comparación normal de `glide_voice_steal` da error RMS izquierdo −32,25 dBFS; tras la séptima nota, el harness adelanta varios ticks de glide antes de procesar el bloque y el piloto los ejecuta a medida que transcurre el audio. Un experimento de diagnóstico genera una copia temporal del piloto con esa agenda anticipada, sin editar el plugin. En la misma máquina y con la misma referencia el error baja a −78,07 dBFS. Esto apoya que la agenda del shim domina la diferencia de este escenario, pero no determina la agenda real del hardware ni acredita fidelidad A/B. El experimento usa 44,1 kHz y bloques de 128, y no reproduce toda la temporización del LFO. El plugin conserva MIDI por offset y ticks reales de 1 ms según la spec.

## CC MIDI de macros Juno

Juno recibe CC 14 DRIFT, 15 GRIT, 16 POCKET y 19 MOVE, igual que el sketch Studio. El evento cambia el DSP en su offset dentro del buffer; el valor se entrega al parámetro del DAW en el hilo de mensajes. Mientras esté pendiente de entrega, el guardado de estado y la exportación GFP deben recoger el último valor. Las pruebas cubren separación antes/después del evento, restauración inmediata y que un preset nuevo descarte CC pendientes. Los controles GUI y automatización del host siguen usando sus IDs existentes.
