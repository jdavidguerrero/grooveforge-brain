# Studio · Diseño (UI y enclosure)

> Diseños generados con Claude Design para **GrooveForge Brain · Studio** (ADR-002, ADR-004).
> Última revisión: 2026-10-09.

## Qué hay en esta carpeta

| Archivo / carpeta | Qué es | Estado |
|---|---|---|
| `claude-design-2026-10-09/GrooveForge Studio UI v3.dc.html` | UI con los controles finales y las etiquetas alineadas al enclosure; desde 2026-10-12 incluye OSC (11a–11e, 11c2), LFO / MOD (12a–12b) y el splash animado (08g) | **vigente** (reemplaza v1 y v2) |
| `claude-design-2026-10-09/Boot Splash.dc.html` | splash animado 08g (6.2 s; en el equipo se reproduce una vez) | **vigente** (reemplaza a 08e) |
| `claude-design-2026-10-09/GrooveForge Studio FX Viz.dc.html` | FX MAIN con visualización (ONDA / ESPECTRO / FIRMA) | **vigente** (extiende v3 y reemplaza sus pantallas 04a–04c) |
| `claude-design-2026-10-09/GrooveForge Studio Enclosure.dc.html` | Enclosure de escritorio: vistas 1:1, corte, conectores, explotada, reglas | **vigente** |
| `claude-design-2026-10-09/Enclosure 3D.html` | Modelo 3D simplificado, con descarga OBJ/GLB (`?cheeks=1` muestra las mejillas) | referencia |
| `claude-design-2026-10-09/GrooveForge Studio UI.dc.html`, `… UI v2.dc.html` | UI v1 y v2 | historial |
| `claude-design-2026-10-09/*.dc.html` (TopBar, SoftBar, BottomBar, BottomBar3, Perform*, Notes) | componentes que usan las páginas | — |
| `claude-design-2026-10-09/support.js`, `studio-data.js`, `three-d-stage.js` | runtime de las páginas | no editar |
| `studio-ui-claude-design.html` | UI v1 standalone (abre sin dependencias) | historial |
| `claude-design-prompt.md` | prompt de la v1 (chat nuevo) | — |
| `claude-design-prompt-v2-controles.md`, `claude-design-prompt-enclosure.md` | prompts de controles y enclosure (chat nuevo) | — |
| `claude-design-prompt-osc-lfo.md` | iteración OSC / LFO / MOD (mismo chat de la v3) | — |

**Cómo abrir:** los `.dc.html` necesitan `support.js` en la misma carpeta. Ábrelos en el navegador
desde `claude-design-2026-10-09/`. Las fuentes (IBM Plex) se cargan desde Google Fonts.

---

## Resumen del enclosure (valores para PCB y CAD)

Coordenadas del panel: **x** desde el borde izquierdo, **y** desde el borde frontal, en mm.

- **Forma:** cuña de 12°, 28 mm adelante y 63 mm atrás. Huella **180 × 165 × 63** sin mejillas;
  **204 × 165 × 65** con mejillas.
- **Materiales:** PETG 3 mm (impreso boca abajo, insertos M3), aluminio 6061-T6 de 2 mm anodizado
  negro con 6 espárragos PEM M3 por debajo, fondo PETG 3 mm con patas de goma Ø14 × 6.
- **Pantalla:** centro (90, 112.4); ventana 111 × 67.8 R1; área activa x 36–144, y 80–144.8.

| Control | x | y |
|---|---|---|
| ENC L / ENC R | 32 / 148 | 62 |
| B1 / B2 / B3 / B4 (paso 20 mm) | 60 / 80 / 100 / 120 | 62 |
| P1 / P2 | 32 / 148 | 32 |
| B5 MODE / B6 HOLD-SHIFT | 60 / 120 | 32 |
| NAV (con anillo) | 90 | 32 |
| IN / VOL (a los costados de la pantalla) | 15 / 165 | 96 |

- **Cortes CNC:** Choc 14 × 14 con rebaje de 16 × 16 × 0.8 por debajo (placa de 1.2 para el clip);
  EC11 Ø7.2 (rosca M7, **verificar**); NAV Ø32 con difusor de PC esmerilado de 2 mm; RV09 Ø7;
  29 ranuras de 2.5 × 10 como salida de aire.
- **Apilado bajo el aluminio:** PCB-K (6 Choc + 16 LED SK6812-MINI-E) a −3.4 mm; PCB-A (5 EC11) a
  −8.5 mm; IN y VOL en dos PCB-V con cable JST; módulo de pantalla de 6 mm (asumido) con junta de 0.5.
- **Interior:** Pi 3B en el piso bajo la pantalla, con los puertos hacia atrás; portadora de
  72 × 50 para Teensy 4.1 + Audio Shield en x 103–175, y 76–126; placa de alimentación USB-C con
  polyfuse de 3 A y TVS. **Se corta el puente VUSB–VIN del Teensy**: todo se alimenta del USB-C de 5V.
