# Prototipo VST3 · Prophet mínimo

> 2026-10-29 · En curso · Dirección de Juan: avanzar con el VST y refinar los A/B con hardware más adelante.
> Base: `specs/gemelo-plugin-v1.md`, `theory/gemelo-plugin-juce.md`, ADR-006.

## Objetivo y teoría

Una primera instancia JUCE recibe notas MIDI y entrega audio estéreo al DAW usando los osciladores PolyBLEP
`Prophet5OscCore`, el filtro CEM3320 `LadderStageCore` y las envolventes `AnalogEnv` existentes. Los osciladores calculan
fase `frecuencia / fs`; el filtro recalcula sus coeficientes a la frecuencia del host. Cada voz contiene su propio
estado y las instancias no comparten estado mutable. La GUI inicial expone controles básicos mediante JUCE.

Este corte es un **prototipo para comprobar la integración del DAW**, no es aún el Prophet completo del Brain: no
interpreta GFP, no incluye POLY-MOD completo, voice spread, unison, macros, cadena Studio ni HYBRID. El pegamento de
voz del prototipo es temporal: el objetivo sigue siendo extraer el control completo del firmware a una única fuente,
sin prometer fidelidad mientras no se haya integrado y medido. No se reemplazan algoritmos existentes por DSP de JUCE.

El suavizado interno del ladder se ajustó a fs (hallazgo de revisión), conservando su valor histórico a 44,1 kHz;
esto no demuestra la equivalencia completa del instrumento entre frecuencias.

**Implementación siguiente:** el suavizado del ladder se calcula en función de fs conservando exactamente el valor
histórico a 44.100 Hz. Se valida la evolución temporal ante un salto de cutoff a 44,1/48/96 kHz y se compara el
golden del Prophet. Esto mide el comportamiento DSP; no sustituye el A/B físico pendiente.

## Contrato y demo

- VST3 de instrumento con salida estéreo; MIDI note-on/off con offset dentro del callback. Cinco voces, robo de la
  voz más antigua, sin asignaciones en el proceso de audio. 44,1/48/96 kHz y bloques de tamaño variable.
- Filtro, resonancia, ataque y release automatizables y guardados por instancia. Parámetros y preset incluidos en
  el estado del proyecto. `releaseResources` y re-prepare limpian voces; no hay buffers compartidos entre instancias.
- Orden del demo: abrir dos instancias, tocar distintas notas, automatizar cutoff, guardar y reabrir. El editor inicial
  es funcional; el diseño visual definitivo de Studio se implementa en un hito posterior.
- Tests nativos cubren silencio inicial, nota audible, release, coherencia ante bloques divididos y aislamiento de
  instancias. La compilación VST3 y validación en un host son pruebas separadas. Un hash igual al Brain no se espera.
- A/B por engine, rendimiento on-device y completa fidelidad permanecen pendientes, como pidió Juan.

### Interpretación MIDI del piloto

El firmware Studio usa sustain global CC 64 (≥64 encendido) y pitch bend global de ±2 semitonos, independientemente
del canal (`sketches/49-studio-brain.cpp`, `pitch_bend`, `release_sustained`, `midi_cc`). El prototipo acepta ambos
con el offset MIDI del host: el pedal retrasa note-off hasta soltarse; volver a tocar una nota sostenida cancela su
release pendiente. El pitch bend cambia el incremento de fase sin reiniciar el oscilador. Esto no implementa todavía
MPE ni mod wheel, y no acredita equivalencia del engine entero.

### Voz Prophet del piloto

El siguiente incremento usa los mismos `VoiceSpread` deterministas del Prophet físico (±4 cents y ±6% cutoff),
asignación rotativa, curva de velocity 65% amplitud / 50% filtro y `AnalogEnv` para la envolvente del filtro. El
control de filtro se ejecuta con un acumulador de 1 kHz anclado a muestras. La conversión de control sigue siendo
un adaptador provisional del plugin: el refactor definitivo deberá compartir esa lógica con el firmware.

Los cálculos de profundidad de velocity y cutoff base del envelope se extraen a `prophet_control_core.h` y los llaman
tanto `Prophet5` como el prototipo. Se conserva el orden de operaciones para proteger el golden de 44,1 kHz. No se
comparte todavía la administración completa de voces, POLY-MOD ni el scheduler.

### Presets GFP

