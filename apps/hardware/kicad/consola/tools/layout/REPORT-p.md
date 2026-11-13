# PCB-P — layout para fabricación JLCPCB

2026-11-13 · base `0ef531f` · KiCad 10.0.6. Trabajo en `/private/tmp/grooveforge-layoutp`.
Método de PCB-F: reconstrucción determinista con `pcbnew`, ruteo A*, verificación geométrica
independiente contra CSV/XML, DRC CLI y exportación. Teoría previa: [THEORY-layout](../../boards/pcb-p/docs/THEORY-layout.md).

**Cierre eléctrico: 0 violaciones DRC, 0 conexiones pendientes, 0 problemas de paridad.**
La liberación mecánica requiere validar las hipótesis de cola/inserción con el sensor real indicadas al final.

## Placa y ensamble

223 × 31 mm; panel x=9–232, y=69–100; FR4 1,6 mm, 2 capas, 1 oz, montaje manual.
Cara superior P=TOP−6,5; inferior K=TOP−5,0; luz nominal 1,5 mm. Ningún cuerpo de conector arriba.
Recortar **metal y soldadura superiores a ≤0,8 mm**, dejando ≥0,7 mm nominal de luz con K.
La pata original del socket mide 3,2 mm: sin recortar sobresaldría 1,6 mm sobre P y no cabe.
No aplicar calor a la película. Apoyo del disco plano, sin serigrafía ni pads bajo Ø18,3;
los rótulos de disco dentro de ese círculo son dibujos de montaje, no tinta fabricada.

Se conservan el esquema y generador eléctricos, con **seis JST-PH J1–J6 de 2 vías**, uno a cada
entrada existente de PCB-M, y doce redes independientes. No se unifican alimentaciones.
El esquema no tiene GND: no se añade plano de masa. Pistas 0,25 mm, separación 0,20 mm,
cobre a corte/ranura ≥0,30 mm; vías Ø0,60/taladro 0,30. Reserva de montaje sin cobre de radio 2,5 mm.

Para mantener paridad sin cambiar el circuito, FSR1–FSR6 identifican los landings de socket en B.
`placement.csv` conserva los seis datums de disco F; las posiciones físicas de pines están en
`docs/layout-targets.json`. La huella **local** conserva el identificador histórico
`FSR402_DirectSolder_1x02_P2.54mm`, pero su geometría ahora es la del socket vertical estándar
KiCad, P2,54, PTH Ø1,0, pad 1,7. Su descripción declara explícitamente que **no** es soldadura directa.
No se altera la huella compartida usada por las fuentes históricas. Ver `gf_consola.pretty/` y `fp-lib-table` de P.

## Ranura, cola y socket

Ranura **NPTH oblonga 3,0 × 8,8 mm**, fresado con extremos R1,5; eje largo transversal a la cola.
Ancho de cola usado: 7,62 mm; holgura transversal nominal total **1,18 mm**. Centro a 12,3 mm
del disco sobre el eje de cola. Sin canto metalizado. Taladro NPTH exportado como G85 con fresa Ø3.
Desbarbar/proteger cantos; la película se curva en el hueco, no contra la arista de FR4.

**Interpretación de la cota recibida:** 37,4 mm desde borde del disco a punta metálica. **Confirmado por Juan (2026-11-13): midió desde donde termina el círculo.** La posición de los sockets queda cerrada.
El socket vertical abre hacia abajo: la punta debe acabar entrando hacia arriba.
El recorrido usa tres arcos de 90° de radio nominal de línea media R=2,5 mm, con un tramo inferior.
Espesor de película nominal 0,46; inserción metálica nominal I=3,0; recto inicial A=1,0 mm.

- Boca del socket respecto a P: −1,6−8,5 = **−10,1 mm**.
- Tramo vertical V = 1,6+8,5+0,23 = **10,33 mm**.
- L = A + B + 3πR/2 + V + R + I.
- B = 37,4−1−11,780972−10,33−2,5−3 = **8,789028 mm**.
- Borde→eje socket = A+B+3R = **17,289028 mm**.
- Centro disco→eje socket D = 9,15+17,289028 = **26,439028 mm**.
- Horizontal inferior a 15,1 mm bajo P; envolvente de película a **TOP−21,83 mm**.

Ángulo +25° hacia +x/+y en V1–V3 y X1–X2; V4 apunta a 205°.
La desviación en y deja las soldaduras fuera del disco vecino. La huella usa **pin 1** como datum;
la tabla da el **centro de ambos pines**, por eso no coincide con `layout-targets.json`.

| Ref. / pad | Centro disco, panel mm | Centro ranura, panel mm | Eje socket, panel mm | Ángulo en planta |
|---|---|---|---|---|
| FSR1 / V1 | 123.5, 84.5 | 134.648, 89.698 | 147.462, 95.674 | 25° |
| FSR2 / V2 | 154.5, 84.5 | 165.648, 89.698 | 178.462, 95.674 | 25° |
| FSR3 / V3 | 185.5, 84.5 | 196.648, 89.698 | 209.462, 95.674 | 25° |
| FSR4 / V4 | 216.5, 84.5 | 205.352, 79.302 | 192.538, 73.326 | 205° |
| FSR5 / X1 | 24.5, 84.5 | 35.648, 89.698 | 48.462, 95.674 | 25° |
| FSR6 / X2 | 55.5, 84.5 | 66.648, 89.698 | 79.462, 95.674 | 25° |

