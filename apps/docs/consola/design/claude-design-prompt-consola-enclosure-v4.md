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
2. **La Pi se desapila y se queda detrás de la pantalla.** Juan ya probó cables DSI largos y son difíciles de manejar:
   la Pi va **atornillada detrás del módulo** con el FFC corto de Waveshare, como en la v2. Para bajar el alto, el
   **Audio+ V2 sale del apilado**: va al lado, conectado por un **cable plano de GPIO de 40 pines** (10–15 cm). Detrás
   de la pantalla queda solo **Pi 5 + Active Cooler + HAT NVMe** (estimado ~35 mm desde la Pi; a verificar cuando llegue
   el HAT). Diseña la ventilación de esa zona (entrada abajo, salida atrás) porque el calor queda bajo la pantalla.
3. **Organización nueva** (§2): pantalla y NeoTrellis al centro, todo lo demás alrededor, por mano.
4. **Keycaps más chicos**: **15 × 15 mm** (antes 18 × 17) sobre el mismo paso de las PCB-K existentes; ajusta la
   familia de moldes a ese tamaño.
5. **Rótulos correctos** de `25` §3 (adjunto): los modos ya no tienen teclas; K2 = BACK · MENU · ◀ · ▶ · BROWSE.

## 1. Fabricación: diseña para dos caminos

El cuerpo **ya no tiene que salir de la Ender 3 V3**. Entrega el diseño para:

- **A (preferido): fabricado afuera en una sola pieza o en dos** — impresión 3D industrial (MJF / SLS en PA12, o FDM
  de cama grande de un servicio local en Bogotá) o **JLC3DP** junto con el pedido de las PCB. Indica el tamaño de
  pieza que pide cada opción, el proceso que recomiendas, el acabado (negro mate) y las reglas de diseño del proceso
  (pared, radios, insertos, tolerancias).
- **B (respaldo): partido para la Ender** (piezas ≤ 210 × 210) con uniones ocultas, como la v2, **sin que eso cambie
  la forma exterior**: la división es interna.

Panel superior de aluminio 2 mm CNC como siempre.

## 2. Organización: centro y alrededor

Pantalla y NeoTrellis forman el **eje central**; alrededor, los grupos según la mano que los usa. Disposición de
referencia (valídala o corrígela con números):

```
          ┌BACK──┐ ┌────────── pantalla 7" ──────────┐ ( JOG )      MASTER   CUE
 [EXP1]   │MENU  │ │                                 │  ◀    ▶      IN   [pad]
 [EXP2]   │BROWSE│ └─────────────────────────────────┘
                    (E1) (E2) (E3) (E4) (E5) (E6) (E7) (E8)
 CUE FILL TAP
 BUILD DROP BREAK  ┌────────── pads M4 8 × 4 ─────────┐  [FSR][FSR]   ‖  ‖  ‖  ‖
 SHIFT             │                                  │  [FSR][FSR]   faders
 PLAY STOP REC     └──────────────────────────────────┘
 LOOP METRO          UNDO DELETE DUPLICATE QUANTIZE NEW   PAGE ◀ ▶  OCT − +
 [Sharp]                                                                 [Sharp]
```

- **Navegación junto a la pantalla:** columna K2 a la izquierda; **jog con ◀ ▶ a la derecha**, como en la MPC.
- **Mano izquierda:** transporte, SHIFT, motor en vivo (CUE · FILL · TAP), sección (BUILD · DROP · BREAK) y los 2 FSR de
  expresión (se sostienen mientras la derecha trabaja).
- **Mano derecha:** 4 FSR de velocity junto a los pads, faders, y MASTER / CUE / IN arriba, como en un mixer DJ.
- **Sharp en las esquinas delanteras** (pedido de Juan): radio libre de 45 mm alrededor de cada lente, nada más en esa
  esquina, lente inclinada unos grados hacia afuera para que la mano que toca pads o transporte no los dispare.
  Valida la regla 15 con eso (en firmware los Sharp vienen apagados por defecto y se activan en Settings).

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
2. **Alto del cuerpo** con el apilado desapilado y de dónde sale cada mm.
3. **Tabla de cambios contra la v3.**
