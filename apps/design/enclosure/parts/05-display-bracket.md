# Part 05 — Display Bracket (Elevacion ESP32-S3 + GC9A01)

> **Autor:** Industrial Design Agent
> **Version:** 0.2 — Correccion geometria real Waveshare Ø41mm PCB circular; modulo debajo del panel
> **Status:** PROPUESTO
> **Material:** PETG dark anthracite (mismo que el body — impresion en la misma sesion si es posible)
> **Fabricacion:** Impresion 3D FDM
> **Derivado de:** `apps/design/enclosure/01-enclosure-design-spec.md` v0.3, §0 (decisiones v0.3), §7, §12
> **Archivo CAD target:**
> - `apps/design/3d-models/05-display-bracket.f3d`
> - `apps/design/3d-models/05-display-bracket.step`
> - `apps/design/3d-models/05-display-bracket.stl`

---

## 0. Funcion de la pieza

El display bracket es un componente de montaje que resuelve la decision de v0.3:
"ESP32-S3 elevado sobre display — cero footprint PCB adicional".

Sus funciones:
- Elevar el modulo ESP32-S3 Waveshare (que incluye el display GC9A01 integrado) a una altura
  visible por encima del top panel de aluminio
- Mantener el modulo estable mecanicamente durante el uso (vibraciones de transporte, golpes suaves)
- Centrar opticameme el display en el cutout de Ø35mm del top panel
- Rutear el cable/conector entre el modulo elevado y la PCB principal

El resultado visual: el display GC9A01 aparece como un elemento emergente del panel,
no flush (como habia en versiones anteriores). Es un feature estetico — el ESP32-S3 sobre el
display se convierte en un elemento visible y reconocible del instrumento.

### Anatomia del modulo Waveshare ESP32-S3 (datasheet verificado)

El modulo Waveshare ESP32-S3-Touch-LCD-1.28 es una **PCB circular Ø41mm** (no cuadrada).
Dimensiones verificadas del datasheet:
- PCB: Ø41mm circular
- Vidrio exterior (LENS OD): Ø38.51mm
- Viewing area activa (LENS V.A): Ø35.67mm
- Altura total del stack TP+LCD: 3.58mm + PCB 0.50mm = 4.08mm (side view)

**Arquitectura de montaje (v0.5):** el modulo va DEBAJO del top panel de aluminio.
El vidrio (Ø38.51mm) sube por el cutout (Ø36mm) y el labio (labio 1.25mm por lado) queda
apoyado en la cara inferior del aluminio — el vidrio aparece flush con la cara superior del panel.
El bracket sujeta la PCB desde abajo y la empuja hacia el panel.

BRACKET_H = 15mm es la distancia desde la cara inferior del aluminio hasta la PCB principal
(mide cuanto baja el bracket dentro del instrumento).

---

## 1. Material y proceso de fabricacion

| Parametro | Valor |
|-----------|-------|
| Material | PETG dark anthracite (mismo filamento que el body) |
| Proceso | FDM, mismas settings que el body (layer 0.2mm, infill 40%, perim 4) |
| Orientacion de impresion | Con la base del bracket hacia abajo (el bracket se imprime "de pie") |
| Post-proceso | Ninguno (la textura FDM interna no se ve; la parte superior visible es la PCB del modulo) |
| Tolerancia FDM para agujeros | ±0.3mm (aplicar a todos los agujeros de fijacion) |

---

## 2. Parametros requeridos del master

Fuente: `00-master-parameters.md`.

