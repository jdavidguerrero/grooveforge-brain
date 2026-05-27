# Part 04 — Cheek Derecho Guadua (Audio OUT + USB)

> **Autor:** Industrial Design Agent
> **Version:** 0.2 — 4 puertos todo USB-C, D-10 resuelto
> **Status:** PROPUESTO
> **Material:** Guadua angustifolia laminada, sellado epoxico
> **Fabricacion:** Corte manual/CNC + acabado artesanal + sellado
> **Derivado de:** `apps/design/enclosure/01-enclosure-design-spec.md` v0.3, §2, §5, §9, §10
> **Archivo CAD target:**
> - `apps/design/3d-models/04-cheek-right.f3d`
> - `apps/design/3d-models/04-cheek-right.step`

---

## 0. Funcion de la pieza

El cheek derecho es el panel lateral derecho del instrumento. Sus funciones:

- Cubre y sella la cara lateral derecha del PETG body
- Aloja 4 conectores (v0.4): 1× USB-C (Power + Audio/MIDI Teensy), 2× jack 1/4" TRS OUT, 1× USB-C Host MIDI
- Identidad visual simetrica al cheek izquierdo: mismo material, mismo perfil inclinado
- Se encastra en el rebaje de 6mm derecho del PETG body

**v0.4 — arquitectura USB simplificada:**
- USB-C Power + USB-C Teensy consolidados en un solo conector (pigtail micro-USB→USB-C en prototipo)
- USB-A Host reemplazado por USB-C Host (estetica todo-USB-C; adaptador para teclados legacy)
- 4 puertos vs 5 del v0.3 — D-10 resuelto completamente (clearance frontal 12.75mm)

---

## 1. Material y proceso de fabricacion

Identico al cheek izquierdo (`03-cheek-left-guadua.md` §1). Los dos cheeks se fabrican del mismo
lote de guadua, idealmente del mismo panel laminado para uniformidad de veta y color.

| Parametro | Valor |
|-----------|-------|
| Material | Guadua angustifolia laminada (tablex o lamina) |
| Espesor nominal | 12mm (CHEEK_T) |
| Cutouts | 2× rectangular (USB-C Data + USB-C Host, identicos) + 2× circular (TRS jacks) |
| Post-proceso | Identico a cheek izquierdo: lijar P120→P240→P400, sellado epoxico |
| Particularidad | Los cutouts rectangulares para USB requieren sierra de calado o freza CNC — mas criticos que los circulares del cheek izquierdo |

### Nota sobre cutouts rectangulares en guadua

Los cutouts circulares (jacks) se hacen con broca Forstner o sierra copa — herramienta estandar.
Los cutouts rectangulares (USB-C: 10.5×5mm, USB-A: 15×8mm) requieren:
- Opcion A: taladro para esquinas + sierra de calado — artesanal, riesgo de astillado
- Opcion B: freza CNC con herramienta de 1mm — preciso pero requiere acceso a CNC
- Opcion C: taladro multiple de Ø5mm + lima de aguja para cuadrar — lento pero controlado

Para prototipo v0.2: Opcion C es la mas segura para la guadua laminada.
Para produccion v1.0: Opcion B (CNC) si el volumen lo justifica.

---

## 2. Parametros requeridos del master

Fuente: `00-master-parameters.md`.

| Parametro | Valor | Descripcion |
|-----------|-------|-------------|
| `CHEEK_T` | 12mm | Espesor del cheek |
| `BODY_D` | 100mm | Profundidad (largo del cheek en Z) |
| `BODY_H` | 38mm | Altura del cheek (caja rectangular v0.6 — BODY_H_F/H_R eliminados) |
| `CHEEK_REBAJE_D` | 6mm | Profundidad encastre en el PETG |
| `INSERT_M3_OD` | 4.2mm | Agujero para heat insert M3 |
| `JACK_HOLE` | 10.5mm | Diametro cutout jack 1/4" TRS |
| `JACK_HOLE_GUAD_TOL` | 0.5mm | Tolerancia cutout guadua = usar Ø11.0mm |
| `JACK_Z` | 21mm | Altura Z centro jacks desde base |
| `USBC_W` | 10.5mm | Cutout USB-C ancho (v0.4 — ambos puertos son USB-C identicos) |
| `USBC_H` | 5.0mm | Cutout USB-C alto |
| `R_USBC_DATA_Z` | 15mm | USB-C (Power + Audio/MIDI Teensy): Z desde panel trasero |
| `R_TRS_OUTL_Z` | 38mm | TRS OUT L: Z desde panel trasero |
| `R_TRS_OUTR_Z` | 60mm | TRS OUT R: Z desde panel trasero |
| `R_USBC_HOST_Z` | 82mm | USB-C Host MIDI: Z desde panel trasero (D-10 resuelto) |

