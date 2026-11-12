# PCB-M — reparación de captura (2026-11-12)

Clon `/private/tmp/grooveforge-pcbgen`, rama `claude/monorepo-groovepush`, base `0a5f53d`.
Sin push; layout y proyecto `.kicad_pro` intactos. PCB-K/F/P no generadas.

Generador Python 3 sin dependencias externas: librerías estándar instaladas, herencia de símbolos,
unidades múltiples, grilla 1,27 mm, pines rotados, cables de 2,54 mm, etiquetas locales/globales,
NC explícitos, colisiones antes de escribir y UUID v5 persistentes. Descripción en `schgen/pcb_m.py`.
Se conserva la hoja 04 vacía en la raíz con una nota: MCP 0x23 trasladado a PCB-K.
Se capturaron resistores CV/MIDI, todas las unidades del MCP6004 CV y desacoplos faltantes.
El frente analógico conserva su relación pin/red. HP y LINE OUT siguen README D4/D9.
Footprints inválidos de ADS1115 sustituidos por el footprint de la propia librería instalada
`Package_SO:TSSOP-10_3x3mm_P0.5mm`; F1 y C_LDO_OUT siguen los tamaños de la BOM del README.

## Verificación real

- `python3 -m pytest -q apps/hardware/kicad/consola/tools/tests --basetemp=.schgen-test-tmp`:
  **25 passed**; incluye exportación KiCad real con cuatro rotaciones y NC, y rechazo de XML corrupto.
- `python3 apps/hardware/kicad/consola/tools/check_netlist.py --regenerate`:
  **PASS**, KiCad **10.0.6**, **113 componentes / 398 pines / 21 NC / 100 redes con nombre**.
  Detecta redes fusionadas/divididas, componentes/pines extra o ausentes, nombres distintos y NC sin marca.
- ERC: **0 errores, 7 avisos**; informe íntegro en `boards/pcb-m/docs/erc-report.txt`.
  Cinco `isolated_pin_label` son I2S TX/RX/MCLK/BCLK/LRCLK internos al módulo, representados
  solo en los sockets. Dos son `LINE_IN_L_R` / `LINE_IN_R_R`, sin terminación documentada.
  No se añadieron exclusiones ERC. PWR_FLAG representa fuentes/retornos externos; no crea una fuente física.
- XML y PDF actualizados; PDF **11 páginas** (raíz + 10 hojas), revisado mediante rasterización.
  Regeneración consecutiva de los **11 .kicad_sch idéntica byte a byte**.
- La exportación avisa de errores de anotación con las referencias heredadas; se conservaron las
  referencias solicitadas. El XML conserva todos los componentes/pines esperados; ERC no reporta errores.

## Contradicciones y decisiones de hardware abiertas

No se reescriben los documentos fuente ni se declaran resueltas estas decisiones. El README es
prioritario frente a docs 24/17; donde su propia tabla es inconsistente, se conserva la captura
existente explícitamente como provisional:

| Conflicto | Captura / pendiente |
|---|---|
| README cross-reference asigna J1.15/17 a ENC1 y J1.2/4 a ENC5; la captura/CONNECTORS asigna 15/17 a ENC3_B/ENC4_A y 2/4 a +5V/GND. Además J1.8 aparece como MIDI_TX y ENC6_B. | Se conserva el pinout **preexistente de J1** (no se infiere un recableado imposible desde esa tabla); 20/22 NC según README D1. La tabla requiere decisión del autor antes de fabricar. |
| README tabla HP_CUE.S=AGND frente a D4/D9.S=HP_VGND. | Se aplica la corrección explícita D9: HP_VGND. Ninguna unión a AGND/GND. |
| README D10: J_PAD_SEL de 3 hilos duplica SW_PAD_SEL + escalera en 02. | Se conservan ambos, sin decidir ubicación ni cable de panel. |
| Doc24 §3/§8 ubica MCP0x23 en PCB-M; §9, README y doc17 lo trasladan a PCB-K. | Sin MCP en PCB-M; J_PCB_K transporta JOG_CLICK y PEDAL_BTN. |
| Doc24 menciona K1/K2 y 41 entradas; README/doc17 usan PCB-K única y doc17 cuenta 36. | No se generan ni se rediseñan otras placas ni se cambia el mapa de firmware. |
| Alimentación: captura antigua unía el mismo LDO a +3.3V/+3.3VA y su retorno a GND/AGND; README D4 pide +3.3VA. | LDO→+3.3VA/AGND; +3.3V queda en sockets Teensy. GND/AGND siguen distintos: falta especificar su unión estrella y alimentación primaria USB-A3 (solo existe J_5V_BKUP DNP). |
| Doc24 pide 1k + diodos por CV y RC 1k/100nF por pot; README no fija esos componentes. | No se incorporan protecciones/RC no tabulados. Confirmar contrato antes del layout. |
| Doc24 MIDI OUT dice 2×47Ω; README D3 añade 100Ω serie. | Se capturan D3: 47Ω en T/R y 100Ω en TX. Polarización de entrada añadida explícitamente: 220Ω LED, 4.7k pull-up RX, 10k base, 1N4148W inversa y 100nF; validar valores en banco. |
| README D5: T27 reservado e-drum frente a routing 74HC4053 del Brain. | T25/27/38/39 y J_EDRUM NC; J_EDRUM DNP. |
| Jacks traseros y selector 3P3T sin pieza/huella definitiva; pull-ups PCB-A duplicables (D7). | Se conservan footprints vacíos donde no hay selección; no inventar una pieza equivalente. Confirmar mecánica, BOM F1 y retirar pull-ups duplicados. |

Esto valida la captura contra su descripción, no resuelve esos contratos ni acredita consumo,
estabilidad analógica, aislamiento/retornos, MIDI/CV en banco o fabricación. Instrucciones y API:
[README.md](README.md); fundamento previo a implementación/tests: [THEORY.md](THEORY.md).
