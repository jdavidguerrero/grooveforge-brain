# Fusion 360 Python API — Referencia Técnica para Scripting Paramétrico

**Propósito:** Documento de referencia preciso para escribir scripts de modelado paramétrico en
Fusion 360, construido desde la documentación oficial y fuentes verificadas. Creado para resolver
bugs recurrentes en scripts que fallan por malentendidos del API object model.

**Fuentes primarias consultadas:**
- Autodesk Fusion 360 API Help — BasicConcepts, ComponentsProxies, ConstructionPlaneSample,
  ExtrudeFeatureSample, ParticipantBodiesSample, SketchCircle, Sketch object reference
- Autodesk Community forums — discusiones verificadas sobre coordenadas en sketches
- Fusion 360 API Python samples oficiales (cloudhelp.autodesk.com)

---

## 1. Object Model — Jerarquía completa

```
Application
└── Document  (FusionDesignDocumentType)
    └── Design  (adsk.fusion.Design)
        └── RootComponent  (design.rootComponent)
            ├── sketches          ← SketchList
            ├── features          ← Features (ExtrudeFeatures, CombineFeatures, etc.)
            ├── constructionPlanes ← ConstructionPlaneList
            ├── constructionAxes
            ├── constructionPoints
            ├── bRepBodies        ← BRepBodyList
            ├── xYConstructionPlane  ← plano base XY (built-in, read-only)
            ├── xZConstructionPlane  ← plano base XZ (built-in, read-only)
            ├── yZConstructionPlane  ← plano base YZ (built-in, read-only)
            └── occurrences       ← OccurrenceList
                └── Occurrence  (instancia de un Component)
                    └── Component  (geometría real)
                        ├── sketches
                        ├── features
                        ├── constructionPlanes
                        ├── xYConstructionPlane  ← built-in del componente
                        ├── xZConstructionPlane  ← built-in del componente
                        └── yZConstructionPlane  ← built-in del componente
```

### Patrón de acceso estándar

```python
import adsk.core
import adsk.fusion
import traceback

def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface

        # Obtener design y root component
        design = adsk.fusion.Design.cast(app.activeProduct)
        rootComp = design.rootComponent

        # ... trabajo aquí ...

    except:
        if ui:
            ui.messageBox('Failed:\n{}'.format(traceback.format_exc()))
```

### Diferencia fundamental: RootComponent vs Component hijo

| Aspecto | RootComponent | Component hijo |
|---|---|---|
| Acceso | `design.rootComponent` | `occ.component` |
| Visibilidad en browser | Directa (nivel top) | Aparece como Occurrence (ej: `Component1:1`) |
| Tiene geometría propia | Sí | Sí |
| Puede reposicionarse | No — es el origen fijo | Sí, via la Occurrence |
| Planos built-in (`xYConstructionPlane`, etc.) | Sí | Sí — EXISTEN en componentes hijos |
| Coordenadas de geometría creada | Espacio mundo (root) | Espacio local del componente |

**Regla crítica:** Cuando accedés a `sketches`, `features` o `constructionPlanes` desde un
componente hijo (`occ.component`), la geometría creada existe en el espacio local de ese
componente, no en world space. Si el componente hijo está en la posición de origen (transform
identidad), el espacio local coincide con world space.

---

## 2. Construction Planes

### 2.1 Planos built-in de un componente

Todos los componentes — tanto `rootComponent` como componentes hijos — tienen tres planos built-in:

```python
# En root component
rootComp.xYConstructionPlane   # plano Y=0 (horizontal en Fusion: Z es up por defecto)
rootComp.xZConstructionPlane   # plano Z=0 (vertical, paralelo al suelo si Z=up)
rootComp.yZConstructionPlane   # plano X=0

# En componente hijo — TAMBIÉN EXISTEN
occ = rootComp.occurrences.addNewComponent(adsk.core.Matrix3D.create())
comp = occ.component
comp.xYConstructionPlane   # válido — no lanza error
comp.xZConstructionPlane   # válido
comp.yZConstructionPlane   # válido
```

