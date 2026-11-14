# Pedido GrooveForge — JLCPCB + LCSC, noviembre de 2026

Preparado el **2026-11-14** para Juan. Opción B: fabricar cinco unidades de cada una de las seis placas;
ensamblar **solo dos PCB-M, SMD en TOP**. El carrito de piezas manuales abastece **una consola + repuestos**,
no las cinco placas de cada diseño ni dos consolas completas. La segunda M ensamblada queda como repuesto SMD.

**Estado:** cinco ZIP disponibles y comprobados; PCB-M sigue en ruteo en otra sesión. Su ZIP, BOM y CPL
quedan pendientes. Este documento no libera PCB-M ni sustituye las comprobaciones físicas y de visor de abajo.
No se ha enviado ni pagado ningún pedido.

## Configuración de fabricación

Rutas desde la raíz del repositorio. Un diseño por línea, entrega como placa individual, FR4.
Todos los importes son **estimados históricos por lote de cinco**, no por unidad ni cotizaciones actuales.

| Placa | ZIP exacto a subir | Tamaño Edge.Cuts, mm | Capas | Espesor, mm | Cobre exterior | Acabado | Máscara | Cantidad PCB | Remove order number | Ensamblaje | USD estimados / lote |
|---|---|---:|---:|---:|---|---|---|---:|---|---|---:|
| pcb-k-left | `apps/hardware/kicad/consola/boards/pcb-k-left/fab/pcb-k-left-gerber-jlc.zip` | 102,2 × 216 | 2 | 1,6 | 1 oz | HASL sin plomo | Negro | 5 | Sí / Remove Mark | No; manual | 14,70 |
| pcb-k-right | `apps/hardware/kicad/consola/boards/pcb-k-right/fab/pcb-k-right-gerber-jlc.zip` | 144 × 152 | 2 | 1,6 | 1 oz | HASL sin plomo | Negro | 5 | Sí / Remove Mark | No; manual | 14,60 |
| pcb-p | `apps/hardware/kicad/consola/boards/pcb-p/fab/pcb-p-gerber-jlc.zip` | 223 × 31 | 2 | 1,6 | 1 oz | HASL sin plomo | Negro | 5 | Sí / Remove Mark | No; manual | 7,40 |
| pcb-a | `apps/hardware/kicad/consola/boards/pcb-a/fab/pcb-a-gerbers.zip` | 217 × 29,6 | 2 | 1,6 | 1 oz | HASL sin plomo | Negro | 5 | Sí / Remove Mark | No; manual | ~11,00 |
| pcb-f | `apps/hardware/kicad/consola/boards/pcb-f/fab/pcb-f-gerber-jlc.zip` | 81 × 83 | 2 | 1,6 | 1 oz | HASL sin plomo | Negro | 5 | Sí / Remove Mark | No; manual | 4,00 |
| pcb-m | **PENDIENTE:** `apps/hardware/kicad/consola/boards/pcb-m/fab/pcb-m-gerber-jlc.zip` | 186 × 80 (placa actual; reconfirmar al cerrar ruteo) | 2 | 1,6 | 1 oz | HASL sin plomo | Negro | 5 | Sí / Remove Mark | **Economic PCBA, TOP, 2 unidades** | 9,40 + ~47,00 ensamble |

