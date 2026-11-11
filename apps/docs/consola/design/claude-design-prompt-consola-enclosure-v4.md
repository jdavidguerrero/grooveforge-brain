# Prompt para Claude Design — Enclosure de la GrooveForge Console, v4

> **Cómo usarlo:** en el mismo proyecto donde salió la v3 («GrooveForge Console»), pegar lo que está debajo de la
> línea. **Adjuntar sí o sí** `apps/docs/consola/25-panel-consola.md` (la v3 no lo recibió y dejó rótulos viejos) y
> el prompt anterior `claude-design-prompt-consola-enclosure.md` como referencia de piezas y medidas.
> Base: revisión de Juan a la v3 (2026-11-11).

---

## Qué no funcionó en la v3 y qué cambia

1. **El escalón losa + quilla no va.** Juan quiere un **bloque macizo**: cara superior plana, alto parejo en todo el
   cuerpo, sin escalones ni quillas. Referencia de carácter: **Akai MPC X** (cuerpo grande, sólido, plano), no la
   Push. Puede ser más grande que la v3.
2. **La Pi va en el piso, con su apilado completo.** Pi 5 + Active Cooler + HAT NVMe (con su regleta apilable) +
   Audio+ V2 encima, todo junto, **apoyado en el piso del cuerpo** (no detrás de la pantalla). Alto estimado del apilado
   **60 mm desde la base de la Pi** (a verificar cuando llegue el HAT; deja ≥ 10 mm de margen y las varillas roscadas
   de la v3 para ajustarlo). De ahí sube el **flex DSI** a la pantalla: ubica la Pi **debajo o muy cerca del módulo de
   pantalla** para que el recorrido sea corto y simple (una sola curva amplia, sin pliegues en filo; regla 8), y elige
   el largo comercial (200 / 300 mm). Como el apilado vive en el piso, **el cuerpo entero toma ese alto de forma pareja**
   (bloque macizo de ~80–90 mm, como una MPC X), no solo una quilla. Diseña la ventilación de la bahía de la Pi
   (entrada abajo o al frente, salida atrás) separada de PCB-M por un tabique.
3. **Organización nueva** (§2): pantalla y NeoTrellis al centro, todo lo demás alrededor, por mano.
4. **Keycaps más chicos**: **15 × 15 mm** (antes 18 × 17) sobre el mismo paso de las PCB-K existentes; ajusta la
   familia de moldes a ese tamaño.
5. **Rótulos correctos** de `25` §3 (adjunto): los modos ya no tienen teclas; K2 = BACK · MENU · ◀ · ▶ · BROWSE.

## 1. Fabricación: diseña para dos caminos

El cuerpo **ya no tiene que salir de la Ender 3 V3**. Entrega el diseño para:

- **A (preferido): impresión local en Bogotá, en impresoras 3D de cama grande** (servicios con camas de ~350–500 mm).
  Divide el cuerpo en el **menor número de piezas** que permita una cama de **350 × 350 mm** (y di cómo queda con una
  de 500 × 500), con uniones ocultas, en PETG o ASA negro mate. Indica pared, insertos, tolerancias y orientación.
- **B (respaldo): partido para la Ender 3 V3** (piezas ≤ 210 × 210) con uniones ocultas, como la v2, **sin que eso cambie
  la forma exterior**: la división es interna.

Panel superior de aluminio 2 mm CNC como siempre.

## 2. Organización: centro y alrededor

Pantalla y NeoTrellis forman el **eje central**; alrededor, los grupos según la mano que los usa. Disposición de
referencia (valídala o corrígela con números):

```
[Sharp]                                                                   [Sharp]
          ┌BACK──┐ ┌────────── pantalla 7" ──────────┐ ( JOG )      MASTER   CUE
 [EXP1]   │MENU  │ │                                 │  ◀    ▶      IN   [pad]
 [EXP2]   │BROWSE│ └─────────────────────────────────┘
                    (E1) (E2) (E3) (E4) (E5) (E6) (E7) (E8)
 CUE FILL TAP
 BUILD DROP BREAK  ┌────────── pads M4 8 × 4 ─────────┐  [FSR][FSR]   ‖  ‖  ‖  ‖
 SHIFT             │                                  │  [FSR][FSR]   faders
 PLAY STOP REC     └──────────────────────────────────┘
 LOOP METRO          UNDO DELETE DUPLICATE QUANTIZE NEW   PAGE ◀ ▶  OCT − +
```

- **Navegación junto a la pantalla:** columna K2 a la izquierda; **jog con ◀ ▶ a la derecha**, como en la MPC.
- **Mano izquierda:** transporte, SHIFT, motor en vivo (CUE · FILL · TAP), sección (BUILD · DROP · BREAK) y los 2 FSR de
  expresión (se sostienen mientras la derecha trabaja).
- **Mano derecha:** 4 FSR de velocity junto a los pads, faders, y MASTER / CUE / IN arriba, como en un mixer DJ.
- **Sharp en las esquinas traseras (superiores)**, detrás de todos los controles, con radio libre de 45 mm alrededor de
  cada lente y nada más en esa esquina (regla 15). Juan los probó: en las esquinas delanteras la mano camino a pads o
  transporte los dispara.

## 3. Lo que se conserva de la v3

Familia de perillas, keycaps (ahora 15 × 15) y tapas de FSR con sus moldes de dos partes; el tetón Ø10.5 bajo el
disco del FSR; varillas roscadas para el apilado ajustable; tabique entre la zona de la Pi y PCB-M; jacks de PCB-M en
el panel trasero; una sola entrada USB-C (fuente oficial de la Pi); las 17 reglas.

## Entregables

Los mismos de la v3 (vista superior 1:1, alineación encoder ↔ soft-key, corte lateral, paneles trasero y frontal,
explotada, 17 reglas, tabla de coordenadas en formato `panel-coordinates.csv` con la placa de cada control, PCB-N/P,
renders, medidas a verificar, familia de perillas y moldes), y además:

1. **Comparación de los caminos A y B** de fabricación: tamaño de pieza, proceso, ventajas y riesgos (sin precios
   inventados: lista lo que hay que cotizar).
2. **Alto del cuerpo** con la Pi apilada en el piso y de dónde sale cada mm.
3. **Tabla de cambios contra la v3.**