El archivo GFP contiene mucho más que los cuatro controles actuales: páginas OSC/MOD, envelopes, velocity, GROOVE y
SYNTH FX. El primer incremento reutiliza el decodificador y banco de fábrica `preset_bank.cpp` del firmware, en una
biblioteca nativa, y rechaza datos no finitos sin alterar el resultado anterior. Se prueban formato actual y rechazo
de archivo truncado/engine equivocado/NaN. El browser del VST no se habilita hasta aplicar los campos necesarios:
aceptar un archivo y omitir parámetros sin avisar confundiría una aproximación con el preset real.
El mapeo de las páginas OSC/MOD del Prophet se extrae a una función compartida que aplica setters sobre un engine;
el sketch 49 usa esa misma función. Su test utiliza un receptor de parámetros y verifica forma, unidades y destinos,
sin hacer depender al VST de AudioStream. El piloto implementa esos setters para que los presets puedan modificar
formas, mezcla, sync, POLY-MOD y WHEEL-MOD. Se prueba el audio generado a partir del patch; LFO y SYNTH FX quedan
pendientes y se debe mostrar esa limitación antes de exponer presets en la interfaz.

El LFO libre reutiliza `dsp::Lfo` a 1 kHz. Modula pitch (curva cuadrática del firmware), cutoff y PWM; WHEEL-MOD mezcla
el LFO con ruido según el patch. Esta etapa no implementa aún sincronización al transporte ni la contribución de tape.

El mapeo del patch para envelopes AMP/FILTER, cantidad de filtro, glide, drive y velocity también se comparte como
función común entre sketch y piloto. El sketch aplica primero las macros existentes; el test del piloto prueba un
patch directo sin afirmar que ya procesa GROOVE o efectos.

El adaptador `apply_prophet_preset_preview` conecta el objeto GFP decodificado con OSC, envelopes, macros de patch y
LFO libre. Devuelve indicadores de FX, GROOVE y DRIFT pendientes. La interfaz permite importar como preescucha y
guarda el patch en el estado del DAW, pero no lo anuncia como fiel.

### Carga y estado del preset en el piloto

La importación GFP se realiza fuera del hilo de audio, valida engine/versión/datos y guarda el contenido binario
completo en el estado del proyecto, no una ruta ni un slot. Un buzón de bytes atómicos acotado entrega el último
patch preparado al callback sin locks ni reserva dinámica; si una escritura coincide con la lectura, el audio espera
al próximo bloque. Las cuatro perillas permanecen automatizables y sobrescriben su parte del patch, con valores
iniciales derivados del preset y sus macros. Una carga inválida deja intactos el patch y los parámetros actuales.
La interfaz identifica la carga como **preview**, porque SYNTH FX y GROOVE están parcialmente implementados y drift
sigue pendiente.

El control `LFO SYNC` usa BPM del playhead del DAW, con 120 BPM de respaldo cuando no hay tempo válido; la división
proviene del GFP. El LFO del Brain corre libre y cambia su velocidad al sincronizar: esta etapa sincroniza velocidad,
no afirma alinear fase en seek o loop. El switch y el patch completo se guardan por instancia.

Primera parte de SYNTH FX: el bus del Prophet pasa por `OutputStageCore` (acople de 8 Hz y saturación) y, si el GFP
lo habilita, por `DriveCore` con tipo, amount, tone y mix del preset. Ambos son los núcleos C++ del Brain y reciben
la frecuencia del host. El piloto todavía procesa el bus mono antes de duplicarlo al estéreo; no incluye la cadena
completa ni afirma equivalencia con las conversiones PCM16 del grafo físico. Los incrementos de TAPE y PUMP se
describen a continuación; chorus/phaser, delay y reverb siguen pendientes.

TAPE usa `dsp::TapeCore` con drive y mix del GFP. Wow/flutter y su modulación del filtro/pitch se describen en el
incremento TAPE más adelante.

PUMP reutiliza `dsp::pump_gain`: con posición de beats y tempo del DAW calcula un objetivo a 1 kHz y suaviza la
ganancia por muestra; cuando el transporte se detiene vuelve gradualmente a unidad. Si el host no comunica posición,
el piloto conserva una posición interna continua a 120 BPM o al BPM disponible. Se trata de una adaptación del reloj:
la fase en seek/loop y el comportamiento frente al jitter de la Teensy se verifican en un host y en el A/B posterior.

ARP reutiliza `dsp::ArpCore` y el mismo mapeo del GFP. Sus eventos usan el reloj musical del host a 1 kHz; el pedal
sustain retrasa la liberación de las teclas alimentadas al arpegiador.

### Incremento CHORD y STRUM: diseño y validación

