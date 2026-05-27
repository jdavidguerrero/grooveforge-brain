# Part 01 — PETG Body (Cuerpo Central)

> **Autor:** Industrial Design Agent
> **Version:** 0.2 — USB right side v0.4: 5 cutouts → 4 cutouts, todo USB-C
> **Status:** PROPUESTO
> **Material:** PETG dark anthracite (color-stable)
> **Fabricacion:** Impresion 3D FDM
> **Derivado de:** `apps/design/enclosure/01-enclosure-design-spec.md` v0.3, §1, §10, §12, §13
> **Archivo CAD target:**
> - `apps/design/3d-models/01-petg-body.f3d`
> - `apps/design/3d-models/01-petg-body.step`
> - `apps/design/3d-models/01-petg-body.stl`

---

## 0. Funcion de la pieza

El PETG body es el chasis estructural del instrumento. Todas las demas piezas se montan en el o
sobre el. Sus funciones:

- Soporte estructural de la PCB principal via 6 standoffs M3
- Mounting del top panel de aluminio via 4 heat inserts en la cara superior
- Rebaje lateral izquierdo y derecho (12mm x 100mm de profundidad) para encastre de cheeks guadua
- Pared trasera con slot para pogo connector de expansion modular (Wurth WR-WST 6-pin)
- Paneles laterales internos con cutouts alineados para jacks y USB (guian el taladro en guadua)
- Base con 4 cavidades para patas de silicona antideslizante
- Oculta todo el cableado interno y la electronica

El PETG body NO tiene apertura en la cara superior — el top panel de aluminio la cubre completamente.
El acceso interno es por la cara inferior (base abierta o con tapa removible — ver gap identificado).

---

## 1. Material y proceso de fabricacion

| Parametro | Valor |
|-----------|-------|
| Material | PETG color-stable dark anthracite (Polymaker PolyLite PETG Dark Grey o equivalente) |
| Proveedor filamento Bogota | Filamento.co, Impresionet, Amazon.com.co |
| Diametro filamento | 1.75mm |
| Layer height superficies presentacion | 0.2mm |
| Layer height interior (si slicing diferencial) | 0.3mm aceptable |
| Infill | 40% gyroid (rigidez isotrópica sin peso excesivo) |
| Perimetros | 4 minimo (equivale a 3.2mm pared efectiva sobre WALL_T = 3mm nominal) |
| Nozzle temp | 240°C |
| Bed temp | 80°C |
| Velocidad exterior | 40mm/s |
| Brim | 8mm (evitar warping en pieza grande) |
| Soporte | Solo en cavidades de jacks laterales si el angulo supera 45 grados |
| Orientacion impresion | Base hacia abajo (la cara de los cutouts de jacks queda vertical — soportes minimos) |

Tolerancia efectiva FDM para agujeros: ±0.3mm (usar esta tolerancia en §4 para cutouts criticos).
Para cutouts de jacks en pared lateral (Ø10.5mm): imprimir con 10.8mm en el modelo para compensar
el undersize tipico de FDM en agujeros circulares.

Post-proceso: lijar borde de la boca superior (donde asienta el aluminio) con P400 para planitud.
No lijar las superficies que se veran en el instrumento — la textura de FDM es la textura final.

---

## 2. Parametros requeridos del master

Fuente: `00-master-parameters.md`.