**Color y acabado:** el [doc 28 §1](../../../../docs/consola/28-pcb-k.md) especifica máscara negra para K;
se conserva y se extiende a las otras placas para uniformidad (elección operativa de este paquete).
No se encontró exigencia de ENIG en las especificaciones revisadas. Se elige HASL sin plomo: no hay contactos
de desgaste que exijan oro y no se justifica el sobrecosto de ENIG. La [tabla oficial de PCBA de JLC](https://jlcpcb.com/capabilities/pcb-assembly-capabilities)
admite Economic de una cara con negro, 1,6 mm y HASL sin plomo; cubre también el tamaño de M y sus 0402.
Standard queda como alternativa solo si la cotización final identifica una restricción concreta;
si se cambia el servicio, revisar sus exigencias de rieles y fiduciales antes de pagar.

**Marcado:** seleccionar `Remove order number = Yes` o `Mark on PCB → Remove Mark` en **las seis líneas**;
no hace falta añadir `JLCJLCJLCJLC` con esta opción. Comprobar la selección en el resumen.
Si se eligiera conservar el número con posición explícita, la [guía oficial de JLC](https://jlcpcb.com/es/help/article/how-to-remove-order-number-from-your-pcb)
requiere ese texto exacto en serigrafía y la opción de posición: habría que reservar una zona libre en B.SilkS,
fuera de ventanas, discos FSR, pads y agujeros, y reexportar el ZIP. Ese cambio no forma parte de este paquete.

### Cómo se midieron los tamaños

Lectura sin guardar con `pcbnew.LoadBoard` (KiCad 10): envolvente de los dibujos en `Edge.Cuts`,
quitando medio ancho de trazo del bounding box de cada dibujo. Se incluyen arcos y recortes;
no se usa el tamaño de la hoja ni la envolvente de huellas/serigrafía.

| Placa | x mínimo…máximo KiCad, mm | y mínimo…máximo KiCad, mm |
|---|---|---|
| pcb-k-left | −38…64,2 | 51…267 |
| pcb-k-right | 64…208 | 191…343 |
| pcb-p | −35…188 | 271…302 |
| pcb-a | 10…227 | 166,4…196 |
| pcb-f | 210…291 | 200…283 |
| pcb-m | 96…282 | 35…115 |

Las seis placas guardadas declaran dos capas y 1,6 mm. Los cinco ZIP existentes pasan CRC y coinciden
byte a byte con los Gerbers/taladros de sus carpetas `fab/`. No se regeneraron archivos de fabricación.

## PCB-M: archivos de ensamblaje pendientes

La otra sesión debe entregar, después del ruteo, relleno y DRC/paridad finales:

| Archivo | Estado y contenido requerido |
|---|---|
| `apps/hardware/kicad/consola/boards/pcb-m/fab/pcb-m-gerber-jlc.zip` | PENDIENTE; Gerbers y taladros finales de la misma revisión |
| `apps/hardware/kicad/consola/boards/pcb-m/fab/pcb-m-bom-jlc.csv` | PENDIENTE; BOM JLC agrupada, solo SMD poblados, referencias y códigos LCSC; cantidades por placa |
| `apps/hardware/kicad/consola/boards/pcb-m/fab/pcb-m-cpl-jlc.csv` | PENDIENTE; centro X/Y en mm, rotación y cara TOP, mismo origen/revisión que los Gerbers |

`fab/bom-pcb-m.csv` es la BOM de auditoría completa, **no** la BOM filtrada de montaje JLC.
Excluir DNP, THT, IDC, sockets Teensy/Audio Shield, SW_PAD1, testpoints, net-ties y puentes de cobre
como piezas compradas. Sí incluir los SMD poblados, también J2_I2C1 **C160404** y C_CV_FILTER1 **C7469972**.
La placa actual tiene los SMD en F.Cu; verificarlo de nuevo al cerrar el ruteo.

## Carrito LCSC y cobertura manual

Importar [pedido-lcsc-v1.csv](pedido-lcsc-v1.csv): **24 códigos**, cantidades para una consola manual.
Identidades según catálogo/BOM existentes; stock, MOQ, escalones y precios se reconfirman al importar.
La clasificación excluye todos los DNP; DNP significa **no poblar**, no una pieza manual faltante.

| LCSC | Uso manual poblado | Necesarios | Pedidos | Repuestos |
|---|---|---:|---:|---:|
| C506653 | MCP23017: A ×1, K izquierda ×2, K derecha ×1 | 4 | 5 | 1 |
| C5378731 | LED inverso: K izquierda ×17, derecha ×9 | 26 | 30 | 4 |
| C49678 | 100 nF 0805: K ×29, F ×5 | 34 | 100 | 66 |
| C5137478 | 1 µF 0805, K | 3 | 20 | 17 |
| C12891 | 22 µF 1206, K | 2 | 10 | 8 |
| C17414 | RESET 10 kΩ 0805, K | 3 | 50 | 47 |
| C17634 | Serie LED 33 Ω 0805, K | 2 | 20 | 18 |
| C14663 | 100 nF 0603, A C1 | 1 | 10 | 9 |
| C1589 | 10 nF 0603, A C10–C25 | 16 | 50 | 34 |
| C15850 | 10 µF 0805, A C2 | 1 | 10 | 9 |
| C131337 | PH 2P vertical, M | 8 | 10 | 2 |
| C173752 | PH 2P horizontal: M ×1, P ×6 | 7 | 9 | 2 |
| C131339 | PH 3P vertical, M | 15 | 18 | 3 |
| C131334 | PH 4P vertical, M jog/M4 | 2 | 3 | 1 |
| C131342 | PH 6P vertical, K izquierda/derecha | 2 | 3 | 1 |
| C158038 | PH 10P vertical, M/K izquierda | 2 | 3 | 1 |
| C160404 | SH 4P SMD, **solo A J2** | 1 | 2 | 1 |
| C160405 | SH 6P SMD, A J3/F J1 | 2 | 3 | 1 |
| C41376163 | Hong Cheng HC-PM254-8.5H-1x24PZ, Teensy M | 2 | 3 | 1 |
| **C22373900** | **Hong Cheng HC-PM254-8.5H-1x14PZ, Audio Shield M J_SHD_L1/J_SHD_R1** | **2** | **3** | **1** |
| C225519 | Header 2×4, M SW_PAD1 | 1 | 2 | 1 |
| C2897365 | HCTL 1×2 inferior, P XS1–XS6; suplemento assembly-sockets.csv | 6 | 10 | 4 |
| C309284 | Jacks 6,35 mm de panel: 2 IN, 2 OUT, pedal, audífonos | 6 | 7 | 1 |
| C49274302 | Jacks 3,5 mm de panel: CV1/CV2 | 2 | 3 | 1 |

Los pasivos económicos conservan los lotes del pedido previo. Conectores: uno por tipo de baja cantidad,
dos PH2P y tres PH3P de repuesto. Los códigos **C14663, C15850 y C160404** también aparecen en M:
sus cantidades sueltas son exclusivamente para **A + sus repuestos**, no duplican el montaje de M.
Ningún código exclusivo del SMD de M se incluye en LCSC; en particular no comprar C7469972 suelto.

Fuera del CSV, con suministro identificado:

- **Compra local:** dos IDC macho con caja/muesca 2×15 P2,54 rectos, M J1_PCBA1 y A J1;
  se recomienda un tercero de repuesto. `Supply=local`, sin LCSC verificado: no inventar código.
- **Juan aporta:** 26 Kailh Choc V2 PG1353, ocho encoders EC11 de A y cuatro RS6011 de F;
  también los seis FSR402, Teensy 4.1, Audio Shield y encoder jog según pedido-v1.
  **No pedir zócalos de teclas CPG135001S30**: doc 28 D-K1b, soldadura directa.
- **PCB-P vigente:** sockets inferiores C2897365 y ranuras para las colas, según su README y
  `assembly-sockets.csv`; esta solución sustituye la antigua soldadura directa D43.
- **Cables ya armados, compra externa:** PH2 ×15, PH3 ×12, PH4 ×2, PH6 ×1, PH10 ×1;
  SH4 ×1, SH6 ×1; plano IDC 30 hilos con dos hembras 2×15 ×1. Cantidades de pedido-v1;
  verificar largos y pin 1 contra `boards/CONNECTORS.md` y `tools/HARNESS.md`.
  Carcasas/terminales sueltos no se compran porque se usarán cables armados.

## Checks antes de pagar

### Piezas reales y plantillas de Juan

- [ ] Imprimir SVG al **100 %, sin ajustar a página**; medir con regla una cota conocida antes de superponer piezas.
  K: `boards/pcb-k-left/docs/pcb-k-left-mounting-1to1.svg` y su equivalente `pcb-k-right`.
  P: `boards/pcb-p/docs/pcb-p-mounting-1to1.svg`, `pcb-p-assembly-1to1.svg`,
  `pcb-p-bottom-1to1.svg` y `pcb-p-tail-section-1to1.svg` en la misma carpeta.
  F: `boards/pcb-f/docs/pcb-f-assembly-1to1.svg` y `pcb-f-bottom-assembly-1to1.svg`.
  A: imprimir `boards/pcb-a/docs/pcb-a-mounting.svg` y `pcb-a-assembly-top.svg` a escala real,
  verificando el contorno 217 × 29,6 mm y el paso de encoders 26 mm.
  Rutas de este listado relativas a `apps/hardware/kicad/consola/`.
- [ ] Superponer Choc V2 y LED inverso en K; comprobar patas, retención en chapa de 2 mm y ventanas LED.
  A: encoders/ejes y separadores de 18 mm. F: un RS6011 real, paso 20 mm, cursor, fijaciones y JST inferior.
  M: comprobar Teensy/Audio Shield y altura de sockets sobre la plantilla final que entregue la sesión de ruteo.
- [ ] **Multímetro, fader desconectado:** cada pata mecánica **MP contra carcasa pita**;
  cada MP contra **cada una de las tres patas eléctricas no pita**, en todo el recorrido.
  Comprobar los cuatro faders. Los MP irán a GND en PCB-F: si falla esta prueba, resolver antes de fabricar.
- [ ] P: probar FSR/socket reales, cola e inserción sin forzar película; cota de cola 37,4 mm desde el borde
  del disco ya confirmada por Juan. Revisar radios, respiradero, espacio inferior hasta TOP−21,83 mm y
  recorte de pines/soldadura superiores a ≤0,8 mm para conservar luz con K.
- [ ] Verificar rosca del jack 6,35 mm contra agujero Ø9,5 y del 3,5 mm contra Ø6,2;
  polaridad/pinout de cables JST e IDC, orientación de enchufe y alturas antes de cerrar compra externa.

### Visor Gerber JLC: abrir cada uno de los seis diseños

- [ ] **K izquierda:** contorno escalonado completo 102,2 × 216, ventanas/recortes de sus **17 LED**,
  PTH de teclas directas y NPTH de montaje; nada tapado por cobre/serigrafía.
- [ ] **K derecha:** contorno 144 × 152, ventanas/recortes de sus **9 LED**, montaje y pasos NPTH Ø4,6
  para los seis separadores de P que la atraviesan; revisar huecos y reservas sin cobre.
- [ ] **P:** contorno 223 × 31, diez NPTH Ø2,7 y **seis ranuras NPTH oblongas 3,0 × 8,8 mm, R1,5**.
  Confirmar fresado sin metalizar en el archivo NPTH (seis G85), sin duplicarlo como corte metalizado;
  seis sockets y seis JST inferiores, zona de discos libre.
- [ ] **A:** tira 217 × 29,6 con esquinas R1, cuatro NPTH Ø3,2, ocho encoders y aperturas de máscara
  completas; comprobar cobre y serigrafía inferiores del IDC/JST. R1/R2/J4 quedan sin poblar.
- [ ] **F:** contorno 81 × 83, ocho NPTH Ø2 de faders y cuatro M3 NPTH Ø3,2,
  máscara alrededor de MP y separación al borde; orientación del JST por debajo.
  Bajo los cursores queda FR4 con reservas sin cobre de 5 × 65 mm; las ranuras de cursor pertenecen
  a la chapa, no son ventanas que haya que fresar en esta PCB.
- [ ] **M (cuando exista ZIP final):** 186 × 80 o nueva medida justificada, contorno cerrado,
  ventanas/recortes previstos, taladros/NPTH de fijación y sockets; no subir una exportación previa al ruteo.
- [ ] En cada visor: escala correcta, dos cobres F/B, máscaras y serigrafías correspondientes,
  PTH/NPTH presentes, contorno sin duplicados, aperturas internas conservadas y `Remove Mark` seleccionado.

### Visor PCBA de M y cierre del carrito

- [ ] Revisar BOM/CPL finales juntos: solo SMD poblados, **TOP, 2 ensambladas de 5 fabricadas**;
  ningún DNP ni THT seleccionado. Verificar disponibilidad y cada equivalencia de código/encapsulado.
- [ ] Comparar **rotaciones y pin 1** pieza por pieza contra serigrafía/pads: IC, conectores SH,
  diodos/LED, transistores y fusible; comprobar polaridad positiva de C_CV_FILTER1. Corregir en el visor
  cualquier giro desplazado antes de aprobar; no asumir que el ángulo KiCad coincide con el de JLC.
- [ ] Ejecutar DRC/paridad final de M en la sesión responsable; revalidar BOM/CPL y ZIP de esa revisión.
  Los checks de BOM de este paquete no certifican el ruteo de M.
- [ ] Importar CSV LCSC y comparar los 24 códigos y cantidades, orientación PH vertical/horizontal,
  stock/MOQ y precios; sin sustituciones automáticas no revisadas ni zócalos de teclas.
- [ ] Revisar el total final con negro/HASL, PCBA, repuestos, envío combinado e impuestos mostrados;
  guardar los resúmenes de pedido y archivos exactos aprobados.

### Envío combinado JLC/LCSC — «combine shipping»

Seguir la [guía oficial de LCSC](https://www.lcsc.com/help-center/ordering-quotation/combine-lcsc-jlcpcb-orders),
consultada para este paquete:

1. Usar el mismo ID de cliente y moneda. Agrupar las seis líneas JLC en un pedido.
2. **Antes del checkout JLC**, copiar de esa guía la dirección vigente del almacén de consolidación
   de Hong Kong, incluido destinatario/teléfono. En LCSC conservar el destino final de Juan.
3. Crear primero el pedido JLC; luego, al pagar LCSC, usar **Combine Orders / combine shipping**
   y seleccionar el pedido JLC. Si no aparece, Juan debe contactar `support@lcsc.com` con ambos números.
4. Si LCSC ya está pedido, solicitar a ese soporte que lo retenga antes de despacharlo y vincule ambos pedidos.
5. Confirmar vinculación y flete recalculado antes de liberar el envío. Una vez combinado no se puede dividir;
   si LCSC ya despachó puede fallar. Para unir varios pedidos JLC, acudir a soporte JLC.

No basta una nota «ship together». Esta preparación no ha enviado mensajes a soporte.

## Presupuesto estimado y validación

Base: [pedido-v1](pedido-v1.md) y [doc 28 §7](../../../../docs/consola/28-pcb-k.md), calculadora pública
consultada allí el 2026-11-12. Los precios K originales eran verdes: usar negro puede variar precio/plazo.
Los ~11 USD de A ya eran una estimación, no un precio confirmado.

- **Placas, seis lotes de cinco: ~61,10 USD.**
- **Ensamble SMD de dos M: ~47 USD adicionales** históricos (preparación, extendidas y componentes);
  M con su lote de placas: ~56,40 USD. Total JLC orientativo: **~108,10 USD**.
- LCSC: recotizar el CSV actualizado; el estimado anterior de chips/conectores no incluye con fiabilidad
  los sockets Audio Shield, jacks ni todos los repuestos nuevos. No se presenta como total vigente.
- Envío histórico orientativo: 30–40 USD, pendiente de consolidación; impuestos y compras externas sin cotizar.
  El total a pagar será el de ambos checkouts, no el histórico ~140 USD de pedido-v1.

Validaciones locales del paquete (2026-11-14): `check_bom.py --board <placa>` pasa para las seis placas,
con cero errores de identidad y cero compras sin verificar, incluidos los seis sockets adicionales de P.
`python3 -m pytest -q apps/hardware/kicad/consola/tools`: **148 passed, 1 skipped**.
La prueba de carrito comprueba cobertura manual con repuestos y ausencia de códigos exclusivos del SMD de M.
El hash del carrito se actualiza en
`boards/pcb-p/docs/fabrication-manifest.json`; solo cambia esa entrada del manifiesto.
