# Part 03 — Cheek Izquierdo Guadua (Audio IN)

> **Autor:** Industrial Design Agent
> **Version:** 0.1
> **Status:** PROPUESTO
> **Material:** Guadua angustifolia laminada, sellado epoxico
> **Fabricacion:** Corte manual/CNC + acabado artesanal + sellado
> **Derivado de:** `apps/design/enclosure/01-enclosure-design-spec.md` v0.3, §2, §4, §9, §10
> **Archivo CAD target:**
> - `apps/design/3d-models/03-cheek-left.f3d`
> - `apps/design/3d-models/03-cheek-left.step`

---

## 0. Funcion de la pieza

El cheek izquierdo es el panel lateral izquierdo del instrumento. Sus funciones:

- Cubre y sella la cara lateral izquierda del PETG body
- Aloja los conectores de entrada de audio: 2× jack 1/4" TRS (IN L y IN R) + 1× jack 3.5mm (AUX/Headphone IN)
- Define la identidad visual lateral del instrumento: guadua angustifolia colombiana
- Se encastra en el rebaje de 6mm del PETG body (mecanismo Moog Subsequent)
- El peso de la guadua (menor que madera densa) contribuye al balance peso/volumen del instrumento

El cheek izquierdo = INPUTS. El cheek derecho = OUTPUTS + POWER. Esta convencion es estetica y funcional
y debe mantenerse en el diseno fisico y en los labels si se graban.

---

## 1. Material y proceso de fabricacion

| Parametro | Valor |
|-----------|-------|
| Material | Guadua angustifolia laminada (tablex o lamina) |
| Espesor nominal | 12mm (CHEEK_T) |
| Sourcing Bogota | Proveedores de guadua laminada: La 14 (seccion materiales), ferreterias de Paloquemao, proveedores de bambu importado MOSO como alternativa |
| Alternativa si guadua no disponible | MOSO bamboo laminado (importado, acabado mas uniforme, mas facil de conseguir, menos identitario) — ver D-04 del master spec |
| Corte exterior | Sierra de cinta o CNC segun disponibilidad; el perfil es una extrusion del contorno del PETG — requiere corte en la cara exterior con el perfil inclinado |
| Cutouts de jacks | Post-ensamblaje con taladro, usando los cutouts del PETG como guia (ver §10 del master spec) |
| Acabado | Lijar con P120 → P240 → P400. Luego sellado epoxico de dos componentes (Resina + endurecedor) o shellac. OBLIGATORIO en Bogota por humedad 70-80% HR. |
| Color/tono | Natural guadua (amarillo pajizo con vetas marron). Sin teñir. |
| Junta con PETG | Araldite estructural en el rebaje + 2 tornillos M3×16 ocultos |

La guadua laminada puede tener variaciones de veta — seleccionar laminas con veta horizontal
(paralela a la profundidad del instrumento) para uniformidad visual. Las variaciones naturales
son parte del caracter del instrumento, no defectos.

### Riesgo de humedad
La guadua laminada sellada con epoxico es estable en condiciones de Bogota. Sin sellar, puede
expandir/contraer hasta 0.5mm en CHEEK_T con variaciones de HR. El sellado epoxico (todas las
caras, especialmente los cortes) es NO NEGOCIABLE antes del ensamblaje.

---

## 2. Parametros requeridos del master

Fuente: `00-master-parameters.md`.

| Parametro | Valor | Descripcion |
|-----------|-------|-------------|
| `CHEEK_T` | 12mm | Espesor del cheek |
| `BODY_D` | 100mm | Profundidad (largo del cheek en Z) |
| `BODY_H` | 38mm | Altura del cheek (caja rectangular v0.6 — BODY_H_F/H_R eliminados) |
| `CHEEK_REBAJE_D` | 6mm | Profundidad del encastre en el PETG |
| `INSERT_M3_OD` | 4.2mm | Agujero para tornillo M3 en el cheek |
| `JACK_HOLE` | 10.5mm | Diametro cutout jack 1/4" TRS |
| `JACK_HOLE_GUAD_TOL` | 0.5mm | Tolerancia cutout guadua = usar Ø11.0mm en el taladro |
| `JACK_Z` | 21mm | Altura Z del centro de los jacks desde la base |
| `MINI_JACK_HOLE` | 7mm | Diametro cutout jack 3.5mm |
| `L_TRS_INL_Z` | 30mm | TRS IN L: Z desde panel trasero (fondo del instrumento) |
| `L_TRS_INR_Z` | 55mm | TRS IN R: Z desde panel trasero |
| `L_MINIJACK_Z` | 78mm | 3.5mm AUX IN: Z desde panel trasero |