| Parametro | Valor | Descripcion |
|-----------|-------|-------------|
| `BODY_W` | 180mm | Ancho externo PETG body |
| `BODY_D` | 100mm | Profundidad (frente a trasero) |
| `BODY_H` | 38mm | Altura del body (caja rectangular — v0.6; BODY_H_F/H_R eliminados) |
| `WALL_T` | 3mm | Espesor de todas las paredes |
| `PCB_W` | 168mm | Ancho PCB (define posicion standoffs X) |
| `PCB_D` | 88mm | Profundidad PCB (define posicion standoffs Z) |
| `PCB_CLEAR` | 3mm | Clearance PCB a pared interna |
| `CHEEK_REBAJE_D` | 6mm | Profundidad del rebaje lateral para cheeks |
| `STANDOFF_H` | 5mm | Altura standoffs PCB |
| `STANDOFF_OD` | 6mm | Diametro exterior standoffs |
| `INSERT_M3_OD` | 4.2mm | Agujero heat insert M3 |
| `INSERT_M3_H` | 6mm | Altura heat insert M3 |
| `JACK_HOLE` | 10.5mm | Cutout jack 1/4" TRS (usar 10.8mm en modelo por tolerancia FDM) |
| `JACK_Z` | 21mm | Altura Z centro jacks desde base |
| `MINI_JACK_HOLE` | 7mm | Cutout jack 3.5mm (usar 7.3mm por FDM) |
| `USBC_W` | 10.5mm | Cutout USB-C ancho (ambos puertos right son identicos) |
| `USBC_H` | 5mm | Cutout USB-C alto |
| `POGO_X` | 90mm | Centro pogo desde borde izquierdo |
| `POGO_Y_CENTER` | 15mm | Altura Y pogo desde base |
| `POGO_SLOT_W` | 14mm | Ancho slot pogo |
| `POGO_SLOT_H` | 8mm | Alto slot pogo |
| `PATA_D` | 10mm | Diametro cavidad patas silicona |
| `PATA_H` | 2mm | Profundidad cavidad patas (ciego — 1mm de base; 3mm sería pasante con WALL_T=3mm) |
| `L_TRS_INL_Z` | 30mm | TRS IN L: Z desde fondo |
| `L_TRS_INR_Z` | 55mm | TRS IN R: Z desde fondo |
| `L_MINIJACK_Z` | 78mm | AUX IN 3.5mm: Z desde fondo |
| `R_USBC_DATA_Z` | 15mm | USB-C (Power + Audio/MIDI Teensy): Z desde fondo — pigtail micro-USB interno |
| `R_TRS_OUTL_Z` | 38mm | TRS OUT L: Z desde fondo |
| `R_TRS_OUTR_Z` | 60mm | TRS OUT R: Z desde fondo |
| `R_USBC_HOST_Z` | 82mm | USB-C Host MIDI: Z desde fondo — D-10 resuelto |

### Parametros locales

| Parametro local | Valor | Descripcion |
|-----------------|-------|-------------|
| `REBAJE_H` | 100% de la altura del perfil en cada Z | El rebaje para cheeks corre toda la altura |
| `STANDOFF_PCB_DIST_X` | 10mm desde borde PCB | Posicion estandar standoffs en X |
| `STANDOFF_PCB_DIST_Z` | 10mm desde borde PCB | Posicion estandar standoffs en Z |
| `PANEL_INSERT_DIST` | 10mm desde borde exterior | Posicion heat inserts para aluminio |

---

## 3. Geometria — sketches y operaciones (en orden)

### Op. 1: Perfil lateral (Side Profile)
- Plano: plano YZ de Fusion (eje Y = altura, Z = profundidad)
- Sketch "Side_Profile_YZ"
- Geometria: rectangulo (caja plana — v0.6, BODY_H=38mm uniforme)
  - Base inferior: linea Z=0 a Z=BODY_D (Y=0, horizontal)
  - Cara trasera: linea vertical Y=0 a Y=BODY_H=38mm en Z=0
  - Cara frontal: linea vertical Y=0 a Y=BODY_H=38mm en Z=BODY_D
  - Cara superior: linea horizontal de (Z=0, Y=38mm) a (Z=BODY_D, Y=38mm)

### Op. 2: Extrude del perfil (cuerpo solido)
- Seleccionar "Side_Profile_YZ"
- Extrude simetrico en X: BODY_W/2 = 90mm en cada direccion
- Resultado: solido rectangular 180×100×38mm (caja plana, top y bottom paralelos)

### Op. 3: Shell (vaciado interior)
- Shell de 3mm (WALL_T) en todas las caras EXCEPTO:
  - La cara superior (el aluminio la cubre — shell la deja abierta, es la "boca" de la caja)
  - La cara inferior (base — dejar solida con WALL_T = 3mm para rigidez)
- Resultado: caja con techo inclinado abierto, base solida, paredes de 3mm

### Op. 4: Rebaje lateral izquierdo (para cheek guadua)
- Sketch "Cheek_Rebaje_L" en cara lateral izquierda exterior (plano XY, en X = -BODY_W/2)
- Geometria: rectangulo que recorre toda la altura de la cara (ajustandose al perfil inclinado)
  x CHEEK_REBAJE_D = 6mm de profundidad en X
- Cut-extrude: 6mm hacia el interior (en X positivo)
- El rebaje corre los 100mm completos de profundidad (Z=0 a Z=BODY_D)
- Resultado: escalon de 6mm en la cara izquierda donde encaja el cheek

