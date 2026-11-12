# Corrección de revisiones — 2026-11-12

Clon de trabajo: `/private/tmp/grooveforge-fixrev`, rama `claude/monorepo-groovepush`,
base `a37ecc51233bd8569729942ed1ca0cdd23c1cb94`. Sin commit ni push. Mandato: los 14 puntos
aprobados por Juan, registrados **antes del cambio de esquema** en doc24 §8, D24–D37.
Teoría previa: [THEORY-fixrev.md](THEORY-fixrev.md). Revisiones de entrada:
[adversarial](../docs/revision-adversarial-2026-11-12.md) y
[hardware](../docs/revision-hw-2026-11-12.md). Se conservan intactas como evidencia histórica.

No se editaron `apps/firmware-teensy`, `apps/consola`, `boards/pcb-f` ni ningún
`boards/pcb-p/*.kicad_pcb`. PCB-A conserva esquema/layout/conectividad fabricados.
PCB-F se regeneró y comprobó en `.fixrev-validation/pcb-f` dentro del clon, sin copiar
sus archivos sobre la placa excluida. Los artefactos entregados de M/K/P se exportaron
además desde sus rutas definitivas. No hay layout nuevo ni aprobación de fabricación.
Las ejecuciones iniciales de pytest usaron el temporal predeterminado del sistema;
la validación final fija temporales y `--basetemp` dentro del clon. No se editaron fuentes externos.

## Los 14 puntos del encargo

| Punto | Decisión | Qué cambió y límite |
|---|---|---|
| 1 UART | D24 | PCB-M TX29→J3_M4.4→CONN1.4 SCL/RX22; RX28←.3←SDA/TX21. .1 GND, .2 NC. Harness describe número físico y función; docs17/18/24 y CONNECTORS corregidos. Macros/docs bajo apps/consola pendientes de la otra tarea. |
| 2 compras | D25 | Campo LCSC en cada símbolo físico M/K; 59 identidades verificadas en catálogo con MPN, fabricante, encapsulado, fecha, URL y whitelist de huellas. M tiene cinco compras aún sin código; no se inventaron. Revisión A por netlist/BOM existente. README deja de ser fuente de códigos. Nuevo check_bom rechaza ausentes y paquetes/valores/metadatos incompatibles. |
| 3 dominios | D26 | IN/EN/C_IN del TLV75533 a +5V_TEENSY; ADS y frente en +3.3VA. Sharp→ADS 10k serie. DAC→MCP6004 CV **2,2k** serie: ≤1,50mA incluso contando toda la tensión3,3V, frente límite ±2mA. 1k propuesto como ejemplo no daba ese margen. Secuencias de cada USB documentadas; medir inyección/rails fantasma, no afirmar operación válida sin VDD. |
| 4 CV cero | D27 | Ambos canales poblados. Setup debe escribir cero a0x60/0x61 y provisionar/verificar EEPROM cero una vez. No mute adicional: un switch doble económico también necesita alimentación/estado seguro y no se justifica sin exigir cero garantizado durante reset. Reset MCU conserva salida anterior hasta escritura. |
| 5 shield | D28 | Sustituido conector genérico de cuatro señales por IN/OUT/HP JST-PH3, mazos cortos soldados a pads descritos desde reverso. Tabla física, dos fotos, contactos nombrados en harness y advertencia VGND. G de shield va a GND, común a Teensy por headers, para no crear segundo enlace AGND/GND fuera de NT1. |
| 6 pedal | D29 | J_PEDAL tip PEDAL_RAW→1k→PEDAL_BTN→J_PCB_K.10; TVS ESD unidireccional PESD3V3U1UL,315 3,3V, K señal/A GND, pull-up10k y10nF. Doc28 §5 actualizado. Colocar junto al conector en layout; prueba ESD física pendiente. |
| 7 EN | D30 | EN_K pin0/EN_A pin1, cada uno10k y puenteGPIO cerrado independiente. Se quitó EN común y su jumper HIGH común. Alta impedancia al reset habilita por pull-up. Solo cambiar EN con ambos buses ociosos; fallo retenido en bajo requiere servicio sin alimentación, no toggle arbitrario. |
| 8 reposo | D31 | 100k de entrada A bufferLED (T13) a GND; 1M por pot tras RC. Error máximo calculado para pot10k≈0,21654%FS (redondeado0,217%); media escala0,1744%FS, cota relativa0,349%. Cable abierto τ≈100ms. |
| 9 FSR | D32 | Foto identifica machos prensadosP2.54. Nueva huella THT hembra/soldadura1×2, taladro1mm, pads separados2,54mm. Datum de disco CSV y seis centros sin cambios, landing en x±1,27/y_KiCad+11 hacia disco; placement y README explican dirección. Confirmar acomodo/altura de cola con pieza real. LayoutP no tocado. |
| 10 entradaLED | D33 | F1→P-MOSAO3401A→+5V_LED, SMAJ5.0A después de fusible hacia GND. F1 protege frente fallo sostenido de TVS; PMOS evita caída Schottky (≤43mV/21mW con0,5A y85mΩ). TVS standoff5V, clamp9,2V al pulso nominal: no promete protección contra sobretensión continua ni clamp a5V. |
| 11 PCA | D34 | C80799=NXP PCA9517ADP,118, MSOP8 comercial/SOT505-1TSSOP8 de NXP. Huella KiCad MSOP-8_3x3mm_P0.65mm; pads x±2,1125,1,625×0,4mm, paso0,65. Body2,9–3,1mm, ancho total4,7–5,1mm del fabricante. |
| 12 montaje | D35 | README y ASSEMBLY: **NO cortar VUSB–VIN**; solo USBTeensy alimenta +5V_TEENSY. Desoldar/no poblar R1/R2PCB-A. La recomendación de HW B-05 se rechaza para esta arquitectura. |
| 13 LEDdoc | D36 | Doc27 J1.5→**J1.6**. Pin5 es masa. |
| 14 anotación | D37 | Sufijo numérico1 a referencias que acababan en letra; Function conserva nombre lógico. Mapa completo docs/reference-map.csv. BOM real212filas, sin «?», sin posproceso; test ya no usa rstrip('?'). XML también compara valor, huella, DNP y campos contra descripción. |

