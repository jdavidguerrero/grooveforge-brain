# Consola: placa principal (PCB-M)

> **Fecha:** 2026-11-12. **Estado:** decisiones D1–D15 aprobadas por Juan (§8); esquema PCB-M en captura,
> layout pendiente del panel (`25-panel-consola.md`). **Base:** ADR-008 §2 y §6 (una Teensy; presupuesto de consumo obligatorio antes de la placa, auditoría G30),
> `03-arquitectura-capas.md` §4 (una placa principal reemplaza portadora + PCB-S + PCB-A; se mantienen las placas de
> botones y faders), `17-mapa-pines-teensy.md` (pines e I2C), `18-teensy-unica-firmware.md`,
> `pendientes.md` (chips soldados por JLC), placas existentes en `apps/hardware/kicad/consola/boards/` y su contrato
> `boards/CONNECTORS.md`.

## 1. Qué es

Una placa, **PCB-M**, que lleva la Teensy 4.1 en zócalo y todo lo que 17 conecta a ella y no vive en el panel:
códec, convertidores, CV, MIDI TRS tipo A, conectores para perillas de audio y los
conectores hacia el panel, el Pi y el exterior. Reemplaza a la **portadora del Brain** y a la **PCB-S**.

## 2. Qué se reutiliza y qué no (hallazgo de esta revisión)

Las placas de GroovePush (rev. auditada 2026-10-22/23, Gerbers listos) se diseñaron para el mapa viejo: dos Teensy,
M4 en Serial5, faders directo a la Teensy, MCP 0x20 en PCB-A y 0x21/0x22 en PCB-K1. **Sus cables son 1:1 y llevan
nombres de señal, no pines de la Teensy**: el mapa a pines vive en el lado de la portadora. Por eso:

| Placa | Veredicto | Por qué |
|---|---|---|
| **PCB-A** (8 encoders + MCP 0x20 + hub) | **Se mantiene fabricada**, pinout de CONNECTORS.md (D8) | Su J1 (IDC 2×15) lleva ENC1–8 A/B, FADER1–4, LED_DIN, +5 V, +3,3 V y GND: PCB-M recibe esos nombres y los lleva a los pines de 17 (p. ej. ENC5_A al pin 12, no al 22). Rediseñarla costaría una placa auditada sin ganar nada |
| **PCB-F** (4 faders) | Se mantiene | Sigue colgando de PCB-A J3; PCB-M lleva FADER1–4 al **ADS1115 0x48** en vez de A0–A3 |
| **PCB-K** única (26 teclas + LEDs, MCP 0x21/0x22/0x23) | Reemplaza K1/K2 (§9) | Cable directo J_PCB_K con I2C, LED_DIN_5V, +5V_LED, GND, JOG_CLICK y PEDAL_BTN; datos desde pin 13 |
| Portadora del Brain + **PCB-S** | **Se reemplazan por PCB-M** | Mapa viejo; PCB-S era un adaptador de la portadora |

**Reparto vigente (`17` §3 y §9 de este documento):** 0x20 en PCB-A = 8 clics; 0x21/0x22 en
PCB-K = 20 teclas; 0x23 en PCB-K = 6 teclas nuevas + clic del jog + pedal. Total **36 de 56**.
PCB-M no puebla ningún MCP23017; JOG_CLICK y PEDAL_BTN viajan por J_PCB_K. Queda sustituida la
propuesta anterior de 41 entradas y de montar 0x23 en PCB-M, también donde la repita `pendientes.md`.

## 3. Bloques de PCB-M

