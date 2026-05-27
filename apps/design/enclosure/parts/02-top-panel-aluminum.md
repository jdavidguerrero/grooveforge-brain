# Part 02 — Top Panel Aluminio CNC

> **Autor:** Industrial Design Agent
> **Version:** 0.2 — v0.5 layout: botones en columnas verticales
> **Status:** PROPUESTO
> **Material:** Aluminio 6061-T6, 2mm, anodizado anthracite clase 2
> **Fabricacion:** CNC local Bogota + anodizado + laser engraving
> **Derivado de:** `apps/design/enclosure/01-enclosure-design-spec.md` v0.3, §2, §3, §9, §12
> **Archivo CAD target:**
> - `apps/design/3d-models/02-top-panel.f3d`
> - `apps/design/3d-models/02-top-panel.step` (para CNC)
> - `apps/design/3d-models/02-top-panel.dxf` (si el proveedor requiere 2D)

---

## Fabrication Spec — Top Panel

- Archivo: `02-top-panel.step` (rev 0.1)
- Material: Aluminio 6061-T6, espesor 2mm
- Acabado: Anodizado clase 2, color anthracite RAL 7016 aproximado (especificar como "dark grey/black anodize" al proveedor)
- Laser engraving post-anodizado: labels de componentes (ENGINE, FX, AI, PRESET, encima de cada boton; CUTOFF sobre ENC1; RESONANCE sobre ENC2; valores de referencia en ingles)
- Tolerancias: cutouts circulares y rectangulares ±0.1mm; holes shafts ±0.05mm
- Proveedor recomendado: CNC local Bogota (por confirmar — ver decision log)
- Tiempo estimado: 5-7 dias habiles
- Costo estimado: $6.00/unidad @ qty 100
- Esquinas: R3mm (CORNER_R) en las 4 esquinas del rectangulo exterior

---

## 0. Funcion de la pieza

El top panel es la superficie principal de contacto del usuario con el instrumento. Sus funciones:

- Cubre y protege la electronica interna (techo del PETG body)
- Provee los cutouts precisos para todos los controles del panel frontal (encoders, botones, display, VOL)
- Soporta el display bracket + modulo ESP32-S3 (a traves del cutout central del display)
- Aporta la rigidez estructural que el PETG no puede garantizar en la zona de controles
- Define la estetica visual primaria del instrumento: superficie anthracite con labels grabados
- Las 4 esquinas atornilladas al PETG son el elemento de cierre del enclosure

El top panel NO se extiende sobre los cheeks de guadua. La junta aluminio/guadua en los bordes
laterales del panel es un detalle de diseno explicito que separa visualmente los materiales.

---

## 1. Material y proceso de fabricacion

| Parametro | Valor |
|-----------|-------|
| Material | Aluminio 6061-T6, placa de 2mm |
| Proveedor materia prima | Ferreteria industrial Bogota (placa 6061-T6 disponible en Bogota) |
| Proceso de corte exterior | CNC fresado 2.5D (contorno exterior + todos los cutouts en una operacion) |
| Proceso anodizado | Anodizado sulfurico clase 2 (tipo II), 15-25 micras, color anthracite / black |
| Proceso laser | Post-anodizado: laser fiber engraving para labels. El laser remueve la anodizacion, dejando aluminio brillante sobre fondo negro — efecto de alta calidad. |
| Tolerancia CNC | ±0.1mm general; ±0.05mm en holes de shafts (usar reamado o pase de acabado) |
| Acabado superficie | Sin lijar adicional post-CNC — la textura del CNC con insert de aluminio a 3000+ RPM es suficientemente limpia para anodizar |
| Desbarbado | Manual con lima fina o vibratorio antes de anodizar — critico para calidad de anodizado |
| Forma | Plana (2.5D — no requiere mecanizado de 5 ejes) |

El panel es geometricamente plano — es la pieza mas sencilla de fabricar. El riesgo
principal es el placement exacto de los cutouts: un error de 0.5mm en la posicion del
shaft hole de un encoder hace que el knob quede descentrado visualmente.

---

## 2. Parametros requeridos del master

Fuente: `00-master-parameters.md`.