La fuente de intervalos es `dsp::ChordMemory`, también usada por el sketch 49: modo memoria con su acorde inicial,
o tríada diatónica según tonalidad e inversión. La ruta es tecla → CHORD → ARP → voces; cuando ARP está activo el
firmware entrega el acorde completo al arpegiador sin retrasos STRUM. En la ruta directa, STRUM retrasa cada voz por
un múltiplo de milisegundos y se atiende en el reloj de control de 1 kHz. Un note-off antes del ataque cancela la
nota pendiente; sustain conserva la tecla raíz y su acorde hasta liberar el pedal. La memoria aprendida en hardware
no forma parte del GFP, por lo que el VST usa la memoria inicial del core y no pretende recuperar acordes aprendidos.
La administración de raíces y notas se mantiene acotada y sin asignaciones en el callback. Se comprueba audio de
acorde, STRUM cancelado, liberación con sustain y la composición CHORD + ARP; esto no sustituye un A/B con hardware.
Las notas compartidas entre raíces se conservan hasta que se suelta la última raíz que las usa; esto evita cortar una
nota prematuramente, y es una diferencia de gestión que debe evaluarse frente al firmware durante el A/B.

### Incremento UNISON Prophet: diseño y validación

El Prophet del Brain asigna hasta cinco voces por nota; la opción de seis se recorta a cinco. El núcleo compartido
`unison_ratio` reparte el ancho de desafinación en cents simétricamente, y `unison_gain` compensa el bus con 1/√N.
Disparar otra nota consume voces disponibles y, al agotarlas, sustituye las más antiguas; al soltarla se liberan
todas las voces de esa nota. La ganancia se aplica junto a PUMP, después de SYNTH FX, como en el sketch 49. Se prueba
el cambio audible, el recorte a cinco voces, el estado GFP, la liberación y la interacción con sustain; la medición
de fidelidad y CPU del hardware queda para el A/B posterior. Referencia: `theory/09-studio-groove.md` §5 y
`dsp/voice_spread.h` del firmware.

### Incremento DRIFT Prophet

El Brain suma a las diferencias fijas de cada voice card una variación por disparo: `dsp::Drift` produce hasta
±12 cents de pitch y ±5 % de cutoff, escalados por la macro. Cada voz de UNISON obtiene dos valores propios;
con la macro en cero no se avanza el generador ni se altera el sonido. El plugin reutiliza el generador y el orden
de las llamadas del Prophet. Las pruebas comparan macro neutra y activa, reproducibilidad por instancia y su
aplicación desde GFP; la evolución temporal y fidelidad física se contrastan después con el hardware.
El `reset` del plugin restablece la semilla inicial para que dos renders con los mismos eventos sean reproducibles;
esta política de reinicio se verificará también en la comparación con el firmware.

### Incremento TAPE: wow y flutter

El firmware combina dos osciladores lentos de 0,5 Hz para wow y dos de 6 Hz para flutter. Su resultado modifica
el cutoff de cinta y una pequeña contribución de pitch del engine. Se extrae esa operación a un núcleo compartido
entre `TapeSaturate` y el VST; el plugin lo actualiza a 1 kHz con tiempo derivado de muestras. Se conservan el umbral
de actualización del filtro del 1 % y el escalado de pitch del sketch 49. Se prueba la salida frente a controles
neutros, continuidad entre bloques y datos de presets. El jitter de control físico y la equivalencia acústica quedan
para el A/B. Referencia: `apps/docs/sprints/08-tape-saturate.md` y `fx/tape_saturate.cpp`.

### Incremento PHASER estéreo

El TYPE PHASER de la ranura CHORUS usa dos instancias del `dsp::PhaserCore` del Brain, con fase derecha desplazada
un cuarto de ciclo. El núcleo mantiene su actualización de coeficiente cada 16 muestras entre llamadas, para que
la partición del host no cambie el resultado; los bloques de 128 muestras del firmware conservan esa cadencia.
El PHASER se inserta después de TAPE y antes de los efectos aún pendientes, y aplica el MIX del GFP por canal.
La prueba verifica estéreo, parámetros GFP y continuidad de partición; no sustituye el A/B físico. Referencias:
`theory/fase3-fx.md`, `audio/phaser_stage.h` y `dsp/phaser_core.h`.

### Incremento CHORUS

El CHORUS Studio usa el `AudioEffectChorus` oficial PJRC (dos taps fijos en una línea efectiva de 600 muestras),
precedido de waveshaper y mezclado con un LFO de amplitud custom. Se adapta el cálculo de taps y lookup PCM a un
núcleo portátil con atribución de licencia; el reloj del LFO custom se avanza a 1 kHz. Los canales disponen de
estado propio como el sketch 49; con entrada mono no se fuerza una anchura estéreo que el firmware no produce.
Las pruebas cubren señal, bypass, preset y partición; cuantización, jitter y A/B físico quedan por verificar.

### Incremento DELAY estéreo

