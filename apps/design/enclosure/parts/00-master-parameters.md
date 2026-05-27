# GrooveForge Brain — Master Parameters (Fusion 360)

> **Autor:** Industrial Design Agent
> **Version:** 0.1
> **Status:** PROPUESTO
> **Derivado de:** `apps/design/enclosure/01-enclosure-design-spec.md` v0.3, §12
> **Fecha:** 2026-05-26
>
> SSoT de todos los user parameters de Fusion 360 para el ensamblaje GrooveForge Brain.
> Cambiar un valor aqui se propaga a todas las piezas que lo referencian.
> NO editar valores aqui sin actualizar tambien las piezas dependientes (columna "Piezas").

---

## Convencion de coordenadas (Fusion 360 Y-UP)

| Eje | Direccion | Referencia zero |
|-----|-----------|-----------------|
| X | Derecha (vista frontal del instrumento) | Centro del body = X=0; borde izquierdo PETG = X=-90mm |
| Y | Arriba (vertical) | Base del instrumento = Y=0 |
| Z | Hacia el usuario (profundidad, frente = Z positivo) | Panel trasero PETG = Z=0; panel frontal = Z=+100mm |

El top panel esta en el plano XZ a altura Y = BODY_H = 38mm (caja rectangular — v0.6).
Las coordenadas de componentes en §3 del master spec usan el sistema de coordenadas del top panel:
X desde borde izquierdo panel (0 a 180mm), Y_panel desde borde trasero panel (0 a 100mm).
En Fusion se traduce a X_fusion = X_panel - 90, Z_fusion = Y_panel / 10 (cm).
BODY_H_F, BODY_H_R y TILT_ANGLE eliminados en v0.6 — ver §1.

---

## 1. Dimensiones Globales del Cuerpo

> **v0.6 — Caja rectangular para stack modular con Pogo pins**
> Razon: el Brain se apila fisicamente sobre slaves/mixer via Pogo pins en la cara trasera.
> La inclinacion original (H_F=42, H_R=30, 7deg) impedia que el brain asentara plano sobre otros
> modulos. Solucion: caja rectangular unificada H=38mm. La ergonomia se resuelve con un stand
> externo opcional (inclinable 10-15deg, imprimible en PETG) — no forma parte del enclosure.
> Decision tomada: 2026-05-27.

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `BODY_W` | 180.0 | mm | Ancho del PETG body central | 01, 02 | §1 master spec |
| `BODY_D` | 100.0 | mm | Profundidad del PETG body (frente a trasero) | 01, 02, 03, 04 | §1 master spec |
| `BODY_H` | 38.0 | mm | Altura del PETG body (caja rectangular — mismo valor frente y trasero) | 01, 02, 03, 04 | v0.6 stack modular |
| `TOTAL_W` | 204.0 | mm | Ancho total instrumento con cheeks | 99 | §1 master spec |

> **ELIMINADOS en v0.6:** `BODY_H_F` (42mm), `BODY_H_R` (30mm), `TILT_ANGLE` (7deg).
> Usar `BODY_H = 38mm` en todas las piezas. Calculo: base 3mm + standoff PCB 5mm +
> componentes max 25mm + clearance 2mm + sin tapa (aluminio cierra arriba) = 35mm min → 38mm.

> **Stack Pogo:** el conector Pogo (6-pin Wurth WR-WST) permanece en la cara TRASERA (Z=0).
> El stack es una cadena horizontal Brain → [Slave] → [Mixer] con caras traseras comunicadas.
> Las unidades pueden apilarse verticalmente (Brain encima de un slave) porque top/bottom son planos.

---

