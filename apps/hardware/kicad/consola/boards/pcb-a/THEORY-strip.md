# PCB-A: criterio previo al layout de tira (2026-11-12)

Cambio mecánico autorizado por Juan: sustituir la L por una tira sin modificar esquema, redes,
referencias ni ejes. Se conserva el contrato de `../CONNECTORS.md` (incluido ESD y DNP R1/R2/J4).
No se usa la indicación histórica «fabricada» como evidencia de un prototipo existente.

La transformación es x_panel=x_KiCad+44, y_panel=371−y_KiCad. Los ejes quedan en y=192,
x=79+26n. Una tira x_KiCad=10…227, y_KiCad=166.4…196 corresponde a panel
x=54…271, y=175…204.6. Pantalla: borde 207.88 → 3.28 mm; reserva M4 termina en 174
→ 1 mm; F termina en 171 → 4 mm. Radio de esquinas 1 mm. La PCB tiene 1.6 mm.

El IDC 2×15 debe girar: su longitud de courtyard 46.76 mm no cabe atravesada en una tira
de 29.6 mm. Se monta abajo con su eje largo paralelo a los encoders. Los pines THT quedan
en la banda frontal, fuera de los cuerpos EC11 y sus pads. El JST de faders sale por la derecha;
los cables M bajan antes de pasar bajo la pantalla. Se validan polilíneas contra prismas,
no solo las rectas entre conectores. Los extremos M/F son hipótesis del informe original;
no se modifica ninguna de esas placas.

Dos capas: señales 0.25 mm, alimentación 0.4 mm, separación ≥0.2 mm y vías 0.6/0.3 mm.
GND en ambas caras con costura y escapes locales. Pads MP de EC11 unidos sólidos, sin térmicos:
el camino ESD del eje al plano debe ser corto; falta comprobar la unión de chapa al plano
físicamente (no convertir agujeros NPTH en una unión eléctrica implícita).

Validación prevista: comparar la lista (referencia,pad,red) y bytes del esquema con edc67a3;
DRC KiCad con zonas rellenadas, cero errores y cero conexiones pendientes; fit de ejes,
reservas y polilíneas; inspección de SVG/Gerber y taladros. El fit usa una envolvente conservadora
en esquinas redondeadas, por lo que no puede convertir un roce en PASS por aproximación.
No valida tolerancias de chapa, esfuerzos, cuerpo real del EC11, altura de conectores ni radios
de curvatura. Esas comprobaciones corresponden al primer montaje.

Referencias locales: `tools/fit/THEORY.md`, informe original de fit en edc67a3,
`../CONNECTORS.md`, panel CSV Console v1 y `build_console.py`. Referencias de método:
manual KiCad PCB Editor (netclasses, DRC, zonas y exportación Specctra),
datasheet Microchip MCP23017/MCP23S17 (desacoplo y dirección A0/A1/A2),
y dibujo de huella EC11 utilizado. No se cambia el circuito del MCP.

## Cierre de ruteo y montaje (2026-11-13)

La inspección DRC obliga a separar el courtyard del IDC de las patas metálicas pasantes,
aunque el cuerpo del encoder esté en la cara opuesta. J1 se desplaza 0,8 mm en Y KiCad
(origen final 207,78;192,07) conservando la tira y todos los ejes. Su marca exterior de
pin 1 se adapta en una biblioteca local; el cobre y el volumen de montaje no cambian.
R2 pasa a (144,8;169) para despejar el MP y el courtyard de J2. H4 pasa al hueco entre
E7/E8, (204;179) KiCad = (248;192) panel: el taladro deja de interferir con RE8.
Estos movimientos exigen rerutear el fan-out; desplazar solo los extremos de pistas
existentes no preserva la separación al resto de pads del IDC.

Las pistas explícitas de GND también se dimensionan a 0,4 mm. Los mínimos por red se
expresan en `pcb-a.kicad_dru`, además del mínimo global de 0,2 mm. El relleno usa
0,3 mm de separación y conexiones MP sólidas. La grilla de cierre se afina a 0,05 mm
con margen de 0,21 mm para poder salir de SSOP sin bajar el ancho de alimentación.
El DRC es quien decide la aceptación de esas rutas: la grilla es una búsqueda aproximada.

En macOS, KiCad 10.0.6 puede abortar al crear marcadores DRC por una llamada de UI en
`BOARD_COMMIT::Push`. El diagnóstico SWIG `audit_copper` evita esa creación, pero el
chequeo de bibliotecas del motor requiere un `PGM_BASE` ausente en Python independiente.
Por ello el diagnóstico conserva las netclasses reales y desacopla temporalmente el
proyecto en memoria; no guarda la placa ni modifica severidades. Sus resultados no
certifican bibliotecas ni variables de hoja. La entrega exige un DRC CLI completo
posterior, sobre el proyecto real y con zonas previamente rellenadas. No se convierten
abortos, reports antiguos ni configuraciones recortadas en PASS.

Referencia de implementación: [KiCad 10.0.6, JobExportDrc](https://github.com/KiCad/kicad/blob/10.0.6/pcbnew/pcbnew_jobs_handler.cpp)
y [WriteDRCReport](https://github.com/KiCad/kicad/blob/10.0.6/pcbnew/python/scripting/pcbnew_scripting_helpers.cpp).
