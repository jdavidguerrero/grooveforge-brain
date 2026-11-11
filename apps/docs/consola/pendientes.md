# Consola: pendientes

> Lista viva de trabajo decidido o propuesto que **no** entra ahora. Cada entrada dice de dónde sale, la propuesta y
> la condición para retomarla. Las decisiones abiertas de arquitectura están en la auditoría
> (`auditoria/auditoria-fable-2026-11.md` §8).

## Backlog de UX (detalles menores, se arreglan en bloque al final)

Decisión de Juan (2026-11-11): los detalles de UX se documentan aquí y se resuelven juntos más adelante; la
prioridad pasa a lo core (pruebas de hardware, fabricación de PCB, diseños).

- **DRUM: no se sabe qué sonido hay en cada pad ni cuál está seleccionado sin navegar.** Con 16 sonidos, si se
  reordena no se sabe dónde quedó el kick. Propuestas: (a) label fijo y grande en la pantalla con el pad
  seleccionado (nombre + categoría + número), visible en todas las vistas DRUM; (b) colores por categoría en la
  NeoTrellis y en la rejilla de pantalla (kick, snare, hats, perc, toms… con la paleta de doc 23), así el kick
  se reconoce por color; (c) al tocar un pad, el nombre aparece un instante aunque no se esté en la vista del
  pad; (d) opción de fijar posiciones por categoría (kick siempre en el pad 1) al cargar un rack.
- **Manual de uso** del modo DRUM/SYNTH (pantallas, NeoTrellis, encoders, PLAY/REC) cuando la UX esté estable.

## Pads de la v2 (Juan, 2026-11-11)

La NeoTrellis (pads de 10 mm, sin fuerza) vale para la v1, pero no es lo mejor para tocar con expresión. En vivo Juan
probablemente usará un **controlador MIDI externo con mejores pads** (conectado al Pi), y la NeoTrellis quedará más
como **secuenciador** y rejilla de escenas. Para una v2: evaluar pads más grandes con velocity/presión propios.
**Condición:** después de usar la v1 en sets reales.

## Secciones del set: BUILD, DROP, BREAK (Juan, 2026-11-11)

Tres teclas Choc físicas (`25` §3) que cambian la sección del set completo: la tecla queda armada (LED parpadea) y se
ejecuta al cambio de frase; SHIFT la ejecuta al siguiente compás; presionarla de nuevo la cancela. Cada sección es
una **receta de Juan por set** (roles que entran o salen, macro de FX, largo del build) y fija la energía objetivo
para la siguiente generación del motor; el motor no decide nada musical. Juan quiere usarlas también para
**arreglos en la composición** (armar la estructura del tema con secciones). **Condición:** spec en el núcleo
(extensión de 19) después de E5; hardware y mapa MIDI ya reservados.

## ✅ Respuesta del touch: lenta e imprecisa (Juan, 2026-11-10) — **resuelto en GUI, 2026-11-11**

Juan, probando DRUM en la Pi: con los encoders todo responde bien, pero tocando la pantalla (teclas de menú,
browse de racks) la respuesta tarda o el toque se pierde. Diagnóstico (2026-11-10):

1. `components/TouchTarget.qml` es un `MouseArea` que actúa en `onClicked` (al soltar) y no muestra estado
   presionado: mientras el dedo está apoyado no hay respuesta visual.
2. Dentro de `Flickable`/`ListView` (browse, listas) un movimiento de pocos píxeles del dedo lo toma el Flickable
   como arrastre y el toque se cancela (`preventStealing: false`).
3. `load_rack` en el host tarda 0,38 s (TRIBE 1, 30 MB) y 0,53 s (TRIBE 2, 45 MB): lee, verifica SHA-256 y
   remuestrea los 64 WAV en el hilo de control, sin aviso de carga en la GUI; después la GUI recibe el rack
   completo y redibuja todo.
4. La GUI (Python, hilo principal) promedia ~50 % de un núcleo (9 min de CPU en 19 min); en reposo ~14 %.

**Implementado:** press visible en todos los TouchTarget; menú/soft keys/tabs al presionar, listas y destinos
dentro de scroll al soltar; umbral25px (~3mm). Avisos LOADING desde la petición hasta ACK/error/desconexión.
Clock/meters actualizan sus propiedades sin reconstruir controles; caché del resumen de rack y soft keys estables.

Comparación controlada en **macOS, Qt6.11.1 offscreen/software, QML90**, 30clock+30meters/s,
6s por pantalla +1s warmup, cProfile activo (CPU de un núcleo; no comparar directamente con el Pi histórico):

| Pantalla | ms/evento antes → después | CPU hilo principal antes → después |
|---|---|---|
| DrumBrowser | 2,409 → 0,281 | 18,93% → 2,97% |
| DrumSteps | 5,240 → 0,988 | 46,52% → 8,20% |
| DrumMixer | 5,032 → 0,631 | 50,55% → 5,70% |

