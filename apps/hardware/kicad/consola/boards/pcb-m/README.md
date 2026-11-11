# PCB-M — Placa Principal GrooveForge Consola

> Esquemático jerárquico en KiCad 10. Sin layout PCB.
> Generado: 2026-11-11. Actualizado: 2026-11-11 (layout update — sin PCB-N, conectores panel por cable, PCB-K).
> Docs de referencia: `apps/docs/consola/24-placa-principal.md`, `17-mapa-pines-teensy.md`, `25-panel-consola.md`, `kicad/consola/boards/CONNECTORS.md`.
>
> **Cambio 2026-11-11:** No habrá PCB-N. Jog (EC11), pots IN/CUE/MASTER, selector de pad y jack HP
> son componentes de panel, cableados directamente a PCB-M via conectores wire-to-board en sheet 06.
> Las 6 teclas CUE/FILL/TAP/BUILD/DROP/BREAK viven en PCB-K (placa nueva bajo el panel); MCP23017 0x23
> permanece en PCB-M y expone sus señales GPA0–GPA5 en J_PCB_K (sheet 08).

---

## Tabla de bloques (10 sub-sheets)

| # | Archivo | Bloque | Componentes clave |
|---|---|---|---|
| 01 | `sheets/01-teensy.kicad_sch` | Teensy 4.1 + Audio Shield | Teensy 4.1 (módulo), SGTL5000 Audio Shield (módulo) |
| 02 | `sheets/02-analog-front.kicad_sch` | Analog front end | NRJ3HF-1 jacks LINE IN L/R, DP3T pad switch, 1µF caps, BAT54S, MCP6004 follower, VREF divider |
| 03 | `sheets/03-adc.kicad_sch` | ADC 16-bit ×2 | ADS1115IDGSR ×2 (0x48 FADERs, 0x49 EXP/Sharp) |
| 04 | `sheets/04-mcp-io.kicad_sch` | GPIO expander I/O | MCP23017T-E/SS (0x23): teclas CUE/FILL/TAP/BUILD/DROP/BREAK, jog click, pedal |
| 05 | `sheets/05-cv-dac.kicad_sch` | CV DAC ×2 + buffer | MCP4725A0T-E/CH ×2 (0x60/0x61), MCP6004 ×1 buffer 1.5× |
| 06 | `sheets/06-surface.kicad_sch` | Superficie: conectores panel + FSR | Wire-to-board: J_JOG 4-pin, J_POT_MASTER/CUE/IN 3-pin×3, J_PAD_SEL 3-pin, J_HP 3-pin; JST-PH FSR ×4, R pulldown 10kΩ ×4 |
| 07 | `sheets/07-midi.kicad_sch` | MIDI TRS In/Out | 6N138 optoacoplador, jacks 3.5mm TRS (Type A) |
| 08 | `sheets/08-connectors-panel.kicad_sch` | Conectores panel → PCB-A / PCB-K | IDC 2×15 J1 (cinta 30-pin), Qwiic I2C, M4 Serial7, PCB-K 8-pin (teclas live), Sharp ×2, Expr FSR ×2 |
| 09 | `sheets/09-connectors-rear.kicad_sch` | Conectores traseros y frente | LINE IN/OUT 6.35mm, CV 3.5mm ×2, HP/CUE, Pedal, E-drum (DNP) |
| 10 | `sheets/10-power.kicad_sch` | Power D6 + LED buffer + I2C | TLV75533PDBV LDO 3.3V, polyfuse 0.75A, 74AHCT1G125 LED shifter, 2.2kΩ pull-ups I2C |

---

## Cross-reference: Teensy pin ↔ Net PCB-M ↔ Fuente

Referencia: `apps/docs/consola/17-mapa-pines-teensy.md` (actualizado 2026-11-11).

