# PCB-M — decisiones D8–D15 y verificación (2026-11-12)

Trabajo exclusivamente en `/private/tmp/grooveforge-pcbm2`, rama `claude/monorepo-groovepush`,
base **670457f**. Sin push. Layout `.kicad_pcb` y proyecto `.kicad_pro` intactos; ninguna placa
PCB-A/K/F/P se modifica o regenera. La API pública de `schgen` y `check_netlist.py` no cambia.
Entrega: `/private/tmp/grooveforge-pcbm2.patch`, diff binario staged contra 670457f.

La descripción `schgen/pcb_m.py` aplica las decisiones **aprobadas por Juan el 2026-11-12**,
registradas en `apps/docs/consola/24-placa-principal.md` §8. Theory previa en [THEORY.md](THEORY.md).
El contrato de cableado y BOM están en [PCB-M README](../boards/pcb-m/README.md); el contrato
fabricado de PCB-A y la integración vigente en [CONNECTORS.md](../boards/CONNECTORS.md).

## Resultado real

- `python3 -m pytest -q apps/hardware/kicad/consola/tools/tests --basetemp=.schgen-test-tmp`:
  **34 passed** (25 existentes actualizados para conectores numéricos + 9 nuevos casos de contrato PCB-M).
  Incluye exportación KiCad real, rechazo de XML corrupto, geometría y UUID persistentes.
  Los nuevos casos cruzan J1 contra CONNECTORS.md y ENCODER_PINS contra firmware, además de
  verificar faders, NC, protecciones, RC, power/net-tie, selector, MIDI y mazos de jacks.
- `python3 apps/hardware/kicad/consola/tools/check_netlist.py --regenerate`:
  **PASS**, KiCad **10.0.6**, **122 componentes / 415 pines / 23 NC / 103 redes con nombre**.
  Cuenta componentes físicos del XML, no los PWR_FLAG virtuales. Compara todos los pines y
  sus particiones/nombres de red; rechaza NC sin marca, fusiones/divisiones y componentes extra/ausentes.
- **ERC: 0 errores, 7 avisos**, íntegro en `boards/pcb-m/docs/erc-report.txt`.
  Cinco avisos `isolated_pin_label`: I2S TX/RX/MCLK/BCLK/LRCLK internos al módulo Audio Shield,
  representados solo en sockets. Dos: `LINE_IN_L_R` / `LINE_IN_R_R`, sin terminación definida.
  Mismos avisos que en la base; no se agregan exclusiones ni se modifica la configuración ERC.
  Los checks ignorados que enumera KiCad proceden de la configuración heredada.
- XML y PDF regenerados. PDF **11 páginas** (raíz + 10 hojas), rasterizado completo con
  `pdftoppm` y revisado visualmente, incluidas hojas CV y alimentación a mayor escala.
  Se compactaron las notas de alimentación para evitar invadir el texto de F1.
- Regeneración consecutiva de los **11 `.kicad_sch` idéntica byte a byte**.
- La CLI sigue emitiendo el aviso heredado de errores de anotación por las referencias descriptivas.
  Se conservan las referencias del proyecto; exporta todos los pines esperados y el ERC cuenta cero errores.

## Contradicciones revisadas

