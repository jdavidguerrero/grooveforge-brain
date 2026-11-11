# Consola: placa principal (PCB-M)

> **Fecha:** 2026-11-11. **Estado:** borrador para aprobación de Juan; ningún esquema antes de aprobar las decisiones
> de §8. **Base:** ADR-008 §2 y §6 (una Teensy; presupuesto de consumo obligatorio antes de la placa, auditoría G30),
> `03-arquitectura-capas.md` §4 (una placa principal reemplaza portadora + PCB-S + PCB-A; se mantienen las placas de
> botones y faders), `17-mapa-pines-teensy.md` (pines e I2C), `18-teensy-unica-firmware.md`,
> `pendientes.md` (chips soldados por JLC), placas existentes en `apps/hardware/kicad/consola/boards/` y su contrato
> `boards/CONNECTORS.md`.

## 1. Qué es

Una placa, **PCB-M**, que lleva la Teensy 4.1 en zócalo y todo lo que 17 conecta a ella y no vive en el panel:
códec, convertidores, expansores de los botones que no tienen placa, CV, MIDI DIN, perillas de audio y los
conectores hacia el panel, el Pi y el exterior. Reemplaza a la **portadora del Brain** y a la **PCB-S**.

## 2. Qué se reutiliza y qué no (hallazgo de esta revisión)

Las placas de GroovePush (rev. auditada 2026-10-22/23, Gerbers listos) se diseñaron para el mapa viejo: dos Teensy,
M4 en Serial5, faders directo a la Teensy, MCP 0x20 en PCB-A y 0x21/0x22 en PCB-K1. **Sus cables son 1:1 y llevan
nombres de señal, no pines de la Teensy**: el mapa a pines vive en el lado de la portadora. Por eso:

| Placa | Veredicto | Por qué |
|---|---|---|
| **PCB-A** (8 encoders + MCP 0x20 + hub) | **Se mantiene sin cambios** (propuesta; `03` decía reemplazarla) | Su J1 (IDC 2×15) lleva ENC1–8 A/B, FADER1–4, LED_DIN, +5 V, +3,3 V y GND: PCB-M recibe esos nombres y los lleva a los pines de 17 (p. ej. ENC5_A al pin 12, no al 22). Rediseñarla costaría una placa auditada sin ganar nada |
| **PCB-F** (4 faders) | Se mantiene | Sigue colgando de PCB-A J3; PCB-M lleva FADER1–4 al **ADS1115 0x48** en vez de A0–A3 |
| **PCB-K1 / PCB-K2** (20 Kailh + LEDs, MCP 0x21/0x22) | Se mantienen | I2C, LED_DIN e INT por PCB-A J4; la cadena de 20 SK6812 sigue igual (datos en pin 13) |
| Portadora del Brain + **PCB-S** | **Se reemplazan por PCB-M** | Mapa viejo; PCB-S era un adaptador de la portadora |

**Consecuencia: contradicción con 17 §3 y 18 (no resuelta aquí, ver D1).** 17 reparte las 41 entradas como
«#1 0x20 = clics + jog + Kailh 1–5, #2 0x21 = Kailh 6–19, #3 0x22 = Kailh 20 + navegación + pedal». Las placas
reales cablean **0x20 = 8 clics de encoder (6 libres sin conector)**, **0x21/0x22 = los 20 Kailh**. Lo que no tiene
placa es: **11 teclas de navegación + clic del jog + pedal = 13 entradas**. Propuesta: el **4.º MCP (0x23) se puebla
en PCB-M** para esas 13, y el mapa de firmware de 18 se reescribe para seguir el cableado físico (los números de nota
no cambian; solo qué pin de qué chip da cada nota). Total: 8 + 20 + 13 = **41 de 56**.

`pendientes.md` decía «3 MCP soldados en la placa principal»: con este hallazgo PCB-M lleva **1 MCP (0x23)** y los
otros 3 ya vienen en PCB-A y PCB-K1 (ensamble JLC de esas placas).

## 3. Bloques de PCB-M

| Bloque | Contenido | Pines / bus (17) | Nota |
|---|---|---|---|
| Teensy 4.1 | Zócalo 2×24 + fila inferior que haga falta; USB micro-B de la Teensy al Pi con cable | — | Corte del puente VUSB-VIN si se alimenta de 5 V de PCB-M (§5) |
| Códec | **D2**: módulo Audio Shield Rev D en headers (como el banco) **o** SGTL5000 soldado | I2S1 7/8/20/21/23, I2C 0x0A | HP out = cue; LINE IN = entrada |
| ADS1115 #1 0x48 | FADER1–4 | I2C | Desde PCB-A J1 |
| ADS1115 #2 0x49 | Expresión 1–2 (TRS), manos Sharp 1–2 (JST) | I2C | Sharp a 5 V con 10 µF local; salida verificada ≤ 3,3 V (divisor si hace falta) |
| MCP23017 0x23 | 11 navegación + clic jog + pedal | I2C | Huellas de botón en PCB-M o conector a una placa de navegación (D3) |
| MCP4725 ×2 0x60/0x61 | CV 1–2 | I2C | Buffer y escala según D4 |
| FSR 1–4 | Divisor 3,3 V → FSR → pin → 10 kΩ → GND | 14–17 | 4 × JST-PH 2 hacia los pads del panel |
| Perillas | MASTER, CUE, IN (10 kΩ lineales, 3,3 V) | 22, 24, 26 | Montadas en PCB-M o por cable (D3); RC 1 kΩ/100 nF por cursor |
| Selector de pad | 0 / −10 / −20 dB: conmutador de 3 posiciones que atenúa LINE IN **y** da la escalera a 41 | 41 | Escalera de 18 |
| Jog | EC11 o encoder de jog | 37/40 + MCP 0x23 | Montaje según panel (D3) |
| NeoTrellis M4 | JST-PH 4: GND, n.c., TX7 (29), RX7 (28) | Serial7 | La M4 sigue con USB al Pi (alimentación y carga de firmware) |
| LEDs de botones | LED_DIN de pin 13 por buffer 74AHCT1G125 a 5 V → PCB-A J1.26 | 13 | Hoy el buffer estaba en la portadora |
| MIDI DIN | TRS tipo A in (6N138, C3001991) y out (2 × 47 Ω a 3,3 V) | Serial1 0/1 | Reservado en firmware; se puebla en v1 |
| Pedal | Jack TRS 6,35 mm a MCP 0x23 | — | Nota 122 |
| Triggers e-drum | Conector (DB15 o RJ45) + huellas sin poblar del acondicionamiento | 25, 27, 38, 39 | Reservado |
| I2C | Un solo par de pull-ups **2,2 kΩ** en PCB-M; **quitar R1/R2 de PCB-A** (o no poblarlas) | 18/19 | Con las de PCB-A quedarían 1,1 kΩ |
| Alimentación | Entrada única, eFuse/polyfuse, 3,3 V digital y 3,3 V analógico separados | — | §5 |