## Seguimiento de cada hallazgo de las dos revisiones

| Revisión | Hallazgo | Estado |
|---|---|---|
| Adversarial1 | TX contra TX | Corregido punto1; continuidad/UART físico pendiente de banco. |
| Adversarial2 | C7466 es inversor | Corregido C12495 Nexperia74AHCT1G125GW,125 SOT353 en componente/catálogo. |
| Adversarial3 | C15578 es TLV1117SOT223 | Corregido C404027 TI TLV75533PDBVR SOT23-5. |
| Adversarial4 | alimentación parásita entre dominios | Cambiado punto3; limitación serie documentada y ensayo apagado pendiente. |
| Adversarial5 | CV2 media escala de fábrica | Contrato/provisionado punto4; implementación y ensayo pendientes a firmware/hardware. |
| Adversarial6 | contrato físico shield ausente | Corregido punto5 con fotos de Juan y mazos verificables por contacto. |
| Adversarial7 | protección pedal ausente | Corregido punto6. |
| Adversarial8 | cuerpo hot-swap no demostrado | **Pendiente físico**: cupón PG1353+CPG135001S30 con espesor real/chapa. No se cambió huella ni se certificó inserción sin pieza. |
| Adversarial9 | ramas averiadas afectan códec | EN separados punto7; firmware pendiente. No garantiza recuperación de corto permanente por EN: NXP exige global+puerto ociosos. Abrir SJ_SDA/SCL sin alimentar es vía de servicio. |
| Adversarial10 | LED/pots flotantes | Corregido punto8; comprobar reset y lectura de cable suelto en banco. |
| Adversarial11 | terminación FSR indeterminada | Corregida eléctricamente/mecánicamente según foto, punto9; verificar cola/altura real. |
| Adversarial12 | LED_DINdoc erróneo | Corregido punto13. |
| HW M-01 | fusible1206/1812 y C17357 | **C17357 no es fusible**. Elegido Bourns MF-MSMF075/24-2 C208467, hold0,75A/trip1,5A,1812, catálogo confirmado. Eliminado override1206. |
| HW M-02 | retirar pull-upsA no estaba en montaje | Checklist ASSEMBLY y README explícitos; no se editó PCB-A fabricada. |
| HW M-03 | sin TVS/inversión | Corregido punto10. |
| HW M-04 | PCA footprint | Corregido punto11 con dibujo NXP y geometría KiCad. |
| HW M-05 | causa startup códec con LEDs abierta | **Pendiente banco prioritario** con26LED desde power-on, medir rails/bus/DIN; preservar init códec antes de DMA en tarea firmware. No atribuir causa física a software sin medida. |
| HW B-01 | cupón zócalo | Pendiente, mismo Adversarial8. |
| HW B-02 | cupón luz LED inverso | Pendiente con4–5LED y chapa/keycaps; pinout EA se conserva. |
| HW B-03 | posible BAT54S incorrecto | Se conserva topología: Nexperia BAT54S p1 ánodo/p2 cátodo/p3 unión central, correcta para clamps entre GND/rail. Revisión adversarial ya descartó el supuesto common-anode; no se cambia por BAT54C. |
| HW B-04 | ADS huella nominal TSSOP/VSSOP | C37593 TI ADS1115IDGSR es VSSOP10/DGS,3×3/paso0,5, ancho≈4,9mm. KiCad TSSOP-10_3x3mm_P0.5mm coincide dimensionalmente; whitelist explica alias comercial. Conservar huella, DFM/ensamble real pendientes. |
| HW B-05 | cortar VUSB–VIN | **Recomendación incorrecta**; prohibición de corte registrada punto12. |
| HW B-06 | umbral FSR | Pull-downs10k conservados; validar perfil mínimo/umbral con resistencias reales en tarea firmware/banco. |
| HW B-07 | PARTS tiene huellas vacías | BOM se obtiene del esquema exportado, no de PARTS. Ningún componente físico poblado M exporta huella vacía; LCSC vacíos se enumeran abajo. No se refactorizó diccionario fuera de alcance. |