---

## 3. Geometria — sketches y operaciones (en orden)

### Op. 1 y 2: Cuerpo del cheek (identico al izquierdo, simetrico)
- Mismos sketches y operaciones que `03-cheek-left-guadua.md` §3 Op. 1 y 2
- La diferencia: el extrude va en X+ (hacia la derecha) en vez de X-
- Perfil identico: rectangulo 38mm de alto × 100mm de profundidad (v0.6 — no inclinado)

### Op. 3: Agujeros de fijacion (heat inserts M3)
- Identico al cheek izquierdo: 2 agujeros Ø4.2mm en la cara interior
- Posiciones: Z=25mm (Y=19mm) y Z=75mm (Y=19mm) — centrado en BODY_H/2=19mm (altura uniforme)

### Op. 4: Cutouts de conectores (para verificacion en assembly)
- En la cara exterior del cheek derecho (X exterior):

Todos los cutouts centrados en Y=JACK_Z=21mm desde la base.

Todos los cutouts centrados en Y=JACK_Z=21mm desde la base.

- USB-C Data: rectangulo 11.0×5.5mm (USBC_W+0.5 tolerancia guadua × USBC_H+0.5),
  centrado en (Z=R_USBC_DATA_Z=15mm, Y=21mm)
- TRS OUT L: circulo Ø11.0mm centrado en (Z=R_TRS_OUTL_Z=38mm, Y=21mm)
- TRS OUT R: circulo Ø11.0mm centrado en (Z=R_TRS_OUTR_Z=60mm, Y=21mm)
- USB-C Host: rectangulo 11.0×5.5mm centrado en (Z=R_USBC_HOST_Z=82mm, Y=21mm)

D-10 RESUELTO: con 4 puertos (eliminando USB-A y consolidando USB-C), los clearances son:
- USB-C Data (Z=15, borde ant Z=20.25) → TRS L (Z=38, borde pos Z=32.75): gap 12.5mm OK
- TRS L (borde ant 43.25) → TRS R (borde pos 54.75): gap 11.5mm OK
- TRS R (borde ant 65.25) → USB-C Host (borde pos 76.75): gap 11.5mm OK
- USB-C Host (borde ant Z=87.25) → borde frontal cheek (Z=100): clearance 12.75mm OK

---

## 4. Cutouts y holes (tabla completa)

Posiciones en Z desde el panel trasero del instrumento (fondo). Centro Y = 21mm (JACK_Z).

| Componente | Forma | Dim. nominal | Dim. en guadua | Tolerancia | Z desde fondo | Clearance con anterior |
|------------|-------|-------------|----------------|------------|---------------|----------------------|
| USB-C (Power + Data) | Rectangular | 10.5×5.0mm | 11.0×5.5mm | ±0.5mm | 15mm | — (primero, 9.75mm del fondo) |
| TRS OUT L | Circular | Ø10.5mm | Ø11.0mm | ±0.5mm | 38mm | Gap borde a borde: 12.5mm OK |
| TRS OUT R | Circular | Ø10.5mm | Ø11.0mm | ±0.5mm | 60mm | Gap borde a borde: 11.5mm OK |
| USB-C Host (MIDI) | Rectangular | 10.5×5.0mm | 11.0×5.5mm | ±0.5mm | 82mm | Gap borde a borde: 11.5mm OK; clearance frontal 12.75mm OK |
| Fijacion 1 (heat insert) | Circular ciego | Ø4.2mm | Ø4.2mm | ±0.2mm | 25mm (cara interior) | Posicion Y=19mm (BODY_H/2) |
| Fijacion 2 (heat insert) | Circular ciego | Ø4.2mm | Ø4.2mm | ±0.2mm | 75mm (cara interior) | Posicion Y=19mm (BODY_H/2) |

### Verificacion de clearance USB-A vs borde frontal

Con Z=97mm (valor original master spec): borde anterior = 97+7.5=104.5mm > 100mm del cheek. IMPOSIBLE.
Con Z=92mm (primera iteracion D-10): borde anterior = 92+7.5=99.5mm. Clearance pared: 0.5mm. INSUFICIENTE.
Con Z=90mm: borde anterior = 97.5mm. Clearance: 2.5mm. Marginal.
Con Z=88mm: borde anterior = 95.5mm. Clearance: 4.5mm. Aceptable pero con conflicto con USB-C Teensy.