## 4. Conectores al exterior (panel trasero, en el borde de PCB-M)

Entrada de alimentación · USB de la Teensy (interno, cable corto al Pi) · audífonos cue (HP out) · LINE IN estéreo ·
CV 1–2 · MIDI in/out TRS · pedal · expresión 1–2 · e-drum (reservado). La salida de sala es la del **Audio+ del
Pi**, no de PCB-M; si se quiere en el mismo panel trasero, cable interno del Audio+ a jacks de panel (D5).

## 5. Presupuesto de consumo (G30) — estimado, a medir en banco

| Carga | Riel | Típico | Peor caso | Fuente |
|---|---|---|---|---|
| Raspberry Pi 5 + NVMe + Audio+ | 5 V | 2,0 A | 5,0 A | Fuente oficial 27 W; con periféricos USB |
| Pantalla 7" DSI | 5 V (desde el Pi) | 0,5 A | 0,7 A | incluida en el Pi si cuelga del conector DSI |
| Teensy 4.1 a 600 MHz | 5 V | 0,10 A | 0,15 A | PJRC |
| Códec, ADS ×2, MCP, MCP4725 ×2, FSR | 3,3 V | 0,04 A | 0,06 A | hojas de datos |
| 20 SK6812MINI-E (botones) | 5 V | 0,30 A (brillo 25 %) | 1,2 A | 60 mA × 20 |
| NeoTrellis M4 (32 NeoPixel) | 5 V por USB | 0,5 A (brillo 25 %) | 1,9 A | 60 mA × 32 |
| Sharp GP2Y0A41 ×2 | 5 V | 0,03 A | 0,05 A | hoja de datos |
| **Total** | 5 V | **≈ 3,5 A** | **≈ 9 A** | |

Con el brillo limitado en firmware (doc 23, 20–25 %) el peor caso realista es **≈ 6 A**. Propuesta en D6.

## 6. Mecánica

Dimensiones y posición de PCB-M, de las perillas y del jog dependen del panel (`kicad/consola/panel/`,
`panel-coordinates.csv`) y del chasis; se fijan en el esquema tras D3. 2 capas, 1,6 mm, ensamble JLC por una cara,
THT a mano (zócalo de la Teensy, jacks, perillas), como las otras placas.

## 7. Verificación antes de pedir

ERC/DRC limpios; `sync_from_netlist` y `export_fab` como en las otras placas; revisión cruzada de 17/18 contra el
netlist (cada pin de 17 aparece una vez); auditoría del Hardware Engineer; prueba de humo en banco con la Teensy:
I2C ve 0x0A, 0x20–0x23, 0x48, 0x49, 0x60, 0x61; audio dúplex ≥ 60 min sin cortes con todos los controles (18).

## 8. Decisiones para Juan

| # | Decisión | Recomendación |
|---|---|---|
| **D1** | Reparto de los MCP: firmware sigue a las placas existentes (0x20 clics, 0x21/0x22 Kailh, **0x23 en PCB-M** = navegación + jog + pedal) en vez de 17 §3 | **Sí**: reutiliza PCB-A/K1/K2 tal cual; actualizar 17 §3 y 18 con tu visto bueno |
| **D2** | Códec: módulo Audio Shield Rev D en headers vs SGTL5000 soldado | **Módulo** en v1: es lo probado en banco y el chip de 32 pines no tiene stock en JLC (el de 20 pines sí, USD 9,3) |
| **D3** | Navegación (11 teclas), jog y perillas: ¿en PCB-M o en una placa pequeña de navegación junto a la pantalla? | Depende del panel: si la pantalla queda sobre PCB-M, en PCB-M; si no, **PCB-N** pequeña por JST-SH |
| **D4** | CV: MCP4725 ×2 (17, pendientes) vs MCP4728 4 CV + 2 gates (03 §4.2). **Contradicción entre docs** | MCP4725 ×2 como dice 17 (es lo más reciente) con buffer a 0–5 V; huella de gates opcional |
| **D5** | Salida de sala del Audio+ en el panel trasero por cable interno | Sí, jacks de panel para todo en un solo lado |
| **D6** | Alimentación: una entrada de **5 V 8–10 A** para todo (Pi por su pin de 5 V) vs **dos fuentes** (USB-C oficial al Pi + 5 V 3 A a PCB-M) | **Dos fuentes en v1**: el Pi conserva su protección y su PD; una sola entrada se evalúa con el consumo medido |
| **D7** | MIDI DIN poblado en v1 | Sí (la marimba y synths externos) |