El intento adversarial de cambiar solo valor/huella/DNP ahora falla `compare_netlist` por
metadatos, aunque las redes no cambien; tests de mutación cubren esos tres casos. El catálogo
además rechaza C7466, huella incompatible, valor incompatible, MPN diferente y anotación incompleta.
Las comprobaciones mantienen aislamiento HP_VGND, pinout IDC fabricado, errata MCP, segmentos
I2C y orden de26LED. No se hacen cambios adicionales de arquitectura.

## Lista para la tarea de firmware (archivos excluidos aquí)

1. `apps/consola/processor/include/shared/Config.h`: corregir **UART_RX_PIN22 / UART_TX_PIN21**
   para M4, actualizar comentarios antiguos Serial5/20/21 según Console Serial7RX28/TX29.
   Serial1 predefinido del core M4 ya usa TX21/RX22: no crear bit-bang ni cambiar a otro SERCOM.
   Actualizar también `apps/consola/processor/UART_CONNECTIONS.md`, incluida tabla/código de ejemplo;
   queda sin editar por exclusión expresa de toda apps/consola. Verificar ambas direcciones.
2. Setup Teensy: escribir cero volátil temprano a **MCP4725 0x60 y0x61**, antes de inicialización
   de audio/superficie; comprobar ACK/fallo y no presentar CV2 como reservado/deshabilitado.
   Herramienta/procedimiento de fábrica escribe EEPROM cero una vez, verifica lectura y POR;
   nunca escribir EEPROM cada boot. Ensayar reset MCU con DAC alimentado y pérdida de cadaUSB.
3. EN_K=GPIO0 y EN_A=GPIO1: reservar ambos, no Serial1Teensy/MIDI DIN. Inicialización/diagnóstico
   y reintentos por rama solo con bus global y puerto ociosos. Timeout o SDA/SCL bajo no significa
   idle. Ante bloqueo persistente registrar fallo y pedir servicio/power-off; no prometer audio
   operativo si una rama ya cortocircuita el bus al reset con EN alto por pull-up.