**Confirmado:** `comp.xZConstructionPlane` existe y funciona en componentes hijos. El error
`AttributeError: 'NoneType' object has no attribute 'xZConstructionPlane'` no se debe a que el
plano no exista, sino a que `comp` es `None` — verificar que `occ.component` no sea `None`.

### 2.2 Crear un plano offset (construcción paramétrica)

```python
planes = rootComp.constructionPlanes   # o comp.constructionPlanes para componente hijo
planeInput = planes.createInput()

# Offset desde xZConstructionPlane en la dirección normal al plano (Y global si plano = XZ)
offsetValue = adsk.core.ValueInput.createByReal(3.0)   # 3 cm (unidades del documento)
planeInput.setByOffset(rootComp.xZConstructionPlane, offsetValue)
offsetPlane = planes.add(planeInput)
```

**Importante sobre `setByOffset`:**
- El primer argumento (`planarEntity`) acepta: `ConstructionPlane`, cara plana (`BRepFace`), o
  perfil de sketch.
- El offset es escalar (positivo = dirección positiva de la normal del plano de referencia).
- Para `xZConstructionPlane` (normal = +Y global), offset positivo crea plano en Y=+3cm.
- El plano resultante queda en el componente cuyo `constructionPlanes` se usó para crearlo.

### 2.3 Plano offset en componente hijo

```python
occ = rootComp.occurrences.addNewComponent(adsk.core.Matrix3D.create())
comp = occ.component

# El plano queda en el comp, no en root
planes = comp.constructionPlanes
planeInput = planes.createInput()
offsetValue = adsk.core.ValueInput.createByReal(5.0)
planeInput.setByOffset(comp.xZConstructionPlane, offsetValue)
offsetPlaneInChild = planes.add(planeInput)
```

**Advertencia sobre `setByOffset` + `comp.xZConstructionPlane`:** Cuando el componente hijo tiene
transform identidad (posición en el origen), los planos built-in del componente hijo son
equivalentes a los del root. Si el componente hijo está desplazado via su Occurrence, los planos
built-in del hijo se comportan en su espacio local, y el offset plane resultante también.

### 2.4 Verificar estado del plano creado

```python
health = offsetPlane.healthState
if health in (
    adsk.fusion.FeatureHealthStates.ErrorFeatureHealthState,
    adsk.fusion.FeatureHealthStates.WarningFeatureHealthState
):
    message = offsetPlane.errorOrWarningMessage
    # Investigar antes de continuar — un plano en error state no puede hospedar sketches
```

### 2.5 Otros métodos de creación de planos

```python
# Por ángulo relativo a una línea
angle = adsk.core.ValueInput.createByString('30.0 deg')
planeInput.setByAngle(sketchLine, angle, referenceProfile)

# Bisectriz entre dos planos
planeInput.setByTwoPlanes(plane1, plane2)

# Por tres puntos
planeInput.setByThreePoints(sketchPoint1, sketchPoint2, sketchPoint3)

# Tangente a una cara cilíndrica
planeInput.setByTangent(cylinderFace, angle, referenceConstructionPlane)

# Tangente a cara en un punto específico
planeInput.setByTangentAtPoint(face, sketchPoint)

# A distancia sobre un path
distance = adsk.core.ValueInput.createByReal(1.0)
planeInput.setByDistanceOnPath(sketchLine, distance)
```

---

## 3. Sketches y Coordenadas

### 3.1 Crear un sketch sobre un plano

```python
sketches = rootComp.sketches
sketch = sketches.add(rootComp.xZConstructionPlane)   # sketch sobre XZ
```

### 3.2 Sistema de coordenadas del sketch — CRÍTICO

**Cada sketch tiene su propio sistema de coordenadas local 2D.** Las coordenadas que se pasan a
métodos como `addByCenterRadius`, `addByTwoPoints`, etc., están en **sketch space** (espacio local
del sketch), no en world space.

**Mapeo de ejes para los planos estándar (confirmado con el API sample oficial):**

Evidencia directa del `ConstructionPlaneSample` oficial, donde un sketch sobre `xZConstructionPlane`
usa estos puntos:
```python
startPoint = adsk.core.Point3D.create(5, 5, 0)   # sketch local (sketchX=5, sketchY=5)
endPoint   = adsk.core.Point3D.create(5, 10, 0)
positionOne = adsk.core.Point3D.create(0, 5.0, 0)
```

