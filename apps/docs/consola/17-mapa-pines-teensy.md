# Consola: mapa de pines de la Teensy única

> **Fecha:** 2026-11-10. **Estado:** propuesta para el prototipo y la placa principal (ADR-008 §2: una sola Teensy con
> superficie + audio I/O + CV). Reemplaza el reparto A/S de `03-arquitectura-capas.md` §4 y los pines de la S en
> `06-teensy-s-e4.md`. **Base:** PJRC, asignación de pines de Teensy 4.1; `01-architecture.md` §3 (pines del Brain);
> auditoría G28/G29. Ningún pin de una ampliación reservada se usa para otra cosa.

## 1. Teensy 4.1 (pines 0–41)

| Pin | Función | Tipo | Nota |
|---|---|---|---|
| 0 / 1 | **MIDI DIN** RX/TX (Serial1) | reservado | Huellas del optoacoplador y jacks TRS (`03` §4.2) |
| 2, 3 | Encoder 1 A/B | digital | |
| 4, 5 | Encoder 2 A/B | digital | |
| 6, 9 | Encoder 3 A/B | digital | |
| 7 | **I2S1 TX** → SGTL5000 DIN | audio | Fijo |
| 8 | **I2S1 RX** ← SGTL5000 DOUT | audio | Fijo |
| 10, 11 | Encoder 4 A/B | digital | |
| 12, 30 | Encoder 5 A/B | digital | |
| 13 | **Datos de LEDs** (SK6812 de los botones) | salida | Tiene el LED de la placa + 470 Ω: sirve como salida, no como entrada con pull-up |
| 14, 15, 16, 17 (A0–A3) | **FSR de velocity 1–4** | analógico, ADC1 | Leídos en la interrupción a 2,5 kHz (`06` §2) |
| 18, 19 | **I2C** SDA/SCL (Wire) | bus | SGTL5000, 2 ADS1115, 3 MCP23017, MCP4725 |
| 20 | **I2S1 LRCLK** | audio | Fijo |
| 21 | **I2S1 BCLK** | audio | Fijo |
| 22 (A8) | Perilla **MASTER** | analógico, ADC2 canal13 | También ADC1; firmware usa ADC2. Deshabilitada en mínimo mientras esté al aire |
| 23 | **I2S1 MCLK** | audio | Fijo |
| 24 (A10) | Perilla **CUE** | analógico, sólo ADC1 canal1 | Buzón desde ISR, sin lectura ADC en loop |
| 25 (A11) | Trigger e-drum 1 | reservado | Acondicionamiento en huellas sin poblar |
| 26 (A12) | Perilla **IN** | analógico, sólo ADC2 canal3 | |
| 27 (A13) | Trigger e-drum 2 | reservado | |
| 28 / 29 | **NeoTrellis M4** RX/TX (Serial7) | UART | Antes Serial5 (20/21), que choca con I2S |
| 31, 32 | Encoder 6 A/B | digital | |
| 33, 34 | Encoder 7 A/B | digital | |
| 35, 36 | Encoder 8 A/B | digital | |
| 37, 40 | **Rueda jog** A/B (EC11) | digital | 40 es A16, usado como digital |
| 38 (A14) | Trigger e-drum 3 | reservado | |
| 39 (A15) | Trigger e-drum 4 | reservado | |
| 41 (A17) | **Selector del pad de entrada** (0 / −10 / −20 dB) | analógico | ADC1/2 canal10; firmware usa buzón ADC1. Escalera para3 posiciones (ADR-004) |

**Ocupación:** 42 de 42. Más pines, si hicieran falta, en los pads inferiores de la 4.1 (42–47 de la microSD y 48–54
de la PSRAM, solo si no se usan esas funciones).

**CV:** por el **MCP4725** en I2C (0x60, y 0x61 para un segundo canal). El pin 29 que el Brain usaba para CV por PWM
(ADR-003) pasa a la M4.

## 2. Bus I2C (Wire, 400 kHz, 3,3 V)

