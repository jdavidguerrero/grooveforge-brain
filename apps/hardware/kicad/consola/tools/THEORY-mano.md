# Ensamble manual de PCB-K y conservación de PCB-A

2026-11-12. Contrato previo al código: pedido-v1 opción B y tarea de Juan.
JLC ensambla PCB-M; PCB-K izquierda/derecha y PCB-A se sueldan a mano.

## Huella dual y separación

Un mismo número de pad une lógicamente el contacto SMD y el taladro metalizado;
son dos terminaciones alternativas del mismo interruptor. El layout debe rutear
ambos pads: repetir el número no crea una pista de cobre. Las patas están en
(0,-5.90) y (5,-3.80) en panel; KiCad invierte Y. Taladro 1.30 y pad circular
1.90 dejan (1.90-1.30)/2 = 0.30 mm de anillo, mayor que 0.15 mm solicitado.
Los pads SMD existentes de 2.60 de ancho están a 3.275 mm de su taladro:
3.275-1.30-0.95 = 1.025 mm de cobre libre, mayor que 0.20 mm.
No se desplazan ni recortan pads SMD. También se comprobarán todos los pares
PTH/SMD con una cota conservadora círculo-rectángulo y el espacio al poste central.
El patrón del zócalo sigue siendo provisional y requiere cupón físico; el taladro
1.3 permite la alternativa soldada, pero no certifica el ajuste del cuerpo/plástico
del zócalo en esos agujeros más pequeños. No ampliar sin revisar la alternativa dual.
Se conservan postes NPTH 5.0 y 1.5 y centro de ventana LED (0,+4.93).

## Selección de piezas

Pasivos PCB-K: 0805 para acceso con cautín, salvo 22 uF 1206. Los campos LCSC
son datos de compra, y deben concordar con el encapsulado: C506653 es MCP23017-E/SS,
SSOP-28 5.3x10.2 paso 0.65, no SOIC-28W. Se conserva numeración y redes.
LED C5378731: hoja OPSCO SK6812MINI-EA-000 B/2, 2026-07-23, pp.4–5:
cuerpo 3.2x2.8x1.78; 1 GND, 2 DIN, 3 VDD, 4 DOUT. Se reutiliza el dibujo
eléctrico KiCad SK6812MINI-E con Value EA y la geometría de su huella inversa,
renombrada EA y con procedencia de la hoja EA. El recorte sigue siendo de Dn,
no se duplica en SW. Colocar LED en B y comprobar orientación en cupón.

## PCB-A y pruebas

DNP es una propiedad de ensamble: no elimina cobre ni redes. R1/R2 y J4 quedan
DNP en esquema y PCB. No se mueven huellas, vías, pistas, zonas ni contorno.
La conectividad original exportada por KiCad se congela como contrato JSON antes
de editar; check_netlist compara componentes y cada pin/red incluyendo NC.
No se convierte PCB-A a un esquema generado ni se regenera con --regenerate.
La BOM usa el exportador real de KiCad e incluye DNP para separar compra de montaje.

Las pruebas verifican geometría, numeración, encapsulados/campos de compra, DNP,
redes conservadas y exportaciones KiCad. ERC detecta problemas eléctricos del
esquema; no demuestra ajuste mecánico, DRC ni funcionamiento físico. No se declara
fabricable ninguna placa por pasar estos checks. La comprobación del panel de PCB-A
se enumera en REPORT-mano sin trasladar su diseño.

Referencias:
- `../fab/pedido-v1.md`, `apps/docs/consola/28-pcb-k.md`, panel CSV v1.
- [LCSC C5378731](https://www.lcsc.com/datasheet/C5378731.pdf),
  [PDF B/2](https://datasheet.lcsc.com/datasheet/pdf/7a14970dbcd694dcd129b083b19b0911.pdf?productCode=C5378731), pp.4–5.
- [C506653 SSOP](https://jlcpcb.com/partdetail/MicrochipTech-MCP23017_ESS/C506653).
- Librerías KiCad 10.0.6, LED_SK6812MINI-E_3.2x2.8mm_P1.5mm_ReverseMount;
  [licencia](https://www.kicad.org/libraries/license/).
