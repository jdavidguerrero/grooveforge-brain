# PCB-M — Placa Principal GrooveForge Consola

> Esquemático jerárquico KiCad 10, decisiones **D8–D15 aprobadas por Juan el 2026-11-12**
> (`apps/docs/consola/24-placa-principal.md` §8). Sin layout PCB; no es aprobación de fabricación.
> Descripción eléctrica: `tools/schgen/pcb_m.py`. Firmware: `apps/consola/surface/include/consola_s/ConsolaMap.h`
> y `apps/docs/consola/17-mapa-pines-teensy.md`. Conector fabricado: `../CONNECTORS.md`.

No habrá PCB-N: jog, pots IN/CUE/MASTER, selector y audífonos son componentes de panel por cable.
PCB-M no lleva MCP23017: **0x20 está en PCB-A; 0x21/0x22/0x23 en la PCB-K única**, que reemplaza
K1/K2. JOG_A/B van a Teensy 37/40; JOG_CLICK y PEDAL_BTN viajan por J_PCB_K al MCP 0x23.

## Bloques (10 subhojas)

| # | Hoja | Contenido |
|---|---|---|
| 01 | `01-teensy` | Sockets Teensy 4.1 y Audio Shield Rev D (módulos); I2S interno |
| 02 | `02-analog-front` | Frente Studio: pad 33k/12k/5.1k, polos A/B del selector por cable, acople 1µF, 1k, BAT54S, bias 100k a VREF; MCP6004 seguidores L/R/VREF; LINE OUT 10µF + 47k sin buffer; divisor PAD_SEL 10k/10k |
| 03 | `03-adc` | ADS1115 0x48 faders AIN0–3; 0x49 expresión/Sharp |
| 04 | `04-mcp-io` | **Vacía**, reservada; MCP 0x23 vive en PCB-K |
| 05 | `05-cv-dac` | MCP4725 0x60/0x61, MCP6004 ganancia 1,5; 1k serie + BAT54S por CV |
| 06 | `06-surface` | JST-PH para jog, pots, selector, HP y FSR; RC 1k/100nF por pot |
| 07 | `07-midi` | Opto 6N138 y polarización; JST-PH para MIDI TRS **tipo A**, OUT 33Ω/10Ω |
| 08 | `08-connectors-panel` | IDC PCB-A, Qwiic, Serial7 M4, PCB-K, Sharp/expresión |
| 09 | `09-connectors-rear` | JST-PH hacia LINE IN/OUT, CV, pedal; E-drum DNP |
| 10 | `10-power` | J_5V_LED_IN → F1; LDO +3.3VA; NT1 AGND/GND; buffer LED y pull-ups I2C |

## Cross-reference: Teensy ↔ señales de PCB-A y panel (D8)

**Manda el pinout de PCB-A ya fabricada**, no la antigua columna Teensy de CONNECTORS.md.
Cinta IDC **1:1**. PCB-M distribuye cada señal al pin que usa el firmware actual:

| Teensy | Función | Net PCB-M | Ruta |
|---|---|---|---|
| T0 / T1 | MIDI RX/TX Serial1 | `MIDI_RX` / `MIDI_TX` | Opto IN / R_MIDI_T → J_MIDI_OUT; **no pasan por J1** |
| T2 / T3 | ENC1 A/B | `ENC1_A` / `ENC1_B` | J1_PCBA **5 / 7** |
| T4 / T5 | ENC2 A/B | `ENC2_A` / `ENC2_B` | J1_PCBA **9 / 10** |
| T6 / T9 | ENC3 A/B | `ENC3_A` / `ENC3_B` | J1_PCBA **13 / 15** |
| T10 / T11 | ENC4 A/B | `ENC4_A` / `ENC4_B` | J1_PCBA **17 / 19** |
| T12 / T30 | ENC5 A/B | `ENC5_A` / `ENC5_B` | J1_PCBA **8 / 21** |
| T31 / T32 | ENC6 A/B | `ENC6_A` / `ENC6_B` | J1_PCBA **23 / 25** |
| T33 / T34 | ENC7 A/B | `ENC7_A` / `ENC7_B` | J1_PCBA **28 / 29** |
| T35 / T36 | ENC8 A/B | `ENC8_A` / `ENC8_B` | J1_PCBA **27 / 24** |
| T13 | LEDs teclas | `LED_DATA` | 74AHCT1G125 → `LED_DIN_5V` → **J_PCB_K.5**; J1.26 NC |
| T14–T17 | FSR velocity 1–4 | `FSR1_ADC`–`FSR4_ADC` | J_FSR1–4, divisor 10k a GND |
| T18 / T19 | I2C SDA/SCL | `I2C_SDA` / `I2C_SCL` | J2_I2C.3/.4, J_PCB_K.3/.4 y convertidores |
| T22 | MASTER | `POT_MASTER` | J_POT_MASTER.2 → **1k → ADC; 100nF a GND** |
| T24 | CUE | `POT_CUE` | J_POT_CUE.2 → **1k → ADC; 100nF a GND** |
| T26 | IN | `POT_IN` | J_POT_IN.2 → **1k → ADC; 100nF a GND** |
| T28 / T29 | M4 RX/TX Serial7 | `M4_RX7` / `M4_TX7` | J3_M4.4 / .3 (cruce hacia TX/RX M4) |
| T37 / T40 | Jog A/B | `JOG_A` / `JOG_B` | J_JOG.2/.3; clic por .4 hacia PCB-K |
| T41 | Selector pad | `PAD_SEL_ADC` | J_PAD_SEL.2 y divisor permanente 10k/10k |
| T7 / T8 / T20 / T21 / T23 | I2S TX/RX/LRCLK/BCLK/MCLK | `I2S_*` | Internos al módulo Audio Shield; sin ruta al panel |
| T25 / T27 / T38 / T39 | E-drum reservado | NC | No se asigna función nueva; J_EDRUM DNP |