Esto confirma que en sketch space **siempre se usan los componentes X e Y del Point3D**, y Z debe
ser 0. Las coordenadas del sketch son 2D (X, Y) con Z=0 ignorado o proyectado al plano.

| Plano del sketch | Sketch local X | Sketch local Y | Eje perpendicular (normal) |
|---|---|---|---|
| `xYConstructionPlane` | → global X | → global Y | global Z |
| `xZConstructionPlane` | → global X | → global Z | global Y |
| `yZConstructionPlane` | → global Y | → global Z | global X |
| Offset de `xZConstructionPlane` | → global X | → global Z | global Y (offset) |

**La regla uniforme del API:** En sketch space, siempre usás `Point3D.create(localX, localY, 0)`.
El API proyecta esos puntos al plano del sketch automáticamente. No necesitás razonar en world
space para crear geometría en el sketch.

**Para un sketch sobre `xZConstructionPlane`:**
- `Point3D.create(3, 4, 0)` coloca el punto en global X=3, Z=4, Y=offset del plano
- El centro `Point3D.create(0, 0, 0)` es el origen del plano (X=0, Z=0)

### 3.3 Advertencia sobre sketches en planos paralelos a YZ

Comportamiento reportado en Autodesk Community: en sketches sobre planos de construcción paralelos
al plano YZ de origen, las coordenadas X e Y pueden aparecer desplazadas respecto a lo esperado.
Para planos paralelos a XY y XZ, las coordenadas son predecibles. Verificar `sketch.xDirection`
y `sketch.yDirection` si hay comportamientos inesperados.

### 3.4 Conversión entre espacios

```python
# Convertir punto de model space (world) a sketch space
worldPoint = adsk.core.Point3D.create(3.0, 5.0, 2.0)  # punto en world space
sketchPoint = sketch.modelToSketchSpace(worldPoint)     # resultado en sketch local

# Convertir de sketch space a model space
localPoint = adsk.core.Point3D.create(3.0, 4.0, 0.0)
modelPoint = sketch.sketchToModelSpace(localPoint)
```

Ambos métodos son "assembly context sensitive": si el sketch está en un componente que tiene una
Occurrence con transform, la conversión lo tiene en cuenta.

### 3.5 Propiedades de orientación del sketch

```python
sketch.xDirection   # Vector3D — eje X del sketch en model space (read-only)
sketch.yDirection   # Vector3D — eje Y del sketch en model space (read-only)
sketch.origin       # Point3D — origen del sketch en model space
sketch.transform    # Matrix3D — de component space a sketch space
```

### 3.6 Ejemplos de sketch circles y rectangles

```python
# Círculo centrado en el origen del sketch
circles = sketch.sketchCurves.sketchCircles
circle = circles.addByCenterRadius(adsk.core.Point3D.create(0, 0, 0), 2.5)

# Círculo desplazado en sketch space
circle2 = circles.addByCenterRadius(adsk.core.Point3D.create(5, 3, 0), 1.0)

# Rectángulo por dos esquinas (en sketch space)
lines = sketch.sketchCurves.sketchLines
rect = lines.addTwoPointRectangle(
    adsk.core.Point3D.create(-5, -3, 0),  # esquina inferior-izquierda en sketch space
    adsk.core.Point3D.create( 5,  3, 0)   # esquina superior-derecha en sketch space
)
```

### 3.7 Obtener perfiles del sketch

```python
prof = sketch.profiles.item(0)   # primer perfil cerrado
# Para múltiples perfiles (ej: sketch con círculo grande y agujero):
outer_prof = sketch.profiles.item(0)
inner_hole  = sketch.profiles.item(1)
# El orden depende del orden de creación — no hay garantía de orden geométrico
```

---

## 4. ExtrudeFeature

### 4.1 Patrón base — extrude por distancia

```python
extrudes = rootComp.features.extrudeFeatures

extInput = extrudes.createInput(
    prof,
    adsk.fusion.FeatureOperations.NewBodyFeatureOperation  # crea body nuevo
)

distance = adsk.core.ValueInput.createByReal(5.0)  # 5 cm
extInput.setDistanceExtent(
    False,   # isSymmetric=False → extrude en una sola dirección
    distance
)

extrude = extrudes.add(extInput)
body = extrude.bodies.item(0)
```