---

## 3. Geometria — sketches y operaciones (en orden)

El cheek izquierdo es un solido de seccion trapezoidal (altura variable frente/trasero)
extruido a lo largo del eje Z (profundidad del instrumento).

### Op. 1: Perfil del cheek (seccion en plano YZ)
- Plano: YZ de Fusion
- Sketch "Cheek_L_Profile"
- Geometria: rectangulo (caja plana — v0.6, igual que el PETG body)
  - Base: linea de (Z=0, Y=0) a (Z=BODY_D=100mm, Y=0)
  - Cara trasera: linea vertical de (Z=0, Y=0) a (Z=0, Y=BODY_H=38mm)
  - Cara frontal: linea vertical de (Z=BODY_D=100mm, Y=0) a (Z=BODY_D=100mm, Y=BODY_H=38mm)
  - Cara superior: linea horizontal de (Z=0, Y=38mm) a (Z=BODY_D=100mm, Y=38mm)
- El perfil forma un rectangulo de 38mm de alto uniforme × 100mm de profundidad

### Op. 2: Extrude del perfil (cuerpo solido del cheek)
- Extrude en X: CHEEK_T = 12mm
- Resultado: bloque de 12mm × 100mm × 38mm
- Orientacion: la cara interior (que da al rebaje del PETG) queda en X=0;
  la cara exterior (visible) queda en X=-12mm

### Op. 3: Encastre en rebaje PETG (paso que reduce la seccion interior)
- El cheek se inserta 6mm dentro del rebaje del PETG.
- En el modelo CAD del cheek, esto implica que la seccion de la lengüeta de encastre
  tiene el espesor total: CHEEK_T = 12mm.
- En el PETG, el rebaje tiene profundidad CHEEK_REBAJE_D = 6mm.
- El cheek NO necesita un feature especial para el encastre en su modelo — simplemente se
  inserta en el rebaje. El ajuste se logra con las tolerancias del PETG.
- EXCEPCION: si se quiere un cheek con "labio" visible (donde la guadua sobresale del plano
  del PETG en 6mm), el cheek tiene espesor total de 12mm y el rebaje de 6mm en el PETG
  hace que 6mm queden visibles y 6mm queden enterrados.
  Verificar con el master spec: el total del instrumento es 204mm = 180mm PETG + 2×12mm cheeks.
  Esto implica que los cheeks NO estan parcialmente enterrados — estan completamente afuera
  del body PETG, adyacentes. El rebaje de 6mm es solo para posicionamiento y adhesion.
  Por lo tanto: el cheek es un bloque de CHEEK_T=12mm completamente exterior, y el rebaje
  del PETG provee un canal de 6mm de profundidad donde el borde interior del cheek se asienta.

### Op. 4: Agujeros de fijacion (tornillos M3×16 ocultos)
- 2 agujeros pasantes de Ø3.5mm (clearance para M3) en la cara interior del cheek
- Posiciones Z: Z=25mm y Z=75mm desde la cara trasera
- Posicion Y: centrado en la altura del perfil en ese Z
  - Altura uniforme BODY_H=38mm en todo Z → centro en Y=BODY_H/2=19mm en ambas posiciones
- Agujeros pasantes en X (de cara interior a cara exterior): Ø3.5mm (clearance hole M3)
- Los tornillos van desde el exterior de los cheeks hacia los heat inserts del PETG (ocultos)
  ATENCION: si los tornillos van desde el EXTERIOR del cheek hacia adentro, son visibles.
  Alternativa preferida segun §10 del master spec: "tornillos M3×16 ocultos" — esto implica
  que los tornillos van desde el INTERIOR del PETG hacia el cheek. En ese caso:
  - El cheek tiene agujeros ciegos de M3 (roscados o con heat insert)
  - El PETG tiene los heat inserts que ya se modelaron en Op. 8 del PETG body
  - Los tornillos van desde adentro del PETG hacia el cheek — completamente ocultos
  Para esto, el cheek necesita: agujero ciego de Ø4.2mm (para heat insert M3) o Ø2.5mm (rosca directa en guadua con M3 machuelado, no recomendado — la guadua puede astillarse).
  Recomendacion: usar heat insert M3 en el cheek de guadua (con herramienta de calor — funciona en maderas densas, la guadua laminada lo acepta).

