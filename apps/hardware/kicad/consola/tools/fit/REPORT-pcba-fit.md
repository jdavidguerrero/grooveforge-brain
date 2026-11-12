# PCB-A · verificación de encaje para Console v1

**Conclusión: PCB-A NO ENCAJA en las reservas de planta v1.** El paso y los ocho ejes pasan; el contorno existente invade PCB-F, PCB-K y la reserva de placa de la M4. Esto bloquea cerrar el montaje para el primer pedido. Los solapes XY no se presentan como choques 3D demostrados: los sustratos están a distintas alturas. La compatibilidad en altura queda condicionada a componentes, soldaduras, M4 y perilla reales. No se ha modificado ninguna placa.

## Reproducción y fuentes

Desde la raíz del clon, sin dependencias externas ni FreeCAD/pcbnew:

```sh
python3 apps/hardware/kicad/consola/tools/fit/check_pcba_fit.py
python3 -B -m unittest discover -s apps/hardware/kicad/consola/tools/fit -p "test_*.py"
```

El script escribe únicamente este informe, `pcba-fit.json` y `pcba-fit.svg` junto al script. Una ejecución correcta termina en 0 aunque el encaje falle; `encoder_pass` y `conclusion` en JSON contienen el resultado de ingeniería. El hash de cada entrada permite detectar cambios. No es un DRC ni una certificación de fabricación. Método y límites: [THEORY.md](THEORY.md). Base revisada: `095370c19b340b084151e3a9079798f92aa0c23d`.

## Ejes: origen de huella, conversión de coordenadas y transformación

Vista superior: X derecha, Y hacia atrás desde el frente, Z desde el piso del cuerpo. El README de PCB-K fija **Y_KiCad = constante − y_panel**. El círculo Ø6 de cada EC11 da el eje local **(+7,5; +2,5) mm**; el origen de huella está en el pad 1 y no en el eje. Los EC11 siguen en F.Cu, mirando arriba.

Ajuste sin escala: **(x_panel, y_panel) = R(0.000000°) · (x_KiCad, −y_KiCad) + (44.000, 371.000) mm**. No hace falta rotación física adicional. La inversión de Y cambia la convención del dibujo, no la cara de montaje.

| Encoder | Origen KiCad | Eje KiCad | Objetivo panel | Eje ajustado | Error mm |
|---|---|---|---|---|---|
| RE1 → E1 | (27.500, 176.500) | (35.000, 179.000) | (79.000, 192.000) | (79.000, 192.000) | 0.000000 |
| RE2 → E2 | (53.500, 176.500) | (61.000, 179.000) | (105.000, 192.000) | (105.000, 192.000) | 0.000000 |
| RE3 → E3 | (79.500, 176.500) | (87.000, 179.000) | (131.000, 192.000) | (131.000, 192.000) | 0.000000 |
| RE4 → E4 | (105.500, 176.500) | (113.000, 179.000) | (157.000, 192.000) | (157.000, 192.000) | 0.000000 |
| RE5 → E5 | (131.500, 176.500) | (139.000, 179.000) | (183.000, 192.000) | (183.000, 192.000) | 0.000000 |
| RE6 → E6 | (157.500, 176.500) | (165.000, 179.000) | (209.000, 192.000) | (209.000, 192.000) | 0.000000 |
| RE7 → E7 | (183.500, 176.500) | (191.000, 179.000) | (235.000, 192.000) | (235.000, 192.000) | 0.000000 |
| RE8 → E8 | (209.500, 176.500) | (217.000, 179.000) | (261.000, 192.000) | (261.000, 192.000) | 0.000000 |

**Pasos reales consecutivos:** 26.000, 26.000, 26.000, 26.000, 26.000, 26.000, 26.000 mm. Criterio ejes ≤0,1 mm y paso nominal 26 mm (tolerancia numérica 0,000001 mm): **PASS**.

