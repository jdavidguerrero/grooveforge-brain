# PCB-F Console: criterio antes del layout

Se conserva el circuito pasivo verificado en `pcb-f-netlist.xml`: cuatro RS6011 B50K,
100 nF desde cada cursor a GND y 100 nF en la entrada +3V3. No se cambia el esquema.
La constante RC máxima del cursor es (50 kΩ / 4) × 100 nF = 1,25 ms;
el filtrado pertenece al contrato eléctrico existente.

La transformación común con PCB-A es x_panel = x_KiCad + 44,
y_panel = 371 − y_KiCad, en mm. Es una conversión de coordenadas, no una inversión
física: faders arriba (F.Cu), rotación KiCad 90°, ejes largos paralelos a Y.
Se toma el rectángulo y los cuatro M3 Ø3,2 del CSV mecánico v5, sin modificarlo.
Cara superior a TOP−9,5, según `apps/design/3d-models/consola/build_console.py`.

Una reserva de 5 × 65 mm por fader (ranura del panel 3 × 63 + 1 mm por lado)
prohíbe pistas, vías y relleno en ambas capas. No es un fresado de la PCB:
el cursor queda encima de FR4 sin cobre. Los dos NPTH de cada RS6011 se conservan.
El contorno y la huella son nominales; las holguras reales requieren una impresión 1:1.

Pistas de señal de 0,25 mm y alimentación de 0,4 mm; separación mínima de 0,2 mm;
vías Ø0,6 / taladro Ø0,3. Un plano GND inferior y costura al cobre superior dan
retorno a las señales y camino ESD a la carcasa. Cada MP de fader utiliza conexión
sólida, sin thermal, como exige `boards/CONNECTORS.md`. Los MP de JST que el esquema
no conecta se conservan sin red. No se inventa una conexión eléctrica.

El script importa componentes/redes del XML exportado directamente del esquema.
El ruteo determinista busca caminos en dos capas, respeta pads ajenos, taladros,
cobre existente y reservas. DRC independiente de KiCad comprueba la geometría real
tras rellenar. La verificación adicional compara TODOS los orígenes con la tabla
panel, rotación/cara, taladros, redes y límites de huellas/cobre. Esto no demuestra
montaje, bond de panel, contacto de carcasa, orientación del potenciómetro real,
ni la envolvente 3D del cable; se verifican físicamente antes de pedir.

Fuentes: `tools/THEORY-placas.md`, `boards/CONNECTORS.md`, `boards/pcb-f/placement.csv`,
`boards/pcb-f/mechanical.csv`, `panel/pcb-outlines-console-v5.csv`, huella local
`ALPS_RS6011P_60mm` (RS6011SP6004, Mounting Hole Dimensions, vista de montaje),
`build_console.py` (FADER_SLOT y PCB_TOP). La huella local exige comprobar pieza real.

## Resolución mecánica del conector y los tornillos

J1 necesita 6,56 mm de ancho de courtyard perpendicular a la salida. El hueco de
la cara superior entre el borde izquierdo y el courtyard de RV1 es de 5,175 mm:
no cabe ahí sin invadir el fader. Se monta en B.Cu a 90°, origen panel (257,5;160,2),
boca −x. La inversión de cara conserva el pinout: el pin 1 queda en (259,5;157,7),
el pin 6 en (259,5;162,7). Su borde trasero de montaje llega a y=165,1; acercarlo
más al M3 posterior en (257;168) consumiría la reserva de su cabeza Ø5,5.
No se mueve el conector hacia la ranura para ganar espacio.

Los M3 son huellas mecánicas board-only con NPTH Ø3,2. El círculo Ø5,5 de cabeza
se documenta en Cmts.User, sin presentarlo como un componente soldado con courtyard.
La huella estándar reserva una arandela Ø6,4 que sobresaldría de este contorno fijo.
La cabeza Ø5,5 deja 0,25 mm nominal al borde y ≈0,412 mm al rectángulo de carcasa
75 × 9 del fader adyacente. No se certifican tolerancias con esos márgenes.
El courtyard original del RS6011 se conserva completo; su geometría de pads,
taladros y cuerpo no se modifica.

El ajuste ESD se guarda como copia local de la huella RS6011: únicamente añade
conexión sólida en los MP. Así KiCad compara contra una biblioteca coherente con
el montaje, sin editar `boards/lib` ni aplicar exclusiones de DRC.