### 4.2 Dirección del extrude — CRÍTICO

**`PositiveExtentDirection` y `NegativeExtentDirection` son relativos a la normal del plano del
sketch, no a ejes globales.**

Para `xZConstructionPlane`:
- La normal del plano es el eje **+Y global**
- `PositiveExtentDirection` extrude hacia **+Y global**
- `NegativeExtentDirection` extrude hacia **-Y global**

Para `xYConstructionPlane`:
- La normal es **+Z global**
- `PositiveExtentDirection` extrude hacia **+Z global**

Para un offset plane creado desde `xZConstructionPlane` en Y=K:
- La normal sigue siendo **+Y global** (el plano es paralelo, la normal es la misma)
- `PositiveExtentDirection` extrude hacia **+Y global** (alejándose del XZ origin en dirección +Y)

```python
# Forma explícita con setOneSideExtent
extInput = extrudes.createInput(prof, adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
extent = adsk.fusion.DistanceExtentDefinition.create(
    adsk.core.ValueInput.createByString("100 mm")
)
extInput.setOneSideExtent(
    extent,
    adsk.fusion.ExtentDirections.PositiveExtentDirection   # hacia +normal del plano
)
extrude = extrudes.add(extInput)
```

### 4.3 Extrude simétrico

```python
distance = adsk.core.ValueInput.createByReal(5.0)
extInput.setDistanceExtent(True, distance)   # isSymmetric=True → ±5cm desde el plano
```

### 4.4 Extrude two-sided (asimétrico)

```python
dist1 = adsk.core.ValueInput.createByString("20 mm")
dist2 = adsk.core.ValueInput.createByString("50 mm")
deg0 = adsk.core.ValueInput.createByString("0 deg")
extInput.setTwoSidesExtent(
    adsk.fusion.DistanceExtentDefinition.create(dist1),
    adsk.fusion.DistanceExtentDefinition.create(dist2),
    deg0,  # taper angle side 1
    deg0   # taper angle side 2
)
```

### 4.5 ThroughAll extent

```python
extent_all = adsk.fusion.ThroughAllExtentDefinition.create()
extInput.setOneSideExtent(extent_all, adsk.fusion.ExtentDirections.NegativeExtentDirection)
```

### 4.6 ToEntity extent (hasta una cara o plano específico)

```python
isChained = True
extent_to = adsk.fusion.ToEntityExtentDefinition.create(targetFace, isChained)
extInput.setOneSideExtent(extent_to, adsk.fusion.ExtentDirections.PositiveExtentDirection)
```

### 4.7 FeatureOperations disponibles

```python
adsk.fusion.FeatureOperations.NewBodyFeatureOperation    # crea body nuevo
adsk.fusion.FeatureOperations.JoinFeatureOperation       # une a body existente
adsk.fusion.FeatureOperations.CutFeatureOperation        # corta body existente
adsk.fusion.FeatureOperations.IntersectFeatureOperation  # intersección
```

### 4.8 participantBodies — controlar qué bodies participan

Por defecto, un CutFeatureOperation afecta a **todos los bodies visibles que intersecten** el
extrude. Para cortar solo bodies específicos:

```python
extCutInput = extrudes.createInput(
    profForCut,
    adsk.fusion.FeatureOperations.CutFeatureOperation
)
distance = adsk.core.ValueInput.createByReal(10.0)
extCutInput.setDistanceExtent(False, distance)

# Especificar explícitamente qué bodies participan en el corte
extCutInput.participantBodies = [body1, body3]   # lista de BRepBody

extrudes.add(extCutInput)
```

**Requisito:** Los bodies en `participantBodies` deben ser intersectados por el extrude para que
el corte sea válido. Si no hay intersección, la feature queda en error state.

### 4.9 startExtent — iniciar desde una entidad (no desde el plano del sketch)

```python
offset_mm = adsk.core.ValueInput.createByString("10 mm")
start_from = adsk.fusion.FromEntityStartDefinition.create(referenceFace, offset_mm)
extInput.startExtent = start_from
```