## 2. Espesores y Clearances

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `WALL_T` | 3.0 | mm | Espesor pared PETG (todos los lados) | 01 | §12 master spec |
| `PANEL_T` | 2.0 | mm | Espesor top panel aluminio | 02 | §12 master spec |
| `CHEEK_T` | 12.0 | mm | Espesor cheeks guadua (cada lado) | 03, 04 | §12 master spec |
| `PCB_W` | 168.0 | mm | Ancho PCB main (BODY_W - 2*WALL_T - 2*PCB_CLEAR) | 01 | §1 master spec |
| `PCB_D` | 88.0 | mm | Profundidad PCB main | 01 | §1 master spec |
| `PCB_CLEAR` | 3.0 | mm | Clearance entre PCB y pared interna PETG | 01 | §1 master spec |
| `CORNER_R` | 3.0 | mm | Radio de esquinas top panel aluminio | 02 | §12 master spec |
| `CHEEK_REBAJE_D` | 6.0 | mm | Profundidad del rebaje PETG para encastre de cheek | 01, 03, 04 | §10 master spec |

---

## 3. Standoffs y Fijaciones

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `STANDOFF_H` | 5.0 | mm | Altura standoffs PCB sobre fondo PETG | 01 | §12 master spec |
| `STANDOFF_OD` | 6.0 | mm | Diametro exterior columna standoff PETG | 01 | Fabricacion estandar M3 |
| `INSERT_M3_OD` | 4.2 | mm | Diametro agujero para heat insert M3 (latarticulation D4.2) | 01, 03, 04 | Estandar Voron/heatset |
| `INSERT_M3_H` | 6.0 | mm | Altura heat insert M3 | 01 | Estandar |
| `SCREW_PANEL_CSINK` | 3.2 | mm | Diametro agujero countersink M3 en aluminio | 02 | Estandar M3 CS |
| `PANEL_MTG_X1` | 10.0 | mm | X desde borde panel: posicion tornillo esquina izq | 02 | Calculo clearance |
| `PANEL_MTG_X2` | 170.0 | mm | X desde borde panel: posicion tornillo esquina der | 02 | Calculo clearance |
| `PANEL_MTG_Y1` | 8.0 | mm | Y_panel desde borde trasero: tornillo trasero | 02 | Calculo clearance |
| `PANEL_MTG_Y2` | 92.0 | mm | Y_panel desde borde frontal: tornillo frontal | 02 | Calculo clearance |
| `PATA_D` | 10.0 | mm | Diametro patas silicona base | 01 | §10 master spec |
| `PATA_H` | 3.0 | mm | Altura patas silicona (eleva base) | 01 | §10 master spec |

---

## 4. Cutouts — Top Panel (encoders)

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `ENC_HOLE` | 6.5 | mm | Diametro cutout shaft encoder EC11 (D-shaft 6mm + 0.5mm) | 02 | §9 master spec |
| `ENC_HOLE_TOL` | 0.05 | mm | Tolerancia cutout shaft encoder (CNC) | 02 | §9 master spec |

### Posiciones encoders (coordenadas panel: X desde borde izq, Y_panel desde borde trasero)

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `ENC1_X` | 22.0 | mm | ENC1 CUTOFF: X desde borde izquierdo panel | 02 | §3 master spec |
| `ENC1_Y` | 55.0 | mm | ENC1 CUTOFF: Y desde borde trasero panel | 02 | §3 master spec |
| `ENC2_X` | 158.0 | mm | ENC2 RESONANCE: X desde borde izquierdo panel | 02 | §3 master spec |
| `ENC2_Y` | 55.0 | mm | ENC2 RESONANCE: Y desde borde trasero panel | 02 | §3 master spec |
| `ENCNAV_X` | 90.0 | mm | ENC NAV ACTION: X (centro panel) | 02 | §3 master spec |
| `ENCNAV_Y` | 22.0 | mm | ENC NAV ACTION: Y desde borde trasero (RESOLUCION D-08: movido de 30 a 22) | 02 | D-08 resuelto |

---

## 5. Cutouts — Top Panel (display)

