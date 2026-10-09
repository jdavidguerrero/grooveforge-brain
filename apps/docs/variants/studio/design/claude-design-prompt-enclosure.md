# Prompt para Claude Design — Validación del enclosure de Studio

> **Cómo usarlo:** pegar lo que está debajo de la línea en Claude Design. Adjuntar
> `studio-ui-claude-design.html` y, si existe, el resultado de la iteración v2 de controles.
> Conviene correr este prompt **antes** de cerrar la iteración de UI: las posiciones físicas que
> salgan acá definen dónde van las etiquetas en pantalla.
>
> Medidas marcadas "verificar" no están confirmadas: hay que medirlas con calibre.
> Última actualización: 2026-10-07

---

## Objetivo

Diseñar y **validar** el enclosure de escritorio de **GrooveForge Brain · Studio**: un sintetizador
híbrido de uso personal para producir house, organic house y melodic house. El resultado debe decir
con números si los controles, la pantalla y la electrónica entran sin choques, si es cómodo de
tocar, y dónde queda cada control respecto de su etiqueta en pantalla.

## Identidad y materiales

Misma familia que el Brain comercial:
- Cuerpo en **PETG** impreso (FDM), pared de 3 mm.
- **Panel superior de aluminio de 2 mm** (CNC), esquinas R3.
- Paleta oscura (antracita #0A0A0A, acentos teal #1D9E75 y púrpura #534AB7).

**Mejillas de guadua: detalle estético opcional, no funcional.** El enclosure tiene que quedar
completo, cerrado, rígido y terminado **sin mejillas**. Las mejillas de guadua (madera colombiana,
12 mm en el comercial) se pueden agregar después como tapas laterales decorativas:
- No cumplen ninguna función estructural.
- No llevan conectores, jacks ni agujeros funcionales.
- Se fijan de forma reversible (tornillos ocultos desde adentro o imanes), sin modificar ninguna otra
  pieza. Deja previstos los puntos de fijación en los laterales del cuerpo.
- El cuerpo sin mejillas tiene que verse terminado (no como si le faltara algo).

Es un equipo de escritorio, no portátil: puede ser más grande que el comercial (180 × 100 × 38 mm).
Evalúa caja plana contra cuña inclinada de 10–15° para leer bien la pantalla sentado.

## Componentes del panel

| Componente | Cant. | Medidas de referencia |
|---|---|---|
| Pantalla Freenove 5" IPS DSI táctil, 800×480 | 1 | área activa ~108 × 64.8 mm (0.135 mm/px); contorno del módulo **verificar** (~121 × 76 mm típico) |
| Encoders ALPS EC11 con push (ENC L, ENC R, P1, P2, NAV) | 5 | agujero 6.5 mm; perilla Ø ~20 mm **verificar** con la perilla elegida |
| Anillo de LEDs alrededor de NAV | 1 | ventana Ø ext. 32 mm / int. 24 mm |
| Botones Kailh Choc V2 con keycap 1u (B1–B6) | 6 | cutout 14 × 14 mm; keycap ~18 × 17 mm |
| Potenciómetro de volumen VOL (Alpha RV09) | 1 | agujero 7 mm |
| Potenciómetro de ganancia de entrada IN (Alpha RV09, igual al VOL) | 1 | agujero 7 mm; misma perilla que VOL |
| Switch de pad de entrada, 3 posiciones (0 / −10 / −20 dB), **panel trasero** | 1 | slide o toggle ON-ON-ON; cutout **verificar** con la pieza elegida |

**Disposición de referencia (a validar o corregir):**

```
┌──────────────────────── pantalla ─────────────────────────┐
└──────────────────────────────────────────────────────────────┘
       [ENC L]   [B1]  [B2]  [B3]  [B4]   [ENC R]                ← fila 1
 (IN)  [ P1 ]   [B5 MODE]  [ NAV ]  [B6 HOLD/SHIFT]  [ P2 ]  (VOL) ← fila 2
```

**IN y VOL van simétricos**, uno en cada extremo de la fila 2, con la misma perilla y a la misma
altura: entrada a la izquierda, salida a la derecha. Si la simetría choca con otra regla, propón la
alternativa más cercana y explícala.

## Conflicto conocido que hay que resolver

En la UI v1, ENC L está centrado en x = 64 px y B1 en x = 196 px. Con 0.135 mm/px eso es 8.6 mm y
26.5 mm: **17.9 mm entre centros**. Una perilla de Ø 20 mm (radio 10) más medio keycap (~9 mm) necesita
al menos ~19 mm, **sin contar espacio para los dedos**. No entra.

Los botones B1–B4 sí entran bajo la pantalla (centros a 18.4 mm entre sí). Hipótesis de partida:
**ENC L, ENC R, P1 y P2 quedan fuera del ancho de la pantalla**, a los costados, y el panel es más
ancho que la pantalla. Valida o propone otra solución.

## Electrónica interna

| Pieza | Medidas de referencia | Notas |
|---|---|---|
| Raspberry Pi 3B | 85 × 56 mm, ~20 mm con disipador | considerar montarla **detrás de la pantalla** (muchos módulos DSI traen agujeros para eso); necesita ventilación |
| Cable DSI de 15 pines | **verificar** largo | radio de curvatura amplio; no doblar en filo |
| Teensy 4.1 + Audio Shield Rev D en placa portadora | Teensy 61 × 18 mm; shield ~36 × 31 mm **verificar** | — |
| PCB de panel (encoders, botones, LEDs) | según layout | montado bajo el aluminio con separadores |

## Conectores (siempre en el cuerpo de PETG, nunca en las mejillas)

En el Brain comercial algunos jacks y puertos van en las mejillas de guadua. En Studio **todos los
conectores van en el panel trasero o en los laterales del cuerpo**, para que las mejillas sean
puramente decorativas.

- **USB-C de alimentación**, 5V 3A: una sola entrada que alimenta Teensy, Pi y pantalla.
- **USB del Teensy** al computador (audio y MIDI USB; solo datos).
- **USB-A host** para el teclado MIDI.
- **Salida de audio L/R**, jacks de 1/4" (agujero 10.5 mm).
- **Entrada de audio L/R** para el modo FX processor, jacks de 1/4".
- **Switch de pad de entrada** (0 / −10 / −20 dB) **junto a las entradas L/R**, con la serigrafía
  "INPUT PAD" y los tres valores legibles.
- **CV out** para el add-on del filtro analógico, jack de 3.5 mm (agujero 7 mm).
- **MIDI IN** TRS de 3.5 mm.
- **Acceso de servicio** a la microSD y al Ethernet de la Pi (tapa o ranura), para desarrollo.

## Reglas de validación (cada una con pasa / no pasa y el valor medido)

1. Perilla–perilla: ≥ 8 mm entre bordes.
2. Perilla–keycap: ≥ 6 mm entre bordes.
3. Keycap–keycap: ≥ 1.5 mm entre bordes.
4. Ningún control a menos de 15 mm del borde frontal del panel.
5. Al girar cualquier perilla, la mano no tapa la pantalla (los controles van debajo).
6. Cada etiqueta en pantalla queda a ≤ 25 mm horizontales de su control físico.
7. El cable DSI llega sin tensión ni dobleces en filo.
8. Hay área de ventilación suficiente para la Pi 3B (indicar en cm²).
9. Todos los conectores accesibles con cables enchufados, sin que choquen entre sí.
10. Huella total razonable para un escritorio junto a un teclado (indicar ancho × fondo × alto),
    con y sin mejillas.
11. Sin mejillas, el enclosure queda completo, cerrado y rígido; con mejillas, se agregan sin
    modificar ninguna otra pieza y sin tapar conectores ni ventilación.
12. IN y VOL quedan simétricos respecto del eje central del panel (indicar la diferencia en mm).
13. El switch de pad se puede mover con los cables de entrada enchufados, y su posición se lee
    desde atrás sin desenchufar nada.

## Entregables

1. **Vista superior a escala 1:1 en mm** con el centro de cada control y de la pantalla.
2. **Tabla de alineación:** por cada control, su centro físico (mm) y el centro de su etiqueta en
   pantalla (px), para actualizar la UI.
3. **Corte lateral:** inclinación, alturas y apilado interno (Pi, pantalla, PCBs).
4. **Panel de conectores** con posiciones.
5. **Vista explotada** con piezas y materiales, incluyendo las mejillas opcionales y sus puntos de
   fijación.
6. **Reporte de las 13 reglas** con pasa / no pasa y el valor.
7. **Dos renders de referencia:** el enclosure sin mejillas (versión base) y con mejillas de guadua.
8. **Lista de medidas a verificar** con calibre antes de fabricar.
