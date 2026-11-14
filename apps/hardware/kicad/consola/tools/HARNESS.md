# Mazos Console — informe generado

Regenerar XML de A y comprobar: `python3 apps/hardware/kicad/consola/tools/check_harness.py --export-pcb-a`.

**Resultado: 0 errores; 32 cables declarados.**

A/B de placas: XML independiente. B de panel: contrato esperado; NO continuidad medida.
Placas: números de pin; M4: rótulos de pad SDA/SCL/GND/3V (sin numeración de conector). Comprobar pin1 antes de crimpar.
Colores sugeridos; marcar ambos extremos. IDC: raya identifica pin1; mantener 1:1.
Abrir puentes invalida continuidad de los alias de ramas; tablas asumen montaje por defecto.

## Fuentes

- pcb-a: `apps/hardware/kicad/consola/tools/pcb-a-harness-netlist.xml`
- pcb-f: `apps/hardware/kicad/consola/boards/pcb-f/docs/pcb-f-netlist.xml`
- pcb-k-left: `apps/hardware/kicad/consola/boards/pcb-k-left/docs/pcb-k-left-netlist.xml`
- pcb-k-right: `apps/hardware/kicad/consola/boards/pcb-k-right/docs/pcb-k-right-netlist.xml`
- pcb-m: `apps/hardware/kicad/consola/boards/pcb-m/docs/pcb-m-netlist.xml`
- pcb-p: `apps/hardware/kicad/consola/boards/pcb-p/docs/pcb-p-netlist.xml`

## Hallazgos

- Sin errores de correspondencia en los netlists leídos.
- D14: retirar físicamente R1/R2 de PCB-A; el esquema fabricado sigue mostrándolos.
- Jacks/selector/Sharp: confirmar terminales de la pieza comprada con continuidad; no hay SKU mecánico definitivo.
- Anillos LINE IN: terminación pendiente; coincidencia del cable no valida una entrada balanceada.

## M-K — PASS

`pcb-m.J_PCB_K1` ↔ `pcb-k-left.J1` · 10 vías · JST-PH.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3.3V | +3V3 | +3.3V / +3.3V | naranja |  | PASS |
| 2 → 2 | I2C_SDA_K | SDA | I2C_SDA_B_K / I2C_SDA_B_K | azul |  | PASS |
| 3 → 3 | GND | GND | GND / GND | negro |  | PASS |
| 4 → 4 | I2C_SCL_K | SCL | I2C_SCL_B_K / I2C_SCL_B_K | amarillo |  | PASS |
| 5 → 5 | GND | GND | GND / GND | negro |  | PASS |
| 6 → 6 | LED_DIN_5V | /cables-power-1/LED_DIN_5V | LED_DIN_5V / LED_DIN_5V | verde |  | PASS |
| 7 → 7 | GND | GND | GND / GND | negro |  | PASS |
| 8 → 8 | +5V_LED_K | +5V_LED | +5V_LED / +5V_LED | rojo |  | PASS |
| 9 → 9 | JOG_CLICK | JOG_CLICK | JOG_CLICK / JOG_CLICK | blanco (marcar señal) |  | PASS |
| 10 → 10 | PEDAL_BTN | PEDAL_BTN | PEDAL_BTN / PEDAL_BTN | blanco (marcar señal) |  | PASS |

## K-left-right — PASS

`pcb-k-left.J2` ↔ `pcb-k-right.J1` · 6 vías · JST-PH.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3V3 | +3V3 | +3.3V / +3.3V | naranja |  | PASS |
| 2 → 2 | GND | GND | GND / GND | negro |  | PASS |
| 3 → 3 | SDA | SDA | I2C_SDA_B_K / I2C_SDA_B_K | azul |  | PASS |
| 4 → 4 | SCL | SCL | I2C_SCL_B_K / I2C_SCL_B_K | amarillo |  | PASS |
| 5 → 5 | +5V_LED | +5V_LED | +5V_LED / +5V_LED | rojo |  | PASS |
| 6 → 6 | LED_DATA | /cables-power-1/LED_DATA | LED_DATA / LED_DATA | verde |  | PASS |