> **v0.5 — Dimensiones reales del modulo Waveshare ESP32-S3-Touch-LCD-1.28 (datasheet verificado):**
> - Vidrio exterior OD: Ø38.51mm — demasiado grande para pasar por el cutout (correcto: labio de vidrio)
> - Viewing area activa: Ø35.67mm — el cutout debe ser mayor que esto para no obstruir
> - PCB circular: Ø41mm — el bracket debe contenerlo desde abajo
> - Instalacion: modulo va DEBAJO del panel aluminio; vidrio presionado contra cara inferior del panel;
>   el cutout Ø36mm es una ventana — el labio de vidrio (38.51 - 36 = 2.51mm total) impide que caiga.

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `GLASS_OD` | 38.51 | mm | Diametro exterior vidrio GC9A01 (LENS OD datasheet) | 02, 05 | Waveshare datasheet |
| `GLASS_VA` | 35.67 | mm | Viewing area activa del display (LENS V.A datasheet) | 02, 05 | Waveshare datasheet |
| `DISPLAY_CUT` | 36.0 | mm | Diametro cutout ventana en aluminio (>GLASS_VA=35.67mm; <GLASS_OD=38.51mm → labio 1.25mm/lado) | 02 | v0.5 datasheet |
| `DISPLAY_CUT_TOL` | 0.1 | mm | Tolerancia cutout display (CNC) | 02 | §9 master spec |
| `DISPLAY_X` | 90.0 | mm | Display: X desde borde izquierdo panel | 02 | §3 master spec |
| `DISPLAY_Y` | 60.0 | mm | Display: Y desde borde trasero panel (RESOLUCION D-08: movido de 57 a 60) | 02 | D-08 resuelto |

---

## 6. Cutouts — Top Panel (LED ring ENC NAV)

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `RING_EXT_D` | 32.0 | mm | Diametro exterior ventana LED ring (RESOLUCION D-08: reducido de 38 a 32mm) | 02 | D-08 resuelto |
| `RING_INT_D` | 24.0 | mm | Diametro interior ventana LED ring (ajustado proporcionalmente) | 02 | D-08 resuelto |
| `RING_TOL` | 0.1 | mm | Tolerancia cutout anular (CNC) | 02 | §9 master spec |

---

## 7. Cutouts — Top Panel (botones Kailh Choc V2)

> **v0.5 — Layout columnas verticales:** Los botones se agrupan en dos columnas verticales
> (una entre ENC1 y display, otra entre display y ENC2). Cada columna tiene 2 botones
> apilados en Y. Clearance minimo con cualquier elemento adyacente: 13mm.
> Layout: [ENC1] [B1/B2 columna] [DISPLAY] [B3/B4 columna] [ENC2], ENCNAV debajo.

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `BTN_CUT` | 14.0 | mm | Lado del cutout cuadrado botones Choc V2 | 02 | §9 master spec |
| `BTN_CUT_TOL` | 0.1 | mm | Tolerancia cutout botones (CNC) | 02 | §9 master spec |
| `BTN_PITCH_V` | 22.0 | mm | Pitch vertical centro a centro dentro de cada columna (B1↔B2, B3↔B4) | 02 | v0.5 calculo clearance |
| `B_COL_L_X` | 52.0 | mm | X columna izquierda (B1+B2): centrada entre ENC1 knob y borde display | 02 | v0.5 layout |
| `B_COL_R_X` | 128.0 | mm | X columna derecha (B3+B4): simetrica, 180 - B_COL_L_X | 02 | v0.5 layout |
| `BTN_Y_UPPER` | 66.0 | mm | Y_panel boton superior de cada columna (B1, B3) — mas cerca del frente | 02 | v0.5 layout |
| `BTN_Y_LOWER` | 44.0 | mm | Y_panel boton inferior de cada columna (B2, B4) — mas cerca del fondo | 02 | v0.5 layout |
| `B1_X` | 52.0 | mm | B1 ENGINE: X panel (= B_COL_L_X) | 02 | v0.5 |
| `B1_Y` | 66.0 | mm | B1 ENGINE: Y panel (= BTN_Y_UPPER) | 02 | v0.5 |
| `B2_X` | 52.0 | mm | B2 FX: X panel (= B_COL_L_X) | 02 | v0.5 |
| `B2_Y` | 44.0 | mm | B2 FX: Y panel (= BTN_Y_LOWER) | 02 | v0.5 |
| `B3_X` | 128.0 | mm | B3 AI/SCALE: X panel (= B_COL_R_X) | 02 | v0.5 |
| `B3_Y` | 66.0 | mm | B3 AI/SCALE: Y panel (= BTN_Y_UPPER) | 02 | v0.5 |
| `B4_X` | 128.0 | mm | B4 PRESET: X panel (= B_COL_R_X) | 02 | v0.5 |
| `B4_Y` | 44.0 | mm | B4 PRESET: Y panel (= BTN_Y_LOWER) | 02 | v0.5 |