La resolucion requiere cambio arquitectural — documentado en `99-assembly.md` §Issues.

---

## 5. Interfaces con otras piezas

| Pieza | Tipo de interface | Tolerancia |
|-------|------------------|------------|
| PETG body (01) | Encastre en rebaje 6mm derecho + Araldite + 2 tornillos M3×16 | Clearance 0.2mm igual al cheek izquierdo |
| Top panel aluminio (02) | Gap visual 0.5-1mm en borde derecho del panel | Sin fijacion directa |
| Conectores USB-C (Teensy) | El conector USB-C del Teensy es PCB-mount; el jack del cheek es solo una guia de acceso | El cutout en el cheek debe estar alineado con el conector en la PCB. Tolerancia de alineacion: ±1mm (el conector tiene tolerancia) |
| Conector USB-A Host | Identico a USB-C pero el conector USB-A es mas voluminoso y tiene mas rigidez mecanica | El cutout debe tener al menos 0.5mm de clearance en todas las dimensiones con el conector fisico |

---

## 6. Validacion post-fabricacion

Identico a `03-cheek-left-guadua.md` §6, mas:

| Check adicional | Metodo | Criterio pass |
|-----------------|--------|---------------|
| Cutouts rectangulares USB | Insertar conector USB-C macho y USB-A macho | Cada conector pasa limpio sin tocar las paredes del cutout |
| Clearance entre cutouts adyacentes | Calibre entre bordes de cutouts consecutivos | Minimo 2mm entre cualquier par de cutouts adyacentes |
| Pared frontal post-USB-A | Calibre desde borde del cutout USB-A hasta cara frontal | Minimo 2mm (depende de posicion final de D-10) |

---

## 7. Export targets

| Archivo | Formato | Path | Uso |
|---------|---------|------|-----|
| Cheek right Fusion archive | .f3d | `apps/design/3d-models/04-cheek-right.f3d` | Edicion CAD |
| Cheek right STEP | .step | `apps/design/3d-models/04-cheek-right.step` | Referencia fabricacion |

---

## 8. Workflow Fusion 360 (paso a paso ejecutable)

```
1.  Activar componente "04_Cheek_Right" en el browser.

2.  Sketch "Cheek_R_Profile" en plano YZ de Fusion (rectangulo — v0.6):
    - (0,0) a (BODY_D=100,0): base
    - (0,0) a (0, BODY_H=38): cara trasera vertical
    - (BODY_D=100,0) a (BODY_D=100, BODY_H=38): cara frontal vertical
    - (0,38) a (100,38): techo horizontal (plano)
    Finish Sketch.

3.  Extrude "Cheek_R_Profile": CHEEK_T = 12mm en X+ (hacia la derecha, simetrico al izquierdo).
    La cara interior queda en X=0; exterior en X=+12mm.

4.  Agujeros de fijacion (heat inserts) — identicos al cheek izquierdo:
    Sketch en cara interior (X=0):
    - Ø4.2mm, profundidad 7mm en (Z=25mm, Y=19mm)
    - Ø4.2mm, profundidad 7mm en (Z=75mm, Y=19mm)

5.  Cutouts de conectores v0.4 (para verificacion):
    Sketch "Cheek_R_Ports" en cara exterior (X=+12mm):
    - Rectangulo 11.0×5.5mm centrado en (Z=15mm, Y=21mm): USB-C Power + Audio/MIDI Teensy
    - Circulo Ø11.0mm centrado en (Z=38mm, Y=21mm): TRS OUT L
    - Circulo Ø11.0mm centrado en (Z=60mm, Y=21mm): TRS OUT R
    - Rectangulo 11.0×5.5mm centrado en (Z=82mm, Y=21mm): USB-C Host MIDI (D-10 resuelto)
    Cut extrude pasante (12mm en X-).
    NOTA: en la verificacion de assembly podra verse si el USB-A interfiere con el borde frontal.

6.  Espejo (opcional en el ensamblaje):
    El cheek derecho puede modelarse como mirror del izquierdo en Fusion (Mirror Body)
    si el perfil es identico. La unica diferencia es la posicion de los cutouts de conectores.
    Modelar independientemente es mas claro.

7.  Posicionar en ensamblaje:
    Joint para alinear cara interior del cheek (X=0) con el fondo del rebaje derecho del PETG.

8.  Exportar: `apps/design/3d-models/04-cheek-right.step`.
```