4. Mantener init SGTL5000 antes de DMALED y validar arranque/reintentos con cadena26LED completa,
   medir ruido/rails/corriente. LEDs pin13 ahora tiene100k externo. Mantener limitador500mA.
5. Pots10k con1M trasRC: ajustar/calibrar si hace falta por error máximo0,217%FS; cable suelto
   decae a cero en escala100ms. No interpretar ADC sin su alimentación válida. Medir offset/settling
   ADS por10k en Sharp a la ganancia/tasa elegidas. Pedal externo activo bajo, RC10nF y debounce lógico.
6. Validar umbral FSR y perfil mínimo con los10k físicos, sin eventos espurios al aire.
   Probar secuencias de USBM4/UART y LED power sin datos para detectar backfeed antes de hot-plug.

## Límites físicos y compras pendientes

Códigos no confirmados quedan **vacíos**. El checker devuelve1 deliberadamente mientras falten;
no se convierte un pendiente de compra en PASS. Sin stock reservado ni certificación de ensamble JLC.
Los códigos verifican identidad, no disponibilidad ni costo al pedir. Los rasgos de cobre TP/NT/SJ
y agujeros no son componentes comprados y están exentos explícitamente; un conector comprado nunca.

```text
pcb-m: 226 components; 0 identity errors; 5 unverified purchases
PENDING J_T41L1: Teensy41-LeftRow | Connector_PinSocket_2.54mm:PinSocket_1x24_P2.54mm_Vertical
PENDING J_T41R1: Teensy41-RightRow | Connector_PinSocket_2.54mm:PinSocket_1x24_P2.54mm_Vertical
PENDING SW_PAD1: PAD 3P3T A/B | Connector_PinHeader_2.54mm:PinHeader_2x04_P2.54mm_Vertical
PENDING C_CV_FILTER1: 47uF 10V | Capacitor_SMD:CP_Elec_5x5.8
PENDING J1_PCBA1: IDC_2x15_PCB-A | Connector_IDC:IDC-Header_2x15_P2.54mm_Vertical
pcb-k-left: 78 components; 0 identity errors; 17 unverified purchases
PENDING SW1: BACK | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW2: MENU | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW3: NAV_L | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW4: NAV_R | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW5: BROWSE | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW6: TAP | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW7: FILL | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW8: CUE_ENG | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW9: BUILD | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW10: DROP | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW11: BREAK | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW12: SHIFT | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW13: UNDO | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW14: DELETE | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW15: DUPLICATE | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW16: QUANTIZE | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW17: NEW | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
pcb-k-right: 43 components; 0 identity errors; 9 unverified purchases
PENDING SW18: PLAY | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW19: STOP | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW20: REC | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW21: LOOP | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW22: METRO | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW23: SCENE4_PAGE_R | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW24: SCENE3_PAGE_L | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW25: SCENE2_OCT- | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
PENDING SW26: SCENE1_OCT+ | gf_consola:Kailh_Choc_V2_PG1353_Hotswap
pcb-a: 37 components; 0 identity errors; 9 unverified purchases
PENDING J1: to carrier J5 (IDC 2x15) | Connector_IDC:IDC-Header_2x15_P2.54mm_Vertical
PENDING RE1: EC11 | Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm
PENDING RE2: EC11 | Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm
PENDING RE3: EC11 | Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm
PENDING RE4: EC11 | Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm
PENDING RE5: EC11 | Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm
PENDING RE6: EC11 | Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm
PENDING RE7: EC11 | Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm
PENDING RE8: EC11 | Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm
```

Motivo: no pude cerrar con evidencia de catálogo y geometría exacta los sockets1×24,
header2×4, IDC2×15 y electrolítico5×5,8 de M. En K la referenciaSW integra tecla+zócalo;
no asigno el SKU del zócalo a la tecla ni certifico una huella mecánica aún provisional.
En A los EC11/IDC preexistentes carecen de SKU exacto confirmado. Quedan para cierre de compras
con pieza/dibujo físico; esto no impide ejecutar ERC o verificar el mazo.