Diagnóstico de convención incorrecta: sin invertir Y, la misma fila colineal daría traslación (44.000, 13.000), y llevaría la prolongación hacia la pantalla (y 183,4–253). Los ejes por sí solos no detectan ese error. Ese caso contradice la conversión explícita del repositorio y no se usa para certificar el montaje.

## Contorno, montaje y conectores reales

**Vértices Edge.Cuts, KiCad → panel, mm:**

| KiCad | Panel |
|---|---|
| (10.000, 170.400) | (54.000, 200.600) |
| (10.000, 192.500) | (54.000, 178.500) |
| (187.000, 192.500) | (231.000, 178.500) |
| (187.000, 240.000) | (231.000, 131.000) |
| (227.000, 240.000) | (271.000, 131.000) |
| (227.000, 170.400) | (271.000, 200.600) |

Envolvente **217 × 69,6 mm**, área **6695.70 mm²**, límites panel (xmin,ymin,xmax,ymax) **(54.000, 131.000, 271.000, 200.600)**. La tira es x 54–271 / y 178,5–200,6; la prolongación es x 231–271 / y 131–178,5. Placeholder FreeCAD: origen (70.000, 177.000, 71.400), tamaño (200.000, 30.000, 1.600); su caja 200×30 no representa esta PCB. No hay contorno de PCB-A en el CSV v5: manda Edge.Cuts.

| Agujero | KiCad | Panel | Taladro mm |
|---|---|---|---|
| H1 | (18.000, 176.000) | (62.000, 195.000) | 3.20 |
| H2 | (74.000, 179.000) | (118.000, 192.000) | 3.20 |
| H3 | (100.000, 179.000) | (144.000, 192.000) | 3.20 |
| H4 | (194.000, 226.000) | (238.000, 145.000) | 3.20 |

El modelo no crea soportes para esos cuatro agujeros de PCB-A; tampoco aparecen en los PEM del CSV v5. H1 queda en (62,195) sobre PCB-K izquierda; H4 en (238,145) sobre la derecha. Falta diseñar soportes bajo PCB-A o estudiar pasos/espaciadores a través de K, respetando la diferencia Z.

| Conector | Origen KiCad | Origen panel | Ángulo KiCad / panel | Cara | Centro pads eléctricos panel | Caja courtyard panel |
|---|---|---|---|---|---|---|
| J1 | (215.730, 232.780) | (259.730, 138.220) | 0° / 0° | B.Cu | (261.000, 156.000) | (256.050, 132.620, 265.950, 179.380) |
| J2 | (194.000, 236.700) | (238.000, 134.300) | 180° / 180° | B.Cu | (238.000, 136.300) | (234.100, 131.020, 241.900, 137.580) |
| J3 | (205.500, 236.700) | (249.500, 134.300) | 180° / 180° | B.Cu | (249.500, 136.300) | (244.600, 131.020, 254.400, 137.580) |
| J4 | (190.300, 210.000) | (234.300, 161.000) | 90° / 90° | B.Cu | (236.300, 161.000) | (231.020, 154.100, 237.580, 167.900) |

J1 es IDC 2×15 vertical, con eje largo paralelo a Y y salida hacia abajo (B.Cu); el origen es pad 1, no centro de cinta. J2/J3 son SH horizontales de 4/6 vías a 180°; J4 es SH10 a 90° y **DNP por contrato**, sin cable. Ángulo panel: dirección del eje local +X, CCW en X derecha/Y atrás. La abertura del SH sigue su geometría local; no confundirla con el eje +X. Las cajas son courtyards (holgura de colocación), no volúmenes de enchufe o radio de curvatura.

## Planta: áreas reales, sin usar la envolvente de la L