| Teensy Pin | Función | Net en PCB-M | Sheet | Ruta |
|---|---|---|---|---|
| T0 / T1 | MIDI RX/TX (Serial1) | `MIDI_RX` / `MIDI_TX` | 07-midi | Teensy → J1 pin 7/8 (CONNECTORS.md) → 07 |
| T2 / T3 | ENC1 A/B | `ENC1_A` / `ENC1_B` | 08-connectors | J1 pin 15/17 → PCB-A |
| T4 / T5 | ENC2 A/B | `ENC2_A` / `ENC2_B` | 08-connectors | J1 pin 19/21 |
| T6 / T9 | ENC3 A/B | `ENC3_A` / `ENC3_B` | 08-connectors | J1 pin 23/25 |
| T7 / T8 | I2S TX/RX | (audio shield interno) | 01-teensy | Fijo hardware |
| T10 / T11 | ENC4 A/B | `ENC4_A` / `ENC4_B` | 08-connectors | J1 pin 27/29 |
| T12 / T30 | ENC5 A/B | `ENC5_A` / `ENC5_B` | 08-connectors | J1 pin 2/4 |
| T13 | LED_DATA (SK6812) | `LED_DATA` | 10-power | 74AHCT1G125 → `LED_DIN_5V` → J1 pin 26 |
| T14–T17 | FSR velocity 1–4 | `FSR1_ADC`–`FSR4_ADC` | 06-surface | JST-PH ÷ R10k → pin |
| T18 / T19 | I2C SDA/SCL | `I2C_SDA` / `I2C_SCL` | múltiples | Bus compartido: ADS1115, MCP23017, MCP4725 |
| T20 / T21 | I2S LRCLK/BCLK | (audio shield interno) | 01-teensy | Fijo |
| T22 | Pot MASTER | `POT_MASTER` | 06-surface | J_POT_MASTER pin 2 (wiper) — pot panel-mount vía cable |
| T23 | I2S MCLK | (audio shield interno) | 01-teensy | Fijo |
| T24 | Pot CUE | `POT_CUE` | 06-surface | J_POT_CUE pin 2 (wiper) — pot panel-mount vía cable |
| T26 | Pot IN | `POT_IN` | 06-surface | J_POT_IN pin 2 (wiper) — pot panel-mount vía cable |
| T28 / T29 | M4 RX/TX (Serial7) | `M4_RX7` / `M4_TX7` | 08-connectors | J3_M4 JST-PH |
| T31 / T32 | ENC6 A/B | `ENC6_A` / `ENC6_B` | 08-connectors | J1 pin 6/8 |
| T33 / T34 | ENC7 A/B | `ENC7_A` / `ENC7_B` | 08-connectors | J1 pin 10/12 |
| T35 / T36 | ENC8 A/B | `ENC8_A` / `ENC8_B` | 08-connectors | J1 pin 13/14 |
| T37 / T40 | Jog A/B | `JOG_A` / `JOG_B` | 06-surface | J_JOG pin 2/3 — EC11 panel-mount vía cable (4-pin: GND/A/B/CLICK) |
| T41 | PAD_SEL | `PAD_SEL_ADC` | 06-surface | J_PAD_SEL pin 2 (wiper ladder) — selector panel-mount vía cable |

**Pines de audio (I2S) no ruteados en PCB-M:** T7, T8, T20, T21, T23 son internos al Audio Shield.
**Pines e-drum reservados (DNP):** T25, T27, T38, T39 → J_EDRUM en 09-connectors-rear (NC).

---

## Conectores

