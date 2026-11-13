# Consola: placa principal (PCB-M)

> **Fecha:** 2026-11-12. **Estado:** decisiones D1–D43 aprobadas por Juan (§8); esquema PCB-M en captura,
> layout pendiente del panel (`25-panel-consola.md`). **Base:** ADR-008 §2 y §6 (una Teensy; presupuesto de consumo obligatorio antes de la placa, auditoría G30),
> `03-arquitectura-capas.md` §4 (una placa principal reemplaza portadora + PCB-S + PCB-A; se mantienen las placas de
> botones y faders), `17-mapa-pines-teensy.md` (pines e I2C), `18-teensy-unica-firmware.md`,
> `pendientes.md` (chips soldados por JLC), placas existentes en `apps/hardware/kicad/consola/boards/` y su contrato
> `boards/CONNECTORS.md`.

## 1. Qué es

Una placa, **PCB-M**, que lleva la Teensy 4.1 en zócalo y todo lo que 17 conecta a ella y no vive en el panel:
códec, convertidores, CV, reserva MIDI TRS tipo A DNP, conectores para perillas de audio y los
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
| NeoTrellis M4 | JST-PH 4: GND, n.c., RX7 (28), TX7 (29) | Serial7 | La M4 sigue con USB al Pi (alimentación y carga de firmware) |
| LEDs de botones | Pin 13 → 74AHCT1G125 → J_PCB_K.6 | 13 | J1_PCBA.26 NC; no usar simultáneamente la antigua ruta por PCB-A |
| MIDI TRS tipo A | IN 6N138; OUT 33 Ω (0,5 W) a 3,3 V y 10 Ω (0,25 W) desde TX | DNP; 0 EN_K / 1 EN_A | D20 sustituye D7; D12; tip = DIN5, ring = DIN4, sleeve OUT = GND, sleeve IN NC |
| Pedal | Jack TRS 6,35 mm a MCP 0x23 | — | Nota 122 |
| Triggers e-drum | Conector (DB15 o RJ45) + huellas sin poblar del acondicionamiento | 25, 27, 38, 39 | Reservado |
| I2C | Un par seleccionable 2,2/4,7 kΩ por segmento local/B_K/B_A, dos PCA9517A en PCB-M; quitar R1/R2 de PCB-A | 18/19, EN_K 0 / EN_A 1 | D18/D21; pull-ups remotos DNP |
| Alimentación | J_5V_LED_IN desde USB-A de Pi → F1 0,75 A → +5V_LED; LDO → +3.3VA | — | §5; AGND/GND solo por NT1 junto al LDO (D9/D10) |

## 4. Conectores al exterior (jacks de panel por cable)

Sin fuente externa adicional (D6); J_5V_LED_IN recibe un cable de un USB-A del Pi (D9) · USB de la Teensy (interno, cable corto al Pi) · audífonos cue (HP out) · LINE IN estéreo ·
CV 1–2 · pedal · expresión 1–2 · e-drum (reservado). La salida de sala es la del **Audio+ del
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
| USB-A 1 (datos + 5 V) | Teensy, LDO analógico/ADS/frente y lógica de 3,3 V y amplificador CV filtrado | 0,15 A | 0,25 A |
| USB-A 2 (datos + 5 V) | NeoTrellis M4 | 0,5 A | 0,6 A (brillo ≤ 25 %) |
| USB-A 3 (solo 5 V, cable de alimentación) | 26 LEDs de las teclas, Sharp ×2, buffer LED | 0,5 A | 0,6 A (brillo ≤ 25 %) |
| **Suma de USB** | | **1,15 A** | **1,45 A ≤ 1,6 A** |