---

## 5. CombineFeature

### 5.1 Patrón base — Join

```python
combineFeatures = rootComp.features.combineFeatures

tools = adsk.core.ObjectCollection.create()
tools.add(toolBody)  # el body que se usa para cortar o unir

combineInput = combineFeatures.createInput(targetBody, tools)
combineInput.operation = adsk.fusion.FeatureOperations.JoinFeatureOperation
combineInput.isNewComponent = False
combineInput.isKeepToolBodies = False

combineFeature = combineFeatures.add(combineInput)
```

### 5.2 CombineFeature — Cut

```python
tools = adsk.core.ObjectCollection.create()
tools.add(toolBody)

combineInput = combineFeatures.createInput(targetBody, tools)
combineInput.operation = adsk.fusion.FeatureOperations.CutFeatureOperation
combineInput.isKeepToolBodies = True   # True = el tool body sobrevive después del corte

combineFeature = combineFeatures.add(combineInput)
```

### 5.3 Cuándo usar Join vs Cut vs ExtrudeFeature CutFeatureOperation

| Situación | Patrón correcto |
|---|---|
| Sketch cerrado → nuevo body sólido | ExtrudeFeature + NewBodyFeatureOperation |
| Extrude que corta otro body directamente desde el sketch | ExtrudeFeature + CutFeatureOperation |
| Dos bodies ya existentes, uno debe cortar al otro | CombineFeature + CutFeatureOperation |
| Dos bodies ya existentes, unir en uno | CombineFeature + JoinFeatureOperation |
| Cortar solo ciertos bodies de varios existentes | ExtrudeFeature + CutFeatureOperation + participantBodies |

### 5.4 Restricciones geométricas para Cut

- El tool body **debe intersectar físicamente** al target body. Si no hay intersección, el
  `combineFeatures.add()` fallará o la feature quedará en error state.
- El tool body debe ser un sólido (BRepBody sólido, no surface body).
- El target body debe ser sólido.

### 5.5 isNewComponent

```python
combineInput.isNewComponent = False  # resultado queda en el componente actual
combineInput.isNewComponent = True   # resultado se mueve a un nuevo componente
```

### 5.6 Operaciones válidas para CombineFeatureInput.operation

```python
adsk.fusion.FeatureOperations.JoinFeatureOperation        # unión
adsk.fusion.FeatureOperations.CutFeatureOperation         # corte (substracción)
adsk.fusion.FeatureOperations.IntersectFeatureOperation   # intersección
# NewBodyFeatureOperation NO es válido para CombineFeature
```

---

## 6. Patrones correctos verificados

### 6.1 Extrude sólido desde plano XZ con offset

Caso de uso: crear un cilindro centrado en el origen, extendido simétricamente en Y.

```python
import adsk.core, adsk.fusion, traceback

def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        design = adsk.fusion.Design.cast(app.activeProduct)
        rootComp = design.rootComponent

        # 1. Sketch en XZ plane
        sketch = rootComp.sketches.add(rootComp.xZConstructionPlane)

        # 2. Círculo — Point3D en sketch space:
        #    en xZConstructionPlane, localX=global X, localY=global Z
        circles = sketch.sketchCurves.sketchCircles
        circles.addByCenterRadius(adsk.core.Point3D.create(0, 0, 0), 3.0)

        prof = sketch.profiles.item(0)

        # 3. Extrude simétrico en Y (±5 cm desde el plano XZ)
        extrudes = rootComp.features.extrudeFeatures
        extInput = extrudes.createInput(
            prof,
            adsk.fusion.FeatureOperations.NewBodyFeatureOperation
        )
        extInput.setDistanceExtent(True, adsk.core.ValueInput.createByReal(5.0))
        extrude = extrudes.add(extInput)
        body = extrude.bodies.item(0)

    except:
        if ui:
            ui.messageBox('Failed:\n{}'.format(traceback.format_exc()))
```

### 6.2 Extrude desde plano XZ offset en Y=K (plano horizontal a altura K)

