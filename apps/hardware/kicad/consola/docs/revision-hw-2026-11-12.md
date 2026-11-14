# Revision de Diseno Hardware — GrooveForge Consola 2026-11-12

Revisión de solo lectura sobre los archivos de diseño del commit e6a61ad (rama
`claude/monorepo-groovepush`), antes del layout y la fabricación.  
Placas cubiertas: PCB-M, PCB-K (izquierda/derecha), PCB-A (congelada), PCB-F, PCB-P.  
Generador: `apps/hardware/kicad/consola/tools/schgen/`.  
Documentos de referencia consultados: `apps/docs/consola/24-placa-principal.md`,
`17-mapa-pines-teensy.md`, `27-leds-teclas.md`, `28-pcb-k.md`,
`mediciones/banco-2026-11-12.md`, `tools/HARNESS.md`, `tools/REPORT.md`,
`tools/REPORT-dft.md`, `tools/REPORT-placas.md`, `boards/pcb-m/README.md`,
`boards/pcb-m/docs/erc-report.txt`, `boards/pcb-a/docs/erc-report.txt`,
`tools/schgen/pcb_m.py`, `tools/schgen/pcb_k.py`,
`apps/consola/surface/include/consola_s/ConsolaMap.h`,
`lib/gf_consola.pretty/SK6812MINI-EA_ReverseMount.kicad_mod`.

---

## Hallazgos ordenados por severidad

Escala: CRITICO = no funciona o daña hardware; ALTO = función perdida sin arreglo fácil;
MEDIO = arreglable con cable, puente o refabricación menor; BAJO = mejora o verificación.

### CRITICO

Ninguno.

---

### ALTO — Ninguno identificado.

---

### MEDIO

| ID | Bloque | Evidencia | Impacto | Acción propuesta |
|----|--------|-----------|---------|-----------------|
| M-01 | PCB-M · Power · F1 footprint | PARTS dict línea 299: `Fuse:Fuse_1812_4532Metric`. Función pcb_m() línea 554 sobreescribe con `footprint='Fuse:Fuse_1206_3216Metric'`. El netlist generado (`pcb-m-netlist.xml`) y el .kicad_sch confirman footprint 1206. README BOM dice "C17357, 1206, según BOM vigente". El part() de línea 376 usa el valor explícito cuando no es None. | Inconsistencia confirmada entre PARTS dict (1812) y el generador + BOM (1206). Si LCSC C17357 es un polyfuse MF-MSMF075-2 en package 1812, el footprint 1206 generado (3,2 × 1,6 mm) no admite el componente físico (4,5 × 3,2 mm). El cuerpo sobresaldría y los pads no coincidirían. Si C17357 es una variante en 1206, el PARTS dict tiene una entrada obsoleta. | Verificar en LCSC el package de C17357 antes del layout. Si es 1812: corregir línea 554 de pcb_m.py para pasar `footprint=None` y dejar que el PARTS dict provea el valor correcto. Si es 1206: actualizar el PARTS dict a `Fuse:Fuse_1206_3216Metric` para consistencia. |
| M-02 | PCB-M · I2C · PCB-A pull-ups | Decisión D14 (doc24, linea 573 de pcb_m.py): "desoldar R1/R2 de PCB-A". La placa PCB-A ya está fabricada. La nota está solo en el texto del esquemático (`sheets[9].notes`), no en ningún documento de assembly BOM formal ni en HARNESS.md. CONNECTORS.md anota "desoldar R1/R2 de PCB-A". | Riesgo de error de ensamblaje: si no se quitan R1/R2 en la PCB-A fabricada, el bus I2C del segmento A verá pull-ups duplicados (2,2 kΩ en paralelo con los de PCB-M), reduciéndolos a ~1,1 kΩ. Dependiendo de la capacitancia del cable, puede causar problemas de timing o fallos intermitentes. | Agregar el paso explícito en un documento de pre-assembly o en HARNESS.md sección "Preparación de módulos antes del primer uso". No requiere cambio de schematic. |
| M-03 | PCB-M · Power · Sin TVS en J_5V_LED_IN | Schematic note hoja 10 línea 572: "sin TVS de entrada adecuada en BOM." REPORT.md lo lista como open issue. El conector J_5V_LED_IN expone la línea USB-A directamente a F1 sin clamp de ESD ni protección contra inversión de polaridad. | Un evento ESD o inserción de conector invertido puede poner voltaje negativo o pico sobre +5V_LED_RAW. Con la PCB de consola cerca del usuario, el riesgo no es despreciable durante uso. | Seleccionar y agregar TVS unidireccional o SMAJ5.0A (5,0 V, 400 W) en paralelo con J_5V_LED_IN antes del pedido. Decisión de BOM pendiente per design. |
| M-04 | PCB-M · PCA9517A footprint | Hoja 12 línea 641: `footprint='Package_SO:VSSOP-8_3x3mm_P0.65mm'` para PCA9517ADP,118 (NXP MSOP-8). La nota del esquemático (línea 675) confirma el lib_id `TCA9517ADGK` mapea al MSOP-8 de NXP. KiCad tiene `Package_SO:MSOP-8_3x3mm_P0.65mm` y `Package_SO:VSSOP-8_3x3mm_P0.65mm` como footprints distintos. Las dimensiones nominales del body son iguales (3 × 3 mm, pitch 0,65 mm) pero las longitudes de pad y courtyard pueden diferir por 0,05-0,1 mm. LCSC C80799 confirmado correcto para PCA9517A. | Si los pads VSSOP-8 son más cortos que los del MSOP-8 real, la soldabilidad manual y la inspección óptica resultan subóptimas. Riesgo bajo para SMT de JLCPCB si la diferencia es < 0,05 mm, pero puede causar DFM warning en la orden de fabricación. | Comparar las dimensiones del pad del footprint `VSSOP-8_3x3mm_P0.65mm` de KiCad contra el package outline de la hoja 2 del datasheet PCA9517A (NXP) antes del layout. Si los pads son iguales o el JLCPCB DFM lo acepta, proceder. De lo contrario, cambiar a `MSOP-8_3x3mm_P0.65mm`. |
| M-05 | PCB-M · LED · Causa eléctrica de startup no resuelta | `banco-2026-11-12.md` §Hallazgo 2: el códec falla al arrancar cuando la línea de datos LED está conectada durante el power-up; causa eléctrica no identificada. El software lo mitiga inicializando el códec antes del DMA. La nota: "Al llegar PCB-M, repetir el arranque con la cadena de 26 LEDs conectada." El 74AHCT1G125 en PCB-M (hoja 10, línea 563) aisla pin 13 del rail +5V_LED, lo que debería resolver el problema de acoplamiento. | Si la causa es acoplamiento resistivo/capacitivo desde la línea LED al bus I2C a través de la breadboard (y no un problema de PCB-M), el 74AHCT1G125 lo resuelve. Si es un problema de impedancia del oscilador del SGTL5000 en startup, PCB-M no lo mejora. Sin confirmación, el arranque podría fallar en la PCB final. | Priorizar el test de arranque con PCB-M como primera verificación de bringup. Tener el firmware workaround (init codec antes de DMA) como fallback confirmado. |