| Parametro | Valor | Descripcion |
|-----------|-------|-------------|
| `BODY_W` | 180mm | Ancho del panel |
| `BODY_D` | 100mm | Profundidad del panel |
| `PANEL_T` | 2mm | Espesor del panel |
| `CORNER_R` | 3mm | Radio esquinas exteriores |
| `ENC_HOLE` | 6.5mm | Diametro hole shaft encoder |
| `ENC_HOLE_TOL` | 0.05mm | Tolerancia holes shafts |
| `ENC1_X` | 22mm | ENC1 CUTOFF: X desde borde izquierdo |
| `ENC1_Y` | 55mm | ENC1 CUTOFF: Y desde borde trasero |
| `ENC2_X` | 158mm | ENC2 RESONANCE: X |
| `ENC2_Y` | 55mm | ENC2 RESONANCE: Y |
| `ENCNAV_X` | 90mm | ENC NAV: X (centro) |
| `ENCNAV_Y` | 22mm | ENC NAV: Y (D-08 resuelto) |
| `DISPLAY_CUT` | 36mm | Diametro cutout display (>GLASS_VA=35.67mm; <GLASS_OD=38.51mm → labio 1.25mm/lado) |
| `DISPLAY_CUT_TOL` | 0.1mm | Tolerancia cutout display |
| `DISPLAY_X` | 90mm | Display: X |
| `DISPLAY_Y` | 60mm | Display: Y (D-08 resuelto) |
| `RING_EXT_D` | 32mm | Diametro exterior ventana LED ring (D-08) |
| `RING_INT_D` | 24mm | Diametro interior ventana LED ring |
| `RING_TOL` | 0.1mm | Tolerancia cutout anular ring |
| `BTN_CUT` | 14mm | Lado cutout cuadrado botones Choc V2 |
| `BTN_CUT_TOL` | 0.1mm | Tolerancia cutout botones |
| `BTN_PITCH_V` | 22mm | Pitch vertical dentro de cada columna |
| `B_COL_L_X` | 52mm | X columna izquierda (B1+B2) |
| `B_COL_R_X` | 128mm | X columna derecha (B3+B4) |
| `BTN_Y_UPPER` | 66mm | Y boton superior (B1, B3) |
| `BTN_Y_LOWER` | 44mm | Y boton inferior (B2, B4) |
| `B1_X` | 52mm | B1 ENGINE: X (v0.5) |
| `B1_Y` | 66mm | B1 ENGINE: Y (v0.5) |
| `B2_X` | 52mm | B2 FX: X (v0.5) |
| `B2_Y` | 44mm | B2 FX: Y (v0.5) |
| `B3_X` | 128mm | B3 AI: X (v0.5) |
| `B3_Y` | 66mm | B3 AI: Y (v0.5) |
| `B4_X` | 128mm | B4 PRESET: X (v0.5) |
| `B4_Y` | 44mm | B4 PRESET: Y (v0.5) |
| `VOL_HOLE` | 7mm | Diametro hole shaft VOL |
| `VOL_HOLE_TOL` | 0.05mm | Tolerancia hole VOL |
| `VOL_X` | 22mm | VOL: X |
| `VOL_Y` | 14mm | VOL: Y |
| `PANEL_MTG_X1` | 10mm | X tornillo esquina izquierda |
| `PANEL_MTG_X2` | 170mm | X tornillo esquina derecha |
| `PANEL_MTG_Y1` | 8mm | Y tornillo trasero |
| `PANEL_MTG_Y2` | 92mm | Y tornillo frontal |
| `SCREW_PANEL_CSINK` | 3.2mm | Diametro agujero countersink M3 |

---

## 3. Geometria — sketches y operaciones (en orden)

### Op. 1: Contorno exterior del panel
- Plano: XZ de Fusion (plano horizontal, Y=0 para el panel — luego se posiciona a la altura correcta)
- Sketch "Panel_Outline" en plano XZ
- Geometria: rectangulo de BODY_W × BODY_D = 180mm × 100mm
- Esquinas: fillet R=CORNER_R = 3mm (aplicar como fillet en el sketch con la funcion Fillet de sketch)
- Origen del sketch: borde izquierdo = X=-90mm, borde trasero = Z=0 (para que el centro del panel quede en X=0, Z=50mm)

### Op. 2: Extrude del contorno
- Extrude en Y+: PANEL_T = 2mm
- Resultado: plancha de 180×100×2mm

