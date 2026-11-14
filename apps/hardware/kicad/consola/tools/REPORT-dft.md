# DFT Console — ejecución y entrega (2026-11-12)

Trabajo continuado sobre el índice y archivos parciales existentes, exclusivamente en
`/private/tmp/grooveforge-dft`, rama `claude/monorepo-groovepush`, base
`8dfc91e1467faaaef4b7be6f6d714ec37cae8a31`. Sin commit ni push.
Entrega: `/private/tmp/grooveforge-dft.patch`, diff staged binario contra `8dfc91e`.

## Contrato implementado

Spec antes de captura: doc24 §8 **D16–D23** y §10; aprobación Juan 2026-11-12.
Teoría previa y límites en [THEORY-dft.md](THEORY-dft.md).

- TP en rieles (5V antes/después de F1, 5V Teensy, 3V3, 3V3A y rieles aislados), retornos,
  VREF, CV antes/después de protección, LED_DIN_5V y SDA/SCL local y a ambos lados de
  resistencias de cada rama. El TP 5V_TEENSY mide el riel del socket, no otro contacto VUSB del módulo.
- Puentes cerrados SJ_FRONT, SJ_CV, SJ_LED_K, SJ_SHARP, SJ_LED_DATA y SDA/SCL por rama.
  R_ISO_IN_L/R **0 Ω 0805** separan los condensadores de salida del frente de LINE IN del códec;
  JP_BYP conservados. Procedimiento para seguir sin entrada/CV en doc24 §10.7; sin protoboard.
- Tres indicadores rojos 0603 ≈1 mA. Filtro CV **10 Ω 0603 + 47 µF 10 V** hacia +5V_CV;
  ganancia ideal 1,5 conservada, con caída y margen de saturación documentados.
- Toda hoja 07, TP MIDI y opciones R_OPT_MIDI están **DNP** y **fuera del BOM** (`in_bom no`).
  Pin Teensy 1 NC; 0 → SJ_EN_GPIO → EN común. MIDI por conversor USB en Pi, sin mazos/agujeros.
  `Symbol.in_bom` es independiente de DNP para conservar el BOM de referencia de las otras placas.
- 2 × **PCA9517ADP,118 C80799**, MSOP-8 3×3 paso 0,65: A local compartido, B_K/B_A separados,
  VCCA/VCCB=3,3 V, 100 nF por alimentación. Símbolo estándar TCA9517ADGK con mismo pinout;
  valor/huella/datasheet identifican NXP. EN con 10 kΩ pull-up; abrir SJ_EN_GPIO antes de cerrar
  SJ_EN_HIGH para forzar alto. No requiere añadir firmware para quedar habilitado por defecto.
- Pull-ups por cada segmento local/B_K/B_A: 2,2 kΩ por defecto con puente cerrado, alternativa
  4,7 kΩ con puente abierto. Remotos DNP; no sumar los tres segmentos como un único bus.
- **47 Ω 0805** serie por línea de cada cable I2C, comentario de sustitución por 0 Ω; ESD
  **USBLC6-2SC6 ST C7519**, SOT-23-6, dos líneas, **3,5 pF máx.** a 1,65 V.
  Stock publicado consultado: 39.775; disponibilidad variable. El ejemplo PESD5V0S2BT supera
  10 pF y se descartó. Fuentes primarias y LCSC enlazadas en doc24 §10.8.
- **J1_PCBA IDC no tiene SDA/SCL.** Su cable I2C es J2_I2C; ahí está la protección de la rama A.
  Hay dos buffers y dos ESD, uno por salida I2C real, sin inventar I2C en el IDC congelado.
- Ambas puntas M–K **PH 10 vías**, **B10B-PH-K-S(LF)(SN) C158038** verificado en LCSC:
  1 3V3 / 2 SDA / 3 GND / 4 SCL / 5 GND / 6 LED / 7 GND / 8 5V_LED / 9 JOG / 10 PEDAL.
  Actualizados doc28 §5, CONNECTORS, README PCB-M, pedido MD/CSV y BOM exportado de K izquierda.

## Resultados reales

KiCad **10.0.6**, CLI instalada en el bundle macOS. Comandos desde la raíz del clon:

```sh
python3 apps/hardware/kicad/consola/tools/check_netlist.py --all --regenerate
python3 -m pytest -q apps/hardware/kicad/consola/tools/tests
python3 apps/hardware/kicad/consola/tools/check_netlist.py --all
python3 apps/hardware/kicad/consola/tools/check_harness.py --export-pcb-a
```