| Obstáculo | Área solapada mm² | Separación mínima XY mm | Resultado |
|---|---|---|---|
| PCB-K izquierda (partición inferida) | 309.400 | 0.000 | SOLAPE |
| PCB-K derecha (partición inferida) | 994.000 | 0.000 | SOLAPE |
| PCB-K v5 conjunto (conservador) | 1303.400 | 0.000 | SOLAPE |
| Ventana M4 | 0.000 | 0.000 | CONTACTO |
| M4 nominal CSV | 0.000 | 1.000 | LIBRE |
| M4 reserva doc28 (ventana +5 mm) | 215.000 | 0.000 | SOLAPE |
| Pantalla módulo | 0.000 | 7.280 | LIBRE |
| PCB-F | 680.000 | 0.000 | SOLAPE |
| PCB-P | 0.000 | 31.000 | LIBRE |
| PCB-M | 0.000 | 75.400 | LIBRE |

PCB-F ocupa x 254–335 / y 88–171; su intersección con A es **17×40 = 680 mm²**. La ventana M4 ocupa x 109–231 / y 107–169: hay **contacto sin margen** en x=231. La M4 nominal del CSV ocupa x 110–230 / y 108–168 y conserva solo **1 mm** lateral. Doc28 §2 reserva 5 mm alrededor de la ventana para la placa colgante: x 104–236 / y 102–174; A invade esa reserva **5×43 = 215 mm²**. Es una reserva documental, no una medición del PCB Adafruit completo.

**PCB-K partida:** no hay layouts ni contornos individuales de izquierda/derecha. CSV v5 y DXF describen la placa conjunta. Se muestra una partición inferida según doc28 §7: corte x=108 y eliminación de la franja inferior x108–231/y169–173. Da envolventes 102×216 y 144×152 como el doc28. También se comprueba la placa conjunta como límite conservador. No se convierte esta inferencia en una propuesta de layout aprobada. Se restan la ventana M4 y la franja, no los pequeños taladros/FSR (alejados del choque de interés); las áreas son de reserva de contorno.

**Pared de 3,2 mm:** límites interiores x/y 3,2–336,8. Holguras del contorno: izquierda 50.800 mm, frente 127.800 mm, derecha 65.800 mm, atrás 136.200 mm. **PASS**. La placa queda lejos de las esquinas redondeadas (R interior 3,4), por lo que no afectan el resultado. El tabique de bahía en y211–213 queda 10,4 mm detrás del borde de A; el de x136–138 empieza en y211. Los topes de las juntas están fuera de su altura/planta.

| Torre cuerpo (centro) | Radio mm | Holgura real al círculo mm |
|---|---|---|
| (8.000, 8.000) | 5.0 | 171.596 |
| (100.000, 8.000) | 5.0 | 165.500 |
| (240.000, 8.000) | 5.0 | 118.000 |
| (332.000, 8.000) | 5.0 | 132.295 |
| (8.000, 110.000) | 5.0 | 77.512 |
| (332.000, 178.000) | 5.0 | 56.000 |
| (332.000, 300.000) | 5.0 | 111.625 |
| (8.000, 333.000) | 5.0 | 135.163 |
| (134.000, 334.000) | 5.0 | 128.400 |

Torres Ø10: mínima holgura **56.000 mm**, PASS en planta. Su techo es z=73.0, a la altura de la cara superior de A; los PEM continúan hasta la chapa y también quedan fuera en XY.

## Altura de EC11, PCB y perilla Ø18

| Magnitud | Valor mm |
|---|---|
| Cara superior aluminio | z=85.000 |
| PCB-A sustrato inferior / superior | 71.400 / 73.000 |
| Base EC11 respecto al aluminio | -12.000 |
| PCB-F inferior / superior | 73.900 / 75.500 |
| Luz A superior → F inferior | 0.900 |
| Luz A superior → K inferior | 7.000 |
| Módulo pantalla inferior / superior (modelo) | 68.200 / 83.000 |