## M-A-IDC — PASS

`pcb-m.J1_PCBA1` ↔ `pcb-a.J1` · 30 vías · IDC.

D8: M.18/20/22/26 NC; MCP polling, LED data only via M-K. A J4 legacy unused.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +5V_LED | /+5V | +5V_LED / +5V_LED | rojo |  | PASS |
| 2 → 2 | +5V_LED | /+5V | +5V_LED / +5V_LED | rojo |  | PASS |
| 3 → 3 | GND | /GND | GND / GND | negro |  | PASS |
| 4 → 4 | GND | /GND | GND / GND | negro |  | PASS |
| 5 → 5 | ENC1_A | /ENC1_A | ENC1_A / ENC1_A | blanco (marcar señal) |  | PASS |
| 6 → 6 | GND | /GND | GND / GND | negro |  | PASS |
| 7 → 7 | ENC1_B | /ENC1_B | ENC1_B / ENC1_B | blanco (marcar señal) |  | PASS |
| 8 → 8 | ENC5_A | /ENC5_A | ENC5_A / ENC5_A | blanco (marcar señal) |  | PASS |
| 9 → 9 | ENC2_A | /ENC2_A | ENC2_A / ENC2_A | blanco (marcar señal) |  | PASS |
| 10 → 10 | ENC2_B | /ENC2_B | ENC2_B / ENC2_B | blanco (marcar señal) |  | PASS |
| 11 → 11 | FADER4_SIG | /FADER4 | FADER4_SIG / FADER4_SIG | blanco (marcar señal) |  | PASS |
| 12 → 12 | FADER3_SIG | /FADER3 | FADER3_SIG / FADER3_SIG | blanco (marcar señal) |  | PASS |
| 13 → 13 | ENC3_A | /ENC3_A | ENC3_A / ENC3_A | blanco (marcar señal) |  | PASS |
| 14 → 14 | FADER2_SIG | /FADER2 | FADER2_SIG / FADER2_SIG | blanco (marcar señal) |  | PASS |
| 15 → 15 | ENC3_B | /ENC3_B | ENC3_B / ENC3_B | blanco (marcar señal) |  | PASS |
| 16 → 16 | FADER1_SIG | /FADER1 | FADER1_SIG / FADER1_SIG | blanco (marcar señal) |  | PASS |
| 17 → 17 | ENC4_A | /ENC4_A | ENC4_A / ENC4_A | blanco (marcar señal) |  | PASS |
| 18 → 18 | NC | NC | NC / NC | sin hilo / aislar |  | NC D8 |
| 19 → 19 | ENC4_B | /ENC4_B | ENC4_B / ENC4_B | blanco (marcar señal) |  | PASS |
| 20 → 20 | NC | /INT_K | NC / INT_K | sin hilo / aislar |  | NC D8 |
| 21 → 21 | ENC5_B | /ENC5_B | ENC5_B / ENC5_B | blanco (marcar señal) |  | PASS |
| 22 → 22 | NC | /INT_A | NC / INT_A | sin hilo / aislar |  | NC D8 |
| 23 → 23 | ENC6_A | /ENC6_A | ENC6_A / ENC6_A | blanco (marcar señal) |  | PASS |
| 24 → 24 | ENC8_B | /ENC8_B | ENC8_B / ENC8_B | blanco (marcar señal) |  | PASS |
| 25 → 25 | ENC6_B | /ENC6_B | ENC6_B / ENC6_B | blanco (marcar señal) |  | PASS |
| 26 → 26 | NC | /LED_DIN | NC / LED_DIN | sin hilo / aislar |  | NC D8 |
| 27 → 27 | ENC8_A | /ENC8_A | ENC8_A / ENC8_A | blanco (marcar señal) |  | PASS |
| 28 → 28 | ENC7_A | /ENC7_A | ENC7_A / ENC7_A | blanco (marcar señal) |  | PASS |
| 29 → 29 | ENC7_B | /ENC7_B | ENC7_B / ENC7_B | blanco (marcar señal) |  | PASS |
| 30 → 30 | +3.3V | /+3V3 | +3.3V / +3.3V | naranja |  | PASS |