| Dispositivo | Dirección | Uso |
|---|---|---|
| SGTL5000 | 0x0A | Control del códec (cue, entrada de línea) |
| ADS1115 #1 | 0x48 | Faders 1–4 |
| ADS1115 #2 | 0x49 | Expresión 1–2, manos Sharp 1–2 |
| MCP23017 #1 | 0x20 (A2 A1 A0 = 0 0 0) | Clics de encoders (PCB-A) |
| MCP23017 #2 | 0x21 (0 0 1) | Transporte, página/octava (PCB-K1) |
| MCP23017 #3 | 0x22 (0 1 0) | Edición, SHIFT, navegación de K2 (PCB-K1) |
| MCP23017 #4 | 0x23 (0 1 1) | Motor en vivo, sección, jog, pedal (PCB-M) |
| MCP4725 | 0x60 / 0x61 | CV |

Un solo par de pull-ups activo en el bus; quitar las de los módulos si el total baja de ~1,5 kΩ.

## 3. Entradas de los MCP23017 (14 por chip; GPA7 y GPB7 sin usar)

**Corregido 2026-11-11 (D1 de `24`, inventario de `25` §3):** el reparto sigue el cableado de las placas existentes.

| Chip | Placa | Entradas | Contenido |
|---|---|---|---|
| #1 (0x20) | PCB-A | 8 + 6 libres | Clics de los encoders 1–8 |
| #2 (0x21) | PCB-K1 | 9 + 5 libres | OCT +, OCT −, PAGE ▶, PAGE ◀; METRO, LOOP, REC, STOP, PLAY |
| #3 (0x22) | PCB-K1 (+ K2) | 11 + 3 libres | UNDO, BACK, MENU, ◀, ▶, BROWSE (las 5 de K2, antes modos), SHIFT; DELETE, DUPLICATE, QUANTIZE, NEW |
| #4 (0x23) | **PCB-M** | 8 + 6 libres | CUE, FILL, TAP, BUILD, DROP, BREAK, clic del jog, pedal |
| **Total** | | **36 / 56** | |

Los modos se eligen en la pantalla (MENU). El orden exacto de pines dentro de cada chip es el de
`kicad/consola/boards/CONNECTORS.md`; el mapa de notas de 18 se actualiza con el firmware.

## 4. Cambios frente a lo anterior

| Antes | Ahora |
|---|---|
| Dos Teensy, enlace UART A↔S (7/8 + sync 37) | Una Teensy; 7/8 son del I2S |
| M4 en Serial5 (20/21) | Serial7 (28/29) |
| MASTER en 23 | 22 (23 es MCLK) |
| Jog en 29/36 | 37/40 |
| 2 MCP | 4 MCP: 0x20 en PCB-A, 0x21/0x22 en PCB-K1, 0x23 en PCB-M |
| CV por PWM en 29 | MCP4725 por I2C |

## Fuentes

- [PJRC, Teensy 4.1: pines, serie, analógicos e I2S](https://www.pjrc.com/teensy/pinout.html).
- Microchip MCP23017, hoja de datos (revisión actual: GPA7/GPB7 solo salida).
- `01-architecture.md` §3 (pines del Brain y su audio shield), `06-teensy-s-e4.md`, ADR-003, ADR-004, ADR-008.


## Corrección de firmware del banco (2026-11-10)

**Sin cambios de pines.** La propiedad ADC pasa a un único planificador por unidad (18§2/7). Mínimo: MASTER22 sin habilitar; FSR14/15 con divisor10k, Sharp en ADS0x48 AIN2 y fader en AIN0. ADS0x49/MCP0x22/MCP4725 no se sondean en ese perfil. Un cursor que pueda desconectarse necesita pull-down externo; el deadband no demuestra cableado. ADC2 en22 se verificó contra `libraries/ADC/ADC.cpp` del paquete Teensyduino1.160.0; CUE24 no tiene ADC2, por eso no basta cambiar todas las lecturas a ADC2 manteniendo los pines.