---

### BAJO

| ID | Bloque | Evidencia | Impacto | Acción propuesta |
|----|--------|-----------|---------|-----------------|
| B-01 | PCB-K · Choc V2 hot-swap socket pads | REPORT-placas.md: "VERIFICAR CON ZOCALO REAL" para las dimensiones del footprint Kailh_Choc_V2_PG1353_Hotswap.kicad_mod (LCSC C5333465). Las dimensiones de pad del CPG135001S30 no fueron verificadas físicamente. | Si los pads no coinciden, puede resultar en solder joints débiles o imposibilidad de insertar el socket. | Fabricar un coupon de 2-3 posiciones antes de la orden completa y verificar inserción / soldabilidad. |
| B-02 | PCB-K · SK6812MINI-EA footprint provisional | Footprint custom `gf_consola:SK6812MINI-EA_ReverseMount`. Pads verificados contra datasheet OPSCO C5378731: pad 1 (GND) = (−2,725; +0,75), pad 2 (DIN) = (−2,725; −0,75), pad 3 (VDD) = (+2,725; −0,75), pad 4 (DOUT) = (+2,725; +0,75), todos 1,35 × 0,82 mm roundrect. La numeración coincide con el datasheet. No se ha verificado el clearance de copper desde el TOP del LED mirando a través de la PCB (reverse-mount). | Si el silkscreen o courtyard del footprint obstruye el agujero de luz necesario para un LED de montaje inverso, el brillo o el ángulo de visión resultan comprometidos. | Fabricar un coupon de 4-5 LEDs en PCB doble cara y verificar que la luz pasa correctamente hacia el operador. REPORT-placas.md ya lo marca. |
| B-03 | PCB-M · Analog front · BAT54S pin mapping | FRONT list línea 326 de pcb_m.py: D_CLAMP_L `{1: 'AGND', 2: '+3.3VA', 3: 'PROT_L'}` con `lib_id='Diode:BAT54S'`. Si el símbolo KiCad Diode:BAT54S es common-anode (A=pin1, K1=pin2, K2=pin3), la topología provee únicamente clamping del lado negativo para PROT_L contra AGND. El rail +3.3VA conectado a K1 no provee high-side clamping con esta orientación. | Circuito capturado del diseño Studio existente ("Frente analogico de la portadora Studio conservado", doc24). El acoplamiento AC (C_AC_L 4,7 µF) y el divisor R_SER/R_BIAS limitan la excursión del voltaje, así que la protección absoluta de alta tensión es de baja prioridad. Sin embargo, una revisión de la topología del BAT54S contra el símbolo KiCad es prudente antes del layout. | Abrir el símbolo `Diode:BAT54S` en KiCad y confirmar asignación de pines (A, K1, K2). Si es common-anode, documentar que la protección activa es solo la negativa y que el diseño la acepta. Si se necesita clamping rail-to-rail completo, sustituir por BAT54C (common cathode). |
| B-04 | PCB-M · ADS1115 footprint | PARTS dict línea 164: `'footprint': 'Package_SO:TSSOP-10_3x3mm_P0.5mm'` para ADS1115IDGSR (VSSOP-10). TI denomina el package "VSSOP-10" (body 3 × 3 mm, pitch 0,5 mm); KiCad no tiene un footprint VSSOP-10 estándar y se usa el TSSOP-10 dimensionalmente equivalente. Práctica habitual en diseño con TI. | El footprint TSSOP-10 es compatiblecon el VSSOP-10 de TI si el pad length es suficiente. JLCPCB lo acepta. Riesgo muy bajo. | Confirmar en la hoja de datos de ADS1115IDGSR (D/C 2024+) que las dimensiones de pad no diferieren > 0,05 mm del footprint KiCad. Sin acción si coincide. |
| B-05 | PCB-M · Montaje · Corte VUSB-VIN en Teensy | README.md sección de montaje menciona el corte del puente VUSB-VIN en el Teensy 4.1 como requisito para alimentación externa. Este paso no está en ningún BOM formal ni en CONNECTORS.md. El corte es irreversible; si se omite, la línea +5V_TEENSY del header de la Pi y el VBUS del USB del Teensy se cortocircuitan. | Si el corte no se realiza al montar PCB-M y se conecta el Teensy a USB y a Pi simultáneamente, se produce cortocircuito entre dos fuentes de 5 V. | Agregar el paso en un checklist de pre-primera-alimentación o en CONNECTORS.md. No requiere cambio de schematic. |
| B-06 | PCB-M · FSR entradas flotantes | `banco-2026-11-12.md` §prueba 4: FSR ADC sin resistencia a GND lee ~1500/4095 en entradas al aire, generando 220 880 eventos espurios en WATCH. pcb_m.py hoja 06 línea 477: R_FSR1-4 (10 kΩ) a GND implementados, lo que resuelve el bias. Pendiente: umbral del perfil mínimo en firmware no validado con R_FSR conectadas. | Sin validación del umbral, los golpes espurios pueden contaminar el perfil mínimo en PCB-M. | Validar umbral FSR en firmware antes del primer test musical con PCB-M. No requiere cambio de hardware. |
| B-07 | PCB-M · Múltiples footprints vacíos en PARTS dict | Líneas 182-317 de pcb_m.py: 24 referencias tienen `footprint: ''` en PARTS dict (J_FSR1-4, R_FSR1-4, J_JOG, J_POT_*, J_HP, U_OPT_MIDI, J_EDRUM, U_LED_BUF, R_SDA, R_SCL, etc.). La mayoría se resuelven en pcb_m() via argumento explícito o la fórmula PH de connector(). Los conectores de panel (J_FSR, J_JOG, J_POT) reciben el footprint auto-generado `JST_PH_B{n}B-PH-K`, que puede no ser el conector físico final. MIDI es DNP. | No bloquea el layout (los footprints se resuelven). Pero el PARTS dict como referencia de BOM da entradas vacías, lo que complica la generación del CPL para JLCPCB si se usa el PARTS dict directamente como fuente de BOM. | Limpiar el PARTS dict en un sprint de consolidación posterior al primer bringup, una vez definidos los conectores de panel definitivos. No urgente para la primera fabricación. |