### Op. 3: Sketch de todos los cutouts (en un solo sketch para eficiencia)
- Nombre: "Panel_Cutouts"
- Mismo plano que "Panel_Outline" (cara superior del panel)
- Nota sobre sistema de coordenadas del sketch: las posiciones de componentes en el master spec
  usan X_panel (0 a 180mm desde borde izquierdo) e Y_panel (0 a 100mm desde borde trasero).
  En el sketch de Fusion (origen en X=-90, Z=0):
  X_fusion = X_panel - 90
  Z_fusion = Y_panel (porque Y_panel crece hacia el frente = Z+)

Posiciones transformadas (v0.5 — botones en columnas verticales):

| Componente | X_panel | Y_panel | X_fusion | Z_fusion | Notas |
|------------|---------|---------|----------|----------|-------|
| ENC1 CUTOFF | 22 | 55 | -68mm | 55mm | |
| ENC2 RESONANCE | 158 | 55 | +68mm | 55mm | |
| ENC NAV ACTION | 90 | 22 | 0mm | 22mm | |
| Display GC9A01 | 90 | 60 | 0mm | 60mm | |
| B1 ENGINE | 52 | 66 | -38mm | 66mm | columna izq, boton superior |
| B2 FX | 52 | 44 | -38mm | 44mm | columna izq, boton inferior |
| B3 AI/SCALE | 128 | 66 | +38mm | 66mm | columna der, boton superior |
| B4 PRESET | 128 | 44 | +38mm | 44mm | columna der, boton inferior |
| VOL pot | 22 | 14 | -68mm | 14mm | |
| Tornillo MTG LL | 10 | 8 | -80mm | 8mm | |
| Tornillo MTG LF | 10 | 92 | -80mm | 92mm | |
| Tornillo MTG RL | 170 | 8 | +80mm | 8mm | |
| Tornillo MTG RF | 170 | 92 | +80mm | 92mm | |

### Geometria del sketch "Panel_Cutouts":

- **ENC1, ENC2, ENC NAV shafts:** circulo Ø ENC_HOLE = 6.5mm en sus posiciones
- **Display GC9A01:** circulo Ø DISPLAY_CUT = 35mm en (X=0, Z=60mm)
- **LED ring ENC NAV:** anillo definido por dos circulos concentricos en (X=0, Z=22mm):
  circulo exterior Ø RING_EXT_D = 32mm, circulo interior Ø RING_INT_D = 24mm.
  El area entre los dos circulos es el cutout anular (la ventana visual del ring).
- **B1, B2, B3, B4:** cuadrado BTN_CUT × BTN_CUT = 14×14mm centrado en cada posicion
  (usar rectangulo centrado en el sketch)
- **VOL shaft:** circulo Ø VOL_HOLE = 7mm en (-68mm, 14mm)
- **Tornillos:** circulo Ø SCREW_PANEL_CSINK = 3.2mm en las 4 posiciones de montaje

### Op. 4: Cut extrude de todos los cutouts
- Seleccionar todas las geometrias del sketch "Panel_Cutouts" (todos los cutouts)
- Cut extrude pasante (through all) — PANEL_T = 2mm
- EXCEPCION: los agujeros de tornillos (Ø3.2mm) se hacen con countersink (avellanado):
  - Drill Ø3.2mm pasante + countersink Ø6mm a 90° para cabeza M3 CS
  - En Fusion usar "Thread" o "Countersink" feature, no extrude simple

---

## 4. Cutouts y holes (tabla completa)

Todas las coordenadas en sistema X_panel (0-180mm desde borde izquierdo),
Y_panel (0-100mm desde borde trasero).

