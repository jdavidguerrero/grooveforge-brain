# Prompt para Claude Design — Enclosure de GroovePush, iteración 2

> **Cómo usarlo:** en el mismo proyecto de Claude Design donde salió la v1
> (`claude-design-2026-10-19-v1/`), pegar lo que está debajo de la línea. Adjuntar la foto de las
> medidas de la Waveshare y, si se puede, la versión actualizada de `claude-design-prompt-enclosure.md`.
> Sale de recalcular `groovepush-geometry.js` con las medidas reales (2026-10-19).

---

## Contexto

La v1 está bien resuelta en estructura (grupos de teclas, alineación de encoders con las
soft-keys, ventilación, panel trasero). Al cargarle las medidas reales de las piezas salen
**cuatro choques** y una restricción de fabricación nueva. Corrige solo eso y conserva el resto.

## Medidas reales (reemplazan a las de la v1)

| Pieza | v1 asumía | Real |
|---|---|---|
| Módulo Waveshare 7" | 170 × 106 mm | **189.32 × 120.24 mm**, R8, **14.8 mm** de espesor. Área activa 155.0 × 88.0 mm **centrada** (bordes 17.16 a los lados, 16.12 arriba y abajo). Atrás: 4 agujeros M2.5 a 140.0 mm y el patrón de la Pi (58 × 49) |
| Apilado vidrio → Pi 5 + Active Cooler + HAT Audio+ V2 | 42.2 mm | **~50 mm** (medido) |
| Faders | ranura 48 mm, cuerpo 59 mm | **ALPS RS6011**: recorrido **60 mm**, cuerpo **75 × 9 × 7 mm**, palanca de 15 o 20 mm, ranura ~3 × 63 mm |
| NeoTrellis M4 | placa genérica | PCB 120.0 × 60.0 mm; 16 agujeros Ø2.8 en x = 2, 28, 32, 58, 62, 88, 92, 118 × y = 15, 45; **micro-USB, JST PH-4 y jack TRRS en un mismo borde largo** |
| Elastómero de los pads | pad de 12 × 12 | botones **10 × 10 mm**, paso 15, 10 mm de alto (11.5 en total) |
| Perillas | Ø18 | **Ø18** se mantiene (con la Ø20 del Brain la regla 6 da 26.6 mm y no pasa) |

## Choques a resolver

1. **El módulo choca con la pared trasera.** Con el área activa donde está, el borde trasero del
   módulo cae en y ≈ 239.1 y la chapa termina en 241: no entra la pared de 3 mm. Alarga el panel o
   corre la pantalla, sin perder la regla 5 (manos que no tapan la pantalla).
2. **No queda aire bajo el HAT.** Con 50 mm de apilado, la holgura entre el HAT y el piso pasa de
   8.1 mm a ~0.5 mm en el punto más bajo de la Pi (su borde delantero). Hace falta ≥ 8 mm: sube el
   frente, cambia el ángulo o mueve la Pi dentro del patrón de montaje del módulo. Recalcula el
   camino del aire de la regla 9.
3. **Los faders no entran.** El cuerpo de 75 mm no cabe entre la fila frontal de teclas y la fila
   de encoders (hay ~71 mm), y F3/F4 quedan debajo de E8 y de las teclas BANK/OCT. Reubícalos con
   el recorrido de 60 mm completo y sin que el cap, en ningún extremo, quede a menos de 3 mm de
   otro control (regla 7).
4. **Los conectores de la M4 necesitan salida.** El borde de micro-USB, JST y TRRS no puede mirar
   hacia los encoders: bajo la fila de encoders está su PCB a −12 mm y no hay lugar para las fichas.
   Oriéntalo hacia el frente (los cables pasan bajo la fila frontal de teclas) o justifica otra
   salida. Reserva 25 mm libres frente a ese borde.

## Restricción nueva: impresión en una Ender 3 V3

El cuerpo de la v1 mide 236 × 235 × 78 mm y la cama es de **220 × 220 mm**. Divide el cuerpo de PETG
en piezas de **≤ 210 × 210 mm en planta**, con uniones que alineen solas (lengüeta y ranura o pines)
y tornillos M3 a insertos. Las uniones no se ven desde arriba (el aluminio las tapa) y no cruzan
conectores, ventilación ni la zona de la pantalla. Si achicando el ancho a ≤ 210 mm alcanza con dos
piezas (frente y fondo), compáralo contra un corte en cuatro y recomienda una opción. El panel de
aluminio se corta por CNC y no tiene límite.

## Entregables

Los mismos de la v1, actualizados, y además:

1. **Tabla de cambios contra la v1**: qué se movió y cuánto.
2. **Reporte de las 13 reglas** recalculado con las medidas reales.
3. **Despiece para impresión**: cada pieza con su tamaño en planta, la orientación sugerida y las
   uniones.
4. **Tabla de coordenadas final** de cada control (x, y en mm) y de los agujeros de montaje de la
   M4, el módulo y la portadora: de ahí salen las PCBs del panel.
