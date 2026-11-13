# PCB-A — tira de encoders Console v1

Tira de dos capas, 217 × 29,6 mm, espesor 1,6 mm y esquinas R1. Esquema, referencias,
pads y redes conservados respecto a `edc67a3`. La validación de entrega está en
[REPORT-strip.md](REPORT-strip.md); el criterio previo está en [THEORY-strip.md](THEORY-strip.md).

## Montaje y coordenadas

Conversión: `x_panel = x_KiCad + 44`, `y_panel = 371 − y_KiCad` (mm).
Contorno KiCad: x=10…227, y=166,4…196. Contorno panel: x=54…271, y=175…204,6.
Ejes E1–E8 del CSV: x=79,105,131,157,183,209,235,261; y=192; paso 26 mm.
Los orígenes de las huellas EC11 están a (−7,5; −2,5) mm de sus ejes.

| Agujero | KiCad (mm) | Panel (mm) | Taladro |
|---|---|---|---|
| H1 | 18; 188 | 62; 183 | NPTH Ø3,2 mm |
| H2 | 74; 179 | 118; 192 | NPTH Ø3,2 mm |
| H3 | 100; 179 | 144; 192 | NPTH Ø3,2 mm |
| H4 | 204; 179 | 248; 192 | NPTH Ø3,2 mm |

H4 está entre E7 y E8 para liberar el cuerpo del último encoder. Las posiciones también
están anotadas en `Cmts.User` de la PCB. Separadores M3 nominales de 10 mm, pendientes de
verificar con el conjunto físico. Los NPTH no constituyen una unión eléctrica a la chapa.

J1 es IDC 2×15 por la cara inferior, con su eje largo paralelo a X; origen KiCad
(207,78;192,07), ángulo +90°. J2 SH4 y J3 SH6 van en la cara inferior; J3 abre hacia
la derecha. U1 y los pasivos quedan dentro de la tira. **R1, R2 y J4 son DNP**;
los pull-ups activos de I2C están en PCB-M. Los pads DNP conservan sus pistas y redes.
Pinout y cables 1:1: [CONNECTORS.md](../CONNECTORS.md).

`Connector_IDC.pretty/` conserva los pads, taladros, cuerpo y courtyard de la huella IDC.
Solo adapta la marca de pin 1 de serigrafía al borde de la tira; la tabla local resuelve
el mismo identificador de huella sin modificar el esquema. El triángulo original se
conserva en `Dwgs.User`, y el marcador de fabricación permanece en `B.Fab`.

## Cobre y archivos de fabricación

Señales 0,25 mm; +3V3, +5V y pistas GND ≥0,4 mm; separación ≥0,2 mm;
vías Ø0,6/taladro Ø0,3 mm. Hay planos GND en ambas caras y costura. Los MP de
los ocho encoders se conectan a GND de forma sólida. Las reglas del proyecto y
`pcb-a.kicad_dru` verifican los mínimos; no se excluyen errores de cobre.

- `fab/`: Gerber, taladros PTH/NPTH y ZIP para fabricación.
- `docs/`: SVG de capas, informe DRC final y verificación de integridad.
- `../../tools/fit/REPORT-pcba-fit.md`: encaje geométrico y corredores del mazo.

No usar capturas o DRC de colocación como aprobación de fabricación. La continuidad
chapa/eje→GND, alturas reales, radios de los cables, tolerancias y accesibilidad de
separadores requieren montaje físico. El diagnóstico de PCB-K usa contornos históricos
y se detalla como limitación del informe de encaje.

## Reproducir validaciones

Desde la raíz del clon, con KiCad 10.0.6 instalado:

```sh
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
KCLI=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
BOARD=apps/hardware/kicad/consola/boards/pcb-a
"$KPY" -B tools/layout/pcba_strip.py verify
"$KCLI" pcb drc --format json --exit-code-violations -o "$BOARD/docs/drc-final.json" "$BOARD/pcb-a.kicad_pcb"
python3 -B apps/hardware/kicad/consola/tools/fit/check_pcba_fit.py
"$KPY" -B tools/layout/pcba_strip.py export
```

El script `tools/layout/pcba_strip.py` también contiene los pasos de colocación,
ruteo y relleno. El ruteo usa Freerouting y completa ramas pendientes con la grilla
del repositorio; cada resultado debe pasar de nuevo por DRC. `audit_copper` es un
diagnóstico SWIG sin comprobación de biblioteca; **no sustituye** el DRC CLI final.