**Base −12 mm: PASS del apilado nominal.** Se extrae el placeholder TOP−13,6 más el espesor real 1,6 mm de A; base EC11 = TOP−12 = z73. No hay separador de A modelado que imponga esa altura en el ensamble. Las superficies de sustrato A/F no se atraviesan, pero **0,9 mm no certifica espacio para componentes superiores/soldaduras en la zona solapada**. La luz a K es 7 mm y también requiere revisar sus componentes inferiores.

Componentes superiores de A cuyo courtyard entra en reservas vecinas: ninguno de los componentes F.Cu medidos (excluyendo encoders y agujeros).

| Ref | Valor | Solape courtyard F mm² | Solape reserva M4 mm² |
|---|---|---|---|

No se demuestra un choque de encapsulados con F: los SH están abajo. Sí quedan **26 pines THT de J1** con centro bajo F (J1.1, J1.10, J1.11, J1.12, J1.13, J1.14, J1.15, J1.16, J1.17, J1.18, J1.19, J1.2, J1.20, J1.21, J1.22, J1.23, J1.24, J1.25, J1.26, J1.3, J1.4, J1.5, J1.6, J1.7, J1.8, J1.9); sus puntas/soldaduras emergen sobre A y tienen solo 0,9 mm hasta el sustrato F. Medir protrusión y fijar margen aislante. No suponer ese volumen nulo.