| Parametro | Valor | Descripcion |
|-----------|-------|-------------|
| `BRACKET_H` | 15mm | Distancia desde cara inferior del aluminio hasta PCB principal (profundidad del bracket dentro del instrumento) |
| `BRACKET_BASE_W` | 45mm | Ancho de la base del bracket (cavity Ø41mm PCB + 2mm pared × 2 = 45mm) |
| `BRACKET_BASE_D` | 45mm | Profundidad de la base (igual a W — PCB es circular) |
| `BRACKET_WALL_T` | 2mm | Espesor de las paredes del bracket |
| `ESP32_MODULE_D_PCB` | 41mm | Diametro PCB circular modulo Waveshare (datasheet verificado) |
| `ESP32_LCD_STACK_H` | 3.58mm | Altura stack TP+LCD (side view datasheet) |
| `GLASS_OD` | 38.51mm | Diametro exterior vidrio GC9A01 (LENS OD datasheet) |
| `GLASS_VA` | 35.67mm | Viewing area activa (LENS V.A datasheet) |
| `DISPLAY_X` | 90mm | Posicion X del centro del display en el panel |
| `DISPLAY_Y` | 60mm | Posicion Y del centro del display en el panel (D-08 resuelto) |
| `DISPLAY_CUT` | 36mm | Diametro cutout ventana en aluminio (>GLASS_VA=35.67, <GLASS_OD=38.51 → labio 1.25mm/lado) |
| `PANEL_T` | 2mm | Espesor del top panel aluminio |

### Parametros locales del bracket

| Parametro local | Valor | Descripcion |
|-----------------|-------|-------------|
| `BRACKET_INNER_D` | ESP32_MODULE_D_PCB + 1mm = 42mm | Diametro cavidad circular interior (clearance 0.5mm por lado para PCB Ø41mm) |
| `CABLE_SLOT_W` | 8mm | Ranura lateral para ruteo del conector FPC del modulo a la PCB |
| `CABLE_SLOT_H` | 4mm | Altura de la ranura de cable |
| `BRACKET_FOOT_D` | 34mm | Diametro del labio/collarín superior que retiene la PCB contra el aluminio |
| `BRACKET_FOOT_H` | 15mm | Profundidad del bracket desde cara inferior del aluminio (= BRACKET_H) |

---

## 3. Geometria — sketches y operaciones (en orden)

**Arquitectura (v0.5):** el bracket es un cilindro-cuna que cuelga DEBAJO del top panel.
El vidrio del modulo (Ø38.51mm) asoma por el cutout (Ø36mm) y su labio (1.25mm/lado) descansa
contra la cara inferior del aluminio. El bracket abraza la PCB circular (Ø41mm) desde abajo
y la presiona hacia arriba contra el panel. El bracket se fija al aluminio desde abajo con
2× M2 screws en el flange o double-sided tape VHB.

```
  cara superior aluminio (visible)
  ══════╤══════════╤══════  ← panel Al 2mm
        │  vidrio  │        ← labio Ø38.51mm apoya aqui
        │  Ø36mm   │        ← cutout ventana
  ──────┘──────────└──────  ← cara inferior aluminio
        │  PCB Ø41 │        ← modulo aqui, presionado arriba
  ┌─────┴──────────┴─────┐  ← collarín retención Ø43mm
  │     bracket PETG     │  ← cuerpo cilindrico baja 15mm
  └──────────────────────┘  ← cara inferior bracket (abierta)
```

### Op. 1: Cuerpo cilindrico principal
- Sketch "Bracket_Body" en plano XZ (Y=0 = cara inferior bracket, orientado boca abajo para imprimir)
- Anular: exterior Ø45mm (BRACKET_BASE_W), interior Ø42mm (ESP32_MODULE_D_PCB + 1mm clearance)
- Extrude en Y+: BRACKET_H = 15mm
- Resultado: tubo cilindrico de pared 1.5mm que abraza la PCB Ø41mm

### Op. 2: Collarín de retencion (labio superior)
- Sketch en cara superior del cuerpo (Y = 15mm)
- Anular: exterior Ø45mm, interior Ø36.5mm (< GLASS_OD=38.51mm — el labio de vidrio no pasa)
- Extrude en Y+: 1mm
- El collarín retiene la PCB para que no caiga; el vidrio queda libre para sobresalir por el cutout

### Op. 3: Flange de montaje
- Sketch en cara superior del collarín (Y = 16mm)
- Cuadrado 45×45mm exterior (o manten circular Ø52mm — lo que encaje mejor en el espacio disponible)
- Anular dejando paso al collarín: interior coincide con exterior del cuerpo Ø45mm
- Extrude en Y+: 2mm
- Fijacion: 2× agujeros Ø2.5mm para M2 (o VHB tape) en el flange — montar desde abajo del aluminio

