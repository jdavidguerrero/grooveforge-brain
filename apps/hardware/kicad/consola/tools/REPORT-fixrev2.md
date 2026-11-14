# Segunda revisión: correcciones y verificación — 2026-11-13

Clon: `/private/tmp/grooveforge-fixrev2`; rama `claude/monorepo-groovepush`;
base `edc67a3d378ef4f0a24941153e993b5a83acd856`. Se continuaron los cambios sin commit
existentes. Sin commit ni push. Spec previo: [doc24 §8 D38–D43](../../../../docs/consola/24-placa-principal.md)
y [doc28 §9](../../../../docs/consola/28-pcb-k.md); D40 actualizado por decisión de Juan
2026-11-13. Teoría: [THEORY-fixrev2.md](THEORY-fixrev2.md).
Entrada histórica conservada: [revisión adversarial 2](../docs/revision-adversarial-2-2026-11-12.md).

## Hallazgo → hecho / pendiente

| Hallazgo | Hecho en esta entrega | Pendiente y límite |
|---|---|---|
| **A1 / D40** | Cable soldado a pads **por rótulo**, sin numeración externa: **SDA=TX M4 → J3_M4.3 → 1 kΩ → RX Teensy 28**; **SCL=RX M4 ← J3_M4.4 ← 1 kΩ ← TX Teensy 29**; **GND → J3_M4.1/GND**; **3V sin conectar**, J3_M4.2 NC. harness.py usa claves GND/3V/SDA/SCL y biyección explícita. README de herramientas/PCB-M, ASSEMBLY y HARNESS concuerdan. check_harness sin aviso pendiente por CONN1. | Juan confirmará los **rótulos impresos** antes de soldar; continuidad física no medida. No depende del orden ni de la vista de un conector M4. |
| **A2 / D38** | SJ_CV desde **+5V_TEENSY**, filtro **10 Ω + 47 µF**, desacoplos 100 nF/10 µF y MCP6004 hoja 05 en **+5V_CV**. **Ambos clamps BAT54S superiores también a +5V_CV**, corregidos en generador, esquema y XML real. +5V_LED queda para LEDs/indicador, 74AHCT1G125 y Sharp. README y spec actualizan secuencias/consumo. | Medir rampas/inrush y pérdida de cada USB. Mismo origen no acredita límites absolutos durante rampas, apertura de SJ_CV o CV externo que realimente +5V_CV. No abrir SJ_CV con DAC activo. Reserva de rama: 2 mA con dos cargas externas de 10 kΩ; no cota para corto. |
| **A3 / D39** | R_M4_RX1 y R_M4_TX1 **1 kΩ 0805**, C17513, una en cada línea, con campo Placement junto a J3_M4. **Baud real 1.000.000**: Config.h/M4 Serial1 y Surface.cpp/Teensy Serial7. Con C total ≤100 pF y R+1 %, τ≤101 ns, subida 10–90 %≤222 ns; a mitad del bit alcanza ≥99,2 %. Se conserva margen en ese modelo. | Layout debe colocarlas junto al conector. Medir capacitancia/bordes y errores del mazo a 1 Mbps. Corriente ideal ≤3,34 mA con un lado apagado **no** equivale a Ioff ni demuestra cumplimiento con NVCC apagada. Hot-plug y brownout no certificados. |
| **A4 (fuera del cambio de hardware)** | Se conserva contrato de cero de ambos DAC, sin editar firmware. La base actual ya contiene `CvDac.cpp` con servicio 0x60/0x61 y comando explícito `@GF CV EEPROM0`; la observación de la revisión sobre 095370c es histórica. | Validar firmware objetivo, cero temprano, provisionado/lectura/POR y reset MCU con ambos jacks medidos. No se ejecutó ni certificó firmware en esta entrega; no hay mute físico que garantice cero durante reset. |
| **A5 (fuera del alcance aprobado)** | Se conserva la huella Choc/CPG135001S30 y su marca provisional; Supply=owner no se presenta como validación mecánica. | Cupón con tecla/zócalo/chapa reales, plástico/PTH y encaje, más luz del LED inverso. No aprobar fabricación por PASS de compras. |
| **M1 / D41** | PESD3V3U1UL y SMAJ5.0A con símbolo **Device:D_Zener unidireccional K1/A2**. Pin1 sigue a PEDAL_BTN/+5V_LED y pin2 a GND: redes conservadas. Huellas locales D_SOD-882_K1/D_SMA_K1 conservan geometría/pads estándar y añaden **K en F.SilkS junto al cátodo**. fp-lib-table de M resuelve la biblioteca local. | Verificar marca/orientación y futuras rotaciones CPL en layout/ensamble. No hay layout final de M aquí. |
| **M2 / D41** | check_bom cubre **M, K izquierda, K derecha, A, F, P**. Lee jerarquía de esquema real, XML KiCad y CSV exportado; compara conjuntos de referencias, duplicados y atributos, incluyendo DNP y excluidos de BOM. CSV debe contener exactamente in_bom=yes. Exenciones por **lib_id/libsource** de TP/NT/SJ/agujeros, nunca por texto de huella. RS6011 ×4 y FSR402 ×6: **owner/Juan**. Se añaden BOM reales F/P. Sockets FSR alternativos: **compra externa**, sin código verificado, no poblados. | Cierre de identidad/completitud no valida stock, mecánica ni futura CPL/cobre. El IC con huella TestPoint y sin LCSC sigue fallando; cambiar además libsource en XML difiere del esquema independiente. |
| **M3 / D42** | MASTER/CUE/IN: pull-down **100 kΩ C25741** tras 1 kΩ/100 nF. Pot de 10 kΩ: error máximo ideal **2,108 % FS** cerca de x=0,7174, media escala **1,691 % FS**, relativo máximo **3,382 %**. Cable abierto τ nominal=10 ms; con R+1 %, C+20 % y fuga +1 µA: V(100 ms)≤0,102 V. Contrato firmware: umbral **≤0,15 V durante 100 ms**, esperar al menos **100 ms de establecimiento** al conectar/arrancar. | Ajustar/calibrar curva y extremos; ADC sin pull-up, medir fugas/temperatura/muestreo. Suprimir CC espurios en reposo. Cable abierto y mínimo legítimo son indistinguibles: estado seguro común, no diagnóstico inequívoco de rotura. |
| **M4 / D43** | Solape por contornos/placement: **V1–V4 bajo K derecha**, X1/X2 fuera. Landings (y=73,5) bajo material K; las ventanas Ø20 de disco no los liberan. Luz **1,5 mm**. Solución elegida: **soldadura directa de los pines metálicos prensados**, sin socket en los seis FSR. Huella DirectSolder, esquema, placement y docs preservan centros disco/landing; inserción superior, soldadura inferior, envolvente superior completa **≤1 mm** (≥0,5 mm nominal de margen). | Con pieza real comprobar Ø1 mm, crimpado, cola y altura completa. Disipar por metal, sin calor sobre película ni pliegue forzado, alivio de tensión. **El layout de PCB-P debe rehacerse con esto**; su .kicad_pcb está intacto por alcance. La prueba geométrica no mide el lote de sensores. |
| **B1 / D41** | Las 63 identidades del catálogo tienen URL **HTTPS exacta por código**; check_bom rechaza texto libre, HTTP y código de URL distinto. C41376163, C225519 y C7469972 sustituyen «buscador LCSC» por enlaces de producto. Fuentes de inventario externo separadas. | Identidad nominal no es reserva de stock ni aprobación de todas las cotas de terminales. |