---

## Verificado sin problemas

Los siguientes ítems fueron revisados contra specs, generator y harness y no presentan
hallazgos que requieran acción antes del layout:

**Separación de rails:**
- +5V_TEENSY y +5V_LED alimentados desde puertos USB-A distintos de la Pi; sin cruce de rail.
- +3.3VA generado por TLV75533PDBV (SOT-23-5) con SJ_FRONT para aislamiento del front analógico.
- RC filter CV: SJ_CV + 10 Ω + 47 µF → +5V_CV (hoja 05 pcb_m.py líneas 464-468).
- Star ground NT1 (Device:NetTie_2 / NetTie-2_SMD_Pad0.5mm): único punto de unión AGND/GND, posicionado junto a U_LDO1 (doc24 §10.4, línea 571 notas).

**HP_VGND:**
- HP_VGND no conectado a GND ni AGND en ningún punto del harness ni del schematic. J_HP pin 3 = HP_VGND; J_SHIELD_HP pin 3 = HP_VGND; sleeve del jack HP = HP_VGND. Verificado en HARNESS.md, pcb_m.py hoja 06 línea 484, hoja 01 líneas 405-406.

**I2C — unicidad de direcciones:**
- SGTL5000=0x0A, ADS1115#1=0x48 (ADDR→GND línea 429), ADS1115#2=0x49 (ADDR→VDD línea 430), MCP23017 0x20/0x21/0x22/0x23 (PCB-A y PCB-K), MCP4725 DAC1=0x60 (A0→GND línea 441), DAC2=0x61 (A0→+3.3V línea 441). Sin colisiones.

