# 28 · PCB-K — placa de teclas (spec)

2026-11-12. Spec antes del esquema: de aquí sale la descripción que el generador de esquemas (`kicad/consola/tools/
schgen`, en curso) convierte en KiCad. Reemplaza a PCB-K1/K2 de la GroovePush (`24` §9).

Fuentes: posiciones de `panel-coordinates-console-v1.csv`; contorno y agujeros de `pcb-outlines-console-v5.csv`;
pines de MCP y notas de [18](18-teensy-unica-firmware.md) y [17 §3](17-mapa-pines-teensy.md); cadena de LEDs de
[27 §1](27-leds-teclas.md); conector de la hoja 08 de PCB-M; alturas de [25 §7](25-panel-consola.md) y del diseño v5.

## 1. Qué lleva

| Bloque | Cantidad | Nota |
|---|---|---|
| Teclas Kailh Choc V2 Brown (táctil 50 gf, 4 patas, ventana para LED SMD) | 26 | Una entrada de MCP por tecla, a GND al pulsar; sin matriz ni diodos |
| LED SK6812MINI-EA (montaje inverso; LCSC C5378731, ensamble manual opción B) | 26 | Uno bajo cada tecla, cadena única, datos a 5 V ya bufferados en PCB-M |
| MCP23017 | 3 | 0x21, 0x22, 0x23 (A2 A1 A0 = 001, 010, 011) |
| Conector a PCB-M | 1 | 8 vías, pinout de la hoja 08 de PCB-M (§5) |

2 capas, 1,6 mm, máscara negra. Sin componentes que lean el bus distinto a los MCP.

## 2. Mecánica

- **Contorno:** los 18 vértices de `pcb-outlines-console-v5.csv`. El CSV se corrigió el 2026-11-12 con los ajustes
  que el diseño v5 aceptado ya traía y que no habían salido en el export: escalón de los vértices 13–14 en **y 221**,
  agujero de (10, 147.5) movido a **(30, 144)** (quedaba a 2 mm de BUILD) y las 11 teclas de motor, sección y
  navegación en sus posiciones v1.
- **Altura:** cara superior de PCB-K a **−3,4 mm** de la cara superior del aluminio, o sea **1,4 mm de luz** bajo la
  chapa. **Ningún componente en la cara superior** salvo las teclas (los LED van de montaje inverso, desde abajo).
- **Cara inferior con zonas prohibidas:** PCB-P (x 9–232, y 69–100) queda 1,5 mm por debajo; la M4 ocupa la ventana
  (x 109–231, y 107–169) y su placa cuelga debajo (margen de 5 mm alrededor de la ventana). Sobre la bahía de la Pi
  (x 9–94, y 219–275) el apilado llega a z ≈ 66 (estimado, el HAT no ha llegado): cualquier pieza inferior ahí ≤ 6 mm.
- **Montaje:** 14 agujeros M2.5 del CSV v5 a espárragos PEM FH-M2.5 de la chapa, con separador de 1,4 mm. Los agujeros
  de PCB-P (y 72 y 97) que caen dentro de PCB-K pasan a través.
- **Ventanas:** M4 122 × 62 y 4 redondas Ø20 bajo los FSR de velocity (del CSV v5).

### Posiciones v1 (resuelto)

El CSV de contornos descargado traía motor en y 182, sección en y 163 y navegación en y 236–310, pero el diseño v5
aceptado ya las había movido a y 174, 155 y 231–305 a pedido de Juan («TAP pasa de 5,3 a 8,0 mm de la perilla E1»).
Valen las v1, que son además las que corta la chapa.

## 3. Teclas

- Huella Choc con **zócalo hot-swap Kailh CPG135001S30** (LCSC C5333465, ~0,16 USD c/u, sin stock el 2026-11-12;
  también se vende por bolsas). Se compran aparte de las teclas: 26 + repuestos. Se sueldan a mano (SMD de 2 pads, pedido-v1 opción B; JLC ensambla solo PCB-M). Confirmar con la hoja de la Choc V2 que usa el mismo zócalo. Alternativa: teclas soldadas.
  **D-K1: zócalos (Juan, 2026-11-12).** Huella base: `Kailh_Choc_V2` de la librería siderakb/key-switches.pretty
  (poste central más grande que la V1, sin los postes laterales de la V1), a confirmar con la medición de §3.