[Reporte, perfiles crudos, capturas y pruebas](../../consola/gui-console/screenshots/touch/REPORT.md).
La carga simulada con respuesta a530ms conserva el aviso mientras llegan clock/meters. La carga real del host
no se acelera. **Resta validación física** EGLFS+evdev/Qt6.8 en el Pi con el mismo browse TRIBE: sensación del
touch, scroll, CPU y audio simultáneo. Calibración identidad y LEDs23 conservados.

## ~~DX7 de 6 operadores como modo adicional~~ → modo implementado; validación física pendiente (ADR-008 §4)

Juan decidió implementarlo ya; queda como registro de la discusión.
Implementación: [modelo y APIs](../theory/dx7-6op.md), [validación reproducible](../theory/dx7-6op-validation.md).
FM 4-OP sigue disponible. Falta la medición Pi/Teensy y la calibración AMD/AMS contra un DX7 físico.

- **Origen:** el diseño de Claude Design mostraba «FM 6-OP»; ADR-005 fijó 4 operadores. Juan preguntó si agregar
  los dos que faltan para acercarlo al DX7.
- **Por qué ADR-005 lo descartó:** no por CPU (estimación 4–5 % de la Teensy con 4 operadores; del orden de 7–8 %
  con 6), sino por la edición: 6 operadores con 5 encoders del Brain Studio. En la consola hay 8 encoders y pantalla
  con diagrama de algoritmo, y si el gemelo corre en el Pi la CPU deja de ser un límite.
- **Propuesta:** mantener **FM 4-OP** (edición simple, también en Studio) y agregar un modo **DX7 6-OP** para
  importar y tocar patches SysEx originales, editables desde la consola y el VST.
- **Lo que de verdad da la fidelidad del DX7**, y es la parte más trabajosa: envolventes de 4 etapas (rate/level)
  por operador, escalado por teclado (level y rate), sensibilidad a la velocity, 32 algoritmos y feedback en el
  operador 6. Sin eso, un SysEx cargado no suena como el original.
- **Afecta:** objeto de audio del DX7 en el firmware, formato GFP, VST, gemelo, páginas de edición (el diagrama de
  algoritmo del brief de Claude Design ya se pidió preparado para crecer de 4 a 6).
- **Referencia de implementación a evaluar:** MSFA (motor FM en el que se basa Dexed). Licencia a verificar (se cree
  Apache 2.0); Dexed es GPL y no conviene mezclarlo.
- **Condición para retomar:** gemelo funcionando en el Pi (benchmark de `12-gemelo-pi.md` aprobado) y E5 cerrado.
  Requiere enmienda a ADR-005 y theory doc antes del código. No es del MVP.

## Chips de la superficie soldados en la PCB por JLCPCB (Juan, 2026-11-10)

- **Decisión:** en la placa principal van los chips soldados por JLC, no módulos. Los módulos (CJMCU-2317, ADS1115 de
  8 pines) son solo para el prototipo de protoboard. No se compran más módulos.
- **Piezas (LCSC, precios unitarios del 2026-11-10):** ADS1115IDGSR **C37593** (VSSOP-10, USD 1,40) ×2 · MCP23017T-E/SS
  **C558584** (SSOP-28, USD 1,72) ×3 + huella del 4.º sin poblar · MCP4725A0T-E/CH **C144198** (SOT-23-6, USD 0,96) ×2
  (0x60/0x61 con su pata A0). Las tres son «extended»: cargo de ~USD 3 por tipo y pedido, a confirmar al cotizar.
- **Direcciones por pista** (ADS 0x48/0x49 con ADDR a GND/3,3 V; MCP 0x20–0x23 con A0–A2) y **un solo par de pull-ups**
  en el bus I2C de la placa.
- **Prototipo actual:** 2 MCP (módulos, puerto A) y 1 ADS1115 (dirección fija 0x48, sin pata ADDR): basta para validar
  el bus compartido con el SGTL5000. El Sharp se prueba en A2 del mismo ADS.

## Entrada MIDI del host en UMP (2026-11-10)

PipeWire 1.4 publica los teclados USB (p. ej. Arturia MiniLab mkII) como **MIDI 2.0 / UMP de 32 bits**, y
`gf-synth-host:midi_in` se declara como MIDI 1.0 de 8 bits: `pw-link` no los conecta. Mientras tanto, un puente
provisional (`~/midi_to_host.py` en el Pi: ALSA → comando `note` del socket) hace sonar el teclado; probado por Juan
con el Model D. Arreglo: el host debe declarar su puerto en UMP (y convertir), o aceptar ambos formatos.

## Contradicción: TX/RX de la NeoTrellis M4 en el conector STEMMA (2026-11-10)

`processor/UART_CONNECTIONS.md` dice SDA (pin 21) = RX y SCL (pin 22) = TX; el código
(`lib/neotrellis_m4/UartInterface/UartInterface.cpp`, Serial1 SERCOM4) dice pin 21 (SDA) = TX y 22 (SCL) = RX. Se
cablea según el código (SDA → Teensy RX7 pin 28, SCL ← Teensy TX7 pin 29) y se confirma en la protoboard; luego se
corrige el documento que esté mal. El pin 3V del conector no se conecta (salida del regulador de la M4).