Condiciones: brillo de LEDs **limitado por firmware sin excepción** (teclas y M4), polyfuse de 0,75 A en la entrada
de 5 V de LEDs de PCB-M, los 5 V de la Teensy y los de LEDs **no se unen**: separación por rama. **NO cortar VUSB–VIN** de Teensy (D35); su USB es la única fuente de +5V_TEENSY. Riesgo: en el peor caso del Pi (CPU al 100 % + NVMe + USB al tope) la fuente queda al límite: **medir en banco
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
| **D7** | ~~MIDI DIN poblado en v1~~ Sustituida por D20: MIDI hardware DNP, conversor USB en Pi |
| **D8** | **2026-11-12, aprobado por Juan:** manda el pinout de PCB-A fabricada en CONNECTORS.md. PCB-M lleva cada ENC a ENCODER_PINS de ConsolaMap.h/17; FADER1–4 → ADS1115 0x48 AIN0–3. J1.18 (spare), .20 (INT_K), .22 (INT_A), .26 (LED_DIN) NC; LEDs por J_PCB_K |
| **D9** | **2026-11-12, aprobado por Juan:** J_5V_LED_IN JST-PH S2B-PH-K (BOM histórica errónea, corregida D25: C173752), pin1 +5V_LED_RAW / pin2 GND desde USB-A de Pi; F1 0,75 A → +5V_LED (teclas, Sharp, buffer y cargas ya capturadas). Sustituida por D26/D33: LDO desde Teensy y protección TVS/PMOS en rama LED |
| **D10** | **2026-11-12, aprobado por Juan:** AGND y GND unidas solo por NT1, símbolo Device:NetTie_2 y footprint estándar NetTie:NetTie-2_SMD_Pad0.5mm, junto a U_LDO1 (+3.3VA) en layout; HP_VGND permanece aislada |
| **D11** | **2026-11-12, aprobado por Juan:** cada CV lleva 1 kΩ en serie y BAT54S desde el nodo del jack a GND/+5V_LED; cada pot MASTER/CUE/IN lleva 1 kΩ serie desde cursor y 100 nF a GND en el lado ADC |
| **D12** | **2026-11-12, aprobado por Juan:** MIDI OUT a 3,3 V según MIDI Association CA-033 (2014): 33 Ω al riel (≥0,5 W), 10 Ω desde TX (≥0,25 W), sin 100 Ω adicional. TRS **tipo A**, tip DIN5, ring DIN4, sleeve DIN2; IN sleeve NC, OUT sleeve GND |
| **D13** | **2026-11-12, aprobado por Juan:** selector en panel; eliminar SW_PAD_SEL de PCB-M. J_PAD_SEL 3 hilos (+3,3 V / PAD_SEL_ADC / GND) y escalera 10k/10k en PCB-M. Polo C: común al ADC, extremos a rieles, contacto central sin cable (divisor da 1,65 V); polos A/B de audio siguen por cable |
| **D14** | **2026-11-12, aprobado por Juan:** pull-ups I2C de 2,2 kΩ en PCB-M; **desoldar R1/R2 de PCB-A** para esta integración. Conservar el pinout fabricado; no se modifican archivos de PCB-A |
| **D15** | **2026-11-12, aprobado por Juan:** jacks comprados en Bogotá, con tuerca en pared trasera del cuerpo y cableados; sustituir huellas de jacks por JST-PH en borde trasero, uno por LINE IN L/R, LINE OUT L/R, PEDAL, CV1/2, MIDI IN/OUT y HP (J_HP existente). Conservar detección de inserción si existe: la captura actual no tiene contacto de detección en ninguno; no se asigna señal nueva |
| **D16** | **2026-11-12, aprobado por Juan:** TP de rieles/retornos, VREF, CV, LEDs e I2C local y por rama; LEDs 0603 ≈1 mA en 3V3, 3V3A y 5V_LED |
| **D17** | **2026-11-12, aprobado por Juan:** SJ_FRONT/SJ_CV/SJ_LED_K/SJ_SHARP/SJ_LED_DATA cerrados; 0 Ω 0805 R_ISO_IN_L/R antes de LINE IN del shield; conservar JP_BYP. No se hace protoboard; fallo de entrada/CV se aísla y la consola continúa sin esa función |
| **D18** | **2026-11-12, aprobado por Juan:** pull-ups seleccionables en cada segmento (local y B de A/K): 2,2 kΩ cerrados por defecto, 4,7 kΩ abiertos alternativos; remotos DNP. Sustituye el bus único de D14 |
| **D19** | **2026-11-12, aprobado por Juan:** SJ_CV → 10 Ω + 47 µF → +5V_CV; conservar ganancia 1,5, medir margen de saturación y caída bajo carga |
| **D20** | **2026-11-12, aprobado por Juan:** toda hoja 07 MIDI DNP, excluida del BOM de ensamble; conversor USB en Pi y cuerpo sin agujeros MIDI. Asignación histórica sustituida por D30: Teensy 0/1 son EN_K/EN_A; TP/ajustes MIDI son solo reserva DNP |
| **D21** | **2026-11-12, aprobado por Juan:** 2 × NXP PCA9517ADP,118 C80799 MSOP-8: lado A local, lado B independiente hacia K y A; VCCA/VCCB 3,3 V; asignación original EN común sustituida por D30: EN_K/EN_A separados desde Teensy 0/1 con 10 kΩ y puentes individuales |
| **D22** | **2026-11-12, aprobado por Juan:** 47 Ω serie 0805 (0 Ω si no hace falta) y ESD 2 líneas <10 pF por cada salida I2C de cable. Selección verificada USBLC6-2SC6 ST C7519, SOT-23-6, 3,5 pF máx. J1_PCBA IDC no tiene I2C: su protección SDA/SCL está en J2_I2C, no se inventan pines en J1 |
| **D23** | **2026-11-12, aprobado por Juan:** J_PCB_K y PCB-K izquierda J1 pasan a JST-PH **10 vías**, B10B-PH-K-S(LF)(SN) C158038. 1 3V3 / 2 SDA / 3 GND / 4 SCL / 5 GND / 6 LED_DIN_5V / 7 GND / 8 5V_LED / 9 JOG_CLICK / 10 PEDAL_BTN. Sustituye 8 vías y mantiene pedal |