### Op. 4: Ranura de cable FPC
- Sketch "Cable_Slot" en una cara lateral del cuerpo (cara que apunta hacia la PCB principal — lado trasero)
- Rectangulo 8×4mm centrado verticalmente en la pared del cuerpo
- Cut extrude pasante a traves de la pared (1.5mm)
- El cable FPC del modulo sale por aqui hacia el conector en la PCB principal

### Op. 5: Nervaduras de rigidez (condicional)
- Si el prototipo muestra wobble lateral: 3 nervaduras triangulares en la cara exterior del cuerpo
- Nervadura: 5mm base × 8mm altura, BRACKET_WALL_T de espesor, distribuidas a 120°
- Solo agregar si el prototipo fisico lo requiere

---

## 4. Cutouts y holes (tabla completa)

| Elemento | Cara | Forma | Dimension | Tolerancia | Posicion | Notas |
|----------|------|-------|-----------|------------|----------|-------|
| Cavidad PCB (tubo) | Interior | Circular | Ø42mm | ±0.3mm | Centrada | Abraza PCB Ø41mm; clearance 0.5mm/lado |
| Collarín retencion | Superior | Anular | Ø45 ext / Ø36.5 int | ±0.2mm | Centrado | Labio Ø38.51mm del vidrio no pasa; PCB sí queda retenida |
| Flange montaje | Tope | Cuadrado/anular | 45×45mm ext | ±0.3mm | Centrado | 2× M2 o VHB contra cara inferior aluminio |
| Ranura cable FPC | Lateral trasera | Rectangular | 8×4mm | ±0.5mm | Centro vertical pared | Ruteo FPC hacia PCB principal |

---

## 5. Interfaces con otras piezas

| Pieza | Tipo de interface | Tolerancia |
|-------|------------------|------------|
| Top panel aluminio (02) | El bracket cuelga desde la cara INFERIOR del aluminio. El collarín (Ø36.5mm interior) retiene la PCB; el vidrio (Ø38.51mm) sobresale por el cutout (Ø36mm) — labio 1.25mm/lado apoyado contra la cara inferior del aluminio. El flange del bracket se fija con 2× M2 o VHB tape contra el aluminio desde abajo. | Clearance cavidad vs PCB: 0.5mm/lado. Labio vidrio: 1.25mm/lado — suficiente para presion uniforme. |
| Modulo ESP32-S3 Waveshare | La PCB circular Ø41mm se desliza dentro del tubo Ø42mm del bracket. El collarín superior impide que caiga. El vidrio sube hacia el panel. Fijacion final: el mismo apriete del flange contra el aluminio comprime el stack. | Clearance tubo vs PCB: 0.5mm/lado. Sin adhesivo en el modulo — la compresion mecanica lo fija. |
| PCB principal | El cable FPC del modulo sale por la ranura lateral (8×4mm) hacia el conector en la PCB principal. | Estimar 80–100mm de cable FPC con loop para absorber la diferencia de altura entre bracket y PCB. |
| PETG body (01) | El bracket no toca directamente el PETG body. El aluminio (fijo al PETG via 4× M3) transmite las cargas del bracket al chasis. | Sin interfaz directa. |

---

## 6. Validacion post-fabricacion

| Check | Metodo | Criterio pass |
|-------|--------|---------------|
| Altura total bracket | Calibre | BRACKET_H = 15mm ±0.5mm desde la base del flange |
| Pie circular Ø32mm | Calibre | 32mm ±0.3mm |
| Fit en cutout Ø35mm | Insertar el pie en el cutout del aluminio impreso en PLA | Entra sin fuerza; puede desplazarse 1.5mm en cualquier direccion (clearance intencional) |
| Cavidad modulo 25.9mm | Insertar modulo ESP32-S3 fisico | Modulo entra sin forzar; sin wobble excesivo (max 0.5mm de juego) |
| Flange planitud | Regla de metal sobre el flange | Sin alabeo; asienta sobre el aluminio sin rockeo |
| Ranura cable | Pasar un cable FPC de 6mm de ancho | Cable pasa sin forzar |
| Rigidez del bracket | Aplicar fuerza lateral de 2N en la parte superior del modulo | Sin deformacion visible; sin wobble |

---

## 7. Export targets

