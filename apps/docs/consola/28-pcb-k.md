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
| LED SK6812MINI-E (montaje inverso) | 26 | Uno bajo cada tecla, cadena única, datos a 5 V ya bufferados en PCB-M |
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
  también se vende por bolsas). Se compran aparte de las teclas: 26 + repuestos. Se sueldan a mano (SMD de 2 pads) o
  los pone JLC. Confirmar con la hoja de la Choc V2 que usa el mismo zócalo. Alternativa: teclas soldadas.
  **D-K1: zócalos (Juan, 2026-11-12).** Huella base: `Kailh_Choc_V2` de la librería siderakb/key-switches.pretty
  (poste central más grande que la V1, sin los postes laterales de la V1), a confirmar con la medición de §3.
- Un polo a la entrada del MCP, el otro a GND. Pull-ups internos del MCP (los activa el firmware); rebote en firmware.
  Las teclas de transporte llevan keycap 1 mm más bajo: no cambia la PCB.
- Modelo de Juan (a confirmar): Choc V2 Low Profile Brown, 4 patas (2 eléctricas + 2 de plástico), ventana para LED SMD de
  montaje inverso: encaja con el SK6812MINI-E de §6. La huella se dibuja con las patas medidas con calibre en una
  tecla real (centro, patas eléctricas y postes), no con la de la Choc V1.
- **Ojo:** la hoja que encontró Juan es de la **PG1511** (CPG151101S22, perfil bajo con patas en posición MX), no de la
  Choc V2 (PG1353). Son teclas distintas: la PG1511 mide 15,25 mm de alto y su carcasa superior queda ~11 mm sobre la
  PCB, lo que no cabe en la luz de 1,4 mm de §2. Prueba rápida: distancia entre las dos patas metálicas,
  **≈ 5,4 mm → Choc** (V1/V2 comparten esas patas), **≈ 6,8 mm → PG1511/MX**.
- LED en la ventana de la tecla, desplazado hacia atrás (+y) del centro; distancia exacta según la hoja de la Choc V2
  (verificar con una tecla real antes de enviar el layout).

## 4. Entradas de los MCP

Igual que el firmware actual (`ConsolaMap.h`, `18`): **no hay que tocar código**. GPIO 0–6 = GPA0–6, 8–14 = GPB0–6;
GPA7/GPB7 no se usan (errata de solo salida). Índice LED = orden de la cadena de `27` §1.

| MCP | Pin | Tecla | x, y | Nota | LED |
|---|---|---|---|---:|---:|
| 0x21 | GPA0 | SCENE1_OCT+ | 242, 160.5 | 89 | 8 |
| 0x21 | GPA1 | SCENE2_OCT− | 242, 145.5 | 90 | 7 |
| 0x21 | GPA2 | SCENE4_PAGE_R | 242, 115.5 | 91 | 5 |
| 0x21 | GPA3 | SCENE3_PAGE_L | 242, 130.5 | 92 | 6 |
| 0x21 | GPB0 | METRO | 208, 39.5 | 101 | 4 |
| 0x21 | GPB1 | LOOP | 189, 39.5 | 97 | 3 |
| 0x21 | GPB2 | REC | 170, 39.5 | 96 | 2 |
| 0x21 | GPB3 | STOP | 151, 39.5 | 95 | 1 |
| 0x21 | GPB4 | PLAY | 132, 39.5 | 94 | 0 |
| 0x22 | GPA0 | UNDO | 39, 132.5 | 98 | 13 |
| 0x22 | GPA1 | BACK | 64, 305 | 102 | 25 |
| 0x22 | GPA2 | MENU | 64, 286.5 | 103 | 24 |
| 0x22 | GPA3 | NAV_L | 64, 268 | 104 | 23 |
| 0x22 | GPA4 | NAV_R | 64, 249.5 | 105 | 22 |
| 0x22 | GPA5 | BROWSE | 64, 231 | 106 | 21 |
| 0x22 | GPA6 | SHIFT | 20, 132.5 | 100 | 14 |
| 0x22 | GPB0 | DELETE | 98, 160.5 | 107 | 9 |
| 0x22 | GPB1 | DUPLICATE | 98, 145.5 | 110 | 10 |
| 0x22 | GPB2 | QUANTIZE | 98, 130.5 | 111 | 11 |
| 0x22 | GPB3 | NEW | 98, 115.5 | 99 | 12 |
| 0x23 | GPA0 | CUE_ENG | 20, 174 | 112 | 20 |
| 0x23 | GPA1 | FILL | 39, 174 | 113 | 19 |
| 0x23 | GPA2 | TAP | 58, 174 | 114 | 18 |
| 0x23 | GPA3 | BUILD | 20, 155 | 115 | 15 |
| 0x23 | GPA4 | DROP | 39, 155 | 116 | 16 |
| 0x23 | GPA5 | BREAK | 58, 155 | 117 | 17 |
| 0x23 | GPA6 | JOG_CLICK (J1.7, desde PCB-M) | — | 88 | — |
| 0x23 | GPB0 | PEDAL_BTN (J1.8, desde PCB-M) | — | 122 | — |

36 de 42 entradas en uso. Las libres (0x21 GPA4–6 y GPB5–6; 0x22 GPB4–6; 0x23 GPB1–6) van a pads de prueba para
ampliar sin otra placa.

**Ubicación de los MCP (cara inferior, cerca de sus teclas):** 0x21 hacia (215, 58), entre el transporte y PCB-P;
0x22 hacia (78, 150), entre la columna de edición y SHIFT/UNDO; 0x23 hacia (30, 200), bajo motor/sección. A ajustar
en el layout. Cada uno: 100 nF + 1 µF a +3V3, RESET a +3V3 con 10 kΩ, INTA/INTB a pads de prueba (el firmware sondea
por I2C, no usa interrupciones).

## 5. Conector J1 ↔ PCB-M

JST-PH 2,0 mm de 8 vías, vertical, cara inferior, zona x 15–45 / y 230–290 (a la izquierda de la columna de
navegación, sobre la bahía de la Pi: el conector mide 6 mm, cabe en la luz de §2 con poco margen; si el HAT sale más
alto, versión en ángulo recto). PH en vez de SH: 2 A por vía para los LEDs.

| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|
| GND | +3V3 | SDA | SCL | LED_DIN_5V | +5V_LED | JOG_CLICK | PEDAL_BTN |

- **I2C sin pull-ups en PCB-K** (huellas 2,2 kΩ sin poblar): el par activo está en PCB-M (`17` §2: un solo par).
- La protección del pedal (jack externo) va en PCB-M antes del cable; PCB-K no la repite.

## 6. LEDs

- 26 × SK6812MINI-E, cadena única en el orden de `27` §1: transporte → columna de escenas (sube) → cruza por la
  franja de 4 mm entre la ventana de la M4 y el borde (y 169–173) → columna de edición (baja) → UNDO, SHIFT →
  sección → motor → navegación (sube).
- Datos: entrada desde J1.5 con resistencia serie de 33 Ω (huella 0603, puente si no hace falta).
- Cada LED con 100 nF a GND; 22 µF a la entrada de +5V_LED. +5 V y GND en pista ancha (≥ 0,8 mm) alrededor del anillo
  que forma la placa; GND con relleno en la cara inferior.
- Corriente: el firmware limita a 500 mA (`27` §4). El polyfuse de 0,75 A de la rama está en PCB-M (`24` §5).

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
| D-K4 | ¿Las teclas son Choc V2 (PG1353) o PG1511? | Medir las patas (§3) |
| D-K3 | ~~Posiciones~~ | Resuelto: v1 (§2) |