| **D24** | **2026-11-12, aprobado por Juan:** UART real Adafruit trellis_m4: TX21=SDA, RX22=SCL. Teensy RX7/28 a J3_M4.3/CONN1.3 SDA/TX21; TX7/29 a .4/CONN1.4 SCL/RX22. CONN1.1 GND, .2 3V3 NC. Evita TX contra TX; corregir macros discordantes en tarea firmware. |
| **D25** | **2026-11-12, aprobado por Juan:** identidad de compra en campo LCSC de cada componente físico; catálogo verificado con fabricante/MPN/encapsulado/huellas/fecha/fuente. Código no confirmado queda vacío y bloquea cierre de compras, nunca se adivina. BOM sale del esquema, no del README. F1 MF-MSMF075/24-2 es 1812. También C157929 era S3B de tres vías: para S2B dos vías usar identidad verificada C173752. |
| **D26** | **2026-11-12, aprobado por Juan:** LDO +3.3VA desde +5V_TEENSY (IN y EN), no LED. ADS, frente, faders y expresión comparten origen Teensy. Sharp→ADS 10 kΩ serie; DAC→MCP6004 CV **2,2 kΩ** serie (1 kΩ no limita 3,3 V/1 kΩ a ±2 mA). Ver análisis de secuencia abajo. |
| **D27** | **2026-11-12, aprobado por Juan:** CV2 es función poblada. Firmware escribe cero a **0x60 y 0x61** al inicio de setup, antes de audio/superficie; provisionar EEPROM cero una vez y comprobarla, no escribirla cada arranque. Sin mute añadido: un switch doble requeriría alimentación/estado seguro propios; EEPROM no garantiza cero durante reset con DAC aún alimentado. |
| **D28** | **2026-11-12, aprobado por Juan:** Audio Shield Rev D por tres mazos cortos soldados a pads reales de 2,54 mm: LINE IN L/R/G, LINE OUT L/R/G y HP L/R/VGND hacia JST-PH funcionales en PCB-M. G del shield a GND (ya común a Teensy), sin segunda unión a AGND; NT1 conserva la estrella. No son headers directos del shield. Dos pads sin rótulo NC; HP_VGND ≠ GND, advertencia en esquema y montaje. Fotos y mapa en README PCB-M. |
| **D29** | **2026-11-12, aprobado por Juan:** pedal en PCB-M junto a J_PEDAL: 1 kΩ serie, TVS ESD unidireccional de una línea 3,3 V a GND, pull-up 10 kΩ a +3.3V y 10 nF a GND del lado protegido antes de J_PCB_K. Limita corriente/ruido del cable externo; debounce sigue en firmware. |
| **D30** | **2026-11-12, aprobado por Juan:** EN_K en Teensy 0, EN_A en Teensy 1, cada EN con 10 kΩ pull-up y SolderJumper GPIO independiente. Sustituye EN común de D20/D21; MIDI completo DNP. NXP §6.1 exige cambiar EN **solo con bus ocioso**; firmware debe gestionar/diagnosticar cada rama y preservar audio local cuando esté aislada. Rama permanentemente baja no está ociosa: no se garantiza recuperación EN en ese estado; apagar y abrir puentes de servicio. |
| **D31** | **2026-11-12, aprobado por Juan:** entrada A del buffer LED (Teensy 13) con 100 kΩ a GND; cada pot con 1 MΩ a GND **tras** RC. Cable suelto decae a cero (τ≈100 ms). Para pot 10 kΩ, Rth≤2,5 kΩ + serie 1 kΩ: error relativo ≤3,5k/1003,5k=0,349%; a media escala 0,174% FS, máximo ≈0,217% FS. Calibrar extremos si hace falta. |
| **D32** | **2026-11-12, aprobado por Juan:** FSR de la foto termina en dos machos prensados de 2,54 mm. Sustituir pads SMD provisionales por landing THT 1×2 de 2,54 mm apto para zócalo hembra o soldadura. Mantener datum disco CSV y orientar landing hacia disco; cola flexible debe acomodarse sin tensión, verificar longitud/plegado y altura con pieza real antes de layout final. PCB-P .kicad_pcb pertenece a otra tarea. |
| **D33** | **2026-11-12, aprobado por Juan:** entrada LEDs: conector → polyfuse → P-MOS AO3401A (D entrada/S carga/G GND) → +5V_LED, con SMAJ5.0A unidireccional a GND **después del fusible**. Fusible limita fallo sostenido de TVS; MOS evita pérdida Schottky: ≤0,043 V y 21 mW a 0,5 A con 85 mΩ, frente ≈0,2–0,25 V/0,10–0,125 W Schottky. TVS 5 V es standoff, no clamp ideal a 5 V; ensayar transitorios, no promete tolerancia a sobretensión continua. |
| **D34** | **2026-11-12, aprobado por Juan:** PCA9517ADP,118 C80799 usa NXP SOT505-1/TSSOP8 (MSOP-8 comercial), 3×3 mm, paso 0,65, ancho total ≈4,9 mm. Usar KiCad MSOP-8_3x3mm_P0.65mm, no asumir identidad solo por nombre VSSOP. |
| **D35** | **2026-11-12, aprobado por Juan:** **NO cortar VUSB–VIN** de Teensy. Su USB desde Pi es la única fuente de +5V_TEENSY; PCB-M no inyecta 5 V allí. Desoldar/no poblar R1/R2 PCB-A antes de integrar. Rev HW B-05 es incorrecta para este diseño. |
| **D36** | **2026-11-12, aprobado por Juan:** doc27 LED_DIN es J1.6 de PCB-K, no J1.5 (GND). Corrige referencia, sin cambiar cadena. |
| **D37** | **2026-11-12, aprobado por Juan:** completar anotación de referencias PCB-M con sufijo numérico válido conservando nombres descriptivos; BOM real sin «?», test no normaliza ni elimina ese carácter. No cambia conectividad. |

