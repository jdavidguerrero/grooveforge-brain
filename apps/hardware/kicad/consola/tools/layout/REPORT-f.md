# PCB-F — layout (2026-11-13)

Layout por `tools/layout/layout_f.py` (Codex); cierre de la entrega por Claude tras el corte de cuota de Codex.

| Verificación | Resultado |
|---|---|
| DRC kicad-cli 10.0.6 | **0 violaciones, 0 pads sin conectar, 0 errores de huella** (re-ejecutado por Claude) |
| Posiciones (docs/geometry-check.json) | 14 huellas, error máx 0,0 mm contra el panel; transformación panel = (x_KiCad + 44, 371 − y_KiCad) |
| Conectividad | 6/6 redes coinciden con el XML del esquema |
| Plano GND | F.Cu y B.Cu, sin cobre en las 4 reservas de ranura de los faders |
| Pistas y vías | 202 |

Fabricación: `boards/pcb-f/fab/` con Gerbers, taladros PTH/NPTH y `pcb-f-gerber-jlc.zip`; STEP en
`apps/design/3d-models/consola/pcb-f.step`; vistas SVG y ensamble 1:1 en `boards/pcb-f/docs/`.

Verificar a mano: imprimir `docs/pcb-f-assembly-1to1.svg` a escala 1:1 y apoyar un fader RS6011 real (pines,
agujeros de fijación y ranura del cursor) y el conector JST-SH de 6 vías antes de pedir.
