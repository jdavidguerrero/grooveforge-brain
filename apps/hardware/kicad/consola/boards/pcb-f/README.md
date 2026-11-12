# PCB-F — Console, esquema sin layout

Regenerar/verificar desde la raiz: `python3 apps/hardware/kicad/consola/tools/check_netlist.py --board pcb-f --regenerate`.

`placement.csv`: mm de panel, origen y desde el frente, +y hacia atras, vista superior.
Rotaciones CCW KiCad; F = superior, B = inferior. En layout: Y_KiCad = constante - y_panel.
Las filas key/socket con igual referencia son un solo footprint mixto; la fila socket es
solo para ensamble. LED a +4.93 mm del centro de tecla en y_panel. Las otras piezas de
PCB-K se colocan abajo (ver zonas/alturas de 28). Aun no se asignan posiciones de soporte.

`docs/` contiene XML real, PDF y ERC. Ver `../../tools/REPORT-placas.md` para limites,
huellas provisionales y verificacion manual. Los archivos `.kicad_pcb`/`.kicad_dru` que ya
existieran en PCB-F pertenecen a GroovePush: no son un layout de Console ni son fabricables
con este esquema. No se ha generado ni actualizado layout en esta entrega.