**Eje H20: comprobación condicional.** Si el EC11 instalado tiene punta a 20 mm de la superficie de montaje, sobresale **20−12 = 8 mm** sobre el aluminio; con carrera de clic de 0,5 mm (0,8 mm máxima en la variante EC11E15244G1), quedan 7,5 mm nominales (7,2 mm al máximo de carrera). El catálogo oficial [Alps EC11E, dibujos 2/3](https://tech.alpsalpine.com/cms.media/product_catalog_ec_01_ec11e_en_611f078659.pdf) y la [ficha EC11E15244G1](https://tech.alpsalpine.com/e/products/detail/EC11E15244G1/) distinguen versiones con clic de 0,5 y 1,5 mm. En la placa solo figura EC11, no un MPN de compra. El nombre H20 y el enlace del footprint no demuestran qué variante se montará.

Para una perilla de agujero ciego que asienta en la punta, con hueco inferior g y profundidad de inserción d, **g+d=8 mm**. Si g≥0,8 mm para permitir la carrera máxima de la variante de 0,5, d≤7,2 mm (antes de tolerancias). Para la variante de 1,5 mm hace falta mayor hueco. Ø18 solo fija el diámetro exterior: no especifica d, altura, ajuste del eje D/estriado ni posición del hombro. Por tanto **perilla pendiente de validación**, no PASS automático. Separación exterior entre perillas: 26−18 = **8 mm**. Corte de chapa Ø7,1 para eje nominal Ø6: **0,55 mm radial**; verificar que el casquillo/hombro real no invada ese corte a z83–85.

El modelo pone el módulo de pantalla con techo z83, es decir 2 mm por debajo de TOP85, aunque el prompt v5 pide vidrio a ras. Se señala esa diferencia sin modificarla; no afecta la separación XY de 7,28 mm de A a la pantalla.

## Mazo: distancias aproximadas y cruces

**Extremos conocidos:** centro de pads eléctricos de J1/J2/J3 de A; Z usado = cara inferior del sustrato (z71,4). No se conocen alturas del enchufe. **Extremos estimados:** J1_PCBA=(261,280,9,6), J2_I2C=(238,280,9,6) en el frente de PCB-M; F.J1=(257,165,73,9), cerca de su borde izquierdo y sobre la cara inferior. PCB-M no tiene footprints colocados. El `.kicad_pcb` de F es legacy 46×92 mm y no corresponde al contorno Console de 81×83: su J1 no se traslada como si fuera Console. Son hipótesis explícitas de encaminado, no coordenadas reales inventadas de esos conectores.

| Ruta | Inicio XYZ | Destino estimado XYZ | Recta 3D mm | Rango destino en toda placa mm | Polilínea de servicio mm |
|---|---|---|---|---|---|
| J1 → PCB-M J1_PCBA | (261.000, 156.000, 71.400) | (261.000, 280.000, 9.600) | 138.55 | (134.979, 225.522) | 185.80 |
| J2 → PCB-M J2_I2C | (238.000, 136.300, 71.400) | (238.000, 280.000, 9.600) | 156.43 | (152.759, 230.875) | 205.50 |
| J3 → PCB-F J1 | (249.500, 136.300, 71.400) | (257.000, 165.000, 73.900) | 29.77 | (5.148, 98.231) | PENDIENTE (atraviesa A) |

| Ruta | Cruza ventana M4 XY | Cruza pantalla XY | Z del cruce de pantalla | Luz mínima por debajo del módulo mm |
|---|---|---|---|---|
| J1 | False | True | (45.544, 9.600) | 22.66 |
| J2 | False | True | (40.616, 9.600) | 27.58 |
| J3 | False | False | — | — |

Las rectas J1/J2 pasan bajo la pantalla **en proyección**, pero ya han descendido por debajo de z68,2 al entrar en su huella: no chocan con el prisma del módulo bajo estas hipótesis. La alternativa de servicio baja a z60 antes de pasar bajo el módulo y sigue hacia PCB-M. J3 no cruza ventana/pantalla, pero su recta estimada **atraviesa el propio sustrato de A**: sube de z71,4 a z73,9 sin salir de la prolongación x231–271/y131–178,5. **Su longitud recta es solo una cota inferior, no una ruta utilizable.** El conector supuesto F.J1 inferior tiene además solo 0,9 mm libres hasta A: su enchufe no queda validado. Para cerrar J3 se necesita colocar F.J1 fuera de la proyección de A o estudiar montarlo arriba, rodear el borde de A y llegar desde arriba de F; la altura del conector se debe cotar antes de calcular un largo de compra. No se aplica ninguna recolocación. Las tres rectas evitan la ventana y M4 nominal en XY; la placa A sigue invadiendo su reserva lateral.

Longitud de compra = recorrido + bucles de montaje/terminación; no basta con la recta 3D. Usar aproximadamente 20–30 % de margen una vez fijados los conectores, medir el camino con cinta y comprobar radio mínimo del cable/IDC seleccionado. No se fija un radio sin ficha del mazo. Antes del pedido faltan ubicación/orientación/altura de los dos conectores M, el de F y el volumen enchufado de J1 (cinta de 30 vías).

## Qué cambiar, sin aplicarlo

1. Mantener los ejes aprobados E1–E8: ya pasan con paso 26 mm. Sustituir en una futura revisión del modelo el placeholder por el contorno real y añadir sus cuatro soportes a z71,4–73.
2. Para conservar PCB-A, rediseñar el alojamiento/contorno de F en la zona x254–271/y131–171 (680 mm²) o justificar un apilado con volumen de componentes y soldaduras. Mover F completa ≥17 mm a la derecha deja su borde en x352 y excede las paredes; moverla ≥40 mm hacia el frente cambia los faders aprobados. Esos cambios requieren decisión de diseño.
3. Resolver el contacto x231 con la ventana y la invasión de 5 mm de la reserva de placa M4: medir M4 real, cotar Z y conectores. No reducir la reserva ni mover los pads sin aprobación.
4. Cerrar los contornos separados de K y sus zonas inferiores; A se proyecta sobre ambas. Suprimir material por esos solapes sin comprobar Z puede ser innecesario. Verificar también los pasos de montaje de A y el volumen del IDC bajo F/K.
5. Alternativa si no se puede acomodar el contorno existente: futura revisión de la prolongación de A/conectores manteniendo ejes y contrato eléctrico. No recortar la PCB existente a mano: hay cobre y componentes en esa región. No se aplica aquí esa alternativa.
6. Elegir MPN exacto del EC11, cotar la perilla y su carrera, y fijar los tres extremos remotos del mazo antes de liberar fabricación. La orden presente es el primer pedido; las notas históricas que dicen «fabricada» no se toman como evidencia de un ensamble validado.

## Superposición a escala y datos

![Vista superior, X derecha e Y hacia atrás](pcba-fit.svg)

Ver [SVG](pcba-fit.svg) y [datos JSON](pcba-fit.json). El SVG distingue contorno real, placeholder y reservas inferidas, e incluye PCB-A/K izquierda/K derecha/F/P/M, M4, pantalla, torres, agujeros, encoders y cables.

## Validación de entrega

En esta base se ejecutaron **16 tests con el Python de KiCad: 16 PASS**. Con Python estándar: **15 PASS y 1 omitido**, correspondiente al contraste opcional con pcbnew. Incluyen el demo de solapes 680/215 mm², contacto M4, ocho ejes, altura y cruces de mazo. El contraste pcbnew comprueba 37 footprints, caras, rotaciones, posiciones globales de pads numéricos, centros de eje y Edge.Cuts sin guardar la placa. El SVG se comprobó como XML y se inspeccionó visualmente mediante una rasterización de sus primitivas. Los hashes de informe/JSON/SVG son idénticos al regenerar con ambos intérpretes. Las únicas adiciones del parche están en esta carpeta; todos los archivos de entrada conservan sus bytes respecto a 095370c.

## Integridad de entradas (SHA-256)

| Entrada | SHA-256 |
|---|---|
| apps/hardware/kicad/consola/boards/pcb-a/pcb-a.kicad_pcb | `bd7d5a506dc5ddb91306afd424f93be3163be7b67919ffaff12bcb6f7528d0e5` |
| apps/hardware/kicad/consola/boards/pcb-f/pcb-f.kicad_pcb | `64df7f9a75aba538b81824337530efbf579301aba029fd92ee685a34befe7712` |
| apps/hardware/kicad/consola/boards/pcb-m/pcb-m.kicad_pcb | `4ff5bed4e7c0a47de8729e05b4af5282fcdac6d088537d9678fcb038863d1c2b` |
| apps/hardware/kicad/consola/panel/panel-coordinates-console-v1.csv | `eb730926c4030ac611e7a12a20579b518f91da12a3b74c435e4b6e816d091d12` |
| apps/hardware/kicad/consola/panel/pcb-outlines-console-v5.csv | `7597c831d31699c37ce87935cae12d2bd64160cf53acae96bf7daee6cda0b634` |
| apps/hardware/kicad/consola/boards/pcb-k-left/README.md | `483281331f15398e23c75cc19818bb7674815ec895ee049ebfa42c2ab82a010b` |
| apps/hardware/kicad/consola/boards/pcb-k-right/README.md | `0bdfffca6f038b3895a384145956e451c555c2630047ca7a4d2eb13ccf1acd23` |
| apps/hardware/kicad/consola/panel/contornos/pcb-k-contorno-v5.dxf | `d1b93a17188cc775cf311894de2dc84becfd6fbaf01a71bb933b6c67c5fdd76d` |
| apps/design/3d-models/consola/build_console.py | `da3a04608e787a25a64bf7801eae7f36e152055e4d4db6b969eb8466709bc316` |
| apps/docs/consola/25-panel-consola.md | `075cd1171c2e1a89b2f258c8ced92fb42fb83ac16ff018094b938add1f84d16d` |
| apps/docs/consola/28-pcb-k.md | `0a873d29751e51195bfe86e531917787e76796b020077461ef4fac07ea4a35a9` |
| apps/docs/consola/design/claude-design-prompt-consola-cuerpo-v5.md | `171ef385def6027dbf3d661cb0699475f85909f6df6ec7c8c362f306fb4bee53` |