### Op. 5: Rebaje lateral derecho (simetrico)
- Simetrico de Op. 4 respecto al plano XY de Fusion
- Mismo CHEEK_REBAJE_D = 6mm

### Op. 6: Heat inserts para top panel (aluminio)
- 4 pedestales en la cara superior, en las esquinas
- Posiciones (coordenadas X_fusion, Z_fusion):
  - Frente izquierda: X=-80mm, Z=+92mm (Y_panel=92, es decir PANEL_MTG_Y2 desde trasero)
  - Frente derecha: X=+80mm, Z=+92mm
  - Trasera izquierda: X=-80mm, Z=+8mm (Y_panel=8, es decir PANEL_MTG_Y1 desde trasero)
  - Trasera derecha: X=+80mm, Z=+8mm
- Cada pedestal: cilindro STANDOFF_OD=6mm exterior, agujero central INSERT_M3_OD=4.2mm,
  profundidad INSERT_M3_H=6mm
- Los pedestales se extruden hacia arriba desde la cara superior (que es el techo inclinado)

### Op. 7: Heat inserts para PCB (standoffs)
- 6 standoffs en la cara del suelo interno (Y=WALL_T desde la base = 3mm)
- Distribucion: 2 filas de 3 standoffs
  - Fila frontal (Z_fusion = BODY_D - PCB_CLEAR - 10mm = 87mm): X = -74mm, 0mm, +74mm
  - Fila trasera (Z_fusion = PCB_CLEAR + 10mm = 13mm): X = -74mm, 0mm, +74mm
  - Nota: los standoffs extremos en X deben quedar dentro de las paredes PETG.
    Con PCB_W=168mm: PCB se extiende X=-84 a X=+84. Standoffs en X=±74 dan 10mm desde el borde PCB.
- Cada standoff: cilindro STANDOFF_OD=6mm, altura STANDOFF_H=5mm, agujero central INSERT_M3_OD=4.2mm, prof. INSERT_M3_H=6mm
- Extruyen desde el suelo interno hacia arriba (en Y positivo)

### Op. 8: Heat inserts para cheeks
- 2 heat inserts en pared lateral izquierda (en el fondo del rebaje de 6mm)
  - Posicion Z: Z=25mm y Z=75mm desde fondo (centros del area de cheek)
  - Posicion Y: centrados en la altura del perfil en ese Z
  - Agujero INSERT_M3_OD=4.2mm, profundidad INSERT_M3_H=6mm (horizontal en X)
- Espejo en pared lateral derecha

### Op. 9: Cutouts pared lateral izquierda (IN)
- Plano: cara lateral izquierda (despues del rebaje, es decir la superficie interior del canal)
- Cutouts (agujeros pasantes en WALL_T = 3mm de la pared lateral interna que da al interior):
  - TRS IN L: circulo Ø10.8mm (10.5 + tolerancia FDM), centro en (Z=L_TRS_INL_Z=30mm, Y=JACK_Z=21mm)
  - TRS IN R: circulo Ø10.8mm, centro en (Z=L_TRS_INR_Z=55mm, Y=JACK_Z=21mm)
  - 3.5mm AUX IN: circulo Ø7.3mm, centro en (Z=L_MINIJACK_Z=78mm, Y=JACK_Z=21mm)
- Estos cutouts se hacen en la pared INTERNA del PETG (dentro del rebaje de 6mm).
  El cheek guadua que se superpone tendra sus propios cutouts alineados.

### Op. 10: Cutouts pared lateral derecha (OUT + USB) — v0.4

4 puertos, todo USB-C, D-10 resuelto. Sketch "Cut_R_USBC_Data/TRS_OUTL/TRS_OUTR/USBC_Host"
en Plane_Right_Z90 (plano paralelo a xY en Z=+90mm). Cut-extrude -10mm hacia interior.

- USB-C Data: rect 10.8×5.3mm, centro en (Z=R_USBC_DATA_Z=15mm, Y=21mm)
  Pigtail micro-USB (Teensy dev) → USB-C female panel-mount se enruta hasta este cutout.
- TRS OUT L: circulo Ø10.8mm, centro en (Z=R_TRS_OUTL_Z=38mm, Y=21mm)
- TRS OUT R: circulo Ø10.8mm, centro en (Z=R_TRS_OUTR_Z=60mm, Y=21mm)
- USB-C Host: rect 10.8×5.3mm, centro en (Z=R_USBC_HOST_Z=82mm, Y=21mm)
  Clearance frontal: 100-82-5.25=12.75mm — D-10 RESUELTO.