| Componente | Forma | Dim. nominal | Tolerancia | X_panel | Y_panel | Notas |
|------------|-------|-------------|------------|---------|---------|-------|
| ENC1 CUTOFF shaft | Circular | Ø6.5mm | ±0.05mm | 22mm | 55mm | D-shaft EC11, pase de reamado |
| ENC2 RESONANCE shaft | Circular | Ø6.5mm | ±0.05mm | 158mm | 55mm | D-shaft EC11, pase de reamado |
| ENC NAV ACTION shaft | Circular | Ø6.5mm | ±0.05mm | 90mm | 22mm | D-08 resuelto: Y=22 |
| Display GC9A01 ventana | Circular | Ø36.0mm | ±0.1mm | 90mm | 60mm | Ventana; vidrio OD=38.51mm descansa en cara inf panel; labio 1.25mm/lado |
| LED ring ventana anular exterior | Circular | Ø32.0mm | ±0.1mm | 90mm | 22mm | D-08: ring reducido. Borde ring = X+/-16mm desde encnav. |
| LED ring ventana anular interior | Circular | Ø24.0mm | ±0.1mm | 90mm | 22mm | Solo el area entre ext e int se corta. |
| B1 ENGINE | Cuadrado | 14.0×14.0mm | ±0.1mm | 52mm | 66mm | Choc V2, col izq superior |
| B2 FX | Cuadrado | 14.0×14.0mm | ±0.1mm | 52mm | 44mm | Choc V2, col izq inferior |
| B3 AI/SCALE | Cuadrado | 14.0×14.0mm | ±0.1mm | 128mm | 66mm | Choc V2, col der superior |
| B4 PRESET | Cuadrado | 14.0×14.0mm | ±0.1mm | 128mm | 44mm | Choc V2, col der inferior |
| VOL shaft (Alpha RV09) | Circular | Ø7.0mm | ±0.05mm | 22mm | 14mm | D-shaft 6mm + 0.5mm clearance/lado |
| Tornillo MTG trasero izquierdo | Circular + csink | Ø3.2mm + csink Ø6mm | ±0.1mm | 10mm | 8mm | M3 countersunk SS |
| Tornillo MTG frontal izquierdo | Circular + csink | Ø3.2mm + csink Ø6mm | ±0.1mm | 10mm | 92mm | M3 countersunk SS |
| Tornillo MTG trasero derecho | Circular + csink | Ø3.2mm + csink Ø6mm | ±0.1mm | 170mm | 8mm | M3 countersunk SS |
| Tornillo MTG frontal derecho | Circular + csink | Ø3.2mm + csink Ø6mm | ±0.1mm | 170mm | 92mm | M3 countersunk SS |

### Verificacion de clearances minimos entre cutouts adyacentes (regla: >3mm)

v0.5 — botones en columnas verticales. Clearances calculados con radio knob ENC = 10mm.

| Par | Gap borde a borde | Status |
|-----|-------------------|--------|
| ENC1 knob (r=10, X=22) ↔ B1/B2 (14mm, X=52) | 13.3mm | OK (holgura generosa) |
| B1↔B2 vertical (mismo X=52, Y=66 y Y=44) | 8.0mm | OK |
| B1 (X=52,Y=66) ↔ Display (Ø35, X=90,Y=60) | 13.5mm | OK |
| B2 (X=52,Y=44) ↔ Display (Ø35, X=90,Y=60) | 14.8mm | OK |
| B2 (X=52,Y=44) ↔ ENC NAV ring ext (Ø32, X=90,Y=22) | 18.4mm | OK |
| ENC2 knob (r=10, X=158) ↔ B3/B4 (14mm, X=128) | 13.3mm | OK |
| B3↔B4 vertical (mismo X=128, Y=66 y Y=44) | 8.0mm | OK |
| ENC NAV ring ext (Ø32, Y=22) ↔ Display (Ø35, Y=60) | 4.5mm | OK (D-08 resuelto) |
| VOL (Ø7, X=22,Y=14) ↔ ENC NAV ring (X=90,Y=22) | 49mm | OK |
| Tornillo MTG (Ø3.2, X=10) ↔ ENC1 shaft (X=22) | 7.15mm | OK |
| ENC NAV shaft (Ø6.5) ↔ ring interior (Ø24) — concentricos | concentrico | OK (diseñado asi) |

---

## 5. Interfaces con otras piezas

| Pieza | Tipo de interface | Tolerancia |
|-------|------------------|------------|
| PETG body (01) | 4 tornillos M3×6 SS countersunk pasan por el aluminio y van a los heat inserts del PETG. El panel asienta sobre el borde superior del PETG (clearance 0.1mm para que el aluminio quede flush o levemente elevado). | El panel debe poder deslizarse sobre el PETG sin fuerza antes de atornillar. Tolerancia de posicion XZ: ±0.3mm. |
| Cheeks guadua (03, 04) | Junta limpia en los bordes laterales (X=0 y X=180 del panel). Sin solape ni tornillos — la guadua es adyacente pero no debajo del aluminio. El gap visible entre aluminio y guadua es un feature de diseno: apuntar a 0.5-1mm de gap limpio. | El panel no fija los cheeks — los cheeks los fija el PETG. El aluminio solo define el gap visual. |
| Display bracket (05) | El bracket se apoya sobre la cara superior del aluminio o pasa a traves del cutout del display. El cutout Ø35mm debe alinearse con el modulo display GC9A01 (Ø32mm de vidrio, Ø35mm de pcb). | El bracket tiene que poder insertarse por el cutout del display o asientarse encima sin taper. Verificar con modelo 3D. |
| Componentes electronicos | Los knobs (Ø18mm ENC1/2, Ø22mm ENC NAV) asientan sobre la cara superior del aluminio. El aluminio 2mm es la unica superficie de referencia para la altura de los knobs. | La planitud del panel es critica: una variacion de 0.3mm en planitud se percibe como wobble en el knob. |

