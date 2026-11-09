# Consola GrooveForge: concepto e investigación

> **Fecha:** 2026-11-09. **Estado:** borrador para refinar con Juan. **No modifica** `00–06` ni los ADR. **Contradicción abierta:** `00-master-strategy.md` plantea el Brain (ESP32 + pantalla redonda) como producto; esta consola es una herramienta personal de producción y directo. Si se adopta, hace falta un ADR y la decisión explícita de Juan antes de tocar la estrategia.

## 1. La idea, en palabras de Juan

> «Producir más rápido… el motor automatiza cosas y me deja dedicarme al proceso creativo de escoger sonidos, organizar y pensar ideas muy rápido. Quiero un hardware específico para esto… para tocar liveset… como un DJ que va mezclando música que se va generando, escogiendo si va o no, ya haciendo lectura de todo lo que llevamos de mi estilo.»

Prioridades, en orden: **(1) producir más rápido, (2) tocar en vivo**, y con eso contenido, imagen y comunidad. **No es para vender** por ahora. Restricción: **un solo dispositivo**, para bajar el costo de PCB, usando lo que ya tiene: Raspberry Pi 5, pantalla de 10", dos Teensy 4.1, potenciómetros, encoders, faders y botones. Juan pide además un **step sequencer**.

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
            pantalla 10" (UI: opciones, cuadrícula, mezcla)
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
| **Raspberry Pi 5** | Cerebro: motor, UI, sampler, mezcla, registro | `tools/motor`, `perc-synth`, presets, racks; la experiencia de `studio-head` (LVGL en Pi) |
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
