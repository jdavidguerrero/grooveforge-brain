# PCB-P: teoría y especificación previa al layout

2026-11-13. Diseño de ranura y socket inferior autorizado por el propietario en el encargo de layout.
Sustituye la terminación directa D43 de doc28 §9. No cambia los seis circuitos independientes del esquema.

## Apilado y recorrido de la cola

Discos Ø18,3 en los centros del CSV v5, sobre FR4 de 1,6 mm. Cara superior P = TOP−6,5;
cara inferior K = TOP−5,0: luz superior 1,5 mm. Ningún conector se monta arriba. Recortar terminales y soldadura superiores a ≤0,8 mm
(la pata original de 3,2 mm sobresaldría 1,6 mm): deja 0,7 mm nominales frente a K.
Socket vertical inferior HCTL PM254-1-02-Z-8.5, P2,54, cuerpo de 8,5 mm, abertura hacia abajo.
La cola debe entrar hacia arriba: un recorrido con sólo una bajada no puede acoplarse.
Se usa un recorrido de tres arcos de 90°, radio de línea media R=2,5 mm, debajo de P.
No plegar ni marcar la película. Los terminales metálicos se insertan 3 mm nominales.

Hipótesis dimensional explícita: L=37,4 mm desde el borde del disco hasta la punta metálica;
no desde el centro. Espesor de película nominal 0,46 mm; eje a z=+0,23 sobre P.
La boca está a z=−1,6−8,5=−10,1. Se reserva un tramo vertical final de R=2,5 mm
antes de la boca. La horizontal inferior queda en z=−15,1; el tramo vertical de bajada,
entre los dos primeros arcos, mide V=1,6+8,5+0,23=10,33 mm.

Con A=1 mm recto tras el borde y B la horizontal inferior:

- L = A + B + 3πR/2 + V + R + inserción.
- B = 37,4 − 1 − 11,780972 − 10,33 − 2,5 − 3 = 8,789028 mm.
- Desplazamiento borde→eje socket = A+B+3R = 17,289028 mm.
- Desplazamiento centro disco→eje socket D = 9,15+17,289028 = **26,439028 mm**.
- Envolvente inferior de película ≈15,33 mm bajo la cara superior P (TOP−21,83).

La cola apunta a +x girada +25° en V1–V3 y X1–X2. V4 apunta a −x girada hacia −y
(205°), separando su socket del de V3. Esto evita soldaduras bajo el disco vecino.
Ranura oblonga NPTH de **3,0 × 8,8 mm**, eje largo transversal a la cola;
centro a 12,3 mm del disco sobre el eje de cola. Luz transversal nominal 8,8−7,62=1,18 mm.
R=1,5 en extremos fresados, sin metalización ni pads. El primer arco atraviesa el espesor
dentro de la ranura, sin doblar la película contra el canto. Proteger/desbarbar el canto.

La longitud real del metal crimpado y su profundidad de inserción son tolerancias de ensamble:
validar el recorrido en el SVG de sección y con un sensor del lote antes de pedir placas.
La geometría no certifica un lote de sensor distinto. Si 37,4 se midió desde el centro,
se debe recalcular D restando 9,15 antes de liberar fabricación.

## Circuito y fabricación

Se conservan J1–J6 JST-PH horizontales de 2 vías del esquema y seis cables 1:1 a PCB-M;
la frase «un JST» del doc25 es anterior al contrato eléctrico actual. Confirmado por el propietario: seis JST-PH de dos vías, sin cambios al mazo ni esquema.
No unir las seis alimentaciones ni añadir GND ficticio: la placa no tiene red GND.
Por ello no hay plano de masa. Pistas ≥0,25 mm; separación ≥0,20 mm; cobre a corte ≥0,30 mm;
vías, si hicieran falta, Ø0,60/taladro 0,30. Dos capas, FR4 1,6 mm, 1 oz, ensamblaje manual.

Los diez agujeros NPTH Ø2,7 se mantienen en x=40/89,5/139/170/201, y=72 y 97.
Reservar radio 2,5 mm sin cobre alrededor para tornillo/separador M2.5.

## Validación observable

El script independiente de verificación lee el CSV y XML recién exportado: centros y agujeros,
capas, posiciones calculadas, ausencia de pads bajo los discos, cobre fuera de cortes y
particiones eléctricas. KiCad DRC con paridad de esquema es el cierre de conectividad.
Se conserva el esquema eléctrico y su BOM de sensores aportados por Juan. Los identificadores
FSR1–FSR6 son los landings de sockets en el PCB; los discos son dibujos de montaje. La huella
local sustituye la geometría D43 reteniendo su identificador para paridad. La compra adicional
de seis unidades figura en assembly-sockets.csv y también se valida desde check_bom. Se conserva el mazo. El CAD no valida fuerza, velocity, contacto crimpado ni fatiga de película.

## Referencias

- Interlink Electronics, *FSR Integration Guide*, §9: superficie plana, no marcar la cola,
  radio sugerido mínimo de aproximadamente 2,5 mm para piezas de evaluación.
  https://www.interlinkelectronics.com/integration-guides
- HCTL PM254-1-02-Z-8.5: https://www.lcsc.com/product-detail/C2897365.html
- CSV v5 y doc28 §2/§9; nueva instrucción del propietario para terminación inferior.

## Contrato de esquema preservado

El esquema y generador se conservan byte por byte respecto a 0ef531f. FSR1–FSR6 representan
cada conjunto sensor + terminación; sus dos pads físicos son el socket inferior, sin símbolo XS adicional.
La biblioteca LOCAL de PCB-P conserva el identificador histórico
`gf_consola:FSR402_DirectSolder_1x02_P2.54mm` para mantener paridad, pero su geometría es
la del socket 1x2. El nombre histórico no prescribe montaje: NO soldar el sensor directamente.
No se altera la biblioteca compartida ni otras placas. La referencia física FSRn marca el socket
correspondiente. El datum de esa huella es pin 1; el disco tiene su propio datum CSV, dibujado en Dwgs.User.
La compra complementaria de sockets se declara en `assembly-sockets.csv` y el pedido LCSC;
la BOM eléctrica exportada queda intacta. El verificador de layout valida también esta compra con
`check_bom.check_components` y exige 6 unidades más repuestos en el pedido.


La planta de montaje añade regla de control de 100 mm y la sección muestra los tres radios de
curvatura a escala 1:1. La serigrafía queda fuera del apoyo de los discos para evitar relieves.
El test de entrega comprueba hashes SHA-256 de la placa, entradas y archivos exportados; detecta
Gerbers/STEP/DRC obsoletos tras editar el PCB. No sustituye la inspección mecánica física.