| Señal de PCB-A | J1_PCBA | Destino PCB-M |
|---|---|---|
| FADER1 / FADER2 / FADER3 / FADER4 | **16 / 14 / 12 / 11** | U_ADS1 **AIN0 / AIN1 / AIN2 / AIN3** (pines 4/5/6/7), dirección **0x48** |
| +5V | 1 / 2 | +5V_LED después de F1 |
| GND | 3 / 4 / 6 | GND |
| +3V3 | 30 | +3.3V digital |
| Spare | **18** | **NC**, sin uso |
| INT_K / INT_A | **20 / 22** | **NC**, MCP por sondeo I2C |
| LED_DIN | **26** | **NC**, LED solo por J_PCB_K |

**I2C no va por el IDC J1:** llega a PCB-A por J2. La PCB-A fabricada no se modifica salvo
**desoldar R1/R2** al instalar PCB-M (D14). No conectar además la antigua cadena PCB-A J4 → K1/K2
al usar la nueva PCB-K por J_PCB_K.

## Conectores hacia panel y otras placas

PH = JST-PH 2,0 mm; normalmente BnB-PH-K vertical. J_5V_LED_IN usa S2B-PH-K horizontal de la BOM.

| Ref | Tipo | Pines en orden 1→n |
|---|---|---|
| J1_PCBA | IDC 2×15 | Pinout fabricado de CONNECTORS.md; destinos en tablas anteriores |
| J2_I2C | JST-SH 4 Qwiic | GND / +3.3V / SDA / SCL |
| J3_M4 | PH 4 | GND / **NC** / M4_TX7 / M4_RX7; M4 alimentada por su USB al Pi |
| J_PCB_K | PH 8 | GND / +3.3V / SDA / SCL / LED_DIN_5V / +5V_LED / JOG_CLICK / PEDAL_BTN |
| J5_SHARP1, J6_SHARP2 | PH 3 cada uno | +5V_LED / SHARPx_ADC / GND |
| J7_EXP1, J8_EXP2 | PH 2 cada uno | +3.3V / EXPx_ADC, pulldown 10k |
| J_JOG | PH 4 | GND / JOG_A / JOG_B / JOG_CLICK |
| J_POT_MASTER/CUE/IN | PH 3 cada uno | +3.3V / POT_x_WIPER / GND; cursor **antes** del RC |
| J_PAD_SEL | PH 3 | +3.3V / PAD_SEL_ADC / GND |
| J_FSR1–4 | PH 2 cada uno | +3.3V / FSRx_ADC; pulldown 10k |
| J_5V_LED_IN | PH 2 S2B-PH-K | **+5V_LED_RAW / GND** desde cable USB-A del Pi |
| J_5V_BKUP | PH 2, **DNP** | +5V_LED_RAW / GND; solo alternativa, no conectar ambas entradas |
| J_EDRUM | PH 5, **DNP** | Todos NC |

### Cableado jack ↔ JST-PH (D15)