---

## 8. Cutouts — Top Panel (potenciometro volumen)

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `VOL_HOLE` | 7.0 | mm | Diametro cutout shaft Alpha RV09 (D-shaft 6mm + 0.5mm clearance/lado) | 02 | §9 master spec |
| `VOL_HOLE_TOL` | 0.05 | mm | Tolerancia cutout VOL shaft (CNC) | 02 | §9 master spec |
| `VOL_X` | 22.0 | mm | VOL pot: X desde borde izquierdo panel | 02 | §3 master spec |
| `VOL_Y` | 14.0 | mm | VOL pot: Y desde borde trasero panel | 02 | §3 master spec |

---

## 9. Cutouts — Paneles Laterales (jacks audio)

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `JACK_HOLE` | 10.5 | mm | Diametro cutout jack 1/4" TRS | 03, 04 | §9 master spec |
| `JACK_HOLE_PETG_TOL` | 0.3 | mm | Tolerancia cutout jack en PETG (FDM) | 01, 03 | §9 master spec |
| `JACK_HOLE_GUAD_TOL` | 0.5 | mm | Tolerancia cutout jack en guadua (herramienta de mano) | 03, 04 | §9 master spec |
| `JACK_Z` | 21.0 | mm | Altura Z del centro de todos los jacks laterales desde base | 03, 04 | §4, §5 master spec |
| `MINI_JACK_HOLE` | 7.0 | mm | Diametro cutout jack 3.5mm | 03 | §9 master spec |

### Posiciones jacks — Cheek LEFT (Y desde fondo del instrumento, Z=fondo a frente)

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `L_TRS_INL_Z` | 30.0 | mm | Jack TRS IN L: Z desde panel trasero | 03 | §4 master spec |
| `L_TRS_INR_Z` | 55.0 | mm | Jack TRS IN R: Z desde panel trasero | 03 | §4 master spec |
| `L_MINIJACK_Z` | 78.0 | mm | Jack 3.5mm AUX IN: Z desde panel trasero | 03 | §4 master spec |

### Posiciones puertos — Cheek RIGHT (v0.4 — 4 puertos, todo USB-C)

> v0.4: USB-C Power + USB-C Teensy consolidados en 1 conector. USB-A reemplazado por USB-C Host.
> D-10 resuelto: clearance frontal 12.75mm (vs 0.5mm en v0.3).
> Implementacion prototipo v0.2: pigtail interno micro-USB (Teensy dev) → USB-C female panel-mount.

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `R_USBC_DATA_Z` | 15.0 | mm | USB-C (Power + Audio/MIDI Teensy): Z desde panel trasero | 01, 04 | §5 v0.4 |
| `R_TRS_OUTL_Z` | 38.0 | mm | Jack TRS OUT L: Z desde panel trasero | 01, 04 | §5 v0.4 |
| `R_TRS_OUTR_Z` | 60.0 | mm | Jack TRS OUT R: Z desde panel trasero | 01, 04 | §5 v0.4 |
| `R_USBC_HOST_Z` | 82.0 | mm | USB-C Host (MIDI teclado): Z desde panel trasero — D-10 RESUELTO | 01, 04 | §5 v0.4 |