| Contradicción registrada en la revisión anterior | Estado / decisión vigente |
|---|---|
| Cross-reference README J1.15/17→ENC1, .2/4→ENC5 y .8 duplicado como MIDI/ENC6_B | **Resuelta D8.** Manda PCB-A fabricada, cable 1:1: ENC1 5/7, ENC2 9/10, ENC3 13/15, ENC4 17/19, ENC5 8/21, ENC6 23/25, ENC7 28/29, ENC8 27/24. Teensy según ENCODER_PINS; faders J1.16/14/12/11→ADS0x48 AIN0–3. Tabla README corregida. J1.18 spare/.20 INT_K/.22 INT_A/.26 LED_DIN NC; LEDs por J_PCB_K.5. |
| HP_CUE.S=AGND frente a HP_VGND; duplicación J_HP/J_HP_CUE | **Resuelta, corrección previa conservada + D15.** J_HP es el único JST de audífonos, pin3=HP_VGND. Se elimina J_HP_CUE. HP_VGND no se une a GND/AGND. LINE OUT sigue después de 10µF. |
| J_PAD_SEL de 3 hilos duplicaba SW_PAD_SEL y la escalera | **Resuelta D13.** Selector en panel, SW_PAD_SEL eliminado; divisor permanente R_SEL_T/B 10k en PCB-M con punto medio PAD_SEL_ADC. Polo C común→J.2, extremos→GND/3V3, contacto central sin cable; 0/1,65/3,3 V con tres hilos. Polos A/B de audio siguen cableados al header 2×4 representado por SW_PAD. |
| Doc24 §2/§3/§8 ubicaba MCP0x23 en PCB-M frente a §9/README/doc17 | **Resuelta documentalmente.** 24 corregido: ningún MCP en PCB-M; 0x23 en PCB-K, JOG_CLICK/PEDAL_BTN por J_PCB_K. Hoja 04 vacía conservada. |
| Doc24 hablaba de K1/K2 y 41 entradas frente a PCB-K única y 36 en doc17 | **Resuelta documentalmente en 24.** PCB-K única con 0x21/0x22/0x23, reparto 36/56 según doc17. No se edita firmware ni otras placas/documentos históricos. |
| Captura antigua LDO +3.3V/+3.3VA y retornos GND/AGND; faltaba unión estrella y conector primario | **Resuelta D9/D10.** LDO→+3.3VA; +3.3V desde Teensy. J_5V_LED_IN S2B-PH-K (BOM)→F1 0,75 A→+5V_LED. NT1 Device:NetTie_2 / NetTie-2_SMD_Pad0.5mm entre AGND y GND; colocación junto a LDO obligatoria en layout. J_5V_BKUP sigue DNP. Rieles 5V_LED y 5V_TEENSY separados. |
| CV sin 1k + diodos y pots sin RC 1k/100nF de doc24 | **Resuelta D11.** 1k serie y BAT54S por CV, nodo del jack protegido a GND/+5V_LED; feedback antes de 1k. RC independiente por MASTER/CUE/IN, capacitor en lado ADC. |
| MIDI OUT doc24 2×47Ω y README 100Ω extra | **Resuelta D12.** CA-033: 33Ω ≥0,5W al riel 3,3V, 10Ω ≥0,25W desde TX; sin resistor extra. TRS tipo A: tip DIN5, ring DIN4, sleeve DIN2 (IN NC/OUT GND). Footprints 1210/1206. Opto IN y polarización previa conservados. |
| Jacks de PCB sin pieza/huella definitiva y selector sin decisión mecánica | **Resuelta arquitectura de montaje D13/D15.** Jacks con tuerca en pared trasera, compra Bogotá, un JST-PH por jack; mazos documentados. Ningún jack anterior tenía contacto de detección de inserción: no se pierde uno ni se inventa señal nueva. Selector en panel; queda por escoger su pieza mecánica. |
| Pull-ups PCB-A duplicables | **Resuelta D14.** 2,2k en PCB-M; **desoldar R1/R2 de PCB-A**, documentado en README y CONNECTORS. No se cambia la PCB-A fabricada. |
| T27 reservado e-drum frente a routing 74HC4053 del Brain | **Abierta, fuera de D8–D15.** T25/27/38/39 y J_EDRUM siguen NC; J_EDRUM DNP. |
| Anillos LINE IN sin terminación | **Abierta.** Se conservan señales R hacia el mazo; no se inventa una entrada balanceada. Son dos avisos ERC conocidos; resolver antes de fabricar. |
| Protección TVS/diodo de entrada 5V | **Nota D9 aplicada; pieza pendiente.** No hay TVS/bloqueo de entrada adecuado en la BOM de PCB-M. BAT54S/1N4148W de señal no sirven como reemplazo. No se añade una pieza nueva sin decisión de BOM. |

## Límites

Esto valida la captura y los contratos aprobados, no el layout. La posición trasera de JST y el
punto estrella junto a U_LDO1 están documentados para la fase de colocación, no implementados en
`.kicad_pcb`. El net-tie mantiene nombres de red distintos y su footprint estándar permite la
unión física controlada. No acredita DRC, consumo real/USB, estabilidad analógica, tolerancias,
transitorios, MIDI/CV en banco ni terminación balanceada. Falta seleccionar jacks/selector del
panel y SKU/potencias de componentes nuevos antes de fabricar. No se genera ninguna otra placa.

Instrucciones de uso y alcance del generador: [README.md](README.md).