Los jacks se compran en **Bogotá**, se fijan **con tuerca a la pared trasera del cuerpo** y van
por cable a los JST-PH del borde trasero de PCB-M. No hay jacks montados en esta PCB. Las hojas
06/07/09 son agrupaciones eléctricas: colocar también J_HP y los JST MIDI en el borde trasero
cuando se haga layout. Confirmar por continuidad los terminales del jack comprado antes de soldar;
la tabla usa **T = tip/punta, R = ring/anillo, S = sleeve/manguito**, no la numeración de sus patillas.

| Jack externo | Ref en PCB-M | JST-PH | Cableado hacia contactos del jack |
|---|---|---|---|
| LINE IN L 6,35 mm TRS | J_LINE_IN_L | B3B-PH-K, 3 vías | **1→T** LINE_IN_L_T, **2→R** LINE_IN_L_R, **3→S** AGND |
| LINE IN R 6,35 mm TRS | J_LINE_IN_R | B3B-PH-K, 3 vías | **1→T** LINE_IN_R_T, **2→R** LINE_IN_R_R, **3→S** AGND |
| LINE OUT L 6,35 mm TS/TRS | J_LINE_OUT_L | B3B-PH-K, 3 vías | **1→T** LINE_OUT_L_T después de 10µF, **2 NC**, **3→S** AGND; ring sin cable si es TRS |
| LINE OUT R 6,35 mm TS/TRS | J_LINE_OUT_R | B3B-PH-K, 3 vías | **1→T** LINE_OUT_R_T después de 10µF, **2 NC**, **3→S** AGND; ring sin cable si es TRS |
| PEDAL 6,35 mm TRS | J_PEDAL | B3B-PH-K, 3 vías | **1→T** PEDAL_BTN, **2 NC** (R sin cable), **3→S** GND |
| CV1 3,5 mm TS | J_CV1 | B2B-PH-K, 2 vías | **1→T** CV1_PROTECTED, **2→S** AGND |
| CV2 3,5 mm TS | J_CV2 | B2B-PH-K, 2 vías | **1→T** CV2_PROTECTED, **2→S** AGND |
| MIDI IN 3,5 mm TRS **tipo A** | J_MIDI_IN | B3B-PH-K, 3 vías | **1→T** MIDI_IN_TIP (DIN5), **2→R** MIDI_IN_RING (DIN4), **3 NC** (sleeve/DIN2 sin cable) |
| MIDI OUT 3,5 mm TRS **tipo A** | J_MIDI_OUT | B3B-PH-K, 3 vías | **1→T** MIDI_OUT_TIP (DIN5), **2→R** MIDI_OUT_RING (DIN4), **3→S** GND (DIN2) |
| Audífonos cue 6,35 mm TRS | **J_HP** (06) | B3B-PH-K, 3 vías | **1→T** HP_L, **2→R** HP_R, **3→S** HP_VGND |

**HP_VGND es una salida activa del SGTL5000, nunca GND/AGND.** Se elimina el duplicado J_HP_CUE:
J_HP es el único conector hacia el jack de audífonos que ya iba por cable.
**Detección de inserción:** los símbolos anteriores eran AudioJack2/3 sin switches; ninguno tenía
pin ni red de detección. Por tanto se conservan todos sus contactos actuales y no se inventa un
pin de detección. Si el jack adquirido trae contactos conmutados, dejarlos sin cable y verificar
que T/R/S se identificaron correctamente. **LINE IN ring sigue sin terminación balanceada definida**;
se conserva la red por cable, pendiente de decisión eléctrica antes de fabricar.

### Selector en panel (D13)

R_SEL_T/R_SEL_B de 10k quedan en PCB-M: +3.3V → 10k → **PAD_SEL_ADC** → 10k → GND.
Se elimina **SW_PAD_SEL**. Cablear el **común del polo C** a J_PAD_SEL.2, sus contactos extremos
a .3 (GND) y .1 (+3,3 V), y **dejar el contacto central sin cable**. Lectura 0 / 1,65 / 3,3 V;
no se necesita un cuarto hilo ni una escalera en el panel. La correspondencia con 0/−10/−20 dB
se verifica con los polos A/B y el firmware en banco.
SW_PAD representa los polos A/B del selector de panel, con header 2×4 en PCB-M: contactos
1 LINE_IN_L_T, 2 TAP10_L, 3 común PAD_L, 4 TAP20_L; 5 LINE_IN_R_T, 6 TAP10_R,
7 común PAD_R, 8 TAP20_R. No montar un selector en la placa.

