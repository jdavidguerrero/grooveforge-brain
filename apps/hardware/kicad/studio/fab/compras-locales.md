# Studio · compras locales (prototipo de 3 placas: portadora, PCB-A y PCB-K — ADR-004 rev. 4)

> Todo lo que no ensambla JLC se compra en tiendas de electrónica de Bogotá. Reemplaza a
> `lcsc-piezas-sueltas.csv` (que queda solo como referencia de part numbers). Lo que dice **verificar** define
> si la pieza entra en la placa o en la caja: llevar la placa (o un calibrador) a la tienda.

## Conectores de placa

| Cant. | Pieza (nombre en tienda) | Verificar | Va en |
|---|---|---|---|
| 2 | Conector **IDC macho 30 pines** (2×15) con carcasa ("box header"), recto | paso 2.54 mm | J5 portadora, J2 PCB-A |
| 2 | Conector **IDC hembra 30 pines** para cable plano | paso 2.54 | cable J5 ↔ J2 |
| 1 | **Carcasa Dupont 2×5** (hembra) + 10 terminales Dupont hembra | paso 2.54 | cable de la Pi, **lado Pi** (pines 1–10). Un IDC (~15 mm) tapa los pines 11–12 del GPIO |
| 1 m | **Cable AWG22** (silicona o hilo multifilar) rojo, negro y otro color | | cable de la Pi soldado en J6 (theory/02 §10J F3): 2 × 5 V, 2 × GND, TX, RX. **No** IDC de 1 A |
| 1 m | **Cable AWG20** rojo y negro | | J10: soldado directo en la placa, desde el módulo USB-C (sin JST-XH genérico) |
| 1 m | **Cable plano** (bus de datos) gris de 30 hilos o más | paso 1.27 mm | cable J5 ↔ J2 |
| 2 | **Espadín hembra 1×40** (regleta hembra) | paso 2.54, **altura 8.5 mm** | zócalos de la Teensy (2 × 24) |
| 2 | **Espadín hembra 1×40** o 1×14 | paso 2.54, altura 8.5 mm | zócalos del shield (2 × 14) |
| 1 | **Espadín macho recto 1×40** | paso 2.54 | J3 (1×5), J9 (1×9), SW1 (2 tiras de 4) |
| 1 | **Espadín macho a 90° 1×40** | paso 2.54 | J1 de PCB-A (1×10) y LINE del shield |
| 10 | **Cables Dupont hembra-hembra** de 10 cm | | J9 ↔ shield |
| 1 | **Kit de hilos JST-SH 1.0 mm precrimpados** (10 hilos, ~10 cm) + carcasa JST-SH 10 | paso **1.0 mm** | cable PCB-K J1 (JST-SH, ensamblado) ↔ PCB-A J1 (header 1×10): JST-SH de un lado, terminal Dupont del otro, pinout 1:1 |
| 1 | **Terminales de crimpar con ficha** (corriente por contacto documentada) para el header de la Pi, y su carcasa 2×5 | paso 2.54; ficha con A por contacto | cable de la Pi, lado Pi (theory/02 §10K D09). No "Dupont" sin marca |
| 1 | **Bases adhesivas para precinto** o puntos de amarre + precintos | | amarre de los mazos de J10, J6 y audio cerca de cada salida de PCB (§10K D09) |

La altura de 8.5 mm de los zócalos es la que se usó para validar la caja (holgura con la Pi). Si solo hay más
altos, avisar antes de soldar.

## Pared trasera (de chasis)

