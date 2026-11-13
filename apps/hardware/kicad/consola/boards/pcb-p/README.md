# PCB-P — Console, esquema sin layout

Regenerar/verificar desde la raiz: `python3 apps/hardware/kicad/consola/tools/check_netlist.py --board pcb-p --regenerate`.

`placement.csv`: mm de panel, origen y desde el frente, +y hacia atras, vista superior.
Rotaciones CCW KiCad; F = superior, B = inferior. En layout: Y_KiCad = constante - y_panel.
Las filas key/socket con igual referencia son un solo footprint mixto; la fila socket es
solo para ensamble. LED a +4.93 mm del centro de tecla en y_panel. Las otras piezas de
PCB-K se colocan abajo (ver zonas/alturas de 28). Aun no se asignan posiciones de soporte.

`docs/` contiene XML real, PDF y ERC. Ver `../../tools/REPORT-placas.md` para limites,
huellas provisionales y verificacion manual. Los archivos `.kicad_pcb`/`.kicad_dru` que ya
existieran en PCB-F pertenecen a GroovePush: no son un layout de Console ni son fabricables
con este esquema. No se ha generado ni actualizado layout en esta entrega.

PCB-P D43: FSR402 de Juan con dos machos prensados P2.54, ver ../../docs/fotos/fsr402-juan.png.
Huella gf_consola:FSR402_DirectSolder_1x02_P2.54mm, PTH dia1mm, SIN SOCKET.
Datum = centro disco CSV; pads x +/-1.27 y_KiCad +11 (panel disco-11), cola hacia disco.
V1-V4 bajo K derecha; X1/X2 sin K arriba. Luz K-P=1.5mm; envolvente superior COMPLETA <=1mm.
Insertar metal desde arriba, soldar por abajo usando disipador hacia pelicula; no aplicar calor
sobre pelicula ni forzar pliegues. Verificar crimpado/cola/insercion/altura con pieza de Juan.
No comprar/poblar sockets en D43; alternativa P2.54 es compra externa sin LCSC verificado.
Mantener centros de disco y landings. El layout PCB-P DEBE REHACERSE por esta terminacion;
.kicad_pcb pertenece a otra tarea y no se ha editado. Ver doc28 seccion9.