- Un polo a la entrada del MCP, el otro a GND. Pull-ups internos del MCP (los activa el firmware); rebote en firmware.
  Las teclas de transporte llevan keycap 1 mm más bajo: no cambia la PCB.
- **Tecla: Kailh Choc V2 PG1353** (hoja CPG135301D02; la de Juan es la Brown, mismo cuerpo). Huella en vista superior,
  mm desde el centro, +y hacia atrás (la hoja la dibuja girada 90°):

  | Elemento | Posición | Agujero |
  |---|---|---|
  | Poste central | 0, 0 | Ø5,0 |
  | Pata eléctrica 1 | 0, −5,90 | **Ø1,3 PTH**, pad Ø1,9 + pad SMD del zócalo |
  | Pata eléctrica 2 | 5,00, −3,80 | **Ø1,3 PTH**, pad Ø1,9 + pad SMD del zócalo |
  | Poste de plástico | −5,00, 5,15 | Ø1,5 |
  | Ventana del LED | 0, 4,93 | corte para SK6812MINI-EA |

  Las patas eléctricas están donde las de la Choc V1, por eso sirve el zócalo **CPG135001S30**; el poste central es
  más grande (Ø5,0 contra Ø3,4 de la V1).
- **Huella doble (Juan, 2026-11-12):** cada pata eléctrica tiene taladro metalizado Ø1,3 con anillo
  de 0,30 mm (pad Ø1,9) y comparte número/red 1 o 2 con su pad SMD inferior del zócalo.
  Se puede soldar la tecla directamente si no llegan los zócalos. Los pads SMD provisionales
  de 2,6 × 2,6 quedan en x = −3,275 y 8,275; distancia al taladro correspondiente 3,275 mm:
  separación cobre-cobre = 3,275 − 1,30 − 0,95 = **1,025 mm**, sin choque (mínimo 0,2).
  Anillo 0,30 ≥ 0,15 mm. Los pares cruzados tienen más separación. En layout hay que rutear
  las dos terminaciones de cada red; compartir número no las conecta físicamente.
  No se cambian los postes **NPTH** Ø5,0 y Ø1,5 ni el centro de recorte LED (x = 0; y = +4,93).
  Los pads del zócalo siguen provisionales: verificar pieza/cupón, incluido que su plástico
  admita estos taladros de 1,3 (antes eran aperturas NPTH de 3); no se certifica aquí ese ajuste.

- **Alturas:** la carcasa inferior (13,95 × 13,95) mide 3,30 sobre la PCB; la superior (15 × 15) se apoya encima. Con
  PCB-K a −3,4 la cara del aluminio queda donde iría la placa de montaje estándar: **la tecla apoya su carcasa
  superior en la chapa**, como en un teclado. Las aletas de clip quedan dentro del espesor de 2 mm.
- **Corte de la chapa:** con zócalos conviene que la chapa sostenga la tecla (si no, se sale al tirar del keycap).
  Primero probar en el cupón el corte de **14,0** con una tecla montada: las aletas se comprimen en los 2 mm y la
  tecla queda firme. Si no entra o queda forzada, 14,6. El 15,4 solo si nada de lo anterior sirve (la tecla quedaría
  sostenida solo por el zócalo).

## 4. Entradas de los MCP

Igual que el firmware actual (`ConsolaMap.h`, `18`): **no hay que tocar código**. GPIO 0–6 = GPA0–6, 8–14 = GPB0–6;
GPA7/GPB7 no se usan (errata de solo salida). Índice LED = orden de la cadena de `27` §1.