### Op. 5: Cutouts de jacks (POST-ENSAMBLAJE — representacion en modelo)
- Los cutouts se hacen post-ensamblaje siguiendo los cutouts del PETG como guia.
- En el modelo CAD incluirlos de todas formas para verificar posicion y clearances:
  - TRS IN L: Circulo Ø11.0mm (JACK_HOLE + guadua tolerancia 0.5mm) en X-face (cara exterior)
    Centro: (Z=L_TRS_INL_Z=30mm, Y=JACK_Z=21mm)
  - TRS IN R: Circulo Ø11.0mm en (Z=55mm, Y=21mm)
  - 3.5mm AUX: Circulo Ø7.5mm (MINI_JACK_HOLE + 0.5mm) en (Z=78mm, Y=21mm)
- En la fabricacion: taladrar pasante de exterior hacia interior, guiando con los cutouts del PETG.
  Usar broca de punta centrada primero, luego broca Forstner Ø11mm o sierra copa.

---

## 4. Cutouts y holes (tabla completa)

| Componente | Cara | Forma | Dim. nominal | Dim. en guadua | Tolerancia | Z desde fondo | Y centro |
|------------|------|-------|-------------|----------------|------------|---------------|----------|
| TRS IN L | Exterior (X exterior) | Circular | Ø10.5mm | Ø11.0mm | ±0.5mm | 30mm | 21mm |
| TRS IN R | Exterior (X exterior) | Circular | Ø10.5mm | Ø11.0mm | ±0.5mm | 55mm | 21mm |
| 3.5mm AUX/Headphone IN | Exterior (X exterior) | Circular | Ø7.0mm | Ø7.5mm | ±0.5mm | 78mm | 21mm |
| Agujero fijacion 1 | Interior (cara que va al PETG) | Circular ciego | Ø4.2mm heat insert | Ø4.2mm | ±0.2mm | 25mm | 19mm (BODY_H/2) |
| Agujero fijacion 2 | Interior | Circular ciego | Ø4.2mm heat insert | Ø4.2mm | ±0.2mm | 75mm | 19mm (BODY_H/2) |

---

## 5. Interfaces con otras piezas

