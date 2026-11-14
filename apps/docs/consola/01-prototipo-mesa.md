# Consola: plan del prototipo de mesa

> **Fecha:** 2026-11-09. **Estado:** plan para aprobar. **Base:** `00-concepto.md` (§8 riesgos P1–P5, §10 refinamiento). **No modifica** `00–06`. Sin PCB ni caja: todo en protoboard y cables, con lo que Juan ya tiene.

## 1. Objetivo

Responder con mediciones, antes de diseñar placas, si la consola es viable con un Raspberry Pi 5 y **una sola** Teensy, y cómo se siente. **Hito demostrable (regla de fase):** el Pi genera dos opciones de batería con el motor; Juan escucha la siguiente por **cue** en audífonos, la **acepta** con un pad, y entra en la sala al **inicio de la siguiente frase de 8 compases**.

## 2. Material

| Tiene | Uso en el prototipo |
|---|---|
| Raspberry Pi 5, 8 GB | Motor, sampler, UI |
| Pantalla táctil 7" 1280×720 | UI (desde E5; antes basta SSH) |
| Raspiaudio Audio+ V2 | Salida de **sala** |
| Teensy 4.1 + Audio Shield (SGTL5000) | Brain (synths) + **cue** + entrada + superficie |
| NeoTrellis 8×4 | Pads |
| FSR 402 | 1 para la prueba de velocity |
| Encoders, faders, botones | 2 encoders, 1 fader, 2 botones |
| Audífonos | Cue |

| Falta (barato) | Para qué |
|---|---|
| Fuente oficial del Pi 5 (27 W, 5,1 V / 5 A) | Sin ella, los USB quedan limitados a 600 mA y ahí va la Teensy |
| microSD de 32 GB o más | Raspberry Pi OS de 64 bits |
| Resistencias de 10 kΩ, protoboard, cables dupont | FSR y controles |
| Cable 3,5 mm a 3,5 mm (o a RCA) | Bucle de medición: salida del Audio+ → entrada de línea del SGTL5000 |

**Teensy:** se flashea conectada directo al Mac y luego vuelve a su aislador USB (regla de Juan). Conectada al Pi va directo: el Pi y la consola comparten la misma tierra.

## 3. Etapas

Cada etapa termina con **pasa / no pasa**. Si una no pasa, se aplica su plan B antes de seguir.

### E0 · Preparar el Pi (≈ 1 sesión)

1. Raspberry Pi OS de 64 bits, actualizado; SSH activado.
2. Activar el Audio+ V2 (overlay de I2S según la guía de Raspiaudio; **a verificar** cuál usa la V2) y comprobar con `aplay -l` y un tono de prueba.
3. Instalar `uv` y clonar el repo.

**Pasa si:** el tono suena limpio por el Audio+.

### E1 · El motor en el Pi (P1) (≈ 1 sesión)

1. `uv sync` en `tools/motor` y `tools/perc-synth`; correr las dos suites de tests.
2. Medir cuánto tarda en generar: una tanda de kicks, un rack de drums completo, los patrones de bajo y un subdrop.
3. ~~Abrir la web del motor desde el Mac~~: descartado. En la consola la interfaz es la app de Qt, que habla con el motor en el mismo Pi.

**Pasa si:** los tests pasan en ARM y generar una tanda de drums tarda menos de unos 10 s (en directo se genera con anticipación, no en el golpe). **Plan B:** precalcular bancos de sonidos en el Mac y que el Pi solo reproduzca y genere MIDI.

### E2 · Sonar en la sala desde el Pi (≈ 2 sesiones)

1. Escribir un **reproductor mínimo** en el Pi: lee un clip MIDI del motor (16 compases) y dispara los WAV de la drum machine a 122 BPM por el Audio+. Primera versión en Python con PipeWire/JACK; se reescribe en C++ si no alcanza.
2. Dejarlo sonando **30 minutos** con `pw-top` abierto: contar xruns (cortes) con buffers de 256 y 128 muestras.
3. **Latencia:** conectar la salida del Audio+ a la entrada de línea del SGTL5000; la Teensy la devuelve al Pi por USB. Medir el retardo del clic de ida y vuelta (`jack_iodelay` o un script que busca el clic). Ese número incluye ambos lados.

**Pasa si:** 0 xruns en 30 min con buffer de 256 y latencia de ida y vuelta medida (objetivo orientativo: < 20 ms; en directo generativo se tolera más que al tocar notas). **Plan B:** kernel en tiempo real, buffers más grandes, o mover el reproductor a C++.