| MCP | Pin | Tecla | x, y | Nota | LED |
|---|---|---|---|---:|---:|
| 0x21 | GPA0 | SCENE1_OCT+ | 242, 160.5 | 89 | 25 |
| 0x21 | GPA1 | SCENE2_OCT− | 242, 145.5 | 90 | 24 |
| 0x21 | GPA2 | SCENE4_PAGE_R | 242, 115.5 | 91 | 22 |
| 0x21 | GPA3 | SCENE3_PAGE_L | 242, 130.5 | 92 | 23 |
| 0x21 | GPB0 | METRO | 208, 39.5 | 101 | 21 |
| 0x21 | GPB1 | LOOP | 189, 39.5 | 97 | 20 |
| 0x21 | GPB2 | REC | 170, 39.5 | 96 | 19 |
| 0x21 | GPB3 | STOP | 151, 39.5 | 95 | 18 |
| 0x21 | GPB4 | PLAY | 132, 39.5 | 94 | 17 |
| 0x22 | GPA0 | UNDO | 39, 132.5 | 98 | 12 |
| 0x22 | GPA1 | BACK | 64, 305 | 102 | 0 |
| 0x22 | GPA2 | MENU | 64, 286.5 | 103 | 1 |
| 0x22 | GPA3 | NAV_L | 64, 268 | 104 | 2 |
| 0x22 | GPA4 | NAV_R | 64, 249.5 | 105 | 3 |
| 0x22 | GPA5 | BROWSE | 64, 231 | 106 | 4 |
| 0x22 | GPA6 | SHIFT | 20, 132.5 | 100 | 11 |
| 0x22 | GPB0 | DELETE | 98, 160.5 | 107 | 13 |
| 0x22 | GPB1 | DUPLICATE | 98, 145.5 | 110 | 14 |
| 0x22 | GPB2 | QUANTIZE | 98, 130.5 | 111 | 15 |
| 0x22 | GPB3 | NEW | 98, 115.5 | 99 | 16 |
| 0x23 | GPA0 | CUE_ENG | 20, 174 | 112 | 7 |
| 0x23 | GPA1 | FILL | 39, 174 | 113 | 6 |
| 0x23 | GPA2 | TAP | 58, 174 | 114 | 5 |
| 0x23 | GPA3 | BUILD | 20, 155 | 115 | 8 |
| 0x23 | GPA4 | DROP | 39, 155 | 116 | 9 |
| 0x23 | GPA5 | BREAK | 58, 155 | 117 | 10 |
| 0x23 | GPA6 | JOG_CLICK (J1.9, desde PCB-M) | — | 88 | — |
| 0x23 | GPB0 | PEDAL_BTN (J1.10, desde PCB-M) | — | 122 | — |

36 de 42 entradas en uso. Las libres (0x21 GPA4–6 y GPB5–6; 0x22 GPB4–6; 0x23 GPB1–6) van a pads de prueba para
ampliar sin otra placa.

**Ubicación de los MCP (cara inferior, cerca de sus teclas):** 0x21 hacia (215, 58), entre el transporte y PCB-P;
0x22 hacia (78, 150), entre la columna de edición y SHIFT/UNDO; 0x23 hacia (30, 200), bajo motor/sección. A ajustar
en el layout. Cada uno: 100 nF + 1 µF a +3V3, RESET a +3V3 con 10 kΩ, INTA/INTB a pads de prueba (el firmware sondea
por I2C, no usa interrupciones).

## 5. Conector J1 ↔ PCB-M

JST-PH 2,0 mm de 10 vías, vertical, cara inferior, zona x 15–45 / y 230–290 (a la izquierda de la columna de
navegación, sobre la bahía de la Pi: el conector mide 6 mm, cabe en la luz de §2 con poco margen; si el HAT sale más
alto, versión en ángulo recto). PH en vez de SH: 2 A por vía para los LEDs.

| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|
| +3V3 | SDA | GND | SCL | GND | LED_DIN_5V | GND | +5V_LED | JOG_CLICK | PEDAL_BTN |