## M-A-I2C — PASS

`pcb-m.J2_I2C1` ↔ `pcb-a.J2` · 4 vías · JST-SH.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | GND | /GND | GND / GND | negro |  | PASS |
| 2 → 2 | +3.3V | /+3V3 | +3.3V / +3.3V | naranja |  | PASS |
| 3 → 3 | I2C_SDA_A | /SDA | I2C_SDA_B_A / I2C_SDA_B_A | azul |  | PASS |
| 4 → 4 | I2C_SCL_A | /SCL | I2C_SCL_B_A / I2C_SCL_B_A | amarillo |  | PASS |

## A-F — PASS

`pcb-a.J3` ↔ `pcb-f.J1` · 6 vías · JST-SH.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | /+3V3 | +3V3 | +3.3V / +3.3V | naranja |  | PASS |
| 2 → 2 | /FADER1 | /faders-1/FADER1 | FADER1_SIG / FADER1_SIG | blanco (marcar señal) |  | PASS |
| 3 → 3 | /FADER2 | /faders-1/FADER2 | FADER2_SIG / FADER2_SIG | blanco (marcar señal) |  | PASS |
| 4 → 4 | /FADER3 | /faders-1/FADER3 | FADER3_SIG / FADER3_SIG | blanco (marcar señal) |  | PASS |
| 5 → 5 | /FADER4 | /faders-1/FADER4 | FADER4_SIG / FADER4_SIG | blanco (marcar señal) |  | PASS |
| 6 → 6 | /GND | GND | GND / GND | negro |  | PASS |

## M-P-1 — PASS

`pcb-m.J_FSR1` ↔ `pcb-p.J1` · 2 vías · JST-PH.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3.3V | +3V3_V1 | +3.3V / +3.3V | naranja |  | PASS |
| 2 → 2 | FSR1_ADC | /fsr-passive-1/FSR_V1 | FSR1_ADC / FSR1_ADC | blanco (marcar señal) |  | PASS |

## M-P-2 — PASS

`pcb-m.J_FSR2` ↔ `pcb-p.J2` · 2 vías · JST-PH.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3.3V | +3V3_V2 | +3.3V / +3.3V | naranja |  | PASS |
| 2 → 2 | FSR2_ADC | /fsr-passive-1/FSR_V2 | FSR2_ADC / FSR2_ADC | blanco (marcar señal) |  | PASS |

## M-P-3 — PASS

`pcb-m.J_FSR3` ↔ `pcb-p.J3` · 2 vías · JST-PH.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3.3V | +3V3_V3 | +3.3V / +3.3V | naranja |  | PASS |
| 2 → 2 | FSR3_ADC | /fsr-passive-1/FSR_V3 | FSR3_ADC / FSR3_ADC | blanco (marcar señal) |  | PASS |

## M-P-4 — PASS

`pcb-m.J_FSR4` ↔ `pcb-p.J4` · 2 vías · JST-PH.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3.3V | +3V3_V4 | +3.3V / +3.3V | naranja |  | PASS |
| 2 → 2 | FSR4_ADC | /fsr-passive-1/FSR_V4 | FSR4_ADC / FSR4_ADC | blanco (marcar señal) |  | PASS |

## M-P-5 — PASS

`pcb-m.J7_EXP1` ↔ `pcb-p.J5` · 2 vías · JST-PH.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3.3V | +3V3_X1 | +3.3V / +3.3V | naranja |  | PASS |
| 2 → 2 | EXP1_ADC | /fsr-passive-1/FSR_X1 | EXP1_ADC / EXP1_ADC | blanco (marcar señal) |  | PASS |

## M-P-6 — PASS

`pcb-m.J8_EXP2` ↔ `pcb-p.J6` · 2 vías · JST-PH.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3.3V | +3V3_X2 | +3.3V / +3.3V | naranja |  | PASS |
| 2 → 2 | EXP2_ADC | /fsr-passive-1/FSR_X2 | EXP2_ADC / EXP2_ADC | blanco (marcar señal) |  | PASS |