| Ref | Sheet | Tipo | Pines | Función |
|---|---|---|---|---|
| J1_PCBA | 08-connectors-panel | IDC 2×15 | 30 | Cinta a PCB-A: encoders, LEDs, I2C, 5V_LED, GND |
| J2_I2C | 08-connectors-panel | JST-SH 4-pin (Qwiic) | 4 | I2C externo: GND / +3.3V / SDA / SCL |
| J3_M4 | 08-connectors-panel | JST-PH 4-pin | 4 | NeoTrellis M4 Serial7: GND / NC / TX7 / RX7 |
| J_PCB_K | 08-connectors-panel | Conn_01x08 (wire-to-board) | 8 | PCB-K teclas live: GND / +3.3V / CUE_BTN / FILL_BTN / TAP_BTN / BUILD_BTN / DROP_BTN / BREAK_BTN |
| J5_SHARP1 / J6_SHARP2 | 08-connectors-panel | JST-PH 3-pin | 3×2 | GP2Y0A41: +5V_LED / ADC / GND |
| J7_EXP1 / J8_EXP2 | 08-connectors-panel | JST-PH 2-pin | 2×2 | FSR expresión: +3.3V / ADC |
| J_JOG | 06-surface | Conn_01x04 (wire-to-board) | 4 | EC11 panel-mount: GND / JOG_A / JOG_B / JOG_CLICK |
| J_POT_MASTER | 06-surface | Conn_01x03 (wire-to-board) | 3 | Pot MASTER panel-mount: +3.3V / POT_MASTER / GND |
| J_POT_CUE | 06-surface | Conn_01x03 (wire-to-board) | 3 | Pot CUE panel-mount: +3.3V / POT_CUE / GND |
| J_POT_IN | 06-surface | Conn_01x03 (wire-to-board) | 3 | Pot IN panel-mount: +3.3V / POT_IN / GND |
| J_PAD_SEL | 06-surface | Conn_01x03 (wire-to-board) | 3 | Selector pad panel-mount: +3.3V / PAD_SEL_ADC / GND |
| J_HP | 06-surface | Conn_01x03 (wire-to-board) | 3 | Jack HP panel-mount: HP_L / HP_R / HP_VGND |
| J_FSR1–J_FSR4 | 06-surface | JST-PH 2-pin | 2×4 | FSR velocity: +3.3V / FSRx_ADC |
| J_LINE_IN_L/R | 09-connectors-rear | NRJ3HF-1 (6.35mm TRS) | 3×2 | LINE IN L/R: T=señal, R=balance, S=AGND |
| J_LINE_OUT_L/R | 09-connectors-rear | 6.35mm TS | 3×2 | LINE OUT L/R de SGTL5000 |
| J_HP_CUE | 09-connectors-rear | 6.35mm TRS | 3 | HP: T=HP_L, R=HP_R, S=AGND |
| J_CV1 / J_CV2 | 09-connectors-rear | 3.5mm TS | 2×2 | CV OUT 0–5V: T=CVx_OUT, S=AGND |
| J_PEDAL | 09-connectors-rear | 6.35mm TRS | 3 | Pedal: T=PEDAL_BTN, S=GND (R=NC) |
| J_EDRUM | 09-connectors-rear | 5-pin (DNP) | 5 | E-drum reservado — no poblar |
| J_MIDI_IN / J_MIDI_OUT | 07-midi | 3.5mm TRS (Type A) | 3×2 | MIDI IN via 6N138, MIDI OUT resistivo |
| J_5V_BKUP | 10-power | Conn_01x02 (DNP) | 2 | Entrada 5V backup, no poblar |

---

## BOM (componentes de PCB-M)

| Referencia | Valor/Tipo | LCSC Part | Notas |
|---|---|---|---|
| U_ADS1, U_ADS2 | ADS1115IDGSR | **C37593** | I2C ADC 16-bit 4ch, VSSOP-10 |
| U_MCP_IO | MCP23017T-E/SS | **C558584** | I2C GPIO 16-bit, SSOP-28 |
| U_DAC1, U_DAC2 | MCP4725A0T-E/CH | **C144198** | I2C DAC 12-bit, SOT-23-6 |
| U_OA_CV | MCP6004T-E/SL | C510498 | Quad opamp rail-to-rail, SOIC-14 — verificar stock |
| U_OPT_MIDI | 6N138 | **C3001991** | Optoacoplador MIDI IN |
| U_LED_BUF | 74AHCT1G125GW | C7466 | Buffer tri-state 1-gate, SC-70-5 |
| U_LDO1 | TLV75533PDBV | C15578 | LDO 3.3V 500mA, SOT-23-5 |
| F1 | MF-MSMF075 (0.75A) | C17357 | Polyfuse reseteable, 1206 |
| R_SDA, R_SCL | 2.2kΩ 1% 0402 | C25879 | Pull-ups I2C — retirar de PCB-A cuando PCB-M instalado |
| R_FSR1–R_FSR4 | 10kΩ 1% 0402 | C25744 | Divisor FSR velocity |
| C_LDO_IN | 10µF X5R 0805 | C15850 | Desacople entrada LDO |
| C_LDO_OUT | 1µF X5R 0402 | C52923 | Desacople salida LDO |
| J_FSR1–J_FSR4 | JST PH S2B-PH-K | C157929 | Conector FSR 2-pin, paso 2.0mm |
| J5_SHARP1, J6_SHARP2 | JST PH S3B-PH-K | C157930 | Conector Sharp IR 3-pin, paso 2.0mm |
| J_LINE_IN_L/R | Neutrik NRJ3HF-1 | — | Panel mount 6.35mm TRS, sourcing externo |
| J_MIDI_IN/OUT | 3.5mm TRS SJ-3523-SMT-TR | C500073 | MIDI TRS, SMD |
| J1_PCBA | IDC 2×15 DS1020-01×15SF21-B2 | — | Verificar footprint Foxconn compatible |