Clearances entre cutouts verificados: 12.5 / 11.5 / 11.5mm — todos OK.

### Op. 11: Cutout panel trasero (pogo)
- Cara trasera (Z=0 en convencion Fusion, que es la pared trasera exterior del PETG body)
- Rectangulo 14.3×8.3mm (POGO_SLOT_W+0.3 x POGO_SLOT_H+0.3)
- Centro en: X=0 (POGO_X=90mm desde borde izquierdo = centro body), Y=POGO_Y_CENTER=15mm desde base
- Cut-extrude pasante (atraviesa WALL_T=3mm de pared trasera)

### Op. 12: Cavidades patas silicona (base)
- Cara inferior exterior (base plana)
- 4 cilindros ciegos: Ø=PATA_D=10mm, profundidad PATA_H=3mm
- Posiciones (X_fusion, Z_fusion): (+/-)80mm x (+/-)45mm (aproximadamente esquinas, 10mm del borde)
- Las patas de silicona se presionan dentro — no necesitan adhesivo si el fit es justo (Ø9.9mm en modelo para leve press-fit)

---

## 4. Cutouts y holes (tabla completa)

| Componente | Cara | Forma | Dim. nominal | Dim. modelo FDM | Tolerancia | Centro X_fusion | Centro Z_fusion | Centro Y |
|------------|------|-------|-------------|-----------------|------------|-----------------|-----------------|----------|
| TRS IN L | Lateral izq. interna | Circular | Ø10.5mm | Ø10.8mm | ±0.3mm | X=exterior izq | Z=30mm | Y=21mm |
| TRS IN R | Lateral izq. interna | Circular | Ø10.5mm | Ø10.8mm | ±0.3mm | X=exterior izq | Z=55mm | Y=21mm |
| 3.5mm AUX IN | Lateral izq. interna | Circular | Ø7.0mm | Ø7.3mm | ±0.3mm | X=exterior izq | Z=78mm | Y=21mm |
| USB-C Data (Power+Audio) | Lateral der. interna | Rectangular | 10.5×5.0mm | 10.8×5.3mm | ±0.3mm | X=exterior der | Z=15mm | Y=21mm |
| TRS OUT L | Lateral der. interna | Circular | Ø10.5mm | Ø10.8mm | ±0.3mm | X=exterior der | Z=38mm | Y=21mm |
| TRS OUT R | Lateral der. interna | Circular | Ø10.5mm | Ø10.8mm | ±0.3mm | X=exterior der | Z=60mm | Y=21mm |
| USB-C Host MIDI | Lateral der. interna | Rectangular | 10.5×5.0mm | 10.8×5.3mm | ±0.3mm | X=exterior der | Z=82mm | Y=21mm |
| Pogo 6-pin | Trasera | Rectangular | 14.0×8.0mm | 14.3×8.3mm | ±0.3mm | X=0 | Z=0 (cara trasera) | Y=15mm |
| Pata silicona x4 | Base | Circular ciego | Ø10mm | Ø9.9mm (press-fit) | ±0.2mm | ±80mm | ±45mm | Y=base |

---

## 5. Interfaces con otras piezas

| Pieza | Tipo de interface | Tolerancia de junta |
|-------|------------------|---------------------|
| Top panel aluminio (02) | 4 heat inserts M3 en pedestales superiores; tornillos M3×6 countersunk desde arriba | Clearance 0.1mm entre aluminio y techo PETG (el aluminio "flota" sobre los inserts) |
| Cheek izquierdo guadua (03) | Rebaje de 6mm en X; Araldite + 2 tornillos M3×16 en heat inserts | El cheek debe tener 0.2mm de clearance en el rebaje para poder ensamblar sin fuerza excesiva |
| Cheek derecho guadua (04) | Idem simetrico | Idem |
| PCB principal | 6 standoffs M3 heat inserts + tornillos M3×8 desde la PCB hacia abajo | Standoff height STANDOFF_H=5mm define el gap entre PCB y fondo interno |
| Display bracket (05) | El bracket cuelga desde la cara inferior del top panel (v0.5: modulo debajo del panel). Sin contacto directo con el PETG body. | Indirecto — via top panel |
| Pogo connector | Slot rectangular en pared trasera; el conector es panel-mount | Clearance 0.3mm alrededor del conector |

---

## 6. Validacion post-fabricacion