```python
def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        design = adsk.fusion.Design.cast(app.activeProduct)
        rootComp = design.rootComponent

        K = 8.0  # offset en cm — el plano quedará en Y=8 (si partís de xZConstructionPlane)

        # 1. Crear plano offset
        planes = rootComp.constructionPlanes
        planeInput = planes.createInput()
        planeInput.setByOffset(
            rootComp.xZConstructionPlane,
            adsk.core.ValueInput.createByReal(K)
        )
        offsetPlane = planes.add(planeInput)

        # 2. Sketch en el plano offset
        sketch = rootComp.sketches.add(offsetPlane)

        # 3. Geometría — coordenadas en sketch space (localX=globalX, localY=globalZ)
        circles = sketch.sketchCurves.sketchCircles
        circles.addByCenterRadius(adsk.core.Point3D.create(5, 3, 0), 2.0)

        prof = sketch.profiles.item(0)

        # 4. Extrude — PositiveExtentDirection → hacia +Y global (alejándose del XZ origin)
        extrudes = rootComp.features.extrudeFeatures
        extInput = extrudes.createInput(
            prof,
            adsk.fusion.FeatureOperations.NewBodyFeatureOperation
        )
        extent = adsk.fusion.DistanceExtentDefinition.create(
            adsk.core.ValueInput.createByReal(4.0)
        )
        extInput.setOneSideExtent(
            extent,
            adsk.fusion.ExtentDirections.PositiveExtentDirection  # → +Y desde el plano
        )
        extrude = extrudes.add(extInput)

    except:
        if ui:
            ui.messageBox('Failed:\n{}'.format(traceback.format_exc()))
```

### 6.3 CUT desde plano YZ (corte vertical en X=K)

```python
def corte_desde_plano_yz(rootComp, target_body, offset_x, profundidad):
    """
    Crea un corte rectangular desde un plano paralelo al YZ, a X=offset_x.
    El sketch se hace en el plano YZ offset, las coordenadas son (localX=globalY, localY=globalZ).
    """
    # 1. Plano offset de YZ en X=offset_x
    planes = rootComp.constructionPlanes
    planeInput = planes.createInput()
    planeInput.setByOffset(
        rootComp.yZConstructionPlane,
        adsk.core.ValueInput.createByReal(offset_x)
    )
    cut_plane = planes.add(planeInput)

    # 2. Sketch en el plano — coordenadas: localX=globalY, localY=globalZ
    sketch = rootComp.sketches.add(cut_plane)
    lines = sketch.sketchCurves.sketchLines
    lines.addTwoPointRectangle(
        adsk.core.Point3D.create(-5, -5, 0),  # localX=Y global, localY=Z global
        adsk.core.Point3D.create( 5,  5, 0)
    )
    prof = sketch.profiles.item(0)

    # 3. Extrude CUT — PositiveExtentDirection desde yZConstructionPlane → +X global
    extrudes = rootComp.features.extrudeFeatures
    extInput = extrudes.createInput(
        prof,
        adsk.fusion.FeatureOperations.CutFeatureOperation
    )
    extInput.setDistanceExtent(
        False,
        adsk.core.ValueInput.createByReal(profundidad)
    )
    extInput.participantBodies = [target_body]  # solo corta este body
    return extrudes.add(extInput)
```

### 6.4 Componente hijo con geometría propia

```python
def crear_componente_hijo(rootComp):
    """
    Crea un componente hijo con una extrusión.
    La geometría se crea en el espacio local del componente hijo.
    Con transform identidad, el espacio local = world space.
    """
    # 1. Crear occurrence con transform identidad (en el origen)
    trans = adsk.core.Matrix3D.create()   # identidad
    occ = rootComp.occurrences.addNewComponent(trans)
    comp = occ.component
    comp.name = "MiComponente"

    # 2. Sketch en el plano XY del componente hijo
    sketch = comp.sketches.add(comp.xYConstructionPlane)
    circles = sketch.sketchCurves.sketchCircles
    circles.addByCenterRadius(adsk.core.Point3D.create(0, 0, 0), 5.0)
    prof = sketch.profiles.item(0)

    # 3. Feature en el componente hijo
    extrudes = comp.features.extrudeFeatures
    extInput = extrudes.createInput(
        prof,
        adsk.fusion.FeatureOperations.NewBodyFeatureOperation
    )
    extInput.setDistanceExtent(False, adsk.core.ValueInput.createByReal(10.0))
    extrude = extrudes.add(extInput)

    # 4. Reusar el componente en otra posición (segunda occurrence, desplazada)
    trans2 = adsk.core.Matrix3D.create()
    trans2.setCell(0, 3, 15.0)   # desplazar 15 cm en X
    occ2 = rootComp.occurrences.addExistingComponent(comp, trans2)

    return comp, occ, occ2
```