## Alimentación, masas y protecciones (D9–D12/D14)

Cable USB-A del Pi (solo alimentación): VBUS → J_5V_LED_IN.1, retorno → .2. F1 MF-MSMF075
**0,75 A** entre +5V_LED_RAW y +5V_LED. El riel protegido alimenta LEDs de teclas (también
J1.1/.2), Sharp, buffer LED y las cargas ya capturadas (MCP6004 CV y entrada LDO).
+5V_TEENSY desde el USB propio permanece separado. Respetar el límite de brillo/consumo de `24` §5;
no conectar en caliente. **No se añade TVS/diodo de entrada** porque no hay pieza adecuada en la
BOM; BAT54S y 1N4148W son protecciones de señal, no una TVS USB ni bloqueo inverso. Pendiente
seleccionar esa protección si se revisa la BOM.

**NT1 Device:NetTie_2 / NetTie:NetTie-2_SMD_Pad0.5mm**, pin1 AGND, pin2 GND, es la única unión
entre estas redes. En layout colocarlo **junto a U_LDO1**, regulador de +3.3VA; no crear puentes
adicionales ni unir HP_VGND. El netlist conserva los nombres GND/AGND y la huella define la unión
de cobre admitida por DRC; el esquema no prueba la colocación física.

Cada CV tiene **1k serie** desde CVx_OUT (realimentación antes del resistor) hasta
**CVx_PROTECTED** en el cable, con **BAT54S**: pin1 GND, pin2 +5V_LED, pin3 nodo protegido.
Cada pot lleva **1k desde el cursor y 100nF a GND en el lado ADC** (τ=100µs, corte ≈1,59kHz).