| Cant. | Pieza | Verificar | Va en |
|---|---|---|---|
| 4 | **Jack 1/4" (6.35 mm) estéreo TRS de chasis**, con tuerca | rosca (agujero de Ø9.6 previsto), cuerpo ≤ 16 × 22 × 28 mm (alto ≤ 22: con z 25 queda a 4.5 mm de la portadora) | IN L/R, OUT L/R |
| 1 | **Jack 3.5 mm estéreo de chasis de cuerpo PLÁSTICO (aislado)**, con tuerca | rosca (Ø6.2 previsto), cuerpo ≤ 18 mm; que el sleeve no toque la tuerca ni el panel | PHONES: el sleeve va a HP_VGND (~1.5 V), **nunca a GND ni al aluminio** |
| 1 | **Jack 3.5 mm estéreo de chasis**, con tuerca | rosca (Ø6.2 previsto), cuerpo ≤ 18 mm | CV OUT |
| 1 | **Selector de 2 polos y 3 vías** para el PAD: rotativo 2 polos × 6 posiciones con anillo de tope en 3 (agujero M9: el panel pasa de Ø6.2 a Ø9.6), o palanca **DP3T real de 8 terminales**. **No** un DPDT ON-OFF-ON ni ON-ON-ON de 6 terminales | con continuidad, la tabla de theory/02 §10J F4 (L: 3↔1 / 3↔2 / 3↔4 · R: 7↔5 / 7↔6 / 7↔8) | INPUT PAD (SW1) |
| 1 | **Módulo consumidor USB-C de 5 V** ("USB-C power breakout" / "Type-C 5V sink") con **5.1 kΩ de CC1 a GND y de CC2 a GND** y salida VBUS/GND. **No** un prolongador pasivo ni un disparador PD de 9/12 V | que tenga las dos resistencias de 5.1 kΩ; ventana del panel 9.4 × 3.6 | 5V IN → J10 (theory/02 §10J F2) |
| 1 | **Prolongador USB-C de panel** (hembra de panel) a micro-USB macho | | USB de datos de la Teensy |
| 1 | **Prolongador USB-A hembra de panel** o USB-A hembra para soldar | | HOST (5 hilos al header de host de la Teensy) |
| 1 m | **Cable apantallado** de audio (estéreo, delgado) | | jacks ↔ pads de la portadora (5–10 cm cada uno) |

Con las medidas reales de jacks y conectores se ajustan los agujeros del cuerpo antes de imprimir.

## PCB-A (sin ensamble)

| Cant. | Pieza | Verificar | Nota |
|---|---|---|---|
| 2 | **Potenciómetro ALPS RK09L114001T** (vertical, 10 kΩ, eje D, L = 20 mm) — o Alpha RD901F-40; **no** RK09K | que sea **B103 / B10K lineal** (marca del cuerpo); altura total ~25 mm desde la base; rosca y alto del buje | IN y VOL. Knobs impresos con **indicador** y agujero para eje D de 6 mm (theory/02 §5) |
| 15 | Condensador cerámico **10 nF 0603** | | antirrebote de los encoders. **Opcional**: el firmware puede filtrar el rebote |
| 2 | Condensador **100 nF 0603** | | filtro de los pots. Opcional |
| 1 | Condensador **10 µF 0805** | | desacople de 3V3 en PCB-A. Recomendado |

Si no se consiguen los SMD 0603, PCB-A funciona sin ellos.

## PCB-K (vuelve al pedido, 2026-10-22)

JLC ensambla en la cara inferior los 18 LEDs, los condensadores y el JST-SH. A mano:

| Cant. | Pieza | Verificar | Nota |
|---|---|---|---|
| 6 | **Kailh Choc V2** | que se enganchen en el rebaje del aluminio (1.2 mm) | se **sueldan a PCB-K** |
| 4 | **Separadores M3 × 10 mm** hexagonales de **5.5 mm** entre caras (o redondos ≤ Ø6.5) | 6.35 mm entre vértices: los agujeros de PCB-K son Ø7.0 | de los espárragos PEM del aluminio a PCB-A, pasan a través de PCB-K |

Ya no hacen falta el anillo WS2812 suelto, los WS2812 de tecla ni los soportes de tecla impresos (§10G).

## 5 V de la Pi (corregido 2026-10-22, theory/02 §10J F3)

~~IDC genérico de 1 A por contacto~~: no alcanza para los picos de la Pi + pantalla (hasta ~2 A). El cable de la Pi
va con **hilos AWG22 soldados en J6** (2 × 5 V, 2 × GND, TX, RX) y carcasa Dupont del lado de la Pi. Prueba:
5 V ≥ 4.85 V en el pin 2 de la Pi con todo encendido y `vcgencmd get_throttled` = `0x0`.

## Armado: notas de la auditoría (2026-10-18)

- **Panel de aluminio:** los 10 agujeros de los espárragos **PEM FH-M3** son de **Ø3.0** (`panel-aluminio.dxf` ya
  corregido). Pedir FH-M3-12 para la pantalla y FH-M3 del largo del separador para PCB-A.
- ~~**Choc:** hilos soldados a las patas~~: con PCB-K los Choc se sueldan a la placa (ADR-004 rev. 4).
- **Pots RV09:** medir antes de cortar el aluminio que el eje quede a 7.5 mm de la fila de pines (footprint de PCB-A).
- **Jacks de chasis:** cablear según la tabla de theory/02 §10I (las etiquetas del panel se leen L → R desde atrás).
