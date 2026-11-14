# Método de verificación de PCB-A antes del primer pedido

Este análisis es de lectura: no transforma ni guarda ninguna placa, esquema o layout.
La especificación de esta auditoría es la petición de encaje de PCB-A sobre panel v1.

## Sistemas de coordenadas y ajuste

El panel usa X a la derecha, Y desde el frente hacia atrás y Z hacia arriba desde
la base del cuerpo. KiCad dibuja Y hacia abajo. El README de PCB-K y
`schgen/console_artifacts.py` fijan `Y_KiCad = constante - y_panel`.
Primero se convierte `(x, y)` de KiCad a `(x, -y)`; después se ajusta una
rotación rígida y una traslación por mínimos cuadrados sobre los ocho ejes.
Esa inversión de Y es un cambio de coordenadas, no una propuesta de voltear la
placa: los EC11 de F.Cu siguen mirando arriba. Ajustar sin ella puede poner la
prolongación de conectores al lado equivocado de la fila. Se calcula también ese
caso como diagnóstico, sin certificarlo como montaje alternativo.

El origen de un footprint no tiene por qué coincidir con el eje. En estos EC11
el círculo de eje Ø6 en F.SilkS y F.Fab da el centro local. Se lee de la placa,
se aplica la rotación KiCad del footprint y luego el ajuste global. No se permite
escalar una placa para ocultar un paso incorrecto. Cada residuo debe ser ≤0,1 mm
y cada paso nominal entre encoders debe ser 26 mm (tolerancia numérica 0,000001 mm).
La tolerancia de registro de 0,1 mm no autoriza un paso nominal distinto.

## Planta y altura

Los contornos ortogonales se dividen en rectángulos interiores disjuntos; cada
uno se transforma y se recorta contra los rectángulos del obstáculo con
Sutherland–Hodgman. Se suman áreas de intersección, se calcula la distancia mínima
entre bordes y se detecta contacto incluso con área cero. Esto respeta la L de
PCB-A y el contorno no convexo de PCB-K; sus cajas envolventes darían falsos choques.
Los pozos se comprueban como círculos de radio 5 mm, no cuadrados. No se discretizan
las dimensiones de placas. Los futuros contornos curvos/no ortogonales se rechazan
explícitamente para que no pasen por una aproximación silenciosa.

Un solape en XY no demuestra colisión 3D: las placas tienen alturas diferentes.
Se contrastan los intervalos Z de los sustratos y el espacio disponible para
componentes, soldaduras y mazos. Sin cota real de M4, referencia exacta de EC11,
modelo de perilla y posiciones de conectores de PCB-M/F no existe certificación
completa. Los extremos de cable estimados, el margen M4 de doc28 y las hipótesis
sobre longitud de eje se muestran separados de las medidas del PCB.

## Qué se valida antes de entregar

Tests con figuras conocidas verifican áreas, contacto, distancias, orientación
KiCad, ejes desplazados y detección de errores de paso. La extracción se contrasta
con pcbnew en modo lectura; regenerar dos veces debe producir los mismos bytes.
El SVG muestra coordenadas de panel (+Y hacia arriba) y todas las placas, torres,
ejes, conectores y recorridos estimados. No cubre DRC, tolerancias de fabricación,
flexión, aislamiento eléctrico ni validación con piezas físicas.

Fuentes locales: panel v1; CSV v5; `build_console.py`; doc25 §7; doc28 §2/§7;
README de PCB-K; PCB-A y PCB-F históricos; PCB-M (sin layout).
Referencia geométrica: Sutherland y Hodgman, *Reentrant Polygon Clipping*,
Communications of the ACM 17(1), 1974, DOI:10.1145/360767.360802.
Referencia de alturas: catálogo oficial Alps Alpine EC11E, dibujos 2/3:
https://tech.alpsalpine.com/cms.media/product_catalog_ec_01_ec11e_en_611f078659.pdf
La huella H20 no identifica por sí sola la variante realmente comprada.
