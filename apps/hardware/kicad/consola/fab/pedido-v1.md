# Pedido v1 de la consola — JLC + LCSC (borrador)

2026-11-12. **Opción B** (decisión de Juan): JLC fabrica las 6 placas y **ensambla solo PCB-M** (SMD). PCB-K izq./der.
y PCB-A se sueldan a mano con chips sueltos de LCSC, que viajan en el mismo envío que las placas de JLC (carrito
combinado; elegir "ship together" al pagar).

> **Borrador de compra, un juego funcional + repuestos indicados.** BOM M/K/A reexportadas
> de los esquemas el 2026-11-12 con `Supply` y `SupplyDescription`; DNP excluidos del pedido.
> JLC ensambla los **SMD de PCB-M**: no comprarlos también como piezas sueltas. Los **THT
> de PCB-M se montan a mano** (doc24 §6) y sí aparecen aquí. La tabla JLC conserva su
> cotización preliminar de dos M ensambladas; este CSV abastece un juego manual, no dos.
> Revisar cantidades finales, precios, stock y BOM/CPL de JLC antes de pagar.

## 1. JLC — placas (2 capas, 1,6 mm, 5 unidades c/u)

| Placa | Tamaño (mm) | USD | Ensamble |
|---|---|---:|---|
| PCB-M | 186 × 60 | 9,40 | **Sí**, SMD, 2 unidades |
| PCB-K izquierda | 102 × 216 | 14,70 | A mano |
| PCB-K derecha | 144 × 152 | 14,60 | A mano |
| PCB-A | ~217 × 70 | ~11 (estimado) | A mano |
| PCB-F | 81 × 83 | 4,00 | A mano |
| PCB-P | 223 × 31 | 7,40 | A mano |

PCB-A ya diseñada, nunca fabricada. Antes de pedirla: R1/R2 (pull-ups I2C) como **no poblar** (PCB-M las tiene, D14),
J4 (cable a la vieja PCB-K1) **no poblar**, y verificar encoders en y = 192 y encaje con el cuerpo en FreeCAD.

## 2. LCSC — chips para soldar a mano (1 juego + repuestos)

| Pieza | LCSC | Cant. | Para | USD c/u |
|---|---|---:|---|---:|
| MCP23017-E/SS (SSOP-28) | C506653 | 5 | 0x20 PCB-A; 0x21 PCB-K der.; 0x22/0x23 PCB-K izq.; 1 repuesto | 1,69 |
| SK6812MINI-EA (montaje inverso) | C5378731 | 30 | 26 teclas + 4 repuestos | 0,072 |
| 100 nF 0805 X7R | C49678 | 100 | 1 por LED + 2 por MCP | 0,014 |
| 1 µF 0805 X7R 50 V | C5137478 | 20 | 1 por MCP | 0,023 |
| 22 µF 1206 X5R 25 V | C12891 | 10 | Entrada +5V_LED de cada PCB-K | 0,10 |
| 10 kΩ 0805 1 % | C17414 | 50 | RESET de cada MCP | 0,003 |
| 33 Ω 0805 1 % | C17634 | 20 | Serie de datos LED a la entrada de cada PCB-K | 0,005 |
| 100 nF 0603 | C14663 | 10 | PCB-A C1 (1 + repuestos) | sin recotizar |
| 10 nF 0603 | C57112 | 20 | PCB-A C10–C25 (16 + repuestos) | sin recotizar |
| 10 µF 0805 | C15850 | 10 | PCB-A C2 (1 + repuestos) | sin recotizar |

No comprar las pull-ups DNP R1/R2 de PCB-A ni R2/R3 de cada PCB-K. Los 100 nF 0805
cubren 29 en PCB-K y 5 en PCB-F, más repuestos. La huella de C2 PCB-A es **0805**.

## 3. LCSC — conectores de montaje manual

Códigos y orientación según catálogo/BOM vigentes, sustituyen los códigos del borrador anterior.
Cantidades para un juego, sin repuestos, salvo lo indicado.