## panel-J_LINE_IN_L — PASS

`pcb-m.J_LINE_IN_L1` ↔ `panel.J_LINE_IN_L` · 3 vías · JST-PH.

Jack LINE IN TRS; ring sin terminación definida en PCB-M.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | LINE_IN_L_T | LINE_IN_L_T | LINE_IN_L_T / LINE_IN_L_T | blanco (marcar señal) | tip | PASS |
| 2 → 2 | /09-connectors-rear/LINE_IN_L_R | LINE_IN_L_R | LINE_IN_L_R / LINE_IN_L_R | blanco (marcar señal) | ring | PASS |
| 3 → 3 | AGND | AGND | AGND / AGND | gris (masa analógica) | sleeve | PASS |

## panel-J_LINE_OUT_L — PASS

`pcb-m.J_LINE_OUT_L1` ↔ `panel.J_LINE_OUT_L` · 3 vías · JST-PH.

Jack LINE OUT TRS; dejar ring sin conexión.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | LINE_OUT_L_T | LINE_OUT_L_T | LINE_OUT_L_T / LINE_OUT_L_T | blanco (marcar señal) | tip | PASS |
| 2 → 2 | NC | NC | NC / NC | sin hilo / aislar | ring NC | NC |
| 3 → 3 | AGND | AGND | AGND / AGND | gris (masa analógica) | sleeve | PASS |

## panel-J_LINE_IN_R — PASS

`pcb-m.J_LINE_IN_R1` ↔ `panel.J_LINE_IN_R` · 3 vías · JST-PH.

Jack LINE IN TRS; ring sin terminación definida en PCB-M.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | LINE_IN_R_T | LINE_IN_R_T | LINE_IN_R_T / LINE_IN_R_T | blanco (marcar señal) | tip | PASS |
| 2 → 2 | /09-connectors-rear/LINE_IN_R_R | LINE_IN_R_R | LINE_IN_R_R / LINE_IN_R_R | blanco (marcar señal) | ring | PASS |
| 3 → 3 | AGND | AGND | AGND / AGND | gris (masa analógica) | sleeve | PASS |

## panel-J_LINE_OUT_R — PASS

`pcb-m.J_LINE_OUT_R1` ↔ `panel.J_LINE_OUT_R` · 3 vías · JST-PH.

Jack LINE OUT TRS; dejar ring sin conexión.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | LINE_OUT_R_T | LINE_OUT_R_T | LINE_OUT_R_T / LINE_OUT_R_T | blanco (marcar señal) | tip | PASS |
| 2 → 2 | NC | NC | NC / NC | sin hilo / aislar | ring NC | NC |
| 3 → 3 | AGND | AGND | AGND / AGND | gris (masa analógica) | sleeve | PASS |

## panel-J_CV1 — PASS

`pcb-m.J_CV1` ↔ `panel.J_CV1` · 2 vías · JST-PH.

Jack CV TS.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | CV1_PROTECTED | CV1_PROTECTED | CV1_PROTECTED / CV1_PROTECTED | blanco (marcar señal) | tip | PASS |
| 2 → 2 | AGND | AGND | AGND / AGND | gris (masa analógica) | sleeve | PASS |

## panel-J_CV2 — PASS

`pcb-m.J_CV2` ↔ `panel.J_CV2` · 2 vías · JST-PH.

Jack CV TS.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | CV2_PROTECTED | CV2_PROTECTED | CV2_PROTECTED / CV2_PROTECTED | blanco (marcar señal) | tip | PASS |
| 2 → 2 | AGND | AGND | AGND / AGND | gris (masa analógica) | sleeve | PASS |

## panel-J_PEDAL — PASS

`pcb-m.J_PEDAL1` ↔ `panel.J_PEDAL` · 3 vías · JST-PH.

Pedal: cierre de contacto entre tip y sleeve; no asignar expresión al ring.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | /09-connectors-rear/PEDAL_RAW | PEDAL_RAW | PEDAL_RAW / PEDAL_RAW | blanco (marcar señal) | tip | PASS |
| 2 → 2 | NC | NC | NC / NC | sin hilo / aislar | ring NC | NC |
| 3 → 3 | GND | GND | GND / GND | negro | sleeve | PASS |

