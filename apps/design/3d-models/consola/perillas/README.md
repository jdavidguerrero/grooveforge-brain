# Perillas — impresión FDM

Generadas por `../build_knobs.py` (FreeCAD). No afectan las PCB: se pueden imprimir y ajustar en cualquier momento.

| Archivo | Uso | Cant. |
|---|---|---|
| `cupon_agujero.stl` | **Imprimir primero.** Tres agujeros estriados con holgura 0,05 / 0,10 / 0,15 mm: probar con un EC11 y elegir el que entra firme sin rajar | 1 |
| `perilla_18.stl` | Ø18 × 15 para los 8 encoders Alps EC11 (eje estriado Ø6, 18 dientes, 28 mm de la base a la punta; con PCB-A a −20 mm asoma 8 mm, doc 25 §7). 24 estrías de agarre, línea indicadora, agujero de 7 mm con ranura de flexión | 8 + repuestos |
| `perilla_pot_18.stl` | Ø18 × 18 para los pots Alps RK09L (MASTER, CUE, IN): agujero en D (Ø6, cara plana a 4,5) y hueco Ø12 abajo que tapa casquillo M7 y tuerca. Supone casquillo de ~8 mm y punta del eje a ~18 mm sobre el aluminio: verificar con calibre | 3 |
| `jog_40.stl` | Ø40 × 14 con hoyuelo para el dedo (eje estriado como los EC11) | 1 |

Impresión: boca abajo (cara superior sobre la cama, el agujero queda hacia arriba sin soportes), PETG o PLA, capa
0,12–0,16 mm, 4 perímetros. Si el cupón elige otra holgura, cambiar `BORE_CLEAR` en `build_knobs.py` y regenerar.
La perilla queda 1 mm sobre la chapa (carrera del clic 0,5 mm).