Sección a escala real: [cola 1:1](../../boards/pcb-p/docs/pcb-p-tail-section-1to1.svg).

## Diez agujeros: sin movimientos

NPTH Ø2,7 para M2.5 y PEM BSO-M2.5. **CSV sin cambios**, también válido para el paso a través de K.
Transformación exacta: `x_panel=x_KiCad+44`, `y_panel=371−y_KiCad`.

| Referencias | x panel | y panel | x KiCad | y KiCad |
|---|---:|---|---:|---|
| H1 / H2 | 40 | 72 / 97 | −4 | 299 / 274 |
| H3 / H4 | 89,5 | 72 / 97 | 45,5 | 299 / 274 |
| H5 / H6 | 139 | 72 / 97 | 95 | 299 / 274 |
| H7 / H8 | 170 | 72 / 97 | 126 | 299 / 274 |
| H9 / H10 | 201 | 72 / 97 | 157 | 299 / 274 |

## Validaciones

| Comprobación | Resultado |
|---|---|
| `kicad-cli pcb drc --exit-code-violations --schematic-parity` | **0 violaciones, 0 unconnected, 0 parity**; exit 0 |
| `layout_p.py --verify-only` | PASS: 28 huellas; seis discos/ranuras/sockets; 10 PEM exactos; 12 redes; sin pads bajo discos |
| `check_netlist.py --all` | PASS; P: 12 componentes / 24 pines / 12 redes; ERC P 0 errores / 0 advertencias |
| `check_bom.py` | PASS en todas las placas y 6 sockets adicionales; 0 compras sin verificar |
| `check_harness.py` | PASS, **32 cables, 0 errores** |
| `python3 -m pytest -q apps/hardware/kicad/consola/tools/tests` | **127 passed** |

DRC completo, sin exclusiones añadidas ni severidades rebajadas para cerrar el diseño.
Informe JSON en `boards/pcb-p/docs/pcb-p-drc.json`; geometría y particiones XML en la misma carpeta.
Los siete avisos ERC históricos de PCB-M no afectan el resultado del comprobador global; P tiene ERC limpio.
Durante depuración se aisló un aborto de KiCad/macOS al crear marcadores de violación; las causas físicas
(vías sobre taladros, separación a ranuras y campos de huella) se corrigieron. La ejecución final completa pasa.

## Compras

**6 sockets + 4 repuestos = 10** en `fab/pedido-lcsc-v1.csv`:
[HCTL PM254-1-02-Z-8.5, C2897365](https://www.lcsc.com/product-detail/C2897365.html).
Stock comprobado en la búsqueda conectada JLCPCB/LCSC el 2026-11-13: **10.481**; cuerpo 8,5 mm,
P2,54, 1×2, vertical THT. Identidad, fabricante, encapsulado, valor y huella en `tools/lcsc_catalog.json`.
`boards/pcb-p/assembly-sockets.csv` asocia XS1–XS6 a FSR1–FSR6 y cara B; `check_bom.py` valida
esta lista adicional con el mismo catálogo. La BOM del esquema conserva los sensores aportados por Juan.

## Archivos entregados

- `boards/pcb-p/pcb-p.kicad_pcb`, proyecto, reglas y librería local.
- `boards/pcb-p/fab/pcb-p-gerber-jlc.zip`: **7 Gerbers + 2 taladros** PTH/NPTH; sin pasta ni CPL.
- `apps/design/3d-models/consola/pcb-p.step`: placa y seis cuerpos de cada familia de conectores.
- `boards/pcb-p/docs/pcb-p-mounting-1to1.svg`, `pcb-p-bottom-1to1.svg`, `pcb-p-tail-section-1to1.svg`.
- Reproducir con Python de KiCad: `python3 tools/layout/layout_p.py` desde el directorio Console
  (usar `/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3`).

## Pendientes y límites de liberación mecánica

1. Confirmar que **37,4 se midió desde el borde**, no desde el centro. Si fue desde el centro,
   el eje calculado se desplaza 9,15 mm: no usar estos archivos sin recalcular.
2. Probar sensor/socket reales: espesor crimpado, inserción nominal 3 mm, ancho de cola, radios sin pliegues,
   respiradero libre y espacio inferior TOP−21,83. El CAD no prueba contacto ni fatiga de película.
3. El encargo dice «un JST», mientras esquema/mazo usan seis. Esta entrega conserva **los seis del esquema**.
   Un único conector de 12 vías exige una modificación coordinada de PCB-M y del mazo, fuera de este layout.
4. Imprimir SVG al 100%, sin ajustar a página; apoyar un sensor y socket reales antes de pedir fabricación.

Referencia de curvatura y apoyo: Interlink Electronics, *FSR Integration Guide*, §9,
[guías oficiales](https://www.interlinkelectronics.com/integration-guides). No se certifica fuerza/velocity en CAD.
