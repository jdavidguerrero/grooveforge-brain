# GrooveForge Brain — Enclosure Design Spec v0.3

> **Autor:** Juan Guerrero (GPROG) / Industrial Design Agent
> **Versión:** 0.4 — USB consolidado (2× USB-C), elimina USB-A y USB-C Power separado
> **Fecha:** 2026-05-26
> **Status del documento:** vivo — secciones marcadas DEFINIDO / PROPUESTO / PENDIENTE
> **Derivado de:** `apps/docs/01-architecture.md` v0.3 + `apps/docs/00-master-strategy.md` §8.2

---

## 0. Por qué este documento existe

El enclosure no es packaging. Es el primer punto de contacto del usuario con el instrumento y define si el Brain se percibe como un producto boutique de $599 o como un hobby kit caro.

Este spec precede cualquier archivo CAD. Todo lo que no esté especificado aquí no se modela.

### Decisiones de versión que impactan todo el diseño

| Decisión | v0.1/v0.2 spec | v0.3 — DEFINITIVO |
|---|---|---|
| Batería | LiPo 3000mAh integrada | **Sin batería — USB-C only** (Batería = versión Pro futura) |
| ESP32-S3 | Módulo plano sobre PCB | **Módulo elevado sobre display** (cero footprint PCB adicional) |
| ENC NAV position | A la derecha del display | **Debajo del display** |
| Botones | 4× Kailh Choc V2 fila inferior | **4× Kailh Choc V2 retroiluminados, fila superior** |
| Filter bypass switch | En panel | **Eliminado** |
| Volumen | Pot 16mm | **Alps RK097N (9mm body) — mantenido** |

**Por qué mantener el VOL:** en performance en vivo el volumen se ajusta por músculo, sin mirar. Un encoder requiere contexto de menú. El RK097N ocupa 9mm de body — el costo en footprint es mínimo vs el beneficio ergonómico.

**Por qué diferir la batería:** la celda LiPo (65×45×8mm) consumía el 33% del área PCB disponible. Sin ella, el device de 180×100mm se vuelve factible con Teensy 4.1 dev board. La versión Pro (con batería) puede usar el mismo enclosure con la celda bajo la PCB en un compartimento dedicado — sin rediseñar el exterior.

---

## 1. Dimensiones Generales v0.3

**Status: PROPUESTO — target para prototipo v0.2 y producción v1.0**

### Device dimensions

```
PETG body central:  180mm × 100mm × 42mm frontal / 30mm trasero
Guadua cheeks:      +12mm cada lado (izq y der)
Total instrumento:  204mm × 100mm × 42mm frontal / 30mm trasero
```

El perfil más flat (42mm vs 52mm de v0.2) es posible porque sin LiPo el componente más alto es el Teensy 4.1 + conector (aprox 8mm sobre PCB) más el standoff (5mm) más la PCB (1.6mm) = ~15mm. Con paredes PETG de 3mm, la altura mínima es ~21mm. El target de 42mm frontal da 21mm de margen — suficiente para el routing interno y la inclinación ergonómica.

### Comparación con referencias

| Instrumento | Ancho | Prof. | Alto | Peso |
|---|---|---|---|---|
| OP-1 original | 230mm | 101mm | 28mm | 1kg |
| OP-1 Field | 228mm | 104mm | 37mm | 1.1kg |
| **GrooveForge Brain v0.3** | **204mm** | **100mm** | **42mm** | **~400g est.** |
| OP-Z | 228mm | 57mm | 10mm | 280g |

El Brain v0.3 tiene el mismo orden de magnitud del OP-1: portátil, sobre escritorio, cabe en mochila.

### Anatomía del enclosure

```
VISTA FRONTAL

│← 12mm →│←————— 180mm (PETG body) ——————→│← 12mm →│
         ┌──────────────────────────────────┐
         │   TOP PANEL ALUMINIO 2mm         │
┌────────┤  [B1][B2][B3][B4]               ├────────┐
│        │  ENC1  [DISPLAY] ENC2            │        │
│ GUADUA │       [ENC NAV]                 │ GUADUA │
│ LEFT   │       [VOL]                     │ RIGHT  │
│ INPUTS │  PETG body (anthracite)         │OUTPUTS │
└────────┴──────────────────────────────────┴────────┘
│← —————————————— 204mm total ——————————————————————→│

Altura frontal: 42mm
Altura trasera: 30mm  (tilt ~7° — más suave que v0.2)
```

