# Prompt para Claude Design — Iteración v2: controles nuevos de Studio

> **Cómo usarlo:** pegar lo que está debajo de la línea en el mismo proyecto de Claude Design donde
> se generó la v1. Adjuntar `studio-ui-claude-design.html` (la v1). Si ya corriste el prompt de
> validación del enclosure, adjunta también su tabla de posiciones físicas (mm ↔ px).
>
> Última actualización: 2026-10-07

---

## Objetivo

Iterar el diseño de **GrooveForge Brain · Studio** (v1 adjunta) para incorporar **2 encoders y 2
botones nuevos**. Conserva todo lo que ya funciona: paleta, tokens, tipografía, grilla de 8 px,
reglas de render para LVGL 8.3 y el estilo de ilustración. Cambia solo lo que piden los controles
nuevos.

## Controles nuevos

- **P1 y P2:** dos encoders con push.
- **B5 MODE:** cambia entre SYNTH y FX. **Reemplaza al doble push de NAV**: todo texto o anotación que
  diga "NAV ×2" pasa a decir "MODE". El push de NAV ahora es inmediato.
- **B6:** en modo FX es **HOLD** (mantener = efecto encendido, soltar = apagado; para drops y cortes,
  como en la Roland RMX-1000). En modo SYNTH es **SHIFT** (P1/P2 pasan a su par secundario).
- **IN:** potenciómetro de ganancia de entrada, simétrico al VOL (IN en el extremo izquierdo de la
  fila 2, VOL en el derecho). Ajusta la ganancia analógica del codec en pasos de ~1.5 dB más un ajuste
  digital fino. Importa sobre todo en modo FX, cuando entra audio externo.
- **INPUT PAD:** switch físico de 3 posiciones (0 / −10 / −20 dB) en el panel trasero. Si el firmware
  lee su posición, la UI la muestra (por ejemplo, "PAD −10 dB").

## Mapeo completo

| Control | Modo FX | Modo SYNTH |
|---|---|---|
| ENC L | Dry/Wet | Cutoff |
| ENC R | Parámetro 1 del FX | Resonance |
| P1 | Parámetro 2 del FX | 1er parámetro de la página |
| P2 | Parámetro 3 del FX | 2º parámetro de la página |
| NAV | recorrer FX; push = on/off | vistas y cursor; push = acción |
| B1–B3 | INSERT / SEND / MASTER | OSC / ENV / LFO |
| B4 | FX favorito | PRESET / ENGINE |
| B5 | MODE | MODE |
| B6 | HOLD | SHIFT |
| IN (pot) | ganancia de entrada | ganancia de entrada |
| VOL (pot) | volumen de salida | volumen de salida |

## Qué hacen P1/P2 en cada pantalla (propuesta, se ajusta con el firmware)

| Pantalla | P1 | P2 | Con SHIFT (B6) |
|---|---|---|---|
| PERFORM | FILTER ENV AMOUNT | LFO → CUTOFF | GLIDE / DRIVE |
| ENV | ATTACK | DECAY | SUSTAIN / RELEASE |
| OSC | WAVE | DETUNE | OCTAVE / MIX |
| LFO | RATE | DEPTH | SHAPE / DESTINATION |
| AI / HARMONY | ARP RATE | HUMANIZE | ARP GATE / SWING |
| SPECTRUM | igual que PERFORM | igual que PERFORM | igual que PERFORM |
| FX MAIN | parámetro 2 del FX | parámetro 3 del FX | (B6 es HOLD) |
| ENGINE SELECT / PRESETS | sin función (mostrar atenuado) | sin función | — |

En ENV, la edición pasa a ser directa con P1/P2: ya no hace falta el flujo de NAV "seleccionar
segmento → push → girar". NAV puede seguir resaltando el segmento como ayuda visual.

Ejemplo para FX MAIN con Ghost Echo: ENC L = DRY/WET 42 %, ENC R = FEEDBACK, P1 = TIME 375 MS,
P2 = MOD DEPTH. Los nombres reales de los parámetros 2 y 3 de cada FX están por definirse; usa
valores ilustrativos y déjalo indicado.