**I2C — PCA9517A:**
- Wiring correcto: A-side = bus compartido (Teensy, codec, ADS, DAC). B_K y B_A son segmentos independientes; nunca conectados entre sí (nota línea 676 pcb_m.py).
- EN: pull-up 10 kΩ a +3.3V (R_I2C_EN línea 665), Teensy pin 0 como input (DEFAULT HIGH = habilitado). SJ_EN_GPIO cerrado por defecto; SJ_EN_HIGH abierto (líneas 666-667).
- Decoupling C_I2C_K_A/B y C_I2C_A_A/B presentes para ambas ramas (líneas 643-644).
- ESD: USBLC6-2SC6 (C7519) + 47 Ω serie en cada línea de cada branch (líneas 646-648, 659-663).

**MCP23017 — PCB-K:**
- Mapeo GPIO verificado contra ConsolaMap.h: chip 0=0x20 (PCB-A), chip 1=0x21 (PCB-K derecha índices 17-25), chip 2=0x22 (PCB-K izquierda índices 0-8), chip 3=0x23 (PCB-K izquierda índices 9-16 + JOG_CLICK + PEDAL_BTN).
- Errata MCP23017 respetada: GPA7 (pin 28) y GPB7 (pin 8) = NC en pcb_k.py (líneas GPIO_PINS y mcp() function).
- RESET conectado a +3.3V (pull-up, lógica activa-baja mantenida inactiva).

**LEDs SK6812MINI-EA:**
- Footprint gf_consola:SK6812MINI-EA_ReverseMount: pad 1=(−2,725; +0,75) mm, pad 2=(−2,725; −0,75) mm, pad 3=(+2,725; −0,75) mm, pad 4=(+2,725; +0,75) mm — coincide con datasheet OPSCO C5378731 pinout 1=GND, 2=DIN, 3=VDD, 4=DOUT.
- Cadena verificada: Teensy pin 13 → 74AHCT1G125 → SJ_LED_DATA → LED_DIN_5V → J_PCB_K.6 → 33 Ω (en PCB-K) → cadena de 26 LEDs (OctoWS2811 DMA, QTIMER4).
- Budget de corriente 500 mA (polyfuse 0,75 A hold / 1,5 A trip) cubre la carga máxima.

**LED data buffer 74AHCT1G125:**
- Footprint correctamente resuelto en pcb_m() línea 563-564: `Package_TO_SOT_SMD:SOT-353_SC-70-5`. Wiring: pin1=GND, pin2=LED_DATA (3,3 V del Teensy), pin3=GND, pin4=LED_BUF_OUT (5 V), pin5=+5V_LED.

**CV output:**
- U_DAC1/U_DAC2: footprint SOT-23-6 correctamente pasado en líneas 441-442.
- U_OA_CV: footprint SOIC-14_3.9x8.7mm_P1.27mm correctamente pasado en líneas 444-446.
- Ganancia: 1 + 10 kΩ/20 kΩ = 1,5 → swing 0 – 4,95 V con DAC de 3,3 V. Consistente con doc24 §10.4.
- Pines de poder U_OA_CV: unidad 5 (pins 4 y 11) correctamente conectados a +5V_CV y GND (línea 458).
- Units 3 y 4 no usados: out conectado a IN− (unity-gain follower de entrada GND, in bom retenido).

