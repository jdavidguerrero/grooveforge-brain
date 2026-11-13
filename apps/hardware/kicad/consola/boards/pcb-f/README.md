# PCB-F — GrooveForge Console

Layout de dos capas **81 × 83 mm**, cuatro RS6011 B50K a paso **20 mm**.
El esquema y los CSV originales se conservan. Ver [teoría](docs/THEORY-layout.md)
y [reporte completo](../../tools/layout/REPORT-f.md).

Desde la raíz del clon:

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 apps/hardware/kicad/consola/tools/layout/layout_f.py
```

El script exporta el esquema a XML, compara las seis redes con el XML verificado,
importa sus diez componentes, coloca cuatro M3, enruta, rellena GND, ejecuta DRC y
exporta `fab/` y `docs/`. `--verify-only` verifica la placa guardada;
`--finish-only` rellena, verifica y exporta una placa existente. No escribe esquemas.

Coordenadas: **x_panel = x_KiCad + 44; y_panel = 371 − y_KiCad** (mm).
Faders: F.Cu / 90°; centros según `placement.csv`. J1: **B.Cu / 90°**, origen
panel **(257,5; 160,2)**, boca hacia **−x**; su extremo trasero llega a y=165,1.
La posición deja libres el M3 posterior izquierdo y RV1. Cara superior del FR4:
TOP−9,5; con espesor 1,6, J1 se monta bajo TOP−11,1.

J1 es **1:1** hacia PCB-A J3: **1 +3V3, 2 FADER1, 3 FADER2, 4 FADER3,
5 FADER4, 6 GND**. Los MP del JST se conservan sin red, según el esquema.
Los doce MP de los faders llevan GND sólido. La copia local `groovepush.pretty`
conserva la geometría upstream y añade únicamente `zone_connect 2` a esos pads;
no cambia la biblioteca compartida ni el identificador del esquema.

`fab/`: Gerbers, taladros **PTH y NPTH separados**, job y reporte de taladros.
`docs/`: DRC JSON, verificación geométrica/eléctrica JSON, XML recién exportado,
SVG por capa, plantilla [1:1](docs/pcb-f-assembly-1to1.svg) y DXF Edge.Cuts.
Los agujeros internos se fabrican desde NPTH; el DXF de Edge.Cuts contiene solo
el perímetro exterior, sin convertir taladros en fresados duplicados.

**Antes de pedir:** imprimir el SVG de montaje al 100 %, sin ajustar a página;
medir el paso de 20 mm y el contorno, superponer un RS6011 real y verificar pads,
NPTH, carcasa, carrera, altura, cabezas M3 Ø5,5 y acceso al cable JST por debajo.
No usar arandelas Ø6,4 en las esquinas sin revisar el montaje. El contorno fijo deja
solo 0,25 mm entre cabeza nominal Ø5,5 y borde. Validar continuidad carcasa→GND
y el bond del panel: los cuatro agujeros M3 son NPTH y no realizan ese bond.