## Disposición física de referencia

```
┌──────────────────────── pantalla 800×480 ────────────────────────┐
└────────────────────────────────────────────────────────────────────┘
       [ENC L]   [B1]  [B2]  [B3]  [B4]   [ENC R]                ← fila 1
 (IN)  [ P1 ]   [B5 MODE]  [ NAV ]  [B6 HOLD/SHIFT]  [ P2 ]  (VOL) ← fila 2
```

Regla de alineación: cada etiqueta en pantalla queda **lo más cerca posible** del control físico que
nombra. Con una pantalla de 5" (área activa ~108 mm de ancho) no se puede alinear todo 1:1: ENC L/R y
P1/P2 probablemente quedan **fuera del ancho de la pantalla**, a los costados. Sus bloques de valor
van pegados al borde de pantalla más cercano. Si tienes la tabla mm ↔ px del enclosure, úsala para
ubicar etiquetas; si no, deja las posiciones de las etiquetas como parámetros anotados.

## Qué cambiar

1. **Barra inferior (hoy 800×64):** tiene que representar dos filas de controles. Propón la solución
   (por ejemplo, la barra de soft-keys B1–B4 como está, más indicadores de P1/P2 en los costados y de
   MODE/HOLD en la fila 2). El área de contenido puede achicarse **como máximo 40 px**.
2. **Estados nuevos visibles:**
   - **SHIFT activo:** las etiquetas de P1/P2 cambian al par secundario y se distinguen sin ambigüedad.
   - **HOLD activo:** indicador fuerte pero barato de dibujar (cambio de color sólido de un pill o
     del borde; nada de flashes de pantalla completa).
   - **MODE:** el banner de cambio de modo (08c) menciona MODE, no NAV ×2.
3. **FX MAIN:** muestra dry/wet + los 3 parámetros a la vez, con su control físico asociado.
4. **ENV:** edición directa con P1/P2, con las variantes SHIFT apagado y SHIFT encendido.
5. **PERFORM y AI:** agregar los valores de P1/P2.
6. **Medidor de entrada en FX MAIN:** estéreo (L/R), con peak hold e indicador de clip. Es la
   referencia para ajustar IN. Barras sólidas, región chica, 30 fps.
7. **Overlay de IN:** al mover el pot aparece un overlay "INPUT GAIN" con el valor en dB y el medidor
   de entrada, con la misma lógica que el overlay de VOL (08a): se oculta 1.5 s después del último
   movimiento. Si el firmware conoce la posición del pad, el overlay la muestra ("PAD −10 dB").
8. **Aviso de clip de entrada:** una variante "INPUT CLIPPING" del aviso que ya existe para la salida,
   con la sugerencia "BAJA IN O SUBE EL PAD".

## Restricciones que se mantienen

- 800×480, escala 1×, LVGL 8.3 con render por software (Raspberry Pi 2B/3B).
- Rellenos sólidos; nada de blur, sombras grandes, glassmorphism ni gradientes de pantalla completa.
- Animación solo en regiones chicas, 30 fps.
- IBM Plex Mono, 7 tamaños, 2 pesos; mismos hex y los 4 tokens de Studio de la v1.
- Todo operable con controles físicos; el touch es un atajo (targets ≥64 px).

## Entregables

1. Pantallas actualizadas: PERFORM, ENV (SHIFT off/on), FX MAIN (HOLD off/on), AI/HARMONY, banner de
   modo, y las que cambien por la barra inferior nueva.
2. Hoja de componentes actualizada: indicadores de P1/P2, MODE, HOLD/SHIFT, la barra inferior nueva,
   el medidor de entrada, el overlay de INPUT GAIN y el aviso de INPUT CLIPPING.
3. Dibujo del panel físico con las dos filas, con la relación etiqueta ↔ control.
4. Anotaciones por pantalla actualizadas (controles físicos, regiones animadas, fuente de datos,
   specs en px).
5. Lista de cambios contra la v1.