Referencias de compra contrastadas durante esta continuación:
[C41376163 / HC-PM254-8.5H-1x24PZ](https://www.lcsc.com/product-detail/C41376163.html),
[C225519 / A2541WV-2x4P](https://www.lcsc.com/product-detail/C225519.html),
[C7469972 / FVH016ADA470M0554](https://www.lcsc.com/pl/product-detail/C7469972.html),
[C17513 / 1 kΩ ±1 % 125 mW 0805](https://www.lcsc.com/product-detail/C17513.html).
La ficha LCSC polaca C7469972 apareció indexada en el buscador web; la apertura directa falló
por caché en la herramienta web. No se sustituyó por otro fabricante ni se inventó una ficha.
Fuentes eléctricas y ecuaciones: doc24 D38/D39/D42 y THEORY-fixrev2.

## Resultados reales

Python **3.14.3**, KiCad CLI **10.0.6**. Comandos desde la raíz del clon:

```sh
python3 -m pytest -q apps/hardware/kicad/consola/tools/tests --basetemp=.fixrev-tmp/pytest
# 127 passed in 49.32s; exit 0, sin skips
python3 apps/hardware/kicad/consola/tools/check_netlist.py --all --directory .fixrev-tmp/validation
# Connectivity PASS en las seis placas; ERC 0 errores; exit 0
python3 apps/hardware/kicad/consola/tools/check_harness.py
# 32 cables, 0 errores; exit 0; sin WARNING por CONN1
python3 apps/hardware/kicad/consola/tools/check_bom.py
# seis placas, 0 errores de identidad y 0 compras sin verificar; exit 0
```

`--directory` contiene copias de **los esquemas finales**, sin regenerar para ocultar cambios,
sus subhojas y proyectos. Las fp-lib-table de la copia se resuelven a las bibliotecas del clon.
Se usa esa opción para que XML/ERC/PDF de **PCB-A** se escriban solamente en copia, dejando
toda la carpeta original excluida intacta. Sin proyecto/table local, la copia produce avisos
adicionales de biblioteca; la ejecución final incluye ambos y conserva los avisos reales.
Para reproducir: copiar cada `<placa>.kicad_sch`, `sheets/` si existe y `<placa>.kicad_pro` a
`.fixrev-tmp/validation/<placa>/`; crear fp-lib-table con gf_consola apuntando al absoluto
`apps/hardware/kicad/consola/lib/gf_consola.pretty` del clon y groovepush al absoluto
`apps/hardware/kicad/consola/boards/lib/groovepush.pretty`. No copiar rutas relativas originales
sin ajustarlas. KiCad configura sus temporales/cache dentro de la carpeta de salida.

| Placa | Componentes | Pines | NC | Redes | Conectividad | ERC errores / avisos | BOM errores / compras sin verificar / externos |
|---|---:|---:|---:|---:|---|---|---|
| M | 228 | 624 | 23 | 150 | PASS | 0 / 7 | 0 / 0 / 1 |
| K izquierda | 78 | 241 | 8 | 58 | PASS | 0 / 0 | 0 / 0 / 17 |
| K derecha | 43 | 127 | 5 | 32 | PASS | 0 / 0 | 0 / 0 / 9 |
| A | 37 | 166 | — | 48 | PASS contrato congelado | 0 / 0 | 0 / 0 / 9 |
| F | 10 | 32 | 0 | 6 | PASS | 0 / 0 | 0 / 0 / 4 |
| P | 12 | 24 | 0 | 12 | PASS | 0 / 0 | 0 / 0 / 6 |

Los siete avisos M son cinco etiquetas I2S internas y los dos rings LINE IN sin terminación;
no se ocultaron con exclusiones nuevas. No equivalen a ERC sin avisos ni recepción balanceada.
Las seis BOM se reexportaron además con la CLI real usando
`sch export bom --fields Reference,Value,Footprint,LCSC,Supply,SupplyDescription,DNP --labels`
con la misma lista: **idénticas byte a byte a fab/bom-<placa>.csv** en las seis.

Pruebas de mutación: fila XML/CSV faltante, extra y duplicada; IC sin identidad disfrazado con
huella TestPoint; libsource falso; URL HTTP/texto/código distinto; cruce SDA/SCL y sustitución
de rótulos M4 por números. Las regresiones existentes siguen validando aislamiento HP_VGND,
IDC fabricado, separación de I2C, GPIO y cadena de 26 LEDs. La primera ejecución detectó las
expectativas antiguas de clamps y la hoja 05 aún sin regenerar; se corrigieron antes del PASS.

## Lista para firmware / banco (sin cambios de firmware)

1. **Pots / D42:** incorporar el nuevo error de carga de 100 kΩ en calibración/curva; no usar
   el cálculo histórico de 1 MΩ. ADC sin pull-up; esperar ≥100 ms al arrancar/conectar y confirmar
   ≤0,15 V durante 100 ms antes de tratarlo como reposo seguro. A 3,3 V equivale aproximadamente
   a 4,55 % FS; convertir con referencia/resolución reales, no fijar un código ADC arbitrario.
   Pot mínimo y abierto comparten estado seguro; suprimir fluctuaciones/CC espurios y medir
   fuga real. El modelo garantiza convergencia bajo el umbral a los 100 ms, no identifica un cable.
2. **UART / D39–D40:** conservar Serial7 RX28/TX29 en el target Console y Serial1 M4
   TX21=SDA/RX22=SCL, ambos 1 Mbps (ya están en la base). Verificar mazo con 1 kΩ por línea,
   errores/latencia y bordes en ambas direcciones; apagar USB de cada lado y medir inyección/rails
   antes de aceptar secuencia de uso. No dar por soportado hot-plug por tener resistencias.
3. **CV / A4:** revisar ejecución de cero temprano a 0x60/0x61 y comando explícito de fábrica
   `@GF CV EEPROM0`, ya presentes en la base. Verificar lectura, POR, reset MCU y pérdida USB
   con osciloscopio; nunca EEPROM en cada boot. Amplificador/clamps ahora comparten origen Teensy;
   ensayar inrush, rampa RC, SJ_CV y backfeed externo. No existe mute de hardware.
4. **Seguimiento previo:** EN_K=0/EN_A=1 ya declarados en la base; comprobar reglas de bus
   global **y puerto** ociosos al conmutar PCA9517A, sin inferir idle de un timeout. Mantener
   init códec antes de LED DMA y brillo/corriente limitados con 26 LEDs y M4. Validar pedal,
   perfiles mínimos FSR y calidad ADC de Sharp al apagar cada dominio. No se verificó ejecución
   on-device ni se modificó firmware para estos puntos.

## Alcance de entrega

Cambios en specs Console 24/28, herramientas/tests, esquemas/artifacts de M/K/F/P, catálogo,
BOM y montaje/mazos. **Intactos**: firmware (también apps/consola), toda boards/pcb-a,
boards/pcb-f/*.kicad_pcb y boards/pcb-p/*.kicad_pcb. No hay layout nuevo, DRC/Gerbers/CPL final
ni ensayo físico; quedan pendientes A4/A5 y las comprobaciones de banco/mecánica indicadas.
El cierre software del encargo no es liberación de fabricación del paquete de seis placas.

Entrega preparada con `git add -A` y
`git diff --cached --binary edc67a3 > /private/tmp/grooveforge-fixrev2.patch`.
Evidencia local no versionada: `.git/fixrev2-validation/` (logs y copias de validación).
Los temporales de revisión quedan fuera del índice; el patch contiene fuentes y artefactos de entrega.
