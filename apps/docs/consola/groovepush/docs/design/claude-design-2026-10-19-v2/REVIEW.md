# Revisión de la v2 del enclosure (2026-10-19)

Recalculado con `groovepush-geometry-v2.js` y las medidas reales.

## Qué resolvió la v2
- Módulo Waveshare real (189.32 × 120.24) sin chocar con la pared, faders RS6011 con 60 mm de
  recorrido (a la derecha de los encoders), conectores de la M4 hacia el frente (la M4 va girada
  180°), cuerpo en 4 piezas de 142 × 121 mm (entran en la Ender 3 V3), 13 reglas en PASA o COND.
- 4 placas de panel: **PCB-A** (8 EC11), **PCB-F** (4 faders), **PCB-K1** (frente + edición,
  15 teclas), **PCB-K2** (modos, 5 teclas).

## Ajustes pendientes (no mueven ningún control en x / y)
1. **La Pi no se puede correr.** La v2 la movió 22.9 mm hacia atrás para ganar altura, pero se
   atornilla al patrón del módulo, que está centrado en vertical (35.62 + 49 + 35.62 = 120.24). Con
   la Pi en su lugar real la holgura HAT ↔ piso es **5.6 mm** (pide ≥ 8). Solución: frente de
   28 → **31–32 mm** (8.6–9.6 mm), o ángulo de 14° → 16° (11.2 mm, la parte trasera sube a 97 mm).
2. **Módulo ↔ pared trasera = 1.1 mm**: poco para PETG impreso. Alargar el panel ~2 mm (D 249 → 251).
3. **Ancho**: pasó de 236 a **284 mm** por llevar los faders a la derecha (62 % del área del Push 3).
4. **Alimentación**: la v2 propone una sola entrada de 5.1 V / 6 A con placa de distribución. Se
   mantiene la decisión de **dos entradas** (fuente oficial de la Pi + USB-C de la portadora).
5. **La M4 girada 180°**: el firmware de la M4 tiene que invertir el mapeo de pads y LEDs.

La tabla de coordenadas para las PCBs está en `hardware/panel/panel-coordinates.csv`.
