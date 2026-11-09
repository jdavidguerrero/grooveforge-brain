# Prompt para Claude Design — Enclosure de GroovePush

> **Cómo usarlo:** pegar lo que está debajo de la línea en Claude Design. Adjuntar, si están a mano:
> el diseño del enclosure del Brain Studio (`grooveforge-brain/apps/docs/variants/studio/design/claude-design-2026-10-09/`)
> para mantener la familia, y fotos de las piezas reales (pantalla, NeoTrellis, faders, perillas).
>
> Medidas marcadas **verificar** no están confirmadas: hay que medirlas con calibre o sacarlas del
> datasheet antes de fabricar.
> Decisiones de base: ADR-0003 (pantalla y GUI), ADR-0004 (botones Choc V2), ADR-0001 (placas
> modulares). Última actualización: 2026-10-19

---

## Objetivo

Diseñar y **validar** el enclosure de **GroovePush**: un controlador para Ableton Live inspirado en
el Push 3, hecho a mano en Bogotá, que también funciona sin computador (modo standalone, con audio
propio o manejando un sintetizador GrooveForge Brain). El resultado tiene que decir con números si
los controles, la pantalla y la electrónica entran sin choques, si es cómodo de tocar con las dos
manos, y dónde queda cada control respecto de su etiqueta en pantalla.

Tiene que ser **notablemente más compacto que un Push 3** (378 × 293 mm es el límite superior, no la
meta): un equipo de escritorio que conviva con un teclado y un computador.

## Identidad y materiales

Misma familia que el **GrooveForge Brain · Studio** (adjunto):
- Cuerpo en **PETG** impreso (FDM), pared de 3 mm.
- **Panel superior de aluminio de 2 mm** (CNC), esquinas R3.
- Paleta oscura: antracita #0A0A0A, acentos teal #1D9E75 y púrpura #534AB7. Serigrafía en
  IBM Plex Mono.
- Evalúa caja plana contra cuña inclinada de 10–15° para leer bien la pantalla sentado.

## Componentes del panel

| Componente | Cant. | Medidas de referencia |
|---|---|---|
| Pantalla Waveshare 7" DSI, 1280×720, táctil capacitiva de 10 puntos | 1 | módulo **189.32 × 120.24 mm**, esquinas R8, **14.8 mm** de espesor; área activa **155.0 × 88.0 mm** centrada (bordes 17.16 a los lados y 16.12 arriba y abajo); 0.121 mm/px. Atrás: 4 agujeros M2.5 a 140.0 mm entre sí y el patrón de la Raspberry Pi (58 × 49) |
| Encoders ALPS EC11 con push (los mismos del Brain) | 8 | agujero Ø7.1 liso (buje Ø6.8); perilla **Ø18** (con la Ø20 del Brain la regla 6 da 26.6 mm y no pasa) |
| Grilla de pads: **Adafruit NeoTrellis M4** (una sola placa 8×4) con 2 elastómeros de silicona 4×4 | 32 pads (8×4) | PCB **120.0 × 60.0 mm** (62 con conectores), 8.3 mm; **16 agujeros Ø2.8** (M2.5) en x = 2, 28, 32, 58, 62, 88, 92, 118 × y = 15, 45 (desde la esquina); **micro-USB, JST PH-4 y jack TRRS en el mismo borde largo**. Elastómero: botones 10 × 10 mm, paso 15 mm, 10 mm de alto, 11.5 mm en total |
| Faders **ALPS RS6011** (serie RS60) | 4 | **recorrido 60 mm**, cuerpo **75 × 9 × 7 mm**, palanca de 15 o 20 mm según el sufijo (**verificar** en la pieza); ranura ~3 × 63 mm |
| Teclas Kailh Choc V2 con keycap 1u y LED por tecla | 20 | cutout 14 × 14 mm con rebaje 16 × 16 × 0.8 mm (como el Brain); keycap ~18 × 17 mm |

**Las 20 teclas, por grupo (la ubicación es tuya; los grupos no se mezclan):**

| Grupo | Teclas |
|---|---|
| Transporte | PLAY · STOP · RECORD · LOOP · METRONOME |
| Modos | SESSION · NOTE · DEVICE · MIX · BROWSE |
| Navegación | BANK ◀ · BANK ▶ · OCTAVE ▲ · OCTAVE ▼ |
| Edición | UNDO · DELETE · DUPLICATE · QUANTIZE · NEW |
| Modificador | SHIFT |

**Disposición de referencia (a validar o corregir), en el espíritu del Push:**

```
┌───────────────────────── pantalla 7" ─────────────────────────┐
└────────────────────────────────────────────────────────────────┘
   (E1) (E2) (E3) (E4) (E5) (E6) (E7) (E8)          ← 8 encoders, uno por soft-key

 [MODOS ×5]   ┌──────────── pads 8 × 4 ────────────┐   ‖ ‖ ‖ ‖
 [EDICIÓN ×5] │                                     │   faders
 [SHIFT]      └─────────────────────────────────────┘   ×4
 [TRANSPORTE ×5]                    [BANK ◀ ▶] [OCT ▲ ▼]
```

## Conflicto conocido que hay que resolver

Los 8 encoders tienen que quedar **debajo de las 8 celdas de soft-key** de la pantalla. Cada celda
mide 1280 / 8 = 160 px = **19.4 mm**, así que las celdas ocupan 155 mm.