## panel-J_HP — PASS

`pcb-m.J_HP1` ↔ `panel.J_HP` · 3 vías · JST-PH.

Audífonos TRS: sleeve exclusivamente HP_VGND, aislado del chasis/masas.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | HP_L | HP_L | HP_L / HP_L | blanco (marcar señal) | tip L | PASS |
| 2 → 2 | HP_R | HP_R | HP_R / HP_R | blanco (marcar señal) | ring R | PASS |
| 3 → 3 | HP_VGND | HP_VGND | HP_VGND / HP_VGND | violeta (retorno HP) | sleeve virtual | PASS |

## panel-J_JOG — PASS

`pcb-m.J_JOG1` ↔ `panel.J_JOG` · 4 vías · JST-PH.

Jog: unir comunes localmente; verificar fases con pieza comprada.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | GND | GND | GND / GND | negro | común encoder + común pulsador | PASS |
| 2 → 2 | JOG_A | JOG_A | JOG_A / JOG_A | blanco (marcar señal) | fase A | PASS |
| 3 → 3 | JOG_B | JOG_B | JOG_B / JOG_B | blanco (marcar señal) | fase B | PASS |
| 4 → 4 | JOG_CLICK | JOG_CLICK | JOG_CLICK / JOG_CLICK | blanco (marcar señal) | pulsador | PASS |

## panel-J_POT_MASTER — PASS

`pcb-m.J_POT_MASTER1` ↔ `panel.J_POT_MASTER` · 3 vías · JST-PH.

Potenciómetro lineal 10k.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3.3V | +3.3V | +3.3V / +3.3V | naranja | extremo horario | PASS |
| 2 → 2 | /06-surface/POT_MASTER_WIPER | POT_MASTER_WIPER | POT_MASTER_WIPER / POT_MASTER_WIPER | blanco (marcar señal) | cursor | PASS |
| 3 → 3 | GND | GND | GND / GND | negro | extremo antihorario | PASS |

## panel-J_POT_CUE — PASS

`pcb-m.J_POT_CUE1` ↔ `panel.J_POT_CUE` · 3 vías · JST-PH.

Potenciómetro lineal 10k.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3.3V | +3.3V | +3.3V / +3.3V | naranja | extremo horario | PASS |
| 2 → 2 | /06-surface/POT_CUE_WIPER | POT_CUE_WIPER | POT_CUE_WIPER / POT_CUE_WIPER | blanco (marcar señal) | cursor | PASS |
| 3 → 3 | GND | GND | GND / GND | negro | extremo antihorario | PASS |

## panel-J_POT_IN — PASS

`pcb-m.J_POT_IN1` ↔ `panel.J_POT_IN` · 3 vías · JST-PH.

Potenciómetro lineal 10k.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3.3V | +3.3V | +3.3V / +3.3V | naranja | extremo horario | PASS |
| 2 → 2 | /06-surface/POT_IN_WIPER | POT_IN_WIPER | POT_IN_WIPER / POT_IN_WIPER | blanco (marcar señal) | cursor | PASS |
| 3 → 3 | GND | GND | GND / GND | negro | extremo antihorario | PASS |

## panel-J_PAD_SEL — PASS

`pcb-m.J_PAD_SEL1` ↔ `panel.J_PAD_SEL` · 3 vías · JST-PH.

Selector 3 posiciones: contacto central polo C SIN cable (D13).

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +3.3V | +3.3V | +3.3V / +3.3V | naranja | extremo alto polo C | PASS |
| 2 → 2 | PAD_SEL_ADC | PAD_SEL_ADC | PAD_SEL_ADC / PAD_SEL_ADC | blanco (marcar señal) | común polo C | PASS |
| 3 → 3 | GND | GND | GND / GND | negro | extremo bajo polo C | PASS |

## panel-SW_PAD — PASS

`pcb-m.SW_PAD1` ↔ `panel.SW_PAD` · 8 vías · header-2x4.