---

## 6. Validacion post-fabricacion

| Check | Metodo | Criterio pass |
|-------|--------|---------------|
| Planitud general | Apoyo sobre superficie de referencia + feeler gauge | Desviacion maxima 0.2mm en cualquier punto |
| Dimension exterior 180×100mm | Calibre digital en 4 bordes | ±0.2mm |
| Holes shafts encoders Ø6.5mm | Pin gauge o calibre de interno | 6.5mm ±0.05mm (no mas: el shaft D-tipo debe girar sin holgura) |
| Hole VOL Ø7.0mm | Pin gauge | 7.0mm ±0.05mm |
| Cutout display Ø35mm | Calibre de arco o template | 35mm ±0.1mm |
| Ring anular: ext Ø32mm, int Ø24mm | Calibre de arco | Cada diametro ±0.1mm |
| Cutouts botones 14×14mm | Insertar keycap Choc V2 real | Pasa sin forzar, sin juego >0.2mm |
| Countersinks M3 | Tornillo M3 CS debe quedar flush o 0.1mm bajo la superficie | Sin protuberancia sobre la superficie del panel |
| Anodizado post-CNC | Inspeccion visual | Sin zonas sin anodizar, sin burbujas, color uniforme |
| Laser engraving | Inspeccion visual | Lineas nitidas, legibles, centradas sobre cada componente |
| Fit sobre PETG body | Apoyar panel sobre PETG sin tornillos | Panel asienta plano, sin rockeo, holes de tornillo alinean con inserts |

---

## 7. Export targets

| Archivo | Formato | Path | Uso | Settings |
|---------|---------|------|-----|----------|
| Top panel Fusion archive | .f3d | `apps/design/3d-models/02-top-panel.f3d` | Edicion | Save from Fusion |
| Top panel STEP | .step | `apps/design/3d-models/02-top-panel.step` | Para CNC Bogota | AP203 o AP214, unidades mm |
| Top panel DXF | .dxf | `apps/design/3d-models/02-top-panel.dxf` | Si el proveedor CNC requiere 2D | Export cara superior como DXF 2D (en Fusion: File > Export > DXF, seleccionar cara) |

El STEP es el formato primario para el CNC. El DXF es backup — algunos proveedores locales prefieren 2D.

---

## 8. Workflow Fusion 360 (paso a paso ejecutable)