- **Conectores (pared trasera):** OUT L/R e IN L/R en 1/4"; MIDI IN TRS 3.5; USB-C del Teensy (datos);
  USB-A host; USB-C 5V 3A; CV OUT 3.5; INPUT PAD (palanca de 3 posiciones, Ø6.2) entre las entradas y
  el USB host; tapa de servicio de la Pi (Ethernet y USB) y tapa inferior para la microSD.
- **Mejillas opcionales:** guadua laminada de 12 mm, 4 imanes N42 Ø10 × 2 por lado detrás de una piel
  de PETG de 0.8 mm. No tapan nada.
- **Reglas:** 12 pasan y 1 queda condicionada (cable DSI de al menos 100 mm).
- **Ventilación:** 14.2 cm² efectivos contra 12.8 requeridos (margen 11 %). Hay bosses para un
  ventilador opcional de 30 × 30 mm.

## Resumen de la UI v3

- **Barra inferior** de 800 × 96 en y 384:
  - Fila 1 (64 px): ENC L 0–104 · B1–B4 en segmentos de 148 px (centros 178 / 326 / 474 / 622) · ENC R 696–800.
  - Fila 2 (32 px, solo display): P1 0–200 · MODE 200–296 · NAV 296–504 · B6 504–600 · P2 600–800.
- **Contenido:** 768 × 328 (32 px menos que en la v1).
- **Overlays:** INPUT GAIN en x 0–416 (chip IN en 0–40); VOL en x 384–800 (chip en 760–800).
- **Alineación:** todas las etiquetas quedan a ≤ 25 mm de su control; el peor caso es IN y VOL con 23.7 mm.

## Resumen de la visualización del FX

- Gráfica de 728 × 256 en (56, 120); pestañas de 3 × 88 × 64 en (518, 48); la lista de FX aparece
  temporalmente al girar NAV; medidor de entrada en (16, 48), 24 × 328.
- Solo `lv_draw_rect` relleno (sin líneas ni arcos). Las curvas que dependen solo de parámetros se
  calculan en la UI y se cachean en un `lv_canvas`.
- El motor manda solo el flujo de la vista visible. Pico: ONDA, 480 B por frame × 30 = 14.4 KB/s.

---

## Revisión 2026-10-09: qué hay que resolver

| # | Tema | Acción |
|---|---|---|
| 1 | La regla 7 depende de que el cable DSI de la Freenove mida ≥ 100 mm | medirlo |
| 2 | El diseño pide agujero de **7.2 mm** para el EC11 (rosca M7); `00-master-parameters.md` del comercial usa 6.5 | verificar con la pieza real; si el comercial usa el mismo EC11, su parámetro también está mal |
| 3 | El anillo de NAV como ventana anular de 32/24 deja **suelto el disco central**. Studio lo resuelve con agujero de Ø32 + difusor | **el enclosure comercial tiene el mismo problema** (`RING_EXT_D` 32 / `RING_INT_D` 24) |
| 4 | Al girar IN o VOL, la mano queda a 1.5 mm del área táctil: riesgo de toques accidentales | zona muerta táctil junto a IN/VOL en el Studio head, y probar en una maqueta |
| 5 | IN y VOL se movieron de los extremos de la fila 2 a los costados de la pantalla (si no, ninguna etiqueta cumplía los 25 mm) | reflejado en ADR-004 rev. 2 |
| 6 | B1–B4 a 20 mm de paso (con 18.4 los keycaps quedaban a 0.4 mm) | ya aplicado en la v3 |
| 7 | El panel pide 5 placas: PCB-K, PCB-A, 2 PCB-V y la portadora, más la de alimentación | evaluar simplificaciones en el theory doc del PCB (por ejemplo, EC11 de buje más largo para una sola placa) |
| 8 | El rebaje de 0.8 mm bajo cada Choc es una operación CNC extra en el aluminio | alternativa: placa de switches separada de 1.2 mm |
| 9 | Visualización: WET es la salida después de dry/wet. Hay que confirmar que el motor expone entrada y salida; la vista previa en bypass supone que el FX sigue procesando | definirlo en el theory doc de visualización |
| 10 | ONDA usa 480 B por frame y `02` limita el payload a 255 B | partirlo en 2 frames (theory doc de visualización) |
| 11 | Abiertos de la v3: SHIFT momentáneo o con enclavamiento; nombres de los parámetros 2 y 3; rango de IN en dB; miniaturas de 120 × 80 | decidir / definir en firmware |
| 12 | La v3 lista como abierto el timeout del heartbeat | ya está en el spec: enlace perdido tras **3 s** sin heartbeat (`02` §4.3) |