### Segunda revisión: decisiones aprobadas por Juan, 2026-11-12 (A1 actualizada 2026-11-13)

Estas decisiones sustituyen expresamente los puntos indicados de D11/D24/D26/D31/D32.

| # | Decisión |
|---|---|
| **D38 / A2** | SJ_CV recibe **+5V_TEENSY**, conserva RC 10 Ω + 47 µF y desacoplos 100 nF/10 µF a +5V_CV. DAC y amplificador comparten origen USB Teensy. +5V_LED conserva LEDs (incluido indicador), buffer 74AHCT1G125 y Sharp; los clamps superiores BAT54S pasan a **+5V_CV**, evitando realimentar LED desde CV activo al quitar USB LED. Sustituye también el riel superior de D11. |
| **D39 / A3** | Dos resistencias serie **1 kΩ 0805**, R_M4_RX1 y R_M4_TX1, junto a **J3_M4**, separan M4_RX7/T28 y M4_TX7/T29 de M4_RX7_CABLE/M4_TX7_CABLE. LCSC **C17513**, UNI-ROYAL 0805W8F1001T5E, 1 %, 125 mW. Limitación de corriente, no aislamiento Ioff ni autorización de hot-plug. |
| **D40 / A1** | **2026-11-13, decisión de Juan:** soldar el cable a los **pads rotulados de la M4 por función**, sin depender de la numeración de CONN1. **SDA = TX M4 → R_M4_RX1 1 kΩ → RX Teensy 28**, **SCL = RX M4 ← R_M4_TX1 1 kΩ ← TX Teensy 29**, **GND → GND**, **3V sin conectar** (J3_M4.2 también NC). J3_M4 conserva pines PCB-M 1=GND, 2=NC, 3=SDA/TX M4, 4=SCL/RX M4. Juan confirmará los rótulos impresos antes de soldar. Sustituye la numeración externa de D24: check_harness usa rótulos de pad y no avisa pendiente por CONN1. |
| **D41 / M1, M2, B1** | TVS unidireccionales con símbolo K1/A2 y marca K junto a pad1; mismas redes. check_bom cubre M/K izq./K der./A/F/P y exige conjuntos de referencias/atributos esquema↔XML↔BOM real. Exenciones por lib_id real TP/NT/SJ/agujero; nunca por texto de huella. Catálogo LCSC exige URL HTTPS exacta de producto; suministros externos conservan fuente de inventario separada. Faders RS6011 y seis FSR402 son de Juan. Zócalos FSR alternativos son compra externa sin código verificado y **no se pueblan** con D43. |
| **D42 / M3** | Pots MASTER/CUE/IN: **100 kΩ** (C25741) tras 1 kΩ/100 nF, sustituye 1 MΩ de D31. Error máximo ideal ≈2,108 % FS para pot de 10 kΩ; calibrar curva/extremos. Umbral de reposo ≤0,15 V sostenido 100 ms, esperar ≥100 ms desde conexión/boot; no es detección inequívoca de cable abierto. Firmware debe suprimir CC en esa zona y tratar pot mínimo/cable abierto como mismo estado seguro. |
| **D43 / M4** | Elegir **soldadura directa de los terminales metálicos prensados**, sin zócalo en los seis FSR. Huella THT P2.54 con inserción desde cara superior y soldadura inferior, contactos metálicos acondicionados para que TODO sobresaliente superior (terminal, crimpado, cola) sea ≤1,0 mm. Nada de calor sobre película; disipador entre metal y película, alivio de tensión fuera de la zona activa. Mantener centros de disco/landing; comprobar con pieza real antes de fabricar. Doc28 detalla solape/alturas. **Layout PCB-P debe rehacerse** por cambio de terminación/huella y envolvente; no se edita su .kicad_pcb en esta tarea. |

