# Prompt para Claude Design — Cuerpo de la GrooveForge Console, v5

> **Cómo usarlo:** en el proyecto «GrooveForge Console» de Claude Design, pegar lo que está debajo de la línea y
> adjuntar: `apps/hardware/kicad/consola/panel/panel-coordinates-console-v1.csv` (coordenadas fijas),
> `apps/docs/consola/25-panel-consola.md` (§7) y `apps/docs/consola/24-placa-principal.md`.
> Base: distribución v1 decidida con Juan el 2026-11-11.

---

## Qué te pido esta vez (y qué no)

La **distribución del panel ya está decidida y es fija**: está en el CSV adjunto (x desde el borde izquierdo, y desde
el frente, en mm, centro de cada control). **No muevas, quites ni agregues controles.** Si algo choca con el cuerpo,
repórtalo con el valor medido y propón el ajuste, pero no lo apliques.

Tu trabajo es el **cuerpo**, el **interior** y la **familia de keycaps y perillas**.

## 1. Forma

- **Bloque macizo**, referencia de carácter **Akai MPC X**: cara superior plana y horizontal, alto parejo en todo el
  cuerpo, **sin escalones, quillas ni cuñas**. Esquinas y cantos sobrios (R3–R6), nada decorativo.
- Panel superior de **aluminio 2 mm CNC** de **340 × 340 mm**, anodizado negro, serigrafía IBM Plex Mono. Los cortes
  salen del CSV (Choc 14 × 14 con rebaje 16 × 16 × 0,8 por debajo; EC11 Ø7,1; pads 11 × 11; ranuras de fader 3 × 63;
  FSR 25,6 × 25,6; ventanas IR de los Sharp con acrílico).
- **Alto:** lo define la bahía de la Pi (§3). Estimado **80–90 mm**; justifica cada mm.
- Patas de goma; sin patas plegables.

## 2. Fabricación

- **Una sola pieza impresa en una cama local de 350 × 350 mm** (servicio de Bogotá), en PETG o ASA negro mate. El
  cuerpo exterior no puede pasar de **≈ 342 × 342 mm** en planta; si con la pared y el margen del servicio no entra,
  propón el corte mínimo (dos piezas) con unión oculta bajo el aluminio.
- **Respaldo:** la misma forma partida en piezas ≤ 210 × 210 para una Ender 3 V3, con uniones ocultas. **La forma
  exterior no cambia** entre las dos versiones.
- Indica pared, orientación de impresión, soportes, insertos M3 y tolerancias. La chapa atornilla al cuerpo con PEM.

## 3. Interior

| Pieza | Medidas | Requisitos |
|---|---|---|
| **Pi 5 + Active Cooler + HAT NVMe + Audio+ V2**, apilados | ~60 mm de alto desde la base de la Pi (**estimado**, el HAT no ha llegado) | **En el piso**, con varillas roscadas M2.5 para ajustar el alto, ≥ 10 mm de margen. Bahía con tabique propio y ventilación (entrada abajo o al frente, salida atrás; indica cm² y el camino del aire) |
| **Flex DSI** de la Pi a la pantalla | largo comercial 200 o 300 mm | Ubica la Pi **debajo o pegada al módulo de pantalla** para un recorrido corto, una sola curva amplia, sin pliegues en filo. Juan ya sufrió con cables DSI largos: que sea simple |
| Módulo Waveshare 7" | 189,32 × 120,24 × 14,8, vidrio **a ras** del aluminio | posición en el CSV (`MOD`) |
| **PCB-M** (Teensy 4.1 + Audio Shield en headers, ~20 mm de alto) | a definir por ti | Contra la pared trasera, con los jacks en su borde (§4) |
| **PCB-K** (una sola placa de teclas) | ~330 × 300, 2 capas | A **−3,4 mm** de la cara superior; lleva las 26 Choc del CSV con ventanas para la M4, los encoders, los faders y los FSR |
| PCB-A (8 EC11) | ya diseñada, ~200 × 30 | Base de los EC11 a −12 mm |
| PCB-F (4 faders RS6011, paso 20) | nueva | Recorrido de 60 mm libre |
| PCB-P (6 FSR en una tira) | plana, rígida | A −6,5 mm, bajo las tapas de FSR |
| NeoTrellis M4 | 120 × 60 | Conectores accesibles |
| Sharp ×2, jog, perillas, selector, jack de audífonos | — | Montados en el panel o el frente, cableados a PCB-M |

## 4. Panel trasero y frontal

Trasero (jacks en el borde de PCB-M y extensiones de panel): **USB-C de alimentación** (única entrada, fuente oficial
de la Pi) · salida de sala L/R 2 × 6,35 mm (cable RCA del Audio+) · LINE IN L/R 2 × 6,35 · LINE OUT L/R 2 × 6,35 · CV
1–2 3,5 mm · MIDI IN/OUT TRS 3,5 mm · pedal 6,35 · e-drum DB15 con tapa ciega · USB-A · Ethernet. Frente: **jack de
audífonos 6,35 mm bajo la perilla CUE** (x = 282). Todos accesibles con cables enchufados, sin chocar.

## 5. Familia de keycaps, perillas y tapas

Igual que la v3 (núcleo rígido + piel de silicona, molde de dos partes imprimible), con estos cambios:

- **Un solo keycap de 13,5 × 13,5 mm** para las 26 teclas (un molde). Vástago Choc V2 a verificar con la tecla. Ventana
  translúcida para el LED.
- **Keycaps del transporte 1 mm más bajos** (misma huella; segunda altura del mismo molde o un inserto).
- Perillas Ø18 (encoders y pots), jog Ø40, tapas de FSR: velocity con cara plana, expresión con cuna.
- LED por zona: verde (tocar) y violeta (navegar/editar); diseña la difusión para que ambos se lean de día.

## Entregables

1. Vista superior 1:1 **con los controles exactamente en las coordenadas del CSV** (verificación: ningún control movido).
2. Corte lateral con todas las alturas: aluminio, PCB-K, PCB-P, M4, PCB-A, faders, módulo, bahía de la Pi, PCB-M.
3. Recorrido del flex DSI y de la ventilación, con cm².
4. Panel trasero y frontal con posiciones.
5. Vista explotada y **plan de impresión** (una pieza para cama de 350, y respaldo para la Ender).
6. Contorno de **PCB-K, PCB-F, PCB-P y PCB-M** en coordenadas del panel, con agujeros de montaje: de ahí salen las PCB.
7. Familia de keycaps, perillas y tapas con sus moldes.
8. Choques encontrados contra el CSV (si hay), con el valor medido y el ajuste propuesto.
9. Dos renders de producto terminado (3/4 frente y trasera).
10. Medidas a verificar con calibre.