### Relación PCB → enclosure

| Dimensión | Internal (PETG body 180mm, paredes 3mm×2) | PCB target |
|---|---|---|
| Ancho (X) | 174mm usable | 168mm PCB (3mm clearance c/lado) |
| Profundidad (Y) | 94mm usable | 88mm PCB (3mm clearance c/lado) |

Con 168×88mm de PCB, la distribución de componentes es cómoda incluso con Teensy 4.1 dev board (61×18mm).

---

## 2. Estrategia de Materiales v0.3

**Status: DEFINIDO**

### Presupuesto (inmutable del BOM)

| Material | Budget BOM | Uso |
|---|---|---|
| PETG cuerpo | $5.00 | Shell central + base |
| Aluminio CNC top panel | $6.00 | Panel superior 2mm anodizado |
| Guadua laminada | $2.50 | Paneles cheek laterales (2×) |
| **Total enclosure** | **$13.50** | Tope no negociable |

### Superficie por material

#### PETG — Cuerpo central 180×100mm

Shell estructural. Paredes laterales = backing de los cheeks de guadua (con rebaje de 12mm para encastre). Panel trasero = solo pogo connector. Base = flat, con patas de goma antideslizante en 4 esquinas.

Color: near-black anthracite, textura FDM visible en laterales y trasero.

#### Aluminio — Top panel 180×100mm

Cubre el cuerpo PETG central. Todos los controles pasan por este panel. Anodizado anthracite clase 2, laser engraving para labels. **No se extiende sobre los cheeks de guadua** — la junta aluminio/guadua en los 4 bordes del panel es un detalle de diseño explícito.

#### Guadua — Cheeks laterales 12mm × 100mm × altura

Guadua angustifolia laminada. Sellado epoxy obligatorio (humedad Bogotá 70-80% HR). Acabado satinado. Cutouts de puertos realizados post-ensamblaje, guiados por los cutouts del PETG para alineación perfecta.

Ver detalles de fabricación en §9.

---

## 3. Layout del Panel Frontal (Top) v0.3

**Status: PROPUESTO — basado en referencia visual aprobada**

### Sistema de coordenadas

Origen (0,0) = esquina inferior izquierda del top panel, visto desde arriba.
X crece hacia la derecha. Y crece hacia atrás (alejándose del usuario).
Top panel: 180mm × 100mm, esquinas R3mm.

### Filosofía de layout v0.3

```
ZONA SUPERIOR — fila de botones retroiluminados
ZONA MEDIA    — ENC1 / DISPLAY+ESP32 elevado / ENC2
ZONA INFERIOR — ENC NAV (con LED ring) centrado bajo el display
ZONA ACCENT   — VOL pot discreto, esquina inferior
```

La lógica de lectura es vertical: el ojo baja desde los botones de contexto (qué modo estoy), pasa por el display (qué está pasando), llega al encoder de acción (ENC NAV — ASK AI / navegar). Los encoders de parámetro (ENC1 izq, ENC2 der) son acción lateral — la mano llega sin mirar.

### Tabla de posiciones (centro del componente)

| Componente | X (mm) | Y (mm) | Notas |
|---|---|---|---|
| B1 — ENGINE | 30 | 82 | Kailh Choc V2 retroiluminado |
| B2 — FX | 58 | 82 | Kailh Choc V2 retroiluminado |
| B3 — AI / SCALE | 122 | 82 | Kailh Choc V2 retroiluminado |
| B4 — PRESET | 150 | 82 | Kailh Choc V2 retroiluminado |
| ENC1 — CUTOFF (eje) | 22 | 55 | ALPS EC11, knob Ø18mm |
| Display GC9A01 (centro) | 90 | 57 | Ø35mm cutout, módulo ESP32 elevado |
| ENC2 — RESONANCE (eje) | 158 | 55 | ALPS EC11, knob Ø18mm |
| ENC NAV — ACTION (eje) | 90 | 30 | ALPS EC11 con 16-LED ring, knob Ø22mm |
| VOL pot (eje) | 22 | 14 | Alpha RV09 (9mm), knob Ø12mm discreto |

