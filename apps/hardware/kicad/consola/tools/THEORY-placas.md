# PCB-K partida, PCB-F y PCB-P: contrato antes del esquema

El esquema representa conexiones, no posiciones físicas. Se declara cada pin del MCP23017
con su número real de SOIC-28; GPA7/GPB7 y los NC del encapsulado quedan NC. Las entradas
libres GPA0–6/GPB0–6 tienen pads, igual que INTA/INTB. Las teclas cierran a GND y dependen
de los pull-ups internos habilitados por firmware. A0/A1/A2 fijan 0x21/0x22/0x23; RESET
usa 10 kohm y cada encapsulado lleva 100 nF + 1 uF. Las resistencias I2C se dibujan DNP:
solo PCB-M debe poblar pull-ups. DNP no significa desconectado en el netlist.

La cadena óptica es una interfaz serie: DOUT del LED i alimenta DIN del i+1, con 33 ohm
antes del primer DIN de cada placa, no en alimentación. El cable 6 vías transporta
DOUT de NEW a la resistencia de PLAY. Cada LED lleva 100 nF y cada placa 22 uF.
El orden nuevo indicado por Juan sustituye los índices antiguos de 28 §4/§6 y 27;
se conserva el mapa de entradas de 28 §4. No se cambia firmware en esta entrega.

PCB-F conserva el contrato de GroovePush: extremos del potenciómetro a +3V3/GND,
cursor a FADERn, carcasa MP a GND y 100 nF por cursor (más 100 nF de desacoplo de entrada,
como la captura anterior). PCB-P es una resistencia de dos terminales por sensor,
entre el pin 1 (+3V3) y el pin 2 (señal) de su propio cable; divisores, filtrado y ADC
pertenecen a PCB-M. No se unen las seis alimentaciones en PCB-P: cada cable es independiente.

Los CSV de colocación usan x hacia la derecha, y desde el frente hacia atrás, mm,
vista superior. Los archivos KiCad de huella usan Y hacia abajo: (dx,dy) de panel
se traduce a (dx,-dy) en la huella. La tecla está arriba y el zócalo/LED abajo;
el zócalo está integrado en la misma huella SW, sin un segundo componente eléctrico.
La fila de zócalo en placement.csv es una referencia de ensamble al mismo SW, no
un segundo footprint a colocar. El LED tiene su propio recorte Edge.Cuts; la huella
SW solo marca la ventana en Dwgs.User para evitar dos recortes coincidentes.

Antes de código/tests, qué se valida: la comparación del XML exportado con KiCad
comprueba todas las particiones de red y NC. Tests independientes fijan el mapa §4,
los conectores, direcciones y la cadena 0–25; una mutación del esquema debe fallar.
Los tests geométricos inspeccionan pads, taladros, capas y las coordenadas del CSV,
y regenerar dos veces debe dar exactamente el mismo esquema. ERC debe dar cero
errores; los avisos se conservan. No se prueba layout, tolerancias, corriente real,
rebote, bus físico, ni ajuste de zócalos/colas/faders. Hace falta cupón 1:1 y piezas reales.

Fuentes: `apps/docs/consola/28-pcb-k.md` completo; `boards/CONNECTORS.md`;
`panel/panel-coordinates-console-v1.csv`; `panel/pcb-outlines-console-v5.csv`;
huellas GroovePush `boards/lib/groovepush.pretty`; símbolos/huella LED KiCad 10.0.6.
Kailh CPG135001S30, plano KHA-PG1350-005EN / KH-PS-1702-35 Rev B:
https://www.kailhswitch.com/Content/upload/pdf/202115927/CPG135001S30-data-sheet.pdf
Interlink 402 (varias opciones de cola): https://www.interlinkelectronics.com/fsr-402
No se considera el patrón provisional de pads FSR o zócalo una cota certificada.