| Pieza | LCSC | Cant. | Dónde |
|---|---|---:|---|
| JST PH 2 vías vertical B2B-PH-K-S(LF)(SN) | C131337 | 8 | PCB-M: FSR ×4, EXP ×2, CV ×2 |
| JST PH 2 vías horizontal S2B-PH-K-S(LF)(SN) | C173752 | 7 | PCB-M entrada 5V_LED ×1; PCB-P ×6 |
| JST PH 3 vías vertical B3B-PH-K-S(LF)(SN) | C131339 | 15 | PCB-M: shield IN/OUT/HP ×3; HP ×1; selector ×1; pots ×3; Sharp ×2; jacks IN/OUT/pedal ×5 |
| JST PH 4 vías vertical B4B-PH-K-S(LF)(SN) | C131334 | 2 | PCB-M: jog y M4 |
| JST PH 6 vías vertical B6B-PH-K-S(LF)(SN) | C131342 | 2 | PCB-K izq. ↔ der. |
| JST PH 10 vías vertical B10B-PH-K-S(LF)(SN) | C158038 | 2 | PCB-M ↔ PCB-K izq. |
| JST SH 4 vías SMD horizontal SM04B-SRSS-TB(LF)(SN) | C160404 | 1 | Solo PCB-A J2 a mano; el de PCB-M va en ensamble JLC |
| JST SH 6 vías SMD horizontal SM06B-SRSS-TB(LF)(SN) | C160405 | 2 | PCB-A J3 ↔ PCB-F J1, a mano |
| Header hembra 1 × 24, 2,54 mm, aislamiento 8,5 mm; Hong Cheng HC-PM254-8.5H-1x24PZ | C41376163 | 2 | PCB-M J_T41L1/J_T41R1, THT a mano |
| Header macho 2 × 4, 2,54 mm recto; CJT A2541WV-2x4P | C225519 | 1 | PCB-M SW_PAD1, THT a mano; interfaz al selector de panel |

**C_CV_FILTER1:** FOSAN FVH016ADA470M0554, **C7469972**, 47 µF 16 V ±20 %, SMD
5×5,4 mm. Va en `bom-pcb-m.csv` para ensamble JLC; **no va en pedido-lcsc-v1.csv**.
Los tres códigos recién aprobados tienen fecha 2026-11-12 y fuente «buscador LCSC» en el catálogo;
la disponibilidad comunicada corresponde a esa verificación, no a una reserva.

## 4. Cables

Sin crimpadora, comprar **cables ya armados** (AliExpress) en vez de carcasas y terminales sueltos:

| Cable | Cant. |
|---|---:|
| JST PH 2.0, 2 vías, doble punta, ~20 cm | 15 |
| JST PH 2.0, 3 vías, doble punta (o una punta para jacks/pots) | 12 |
| JST PH 2.0, 4 / 6 / 10 vías, doble punta | 2 / 1 / 1 |
| JST SH 1.0 (Qwiic), 4 y 6 vías, doble punta | 1 y 1 |
| Cable plano IDC 30 hilos con 2 conectores hembra 2 × 15, ~20 cm | 1 |

Con crimpadora: terminales PH C5292557 (≈ 0,005 USD) y carcasas PHR-n, a buscar en LCSC.

## 5. Fuera de LCSC/JLC

- **Compra local declarada (`Supply=local`):** caja IDC 2×15 2,54 mm recta (box header) con muesca,
  **2 unidades**: PCB-M J1_PCBA1 y PCB-A J1. Sin código LCSC verificable; no incluir en CSV LCSC.
- **Suministradas por Juan (`Supply=owner`):** SW1–SW26, teclas Kailh Choc V2 PG1353 con
  zócalos Kailh CPG135001S30 comprados aparte por AliExpress (26 conjuntos). No volver a pedir
  teclas ni zócalos en este CSV. El cupón mecánico de la huella sigue pendiente.
- **Suministrados por Juan (`Supply=owner`):** PCB-A RE1–RE8, 8 encoders EC11 con pulsador ya disponibles.
- Jacks de pared: **en el pedido LCSC** (2026-11-13): 7 × HOOYA PJ-612 6,35 mm C309284 y 3 × CAX PJ-392-AU 3,5 mm C49274302. Verificar en la hoja la rosca del 6,35 contra el agujero Ø9,5 del cuerpo.
  (CV1, CV2; MIDI sin poblar, va por USB) y audífonos 6,35 mm.
- Juan también tiene el encoder de jog, faders RS6011 ×4, FSR 402 ×6, Teensy 4.1 y Audio Shield.
  D43: FSR por soldadura directa de pines metálicos, sin sockets. Los sockets hembra 1×2 P2,54 alternativos son compra externa sin LCSC verificado, no poblados ni incluidos en este pedido.

## Estimado histórico (opción B, pendiente de recotizar)

No es una cotización actualizada del CSV corregido. Precios nuevos sin verificar.

Placas ~61 + ensamble de PCB-M ~47 (preparación + extendidas + piezas) + chips sueltos ~14 + conectores ~20 ≈
**140 USD**, más envío (~30–40) e IVA/arancel según DIAN (sin verificar).

D23 (Juan, 2026-11-12): sustituye ambos headers de 8 vías por **C158038**, 10 vías PH vertical; mazo con dos carcasas PHR-10, no PHR-8. Ver doc28 §5 para orden 1:1. Identidad LCSC y pinout verificados el 2026-11-12.