**Nota ENC NAV:** centrado bajo el display. El LED ring (Ø38mm exterior) necesita 19mm de radio libre desde el centro del encoder. Distancia ENC NAV — borde display: 57 - 30 = 27mm entre centros. Con display Ø35mm (radio 17.5mm) y ring radio externo 19mm, el gap borde display → borde ring exterior es 27 - 17.5 - 19 = -9.5mm. **Interferencia — corregir:** mover ENC NAV a Y=22 y display a Y=60, o reducir el ring a Ø32mm exterior. Ver D-08 en §14.

**Nota botones B1/B2 vs B3/B4:** los botones están partidos en dos grupos de 2, con el display entre medias — igual que la imagen de referencia. El gap visual entre B2 y B3 (X=58 a X=122, gap de 64mm) coincide con el ancho del display+encoders. Esto es intencional: los botones de contexto (ENGINE, FX) van a la izquierda del display; los botones de acción (AI, PRESET) a la derecha.

### Sketch ASCII dimensional (vista top)

```
180mm
|←————————————————————————————————————————→|

┌──────────────────────────────────────────┐
│  [B1]  [B2]              [B3]  [B4]     │  Y=82
│                                          │
│  ◎         .────.              ◎        │  Y=57/55
│  ENC1     ( GC9A )            ENC2      │
│ CUTOFF     `────'           RESONANCE   │
│           [ESP32↑]                      │
│              ○○                         │
│           ◎○    ○                       │  Y=30
│           ○  ENCNAV  ○  ← LED ring     │
│              ○○                         │
│  ◎                                      │  Y=14
│  VOL                                    │
└──────────────────────────────────────────┘
  ↑                          ↑
  X=22                       X=158
```

### Verificación de clearances críticos

**ENC1 ↔ borde panel izquierdo:** X=22 - knob_radius(9) = 13mm desde el borde. Con borde redondeado R3mm del aluminio, el knob queda 10mm del borde visual — ajustado pero funcional.

**B1 (X=30) ↔ ENC1 (X=22) vertical:** botones en Y=82, ENC1 en Y=55 → separación vertical 27mm. Con keycap Kailh de 11×11mm y knob ENC1 de Ø18mm, el clearance es >10mm. OK.

**ENC NAV ↔ VOL:** ENC NAV en (90,30), VOL en (22,14). Distancia = √(68²+16²) = 70mm. Sin interferencia.

**B1-B2 gap vs B3-B4 gap:** B2 en X=58, B3 en X=122 → gap de 64mm entre grupos. Display de 35mm + 2×(ENC shaft X=90-58=32mm de margen). Los knobs de ENC1/ENC2 no interfieren con los keycaps de los botones porque están en diferente fila (Y=55 vs Y=82).

---

## 4. Panel Lateral Izquierdo (Guadua Cheek LEFT — AUDIO IN)

**Status: PROPUESTO v0.3 — sin cambios vs v0.2**

Side panel izquierdo = entradas de audio. Sin MIDI DIN en BOM actual.

### Vista del cheek LEFT (exterior → interior)

```
←———————————————— 100mm (profundidad) ————————————————→
     FONDO (Y=0)                     FRENTE (Y=100)

┌──────────────────────────────────────────────────────┐ ─
│            ○                ○              ○         │ │
│       [1/4" TRS]      [1/4" TRS]      [3.5mm]       │ │ 42mm
│         IN L            IN R          HEAD IN        │ │
└──────────────────────────────────────────────────────┘ ─
              ↑                ↑              ↑
             Y=30             Y=55           Y=78