| Archivo | Formato | Path | Uso | Settings |
|---------|---------|------|-----|---------|
| Bracket Fusion archive | .f3d | `apps/design/3d-models/05-display-bracket.f3d` | Edicion | Save from Fusion |
| Bracket STEP | .step | `apps/design/3d-models/05-display-bracket.step` | Referencia | AP203, mm |
| Bracket STL | .stl | `apps/design/3d-models/05-display-bracket.stl` | Impresion 3D | 0.01mm tolerance, 10 deg angle |

---

## 8. Workflow Fusion 360 (paso a paso ejecutable)

```
1.  Activar componente "05_Display_Bracket" en el browser.

2.  Sketch "Bracket_Foot_Base" en plano XZ (Y=0):
    - Circulo Ø32mm centrado en (0,0).
    Finish Sketch.

3.  Extrude "Bracket_Foot_Base": BRACKET_FOOT_H = 5mm en Y+. New Body.

4.  Sketch "Bracket_Flange" en la cara superior del pie (Y=5mm):
    - Rectangulo 40×40mm centrado en (0,0). Fillet R3mm en esquinas.
    Finish Sketch.

5.  Extrude "Bracket_Flange": 2mm en Y+. Join con el cuerpo existente.

6.  Sketch "Bracket_Outer" en la cara superior del flange (Y=7mm):
    - Rectangulo exterior 40×40mm con fillet R3mm (mismo que el flange).
    - Rectangulo interior 25.9×25.9mm (cavidad para el modulo).
    - El sketch tiene el anular entre los dos rectangulos (las paredes).
    Finish Sketch.

7.  Extrude "Bracket_Outer": 13mm en Y+ (total bracket sobre panel = 2+13=15mm = BRACKET_H).
    Join. Solo el material entre el rectangulo exterior e interior se extrude (las paredes).

8.  Sketch "Cable_Slot" en cara trasera del bracket (la cara en Z=BRACKET_BASE_D/2=20mm):
    - Rectangulo 8×4mm centrado en la cara, aproximadamente en Y=10mm (centro de altura del bracket).
    - Ajustar la posicion Y segun la direccion del cable del modulo.
    Finish Sketch.

9.  Cut extrude "Cable_Slot": pasante en Z-, profundidad BRACKET_WALL_T=2mm.

10. Posicionar en ensamblaje:
    En 99-assembly.f3d, el centro del pie del bracket debe coincidir con el centro del cutout
    del display en el aluminio: X_fusion=0 (que es DISPLAY_X=90mm del borde izq en coord panel),
    Z_fusion=60mm (DISPLAY_Y).
    El flange asienta sobre la cara superior del top panel (Y=PANEL_T=2mm desde la cara inferior
    del aluminio, o Y=altura del techo inclinado del PETG + PANEL_T desde la base del instrumento).

11. Verificar interferencias en el ensamblaje:
    - El pie del bracket (Ø32mm) debe pasar por el cutout del aluminio (Ø35mm) sin tocar.
    - El flange del bracket (40×40mm) no debe solaparse con los cutouts de botones adyacentes:
      B2 esta en X_panel=58mm (X_fusion=-32mm), el borde derecho del flange esta en X_fusion=+20mm.
      Gap: -32-14/2 = -39mm borde izq B2 vs +20mm borde der flange: gap = -39-20 = -59mm.
      ATENCION: la convencion de X en el panel vs X_fusion puede confundir. Verificar en el modelo.

12. Exportar STL: orientado con el flange hacia abajo (cara superior del pie = Z+ en STL).
    Guardar en `apps/design/3d-models/05-display-bracket.stl`.
```

### Nota sobre la altura BRACKET_H = 15mm

El valor de 15mm es una estimacion inicial. Los factores que lo determinan son:
1. La altura del flange del modulo ESP32-S3 Waveshare sobre el top panel — debe ser perceptiblemente visible desde el frente
2. El angulo de vision del display desde el frente del instrumento: con el instrumento inclinado 7 grados y el display a 15mm de altura, el angulo de vision del display cambia respecto a si estuviera flush con el panel
3. La estabilidad mecanica: a mayor altura, mayor momento de palanca ante golpes laterales

Validar BRACKET_H con prototipo fisico impreso en PLA antes de definir como DEFINIDO.
Valores alternativos a evaluar: 10mm (mas conservador), 20mm (mas prominente).