El delay reutiliza `dsp::PingPongDelayCore` del Brain: líneas PCM16, realimentación cruzada, TONE, MIX y división
musical del GFP. El DAW reserva una línea por canal al preparar la instancia y el callback no reserva memoria.
El tempo del transporte actualiza el tiempo, que el núcleo suaviza por muestra. Se insertan los ecos después de
chorus/phaser y antes de PUMP. Reset y cambio de preset limpian las líneas para no reproducir colas del patch anterior;
se mide posteriormente el coste de ese borrado en sesiones con múltiples instancias. La prueba valida eco estéreo,
sincronización de tiempo y restauración GFP; la cuantización del grafo completo se compara en el A/B.

### Incremento REVERB PLATE

El modo PLATE del Brain usa `AudioEffectFreeverbStereo` oficial de Teensy, con ocho combs y cuatro allpass por canal.
Se adapta su cálculo de punto fijo, y las longitudes de líneas se escalan con la frecuencia del host para mantener
los tiempos a 44,1, 48 y 96 kHz. El host reserva las líneas al preparar la instancia; el procesamiento no reserva.
La señal de entrada es la media de los canales después del delay y el mezclador aplica los niveles del sketch 49.
Se comprueban cola, estéreo, bypass y GFP; el resultado a tasas distintas y la cuantización deben contrastarse contra
el hardware. Referencia: `audio/plate_reverb.h`, `sketches/49-studio-brain.cpp` y `effect_freeverb.cpp` de PJRC.

### Incremento REVERB MODAL

Los cuatro materiales se calculan con los seis modos y el T60 de `ModalReverb` del Brain, compartiendo la tabla
de parámetros. Cada canal usa seis biquads pasa banda con los coeficientes de `AudioFilterBiquad::setBandpass`,
pero procesados en float por el VST; por ello el ruido de cuantización PCM16 y la saturación de los mixers PJRC no
son idénticos. MIX y los parámetros DECAY/TYPE/SIZE siguen el sketch 49. Se prueban los cuatro materiales, colas,
bypass y selección GFP. El nivel y la fidelidad frente al grafo fijo quedan para A/B con el hardware. Referencia:
`fx/modal_reverb.cpp` y Rossing, *The Science of Sound*, cap. 18.

## Referencias

- Código del Brain: `src/engines/prophet_osc_core.*`, `src/dsp/ladder_stage_core.h`, `src/dsp/analog_env.h`,
  `src/engines/prophet_5.cpp`; documentación del filtro y envelopes en `theory/fidelidad-engines.md`.
- [JUCE AudioProcessor](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html),
  [JUCE CMake API](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md).
- Välimäki y Huovilainen, “Antialiasing Oscillators in Subtractive Synthesis”, IEEE SPM, 2007;
  Zavalishin, *The Art of VA Filter Design*, 2018.

## Estado de la primera compilación

- JUCE 8.0.3 local de GroovePilot, commit `5179f4e720d8406ebd1b5401c86aea8da6cc83c9`; build VST3 de macOS arm64.
- El editor inicial muestra cutoff, resonancia, ataque y release, con tokens oscuros/teal de Studio. No es aún el
  diseño completo de SYNTH ni A/B de HYBRID.
- Tests CTest de DSP y procesador: silencio, nota/release, tres sample rates, instancias independientes, MIDI con
  offset, salida estéreo, estado y creación del editor. La prueba automatizada no sustituye una sesión en Live.
- Bundle construido y firmado localmente ad-hoc para desarrollo; falta escanearlo y tocarlo en Ableton. No se instaló
  en la carpeta global del sistema ni se ha distribuido.
- Sigue pendiente el control completo compartido Prophet, los seis engines, presets/FX y HYBRID. La comparación A/B
  y la CPU física se hacen después, según la instrucción de Juan.
- Incremento posterior: filtro con suavizado dependiente de fs, sustain MIDI, pitch bend y voz Prophet con spread,
  envolvente de filtro y rotación. Velocity y cutoff base del envelope comparten `prophet_control_core.h` con Teensy.
  La suite nativa pasó **156/156**, `sketch49` compiló y las seis referencias digitales coincidieron byte a byte en
  este Mac. CTest verificó el DSP y el procesador JUCE, incluido sustain, bend y bloques variables. La instalación
  global anterior sigue siendo la versión previa; el build actualizado está en `apps/brain-plugin/dist/next/`.
- Presets Prophet: codec GFP 2–5 reutiliza firmware y pasa pruebas con banco de fábrica, migraciones y errores. Los
  mapeos de OSC/MOD y envelopes son compartidos con Studio. El plugin importa GFP, guarda su contenido en el DAW,
  sincroniza velocidad del LFO al BPM y aplica PUMP, etapa de salida, DRIVE y saturación básica de TAPE. Sigue siendo
  preescucha: faltan un navegador de banco, SYNC de fase y los demás FX. CHORD/STRUM ya usa
  `ChordMemory`, con pruebas de liberación, sustain y composición con ARP.