```

| Puerto | Y desde fondo | Z centro | Componente |
|---|---|---|---|
| 1/4" TRS IN L | 30mm | 21mm | Jack Neutrik NYS229 o equiv. |
| 1/4" TRS IN R | 55mm | 21mm | Jack Neutrik NYS229 o equiv. |
| 3.5mm AUX/Headphone IN | 78mm | 21mm | Jack 3.5mm stereo PCB-mount |

---

## 5. Panel Lateral Derecho (Guadua Cheek RIGHT — AUDIO OUT + USB)

**Status: DEFINIDO v0.4 — 4 puertos, todo USB-C, D-10 resuelto**

Side panel derecho = salidas de audio + conexiones digitales.

### Decisión USB v0.4 (origen: revisión de arquitectura)

| Decision | v0.3 | v0.4 DEFINITIVO | Razon |
|---|---|---|---|
| USB Power | USB-C Power separado | **Consolidado en USB-C Teensy** | Teensy USB provee 5V cuando conectado; power bank funciona igual |
| USB Teensy | Puerto propio USB-C | **USB-C unico (Power + Audio + MIDI)** | Un solo cable al PC para todo |
| USB Host | USB-A female | **USB-C Host** | Estetica todo USB-C; adaptador USB-C→USB-A para teclados legacy |
| Total puertos | 5 | **4** | Elimina D-10, clearances holgados, BOM mas barato |

**Implementacion en prototipo v0.2 (Teensy dev board):**
El Teensy 4.1 dev board tiene micro-USB nativo. Se usa un **pigtail interno micro-USB→USB-C**
(cable corto < 15cm, panel-mount USB-C female) para exponer el puerto como USB-C en el panel.
Solución estandar en mundo DIY synth (Mutable Instruments, Befaco).

**Implementacion en v1.0 (PCB custom):**
iMXRT1062 bare con USB-C nativo — no requiere adaptador. Mismo conector panel, mismo cutout.

### Vista del cheek RIGHT (exterior → interior)

```
←———————————————— 100mm (profundidad) ————————————————→
     FONDO (Y=0)                     FRENTE (Y=100)

┌──────────────────────────────────────────────────────┐ ─
│    ⬛                ○           ○             ⬛     │ │
│ [USB-C           [1/4" TRS]  [1/4" TRS]   [USB-C    │ │ 42mm
│  Power+Data]       OUT L       OUT R       HOST]     │ │
└──────────────────────────────────────────────────────┘ ─
      ↑                  ↑           ↑             ↑
     Z=15               Z=38        Z=60          Z=82