### E3 · Una Teensy con el Brain conectada al Pi (P3) (≈ 2 sesiones)

1. El firmware actual ya es `USB_MIDI_AUDIO_SERIAL`: el Pi debe verla como tarjeta de audio y puerto MIDI. Comprobar con `aplay -l`, `arecord -l` y `aconnect -l`.
2. **Brain → Pi:** el Pi graba el audio USB del Brain mientras le manda notas por MIDI.
3. **Pi → cue:** el Pi manda una mezcla distinta por el USB de salida hacia la Teensy, que la saca por los audífonos del SGTL5000.
4. Dejar sala (Audio+) y cue (SGTL5000) sonando a la vez **una hora**, escuchando y grabando: los dos relojes son distintos y pueden aparecer clics.

**Pasa si:** el audio del Brain llega limpio al Pi y el cue no tiene clics audibles en una hora. **Plan B:** si el cue tiene clics, un adaptador USB de audio barato para audífonos; si el audio del Brain falla por USB, su salida de línea va a una entrada física.

### E4 · Superficie mínima en la misma Teensy (≈ 2–3 sesiones)

1. Conectar al I2C de la Teensy (el mismo del SGTL5000) las dos placas NeoTrellis (0x2E y 0x2F). Más adelante irán ahí los MCP23017.
2. Un FSR con su resistencia de 10 kΩ a una entrada analógica; 2 encoders, 1 fader y 2 botones.
3. La Teensy manda los eventos al Pi por MIDI USB (pads como notas, FSR con velocity, encoders y fader como CC).
4. **Medir, con todo a la vez** (6 voces, 5 FX, pads y encoders moviéndose, LEDs del NeoTrellis animados):
   - CPU de audio con `AudioProcessorUsageMax()` (presupuesto: ≤ 60 %, `01-architecture.md` §5.2).
   - Clics en la salida del Brain.
   - Tiempo de pad a evento en el Pi.
   - Velocity del FSR: que 10 golpes suaves y 10 fuertes queden en rangos separados.

**Pasa si:** CPU ≤ 60 %, sin clics, pad → evento en menos de 5 ms, y velocity utilizable. **Plan B:** la superficie pasa a la **segunda Teensy** (`00-concepto.md` §10.2.3).

### E5 · El hito: aceptar en la frase (≈ 2–3 sesiones)

1. El motor en el Pi prepara dos opciones de drums (patrón + sonido) sobre el mismo kick.
2. Suena la opción A en sala. La B se escucha por **cue**.
3. Un pad del NeoTrellis = **ACEPTAR**: la B entra al inicio de la siguiente frase de 8 compases. Otro pad = **SALTAR**: el motor prepara otra.
4. Cada decisión se escribe en el registro, como en el estudio.
5. Primera pantalla en el 7": qué suena, qué espera en cue y cuántos compases faltan para la frase.

**Pasa si:** Juan lo usa durante 15 minutos seguidos y quiere seguir. Es el criterio que más pesa.

### E6 · Opcionales, con tiempo acotado

| Prueba | Tiempo | Qué responde |
|---|---|---|
| **Link** con `aalink` entre el Pi y Live | 1 sesión | Tempo y fase compartidos (modo HÍBRIDO) |
| **MIDI por red** (`rtpmidid` en el Pi, red MIDI de macOS) | 1 sesión | Mandar el MIDI por rol a Live sin cable |
| **VST para ARM** (P4): compilar el GrooveForge VST como aplicación standalone para el Pi | 1 día, máximo | Si el Pi puede hacer también los synths y liberar la Teensy |

## 4. Qué se registra

Cada medición queda en `apps/docs/consola/mediciones/` con fecha, configuración (buffer, kernel, versión del firmware) y resultado. Sin número medido no se marca una etapa como pasada.

## 5. Calendario orientativo

Con 10–18 h por semana: **E0–E2 la primera semana, E3–E4 la segunda, E5 la tercera**, y los opcionales cuando haya un hueco. Al final de E5 se decide la arquitectura (una o dos Teensy, quién hace los synths) y recién ahí se dibuja la PCB de la superficie.

## 6. No cubre

Caja, PCB, batería, la UI completa, el secuenciador y el modo ESTUDIO. Todo eso depende de lo que salga de E2–E5.