Mismo selector: polos A/B audio, header 2x4 2.54mm (no IDC con llave).

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | LINE_IN_L_T | LINE_IN_L_T | LINE_IN_L_T / LINE_IN_L_T | blanco (marcar señal) | A 0dB | PASS |
| 2 → 2 | /02-analog-front/TAP10_L | TAP10_L | TAP10_L / TAP10_L | blanco (marcar señal) | A -10dB | PASS |
| 3 → 3 | /02-analog-front/PAD_L | PAD_L | PAD_L / PAD_L | blanco (marcar señal) | A común | PASS |
| 4 → 4 | /02-analog-front/TAP20_L | TAP20_L | TAP20_L / TAP20_L | blanco (marcar señal) | A -20dB | PASS |
| 5 → 5 | LINE_IN_R_T | LINE_IN_R_T | LINE_IN_R_T / LINE_IN_R_T | blanco (marcar señal) | B 0dB | PASS |
| 6 → 6 | /02-analog-front/TAP10_R | TAP10_R | TAP10_R / TAP10_R | blanco (marcar señal) | B -10dB | PASS |
| 7 → 7 | /02-analog-front/PAD_R | PAD_R | PAD_R / PAD_R | blanco (marcar señal) | B común | PASS |
| 8 → 8 | /02-analog-front/TAP20_R | TAP20_R | TAP20_R / TAP20_R | blanco (marcar señal) | B -20dB | PASS |

## panel-J5_SHARP1 — PASS

`pcb-m.J5_SHARP1` ↔ `panel.J5_SHARP1` · 3 vías · JST-PH.

Sharp GP2Y0A41: mapear por FUNCIÓN, no asumir orden físico del sensor; VO ≤3.3V.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +5V_LED_SHARP | +5V_LED | +5V_LED / +5V_LED | rojo | VCC | PASS |
| 2 → 2 | /08-connectors-panel/SHARP1_RAW | SHARP1_RAW | SHARP1_RAW / SHARP1_RAW | blanco (marcar señal) | VO | PASS |
| 3 → 3 | GND | GND | GND / GND | negro | GND | PASS |

## panel-J6_SHARP2 — PASS

`pcb-m.J6_SHARP2` ↔ `panel.J6_SHARP2` · 3 vías · JST-PH.

Sharp GP2Y0A41: mapear por FUNCIÓN, no asumir orden físico del sensor; VO ≤3.3V.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +5V_LED_SHARP | +5V_LED | +5V_LED / +5V_LED | rojo | VCC | PASS |
| 2 → 2 | /08-connectors-panel/SHARP2_RAW | SHARP2_RAW | SHARP2_RAW / SHARP2_RAW | blanco (marcar señal) | VO | PASS |
| 3 → 3 | GND | GND | GND / GND | negro | GND | PASS |

## panel-J3_M4 — PASS

`pcb-m.J3_M4` ↔ `panel.J3_M4` · 4 vías · JST-PH.

D40 (Juan 2026-11-13): soldar a pads rotulados por función, sin depender de números del conector. M4 por USB propio; Juan confirmará los rótulos impresos antes de soldar. R_M4_RX1/R_M4_TX1 de 1 kΩ junto a J3_M4; pad 3V y J3_M4.2 sin hilo.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → GND | GND | GND | GND / GND | negro | pad GND -> GND | PASS |
| 2 → 3V | NC | NC | NC / NC | sin hilo / aislar | pad 3V sin conectar | NC |
| 3 → SDA | /08-connectors-panel/M4_RX7_CABLE | M4_RX7_CABLE | M4_RX7_CABLE / M4_RX7_CABLE | blanco (marcar señal) | pad SDA = TX21 M4 -> 1 kΩ -> RX Teensy 28 | PASS |
| 4 → SCL | /08-connectors-panel/M4_TX7_CABLE | M4_TX7_CABLE | M4_TX7_CABLE / M4_TX7_CABLE | blanco (marcar señal) | pad SCL = RX22 M4 <- 1 kΩ <- TX Teensy 29 | PASS |