**Notas de BOM:**
- Número de LCSC a verificar con stock actual en JLCPCB antes de fabricar.
- Componentes sin LCSC: NRJ3HF-1 (Neutrik), IDC 2×15 → pedir aparte.
- Desacoplamiento (100nF + 10µF) por IC no incluido en esta tabla — agregar en layout.

---

## Dudas y conflictos abiertos

### D1 — Pines J1 20 y 22: INT_K / INT_A vs. JOG_B / PAD_SEL

**Problema:** En CONNECTORS.md, J1 pin 20 = INT_K (antes Teensy T41) y pin 22 = INT_A (antes T40).
Con la Teensy única, T40 = JOG_B y T41 = PAD_SEL. Los nets INT_K/INT_A ya no corresponden.
**Estado en esquemático:** pines 20 y 22 de J1_PCBA tienen flag NC en el lado PCB-M.
**Acción requerida:** confirmar si INT_K/INT_A del MCP23017 de PCB-A siguen necesarios. Si sí,
reutilizar dos pines libres de J1 para rutar INT. Si no, mantener NC.

### D2 — Resistores de feedback CV (MCP6004 buffer 1.5×)

**Problema:** Rf = 10kΩ y Rin = 20kΩ del buffer de ganancia 1.5× están anotados como texto en el
esquemático (sheet 05-cv-dac) pero no como componentes discrete con footprint.
**Acción requerida:** agregar R_CV_RF1/RF2 y R_CV_RI1/RI2 como Device:R en el esquemático con
los valores y footprints antes de exportar BOM final.

### D3 — Resistores de protección MIDI OUT

**Problema:** spec MIDI Association recomienda 2×47Ω (tip y ring) + 100Ω serie en MIDI OUT para
protección de cortocircuito. Están anotados como texto en sheet 07-midi pero no como componentes.
**Acción requerida:** agregar R_MIDI_T, R_MIDI_R, R_MIDI_S con valores y footprint.

### D4 — Componentes de sheet 02 (analog front end) no poblados

**Problema:** Sheet 02-analog-front.kicad_sch está vacío. Es el bloque más complejo:
DP3T selector de nivel (0/−10/−20dB), acondicionamiento LINE IN, buffer LINE OUT desde SGTL5000,
HP out, PAD_SEL_ADC.
**Acción requerida:** diseñar y capturar sheet 02 en sprint separado.

### D5 — Pin 27 (74HC4053 routing Synth/FX) sin asignar en PCB-M

**Problema:** `17-mapa-pines-teensy.md` reserva T27 para "trigger e-drum 2" pero el CLAUDE.md del
proyecto menciona que T27 = 74HC4053 (ruteo Synth/FX) a revisar con D5.
**Acción requerida:** confirmar función definitiva de T27 antes de poblar J_EDRUM.

### D6 — Desacoplamiento por IC no capturado en esquemático

**Problema:** 100nF + 10µF por pin VCC de ADS1115 ×2, MCP23017, MCP4725 ×2, MCP6004, TLV75533
no están en el esquemático — deben agregarse en layout o antes.
**Acción requerida:** agregar en sheets 03/04/05/10 con C_ADS1_VCC, etc.

### D7 — Pull-ups I2C: conflict con PCB-A

**Estado actual:** R_SDA y R_SCL (2.2kΩ) están en PCB-M (sheet 10).
**Riesgo:** si PCB-A tiene sus propios pull-ups R1/R2, hay dos pares en paralelo (≈1.1kΩ).
**Acción requerida:** confirmar y remover R1/R2 de PCB-A cuando PCB-M esté instalado, o
diseñar un sistema de jumpers/DNP.

---

## ERC Status

```
Errors:   2    (artefacto ERC jerárquico — ver docs/erc-report.txt)
Warnings: 123  (esperados: net labels locales en diseño multi-sheet sin hierarchical pins)
```

Ver `docs/erc-report.txt` para análisis completo.
Los warnings no impiden generar netlist ni PDF. Para ERC limpio en producción: migrar a
`global_label` o implementar hierarchical sheet pins.

---

## Outputs

| Archivo | Contenido |
|---|---|
| `docs/pcb-m-schematic.pdf` | PDF del esquemático completo (10 sheets) |
| `docs/pcb-m-netlist.xml` | Netlist KiCad XML para verificación |
| `docs/erc-report.txt` | Reporte ERC con análisis de errores |
