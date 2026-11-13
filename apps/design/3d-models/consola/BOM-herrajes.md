# BOM de herrajes — GrooveForge Console

2026-11-13 · **Un equipo**, sin repuestos ni múltiplos comerciales. Base `0ef531f` + corrección de anclajes PCB-A.
Las cantidades de la tabla son totales por partida; las alternativas sustituyen partidas, no se suman.
Los cuatro espárragos del marco M4 son la decisión nueva de Juan de esta revisión; sus posiciones y el marco
siguen por definir. Esta BOM permite cotizar; no libera esos cuatro agujeros para corte.

Fuentes locales: [CSV de contornos y notas](../../../hardware/kicad/consola/panel/pcb-outlines-console-v5.csv),
[FABRICACION](FABRICACION.md), [doc28 §2](../../../docs/consola/28-pcb-k.md#2-mecánica),
[doc25 §7](../../../docs/consola/25-panel-consola.md#7-distribución-v1-juan-2026-11-11--fijada)
y [generador CAD](build_console.py) (`pem_holes`, `PCB_TOP`, `TOWER_TOP`, `body_details`, `SEAM_BOSSES`).
No se cuentan las piezas electrónicas soldadas como herrajes.

## Lista para cotizar

Todas las roscas métricas: M2,5×0,45 y M3×0,5. Acero zincado o inoxidable compatible con el aluminio;
no mezclar tuercas autoblocantes con pares de apriete que superen la retención del clinchado.
«Altura» de un separador significa **proyección útil desde la cara inferior del aluminio hasta el apoyo**.
Confirmar con el dibujo del proveedor si su longitud comercial incluye la chapa de 2 mm; no comprar por el
sufijo de longitud sin cotejar este datum. Las longitudes de tornillo se miden bajo cabeza, salvo avellanados.

| Partida | Cant. total | Destino / especificación de compra |
|---|---:|---|
| Espárrago autoinsertable PEM FH-M2.5 | 14 | PCB-K (izq. + der.), uno por agujero del CSV. Propuesta inicial de proyección roscada útil 8 mm, a cotejar con el datum comercial |
| Separador tubular aislante M2.5, **1,4 mm** | 14 | Entre chapa y cara superior K; longitud especial, interior ≥2,7 y exterior compatible con el espacio libre |
| Tuerca M2.5 | 14 | Retención de K por debajo; espesor supuesto 2 mm para elegir el espárrago |
| Espárrago FH-M2.5 **más largo**, marco NeoTrellis M4 | 4 | Nuevos; fuera de la ventana M4 122×62. Añadir ≈3 mm de marco al presupuesto del espárrago K; objetivo útil ≥11 mm, proponer 12 mm. No usar los de 8 mm sin medir |
| Tuerca M2.5 para marco M4 | 4 | Una por nuevo espárrago; incluida en total de tuercas de abajo |
| Separador autoinsertable BSO-M2.5, **4,5 mm útiles** | 10 | PCB-P; tornillo desde abajo, agujero PCB Ø2,7; pasa por K donde se solapan, requiere paso libre |
| Tornillo M2.5×6, cabeza cilíndrica baja | 10 | PCB-P. Propuesta: 6−1,6=4,4 mm de entrada sin arandela; validar fondo ciego del separador elegido |
| Separador BSO-M3, **7,5 mm útiles** | 4 | PCB-F, centros (257,91), (332,91), (257,168), (332,168) |
| Tornillo M3×6, cabeza cilíndrica baja | 8 | 4 PCB-F + 4 PCB-A; propuesta, entrada 6−1,6=4,4 mm sin arandela; verificar profundidad roscada y volumen de cabeza |
| Separador BSO-M3, **18 mm útiles** | 4 | PCB-A H1–H4; reemplaza la anotación antigua de 10. Cuerpo dentro de envolvente Ø5,6 para conservar el fit |
| Separador BSO-M3, **10 mm útiles** | 9 | Panel a las nueve torres del cuerpo; no confundir con los de A |
| Tornillo M3×10 | 9 | Desde abajo por los pozos Ø6,5 de las torres; el pozo deja un techo nominal de 6 mm: 10−6=4 mm de entrada |
| Inserto térmico M3, exterior Ø4, largo 6 | 6 | Torres PCB-M; taladro del CAD Ø4×6 desde z=3. Verificar diámetro recomendado del inserto real en cupón PETG |
| Tornillo M3×8 | 6 | PCB-M, desde arriba; entrada 8−1,6=6,4 mm nominal. Inserto Ø4×6 pasante: medir salida y no topar contra el piso; usar arandela 0,5 si hace falta limitar a 5,9 mm |
| Tornillo M3×16 | 15 | Uniones de las cuatro piezas PETG; `SEAM_BOSSES` tiene 15 entradas. Paso por mitad del tope 8 mm: 16−8=8 mm hacia la mitad receptora, comprobar salida del inserto de 6 |
| Inserto térmico M3, exterior Ø4, largo 6 | 15 | Receptores de las 15 juntas. Misma familia que los 6 de PCB-M: **21 insertos en total** |
| Pata de goma autoadhesiva | 4 | Propuesta de compra: Ø15–20, altura 3–5 mm, una por esquina; confirmar apoyo fuera de pozos y juntas. No requiere perno; CAD aún sin patas |

El tornillo se acepta por entrada de rosca y acceso, no solo por longitud nominal: comprobar en cupón
que ni el M3×8 ni el M3×16 hacen fondo o expulsan un inserto. Si se añaden arandelas, restar su espesor de la entrada.
No hay tornillos extra para las ocho carcasas EC11 de A: quedan soldados a la PCB y el casquillo está bajo la chapa.

## Totales consolidados

| Ítem agrupado | Total por Console |
|---|---:|
| FH-M2.5 | **18** = 14 K + 4 M4; dos longitudes, 8 y 12 útiles propuestas |
| Tuercas M2.5 | **18** = 14 K + 4 marco M4 |
| Separadores tubulares M2.5 de 1,4 | **14** |
| BSO-M2.5 de 4,5 útiles | **10** |
| BSO-M3 de 7,5 / 18 / 10 útiles | **4 / 4 / 9** = 17 |
| Autoinsertables en la chapa | **45** = 18 FH + 27 BSO; **41 ubicados en CAD/CSV**, 4 M4 pendientes |
| Tornillos M2.5×6 | **10** |
| Tornillos M3×6 / ×8 / ×10 / ×16 | **8 / 6 / 9 / 15** = **38 M3** |
| Tornillos métricos sueltos, partidas de arriba | **48** = 10 M2.5 + 38 M3; no incluye alternativa avellanada |
| Insertos térmicos M3 Ø4×6 | **21** = 6 PCB-M + 15 juntas |
| Patas de goma adhesivas | **4**, cantidad propuesta |

Arandelas no incluidas en el montaje nominal: según alternativa se requieren 14 calces de K,
10 de P, 4 de F o 6 arandelas M3 de 0,5 para PCB-M. Elegir la alternativa antes de sumar al pedido.

## Alturas especiales y alternativas estándar

TOP=83+2=**85**. Cara inferior chapa=**83**. Placas FR4 de 1,6 mm.
Para placas suspendidas, `z_superior = 83 − altura_útil_separador − calce_bajo_separador`.
Una arandela añadida bajo un separador **baja aún más** la placa; no compensa uno demasiado largo.
Las medidas comerciales propuestas son familias comunes, sujetas al catálogo del proveedor.

| Conjunto | Apilado nominal | Alternativa con altura estándar y efecto exacto |
|---|---|---|
| K: calce **1,4 especial** | 83−1,4=**81,6**; inferior=80; equivale a TOP−3,4 | Siguiente 1,5: 83−1,5=81,5, baja **0,1 mm**. Cambia asiento de Choc y debe validarse. Para mantener 81,6: tubular 1,0 + calce impreso/laminado 0,4, **14 de cada uno**; una arandela de 0,5 daría 1,5, no 1,4 |
| P: BSO **4,5 especial** | 83−4,5=**78,5**=TOP−6,5; luz a K inferior=80−78,5=**1,5** | Siguiente altura útil 5: 83−5=78, baja **0,5**, luz K/P sube a 2. El FSR queda 0,5 más abajo: calce superior de sensor 0,5 (6 sensores), o aceptar/revalidar ese cambio de altura. Para conservar la PCB: separador 4 + calce bajo apoyo 0,5 (**10**), si existe familia adecuada |
| F: BSO **7,5 especial** | 83−7,5=**75,5**=TOP−9,5 | Siguiente altura útil 8: 83−8=75, baja **0,5**. Sube el presupuesto necesario de vástago sobre panel en 0,5; validar RS6011/perilla. Para mantener 75,5: separador útil 7 + calce 0,5 (**4**), o 6 + calce impreso rígido 1,5 si 7 no existe |
| A: BSO **18** | 83−18=**65**=TOP−20; inferior=63,4; EC11 hasta 65+28=93, sobresale **8** | No sustituir por 10 (daría z73 y 16 mm salientes). Mantener apoyo z65 y cotejar longitud comercial con el dibujo |
| Torres panel/cuerpo | 83−10=**73**=`TOWER_TOP` | Mantener 10 útiles; cambiarlo exige cambiar altura de torre, no sumar arandelas a ciegas |
| PCB-M sobre piso | piso3 + torre5=**8**, superior PCB=**9,6** | El inserto no define la altura de la torre. No añadir separadores de 5 comerciales además de los pilares impresos |

**Advertencia de nomenclatura:** el catálogo PEM BSO estándar consultado lista M3 y mayores;
no da una referencia BSO-M2.5 en esa tabla. «BSO-M2.5» y las alturas útiles 4,5/7,5 de este proyecto
son requisitos funcionales para cotizar equivalentes; no son códigos PEM verificados. Para P puede
hacer falta otra familia autoinsertable de rosca M2.5 o un separador roscado con tornillo avellanado.
No autorizar sustituciones sin comprobar diámetro exterior, rosca, proyección y longitud de entrada.

Para los espárragos K, presupuesto útil desde abajo de chapa: calce1,4 + PCB1,6 + tuerca2 + salida de
rosca1 = **6 mm**; propuesta 8 deja 2 mm para tolerancia/acceso. Marco M4 con ≈3 adicionales:
8+3≈**11**, siguiente propuesta **12**. Son presupuestos de compra; el marco real fijará el largo definitivo.
No se impone aquí una altura de montaje de la placa NeoTrellis aún sin medir.

## Montaje de A y comprobación de paso por K

Centros de panel H1=(84,179), H2=(118,192), H3=(144,192), H4=(248,192). Taladros de PCB-A Ø3,2 NPTH;
la chapa lleva Ø4,2 nominal. [Chequeo reproducible](../../../hardware/kicad/consola/tools/fit/REPORT-pcba-mounting.md):
puentes láser mínimos conservadores **4,2 / 7,35 / 7,35 / 7,35 mm** (requisito ≥1,5).
Separadores z65…83 frente a K z80…81,6: atraviesan su intervalo Z, pero **ninguno atraviesa material de K**
en los contornos especificados. Envolvente de cuerpo Ø5,6; H1 deja **1,2 mm** al borde K.
Se usaron el contorno conjunto v5 y particiones izq./der. de doc28 §7; en esta base no hay layouts de K
izq./der. para certificar. Revalidar al completarlos y contra el cuerpo del proveedor final.

## Compra y proceso de chapa

PEM es difícil de conseguir en Bogotá según el contexto de compra del proyecto; no se ha confirmado stock local.
Cotizar en AliExpress equivalentes **self-clinching** (espárrago enrasado FH y separador ciego BSO), con dibujo,
material, tolerancias y muestra. Un separador hexagonal ordinario no se autoinserta en aluminio.

**Alternativa sin PEM:** tornillo avellanado desde la cara visible, separador hembra roscado por debajo y tornillo
inferior para cada PCB. Para K/M4, perno avellanado pasante + calce + tuerca. Sustituiría hasta **45 autoinsertables**
por **45 tornillos superiores: 28 M2.5 +17 M3**, más sus separadores/tuercas; Los 27 tornillos inferiores P/F/A/torres se conservan.
Longitud superior según profundidad de rosca del separador y avellanado; no fijada. Exige redibujar agujeros,
avellanar con profundidad controlada en chapa de 2 mm, revisar resistencia y aplicar otra vez el chequeo de
puentes y contornos. El corte láser actual no incluye avellanados y esta alternativa no está liberada.

Preferir **aluminio 5052** de temple compatible con clinchado frente a **6061-T6**: hace falta deformación plástica
local y dureza menor que la del inserto. La selección final depende de familia/material del herraje y dureza
del lote. [PEM, guía de acabados](https://www.pemnet.com/eu/wp-content/uploads/sites/10/2022/06/Surface_Finish.pdf)
describe las limitaciones de 6061-T6. Para tornillería de acero/inox en aluminio, **prensar después del anodizado**,
con punzón/yunque paralelos, protección cosmética y cupón del mismo lote; no a martillazos.
[PEM, instrucciones de instalación](https://www.pemnet.com/wp-content/uploads/sites/2/2022/06/index-quick-product-locator.pdf).

El Ø4,2 del generador es nominal del proyecto. **PEM BSO-M3 original exige taladro 4,22 +0,08/−0 mm**, cuerpo
hexagonal nominal 4,8 entre caras y centro a borde mínimo 6 mm; una web láser de 1,5 por sí sola no certifica
el clinchado. [Ficha dimensional PEM BSO-M3](https://www.pemnet.com/products/product-finder/bso-m3-16zi/).
Elegir proveedor y ajustar el cupón/corte a su taladro antes de fabricar; no usar Ø4,2 como tolerancia de aceptación.
Para FH y equivalentes M2.5, verificar también diámetro y espesor mínimo en su propia ficha. Preservar el apoyo
plano alrededor del agujero y no desbarbar por rutina la zona de retención. Es una fijación mecánica, no una
unión eléctrica garantizada de GND a la chapa anodizada.

## Por definir

Estas cantidades son **adicionales** a los totales anteriores y quedan sin asignar hasta medir/diseñar su soporte.

| Conjunto | Falta resolver |
|---|---|
| Módulo de pantalla Waveshare 7″ | Puntos/roscas reales, fijación bajo aluminio y apoyo del vidrio en z83; espaciadores y tornillos no dimensionados |
| Bahía Pi 5 | Soporte de Pi/HAT, tapa de bahía abierta, tuercas/insertos y acceso a USB/DSI; el placeholder no es un montaje |
| Sharp GP2Y0A41 ×2 | Dos brackets, taladros del sensor, fijación al panel/cuerpo y largo de tornillo; la ventana IR no es un soporte |
| Encoder jog | Rosca/tuerca/arandela del casquillo, antirotación, espesor disponible y sujeción de perilla; confirmar piezas incluidas con el EC11 de Juan |
| Marco impreso M4 | Geometría y espesor ≈3 mm, ubicación de los **4 FH largos fuera de la ventana**, rigidez, apoyo PCB/elastómero; 4 FH y 4 tuercas ya contados, no duplicar. No hay todavía cuatro cortes extra en CAD |
| Puntos para bridas | Ubicación, cantidad y sujeción adhesiva o atornillada; radios y alivio de tensión de IDC/JST/DSI/USB |
| Pots, selector y jacks de pared | Inventariar lo incluido: 3 pots → 3 tuercas + 3 arandelas si no vienen; selector → soporte según pieza. Jacks del pedido: 7×6,35 +3×3,5 → hasta 10 tuercas/arandelas, confirmar cuáles se montan (MIDI sin poblar). No recomprar accesorios incluidos |

No se inventan taladros de pantalla, Pi, jog o M4 a partir de su ventana visible. Tampoco se consideran
resueltas estas fijaciones por tener un volumen placeholder en FreeCAD.