```
1.  Activar el componente "02_Top_Panel_Aluminum" en el browser (New Component).

2.  Crear un offset construction plane a la altura del techo frontal del PETG:
    En el ensamblaje, esta altura es variable (inclinada). Para el panel plano, modelarlo
    en Y=0 primero y luego posicionarlo al ensamblar.
    Para modelado independiente: usar el plano XZ en Y=0.

3.  Sketch "Panel_Outline" en plano XZ (Y=0):
    - Rectangulo de -90mm a +90mm en X, de 0mm a +100mm en Z.
    - Fillet de 3mm en las 4 esquinas (Sketch > Fillet).
    - Finish Sketch.

4.  Extrude "Panel_Outline": 2mm en Y+ (PANEL_T = 2mm). New Body.

5.  Sketch "Panel_Cutouts" en la cara superior del panel (Y=2mm, plano paralelo al XZ):
    Usando la tabla de posiciones transformadas (X_fusion = X_panel - 90, Z_fusion = Y_panel):

    a. ENC1: Circle Ø6.5mm center (-68, 55)
    b. ENC2: Circle Ø6.5mm center (+68, 55)
    c. ENC NAV shaft: Circle Ø6.5mm center (0, 22)
    d. Display ventana: Circle Ø36mm center (0, 60)  ← ventana; vidrio OD=38.51mm descansa debajo
    e. Ring exterior: Circle Ø32mm center (0, 22)
    f. Ring interior: Circle Ø24mm center (0, 22)
       NOTA: los circulos e y f son concentricos con c. En el extrude de cut,
       seleccionar SOLO el area entre el circulo ext e int (anillo).
       En Fusion, hacer Trim en el sketch para dejar solo el anillo visible,
       o usar dos cut-extrudes separados (primero Ø32, luego repair con Ø24 como join).
       Alternativa mas simple: un cut-extrude con el circulo Ø32mm, luego un extrude JOIN
       con un cilindro Ø24mm de 2mm (rellena el centro) — esto da el anillo.
    g. B1 ENGINE:  Rectangle 14×14mm centered at (-38, 66)  ← col izq, superior
    h. B2 FX:     Rectangle 14×14mm centered at (-38, 44)  ← col izq, inferior
    i. B3 AI:     Rectangle 14×14mm centered at (+38, 66)  ← col der, superior
    j. B4 PRESET: Rectangle 14×14mm centered at (+38, 44)  ← col der, inferior
    k. VOL: Circle Ø7mm center (-68, 14)
    l. Tornillo MTG LL: Circle Ø3.2mm center (-80, 8)
    m. Tornillo MTG LF: Circle Ø3.2mm center (-80, 92)
    n. Tornillo MTG RL: Circle Ø3.2mm center (+80, 8)
    o. Tornillo MTG RF: Circle Ø3.2mm center (+80, 92)

    Finish Sketch.

6.  Cut-extrude "Cutouts_Main": seleccionar los perfiles de cutouts a, b, c, d, g, h, i, j, k.
    Cut, Through All (o exactamente PANEL_T = 2mm).
    ATENCION: NO incluir los circulos del ring ni los de tornillos en este paso.

7.  Cut-extrude "Ring_Outer": seleccionar circulo Ø32mm del ring.
    Cut, Through All.

8.  Join-extrude "Ring_Fill": Sketch en la cara inferior del panel (Y=0) o nuevo sketch.
    Circulo Ø24mm centrado en (0, 22).
    Extrude JOIN, 2mm en Y+. Esto rellena el centro del anillo cortado.
    Resultado final: solo el area anular entre Ø24 y Ø32 esta cortada.

9.  Countersink para 4 tornillos:
    - Hole feature en Fusion: seleccionar cara superior del panel.
    - Tipo: Counterbore o Countersink.
    - Para M3 CS standard: Drill Ø3.2mm (clearance hole), Countersink Ø6mm, angulo 90°.
    - Repetir para las 4 posiciones de montaje.

10. Shaft holes con tolerancia final (opcional, si el CNC no puede hacer ±0.05mm en el sketch):
    Hacer un Hole feature separado para los 3 shaft holes (ENC1, ENC2, ENCNAV, VOL):
    Ø6.5mm (±0.05mm), Through, sin countersink.
    En las notas para el CNC: especificar que estos holes requieren pase de acabado.

11. Verificar interferencias: en el ensamblaje, posicionar el top panel sobre el PETG body
    y usar Inspect > Interference. Corregir antes de exportar.

12. Exportar: File > Export > .step (AP214, mm). Guardar en `apps/design/3d-models/02-top-panel.step`.
    Exportar DXF de la cara superior: seleccionar cara > Export Sketch as DXF.
```

### Notas para el proveedor CNC (incluir en el pedido)

- Material: aluminio 6061-T6, placa 2mm (proveer placa o pedir al CNC que consiga)
- Todos los cutouts deben ser pasantes (through)
- Holes circulares Ø6.5mm y Ø7.0mm: requieren tolerancia ±0.05mm (pase de reamado o pase de acabado fino)
- Demas cutouts: tolerancia ±0.1mm es aceptable
- Esquinas rectangulares de botones (14×14mm): radio de herramienta CNC dejara un pequeño radio interior. Maximo aceptable: R0.5mm (herramienta Ø1mm en el paso de acabado de esquinas).
- Post-CNC: desbarbar todos los bordes y aristas antes de anodizar
- Anodizar: tipo II (sulfurico) clase 2, color black/anthracite, 15-25 micras
- Laser engraving: post-anodizado, proveer archivo .dxf o .svg con el arte de los labels