**Consumo añadido a +5V_TEENSY (D38):** MCP6004 completo 4×100 µA = 0,40 mA típico,
4×170 µA = 0,68 mA máximo de reposo bajo las condiciones de la tabla eléctrica Microchip.
Dos redes de feedback a 4,95 V: 2×4,95/30k = 0,33 mA; incremento sin carga ≈0,73 mA típico,
≈1,01 mA usando máximo IQ. Con dos cargas externas de 10 kΩ se suman ≤0,99 mA: reservar
**2 mA** continuos con esa carga (no cota para cortocircuito); caída RC ≈20 mV. El tope de rama
USB de 250 mA se mantiene como objetivo a medir, ahora incluyendo esos 2 mA. Inrush del
47+10 µF requiere ensayo; 5 V/10 Ω = 0,5 A inicial ideal breve, no consumo continuo.

**UART a 1 Mbps (D39):** `apps/consola/processor/include/shared/Config.h` define
`UART_BAUD_RATE=1000000`; `processor/lib/neotrellis_m4/UartInterface/UartInterface.cpp` lo
aplica a Serial1. `apps/consola/surface/src/Surface.cpp` usa Serial7.begin(1000000)
en Teensy: ambos extremos a **1 Mbps**, bit=1 µs. Con presupuesto
**C total receptor+cable ≤100 pF**, R máxima 1,01 kΩ: τ≤101 ns, 10–90 %≤222 ns,
cruce 0,7×VDD≤122 ns; a mitad del bit se alcanza ≥99,2 % del nivel final. Sin divisor DC
salvo fugas del receptor. Un cable corto cumple por cálculo bajo ese presupuesto, no se ha
medido su capacitancia: verificar bordes/niveles/errores a 1 Mbps con mazo real; si C>100 pF,
revisar antes de aceptar. Con un lado apagado, corriente ideal ≤3,3/990=3,34 mA; **no** prueba
cumplimiento de NXP NVCC apagada ni elimina rail fantasma. Ensayar secuencias en ambos sentidos.

**Cable abierto de pot (D42):** para fracción de cursor x, Rth=10k·x·(1−x),
Vadc/3,3=x·100k/(100k+1k+Rth). Error absoluto máximo ≈0,02108 FS cerca de x=0,7174;
a media escala ≈1,691 % FS. Error relativo máximo 3,5k/103,5k≈3,382 %.
Cable abierto: τ=100k×100nF=10 ms; fuga de +1 µA da 0,10 V nominal, no cero.
Con R+1 % y C+20 %, τ≤12,12 ms y asíntota≤0,101 V: tras 100 ms desde 3,3 V
V≤0,102 V. Umbral 0,15 V deja ≈48 mV de margen en este modelo; configurar ADC sin
pull-up y validar fuga/contaminación/temperatura/muestreo. No confundir extremo mínimo
legítimo con avería: para distinguirlos hace falta información adicional. Llevar este contrato
a la tarea de firmware; aquí no se modifica código de firmware.

