# Consola GrooveForge: concepto e investigación

> **Fecha:** 2026-11-09. **Estado:** borrador para refinar con Juan. **No modifica** `00–06` ni los ADR. **Contradicción abierta:** `00-master-strategy.md` plantea el Brain (ESP32 + pantalla redonda) como producto; esta consola es una herramienta personal de producción y directo. Si se adopta, hace falta un ADR y la decisión explícita de Juan antes de tocar la estrategia.

## 1. La idea, en palabras de Juan

> «Producir más rápido… el motor automatiza cosas y me deja dedicarme al proceso creativo de escoger sonidos, organizar y pensar ideas muy rápido. Quiero un hardware específico para esto… para tocar liveset… como un DJ que va mezclando música que se va generando, escogiendo si va o no, ya haciendo lectura de todo lo que llevamos de mi estilo.»

Prioridades, en orden: **(1) producir más rápido, (2) tocar en vivo**, y con eso contenido, imagen y comunidad. **No es para vender** por ahora. Restricción: **un solo dispositivo**, para bajar el costo de PCB, usando lo que ya tiene: Raspberry Pi 5 de 8 GB, pantalla táctil de 7" (1280×720, 10 puntos), dos Teensy 4.1, NeoTrellis de 32 pads, potenciómetros, encoders, faders y botones. Juan pide además un **step sequencer**.

## 2. Qué hay en el mercado y qué se aprende de cada uno

| Equipo | Qué es | Precio aprox. (USD) | Lo que se toma |
|---|---|---|---|
| **Ableton Push 3 standalone** | Live dentro de un controlador: Intel i3-1115G4, 8 GB, batería | 1 799 | Que un controlador de Live puede ser standalone. Pero es x86 y caro; los MIDI Tools generativos de Live 12 no corrían en Push según un vendedor (dato a confirmar) |
| **Akai MPC Live II** | Sampler y secuenciador standalone, pantalla 7", 16 pads, parlantes | 1 299 | El flujo de pads + pantalla táctil, y que también sirve de controlador de DAW |
| **Synthstrom Deluge** | Synth, sampler y secuenciador con 128 pads RGB (16×8) | ~1 529 | La cuadrícula grande como piano roll y step sequencer a la vez |
| **Teenage Engineering OP-XY** | Secuenciador de 64 pasos, synth y sampler, generador de acordes («brain chord») | 2 299 (lanzamiento) | Edición por paso (parameter locks) y progresiones generadas |
| **Torso T-1** | Secuenciador **algorítmico** de 16 pistas: euclidiano, arpegiador, generador de acordes y melodías; **sin sonido propio** | ~699 | Lo generativo en hardware se toca y gusta. Pero sus algoritmos son genéricos: no conocen el estilo de nadie |
| **Zynthian V5** | Plataforma abierta **sobre Raspberry Pi** (V5.1 para Pi 5): 50 motores, cientos de efectos, mezclador, pantalla 5", 20 botones RGB, 4 encoders, DAC PCM5242 | Kit 490 € sin Pi / 570 € con Pi (V5) | **Prueba de que un Pi corre un instrumento completo en directo.** Código abierto del que se puede aprender (motores, mezclador, audio en Linux) |
| **Critter & Guitari Organelle M** | Instrumento con Pi (CM3+) que corre patches de Pure Data | ~595 | Que una caja simple con Pi y pocos controles es un instrumento serio |

**El hueco:** ninguno genera **desde el estilo de quien lo toca**. El T-1 genera, pero con reglas genéricas. Push y MPC tienen sonido y pads, pero no proponen. La consola juntaría las tres cosas: **genera con tus reglas y tu historial, suena, y tú decides en vivo qué entra**.

## 3. Arquitectura propuesta (un solo dispositivo)