Con perillas de Ø 18 mm y ≥ 8 mm entre bordes, el paso mínimo es 26 mm y la fila mide ~200 mm. El
encoder de cada extremo queda ~23 mm corrido de su celda (con Ø 20 mm, ~30 mm). La regla 6 pide
≤ 25 mm. **Valida qué perilla y qué paso cumplen**, o propone otra solución (perilla más chica,
celdas de la GUI redistribuidas, encoders fuera del ancho de la pantalla). Explica el trade-off.

## Electrónica interna

| Pieza | Medidas de referencia | Notas |
|---|---|---|
| Raspberry Pi 5 + Active Cooler + RaspiAudio Audio+ V2 (HAT) | **~50 mm desde el vidrio de la pantalla hasta el HAT** (medido) | **atornillada detrás de la pantalla** con el patrón 58 × 49; disipa más que una Pi 3B: ventilación real, no solo ranuras. Las salidas del HAT (RCA y 3.5 mm) se llevan al panel trasero |
| Placa portadora (reutilizada del Brain) | **97 × 100 mm**; ~20 mm de alto sobre el piso (separador + placa + zócalo + Teensy) | Teensy 4.1 en zócalo; conector de panel IDC 2×15; cable I2C desde el zócalo del Audio Shield (que en GroovePush va vacío) |
| PCB de teclas (20 Choc V2 + LEDs + 2× MCP23017) | según layout | a −3.4 mm bajo la cara superior del aluminio, como la PCB-K del Brain |
| PCB de encoders | según layout | a −12 mm (base de los EC11), como la PCB-A del Brain |
| NeoTrellis M4 | 120.0 × 60.0 × 8.3 mm | se atornilla por sus agujeros M2.5; el elastómero asoma por el aluminio. Se alimenta por su **micro-USB** (5 V) y se conecta a la portadora por UART con su puerto JST PH-4; trae jack TRRS (sin uso por ahora). Los tres conectores están en el mismo borde largo: ese borde necesita espacio para los cables |
| Cable DSI | largo **verificar** | radio de curvatura amplio; no doblar en filo |

## Conectores (panel trasero o laterales)

- **USB-C 5 V / 3 A de la portadora**: alimenta Teensy, MCP23017, LEDs y NeoTrellis.
- **USB-C de la Pi 5** (fuente oficial de 27 W): alimenta Pi, pantalla y HAT. Son **dos entradas**
  porque la portadora tiene un polyfuse de 3 A y no alcanza para la Pi 5 con la pantalla. Si ves una
  forma limpia de unificarlas en una sola entrada de 5 V / 5 A, propónla.
- **USB del Teensy**: MIDI al computador, o al USB-A host del Brain en standalone (solo datos).
- **Salida de audio** del Audio+ V2: L/R (RCA o 1/4", **verificar** qué conviene) y auriculares 3.5 mm.
- **USB-A de la Pi 5** (opcional, para almacenamiento o un teclado MIDI en standalone).
- **Agujero reservado para entrada de audio** (futuro HAT Mic+), tapado por ahora.
- **Acceso de servicio** a la microSD y al Ethernet de la Pi (tapa o ranura).

## Fabricación del cuerpo

El cuerpo de PETG se imprime en una **Creality Ender 3 V3 (cama de 220 × 220 mm, 250 mm de alto)**.
Ninguna pieza puede pasar de **210 × 210 mm** en planta (margen para el brim). Si el cuerpo no
entra, divídelo en piezas con uniones que alineen solas (lengüeta y ranura, o pines) y tornillos
M3 a insertos, sin que las uniones queden a la vista en el panel ni atraviesen conectores. El
panel de aluminio se corta por CNC y no tiene ese límite.

## Reglas de validación (cada una con pasa / no pasa y el valor medido)

1. Perilla–perilla: ≥ 8 mm entre bordes.
2. Perilla–keycap y perilla–pad: ≥ 6 mm entre bordes.
3. Keycap–keycap: ≥ 1.5 mm entre bordes.
4. Ningún control a menos de 15 mm del borde frontal del panel.
5. Con una mano en los pads y la otra en los encoders, ninguna tapa la pantalla.
6. Cada etiqueta en pantalla queda a ≤ 25 mm horizontales de su encoder.
7. Los 4 faders se pueden mover a la vez con dos dedos de una mano, y su recorrido completo no
   choca con nada.
8. El cable DSI llega sin tensión ni dobleces en filo.
9. Ventilación de la Pi 5 suficiente (indicar cm² de entrada y salida y el camino del aire).
10. Todos los conectores accesibles con cables enchufados, sin chocar entre sí.
11. Huella total (ancho × fondo × alto), comparada con el Push 3 (378 × 293 mm).
12. La pantalla táctil se alcanza cómodamente sin pasar la mano por encima de los encoders.
13. Las 20 teclas quedan agrupadas como en la tabla, y se ubican sin mirar (por posición y grupo).

## Entregables

1. **Vista superior a escala 1:1 en mm** con el centro de cada control y de la pantalla.
2. **Tabla de alineación:** por cada encoder, su centro físico (mm) y el centro de su celda de
   soft-key en pantalla (px), para fijar la GUI.
3. **Corte lateral:** inclinación, alturas y apilado interno (Pi 5 + HAT + cooler, pantalla,
   portadora, PCBs de panel, NeoTrellis M4).
4. **Panel de conectores** con posiciones.
5. **Vista explotada** con piezas y materiales.
6. **Reporte de las 13 reglas** con pasa / no pasa y el valor.
7. **Dos renders de producto terminado** (no prototipo): vista 3/4 de frente y vista trasera.
8. **Lista de medidas a verificar** con calibre antes de fabricar.
9. **Despiece para impresión**: cada pieza del cuerpo con sus medidas en planta (≤ 210 × 210 mm),
   la orientación de impresión y las uniones.