| Bloque | Contenido | Pines / bus (17) | Nota |
|---|---|---|---|
| Teensy 4.1 | Zócalo 2×24 + fila inferior que haga falta; USB micro-B de la Teensy al Pi con cable | — | Corte del puente VUSB-VIN si se alimenta de 5 V de PCB-M (§5) |
| Códec | **D2**: módulo Audio Shield Rev D en headers (como el banco) **o** SGTL5000 soldado | I2S1 7/8/20/21/23, I2C 0x0A | HP out = cue; LINE IN = entrada; LINE OUT = salida del modo procesador de FX. **Frente analógico copiado de la portadora del Brain Studio** (NRJ3HF-1, DP3T de pad, MCP6004, BAT54S, VREF): `25` §6 |
| ADS1115 #1 0x48 | FADER1–4 | I2C | Desde PCB-A J1 |
| ADS1115 #2 0x49 | Expresión 1–2 (**FSR de presión sostenida en el panel**, `00` §10.2.1), manos Sharp 1–2 (JST) | I2C | FSR en divisor a 3,3 V; Sharp a 5 V con 10 µF local, salida verificada ≤ 3,3 V |
| MCP23017 0x23 (en PCB-K) | 6 teclas + clic jog + pedal | I2C por J_PCB_K | Ningún MCP en PCB-M (§9) |
| MCP4725 ×2 0x60/0x61 | CV 1–2 | I2C | Buffer rail-to-rail con ganancia 1,5 → **0–5 V**, 1 kΩ en serie y diodos de protección por salida (D4) |
| FSR 1–4 | Divisor 3,3 V → FSR → pin → 10 kΩ → GND | 14–17 | 4 × JST-PH 2 hacia los pads del panel |
| Perillas | MASTER, CUE, IN (10 kΩ lineales, 3,3 V) | 22, 24, 26 | Panel por cable; RC 1 kΩ/100 nF por cursor |
| Selector de pad | 0 / −10 / −20 dB: conmutador de 3 posiciones que atenúa LINE IN **y** da la escalera a 41 | 41 | Divisor 10k/10k permanente en PCB-M, J_PAD_SEL de 3 hilos (D13) |
| Jog | EC11 o encoder de jog | 37/40 + MCP 0x23 | Panel por cable J_JOG |
| NeoTrellis M4 | JST-PH 4: GND, n.c., TX7 (29), RX7 (28) | Serial7 | La M4 sigue con USB al Pi (alimentación y carga de firmware) |
| LEDs de botones | Pin 13 → 74AHCT1G125 → J_PCB_K.5 | 13 | J1_PCBA.26 NC; no usar simultáneamente la antigua ruta por PCB-A |
| MIDI TRS tipo A | IN 6N138; OUT 33 Ω (0,5 W) a 3,3 V y 10 Ω (0,25 W) desde TX | Serial1 0/1 | D12; tip = DIN5, ring = DIN4, sleeve OUT = GND, sleeve IN NC |
| Pedal | Jack TRS 6,35 mm a MCP 0x23 | — | Nota 122 |
| Triggers e-drum | Conector (DB15 o RJ45) + huellas sin poblar del acondicionamiento | 25, 27, 38, 39 | Reservado |
| I2C | Un solo par de pull-ups **2,2 kΩ** en PCB-M; **quitar R1/R2 de PCB-A** (o no poblarlas) | 18/19 | Con las de PCB-A quedarían 1,1 kΩ |
| Alimentación | J_5V_LED_IN desde USB-A de Pi → F1 0,75 A → +5V_LED; LDO → +3.3VA | — | §5; AGND/GND solo por NT1 junto al LDO (D9/D10) |

## 4. Conectores al exterior (jacks de panel por cable)

Sin fuente externa adicional (D6); J_5V_LED_IN recibe un cable de un USB-A del Pi (D9) · USB de la Teensy (interno, cable corto al Pi) · audífonos cue (HP out) · LINE IN estéreo ·
CV 1–2 · MIDI in/out TRS · pedal · expresión 1–2 · e-drum (reservado). La salida de sala es la del **Audio+ del
Pi**, no de PCB-M; si se quiere en el mismo panel trasero, cable interno del Audio+ a jacks de panel (D5).

D15: los jacks se compran en Bogotá y se fijan con tuerca a la pared trasera; en el borde de PCB-M
solo hay JST-PH, uno por jack. HP usa el único J_HP por cable. Pin a pin en `boards/pcb-m/README.md`.

## 5. Presupuesto de consumo (G30) — estimado, a medir en banco

| Carga | Riel | Típico | Peor caso | Fuente |
|---|---|---|---|---|
| Raspberry Pi 5 + NVMe + Audio+ | 5 V | 2,0 A | 5,0 A | Fuente oficial 27 W; con periféricos USB |
| Pantalla 7" DSI | 5 V (desde el Pi) | 0,5 A | 0,7 A | incluida en el Pi si cuelga del conector DSI |
| Teensy 4.1 a 600 MHz | 5 V | 0,10 A | 0,15 A | PJRC |
| Códec, ADS ×2, MCP, MCP4725 ×2, FSR | 3,3 V | 0,04 A | 0,06 A | hojas de datos |
| 26 SK6812MINI-E (20 teclas + hasta 6 nuevas, `25` §3) | 5 V | 0,4 A (brillo 25 %) | 1,6 A | 60 mA × 26 |
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
| USB-A 3 (solo 5 V, cable de alimentación) | 26 LEDs de las teclas, Sharp ×2, buffer de LEDs | 0,5 A | 0,6 A (brillo ≤ 25 %) |
| **Suma de USB** | | **1,15 A** | **1,45 A ≤ 1,6 A** |

Condiciones: brillo de LEDs **limitado por firmware sin excepción** (teclas y M4), polyfuse de 0,75 A en la entrada
de 5 V de LEDs de PCB-M, los 5 V de la Teensy y los de LEDs **no se unen** (corte VUSB–VIN, diodo o separación por
rama). Riesgo: en el peor caso del Pi (CPU al 100 % + NVMe + USB al tope) la fuente queda al límite: **medir en banco
con medidor USB antes de fabricar**; si no alcanza, el plan B es una segunda entrada de 5 V para PCB-M (huella
prevista, sin poblar).

## 6. Mecánica

Dimensiones y posición de PCB-M, de las perillas y del jog dependen del panel (`kicad/consola/panel/`,
`panel-coordinates.csv`) y del chasis; se fijan en el esquema tras D3. 2 capas, 1,6 mm, ensamble JLC por una cara,
THT a mano (zócalo de Teensy y conectores); jacks, selector y perillas en panel por cable, como las otras placas.

## 7. Verificación antes de pedir