### 6.5 CombineFeature Cut (dos bodies existentes)

```python
def combine_cut(rootComp, target_body, tool_body, keep_tool=False):
    """
    Substrae tool_body de target_body usando CombineFeature.
    Requiere que ambos bodies se intersecten físicamente.
    """
    combineFeatures = rootComp.features.combineFeatures

    tools = adsk.core.ObjectCollection.create()
    tools.add(tool_body)

    combineInput = combineFeatures.createInput(target_body, tools)
    combineInput.operation = adsk.fusion.FeatureOperations.CutFeatureOperation
    combineInput.isNewComponent = False
    combineInput.isKeepToolBodies = keep_tool

    return combineFeatures.add(combineInput)
```

---

## 7. Anti-patrones conocidos

### AP-01: Usar world space coordinates en sketch methods

```python
# MAL — pasando coordenadas en world space cuando el sketch espera sketch space
sketch = rootComp.sketches.add(offsetPlane)  # plano a Y=8
# Incorrecto: intentar poner el punto en world space
circles.addByCenterRadius(adsk.core.Point3D.create(3, 8, 4), 2.0)
# El "8" en Y no tiene efecto — el sketch proyecta al plano, el Y es ignorado

# BIEN — usar siempre sketch space (localX, localY, 0)
circles.addByCenterRadius(adsk.core.Point3D.create(3, 4, 0), 2.0)
# En xZConstructionPlane: localX=3 → globalX=3, localY=4 → globalZ=4
```

**Raíz del bug:** El API acepta coordenadas fuera del plano del sketch (no lanza error), pero las
proyecta silenciosamente. El punto (3, 8, 4) sobre un plano XZ offset se proyecta a (3, K, 4),
ignorando el Y=8. El resultado visualmente correcto puede obtenerse por azar si K≈8.

### AP-02: Asumir que el extrude direction es un eje global fijo

```python
# MAL — asumir que PositiveExtentDirection siempre va en +Z
# Un sketch en xYConstructionPlane extruye en +Z con PositiveExtentDirection
# Un sketch en xZConstructionPlane extruye en +Y con PositiveExtentDirection
# Un sketch en yZConstructionPlane extruye en +X con PositiveExtentDirection
# Un sketch en plano inclinado extruye en la dirección normal a ese plano

# REGLA: PositiveExtentDirection = dirección de la normal positiva del plano del sketch
# Si necesitás dirección específica, usá NegativeExtentDirection o creá el plano con offset negativo
```

### AP-03: Intentar crear un sketch sobre un plano en error state

```python
# MAL — sin verificar health state del plano
offsetPlane = planes.add(planeInput)
sketch = rootComp.sketches.add(offsetPlane)   # puede fallar silenciosamente

# BIEN — verificar siempre antes de usar
offsetPlane = planes.add(planeInput)
if offsetPlane is None:
    raise RuntimeError("El plano no pudo ser creado")
health = offsetPlane.healthState
if health == adsk.fusion.FeatureHealthStates.ErrorFeatureHealthState:
    raise RuntimeError(f"Plano en error: {offsetPlane.errorOrWarningMessage}")
sketch = rootComp.sketches.add(offsetPlane)
```

### AP-04: CombineFeature cut sin intersección

```python
# MAL — tool_body y target_body no se intersectan
# Resultado: la feature se crea pero queda en WarningFeatureHealthState o ErrorFeatureHealthState
# No lanza excepción Python — el error es silencioso

# BIEN — verificar antes:
# 1. Asegurarse que las bounding boxes de los bodies se intersectan
# 2. O usar tryCreate en lugar de add (no disponible en CombineFeature, pero sí es buena práctica
#    verificar el resultado)
combineFeature = combineFeatures.add(combineInput)
if combineFeature.healthState == adsk.fusion.FeatureHealthStates.ErrorFeatureHealthState:
    raise RuntimeError(f"Combine falló: {combineFeature.errorOrWarningMessage}")
```