Fuentes: [Microchip MCP6004 tabla DC](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP6001-1R-1U-2-4-1-MHz-Low-Power-Op-Amp-DS20001733L.pdf),
[NXP RT1060 §4.2.1.3/tabla 22](https://www.nxp.com/docs/en/nxp/data-sheets/IMXRT1060CEC.pdf),
[1 kΩ 0805 C17513](https://www.lcsc.com/product-detail/C17513.html).

### Secuencia de alimentación y límites (D26–D27)

Sin USB LED: Teensy, +3.3V, +3.3VA, ADS, frente y sensores excitados por Teensy siguen activos;
LEDs y Sharp quedan sin alimentación; amplificador CV sigue desde Teensy (D38). Las resistencias
DAC→CV de 2,2 kΩ se conservan, sin atribuirles protección de tensión con operacional apagado.
Sin USB Teensy: ADS/frente/faders/expresión/DAC/amplificador CV apagados; Sharp/LED pueden seguir activos.
10 kΩ limita una salida Sharp hasta 5 V a ≤0,50 mA en ADS apagado, menor que ±10 mA.
No confundir limitar corriente con operación analógica válida: ADS AIN ≤VDD+0,3 V y
MCP6004 VIN ≤VDD+1 V son límites absolutos; apagar/inyección requiere ensayo de rail fantasma,
secuencia lenta, SJ_CV abierto y temperatura. No interpretar lecturas sin su alimentación.
Para EN, tanto bus global como puerto repetidor deben estar ociosos (NXP §6.1). Timeout no equivale a bus ocioso: no conmutar arbitrariamente una rama retenida en bajo; usar aislamiento manual sin alimentación y registrar fallo.
El DAC restaura EEPROM al POR (de fábrica media escala); un reset solo de MCU no es POR del DAC.
Cero previo a firmware depende del provisionado y del historial de alimentación; no hay mute garantizado.
La M4 tiene USB propio: comprobar UART y LEDs sin su rail antes de permitir conexión en caliente.

Fuentes verificadas 2026-11-12: [Adafruit variant.h](https://raw.githubusercontent.com/adafruit/ArduinoCore-samd/master/variants/trellis_m4/variant.h),
[TI ADS1115 §5.1](https://www.ti.com/lit/gpn/ADS1115),
[Microchip MCP6004 §1/§4.1.2](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP6001-1R-1U-2-4-1-MHz-Low-Power-Op-Amp-DS20001733L.pdf),
[MCP4725 §5.4](https://ww1.microchip.com/downloads/en/devicedoc/22039d.pdf),
[NXP PCA9517A §6.1/§13](https://www.nxp.com/docs/en/data-sheet/PCA9517A.pdf),
[AO3401A](https://www.aosmd.com/sites/default/files/res/datasheets/AO3401A.pdf).

## 9. Cambio 2026-11-11: el 4.º MCP pasa a PCB-K

Con la distribución v1 (`25` §7) las 26 teclas van en **una sola PCB-K**; Juan decidió poner ahí también el **MCP
0x23** junto a 0x21 y 0x22. PCB-M ya no lleva MCP: el clic del jog y el pedal (que entran a PCB-M por cable) viajan
a PCB-K por el cable PCB-M ↔ PCB-K, junto con I2C, LED_DIN, +5 V de LEDs y GND. PCB-K reemplaza a K1/K2.


## 10. Diagnóstico y reparación

**Aprobado por Juan, 2026-11-12; especificación previa a la captura DFT.** Primera fabricación:
localizar un corto, medir una señal y aislar una carga debe ser posible sin cortar pistas.
Es una ampliación de servicio de D9–D15. D23 cambia ambas puntas M–K; PCB-A queda congelada.

### 10.1 Acceso con instrumentos

Todos los TP usan `Connector:TestPoint` estándar y `TestPoint:TestPoint_Pad_D1.5mm` (SMD de
1,5 mm). Conservar TP_VREF y TP_BUF_L/R existentes. Añadir TP en +5V_TEENSY (riel del socket,
no acceso separado al VUSB físico del módulo), +5V_LED_RAW antes de F1, +5V_LED después de F1,
+3.3V, +3.3VA, AGND y GND: permiten distinguir falta de fuente, fusible abierto y caída del LDO,
y medir ambos retornos sin confundirlos con HP_VGND. Añadir también +3.3VA_FRONT y +5V_CV,
para medir después de sus aislamientos/filtro.

TP en I2C_SDA/SCL local y en ambos lados de las resistencias de cada rama permite medir nivel bajo y tiempo de subida con el mazo real; en LED_DIN_5V,
MIDI_RX/TX (reserva DNP, sin unión a Teensy) permite seguir datos sin tocar patas pequeñas. TP CV1_OUT/CV2_OUT mide antes del
resistor de protección; TP CV1_PROTECTED/CV2_PROTECTED permite comparar con el jack bajo carga.
En layout: TP accesibles por la cara de servicio, serigrafía de red, GND/AGND cerca de señales y
separación suficiente para la punta. El esquema no acredita esa accesibilidad física.

### 10.2 Aislamiento reversible

Puentes `Jumper:SolderJumper_2_Bridged` con huella estándar cerrada por cobre; para abrir se
retira el puente de cobre y para cerrar se resuelda. No basta aspirar estaño de una huella virgen.

| Puente | Recorrido | Por qué |
|---|---|---|
| SJ_FRONT | +3.3VA → +3.3VA_FRONT | Aislar MCP6004 de entrada, clamps y divisor VREF; ADS siguen en +3.3VA |
| SJ_CV | +5V_TEENSY → +5V_CV_IN | Aislar alimentación del amplificador CV y su filtro |
| SJ_LED_K | +5V_LED → +5V_LED_K → J_PCB_K.8 | Localizar cortos del mazo/LEDs de teclas |
| SJ_SHARP | +5V_LED → +5V_LED_SHARP → ambos Sharp y sus 10 µF | Distinguir la rama de sensores de la de LEDs |
| SJ_SDA_A / SJ_SCL_A | B de U_I2C_A → puente → 47 Ω → J2_I2C | Desconectar la rama PCB-A/F si mantiene el bus bajo |
| SJ_SDA_K / SJ_SCL_K | B de U_I2C_K → puente → 47 Ω → J_PCB_K | Desconectar la rama PCB-K independientemente |
| SJ_LED_DATA | salida LED_BUF_OUT → LED_DIN_5V | Separar buffer y mazo durante diagnóstico de datos |

**J1_PCBA no contiene SDA/SCL** (D8): no añadir señales ni puentes ficticios allí. Su I2C es el
J2_I2C adyacente. Los puentes abren solo el recorrido indicado; no constituyen aislamiento
galvánico ni eliminan alimentación parásita por datos/clamps. Manipular apagado; para medir
consumo aislado, desconectar también el mazo/señales que puedan realimentar el bloque.

### 10.3 Indicadores y pull-ups

Tres LEDs rojos 0603 a +3.3V, +3.3VA y +5V_LED con resistencias 0603 de 1,5 kΩ, 1,5 kΩ y 3,3 kΩ
respectivamente. Con Vf≈1,8 V consumen ≈1 mA cada uno; facilitan la prueba de humo sin
instrumentos. El LED analógico retorna a AGND y los otros a GND. Luz no certifica tensión ni ruido.

R_SDA/R_SCL de 2,2 kΩ se habilitan mediante SJ_PU_SDA_22/SJ_PU_SCL_22 **cerrados**. Segundo
par R_SDA_47/R_SCL_47 de 4,7 kΩ mediante SJ_PU_SDA_47/SJ_PU_SCL_47 **abiertos**. Cambiar
ambas líneas juntas: para 4,7 kΩ abrir primero los de 2,2 kΩ. Ambos pares activos dan ≈1,50 kΩ,
no 4,7 kΩ. Una R menor acelera la subida pero exige más corriente de hundimiento; 4,7 kΩ solo
sirve si el bus medido admite una subida más lenta. D14 sigue vigente: retirar R1/R2 de PCB-A;
los pull-ups opcionales de PCB-K quedan DNP. Arrancar a 100 kHz; validar a 400 kHz con el mazo.

### 10.4 Alimentación del amplificador CV

SJ_CV → **10 Ω + 47 µF** a GND crea **+5V_CV** para U_OA_CV y su desacoplo existente
100 nF/10 µF. El RC solo tiene fc nominal≈339 Hz (τ=470 µs); el condensador de 10 µF existente
reduce la fc real. D38: recibe +5V_TEENSY. Atenúa perturbaciones rápidas de ese riel, no regula ni elimina ruido lento.
Usar R 0603 y C polarizado SMD 47 µF ≥10 V; medir caída 10 Ω × corriente y transitorios en banco.
D38: clamps superiores de salida a +5V_CV; pueden realimentar ese dominio desde un CV externo.

Ganancia **1 + 10k/20k = 1,5** conservada; 4,95 V es el valor ideal, no un máximo garantizado:
la salida necesita margen respecto a +5V_CV y el resistor de 1 kΩ cae con carga. **No se añade
LDO DNP**: sin layout no se puede demostrar que cabe, y exige seleccionar dropout/estabilidad
además de un puente de selección. Si después se decide un riel de 4,5 V, el límite ideal sería
G≤4,5/3,3≈1,36, menor con margen de salida; habría que reducir Rf (Ri=20k), recalibrar firmware
y revisar el criterio de 4,9 V de doc30. No poblar un LDO manteniendo la expectativa 0–4,95 V.

### 10.5 Ajustes conservadores sin rediseño

Huellas 0603 **DNP** en paralelo con R_BIAS_L/R (100 kΩ a VREF), R_MIDI_RX (pull-up 4,7 kΩ)
y R_MIDI_BASE (10 kΩ del 6N138). Permiten reducir esos valores al medir asentamiento/pulsos
en doc30, sin puentear protecciones. No cambiar los resistores MIDI OUT de CA-033 ni añadir
atajos a clamps/aislamiento. Las huellas vacías no alteran el circuito nominal; cualquier valor
poblado se decide con medidas y se registra antes de fabricar.

### 10.6 Aceptación y límites

Regenerar PCB-M, comparar cada pin del XML y obtener ERC con cero errores; revisar PDF.
Tests verifican accesos, separación de ramas, opciones de pull-up y RC; el verificador de mazos
compara XML independientes con alias explícitos por placa. NC intencionales de D8 se documentan
por pin, sin convertir todo NC en comodín. Detectar y reportar cruces no autoriza editar PCB-A. D23 sí autoriza el cambio de PCB-K.
La prueba física sobre placa sigue doc30 A/B: VREF, audio, rango CV/carga/rizado. MIDI reservado DNP.
Faltan layout/DRC, consumo real, subida I2C y demostración de banco antes del pedido.

Referencias: doc30; contrato `boards/CONNECTORS.md`; símbolos/huellas estándar de KiCad 10;
Microchip MCP6001/2/4 (DS20001733, swing de salida bajo carga), NXP UM10204 (tiempos I2C,
Rp y capacitancia), MIDI CA-033 (2014, resistencias de OUT). No se modifica la arquitectura de audio.

### 10.7 Frente analógico y MIDI DNP

R_ISO_IN_L/R, 0 Ω 0805, separan la salida AC de C_LIN_L/R de cada SHIELD_LINE_IN_L/R.
Si falla la entrada: apagar, retirar ambos 0 Ω y abrir SJ_FRONT; desconectar el mazo de entrada
si hay backfeed. El shield mantiene cue/salida y la consola funciona sin entrada de línea.
Para probar JP_BYP, retirar primero el operacional defectuoso: cerrar bypass contra una salida
activa puede provocar contención. JP_BYP no sustituye el aislamiento del códec.
Si falla CV: apagar, desconectar jacks CV y abrir SJ_CV; si el fallo está en un DAC local,
retirarlo o desconectar sus señales/alimentación para que no retenga I2C. Consola sin CV hasta reparar.
Abrir alimentación no elimina backfeed por entradas/clamps. Ver doc30 como procedimiento sobre placa;
**el protoboard no se hace**. Hoja 07 conserva huellas pero todos sus símbolos están DNP y fuera
del BOM de ensamble. No montar mazos ni perforar MIDI; usar conversor MIDI USB en Pi.

### 10.8 I2C por segmentos y cable M–K

[NXP PCA9517A rev.4.1](https://www.nxp.com/docs/en/data-sheet/PCA9517A.pdf), §§5–7:
VCCA=VCCB=3,3 V; A local común (Teensy, códec, ADS, DAC), B separado por rama K/A.
VOL B ≈0,5 V; **no conectar dos lados B**. Cambiar EN solo con bus ocioso.
Pinout MSOP-8: 1 VCCA, 2 SCLA, 3 SDAA, 4 GND, 5 EN, 6 SDAB, 7 SCLB, 8 VCCB.
Se usa el símbolo estándar KiCad TCA9517ADGK por igualdad de pines; valor, huella y datasheet
identifican la pieza **NXP PCA9517ADP,118, C80799**, MSOP-8 3×3 mm paso 0,65.
D30 sustituye EN común: EN_K/EN_A con pull-up 10 kΩ y SJ_EN_K_GPIO/SJ_EN_A_GPIO cerrados a Teensy 0/1. Para forzar alto, apagar y abrir el puente GPIO correspondiente; la pull-up mantiene alto sin cortocircuitar un GPIO bajo.
No se cambia firmware aquí: al reset pin0/1 en alta impedancia y pull-ups habilitan ramas. La tarea firmware debe gestionar EN_K/EN_A según D30; no iniciar Serial1 ni cambiar EN con bus retenido en bajo.

Un par de 2,2 kΩ por segmento (local, B_K, B_A); puentes cerrados. Alternativa 4,7 kΩ con
puentes abiertos: abrir 2,2 kΩ antes de cerrar 4,7 kΩ. No sumar los tres pares como si fueran
un único bus. R1/R2 de PCB-A se retiran; pull-ups K DNP. SDA/SCL de rama conservan dominios
separados en el checker: un alias nunca puede convertir lado A local en cable B.

Cada salida: B → puente cerrado → 47 Ω 0805 → cable, TVS y TP del lado del conector.
Resistencias sustituibles por 0 Ω si la medida lo permite; medir VOL/ACK y subida 30–70 %
a 100 kHz antes de 400 kHz. La caída serie se suma al VOL del esclavo visto por el buffer.
J1_PCBA no tiene SDA/SCL; colocar esta protección junto a J2_I2C para el cable de PCB-A.
El IDC conserva sus señales congeladas y no lleva una tercera rama I2C ficticia.

ESD: [ST USBLC6-2SC6 DS4260](https://www.st.com/resource/en/datasheet/usblc6-2.pdf),
2 líneas, 3,5 pF máx. I/O–GND a 1,65 V; curva típica ≈3 pF a 0 V. Pines 1/6 SDA,
3/4 SCL, 2 GND, 5 a 3V3 con 100 nF. Retorno corto y ancho al plano GND junto a conector.
No es una limitación DC precisa a 3,3 V ni garantiza inmunidad de la consola sin validar layout.
[LCSC C7519](https://www.lcsc.com/product-detail/esd-and-surge-protection-tvs-esd_stmicroelectronics-usblc6-2sc6_C7519.html)
publica stock 39.775 (consulta 2026-11-12; disponibilidad variable). PESD5V0S2BT descartado:
[hoja Nexperia](https://assets.nexperia.com/documents/data-sheet/PESD5V0S2BT.pdf) indica 45 pF máx., supera 10 pF.

Cable M–K 1:1 de 10 vías según D23 y doc28 §5; tierras 3/5/7 intercaladas.
[JST B10B-PH-K-S(LF)(SN), LCSC C158038](https://www.lcsc.com/product-detail/C158038.html):
PH 2 mm, 1×10, vertical, 2 A/vía; el pedido incluye dos headers y mazo PHR-10 en ambas puntas.
La huella mide 21,9 mm de ancho según catálogo JST: revisar espacio en layout.