## panel-J_5V_LED_IN — PASS

`pcb-m.J_5V_LED_IN1` ↔ `panel.J_5V_LED_IN` · 2 vías · JST-PH.

USB-A Pi solo alimentación. D+/D- sin cable; no unir VBUS con +5V_TEENSY.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | +5V_LED_RAW | +5V_LED_RAW | +5V_LED_RAW / +5V_LED_RAW | rojo | USB-A VBUS | PASS |
| 2 → 2 | GND | GND | GND / GND | negro | USB-A GND | PASS |

## panel-J_SHIELD_IN — PASS

`pcb-m.J_SHIELD_IN1` ↔ `panel.J_SHIELD_IN` · 3 vías · JST-PH.

Rev D reverse: lower Lout/G/Rout/Rin/Lin; upper G over Rin/Lin. 2.54 mm pitch.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | SHIELD_LINE_IN_L | SHIELD_LINE_IN_L | SHIELD_LINE_IN_L / SHIELD_LINE_IN_L | blanco (marcar señal) | lower LINE IN L (rightmost) | PASS |
| 2 → 2 | SHIELD_LINE_IN_R | SHIELD_LINE_IN_R | SHIELD_LINE_IN_R / SHIELD_LINE_IN_R | blanco (marcar señal) | lower LINE IN R | PASS |
| 3 → 3 | GND | GND | GND / GND | negro | upper G above LINE IN L | PASS |

## panel-J_SHIELD_OUT — PASS

`pcb-m.J_SHIELD_OUT1` ↔ `panel.J_SHIELD_OUT` · 3 vías · JST-PH.

Rev D reverse. Two unlabeled upper pads over Lout/Rout MUST stay NC.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | SHIELD_LINE_OUT_L | SHIELD_LINE_OUT_L | SHIELD_LINE_OUT_L / SHIELD_LINE_OUT_L | blanco (marcar señal) | lower LINE OUT L (leftmost) | PASS |
| 2 → 2 | SHIELD_LINE_OUT_R | SHIELD_LINE_OUT_R | SHIELD_LINE_OUT_R / SHIELD_LINE_OUT_R | blanco (marcar señal) | lower LINE OUT R | PASS |
| 3 → 3 | GND | GND | GND / GND | negro | lower LINE OUT G | PASS |

## panel-J_SHIELD_HP — PASS

`pcb-m.J_SHIELD_HP1` ↔ `panel.J_SHIELD_HP` · 3 vías · JST-PH.

Rev D: CAUTION do not short VGND to GND; isolated headphone return.

| Pin A → pin / pad B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |
|---|---|---|---|---|---|---|
| 1 → 1 | HP_L | HP_L | HP_L / HP_L | blanco (marcar señal) | bottom right L | PASS |
| 2 → 2 | HP_R | HP_R | HP_R / HP_R | blanco (marcar señal) | bottom right R | PASS |
| 3 → 3 | HP_VGND | HP_VGND | HP_VGND / HP_VGND | violeta (retorno HP) | bottom right VGND | PASS |

## Conectores excluidos explícitamente

- `pcb-a.J4`: Ruta histórica K1/K2; NO conectar en integración PCB-M (D8).
- `pcb-m.J_5V_BKUP1`: Backup DNP; no conectar segunda fuente simultánea.
- `pcb-m.J_EDRUM1`: Reservado DNP, sin mazo autorizado.
- `pcb-m.J_MIDI_IN1`: D20: MIDI USB en Pi; DNP, sin mazo ni agujero en cuerpo.
- `pcb-m.J_MIDI_OUT1`: D20: MIDI USB en Pi; DNP, sin mazo ni agujero en cuerpo.
- `pcb-m.J_T41L1`: Socket del módulo Teensy, no mazo.
- `pcb-m.J_T41R1`: Socket del módulo Teensy, no mazo.

## Límites

No valida longitud/sección/corriente de cable, crimpado, ruido, hot-plug, DRC ni mediciones de banco.
Excepciones D8 por pin y alias por placa en `harness.py`; no modificar alias para ocultar un cruce.