MIDI OUT **3,3 V tipo A**: R_MIDI_R **33Ω ≥0,5W** de +3.3V a ring, R_MIDI_T **10Ω ≥0,25W**
de MIDI_TX a tip. Se elimina R_MIDI_S=100Ω y la anterior pareja de 47Ω. Referencia:
[MIDI Association, CA-033 (2014)](https://midi.org/5-pin-din-electrical-specs),
[figura 1, p.2, publicada por AMEI](https://amei.or.jp/midistandardcommittee/Recommended_Practice/ca33-j.pdf).
Footprints 1210/1206 respectivamente; seleccionar resistores con esa potencia nominal.

**Pull-ups I2C en PCB-M:** R_SDA/R_SCL 2,2k a +3.3V. **Desoldar R1/R2 de PCB-A** al integrarla;
con ambos pares quedarían 1,1k. No modificar pistas ni el pinout de PCB-A. Revisar el total de
pull-ups de los módulos ya instalados antes de elevar el bus a 400kHz.

## BOM de referencia PCB-M

Números LCSC heredados; comprobar stock y potencia al comprar. No se especifica referencia de
jack de PCB: todos son de panel comprados localmente. Componentes nuevos sin SKU cerrado se
identifican por valor, encapsulado y potencia, sin inventar equivalentes.

| Referencia | Valor / pieza | LCSC heredado | Encapsulado / notas |
|---|---|---|---|
| U_ADS1/2 | ADS1115IDGSR | C37593 | TSSOP-10_3x3mm_P0.5mm de librería KiCad |
| U_DAC1/2 | MCP4725A0T-E/CH | C144198 | SOT-23-6 |
| U_OA_CV / U_OA_IN | MCP6004 | C510498 | SOIC-14; unidades libres como seguidores |
| U_OPT_MIDI | 6N138 | C3001991 | DIP-8 |
| U_LED_BUF | 74AHCT1G125GW | C7466 | SC-70-5 |
| U_LDO1 | TLV75533PDBV | C15578 | SOT-23-5, +3.3VA |
| F1 | MF-MSMF075 0,75A | C17357 | 1206, según BOM vigente |
| NT1 | NetTie_2 | — | NetTie-2_SMD_Pad0.5mm, cobre; no componente comprado |
| R_CV_SER1/2 | 1k | — | 0402 |
| D_CV_CLAMP1/2 | BAT54S | — | SOT-23, misma pieza de señal del frente |
| R_POT_MASTER/CUE/IN | 1k | — | 0402 |
| C_POT_MASTER/CUE/IN | 100nF | — | 0402 |
| R_CV_RF1/2 / R_CV_RI1/2 | 10k / 20k | — | 0402, ganancia 1,5 |
| R_MIDI_R / R_MIDI_T | 33Ω ≥0,5W / 10Ω ≥0,25W, ±5% o mejor | — | **1210 / 1206**, CA-033 |
| R_SDA / R_SCL | 2,2k 1% | C25879 | 0402; retirar R1/R2 de PCB-A |
| R_FSR1–4 | 10k 1% | C25744 | 0402 |
| R_SEL_T/B | 10k | — | 0603, divisor en PCB-M |
| C_LDO_IN / C_LDO_OUT | 10µF / 1µF | C15850 / C52923 | 0805 / 0402 |
| J_5V_LED_IN | **JST PH S2B-PH-K** | **C157929** | 2 vías, horizontal; familia ya usada por FSR |
| J_FSR1–4, J_CV1/2 | JST PH B2B-PH-K | — | 2 vías vertical (revisar variantes frente a S2B de BOM anterior) |
| J5_SHARP1/J6_SHARP2, J_HP, J_POT_x, J_PAD_SEL, J_LINE_IN/OUT_L/R, J_PEDAL, J_MIDI_IN/OUT | JST PH B3B-PH-K | — | 3 vías vertical; reemplazan las entradas de jacks SMD de la BOM anterior |
| J1_PCBA | IDC 2×15 | — | IDC-Header_2x15_P2.54mm_Vertical; verificar mecánica |

Desacoplos por IC capturados: ADS1115/MCP4725/MCP6004 CV 100nF + 10µF; buffer LED y opto 100nF;
LDO y frente conservan sus capacitores. No puebla MCP23017 en PCB-M.

## Decisiones resueltas y pendientes

D8–D15 están registradas en `24` §8 (2026-11-12, **aprobado por Juan**). Quedan resueltos los
antiguos conflictos de J1/cross-reference, duplicación del selector, falta de entrada primaria,
masas sin unión, protecciones CV/pots, valores MIDI, pull-ups duplicados y montaje de jacks.
La corrección previa HP/LINE OUT se conserva: HP_VGND aislada, LINE OUT después del capacitor.
La ubicación MCP 0x23 y el conteo 36/56 quedan consistentes con `17` y `24` §9.

Pendientes fuera de estas decisiones: terminación ring de LINE IN; función T27 frente al Brain
(J_EDRUM DNP); selección mecánica del selector/jacks y componentes sin SKU; protección de entrada
USB sin pieza en BOM; presupuesto de consumo medido, banco MIDI/CV/audio, layout/DRC.
Ver [REPORT.md](../../tools/REPORT.md) para el alcance exacto de la verificación.

## Dimensiones y layout pendiente

Referencia `25-panel-consola.md`: PCB-M **186 × 60 mm**, x140–326 / y276–336 mm en panel.
Los jacks pertenecen a la pared del cuerpo: sus pitches no fijan las huellas de PCB-M. Colocar los
JST de cada jack en el borde trasero, J_PCB_K hacia el centro e IDC J1 hacia PCB-A; NT1 junto a U_LDO1.
El archivo `.kicad_pcb` y el proyecto no se modifican en esta captura; la colocación queda pendiente.

## Regeneración y verificación

Desde la raíz:

```sh
python3 -m pytest -q apps/hardware/kicad/consola/tools/tests --basetemp=.schgen-test-tmp
python3 apps/hardware/kicad/consola/tools/check_netlist.py --regenerate
```

No editar las hojas generadas; cambiar `tools/schgen/pcb_m.py`. Sin `--regenerate` se verifican
las hojas existentes sin sobrescribirlas. XML compara pines, NC y particiones/nombres de redes;
ERC no acredita montaje ni rendimiento analógico. Son **11 páginas** (raíz + 10 subhojas).

**Resultados reales (KiCad 10.0.6): 34 tests pasan; Connectivity PASS: 122 componentes,
415 pines, 23 NC, 103 redes con nombre; ERC 0 errores y 7 avisos.** Cinco avisos son I2S
interno al módulo y dos los anillos LINE IN aún sin terminación; no hay exclusiones nuevas.
PDF de 11 páginas rasterizado y revisado; 11 esquemas idénticos byte a byte al regenerar.
La CLI conserva el aviso de anotación de referencias descriptivas heredadas; no omite pines.
Detalle íntegro en REPORT.md.

| Archivo | Contenido |
|---|---|
| `docs/pcb-m-schematic.pdf` | Esquema completo |
| `docs/pcb-m-netlist.xml` | Netlist XML verificado |
| `docs/erc-report.txt` | ERC íntegro, sin exclusiones añadidas |