---

## 10. Cutouts — Paneles Laterales (USB)

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `USBC_W` | 10.5 | mm | Cutout USB-C ancho (v0.4: ambos puertos right son USB-C identicos) | 01, 04 | §9 v0.4 |
| `USBC_H` | 5.0 | mm | Cutout USB-C alto | 01, 04 | §9 v0.4 |
| `USBC_PETG_TOL` | 0.3 | mm | Tolerancia cutout USB-C en PETG | 01, 04 | §9 v0.4 |

---

## 11. Cutouts — Panel Trasero (pogo connector)

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `POGO_X` | 90.0 | mm | Pogo 6-pin: X centro desde borde izquierdo PETG | 01 | §6 master spec |
| `POGO_Z` | 0.0 | mm | Pogo: en la cara trasera (Z=0 en convencion Fusion) | 01 | §6 master spec |
| `POGO_Y_CENTER` | 15.0 | mm | Pogo: altura Y desde base | 01 | §6 master spec |
| `POGO_SLOT_W` | 14.0 | mm | Ancho slot pogo 6-pin Wurth WR-WST | 01 | Datasheet Wurth |
| `POGO_SLOT_H` | 8.0 | mm | Alto slot pogo connector | 01 | Datasheet Wurth |

---

## 12. Display Bracket (ESP32-S3 elevado)

| Parametro | Valor | Unidad | Descripcion | Piezas | Fuente |
|-----------|-------|--------|-------------|--------|--------|
| `BRACKET_H` | 15.0 | mm | Altura del bracket desde cara inferior del top panel hasta el PCB principal (el modulo queda suspendido debajo del aluminio) | 05 | §0 decisiones v0.3 |
| `BRACKET_BASE_W` | 45.0 | mm | Ancho base bracket (cavity >= Ø41mm PCB + 2mm paredes x2 = 45mm) | 05 | Waveshare datasheet v0.5 |
| `BRACKET_BASE_D` | 45.0 | mm | Profundidad base bracket (igual a W — PCB es circular) | 05 | Waveshare datasheet v0.5 |
| `BRACKET_WALL_T` | 2.0 | mm | Espesor paredes bracket PETG | 05 | Calculo rigidez |
| `ESP32_MODULE_D_PCB` | 41.0 | mm | Diametro PCB circular del modulo ESP32-S3-Touch-LCD-1.28 (Waveshare datasheet) | 05 | Waveshare datasheet v0.5 |
| `ESP32_LCD_STACK_H` | 3.58 | mm | Altura stack TP+LCD del modulo (side view datasheet) | 05 | Waveshare datasheet |

---

## Script Python — Crear todos los parametros en Fusion 360

El siguiente script crea todos los user parameters.
Shift+S en Fusion → Scripts → New Script → pegar y Run.