**D23 (Juan, 2026-11-12):** ambos headers B10B-PH-K-S(LF)(SN), [LCSC C158038](https://www.lcsc.com/product-detail/C158038.html), huella 1×10 PH vertical, ancho 21,9 mm. Cable PHR-10 ↔ PHR-10 1:1.

- **I2C sin pull-ups en PCB-K** (huellas 2,2 kΩ sin poblar): el par activo está en el segmento B_K de PCB-M tras PCA9517A, con 2,2 kΩ por defecto o 4,7 kΩ seleccionables (doc24 D18/D21). El cable K izquierda–derecha sigue 6 vías, en el mismo dominio B_K.
- D29: junto al jack en PCB-M: 1 kΩ serie, ESD 3,3 V a GND, pull-up 10 kΩ a 3,3 V y 10 nF a GND antes de J_PCB_K. PCB-K recibe PEDAL_BTN protegido; no repite protección.

## 6. LEDs

- 26 × SK6812MINI-EA (LCSC C5378731), cadena única en el orden de `27` §1, para las dos placas de §7: J1.6 → BACK (LED0) →
  MENU → NAV_L → NAV_R → BROWSE → TAP → FILL → CUE_ENG → BUILD → DROP → BREAK → SHIFT → UNDO →
  DELETE → DUPLICATE → QUANTIZE → NEW (LED16), todo en la izquierda. DOUT de NEW → único hilo LED_DATA del
  cable → DIN de PLAY (LED17) en la frente-derecha → STOP → REC → LOOP → METRO → SCENE4_PAGE_R →
  SCENE3_PAGE_L → SCENE2_OCT− → SCENE1_OCT+ (LED25). No cruza la franja sobre la ventana de la M4.
- Datos: entrada desde J1.6 con resistencia serie de 33 Ω (huella 0805, puente si no hace falta).
- Cada LED con 100 nF a GND; 22 µF a la entrada de +5V_LED. +5 V y GND en pista ancha (≥ 0,8 mm) alrededor del anillo
  que forma la placa; GND con relleno en la cara inferior.
- Corriente: el firmware limita a 500 mA (`27` §4). El polyfuse de 0,75 A de la rama está en PCB-M (`24` §5).

**Ensamble manual:** 100 nF C49678, 1 µF C5137478, 10 kΩ C17414 y 33 Ω C17634
usan 0805; 22 µF C12891 usa **1206**. MCP23017-E/SS C506653 usa SSOP-28
5,3 × 10,2 mm, paso 0,65 (no SOIC). R2/R3 2,2 kΩ 0805 C17520 siguen DNP.
LED: [hoja LCSC C5378731](https://www.lcsc.com/datasheet/C5378731.pdf), OPSCO B/2
(2026-07-23), pp.4–5: 3,2 × 2,8 × 1,78 mm; **1 GND, 2 DIN, 3 VDD, 4 DOUT**.
Compatible con el símbolo eléctrico MINI-E y la geometría inversa KiCad de paso 1,5 mm;
la huella local se llama `SK6812MINI-EA_ReverseMount`. Se conserva el recorte de Dn.

## 7. Costo: una placa o dos

Cotización JLC del 2026-11-12 (calculadora pública; 2 capas, 1,6 mm, verde, 5 unidades, 1 diseño, sin envío ni
ensamble; el negro puede cambiar ambos precios por igual):

| Opción | Envolvente | Precio |
|---|---|---:|
| Una placa | 246 × 292 | 56,18 USD |
| Dos placas: izquierda (edición, SHIFT/UNDO, motor, sección, navegación, J1) | 102 × 216 | 14,70 USD |
| ″ frente-derecha (transporte y escenas) | 144 × 152 | 14,60 USD |
| **Dos placas, total** | | **29,30 USD** |

Partida: ahorra ~27 USD, se elimina la franja de 4 mm sobre la ventana de la M4 y se suma un cable corto de 6 vías
(+3V3, GND, SDA, SCL, +5V_LED, LED_DATA) entre la esquina (108, 104) de ambas. 0x21 va a la derecha; 0x22, 0x23 y J1 a
la izquierda. La cadena de LEDs se reordena para empezar en la izquierda y terminar en la derecha (un solo hilo de
datos en el cable); eso cambia la tabla de índices de `27` §1 y del firmware (cambio pequeño). Si JLC ensambla, son
dos cargos de preparación en vez de uno: cotizarlo antes de elegir ensamble.

## 8. Verificación

- Esquema: netlist del generador contra la tabla de §4 y §5 (cada tecla en su pin, nada fusionado); ERC limpio.
- Layout: DRC limpio; huellas contra el CSV v1 (centro de tecla ± 0,1 mm); chequeo 3D en FreeCAD con la placa
  exportada (luz de 1,4 mm a la chapa, nada contra PCB-P ni la M4).
- Banco: `i2cdetect` ve 0x21–0x23; las 26 teclas generan su nota de §4; `@GF KEYLEDS 26` recorre la cadena completa.

## Decisiones pendientes de Juan

| | Pregunta | Recomendación |
|---|---|---|
| D-K1 | ~~Zócalo o soldadas~~ | Resuelto: zócalos |
| D-K2 | ~~Una o dos placas~~ | Resuelto: dos (§7) |
| D-K4 | ~~Modelo de tecla~~ | Resuelto: PG1353 (hoja enviada por Juan) |
| D-K3 | ~~Posiciones~~ | Resuelto: v1 (§2) |