```
            pantalla 7" táctil (UI: opciones, cuadrícula, mezcla)
                         │
superficie ─USB─ Teensy B ─┐
(pads, encoders,           ├─ Raspberry Pi 5 ── interfaz de audio (4 salidas: SALA + CUE)
 faders, botones, LEDs)    │   · motor (reglas → IA local)
Brain ─USB audio + MIDI─ Teensy A   · sampler (drums propios, loops, subdrop)
(synths HYBRID, FX)                 · mezclador y decisiones
MIDI IN/OUT · Link ────────────────┘   · registro (aprende de cada set)
```

| Pieza | Rol | Reutiliza |
|---|---|---|
| **Raspberry Pi 5 (8 GB)** | Cerebro: motor, UI, sampler, mezcla, registro | `tools/motor`, `perc-synth`, presets, racks; la experiencia de `studio-head` (LVGL en Pi) |
| **Teensy A** | Motor de sonido del Brain: synths, capas HYBRID y FX. Envía su audio al Pi por **USB audio** (2 canales por defecto en la Audio Library) | Todo `firmware-teensy` |
| **Teensy B** | Lee la superficie y maneja los LEDs; habla con el Pi por USB | Código de UI del Brain (encoders, debounce, LEDs) |
| **Interfaz de audio** | 4 salidas: estéreo de sala y estéreo de **cue** para audífonos, como en un mixer de DJ | Una interfaz USB compatible de clase, o un HAT (opción a estudiar) |

**Alternativa a estudiar:** compilar el motor del VST (JUCE) para Linux ARM y que el Pi haga también los synths. Ahorraría el Teensy A, pero hay reportes de problemas (un AudioPluginHost headless de JUCE 8 que fallaba en un Pi 5) y de compilación lenta en el Pi. Por eso el plan base mantiene el Teensy.

## 4. Modos

| Modo | Para qué | Qué hace |
|---|---|---|
| **ESTUDIO** | Producir más rápido | La consola controla el motor y Live en el Mac: elegir opciones, cuadrícula del armado, capas, mezcla de partida. El Mac hace el trabajo pesado |
| **LIVE** | Set en vivo, sin laptop | El motor genera en el Pi y tú mezclas lo que va entrando (§5) |
| **SECUENCIADOR** | Escribir y editar a mano | Step sequencer de 16 pasos por carril; el motor rellena o varía lo que pidas |

## 5. El set en vivo: un DJ de música generada

1. Por cada rol (kick, drums, perc, bajo, acordes, lead, pad), el motor tiene **la siguiente opción lista**, generada con tus reglas y tus registros.
2. La escuchas **por cue** antes de que suene en la sala.
3. **Acepta o salta** con un botón. Si aceptas, entra al **inicio de la próxima frase** (8 o 16 compases).
4. Los faders mezclan grupos; un botón por sección pide **cambio**, y el motor pone la transición: roll, riser, fill de llamador, subdrop.
5. Todo lo que aceptas o saltas va al registro: la consola **aprende de tus sets**, no solo del estudio.

## 6. Step sequencer

Una cuadrícula de pads funciona como en Deluge o MPC: cada fila es un carril (instrumento o nota) y cada columna un paso. La diferencia: **el motor puede escribir en ella**. Le pides «un fill de llamador» o «otra clave», y aparece en los pasos, editable. Por paso: velocity, microtiming y probabilidad (como el parameter lock del OP-XY), para que los patrones respiren como tus ghosts.

## 7. Cuántas PCB

De todo lo pendiente (portadora, PCB-A, PCB-K, filtro, Push, mezclador) a lo mínimo:

| PCB | Contenido |
|---|---|
| **Superficie** (nueva) | Pads o botones, encoders, faders, LEDs y Teensy B |
| **Brain** | Teensy A con su audio: se evalúa reutilizar la **portadora** y la **PCB-A** ya diseñadas para Studio |

El filtro analógico externo, el mezclador tipo Xone y el Push separado quedan fuera de esta etapa.

## 8. Riesgos y cómo probarlos antes de fabricar (prototipo de mesa)