```python
import adsk.core
import adsk.fusion

def run(context):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    params = design.userParameters

    definitions = [
        # --- Dimensiones globales ---
        ("BODY_W",          "180 mm",   "Ancho PETG body central"),
        ("BODY_D",          "100 mm",   "Profundidad PETG body (frente a trasero)"),
        ("BODY_H",          "38 mm",    "Altura PETG body — rectangular v0.6 (BODY_H_F/H_R eliminados)"),
        ("TOTAL_W",         "204 mm",   "Ancho total con cheeks"),
        # --- Espesores ---
        ("WALL_T",          "3 mm",     "Espesor pared PETG"),
        ("PANEL_T",         "2 mm",     "Espesor top panel aluminio"),
        ("CHEEK_T",         "12 mm",    "Espesor cheeks guadua"),
        ("PCB_W",           "168 mm",   "Ancho PCB main"),
        ("PCB_D",           "88 mm",    "Profundidad PCB main"),
        ("PCB_CLEAR",       "3 mm",     "Clearance PCB a pared PETG"),
        ("CORNER_R",        "3 mm",     "Radio esquinas top panel"),
        ("CHEEK_REBAJE_D",  "6 mm",     "Profundidad rebaje PETG para cheek"),
        # --- Standoffs ---
        ("STANDOFF_H",      "5 mm",     "Altura standoffs PCB"),
        ("STANDOFF_OD",     "6 mm",     "Diametro exterior standoff"),
        ("INSERT_M3_OD",    "4.2 mm",   "Agujero heat insert M3"),
        ("INSERT_M3_H",     "6 mm",     "Altura heat insert M3"),
        # --- Cutouts encoders ---
        ("ENC_HOLE",        "6.5 mm",   "Cutout shaft encoder EC11"),
        ("ENC1_X",          "22 mm",    "ENC1 CUTOFF X panel"),
        ("ENC1_Y",          "55 mm",    "ENC1 CUTOFF Y panel"),
        ("ENC2_X",          "158 mm",   "ENC2 RESONANCE X panel"),
        ("ENC2_Y",          "55 mm",    "ENC2 RESONANCE Y panel"),
        ("ENCNAV_X",        "90 mm",    "ENC NAV X panel (centro)"),
        ("ENCNAV_Y",        "22 mm",    "ENC NAV Y panel (D-08 resuelto)"),
        # --- Cutouts display (v0.5 — datasheet verificado) ---
        ("GLASS_OD",        "38.51 mm", "Diametro exterior vidrio GC9A01 (LENS OD)"),
        ("GLASS_VA",        "35.67 mm", "Viewing area activa GC9A01 (LENS V.A)"),
        ("DISPLAY_CUT",     "36 mm",    "Cutout ventana display (>VA=35.67, <OD=38.51, labio 1.25mm)"),
        ("DISPLAY_X",       "90 mm",    "Display X panel"),
        ("DISPLAY_Y",       "60 mm",    "Display Y panel (D-08 resuelto)"),
        # --- Cutouts LED ring ---
        ("RING_EXT_D",      "32 mm",    "Diametro exterior ventana LED ring (D-08)"),
        ("RING_INT_D",      "24 mm",    "Diametro interior ventana LED ring"),
        # --- Cutouts botones (v0.5 — columnas verticales) ---
        ("BTN_CUT",         "14 mm",    "Cutout cuadrado botones Choc V2"),
        ("BTN_PITCH_V",     "22 mm",    "Pitch vertical dentro de columna (B1-B2, B3-B4)"),
        ("B_COL_L_X",       "52 mm",    "X columna izquierda botones"),
        ("B_COL_R_X",       "128 mm",   "X columna derecha botones"),
        ("BTN_Y_UPPER",     "66 mm",    "Y boton superior (B1, B3) — cerca del frente"),
        ("BTN_Y_LOWER",     "44 mm",    "Y boton inferior (B2, B4) — cerca del fondo"),
        ("B1_X",            "52 mm",    "B1 ENGINE X panel (v0.5)"),
        ("B1_Y",            "66 mm",    "B1 ENGINE Y panel (v0.5)"),
        ("B2_X",            "52 mm",    "B2 FX X panel (v0.5)"),
        ("B2_Y",            "44 mm",    "B2 FX Y panel (v0.5)"),
        ("B3_X",            "128 mm",   "B3 AI X panel (v0.5)"),
        ("B3_Y",            "66 mm",    "B3 AI Y panel (v0.5)"),
        ("B4_X",            "128 mm",   "B4 PRESET X panel (v0.5)"),
        ("B4_Y",            "44 mm",    "B4 PRESET Y panel (v0.5)"),
        # --- Cutout VOL ---
        ("VOL_HOLE",        "7 mm",     "Cutout shaft Alpha RV09"),
        ("VOL_X",           "22 mm",    "VOL X panel"),
        ("VOL_Y",           "14 mm",    "VOL Y panel"),
        # --- Cutouts jacks laterales ---
        ("JACK_HOLE",       "10.5 mm",  "Cutout jack 1/4 TRS"),
        ("JACK_Z",          "21 mm",    "Altura Z jacks laterales desde base"),
        ("MINI_JACK_HOLE",  "7 mm",     "Cutout jack 3.5mm"),
        ("L_TRS_INL_Z",     "30 mm",    "TRS IN L: Z desde fondo"),
        ("L_TRS_INR_Z",     "55 mm",    "TRS IN R: Z desde fondo"),
        ("L_MINIJACK_Z",    "78 mm",    "3.5mm AUX IN: Z desde fondo"),
        ("R_USBC_DATA_Z",   "15 mm",    "USB-C Power+Data Teensy: Z desde fondo (v0.4)"),
        ("R_TRS_OUTL_Z",    "38 mm",    "TRS OUT L: Z desde fondo"),
        ("R_TRS_OUTR_Z",    "60 mm",    "TRS OUT R: Z desde fondo"),
        ("R_USBC_HOST_Z",   "82 mm",    "USB-C Host MIDI: Z desde fondo (v0.4, D-10 resuelto)"),
        # --- Cutouts USB ---
        ("USBC_W",          "10.5 mm",  "Cutout USB-C ancho"),
        ("USBC_H",          "5 mm",     "Cutout USB-C alto"),
        # --- Pogo trasero ---
        ("POGO_X",          "90 mm",    "Pogo X desde borde izquierdo PETG"),
        ("POGO_Y_CENTER",   "15 mm",    "Pogo altura Y desde base"),
        ("POGO_SLOT_W",     "14 mm",    "Slot pogo ancho"),
        ("POGO_SLOT_H",     "8 mm",     "Slot pogo alto"),
        # --- Bracket ESP32 (v0.5 — PCB circular Ø41mm, modulo debajo del panel) ---
        ("BRACKET_H",           "15 mm",    "Altura bracket desde cara inf panel hasta PCB principal"),
        ("BRACKET_BASE_W",      "45 mm",    "Ancho base bracket (cavity Ø41mm + 2mm pared x2)"),
        ("BRACKET_BASE_D",      "45 mm",    "Profundidad base bracket"),
        ("BRACKET_WALL_T",      "2 mm",     "Espesor paredes bracket PETG"),
        ("ESP32_MODULE_D_PCB",  "41 mm",    "Diametro PCB circular modulo Waveshare"),
        ("ESP32_LCD_STACK_H",   "3.58 mm",  "Altura stack TP+LCD (datasheet side view)"),
    ]

    valueInput = adsk.core.ValueInput
    unitsMgr = design.unitsManager

    for name, expr, comment in definitions:
        existing = params.itemByName(name)
        if existing:
            existing.expression = expr
            existing.comment = comment
        else:
            params.add(name, valueInput.createByString(expr), "", comment)

    app.userInterface.messageBox("Parametros GrooveForge Brain cargados correctamente.")
```

---

## Reglas de actualizacion

1. Todo cambio de valor en este archivo debe propagarse a las piezas listadas en la columna "Piezas".
2. Despues de cambiar un parametro, re-ejecutar el script Python en Fusion para sincronizar.
3. Las tolerancias (columnas `_TOL`) nunca se cambian sin aprobacion del hardware engineer — afectan fit de componentes fisicos.
4. Los parametros de posicion de componentes (`ENC1_X`, `BTN_Y`, etc.) son SSoT. Si el hardware engineer necesita moverlos, el cambio se documenta aqui primero, luego se actualiza el spec maestro.
5. Los parametros marcados como "D-08 resuelto" o "D-10 resuelto" tienen sus nuevos valores ya integrados — no revertir a los valores del spec maestro v0.3.