**Conectores J1_PCBA y J2_I2C:**
- J1_PCBA: footprint `Connector_IDC:IDC-Header_2x15_P2.54mm_Vertical` (línea 521). Net list de 30 pines verificado contra mapa PCB-A.
- J2_I2C: footprint `Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal` (línea 522-523). Qwiic-compatible.
- J_PCB_K: auto-footprint por fórmula PH con n=10 → `JST_PH_B10B-PH-K_1x10_P2.00mm_Vertical`, LCSC C158038 asignado (línea 527). Net list (líneas 525-526) coincide con HARNESS.md M-K 10-vía.

**R_ISO_IN_L/R:**
- Presentes explícitamente en hoja 02, líneas 418-419: 0 Ω, 0805, con nets FRONT_LINE_IN_L/R → SHIELD_LINE_IN_L/R. Separan el front analógico del codec como marcado en D17.

**MIDI:**
- Todos los símbolos de hoja 07 marcados `dnp=True` y `in_bom=False` (líneas 510-511). No habrá hueco en el panel ni harness de MIDI. CA-033: R_MIDI_R=33 Ω (línea 503), R_MIDI_T=10 Ω (línea 501), correcto per D23.

**Pull-ups I2C:**
- Esquema seleccionable: 2,2 kΩ CERRADO por defecto (SJ_PU_SDA/SCL_22) y 4,7 kΩ ABIERTO (SJ_PU_SDA/SCL_47) en bus compartido (líneas 598-602). Por segmento B_K y B_A: misma lógica (líneas 649-651). Ningún pull-up en PCB-K (DNP per D22). Eliminar R1/R2 de PCB-A es el único paso externo necesario.

**ERC:**
- PCB-M: 0 errores, 7 warnings (5 señales I2S internas al módulo Audio Shield, 2 anillos LINE_IN sin terminar — deferred). Sin errores eléctricos reales.
- PCB-A: 0 errores, 0 warnings.
- PCB-K izquierda/derecha, PCB-F, PCB-P: 0 errores por REPORT-placas.md.

**Bench measurements:**
- I2C con cables de 40–50 cm: 0 errores, 0 timeouts en 60 s con 20 LEDs + tono 440 Hz (banco-2026-11-12.md prueba 4).
- Alimentación bajo carga completa: +5V_LED min 5,05 V, sin undervoltage (prueba 5).
- DMA LED + codec: funcionando simultáneamente, loop_max_us 278 µs vs límite 2000 µs.

---

## Listo para layout?

**NO todavia. Dos acciones bloqueantes antes del layout:**

1. **M-01 — Verificar LCSC C17357 package (1206 vs 1812).** El polyfuse F1 tiene footprint
   `Fuse:Fuse_1206_3216Metric` en el netlist generado y el README BOM dice "1206, según BOM
   vigente", pero el PARTS dict define `Fuse:Fuse_1812_4532Metric`. El MF-MSMF075-2 estándar
   es 1812. Confirmar en la página de LCSC C17357 cuál es el package real del componente.
   Si es 1812: corregir la línea 554 de pcb_m.py para que `footprint=None` (y el PARTS dict
   provee el 1812 correcto). Si es 1206: actualizar el PARTS dict para consistencia y regenerar.
   Este es el único bloqueante real de layout.

2. **M-03 — Seleccionar TVS para J_5V_LED_IN.** El rail de entrada USB carece de protección
   ESD/inversión. Seleccionar el componente, agregar al PARTS dict y al generator antes de
   enviar a JLCPCB para evitar re-spin solo por este componente.

**Acciones recomendadas antes de la orden de fabricación (no bloqueantes para layout):**

- M-02: Documentar el paso de desoldado R1/R2 de PCB-A en CONNECTORS.md o en un checklist
  de pre-assembly.
- M-04: Comparar pad dimensions del VSSOP-8 KiCad footprint contra el datasheet PCA9517A
  para confirmar compatibilidad.
- M-05: Planificar test de bringup con PCB-M: primer encendido con 26 LEDs conectados para
  confirmar que el 74AHCT1G125 resuelve el problema de startup del códec.
- B-01/B-02: Fabricar coupons para Choc V2 hot-swap y SK6812MINI-EA antes de la orden
  completa de PCB-K.
- B-05: Agregar el corte VUSB-VIN del Teensy al checklist de montaje.

**El resto del diseño (separación de rails, HP_VGND, I2C segmentado, direcciones, mapeo de
pines vs firmware, harness de 29 cables, gadget de ERC) se verificó sin hallazgos que
bloqueen el layout.**