ERC/DRC limpios; `sync_from_netlist` y `export_fab` como en las otras placas; revisión cruzada de 17/18 contra el
netlist (cada pin de 17 aparece una vez); auditoría del Hardware Engineer; prueba de humo en banco con la Teensy:
I2C ve 0x0A, 0x20–0x23, 0x48, 0x49, 0x60, 0x61; audio dúplex ≥ 60 min sin cortes con todos los controles (18).

## 8. Decisiones

| # | Decisión |
|---|---|
| **D1** | **Sí:** el firmware sigue el cableado de las placas existentes (0x20 clics en PCB-A, 0x21/0x22 Kailh en K1/K2) y el **4.º MCP (0x23) se puebla en PCB-K**, según §9. El reparto vigente es `17` §3: 36 entradas |
| **D2** | **Módulo Audio Shield Rev D** en headers (Juan lo tiene; ya estaba en la portadora) |
| **D3** | **Panel:** jog, pots, selector y HP por cable a PCB-M; teclas y MCP 0x21/0x22/0x23 en PCB-K (§9); jacks según D15 |
| **D4** | **MCP4725 ×2**, salida 0–5 V con buffer (resuelve la contradicción con 03 §4.2: MCP4728 queda descartado) |
| **D5** | Salida del Audio+: **cable RCA macho → dos jacks hembra de 6,35 mm** en el panel trasero, o soldado directo al Audio+ |
| **D6** | **Una sola fuente: la oficial del Pi.** Todo se alimenta desde el Pi (reparto en §5); plan B con huella sin poblar |
| **D7** | **MIDI DIN poblado** en v1 |
| **D8** | **2026-11-12, aprobado por Juan:** manda el pinout de PCB-A fabricada en CONNECTORS.md. PCB-M lleva cada ENC a ENCODER_PINS de ConsolaMap.h/17; FADER1–4 → ADS1115 0x48 AIN0–3. J1.18 (spare), .20 (INT_K), .22 (INT_A), .26 (LED_DIN) NC; LEDs por J_PCB_K |
| **D9** | **2026-11-12, aprobado por Juan:** J_5V_LED_IN JST-PH S2B-PH-K (BOM C157929), pin1 +5V_LED_RAW / pin2 GND desde USB-A de Pi; F1 0,75 A → +5V_LED (teclas, Sharp, buffer y cargas ya capturadas). Sin TVS de entrada añadida: no hay pieza adecuada en la BOM; documentar pendiente |
| **D10** | **2026-11-12, aprobado por Juan:** AGND y GND unidas solo por NT1, símbolo Device:NetTie_2 y footprint estándar NetTie:NetTie-2_SMD_Pad0.5mm, junto a U_LDO1 (+3.3VA) en layout; HP_VGND permanece aislada |
| **D11** | **2026-11-12, aprobado por Juan:** cada CV lleva 1 kΩ en serie y BAT54S desde el nodo del jack a GND/+5V_LED; cada pot MASTER/CUE/IN lleva 1 kΩ serie desde cursor y 100 nF a GND en el lado ADC |
| **D12** | **2026-11-12, aprobado por Juan:** MIDI OUT a 3,3 V según MIDI Association CA-033 (2014): 33 Ω al riel (≥0,5 W), 10 Ω desde TX (≥0,25 W), sin 100 Ω adicional. TRS **tipo A**, tip DIN5, ring DIN4, sleeve DIN2; IN sleeve NC, OUT sleeve GND |
| **D13** | **2026-11-12, aprobado por Juan:** selector en panel; eliminar SW_PAD_SEL de PCB-M. J_PAD_SEL 3 hilos (+3,3 V / PAD_SEL_ADC / GND) y escalera 10k/10k en PCB-M. Polo C: común al ADC, extremos a rieles, contacto central sin cable (divisor da 1,65 V); polos A/B de audio siguen por cable |
| **D14** | **2026-11-12, aprobado por Juan:** pull-ups I2C de 2,2 kΩ en PCB-M; **desoldar R1/R2 de PCB-A** para esta integración. Conservar el pinout fabricado; no se modifican archivos de PCB-A |
| **D15** | **2026-11-12, aprobado por Juan:** jacks comprados en Bogotá, con tuerca en pared trasera del cuerpo y cableados; sustituir huellas de jacks por JST-PH en borde trasero, uno por LINE IN L/R, LINE OUT L/R, PEDAL, CV1/2, MIDI IN/OUT y HP (J_HP existente). Conservar detección de inserción si existe: la captura actual no tiene contacto de detección en ninguno; no se asigna señal nueva |

## 9. Cambio 2026-11-11: el 4.º MCP pasa a PCB-K

Con la distribución v1 (`25` §7) las 26 teclas van en **una sola PCB-K**; Juan decidió poner ahí también el **MCP
0x23** junto a 0x21 y 0x22. PCB-M ya no lleva MCP: el clic del jog y el pedal (que entran a PCB-M por cable) viajan
a PCB-K por el cable PCB-M ↔ PCB-K, junto con I2C, LED_DIN, +5 V de LEDs y GND. PCB-K reemplaza a K1/K2.
