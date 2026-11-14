# PCB-K izquierda y derecha — layout para JLCPCB

2026-11-13 · base `3495fbb` · KiCad 10.0.6. Ensamble a mano: Choc V2 PG1353 (zócalo en B + PTH Ø1,3), SK6812MINI-EA de montaje inverso, MCP23017.

## Cómo se genera

Determinista, desde los netlists exportados (sin regenerar esquemas):

```
PY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
cd apps/hardware/kicad/consola/tools/layout
$PY build_k.py            # placas, huellas, contorno, agujeros, reservas
$PY route_k.py prepare    # geometría para el ruteador
./grid_router ../../boards/pcb-k-<lado>/routing-input.txt ../../boards/pcb-k-<lado>/routing-output.txt
python3 retry_k.py <lado> # reintentos con prioridad a las fallas
$PY route_k.py import
$PY finish_k.py           # campos, GND macizo en conectores, planos, vías de unión, limpieza
```

Cambios de esta pasada (Claude, tras los cortes de Codex y de los agentes): se reconstruyó desde cero porque la placa
mezclaba ruteos de dos herramientas (33/51 errores). Ramales cortos de GND desde cada pad SMD y cada salida GND de
MCP al PTH de tecla más cercano; margen de NPTH 0,8 mm en el ruteador; vías de unión GND en rejilla de 5 mm y
unión de islas; retiro de vías de una sola capa y de vías GND a <0,85 mm entre centros; atributo BOM según el esquema;
la reserva de los agujeros del marco prohíbe cobre, no cuerpos (pads medidos a ≥5,4 mm del centro).

## Resultado

| Placa | Errores DRC | Sin conectar | Paridad | Advertencias |
|---|---|---|---|---|
| pcb-k-left | 0 | 0 | 0 | silk_edge_clearance 4, lib_footprint_mismatch 17 |
| pcb-k-right | 0 | 0 | 0 | lib_footprint_mismatch 9 |

Advertencias aceptadas: `lib_footprint_mismatch` (courtyard de tecla recortado a 15 mm a propósito, §courtyards en
complete_k_notes.md) y `silk_edge_clearance` (serigrafía cerca del borde, sin efecto eléctrico).

`check_netlist --all`, `check_bom`, `check_harness` (32 cables, 0 errores) y `pytest tools/tests` (127) pasan.

## Agujeros (panel / KiCad = panel − (44, ·), y_KiCad = 371 − y_panel)

| Placa | Ref | Panel mm | Ø | Uso |
|---|---|---|---|---|
| left | H1 | (30.0, 144.0) | 3.2 | conservar PEM M2.5 |
| left | H2 | (82.0, 124.0) | 3.2 | conservar PEM M2.5 |
| left | H3 | (10.0, 200.0) | 3.2 | conservar PEM M2.5 |
| left | H4 | (64.0, 204.0) | 3.2 | conservar PEM M2.5 |
| left | H5 | (12.0, 247.0) | 3.2 | conservar PEM M2.5 |
| left | H6 | (12.0, 300.0) | 3.2 | conservar PEM M2.5 |
| left | H7 | (53.0, 316.0) | 3.2 | conservar PEM M2.5 |
| left | H8 | (80.0, 169.0) | 3.2 | conservar PEM M2.5 |
| left | H9 | (77.0, 130.0) | 3.2 | conservar PEM M2.5 |
| left | H10 | (77.0, 158.0) | 3.2 | conservar PEM M2.5 |
| right | H1 | (112.0, 32.0) | 3.2 | conservar PEM M2.5 |
| right | H2 | (228.0, 32.0) | 3.2 | conservar PEM M2.5 |
| right | H3 | (112.0, 100.0) | 3.2 | conservar PEM M2.5 |
| right | H4 | (228.0, 100.0) | 3.2 | conservar PEM M2.5 |
| right | H5 | (247.0, 101.5) | 3.2 | conservar PEM M2.5 |
| right | H6 | (246.0, 176.5) | 3.2 | conservar PEM M2.5 |
| right | H7 | (244.0, 94.0) | 3.2 | conservar PEM M2.5 |
| right | H8 | (244.0, 172.0) | 3.2 | conservar PEM M2.5 |
| right | H9 | (139.0, 72.0) | 4.6 | paso PEM PCB-P |
| right | H10 | (139.0, 97.0) | 4.6 | paso PEM PCB-P |
| right | H11 | (170.0, 72.0) | 4.6 | paso PEM PCB-P |
| right | H12 | (170.0, 97.0) | 4.6 | paso PEM PCB-P |
| right | H13 | (201.0, 72.0) | 4.6 | paso PEM PCB-P |
| right | H14 | (201.0, 97.0) | 4.6 | paso PEM PCB-P |

Marco de la M4: (77, 130), (77, 158), (244, 94), (244, 172); reserva inferior R4,6 sin cobre. Cabeza PEM Ø4,2 a ≥2,65 mm
de cualquier recorte de la chapa (el más cercano es SCENE1_OCT+). Salida trasera de la M4 (y≈168, x 150–190) libre.
Pasos de PCB-P: solo los seis de x 139/170/201 cruzan K derecha; NPTH Ø4,6 con reserva Ø7.
H1 de PCB-A pasa a (84, 179): la posición (72, 181) caía dentro de la franja x 68–74,5 de K izquierda.

## Fabricación

`boards/pcb-k-<lado>/fab/`: Gerbers, taladros PTH/NPTH con mapas y `pcb-k-<lado>-gerber-jlc.zip`.
STEP en `apps/design/3d-models/consola/pcb-k-<lado>.step`; SVG 1:1 en `docs/pcb-k-<lado>-mounting-1to1.svg`.

## Pendiente

- Imprimir el SVG 1:1 y comprobar teclas, zócalos y agujeros con piezas reales antes de pedir.
- Diseñar el marco impreso de la M4 en FreeCAD (no afecta a las placas).