| Pieza | Tipo de interface | Tolerancia |
|-------|------------------|------------|
| PETG body (01) | La cara interior del cheek se asienta en el rebaje de 6mm del PETG. Araldite estructural en toda la superficie del rebaje + 2 tornillos M3×16 en los heat inserts. | Clearance entre cheek e interior del rebaje: 0.2mm en X (el cheek debe deslizar sin fuerza antes de aplicar adhesivo) |
| Top panel aluminio (02) | El borde superior del cheek queda adyacente al borde lateral del top panel. Gap visual intencional de 0.5-1mm entre aluminio y guadua. No hay fijacion directa entre cheek y panel. | El gap depende de las tolerancias de fabricacion del PETG y el cheek — no es controlado directamente. |
| Jacks audio (componentes) | Los jacks Neutrik NYS229 (1/4" TRS) pasan por el cutout de la guadua y se roscan en el cheek. La rosca es en la tuerca del jack sobre la cara exterior de la guadua. | La guadua debe tener espesor suficiente (12mm) para que la tuerca del jack y la arandela puedan apretar. Con CHEEK_T=12mm y grosor de arandela ~2mm, la distancia disponible es 10mm — suficiente para la rosca de los NYS229. |

---

## 6. Validacion post-fabricacion

| Check | Metodo | Criterio pass |
|-------|--------|---------------|
| Espesor CHEEK_T | Calibre digital en 5 puntos | 12mm ±0.3mm |
| Profundidad | Calibre digital en ambos extremos | 100mm ±0.5mm |
| Altura | Calibre digital en cara frontal y trasera | 38mm ±0.5mm (mismo valor — caja rectangular v0.6) |
| Fit rectangular | Apoyar sobre el PETG body sin adhesivo | El cheek debe asentar flush contra el rebaje sin gaps |
| Cutout TRS jacks | Insertar jack Neutrik NYS229 fisico | Jack entra sin forzar, la rosca queda al exterior |
| Cutout 3.5mm | Insertar jack 3.5mm fisico | Jack entra sin forzar |
| Sellado epoxico | Inspeccion visual + toque con dedo | Sin zonas sin sellado; superficie no absorbente al tacto |
| Acabado tactil | Pasar la palma de la mano por la cara exterior | Sin asperezas perceptibles, textura satinada uniforme |
| Humedad 48h | Dejar en ambiente Bogota 48h post-sellado | Sin alabeo o cambio de dimension >0.3mm |

---

## 7. Export targets

| Archivo | Formato | Path | Uso |
|---------|---------|------|-----|
| Cheek left Fusion archive | .f3d | `apps/design/3d-models/03-cheek-left.f3d` | Edicion CAD |
| Cheek left STEP | .step | `apps/design/3d-models/03-cheek-left.step` | Referencia para fabricacion |

No se exporta STL para guadua — el cheek se fabrica con herramientas de carpinteria, no impresion 3D.
El STEP se usa como referencia de dimensiones para el carpintero o el operario de sierra de cinta.

---

## 8. Workflow Fusion 360 (paso a paso ejecutable)

```
1.  Activar componente "03_Cheek_Left" en el browser.

2.  Sketch "Cheek_L_Profile" en plano YZ de Fusion (rectangulo — v0.6):
    - (0,0) a (BODY_D=100,0): base
    - (0,0) a (0, BODY_H=38): cara trasera vertical
    - (BODY_D=100, 0) a (BODY_D=100, BODY_H=38): cara frontal vertical
    - (0, 38) a (100, 38): techo horizontal (plano)
    Finish Sketch.

3.  Extrude "Cheek_L_Profile": CHEEK_T = 12mm en X- (hacia la izquierda).
    ATENCION: la convencion es que el borde interior del cheek (que da al PETG) esta en X=0,
    y el cheek se extiende hacia X=-12mm (exterior). Verificar con el ensamblaje.

4.  Agujeros de fijacion (heat inserts):
    Sketch "Cheek_L_Fixings" en la cara interior (X=0):
    - Punto en (Z=25mm, Y=16.5mm)
    - Punto en (Z=75mm, Y=19.5mm)
    Hole feature: Ø4.2mm, profundidad 7mm (1mm mas que INSERT_M3_H=6mm para tolerance).
    Tipo: Simple (sin rosca en el CAD — la rosca es el heat insert fisico).

5.  Cutouts de jacks (para verificacion en assembly):
    Sketch "Cheek_L_Jacks" en la cara exterior (X=-12mm):
    - Circulo Ø11.0mm centrado en (Z=30mm, Y=21mm) para TRS IN L
    - Circulo Ø11.0mm centrado en (Z=55mm, Y=21mm) para TRS IN R
    - Circulo Ø7.5mm centrado en (Z=78mm, Y=21mm) para 3.5mm AUX
    Cut extrude pasante (12mm en X+, de cara exterior a cara interior).
    NOTA: en fabricacion real, estos cutouts se hacen post-ensamblaje. En el modelo son
    para verificar clearances unicamente.

6.  Posicionar en el ensamblaje:
    En el archivo de ensamblaje (99-assembly.f3d), usar Joint para alinear la cara interior
    del cheek (X=0) con la superficie interior del rebaje izquierdo del PETG body.
    El rebaje del PETG tiene 6mm de profundidad en X; el cheek se inserta hasta que
    su cara interior toca el fondo del rebaje.

7.  Exportar STEP: `apps/design/3d-models/03-cheek-left.step`.
```

### Nota sobre etiquetado (D-07 del master spec)

El master spec deja abierta la decision de etiquetado en guadua (laser engraving vs inciso+paint vs sticker).

Recomendacion para v0.2 prototipo: laser engraving con "IN" en la cara exterior del cheek izquierdo,
cerca del borde superior. El laser de CO2 corta bien la guadua laminada. La linea grabada queda de color
oscuro natural sobre el fondo pajizo — contraste suficiente sin pintura.

Para produccion v1.0: evaluar si el label agrega valor o si la posicion de los jacks es autoevidente
(IN = izquierda, OUT = derecha es una convencion de audio universalmente conocida).
