# PCB-K izquierda y derecha: criterio previo al layout

Fuentes: docs/consola/27 y 28 completos, placement.csv de ambas placas,
contorno v5 y HARNESS.md en el mismo árbol. No se cambian los esquemas.

La transformación es x_panel=x_KiCad+44; y_panel=371-y_KiCad.
La partición elimina el puente bajo la M4: izquierda x=6..108,
y=104..320; derecha x=108..252, y=28..180. La ventana de la M4
queda abierta en el borde de ambas piezas, evitando una tira de FR4 de 1 mm.
Se conserva el escalón y=221 y las cuatro ventanas FSR de 20 mm.

Las teclas mantienen su huella doble original: los pads PTH y SMD con el
mismo número necesitan cobre entre sí. Los LEDs se montan en B, su centro
está 4,93 mm detrás de cada tecla; el recorte forma parte de Dn, no de SWn.
El plano GND ofrece retorno a los datos de 800 kHz y desacoplo local. Cada
100 nF se sitúa junto al VDD de su LED. La alimentación de LEDs se enruta
con 0,8 mm, señales 0,25 mm y escapes SSOP, si hicieran falta, de 0,15 mm.
No se reduce la separación de 0,2 mm para forzar un resultado DRC.

La validación debe comparar pads/redes con XML (también pads duplicados),
posiciones/cara con placement, cadena con doc27, agujeros con v5 y DRC
real de KiCad. DRC no mide la pieza física, altura del HAT ni el encaje del
zócalo provisional. La superposición del margen de la M4 con los zócalos
adyacentes necesita resolución mecánica explícita antes de fabricar.

Referencias de diseño: Microchip MCP23017 DS20001952 (desacoplo y pines),
OPSCO SK6812MINI-EA B/2 pp.4–5 (pinout en doc28), Kailh CPG135301D02
(cotas transcritas en doc28); JLC restricciones fijadas por esta tarea.