| # | Pregunta | Prueba con lo que ya hay | Si sale mal |
|---|---|---|---|
| P1 | ¿El motor corre bien en el Pi 5? | Instalar `tools/motor` y generar opciones | Optimizar o mover partes a Teensy |
| P2 | ¿Latencia de audio aceptable? | Bucle de retorno con la interfaz y `jack_iodelay` o `pw-top`. No hay cifras públicas fiables para Pi 5: hay que medir | Buffer más grande en LIVE (lo generativo tolera más latencia que tocar notas) |
| P3 | ¿El audio del Brain llega bien al Pi por USB? | Teensy A como interfaz USB de 2 canales | Salida I2S o analógica del Teensy a la interfaz |
| P4 | ¿El VST compila para ARM? | Compilación cruzada del VST en modo standalone | Se queda el Teensy A |
| P5 | ¿Los controles se sienten bien? | Teensy B con los encoders, faders y potes que ya tienes, sin PCB | Cambiar la distribución antes de diseñar la placa |

**Regla de fase (CLAUDE.md):** sin demo audible no se avanza. El primer hito sería: el Pi genera un patrón con el motor, el Teensy lo hace sonar y un botón acepta la siguiente opción en la frase correcta.

## 9. Lo que falta decidir con Juan

1. **Tamaño y forma:** ¿mesa de DJ (horizontal, grande) o caja portátil?
2. **Superficie:** ¿cuántos pads (16, 32, 64), sensibles a velocity o no? ¿Cuántos faders y para qué grupos?
3. **Conexiones:** ¿MIDI DIN para e-drum o marimba? ¿Ableton Link? ¿CV para el filtro externo?
4. **Mezcla:** ¿digital dentro del Pi (lo planteado) o una sección analógica?
5. **Audio:** ¿interfaz USB externa o un HAT dentro de la caja?
6. **Tribe en vivo:** no corre en el Pi. ¿Se reemplaza con muestras propias, o se graban sus patrones como audio?
7. **Batería:** ¿sí o no?

## 10. Refinamiento (2026-11-09)

Respuestas de Juan: pantalla **7" 1280×720 táctil de 10 puntos**; Raspberry Pi 5 de **8 GB**; forma **lo más portátil posible, estilo MPC o Push 3**; pads: **NeoTrellis de 32** (8×4) y el resto botones; quiere **varias conexiones, Ableton Link incluido**; Tribe: **sacar sus samples**; audio: tiene un **HAT con un PCM** para el Pi y el **SGTL5000** del Teensy.

### 10.1 Audio: los dos, cada uno con su trabajo

El cue exige **dos salidas estéreo independientes** (sala y audífonos). Ninguno de los dos solo las da: el SGTL5000 tiene salida de línea y de audífonos, pero las dos salen del **mismo DAC**, con volúmenes separados y la misma mezcla. Propuesta:

| Salida | Por dónde | Por qué |
|---|---|---|
| **SALA** (main) | HAT del Pi | Directo del Pi, sin pasar por USB: la menor latencia y un solo reloj para lo que oye el público |
| **CUE** (audífonos) | SGTL5000 del Teensy A, que recibe la mezcla de cue del Pi por USB audio | Ya tiene amplificador de audífonos. Si la sincronía entre los dos relojes produce algún clic, se oye solo en audífonos |
| **ENTRADA** (e-drum, marimba, voz) | Line-in del SGTL5000 → Teensy → Pi por USB | El Teensy ya envía su audio (el Brain) al Pi; la entrada viaja igual |

**HAT de Juan: Raspiaudio Audio+ V2.** Es **solo salida** (DAC estéreo, 32 bit / 384 kHz, 112 dB de relación señal/ruido). El fabricante no nombra el chip; esas cifras coinciden con las de un PCM5102A, sin confirmar. Sin entrada, el plan de la tabla queda así: Audio+ V2 para **sala**, SGTL5000 para **cue y entrada**. Prueba P3 ampliada: medir clics en el cue con los dos relojes durante una hora.