Los cuatro terminaron con **exit 0**. Pytest con TMPDIR dentro del clon:
**94 passed in 41.39s**, sin skips. El último `--all` comprueba el esquema en disco sin regenerarlo.
XML, ERC y PDF actualizados para las seis placas; PCB-A solo exportada y comprobada, nunca regenerada.

| Placa | Conectividad | Componentes | Pines | NC | Redes | ERC errores / avisos | PDF páginas |
|---|---|---:|---:|---:|---:|---|---:|
| PCB-M | PASS | 210 | 587 | 24 | 140 | 0 / 7 | 13 |
| PCB-K izquierda | PASS | 78 | 241 | 8 | 58 | 0 / 0 | 11 |
| PCB-K derecha | PASS | 43 | 127 | 5 | 32 | 0 / 0 | 7 |
| PCB-F | PASS | 10 | 32 | 0 | 6 | 0 / 0 | 2 |
| PCB-P | PASS | 12 | 24 | 0 | 12 | 0 / 0 | 2 |
| PCB-A congelada | PASS frozen legacy | 37 | 166 | — | 48 exportadas | 0 / 0 | 1 |

Los **7 avisos** PCB-M son los conocidos: cinco etiquetas I2S internas al Audio Shield y dos
anillos LINE IN sin terminación definida. No se agregaron exclusiones ERC. Los checks ignorados
que enumera KiCad proceden de la configuración heredada. Persiste el aviso CLI de anotación por
referencias descriptivas; el BOM real les agrega «?». El test normaliza solo ese sufijo, verifica
que no sale **ninguna referencia MIDI**, y confirma que los buffers/conector activos sí aparecen.
Antes de usar un CSV de PCB-M para ensamble debe resolverse esa anotación heredada; no se
renombraron referencias en esta tarea.

La regeneración de PCB-F actualizó metadatos/footprints de condensadores a 0805 C49678 que
**ya estaban en su factoría de la base**, sin cambio de conectividad. No se editó su descripción.
La selección automática de papel ahora reserva 50 mm para cajetín: raíz PCB-M en A3, evitando
que las doce hojas invadan el cajetín A4. Revisados PNG de raíz, frente, CV, MIDI DNP,
conectores, diagnóstico, rama I2C y entrada de K izquierda. PDF permite zoom vectorial.

## Mazos: hallazgos efectivos

[HARNESS.md](HARNESS.md): **29 cables, 0 errores de correspondencia**, todos los XML leídos.
Incluye 11 cables entre placas y 18 contratos de panel con pin/red/color/contacto.
MIDI excluido expresamente porque no se monta. Comparación por pin físico, con número de
pines de libparts, alias por placa, dominio de riel e I2C verificado antes de alias, sin NC comodín.

No se encontraron nuevos cruces ni pines desplazados en los cables declarados. Las diferencias
IDC M.20/22/26 NC frente a INT_K/INT_A/LED_DIN de A son excepciones **exactas D8** ya aprobadas;
M.18 y A.18 NC. No se ocultaron como alias. PCB-A J4 queda sin mazo, DNP. R1/R2 de PCB-A
permanecen en el esquema como DNP: si la unidad fabricada los trae, retirarlos físicamente.
Los terminales del jog/selector/jacks/Sharp se documentan por función; falta confirmar con
continuidad la pieza mecánica comprada. El checker no mide esos mazos físicos.

Tests incluyen XML bueno, cruce de rieles, pin desplazado, pin ausente, conteo de libpart,
mapa no biyectivo, NC exacto, alias inseguro, jerarquía ambigua, panel no documentado y
cruce local/B_K/B_A incluso oculto por alias. También aislamiento de entrada/carga,
pinout buffer/ESD/10 vías y exclusión MIDI en el exportador BOM real.

## PCB-A congelada y límites

Esquema, PCB y proyecto de PCB-A son **idénticos byte a byte a 8dfc91e**. Solo cambian sus
exportaciones de revisión. Contrato de conectividad congelado PASS, sin corregir PCB-A.
La copia adicional `pcb-a-harness-netlist.xml` permite reproducir el checker con XML independiente.

Esto verifica captura y contratos de cables. Quedan fuera layout/DRC, continuidad física,
subida/VOL I2C con el mazo real, inmunidad ESD, consumo, ruido, swing CV y audio sobre placa.
El banco existente no acredita estos bloques nuevos. Doc30 se usa como guía sobre PCB,
no como requisito de hacer protoboard. Firmware y mecánica del cuerpo no se modificaron.