| Check | Metodo | Criterio pass |
|-------|--------|---------------|
| Planitud cara superior (boca) | Regla de metal apoyada sobre el borde | Luz visible < 0.5mm |
| Ancho externo BODY_W | Calibre digital | 180mm ±0.5mm |
| Profundidad BODY_D | Calibre digital | 100mm ±0.5mm |
| Altura BODY_H | Calibre digital en cara frontal y trasera | 38mm ±0.5mm (mismo valor — caja rectangular) |
| Profundidad rebaje cheek | Calibre de profundidad | 6mm ±0.3mm (en ambos lados) |
| Cutouts jacks laterales | Insertar jack fisico Neutrik NYS229 | El jack entra sin fuerza excesiva, rosca accessible |
| Cutouts USB-C laterales | Insertar conector USB-C macho en cada uno | El conector pasa limpio, sin interferencia |
| Cutout pogo trasero | Verificar con caliper o con el conector fisico | No hay interferencia con las paredes |
| Heat inserts PCB | Atornillar M3×8 con standoffs metalicos | Rosca agarra sin wobble |
| Heat inserts aluminio | Atornillar M3×6 sin top panel | Rosca agarra sin wobble |
| Heat inserts cheeks | Atornillar M3×16 desde el cheek | Rosca agarra sin wobble |

---

## 7. Export targets

| Archivo | Formato | Path | Uso | Settings |
|---------|---------|------|-----|---------|
| PETG body Fusion archive | .f3d | `apps/design/3d-models/01-petg-body.f3d` | Edicion | Save from Fusion |
| PETG body STEP | .step | `apps/design/3d-models/01-petg-body.step` | Verificacion en otros CAD | AP203 o AP214 |
| PETG body STL | .stl | `apps/design/3d-models/01-petg-body.stl` | Impresion 3D directa | Mesh refinement: 0.01mm tolerance, 10 deg angle |

Antes de exportar STL: verificar orientacion (base abajo = Z- en STL) y que no haya superficies no-manifold.

---

## 8. Workflow Fusion 360 (paso a paso ejecutable)

