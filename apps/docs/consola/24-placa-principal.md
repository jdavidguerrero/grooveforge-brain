# Consola: placa principal (PCB-M)

> **Fecha:** 2026-11-11. **Estado:** decisiones D1–D7 tomadas por Juan (§8); falta el panel (`25-panel-consola.md` y
> Claude Design) para fijar dimensiones y qué va en PCB-M o en placas nuevas. Ningún esquema antes del panel. **Base:** ADR-008 §2 y §6 (una Teensy; presupuesto de consumo obligatorio antes de la placa, auditoría G30),
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
| ADS1115 #2 0x49 | Expresión 1–2 (**FSR de presión sostenida en el panel**, `00` §10.2.1), manos Sharp 1–2 (JST) | I2C | FSR en divisor a 3,3 V; Sharp a 5 V con 10 µF local, salida verificada ≤ 3,3 V |
| MCP23017 0x23 | 11 navegación + clic jog + pedal | I2C | Huellas de botón en PCB-M o conector a una placa de navegación (D3) |
| MCP4725 ×2 0x60/0x61 | CV 1–2 | I2C | Buffer rail-to-rail con ganancia 1,5 → **0–5 V**, 1 kΩ en serie y diodos de protección por salida (D4) |
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

Sin entrada de alimentación propia (D6) · USB de la Teensy (interno, cable corto al Pi) · audífonos cue (HP out) · LINE IN estéreo ·
CV 1–2 · MIDI in/out TRS · pedal · expresión 1–2 · e-drum (reservado). La salida de sala es la del **Audio+ del
Pi**, no de PCB-M; si se quiere en el mismo panel trasero, cable interno del Audio+ a jacks de panel (D5).

## 5. Presupuesto de consumo (G30) — estimado, a medir en banco

| Carga | Riel | Típico | Peor caso | Fuente |
|---|---|---|---|---|
| Raspberry Pi 5 + NVMe + Audio+ | 5 V | 2,0 A | 5,0 A | Fuente oficial 27 W; con periféricos USB |
| Pantalla 7" DSI | 5 V (desde el Pi) | 0,5 A | 0,7 A | incluida en el Pi si cuelga del conector DSI |
| Teensy 4.1 a 600 MHz | 5 V | 0,10 A | 0,15 A | PJRC |
| Códec, ADS ×2, MCP, MCP4725 ×2, FSR | 3,3 V | 0,04 A | 0,06 A | hojas de datos |
| 30 SK6812MINI-E (20 teclas + 10 nuevas, `25` §3) | 5 V | 0,45 A (brillo 25 %) | 1,8 A | 60 mA × 30 |
| NeoTrellis M4 (32 NeoPixel) | 5 V por USB | 0,5 A (brillo 25 %) | 1,9 A | 60 mA × 32 |
| Sharp GP2Y0A41 ×2 | 5 V | 0,03 A | 0,05 A | hoja de datos |
| **Total** | 5 V | **≈ 3,5 A** | **≈ 9 A** | |

Con el brillo limitado en firmware (doc 23, 20–25 %) el peor caso realista es **≈ 6 A**.

**Con una sola fuente (D6: la oficial del Pi, 27 W = 5 V 5 A).** El Pi 5 con esa fuente habilita **1,6 A en total
para sus USB-A**. Reparto propuesto, todo desde el Pi:

| Rama | Alimenta | Típico | Tope |
|---|---|---|---|
| Fuente → Pi (núcleo, NVMe, Audio+, pantalla) | — | 2,5 A | 3,4 A |
| USB-A 1 (datos + 5 V) | Teensy y la lógica de 3,3 V de PCB-M | 0,15 A | 0,25 A |
| USB-A 2 (datos + 5 V) | NeoTrellis M4 | 0,5 A | 0,6 A (brillo ≤ 25 %) |
| USB-A 3 (solo 5 V, cable de alimentación) | 30 LEDs de las teclas, Sharp ×2, buffer de LEDs | 0,5 A | 0,6 A (brillo ≤ 25 %) |
| **Suma de USB** | | **1,15 A** | **1,45 A ≤ 1,6 A** |

Condiciones: brillo de LEDs **limitado por firmware sin excepción** (teclas y M4), polyfuse de 0,75 A en la entrada
de 5 V de LEDs de PCB-M, los 5 V de la Teensy y los de LEDs **no se unen** (corte VUSB–VIN, diodo o separación por
rama). Riesgo: en el peor caso del Pi (CPU al 100 % + NVMe + USB al tope) la fuente queda al límite: **medir en banco
con medidor USB antes de fabricar**; si no alcanza, el plan B es una segunda entrada de 5 V para PCB-M (huella
prevista, sin poblar).

## 6. Mecánica

Dimensiones y posición de PCB-M, de las perillas y del jog dependen del panel (`kicad/consola/panel/`,
`panel-coordinates.csv`) y del chasis; se fijan en el esquema tras D3. 2 capas, 1,6 mm, ensamble JLC por una cara,
THT a mano (zócalo de la Teensy, jacks, perillas), como las otras placas.

## 7. Verificación antes de pedir

ERC/DRC limpios; `sync_from_netlist` y `export_fab` como en las otras placas; revisión cruzada de 17/18 contra el
netlist (cada pin de 17 aparece una vez); auditoría del Hardware Engineer; prueba de humo en banco con la Teensy:
I2C ve 0x0A, 0x20–0x23, 0x48, 0x49, 0x60, 0x61; audio dúplex ≥ 60 min sin cortes con todos los controles (18).

## 8. Decisiones

| # | Decisión | Recomendación |
|---|---|---|
| # | Decisión (Juan, 2026-11-11) |
|---|---|
| **D1** | **Sí:** el firmware sigue el cableado de las placas existentes (0x20 clics en PCB-A, 0x21/0x22 Kailh en K1/K2) y el **4.º MCP (0x23) se puebla** para lo que no tiene placa. 17 §3 y 18 se actualizan cuando el inventario de `25` quede aprobado |
| **D2** | **Módulo Audio Shield Rev D** en headers (Juan lo tiene; ya estaba en la portadora) |
| **D3** | **Panel por definir:** se revisa con Claude Design (`25` + prompt de enclosure de la consola). PCB-M o placas nuevas según ese resultado |
| **D4** | **MCP4725 ×2**, salida 0–5 V con buffer (resuelve la contradicción con 03 §4.2: MCP4728 queda descartado) |
| **D5** | Salida del Audio+: **cable RCA macho → dos jacks hembra de 6,35 mm** en el panel trasero, o soldado directo al Audio+ |
| **D6** | **Una sola fuente: la oficial del Pi.** Todo se alimenta desde el Pi (reparto en §5); plan B con huella sin poblar |
| **D7** | **MIDI DIN poblado** en v1 |