### AP-05: Acceder a xZConstructionPlane en componente hijo que es None

```python
# MAL — comp puede ser None si la occurrence no tiene componente asignado
occ = rootComp.occurrences.addNewComponent(trans)
comp = occ.component
plane = comp.xZConstructionPlane   # AttributeError si comp es None

# BIEN
occ = rootComp.occurrences.addNewComponent(trans)
comp = occ.component
if comp is None:
    raise RuntimeError("La occurrence no tiene componente asociado")
plane = comp.xZConstructionPlane
```

### AP-06: Reusar el mismo planeInput para múltiples planos sin verificar

```python
# POTENCIALMENTE PROBLEMÁTICO — reusar el mismo planeInput
planeInput = planes.createInput()
planeInput.setByOffset(rootComp.xZConstructionPlane, v1)
plane1 = planes.add(planeInput)
planeInput.setByOffset(rootComp.xZConstructionPlane, v2)  # modifica el input
plane2 = planes.add(planeInput)
# plane1 puede quedar en estado inconsistente en algunas versiones del API

# MÁS SEGURO — crear nuevo input para cada plano
planeInput1 = planes.createInput()
planeInput1.setByOffset(rootComp.xZConstructionPlane, v1)
plane1 = planes.add(planeInput1)

planeInput2 = planes.createInput()
planeInput2.setByOffset(rootComp.xZConstructionPlane, v2)
plane2 = planes.add(planeInput2)
```

### AP-07: Usar setByPlane en modo paramétrico

```python
# MAL — setByPlane solo funciona en Direct Modeling mode
planeInput.setByPlane(myAdskPlane)
planes.add(planeInput)  # falla en modo paramétrico (Parametric Design mode)

# BIEN — usar setByOffset, setByAngle, etc. que son paramétricos
```

### AP-08: `sketch.profiles.item(0)` sin entender qué perfil es cuál

```python
# TRAMPA — el orden de profiles no es geométrico, es de creación
# Un sketch con un círculo grande y un círculo pequeño concéntrico genera 2 profiles:
# item(0) puede ser el anillo (entre ambos) o el círculo interior según el orden de creación

# VERIFICAR cuál profile necesitás antes de usarlo, o dibujar sketches con un solo profile
# para evitar ambigüedad
for i in range(sketch.profiles.count):
    prof = sketch.profiles.item(i)
    # inspeccionar area, areaProperties, etc. si hay ambigüedad
```

---

## 8. Resumen de decisiones de diseño del API

1. **Toda la geometría de un sketch está en sketch space (2D local).** El Point3D(x, y, 0) pasado
   a métodos de sketch se interpreta en las coordenadas locales del sketch, no en world space.
   El Z siempre debe ser 0 — el API proyecta silenciosamente.

2. **Los componentes hijos TIENEN `xYConstructionPlane`, `xZConstructionPlane`,
   `yZConstructionPlane`.** No son exclusivos del root component. Su comportamiento es en el
   espacio local del componente.

3. **`PositiveExtentDirection` = normal positiva del plano del sketch.** No es un eje global
   absoluto. Para el plano XZ, la normal es +Y. Para XY, es +Z. Para YZ, es +X.

4. **`setByOffset` crea el plano en el componente cuyo `constructionPlanes` se usó.** El plano
   resultante no "migra" al root. Si se creó desde `comp.constructionPlanes`, vive en `comp`.

5. **`participantBodies` en ExtrudeFeature permite corte selectivo.** Sin especificarlo, todos los
   bodies visibles que intersecten el extrude son cortados.

6. **Los errores de features son silenciosos en Python.** `planes.add()`, `extrudes.add()`,
   `combineFeatures.add()` no lanzan excepciones si hay un problema geométrico. Verificar
   `.healthState` después de cada operación crítica.

7. **Una Occurrence es un puntero a un Component.** El Component tiene la geometría real. Múltiples
   Occurrences pueden apuntar al mismo Component. Cambiar el Component afecta todas las Occurrences.