### 10.2 Superficie con NeoTrellis 8×4

Los pads NeoTrellis son botones de silicona con LED RGB: detectan pulsado o suelto, **sin velocity**. Para el directo y el secuenciador basta; la dinámica se da con un acento por paso o con un encoder. Dos placas 4×4 en I2C (direcciones 0x2E y 0x2F) forman el 8×4, con la biblioteca seesaw de Adafruit.

| Modo | Columnas (8) | Filas (4) |
|---|---|---|
| **LIVE** | Un rol por columna: KICK, DRUMS, PERC, TOP, BASS, CHORDS, LEAD, FX | 1 encendido/silencio · 2 escuchar la siguiente por cue · 3 **aceptar** · 4 saltar u otra variación |
| **SECUENCIADOR** | 8 pasos (con página para 16) | 4 carriles; Shift + pad = acento |
| **ESTUDIO** | Opciones de la pestaña activa del motor | Elegir, poner y navegar |

Botones fijos (propuesta): Play/Stop, Shift, Modo, **Cambio de sección** (build, drop, break), Tap, Cue maestro. Encoders: 8 bajo la pantalla, como en Push (parámetros de lo seleccionado). Faders: 4 por grupos (RITMO, BAJO, MÚSICA, FX), más perillas de MASTER y CUE.

### 10.2.1 Diez FSR 402: pads con fuerza