```
1.  Nuevo documento Fusion 360. Guardar como "GrooveForge_Brain_Enclosure".

2.  Ejecutar script Python de 00-master-parameters.md para cargar todos los user parameters.
    Verificar en Modify > Change Parameters que los valores son correctos.

3.  Crear componente "01_PETG_Body" en el browser (root > New Component).
    Activar el componente (doble click en el nombre).

4.  Sketch "Side_Profile_YZ" en plano YZ (plano lateral de Fusion, eje Y=arriba, Z=profundidad):
    - Linea de (0,0) a (0, BODY_H_R): cara trasera
    - Linea de (0,0) a (BODY_D, 0): base inferior
    - Linea de (BODY_D, 0) a (BODY_D, BODY_H_F): cara frontal
    - Linea de (0, BODY_H_R) a (BODY_D, BODY_H_F): techo inclinado
    - Cerrar el perfil. Finish Sketch.

5.  Extrude "Side_Profile_YZ": distancia simetrica BODY_W/2 = 90mm en ambos sentidos del eje X.
    Operacion: New Body. Resultado: solido de 180×100mm con techo inclinado.

6.  Shell: seleccionar la cara superior (techo inclinado) para remover (opening).
    Thickness = WALL_T = 3mm. Aplicar a todas las demas caras.
    ATENCION: la cara inferior (base) debe quedar con WALL_T de espesor — Fusion puede
    querer abrirla tambien. Si eso ocurre, hacer Shell sin la base y agregar la base
    como feature separada.

7.  Sketch "Cheek_Rebaje_L" en cara lateral izquierda exterior (plano que pasa por X = -BODY_W/2):
    - Rectangulo de (Z=0, Y=0) a (Z=BODY_D, Y=BODY_H max en ese punto)
    - En realidad es toda la cara lateral izquierda.
    - Cut extrude: profundidad CHEEK_REBAJE_D = 6mm hacia el interior (en X+).
    NOTA: el resultado debe ser un escalon que reduce la anchura visible del PETG en 6mm por lado.

8.  Mirror feature "Cheek_Rebaje_L" respecto al plano XY (plano de simetria del body):
    Esto crea el rebaje derecho identico.

9.  Pedestales heat inserts top panel (4 unidades):
    Sketch "Panel_Inserts" en el techo inclinado (seleccionar la cara inclinada como plano).
    Crear 4 puntos en las esquinas segun PANEL_MTG_X1/X2 y PANEL_MTG_Y1/Y2.
    Para cada punto: extrude cilindro Ø STANDOFF_OD = 6mm, hacia ABAJO (interior del body),
    altura INSERT_M3_H = 6mm. Luego taladrar agujero central Ø INSERT_M3_OD = 4.2mm, pasante.
    ATENCION: en la cara inclinada, los pedestales deben ser perpendiculares al plano inclinado,
    no verticales. En Fusion, seleccionar la cara como plano base del sketch garantiza esto.

10. Standoffs PCB (6 unidades):
    Sketch "PCB_Standoffs" en el suelo interno (cara interior de la base, Y = WALL_T desde exterior).
    Posiciones X: -74mm, 0mm, +74mm. Posiciones Z: 13mm y 87mm.
    Para cada punto: extrude cilindro Ø STANDOFF_OD = 6mm hacia arriba (Y+), altura STANDOFF_H = 5mm.
    Taladrar agujero central Ø INSERT_M3_OD = 4.2mm, profundidad INSERT_M3_H = 6mm.

11. Heat inserts cheeks (2 por lado = 4 total):
    Sketch "Cheek_Inserts_L" en la cara interior vertical izquierda (el fondo del rebaje de 6mm).
    2 puntos en Z=25mm e Z=75mm, centrados en la altura media del perfil en cada Z.
    Extrude cilindro Ø STANDOFF_OD = 6mm en X (horizontal, hacia el interior del body),
    profundidad = WALL_T suficiente para el insert. Taladrar Ø INSERT_M3_OD = 4.2mm, prof. 6mm.
    Mirror para el lado derecho.

12. Cutouts laterales izquierdos (IN):
    Sketch "Cutouts_Left" en la cara interna izquierda (superficie vertical dentro del rebaje).
    - Circulo Ø10.8mm en (Z=30mm, Y=21mm) para TRS IN L
    - Circulo Ø10.8mm en (Z=55mm, Y=21mm) para TRS IN R
    - Circulo Ø7.3mm en (Z=78mm, Y=21mm) para 3.5mm AUX
    Cut extrude pasante en X (atraviesa la pared de WALL_T=3mm + el rebaje de 6mm).

13. Cutouts laterales derechos (OUT + USB):
    Sketch "Cutouts_Right" en la cara interna derecha.
    - Rectangulo 10.8×5.3mm centrado en (Z=15mm, Y=21mm) para USB-C Power
    - Circulo Ø10.8mm en (Z=38mm, Y=21mm) para TRS OUT L
    - Circulo Ø10.8mm en (Z=60mm, Y=21mm) para TRS OUT R
    - Rectangulo 10.8×5.3mm centrado en (Z=80mm, Y=21mm) para USB-C Teensy
    - Rectangulo 15.3×8.3mm centrado en (Z=92mm, Y=21mm) para USB-A Host
    Cut extrude pasante en X.

14. Cutout pogo trasero:
    Sketch "Cutout_Pogo" en la cara exterior trasera (Z=0 exterior).
    Rectangulo 14.3×8.3mm centrado en (X=0, Y=15mm).
    Cut extrude pasante (WALL_T = 3mm).

15. Cavidades patas silicona:
    Sketch "Rubber_Feet" en la cara exterior de la base.
    4 circulos Ø9.9mm en posiciones (+/-)80mm x (+/-)45mm.
    Cut extrude ciego: profundidad PATA_H = 3mm.

16. Fillets opcionales (esteticos):
    Fillet de 2mm en aristas exteriores verticales del PETG (las 4 aristas verticales del body).
    No aplicar fillets en la boca superior (interfiere con el asiento del aluminio) ni en el rebaje de cheeks.

17. Guardar y exportar (ver §7 Export targets).
```

### Hints para evitar errores comunes en Fusion

- En el paso 6 (Shell): si Fusion falla, probar con "Shell" type "Inside". Si sigue fallando, usar el metodo alternativo: extrude del perfil interior (BODY_W - 2×WALL_T) y restar del solido exterior.
- En el paso 9 (pedestales inclinados): si el sketch en la cara inclinada da problemas, usar un offset plane paralelo a la cara inclinada y extrude hacia la cara.
- En el paso 12 y 13 (cutouts laterales): asegurarse de que el cut-extrude corta tanto la pared PETG (3mm) como el espacio del rebaje (6mm) — hacer el cut con profundidad de al menos 10mm para garantizar que sea pasante.
