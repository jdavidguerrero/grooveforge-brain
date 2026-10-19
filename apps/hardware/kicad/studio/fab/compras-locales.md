# Studio · compras locales (prototipo de 2 placas)

> Todo lo que no ensambla JLC se compra en tiendas de electrónica de Bogotá. Reemplaza a
> `lcsc-piezas-sueltas.csv` (que queda solo como referencia de part numbers). Lo que dice **verificar** define
> si la pieza entra en la placa o en la caja: llevar la placa (o un calibrador) a la tienda.

## Conectores de placa

| Cant. | Pieza (nombre en tienda) | Verificar | Va en |
|---|---|---|---|
| 2 | Conector **IDC macho 30 pines** (2×15) con carcasa ("box header"), recto | paso 2.54 mm | J5 portadora, J2 PCB-A |
| 2 | Conector **IDC hembra 30 pines** para cable plano | paso 2.54 | cable J5 ↔ J2 |
| 1 | Conector **IDC macho 10 pines** (2×5) con carcasa, recto | paso 2.54 | J6 portadora |
| 1 | Conector **IDC hembra 10 pines** para cable plano | paso 2.54 | cable J6, lado portadora |
| 1 | **Carcasa Dupont 2×5** (hembra) + 10 terminales Dupont hembra | paso 2.54; ver nota del 5 V de la Pi | cable J6, **lado Pi** (pines 1–10). Un IDC (~15 mm) tapa los pines 11–12 del GPIO |
| 1 m | **Cable plano** (bus de datos) gris de 40 hilos | paso 1.27 mm | se separa en 30 y 10 hilos |
| 1 | **JST-XH 2 pines** macho de placa, recto, con hembra y terminales | paso **2.5 mm** | J10 (entrada 5 V) |
| 2 | **Espadín hembra 1×40** (regleta hembra) | paso 2.54, **altura 8.5 mm** | zócalos de la Teensy (2 × 24) |
| 2 | **Espadín hembra 1×40** o 1×14 | paso 2.54, altura 8.5 mm | zócalos del shield (2 × 14) |
| 1 | **Espadín macho recto 1×40** | paso 2.54 | J3 (1×5), J9 (1×9), SW1 (2 tiras de 4) |
| 1 | **Espadín macho a 90° 1×40** | paso 2.54 | J1 de PCB-A (1×10) y LINE del shield |
| 20 | **Cables Dupont hembra-hembra** de 10 cm | | Choc ↔ J1, J9 ↔ shield |

La altura de 8.5 mm de los zócalos es la que se usó para validar la caja (holgura con la Pi). Si solo hay más
altos, avisar antes de soldar.

## Pared trasera (de chasis)

| Cant. | Pieza | Verificar | Va en |
|---|---|---|---|
| 4 | **Jack 1/4" (6.35 mm) estéreo TRS de chasis**, con tuerca | rosca (agujero de Ø9.6 previsto), cuerpo ≤ 16 × 22 × 28 mm (alto ≤ 22: con z 25 queda a 4.5 mm de la portadora) | IN L/R, OUT L/R |
| 1 | **Jack 3.5 mm estéreo de chasis de cuerpo PLÁSTICO (aislado)**, con tuerca | rosca (Ø6.2 previsto), cuerpo ≤ 18 mm; que el sleeve no toque la tuerca ni el panel | PHONES: el sleeve va a HP_VGND (~1.5 V), **nunca a GND ni al aluminio** |
| 1 | **Jack 3.5 mm estéreo de chasis**, con tuerca | rosca (Ø6.2 previsto), cuerpo ≤ 18 mm | CV OUT |
| 1 | **Interruptor de palanca 3 posiciones** (ON-ON-ON o ON-OFF-ON según el cableado del pad) | rosca (Ø6.2 previsto) | INPUT PAD |
| 2 | **Prolongador USB-C de panel** (hembra de panel a cable) | uno a micro-USB macho (Teensy); el otro solo para 5 V | 5V IN, USB |
| 1 | **Prolongador USB-A hembra de panel** o USB-A hembra para soldar | | HOST (5 hilos al header de host de la Teensy) |
| 1 m | **Cable apantallado** de audio (estéreo, delgado) | | jacks ↔ pads de la portadora (5–10 cm cada uno) |

Con las medidas reales de jacks y conectores se ajustan los agujeros del cuerpo antes de imprimir.

## PCB-A (sin ensamble)

| Cant. | Pieza | Verificar | Nota |
|---|---|---|---|
| 2 | **Potenciómetro 9 mm vertical B10K** (tipo RV09, lineal) | 3 pines a 2.5 mm + 2 patas; eje estriado Ø6, ~25 mm desde la base | IN y VOL. Si el eje es más corto, el knob queda bajo |
| 15 | Condensador cerámico **10 nF 0603** | | antirrebote de los encoders. **Opcional**: el firmware puede filtrar el rebote |
| 2 | Condensador **100 nF 0603** | | filtro de los pots. Opcional |
| 1 | Condensador **10 µF 0805** | | desacople de 3V3 en PCB-A. Recomendado |

Si no se consiguen los SMD 0603, PCB-A funciona sin ellos.

## Cable PCB-K → ya no aplica

Sin PCB-K no hace falta el JST-SH. Los Choc se cablean con Dupont a J1 de PCB-A.

## 5 V de la Pi por el IDC de 10

Los IDC hembra genéricos suelen ser de **1 A por contacto**, y la Pi toma el 5 V por dos (pines 2 y 4): ~2 A.
Una Pi 3B con la pantalla consume ~0.8–1.2 A en uso normal, así que alcanza. Si hay bajones
(`vcgencmd get_throttled` ≠ 0x0), soldar dos hilos extra de 5 V y GND en paralelo al cable plano.

## Armado: notas de la auditoría (2026-10-18)

- **Panel de aluminio:** los 10 agujeros de los espárragos **PEM FH-M3** son de **Ø3.0** (`panel-aluminio.dxf` ya
  corregido). Pedir FH-M3-12 para la pantalla y FH-M3 del largo del separador para PCB-A.
- **Choc:** los hilos de J1 se **sueldan** a las patas del switch; un Dupont enchufado ahí cuelga ~14 mm y toca PCB-A.
- **Pots RV09:** medir antes de cortar el aluminio que el eje quede a 7.5 mm de la fila de pines (footprint de PCB-A).
- **Jacks de chasis:** cablear según la tabla de theory/02 §10I (las etiquetas del panel se leen L → R desde atrás).