Además se detectó/corrigió otro código heredado al contrastar: **C157929 es S3B-PH de3vías**,
no S2B de2vías. La entrada LED lleva **S2B-PH-K-S(LF)(SN) C173752** confirmado.
Fuente de cada identidad en [lcsc_catalog.json](lcsc_catalog.json). Referencias primarias de límites:
[ADS1115 §5.1](https://www.ti.com/lit/gpn/ADS1115),
[MCP6004 §1/4.1.2](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP6001-1R-1U-2-4-1-MHz-Low-Power-Op-Amp-DS20001733L.pdf),
[PCA9517A §6.1 yfig16](https://www.nxp.com/docs/en/data-sheet/PCA9517A.pdf),
[PESD3V3U1UL pinout](https://assets.nexperia.com/documents/data-sheet/PESD3V3U1UL.pdf),
[AO3401A](https://www.aosmd.com/sites/default/files/res/datasheets/AO3401A.pdf),
[MCP4725 POR/EEPROM §5.4](https://ww1.microchip.com/downloads/en/devicedoc/22039d.pdf),
[core M4](https://raw.githubusercontent.com/adafruit/ArduinoCore-samd/master/variants/trellis_m4/variant.h).

Persisten límites ya descritos por la revisión: una CV externa puede realimentar +5V_LED por
BAT54S; abrir solo SJ_LED_K mantiene el dato conectado a LEDs apagados (abrir también
SJ_LED_DATA para servicio, sin alimentación). USBM4 separado tampoco demuestra aislamiento
UART sin su rail. Se documentan para ensayo, sin atribuir daño medido. La terminación balanceada
de rings LINE IN, el layout/DRC final, la pasta/CPL y la orientación real de componentes quedan
pendientes de sus tareas físicas; ningún PASS lógico los certifica.

## Verificación final

Comandos ejecutados desde la raíz del clon:

```sh
mkdir -p .fixrev-tmp
TMPDIR="$PWD/.fixrev-tmp" TMP="$PWD/.fixrev-tmp" TEMP="$PWD/.fixrev-tmp" python3 -m pytest -q apps/hardware/kicad/consola/tools/tests --basetemp=.fixrev-tests
# PCB-A se copió a .fixrev-validation/pcb-a antes de verificar todo.
python3 apps/hardware/kicad/consola/tools/check_netlist.py --all --regenerate --directory .fixrev-validation
python3 apps/hardware/kicad/consola/tools/check_harness.py
python3 apps/hardware/kicad/consola/tools/check_bom.py
```

- **pytest: 102 passed** (47,14 s), incluidos tests de mutación y exportación real KiCad.
- **check_netlist --all: PASS**, las seis placas con **ERC 0 errores**. PCB-M conserva siete
  avisos conocidos (cinco señales I2S internas al módulo y dos rings LINE IN sin terminar);
  K izquierda/derecha, F, P y A: cero avisos. Sin exclusiones nuevas.
- **check_harness: 32 cables, 0 errores**, incluye mazos a pads del shield.
- **check_bom: 0 errores de identidad, 40 compras sin confirmar** (M5, K17+9, A9); salida1,
  explícitamente pendiente. Las cinco compras M y las 35 de K/A están enumeradas arriba.
- BOM real M exportada por `kicad-cli sch export bom` con campos
  **Reference,Value,Footprint,LCSC,DNP**: **212 filas, ningún «?»** y ninguna huella vacía.
  Se exportaron también K izquierda/derecha tras completar identidades de sus conectores.
- `git diff --check`: limpio. Comprobación de rutas excluidas: sin cambios en firmware,
  apps/consola, boards/pcb-f ni layoutP. Sin push. Patch binario staged contra a37ecc5.

Artefactos: BOM en `../fab/bom-pcb-m.csv`, mapa de referencias en
`../boards/pcb-m/docs/reference-map.csv`, esquemas/XML/ERC/PDF en cada placa M/K/P,
checklist en `../boards/pcb-m/ASSEMBLY.md`. Los temporales de validación no forman parte del patch.