```

| Puerto | Z desde fondo | Y centro | Componente | Notas |
|---|---|---|---|---|
| USB-C (Power + Audio/MIDI) | 15mm | 21mm | USB-C female panel-mount | Pigtail interno micro-USB→USB-C en v0.2 |
| 1/4" TRS OUT L | 38mm | 21mm | Jack Neutrik NYS229 o equiv. | |
| 1/4" TRS OUT R | 60mm | 21mm | Jack Neutrik NYS229 o equiv. | |
| USB-C Host | 82mm | 21mm | USB-C female panel-mount | Teensy USB Host pins; adaptador para teclados USB-A |

**Clearances verificados (4 puertos):**
- USB-C (Z=15, span Z=9.75→20.25) → TRS L (Z=38, span 32.75→43.25): gap 12.5mm OK
- TRS L → TRS R (Z=60, span 54.75→65.25): gap 11.5mm OK
- TRS R → USB-C Host (Z=82, span 76.75→87.25): gap 11.5mm OK
- USB-C Host → borde frontal (Z=100): clearance **12.75mm** — D-10 RESUELTO

---

## 6. Panel Trasero (PETG)

**Status: PROPUESTO v0.3**

Simplificado: solo pogo connector de expansión modular. Sin otros puertos.

| Componente | X centro | Z centro | Notas |
|---|---|---|---|
| Pogo 6-pin expansion | 90mm (centro) | 15mm | Würth WR-WST o equiv. |

---

## 7. Componentes del Panel — Especificaciones

**Status: DEFINIDO / PROPUESTO por componente**

### Encoders — ALPS EC11

Los tres encoders usan el mismo footprint ALPS EC11. El knob diferencia su función visualmente.

| Encoder | Knob Ø | Knob estilo | Función |
|---|---|---|---|
| ENC1 (CUTOFF) | 18mm | Knurled, bajo perfil | Cutoff / param navigation |
| ENC2 (RESONANCE) | 18mm | Knurled, bajo perfil | Resonance / value |
| ENC NAV (ACTION) | 22mm | Liso, perfil alto | Navigate / ASK AI / confirm |

El ENC NAV más grande y alto lo hace físicamente distinguible sin mirar — el músico lo encuentra por tacto.

### LED Ring — ENC NAV

16× WS2812B-2020 (2×2mm) montados en una PCB anular. La PCB del ring se monta concéntrica al encoder con espacio para el eje. Las dimensiones del ring deben resolverse contra el clearance del display (ver D-08).

### Botones — Kailh Choc V2 Brown (retroiluminados)

| Spec | Valor |
|---|---|
| Tipo | Tactile low-profile, PCB mount |
| Travel | 3.0mm |
| Actuation | 45gf |
| LED | RGB SMD en posición de backlight (Choc V2 compatible) |
| Keycap | PBT translúcido, 1U (18.6×17.6mm), serigrafía laser |
| Pitch en panel | 28mm centro a centro (B1↔B2 y B3↔B4) |
| Color LED | Teal (#00C9B1) para modo normal, blanco para AI |

### Volumen — Alps RK097N

Ver §8 para specs completos del RK097N.

Knob: Ø12mm, altura 10mm, estilo "chicken head" discreto o cylindrical. El VOL queda en la esquina inferior izquierda del panel — accesible con el pulgar de la mano izquierda sin requerir atención visual.

---

## 8. Potenciómetro de Volumen — Alpha RV09 (recomendado BOM)

**Status: PROPUESTO — mismo footprint 9mm, D-shaft disponible, $0.25/unit**

El control de volumen no requiere calidad de parámetro de síntesis — es un nivel de salida que el usuario toca una vez al inicio del set. Un pot de gama media es suficiente. El **Alpha RV09** es el estándar del mundo Eurorack DIY (Mutable Instruments, Befaco, cientos de módulos open-source) y tiene el mismo footprint 9mm que el RK097N a un tercio del precio.

### Specs Alpha RV09

| Parámetro | Valor | Notas |
|---|---|---|
| Body footprint | 9×9mm | Igual que RK097N |
| Altura desde PCB (body) | ~13mm | |
| Shaft diámetro | 6mm | **D-shaft disponible** — pedir variante "D" al ordenar |
| Shaft largo | 15mm (estándar) | |
| Rotación total | 300° | |
| Cutout panel | **Ø7mm** | 6mm shaft + 0.5mm clearance/lado |
| Taper | A (audio/log) y B (lineal) | Usar **A** para volumen |
| Pin layout | 3 pines en fila, pitch 5mm | Compatible con footprint RK097N |
| Vida útil | ~10,000 ciclos | Suficiente para uso de instrumento |
| Precio unitario | **~$0.20–0.40 USD** | LCSC, AliExpress |

### Jerarquía de opciones (de más a menos económico)

| Parte | Precio/unit | D-shaft | Calidad | Cuándo usar |
|---|---|---|---|---|
| Generic 9mm (WH9011/B9) | **$0.08–0.15** | Algunas vars. | Básica | Producción alto volumen, qty >500 |
| **Alpha RV09** ← recomendado | **$0.20–0.40** | Sí | Buena | Prototipo y producción v1.0 |
| Bourns PTV09A | $0.35–0.55 | Sí | Muy buena | Si el RV09 no está disponible |
| Alps RK097N | $0.80–1.50 | No | Premium | Descartado — overkill para volumen |

### Dónde conseguir en Colombia

- **LCSC** (lcsc.com): envío DHL a Bogotá, 5-7 días hábiles, mínimo de pedido ~$20 USD
- **AliExpress** sellers: más barato, 15-30 días, riesgo de calidad variable — aceptable para prototipo
- **Mercado Libre Colombia**: buscar "potenciometro 9mm audio B10K" — calidad varía, pedir muestra antes de qty

Al ordenar especificar: **Alpha RV09 Series, shaft D tipo, taper A (audio), 10kΩ** (el valor exacto de resistencia lo define el hardware engineer según el circuito de nivel de salida del SGTL5000).

---

## 9. Especificaciones de Cutouts

**Status: PROPUESTO v0.3**

### Top Panel (aluminio CNC 2mm)

| Componente | Forma | Dimensión nominal | Tolerancia | Notas |
|---|---|---|---|---|
| Display GC9A01 | Circular | Ø35.0mm | ±0.1mm | Bezel oculta el gap |
| ENC1 / ENC2 shaft | Circular | Ø6.5mm | ±0.05mm | D-shaft Ø6mm |
| ENC NAV shaft | Circular | Ø6.5mm | ±0.05mm | |
| LED ring (ventana visual) | Anular | ext Ø38mm, int Ø28mm | ±0.1mm | Ajustar según resolución D-08 |
| B1 / B2 / B3 / B4 | Cuadrado | 14.0×14.0mm | ±0.1mm | Keycap Choc V2 pasa por aquí |
| VOL shaft (Alpha RV09) | Circular | Ø7.0mm | ±0.05mm | Shaft 6mm D-type + 0.5mm clearance |

### Panel Lateral Izquierdo (PETG + Guadua alineados)

| Componente | Forma | Dimensión PETG | Dimensión Guadua |
|---|---|---|---|
| 1/4" TRS IN L | Circular | Ø10.5mm ±0.3 | Ø10.5mm ±0.5 |
| 1/4" TRS IN R | Circular | Ø10.5mm ±0.3 | Ø10.5mm ±0.5 |
| 3.5mm AUX IN | Circular | Ø7.0mm ±0.3 | Ø7.0mm ±0.5 |

### Panel Lateral Derecho (PETG + Guadua alineados) — v0.4

| Componente | Forma | Dim. PETG | Dim. Guadua | Z desde fondo |
|---|---|---|---|---|
| USB-C (Power + Audio/MIDI) | Rectangular | 10.5×5.0mm ±0.3 | 11.0×5.5mm ±0.5 | 15mm |
| 1/4" TRS OUT L | Circular | Ø10.5mm ±0.3 | Ø11.0mm ±0.5 | 38mm |
| 1/4" TRS OUT R | Circular | Ø10.5mm ±0.3 | Ø11.0mm ±0.5 | 60mm |
| USB-C Host | Rectangular | 10.5×5.0mm ±0.3 | 11.0×5.5mm ±0.5 | 82mm |

**Nota implementacion:** El PETG body tiene los cutouts en la pared interna. El pigtail
micro-USB→USB-C del Teensy se enruta al cutout de 15mm. El cable USB Host del Teensy
(5V, D+, D-, GND) se enruta al cutout de 82mm. Ambos cutouts son identicos (misma forma, misma herramienta).

---

## 10. Estrategia de Ensamblaje v0.3

**Status: PROPUESTO**

### Orden de ensamblaje

```
1.  Imprimir PETG body (180×100mm, perfil 30→42mm)
2.  Instalar heat inserts M3: 6× PCB standoff + 4× aluminio + 4× cheeks (2 por lado)
3.  Montar PCB principal con standoffs M3×5
4.  Instalar ESP32-S3 module sobre bracket display (elevado)
5.  Preparar cheeks guadua: laminar → cortar → lijar → sellar (48h secado)
6.  Hacer cutouts en PETG walls laterales (jacks + USB)
7.  Encajar cheeks en rebaje PETG + Araldite + tornillos M3 ocultos
8.  Hacer cutouts en guadua (guiados por PETG, taladro pasante)
9.  Lijar bordes cutouts guadua (lija P400 enrollada)
10. Montar jacks y conectores en paneles laterales
11. Atornillar top panel aluminio (4× M3 countersunk en esquinas)
12. Instalar knobs y keycaps
13. Pegar patas antideslizantes en base (4× silicona Ø10mm)
```

### Fijaciones — resumen

| Tipo | Componentes | Cant. | Comentario |
|---|---|---|---|
| Heat inserts M3 latón | PCB + aluminio + cheeks | 14 total | 6+4+2+2 |
| Tornillos M3×8 SS countersunk | PCB a PETG | 6 | Desde arriba |
| Tornillos M3×6 SS countersunk | Aluminio a PETG | 4 | Visibles, estéticos |
| Tornillos M3×16 SS | Cheeks a PETG (ocultos) | 4 | 2 por cheek |
| Araldite estructural | Cheeks a PETG | — | Adhesivo primario |
| Patas silicona Ø10mm | Base | 4 | Antideslizante + eleva base 3mm |

---

## 11. Roadmap de Hardware por Versión

**Status: PROPUESTO — referencia para tomar decisiones de PCB**

| Versión | Procesador | Power | Device size | Prioridad |
|---|---|---|---|---|
| **v0.1 proto** | Teensy 4.1 dev board | USB-C only | 220×130mm (placeholder) | Firmware Sprint 31+ |
| **v0.2 proto** | Teensy 4.1 dev board | USB-C only | **204×100mm** | Form factor validation |
| **v1.0 release** | iMXRT1062 bare + SGTL5000 en PCB custom | USB-C only | **204×100mm** (mismo enclosure) | Producción $599 |
| **v1.5 Pro** | iMXRT1062 bare | USB-C + LiPo 2500mAh | 204×100mm + compartimento batería en base | Premium tier |

**Nota v1.0 — iMXRT1062 bare:** el chip del Teensy 4.1 (NXP iMXRT1062) existe en paquete LQFP-100 (no BGA), soldable con hot-air a nivel amateur-avanzado. Requiere FLASH externa (W25Q64 o equiv, 8MB), oscilador 16MHz, y decoupling cuidadoso. Proyectos de referencia: Polyend Tracker (mismo procesador), OpenDeck controller. La migración de Teensy 4.1 a iMXRT1062 bare ahorra ~15mm de longitud y $15 de costo por unidad.

---

## 12. Plan de Modelado 3D (Fusion 360) v0.3

**Status: PENDIENTE**

### User Parameters

| Parámetro | Valor | Descripción |
|---|---|---|
| `PCB_W` | 168mm | Ancho PCB main (body 180 - 2×3mm wall - 2×3mm clearance) |
| `PCB_D` | 88mm | Profundidad PCB main |
| `WALL_T` | 3.0mm | Espesor pared PETG |
| `PANEL_T` | 2.0mm | Espesor top panel aluminio |
| `CHEEK_T` | 12.0mm | Espesor cheeks guadua |
| `BODY_W` | 180.0mm | Ancho PETG body central |
| `BODY_D` | 100.0mm | Profundidad PETG body |
| `BODY_H_F` | 42.0mm | Altura frontal |
| `BODY_H_R` | 30.0mm | Altura trasera |
| `TOTAL_W` | 204.0mm | Ancho total con cheeks |
| `ENC_HOLE` | 6.5mm | Cutout shafts encoder |
| `BTN_CUT` | 14.0mm | Cutout cuadrado botones Kailh |
| `DISPLAY_CUT` | 35.0mm | Cutout circular display |
| `VOL_HOLE` | 7.0mm | Cutout shaft RK097N |
| `JACK_HOLE` | 10.5mm | Cutout jacks 1/4" TRS |
| `USBC_W` | 10.5mm | Cutout USB-C ancho |
| `USBC_H` | 5.0mm | Cutout USB-C alto |
| `USBA_W` | 15.0mm | Cutout USB-A ancho |
| `USBA_H` | 8.0mm | Cutout USB-A alto |
| `STANDOFF_H` | 5.0mm | Altura standoffs PCB |

### Orden de modelado

1. **Sketch "Side Profile XZ"** en plano YZ: rampa 30→42mm sobre 100mm de profundidad (tilt ~7°)
2. **Extrude** → **Shell** 3mm → cuerpo PETG base
3. **Rebaje lateral** 12mm en X±: canal para encastre de cheeks
4. **Pedestales heat inserts**: 6 PCB, 4 aluminio, 4 cheeks
5. **Sketch "Top Panel"**: todos los cutouts con coordenadas de §3
6. **Sketch "Left Panel"**: cutouts jacks IN con coordenadas de §4
7. **Sketch "Right Panel"**: cutouts jacks OUT + USB con coordenadas de §5
8. **Sketch "Rear Panel"**: cutout pogo centrado
9. **Cheek bodies**: sólidos guadua 12×100×(perfil variable)
10. **Assembly**: verificar interferencias, especialmente LED ring vs display

---

## 13. Plan de Primera Iteración de Impresión

**Status: PENDIENTE**

### Fase 1 — Top panel PLA (2-3h)

Imprimir top panel en PLA 1:1 (180×100×2mm) con todos los cutouts. Colocar componentes reales encima y verificar:
- EC11 pasan por Ø6.5mm holes
- Keycaps Choc V2 pasan por 14×14mm con clearance
- Módulo Waveshare (display) cabe en Ø35mm
- RK097N shaft pasa por Ø7mm
- Spacing entre knobs correcto para operar simultáneamente
- ENC NAV debajo del display se siente natural

### Fase 2 — Junta lateral PLA (4-6h)

Imprimir una sección lateral del PETG (con rebaje) + bloque PLA "simulando guadua" (12mm, con cutouts de jacks). Verificar alineación de cutouts y fit mecánico del cheek.

### Fase 3 — PETG body completo (18-22h)

Cuerpo PETG completo sin cheeks. Montar PCB, verificar standoffs y clearances internos.

### Fase 4 — Cheeks guadua reales

Primer prototipo completo con guadua laminada real.

### Print settings PETG

| Setting | Valor |
|---|---|
| Layer height | 0.2mm |
| Infill | 40% gyroid |
| Perimeters | 4 |
| Nozzle temp | 240°C |
| Bed temp | 80°C |
| Speed exterior | 40mm/s |
| Brim | 8mm |

---

## 14. Decisiones Abiertas

| # | Decisión | Opciones | Bloqueado por |
|---|---|---|---|
| D-01 | Tilt 7° vs perfil plano | 7° mejor ergonomía, plano más fácil de apilar | Validación con maqueta |
| D-02 | Cheek thickness: 12mm vs 15mm | 12mm más compacto, 15mm más visible | Preferencia estética |
| D-03 | Junta aluminio/guadua | Gap limpio vs bezel de aluminio 1mm | Decisión estética |
| D-04 | Guadua artesanal vs MOSO laminado | MOSO fácil, guadua más identitaria | Disponibilidad material |
| D-05 | RK097N shaft: liso vs D-shaft | D-shaft = knob más seguro, liso = más opciones de knob | Confirmar al ordenar |
| D-06 | RK097N: taper A (audio) vs B (lineal) | A recomendado para volumen | Preferencia perceptual |
| D-07 | Etiquetado guadua | Laser engraving vs inciso+paint vs sticker | Costo vs calidad proto |
| D-08 | LED ring vs display clearance | Reducir ring a Ø32mm ext, o bajar ENC NAV a Y=20 | Verificar con modelo 3D |
| D-09 | 4 botones — distribución | 2+2 separados por display (propuesto) vs 4 seguidos | Preferencia estética |
| D-10 | USB-A en cheek RIGHT: Y=97 muy cerca del borde | Mover a Y=90 o aceptar | Verificar con modelo 3D |

---

## 15. Prompt Referencia Visual (AI Image Generation)

```
Boutique hardware synthesizer top-down view, dark anthracite aluminum panel 180mm wide,
natural guadua bamboo cheek side panels (Moog Subsequent style), four backlit rectangular
mechanical keyboard buttons top row split in two groups with gap for display, large circular
1.28 inch round display center with stacked ESP32 module above it, two medium aluminum knobs
flanking the display (left: cutoff, right: resonance), one larger aluminum knob below the
display with green LED ring (action encoder), small discrete volume knob bottom left corner,
product photography studio lighting, straight top-down view, Moog Subsequent aesthetic with
Latin American craft, Bogotá Colombia, premium compact synthesizer 204mm total width
```

---

## Historial de revisiones

| Versión | Fecha | Cambios |
|---|---|---|
| v0.1 | 2026-05-26 | Draft inicial — guadua franja frontal, 220×130mm |
| v0.2 | 2026-05-26 | Guadua como cheeks laterales Moog Subsequent, LEFT=IN RIGHT=OUT, 244×130mm |
| v0.3 | 2026-05-26 | Sin batería (USB-C only), device 204×100mm, perfil 42/30mm, ENC NAV debajo display, 4 botones retroiluminados fila superior, Alps RK097N para VOL, ESP32 elevado sobre display |
| v0.4 | 2026-05-26 | Cheek RIGHT: 5 puertos → 4 puertos. USB-C Power + USB-C Teensy consolidados en un solo USB-C (pigtail micro-USB→USB-C en prototipo). USB-A Host → USB-C Host. D-10 resuelto (clearance 12.75mm). |
