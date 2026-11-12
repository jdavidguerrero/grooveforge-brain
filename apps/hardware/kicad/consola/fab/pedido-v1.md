# Pedido v1 de la consola — JLC + LCSC (borrador)

2026-11-12. **Opción B** (decisión de Juan): JLC fabrica las 6 placas y **ensambla solo PCB-M** (SMD). PCB-K izq./der.
y PCB-A se sueldan a mano con chips sueltos de LCSC, que viajan en el mismo envío que las placas de JLC (carrito
combinado; elegir "ship together" al pagar).

> **Borrador.** Las cantidades salen de las specs (`docs/consola/24`, `28`) y del README de PCB-M. Se cierran con la
> lista de piezas exportada de los esquemas finales (PCB-M con D8–D15, PCB-K/F/P en curso). No pedir antes de esa
> revisión. Precios LCSC del 2026-11-12, sin envío.

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

Pasivos de PCB-A: según su esquema (100 nF, 10 nF, 10 µF en 0603). Se agregan con su lista de piezas.

## 3. LCSC — conectores (lado placa)

| Pieza | LCSC | Cant. | Dónde |
|---|---|---:|---|
| JST PH 2 vías vertical (B2B-PH-K-S) | **buscar** (el original C20504437 está sin stock; vale un clon PH 2.0) | 15 | PCB-M: FSR ×4, EXP ×2, CV ×2, 5V_LED_IN; PCB-P ×6 |
| JST PH 3 vías vertical (B3B-PH-K-S) | C131339 | 14 | PCB-M: Sharp ×2, pots ×3, selector, audífonos, jacks TRS ×7 |
| JST PH 4 vías vertical | C566011 | 2 | PCB-M: jog, M4 |
| JST PH 6 vías vertical | C495261 | 2 | Cable PCB-K izq. ↔ der. |
| JST PH 8 vías vertical | C594302 | 2 | PCB-M ↔ PCB-K izq. |
| JST SH 4 vías SMD vertical | C51940129 | 2 | I2C PCB-M ↔ PCB-A |
| JST SH 6 vías SMD vertical | C160392 | 2 | PCB-A ↔ PCB-F |
| Header hembra 1 × 24, 2,54 mm | C41376163 | 2 | Zócalo de la Teensy en PCB-M |
| Caja IDC 2 × 15, 2,54 mm | **buscar** | 2 | PCB-M J1 y PCB-A J1 |

Cantidades de jacks a confirmar con el esquema de PCB-M (D15: un conector por jack de pared).

## 4. Cables

Sin crimpadora, comprar **cables ya armados** (AliExpress) en vez de carcasas y terminales sueltos:

| Cable | Cant. |
|---|---:|
| JST PH 2.0, 2 vías, doble punta, ~20 cm | 15 |
| JST PH 2.0, 3 vías, doble punta (o una punta para jacks/pots) | 14 |
| JST PH 2.0, 4 / 6 / 8 vías, doble punta | 2 / 1 / 1 |
| JST SH 1.0 (Qwiic), 4 y 6 vías, doble punta | 1 y 1 |
| Cable plano IDC 30 hilos con 2 conectores hembra 2 × 15, ~20 cm | 1 |

Con crimpadora: terminales PH C5292557 (≈ 0,005 USD) y carcasas PHR-n, a buscar en LCSC.

## 5. Fuera de LCSC/JLC

- Zócalos Kailh Choc CPG135001S30: 2 paquetes de 20 (AliExpress; en LCSC sin stock).
- ~8 teclas Kailh Choc V2 Brown.
- Jacks de pared (Bogotá): 6,35 mm TRS ×5 (LINE IN L/R, LINE OUT L/R, PEDAL), 3,5 mm ×4 (CV1, CV2, MIDI IN, MIDI OUT) y audífonos 6,35 mm.
- Ya los tiene Juan: encoders EC11 ×9, faders RS6011 ×4, FSR 402 ×6, Teensy 4.1 y Audio Shield.

## Estimado (opción B)

Placas ~61 + ensamble de PCB-M ~47 (preparación + extendidas + piezas) + chips sueltos ~14 + conectores ~20 ≈
**140 USD**, más envío (~30–40) e IVA/arancel según DIAN (sin verificar).