Juan tiene 10 resistencias sensibles a la fuerza **FSR 402** (Interlink, redondas de ½"). Bajan su resistencia al apretar: empiezan a responder hacia 0,2 N, llegan a unos 20 N y suben en menos de 3 µs. Le dan a la consola lo que el NeoTrellis no tiene:

| Uso | Cuántos | Qué hace |
|---|---|---|
| **Pads con velocity** | 4 (decisión de Juan, 2026-11-09) | Para tocar en vivo kick, perc y golpes de Tribe con dinámica real, o grabar acentos en el secuenciador |
| **Expresión** | 2 | Apretar y sostener: uno abre el filtro o la macro MOVE del Brain; el otro manda un throw al Echo o empuja un riser |

**Cómo se leen:** cada FSR en un divisor con una resistencia fija (≈10 kΩ, a ajustar) hacia una entrada analógica del Teensy B. La **velocity** es el pico de la lectura en los primeros 3–5 ms después de cruzar el umbral; la **presión** sostenida se lee continua, como aftertouch.

**Para que se sientan bien:** un FSR responde distinto según cómo se reparte la fuerza (Interlink). Hace falta un **actuador**: una tapa de silicona o goma con un disco rígido pequeño encima del área activa, sobre una base plana y firme. Cada pad se calibra con su propia curva, porque no son sensores de precisión. Para golpes fuertes y rápidos, como un e-drum, un piezo suele ser mejor; para dedos, el FSR sirve.

### 10.2.2 Botones

**Propuesta: 22 botones con LED**, agrupados por función, más los 8 clics de los encoders. MPC Live tiene una treintena y Push 3 más de cincuenta; con el NeoTrellis haciendo gran parte del trabajo, 22 cubren el directo sin menús.

| Grupo | Botones | Cuántos |
|---|---|---|
| **Transporte** | PLAY, STOP, REC, TAP | 4 |
| **Modo y navegación** | MODO (LIVE / SECUENCIADOR / ESTUDIO), SHIFT, ◀ ▶ ▲ ▼, ATRÁS | 7 |
| **Motor en vivo** | CUE, ACEPTAR, SALTAR, GENERAR (otra tanda), y cambio de sección: BUILD, DROP, BREAK | 7 |
| **Edición** | DUPLICAR, BORRAR, DESHACER, FILL (el motor escribe un fill) | 4 |

**Pines del Teensy B (4.1: 55 digitales, 18 analógicos):**

| Qué | Pines |
|---|---|
| NeoTrellis 8×4 + expansores de botones | I2C: 2 (bus compartido) |
| 22 botones | **2 expansores MCP23017** en el mismo I2C (16 entradas cada uno: quedan 10 libres para crecer) |
| LEDs de los 22 botones | 1 pin: cadena WS2812 bajo tapas translúcidas |
| 8 encoders con clic | 24 digitales |
| 4 FSR de fuerza + 2 de expresión | 6 analógicos |
| 4 faders + perillas MASTER y CUE | 6 analógicos |
| MIDI TRS in/out + sync | 3 digitales |

Quedan unos 25 digitales y 6 analógicos libres, así que el número de botones puede subir sin cambiar de microcontrolador.

### 10.3 Conexiones

| Conexión | Para qué | Cómo |
|---|---|---|
| **Ableton Link** | Tempo y fase con Live, otras apps y otros equipos, por red | En el Pi, con `aalink` (asyncio, GPL-3, tiene builds ARM64) o `LinkPython` |
| **Wi-Fi / Ethernet** | Modo ESTUDIO: hablar con el motor del Mac | El motor ya es un servidor HTTP local |
| **USB host** (Pi) | Controladores, e-drum USB, memoria | Puertos USB-A del Pi |
| **MIDI TRS in/out** | e-drum, marimba, synths | UART del Teensy B, jacks de 3,5 mm |
| **Sync analógico** | Pocket Operator, Volca | Un pulso desde el Teensy B |
| **CV** | Filtro ladder externo | La salida CV del Brain (pin 29) que ya existe |
| **Audio in/out** | §10.1 | — |

### 10.4 Energía y portabilidad

El Pi 5 necesita **5,1 V a 5 A** (fuente oficial de 27 W). Con 3 A, los puertos USB quedan limitados a 600 mA en total, y ahí van los dos Teensy. Muchas baterías USB-C llegan a sus vatios subiendo el voltaje, no entregando 5 A a 5 V. Por eso la primera versión va **con fuente de pared**, y la batería se estudia después, como en el Push 3. Medida orientativa: pantalla de 7" (~15,5 × 8,7 cm), NeoTrellis 8×4 (~12 × 6 cm) y faders: una caja del orden de **32 × 22 cm**, menor que una MPC Live II.

### 10.5 Tribe en el directo

Plan: grabar cada articulación del kit de Tribe a varias velocities y cargarlas en el sampler del Pi con **el mismo mapa de notas** de `kits/tribe_colombia.json`. Así los patrones y fills que ya escribe el motor suenan igual en la consola. **A revisar:** la licencia de Tribe. La ficha del kit marca «samplear: revisar licencia»; el uso en sets propios suele estar permitido y la redistribución no, pero hay que leer su EULA.

## Fuentes

- Push 3: [Ableton, especificaciones](https://www.ableton.com/en/push/tech-specs/), [MusicRadar](https://musicradar.com/news/ableton-push-3-standalone-live), [Sweetwater](https://www.sweetwater.com/store/detail/Push3WP). MIDI Tools y Push 3: [Manifest Audio](https://manifestaudio.gumroad.com/l/midi-toolset-001).
- MPC Live II: [Dale Pro Audio](https://daleproaudio.com/products/akai-mpc-live-2-drum-machine-workstation-with-built-in-speakers), [MusicRadar](https://www.musicradar.com/news/akais-mpc-live-ii-has-built-in-monitors-for-true-standalone-music-making).
- Deluge: [Synthstrom](https://synthstrom.com/product/deluge/), [Perfect Circuit](https://perfectcircuit.com/synthstrom-deluge.html), [MusicTech](https://musictech.com/news/gear/synthstrom-audible-deluge-groovebox-new-screen).
- OP-XY: [MusicTech](https://musictech.com/news/gear/teenage-engineering-op-xy-2/), [MusicRadar, reseña](https://www.musicradar.com/music-tech/synths/teenage-engineering-op-xy-review).
- Torso T-1: [Juno, reseña](https://www.juno.co.uk/junodaily/2024/09/06/torso-electronics-t-1-review), [Synthtopia](https://www.synthtopia.com/?p=135938).
- Zynthian: [Synth Anatomy (V5)](https://synthanatomy.com/2023/07/zynthian-v5-next-gen-of-the-raspberry-pi-based-hardware-plugin-host.html), [blog (V5.1 para Pi 5)](https://blog.zynthian.org/2024/09/zynthian-v5-1-kit-released/), [MusicRadar](https://www.musicradar.com/news/zynthian-5-linux-device).
- Organelle: [Engadget](https://www.engadget.com/2019/07/16/organelle-m-portable-hackable-music-computer-hands-on/), [Synthtopia](https://www.synthtopia.com/content/2017/12/29/critter-guitari-organelle-the-beauty-of-open-source-music-software/).
- Audio en Raspberry Pi: [Linux Audio Wiki](https://wiki.linuxaudio.org/wiki/raspberrypi), [Pisound, especificaciones](https://blokas.io/pisound/docs/specs/), [Pisound Micro](https://blokas.io/pisound-micro/docs/detailed-specs/).
- JUCE en ARM: [Elk, compilar plugins](https://elk-audio.github.io/elk-docs/html/embedded/building_plugins_for_elk.html), [foro JUCE, AudioPluginHost headless en Pi 5](https://forum.juce.com/t/headless-aph-on-raspberry-pi/65054), [foro JUCE, compilación cruzada](https://forum.juce.com/t/crosscompile-to-arm-raspberry/31183).
- Teensy como interfaz USB: [foro PJRC](https://forum.pjrc.com/showthread.php?p=327016), [foro Zynthian](https://discourse.zynthian.org/t/foss-digital-audio-mixer/9881).
- Live 12 y herramientas generativas: [Attack Magazine](https://www.attackmagazine.com/reviews/gear-software/live-12s-midi-tools-midi-revolution/), [Sound On Sound](https://www.soundonsound.com/news/ableton-launch-live-12).
- NeoTrellis: [Adafruit Learn](https://learn.adafruit.com/adafruit-neotrellis), [proyecto en Hackaday](https://hackaday.io/project/186327-neotrellis-explorations).
- Ableton Link en Python: [aalink (PyPI)](https://pypi.org/project/aalink/), [aalink en piwheels](https://www.piwheels.org/project/aalink), [LinkPython](https://github.com/thegamecracks/link-python).
- SGTL5000: [hoja de datos](https://pjrc.com/teensy/SGTL5000.pdf), [mezclador digital con segundo codec para auriculares](https://blog.brixit.nl/digital-audio-mixer-pt-2/), [foro PJRC, salida de línea](https://forum.pjrc.com/threads/66630-What-are-the-Line-OUT-pins-on-Audio-Adaptor-Board-Teensy-3-x-for).
- Alimentación del Pi 5: [CanaKit, fuente oficial 27 W](https://canakit.com/official-raspberry-pi-5-power-supply-27w-usb-c.html), [Yahboom, alimentar el Pi 5](https://www.yahboom.net/public/upload/upload-html/1703664332/3.%20Powering%20the%20Raspberry%20PI%205.html).
- FSR 402: [Interlink, guía de integración (MIT)](https://courses.media.mit.edu/2022spring/mas836/readings/fsrguide.pdf), [Adafruit](https://learn.adafruit.com/force-sensitive-resistor-fsr/overview), [Rapid Online, especificaciones](https://rapidonline.com/interlink-electronics-fsr402-0-5-diameter-force-sensing-resistor-78-4002).
- Raspiaudio Audio+: [pinout.xyz](https://pinout.xyz/pinout/raspiaudio_audio).
