# Part 99 — Assembly (Ensamblaje Completo)

> **Autor:** Industrial Design Agent
> **Version:** 0.1
> **Status:** PROPUESTO
> **Derivado de:** `apps/design/enclosure/01-enclosure-design-spec.md` v0.3, §10
> **Archivo CAD target:** `apps/design/3d-models/99-assembly.f3d`

---

## 0. Jerarquia de ensamblaje

```
99-assembly.f3d
│
├── 01_PETG_Body         <- pieza madre, referencia de todos los demas
│   ├── Heat inserts M3 (14 total: 6 PCB + 4 aluminio + 4 cheeks)
│   └── Patas silicona x4 (base)
│
├── 02_Top_Panel_Al      <- asienta sobre 01, 4 tornillos M3×6 CS
│   └── 05_Display_Bracket  <- asienta sobre 02 (pie en cutout display)
│       └── ESP32-S3 module  <- inserto en cavidad del bracket
│
├── 03_Cheek_Left        <- encaja en rebaje izquierdo de 01
│   ├── Jack TRS IN L
│   ├── Jack TRS IN R
│   └── Jack 3.5mm AUX IN
│
├── 04_Cheek_Right       <- encaja en rebaje derecho de 01
│   ├── USB-C Power (conector)
│   ├── Jack TRS OUT L
│   ├── Jack TRS OUT R
│   ├── USB-C Teensy (conector)
│   └── USB-A Host (conector)
│
└── PCB_Main             <- atornillada a standoffs de 01
    ├── Teensy 4.1
    ├── SGTL5000
    ├── Alps RV09 VOL (conecta a top panel via shaft)
    ├── EC11 ENC1/ENC2/ENCNAV (conectan a top panel via shaft)
    ├── Kailh Choc V2 x4 (conectan a top panel via keycap)
    └── LED ring ENC NAV (concentric al ENCNAV, visible por cutout anular)
```

---

## 1. Orden completo de montaje

Basado en §10 del master spec, expandido con pasos intermedios y criterios de go/no-go.

### Fase 0 — Preparacion de materiales (antes de ensamblar)

```
0.1  Imprimir PETG body (18-22h). Verificar todas las dimensiones criticas (§6 de 01-petg-body.md).
0.2  Imprimir display bracket en PETG (2-3h). Verificar fit con modulo ESP32-S3.
0.3  Cortar cheeks guadua (sierra de cinta): perfil 30→42mm × 100mm × 12mm.
0.4  Lijar cheeks: P120 → P240 → P400 en todas las caras.
0.5  Sellar cheeks con epoxico: aplicar 2 manos en todas las caras, incluyendo los cortes
     (especialmente los cortes de jacks que se haran despues). Dejar secar 48h.
0.6  Mandar a CNC el top panel de aluminio (mientras seca la guadua).
     Enviar archivo 02-top-panel.step + instrucciones del proveedor (ver §8 de 02-top-panel-aluminum.md).
0.7  Al recibir el aluminio CNC: verificar todas las dimensiones criticas (§6 de 02-top-panel-aluminum.md).
```

### Fase 1 — Instalacion de heat inserts en PETG body

```
1.1  Herramienta: soldador de temperatura regulada con punta para heat inserts (230-250°C).
1.2  Instalar 4 heat inserts M3 en pedestales del top panel (esquinas superiores del body).
     Insertar calentado lentamente hasta que queden flush con la superficie o 0.2mm bajo ella.
     Verificar: tornillo M3 debe entrar sin resistencia excesiva y sin wobble.
1.3  Instalar 6 heat inserts M3 en standoffs de PCB (suelo interno).
     Verificar: idem.
1.4  Instalar 2 heat inserts M3 en pared lateral izquierda (para cheek).
     ATENCION: los inserts en pared lateral van en orientacion horizontal — el soldador debe
     trabajar horizontalmente. Usar un soporte o hacer el body boca abajo.
1.5  Instalar 2 heat inserts M3 en pared lateral derecha.
1.6  Enfriar 30 minutos antes de continuar.
1.7  Verificar los 14 inserts: roscar y desroscar un tornillo en cada uno. Todos deben agarrar.
```

### Fase 2 — Montaje de PCB

```
2.1  Instalar componentes en la PCB si no estan instalados (encoders, botones, jacks laterales,
     potenciometro VOL — todos los componentes de panel van en la PCB antes de montarla).
2.2  Posicionar PCB sobre los 6 standoffs del PETG. Verificar que los shafts de encoders y
     el shaft del VOL alinean aproximadamente con las posiciones de cutout del top panel.
2.3  Atornillar PCB con 6 tornillos M3×8 SS. Apretar en patrón cruzado.
     Torque de apriete: aproximadamente 0.3 N·m (mano + un cuarto de vuelta con herramienta).
2.4  Conectar cables de sub-sistemas (si hay separacion en la PCB).
2.5  Conectar cable del modulo ESP32-S3 (dejar suelto — se monta despues del bracket).
```

### Fase 3 — Preparacion y montaje de cheeks guadua

```
3.1  ANTES de pegar: ensamblar los cheeks en los rebajes del PETG en seco (sin adhesivo).
     Verificar que asientan flush contra el body sin gaps visibles.
     Verificar que los heat inserts de los cheeks quedan alineados con los de las paredes del PETG.
3.2  Marcar la posicion de los cutouts de jacks usando un marcador a traves de los cutouts
     del PETG (los cutouts del PETG son la guia de taladrado).
3.3  Retirar los cheeks. Taladrar los cutouts en la guadua (usando las marcas de posicion):
     - TRS IN L, IN R, OUT L, OUT R: broca Forstner Ø11mm (o sierra copa)
     - 3.5mm AUX IN: broca comun Ø7.5mm
     - USB-C Power, USB-C Teensy: taladro multiple + lima (ver nota en 04-cheek-right-guadua.md §1)
     - USB-A Host: idem, con dimension 15.5mm × 8.5mm
3.4  Lijar el borde interior de los cutouts de guadua: lija P400 enrollada.
3.5  Aplicar Araldite estructural en el canal del rebaje del PETG (solo el fondo y los laterales
     del canal — NO en los bordes visibles donde el exceso de adhesivo mancharia el exterior).
3.6  Insertar el cheek izquierdo en el rebaje, presionando hasta asentar.
3.7  Atornillar 2 tornillos M3×16 desde el interior del PETG hacia los heat inserts del cheek.
     Apretar a mano primero, luego con llave hasta que el cheek este firme (sin sobreapriete —
     la guadua puede astillarse con exceso de torque).
3.8  Repetir 3.5-3.7 para el cheek derecho.
3.9  Dejar curar el Araldite: 24h minimo a temperatura ambiente (20-25°C Bogota).
     NO montar el top panel hasta que el adhesivo haya curado.
3.10 Instalar los jacks y conectores en los paneles laterales a traves de los cutouts
     (tuercas y arandelas en la cara exterior de la guadua).
```

### Fase 4 — Montaje del display bracket y modulo ESP32-S3

```
4.1  Pasar el cable del modulo ESP32-S3 por la ranura lateral del bracket antes de insertar
     el modulo en el bracket.
4.2  Insertar el modulo ESP32-S3 en la cavidad del bracket. Fijar con double-sided tape VHB
     en la base de la cavidad (bajo el modulo, no visible).
4.3  Conectar el cable del ESP32-S3 al conector correspondiente en la PCB.
4.4  Insertar el pie del bracket (Ø32mm) a traves del cutout del aluminio (Ø35mm).
     Centrar visualmente el display en el cutout.
     Fijar el flange del bracket a la cara superior del aluminio con double-sided tape VHB
     de alta resistencia (Ø de 35mm de area de contacto en el flange).
     ATENCION: el VHB cura por presion — aplicar presion uniforme durante 5 minutos.
     Una vez pegado, no es repositionable sin destruir el tape.
     ALTERNATIVA para prototipo: usar tape de doble cara standard (repositionable) hasta
     validar la posicion final del bracket.
```

### Fase 5 — Montaje del top panel de aluminio

```
5.1  Posicionar el top panel sobre el PETG body.
     Los 4 holes de tornillo del aluminio deben alinear con los 4 heat inserts del body.
     Los shafts de los encoders (ENC1, ENC2, ENC NAV) deben pasar por sus holes.
     El shaft del VOL debe pasar por su hole.
     Los keycaps de los 4 botones Choc V2 deben pasar por los cutouts de 14×14mm.
     Si el bracket ya esta montado: el bracket con el modulo ESP32-S3 sobresale del panel.
     RECOMENDACION: montar el bracket ANTES de colocar el top panel (ver orden de Fase 4 y 5).
     Alternativa: el pie del bracket se pasa por el cutout desde arriba del panel antes de pegar.
5.2  Verificar que el panel asienta flush sobre el PETG body en todos los bordes.
5.3  Atornillar 4 tornillos M3×6 SS countersunk en las esquinas.
     Los tornillos deben quedar flush o levemente bajo la superficie del aluminio (countersunk).
     Apretar en patron diagonal. Torque: 0.3-0.4 N·m (sin sobre-apretar en aluminio de 2mm).
5.4  Verificar que el panel no tiene gaps visibles con el PETG en ningún borde.
```

### Fase 6 — Instalacion de knobs y keycaps

```
6.1  Instalar knobs en los tres encoders:
     - ENC1 (CUTOFF): knob Ø18mm knurled, bajo perfil
     - ENC2 (RESONANCE): knob Ø18mm knurled, bajo perfil
     - ENC NAV (ACTION): knob Ø22mm liso, perfil alto
     Los knobs con D-shaft se aprietan con tornillo set (Allen 1.5mm generalmente).
     Posicion del set screw: alinear con el plano D del shaft para maximo agarre.
6.2  Instalar knob del VOL: Ø12mm "chicken head" o cylindrical.
6.3  Instalar keycaps en los 4 Kailh Choc V2.
6.4  Encender el instrumento y verificar que los LEDs de los keycaps funcionan.
6.5  Verificar el funcionamiento de los encoders (sin skip ni puntos muertos).
```

### Fase 7 — Patas silicona y cierre

```
7.1  Presionar las 4 patas de silicona Ø10mm en las cavidades de la base del PETG.
     Las patas deben quedar press-fit (Ø9.9mm de cavidad vs Ø10mm de pata).
     Si no hay presion suficiente: gotita de cianocrilato en la cavidad antes de insertar.
7.2  Test final:
     - El instrumento no se mueve sobre la mesa (patas hacen su funcion)
     - La altura frontal del instrumento con patas: BODY_H_F + PATA_H = 42+3 = 45mm
     - Todos los conectores accesibles desde afuera sin obstrucion
     - Todos los controles operables sin interferencia entre si
7.3  Documentar el numero de serie del prototipo (tira de papel bajo una de las patas
     o etiqueta interior en el compartimento).
```

---

## 2. Tabla de fijaciones consolidada

| Tipo | Cant. | Donde | Torque aprox. | Notas |
|------|-------|-------|---------------|-------|
| Heat insert M3 latón | 14 | Body PETG: 6 PCB standoffs + 4 top panel pedestals + 2 cheek izq + 2 cheek der | — (calor) | 230-250°C; flush o 0.2mm bajo la superficie |
| Tornillo M3×8 SS (PCB) | 6 | PCB principal a standoffs PETG | 0.3 N·m | Patron cruzado |
| Tornillo M3×6 SS countersunk (top panel) | 4 | Top panel Al a body PETG | 0.3-0.4 N·m | Flush con superficie aluminio |
| Tornillo M3×16 SS (cheeks) | 4 | Cheeks guadua a body PETG (2 por cheek) | 0.2-0.3 N·m | No sobrecargar: astilla la guadua |
| Araldite estructural | — | Canal del rebaje PETG para cada cheek | — | 24h cura; no reposicionable |
| Double-sided tape VHB | — | Flange del bracket sobre top panel | 5 min presion | Asegurarse de centrar antes de aplicar |
| Double-sided tape VHB | — | Modulo ESP32-S3 en cavidad del bracket | — | |
| Patas silicona Ø10mm | 4 | Cavidades base PETG | — | Press-fit; gotica cianocrilato si flojo |
| Tuercas jacks TRS/3.5mm | 6 | Jacks en cheeks guadua (3 izq + 3 der en audio) | Mano | No sobrecargar |
| Tornillos set knobs | 3-4 | Knobs en shafts de encoders y VOL | Mano + 1/8 vuelta | Allen 1.5mm o 2mm segun knob |

---

## 3. Tolerancias de ensamblaje (gap entre piezas)

| Interfaz | Gap objetivo | Tolerancia | Notas |
|----------|-------------|------------|-------|
| Aluminio top panel ↔ borde superior PETG | 0 mm (flush) o 0.1mm levemente elevado | ±0.2mm | Si el aluminio sobresale mas de 0.3mm, se siente al pasar el dedo |
| Cheek guadua ↔ PETG body (borde visible) | 0 mm (flush) con el rebaje de 6mm | ±0.3mm FDM | El adhesivo Araldite rellena gaps de hasta 0.5mm |
| Aluminio top panel ↔ cheek guadua (junta lateral) | 0.5-1.0 mm de gap visible | ±0.3mm | Gap intencional — feature de diseno. Si queda mayor a 1.5mm, se ve descuidado. |
| Pie del bracket ↔ cutout display aluminio | 1.5 mm por lado (clearance 3mm total) | ±0.3mm | El bracket se centra visualmente antes de fijar con VHB |
| Shaft encoder ↔ hole aluminio (Ø6.5mm vs Ø6mm shaft) | 0.25mm por lado | ±0.05mm (CNC) | El shaft debe girar libremente; si hay wobble, el CNC fue impreciso |
| Keycap Choc V2 ↔ cutout 14×14mm | 0.0-0.3mm por lado | ±0.1mm (CNC) | El keycap debe moverse verticalmente sin rozar el aluminio |

---

## 4. Issues conocidos y resoluciones

### D-08: LED ring vs display clearance

**Problema original (master spec v0.3):** Display en Y=57mm, ENC NAV en Y=30mm. Gap entre borde
inferior display y borde superior ring: 57-17.5-30-19 = -9.5mm (interferencia).

**Resolucion adoptada en los specs modulares:**
- Display movido a Y=60mm (de 57 a 60)
- ENC NAV movido a Y=22mm (de 30 a 22)
- Ring reducido a Ø32mm exterior (de Ø38mm)

**Verificacion post-resolucion:**
- Gap borde inferior display ↔ borde superior ring: 60-17.5-22-16 = 4.5mm. OK (>3mm minimo).
- El ENC NAV a Y=22mm esta a 22mm del borde trasero del panel. Con el borde del panel en Y=0
  y el rebaje del pogo trasero a 15mm de la base, no hay conflicto mecanico.
- El knob Ø22mm del ENC NAV a Y=22mm: borde trasero del knob = 22-11=11mm del borde trasero
  del panel. Clearance marginal pero funcional — la mano del usuario no necesita espacio detras del knob.

**Parametros actualizados en 00-master-parameters.md:**
- DISPLAY_Y: 60mm (era 57mm)
- ENCNAV_Y: 22mm (era 30mm)
- RING_EXT_D: 32mm (era 38mm)
- RING_INT_D: 24mm (era 28mm, ajustado proporcionalmente)

**Status: RESUELTO en los specs modulares. Pendiente validacion con modelo 3D.**

---

### D-10: USB-A Host muy cerca del borde frontal del cheek

**Problema original (master spec v0.3):** USB-A en Y=97mm (en coordenadas del cheek = Z=97mm
desde el fondo). Borde anterior del cutout: 97+7.5=104.5mm > 100mm. IMPOSIBLE.

**Primera iteracion de resolucion:** mover a Z=92mm → borde ant = 99.5mm → pared frontal = 0.5mm.
INSUFICIENTE para guadua.

**Analisis de la causa raiz:**
Los 5 puertos del cheek derecho necesitan mas espacio del disponible en 100mm si se requieren
clearances minimos de 3mm entre cutouts.

Espacio total necesario (calculado en 04-cheek-right-guadua.md §3):
- USB-C Power (10.5mm ancho, en Z=15mm): ocupa Z=9.75 a Z=20.25mm
- Gap minimo 3mm → siguiente empieza en Z=23.25mm
- TRS OUT L (Ø10.5mm, en Z=38mm): empieza en Z=32.75mm — gap=32.75-20.25=12.5mm OK
- TRS OUT R (Ø10.5mm, en Z=60mm): gap OK
- USB-C Teensy (10.5mm ancho, en Z=80mm): borde ant en Z=85.25mm
- Gap minimo 3mm → USB-A empieza en Z=88.25mm. Centro USB-A: 88.25+7.5=95.75mm.
  Borde anterior: 95.75+7.5=103.25mm. EXCEDE el largo de 100mm.

**Opciones arquitecturales:**

| Opcion | Descripcion | Pros | Contras |
|--------|-------------|------|---------|
| A | Reducir ancho USB-A de 15mm a 12mm | Cabe en Z=90mm | USB-A fisico mide ~12×5mm (tipo A): factible si el conector es fino |
| B | Mover USB-C Power al panel trasero PETG junto con el pogo | Libera 17mm en el cheek derecho | Rompe la convencion LEFT=IN, RIGHT=OUT. USB-C poder en el trasero es estandar en muchos sintetizadores. |
| C | Mover USB-C Teensy al panel trasero | Idem | El USB-C de Teensy es el de uso frecuente (flashing) — en el trasero es menos accesible |
| D | Usar cheeks de 120mm de profundidad en vez de 100mm | Todo cabe con clearances | Rompe la dimension BODY_D=100mm y cambia el total |
| E | Eliminar USB-C Power (alimentar solo por USB-C Teensy) | 5 puertos → 4 puertos, todo cabe | La dualidad power/data en el mismo conector puede complicar el circuito de power |

**Recomendacion:**
Opcion B (USB-C Power al trasero). Justificacion: en la mayoria de sintetizadores de estudio,
el power va en el panel trasero (Moog Sub25, Roland SE-02, Arturia MiniLab). El usuario conecta
el power una vez y lo deja. El frente derecho queda para los conectores de uso frecuente.

Con Opcion B, el cheek derecho tiene 4 puertos:
- TRS OUT L: Z=30mm
- TRS OUT R: Z=52mm
- USB-C Teensy: Z=68mm
- USB-A Host: Z=83mm → borde ant = 90.5mm → clearance frontal = 9.5mm. OK.

El panel trasero PETG tendria: Pogo (centro, X=90) + USB-C Power (X=30 o X=150).

**Status: PENDIENTE decision del hardware engineer (afecta circuito de power y pin mapping).**
Los specs modulares documentan la version con los 5 puertos como "provisional D-10" hasta
que el hardware engineer confirme si Opcion B es viable electricamente.

---

### Gap identificado: base del PETG body (abierta o con tapa)

El spec maestro v0.3 no especifica si la base del PETG body tiene tapa removible o es
permanentemente cerrada (solo con patas de silicona).

Con la base cerrada (solida, sin apertura), el acceso a la electronica interna es imposible
sin desatornillar el top panel. Esto es aceptable para un instrumento de produccion (como el OP-1)
pero dificil para el prototipo donde el desarrollo firmware requiere acceso frecuente al interior.

**Recomendacion para prototipo v0.2:** base abierta (base del PETG es solo el suelo con los
standoffs — sin tapa). La tension y el LiPo (en versiones futuras) quedan accesibles desde abajo.
Las patas de silicona elevan el instrumento 3mm (PATA_H) para que nada toque la mesa.

**Para produccion v1.0:** evaluar tapa de base con clips o tornillos — o mantener la base
abierta si el instrumento se usa siempre sobre mesa.

---

## 5. Procedimiento de troubleshooting si una pieza no encaja

### Top panel no asienta flush sobre PETG

1. Verificar que los 4 pedestales de heat inserts esten a la misma altura (calibre de profundidad)
2. Verificar que no hay material PETG sobrante en la zona de asiento del panel (lija P400)
3. Verificar que los knobs y shafts de encoders no esten obstruyendo (los shafts deben pasar por los holes antes de bajar el panel)
4. Verificar que los keycaps no esten bloqueando el panel (presionar los botones hacia abajo mientras se asienta el panel)

### Cheek no asienta en el rebaje PETG

1. Medir el ancho del rebaje con calibre (debe ser CHEEK_REBAJE_D=6mm ±0.3mm)
2. Medir el espesor del borde del cheek que entra en el rebaje (debe ser ≤ 6mm)
3. Si el cheek FDM del PETG tiene undersize: lijar el fondo del canal con lija envuelta en un bloque
4. Si la guadua tiene oversize: lijar la cara interior del cheek (la que entra en el canal)
5. El clearance de 0.2mm es suficiente para insertar sin fuerza excesiva — si requiere golpes, hay interferencia

### Shaft de encoder no pasa por el hole del aluminio

1. Verificar el diametro del hole (debe ser ENC_HOLE=6.5mm)
2. Verificar que el shaft EC11 es D-shaft de 6mm (algunos EC11 tienen shaft redondo de 6.35mm)
3. Si el hole es correcto (6.5mm) y el shaft D no pasa: el anodizado dejo una capa gruesa. Ampliar el hole 0.1mm con una lima de aguja circular.
4. Si el shaft redondo de 6.35mm no pasa por 6.5mm: el CNC fue impreciso. Pedir re-trabajo.

### Display bracket no queda centrado en el cutout

1. El bracket tiene 1.5mm de clearance por lado (Ø32mm vs Ø35mm) — puede desplazarse.
2. Antes de pegar el VHB: centrar el display mirando desde arriba que el gap sea uniforme en todo el perimetro.
3. Una vez centrado, aplicar presion uniforme durante 5 minutos para que el VHB agarre.
4. Si el bracket queda off-center >1mm visiblemente: el VHB no curo aun (primeras horas) — usar espatula de plastico para levantar y reposicionar.

### LED ring no es visible a traves del cutout anular

1. Verificar que el cutout anular (Ø32mm ext / Ø24mm int) fue cortado correctamente por el CNC.
2. Verificar que la PCB del ring esta a la altura correcta debajo del aluminio (la altura de los LEDs debe ser menor que PANEL_T=2mm del aluminio).
3. Verificar que los LEDs del ring estan encendidos (firmware).
4. La ventana anular de 4mm de ancho (radio ext - radio int = 16-12=4mm) es pequeña — el efecto visual es una franja de luz circular. Si el grosor del ring no es suficiente para ver los LEDs, puede necesitarse ampliar el cutout exterior (aumentar RING_EXT_D).

---

## 6. Verificacion de ensamblaje completo

| Check | Metodo | Criterio pass |
|-------|--------|---------------|
| Dimension total 204×100mm | Calibre exterior (incluyendo cheeks) | 204mm ±1mm en X, 100mm ±0.5mm en Z |
| Altura frontal con patas | Calibre desde mesa hasta top del aluminio frontal | 42+3+2=47mm ±1mm |
| Planitud del top panel | Regla de metal | Sin deflexion perceptible al aplicar fuerza de 10N en el centro |
| Torque de encoders | Girar cada encoder | Giro suave sin puntos duros; click del switch audible |
| Actuation de botones | Presionar cada boton Choc V2 | Click audible y tactil; el keycap regresa completamente |
| Iluminacion keycaps | Encender firmware | Todos los keycaps iluminados uniformemente en teal |
| LED ring ENC NAV | Encender firmware | Ring visible y uniforme a traves del cutout anular |
| Display GC9A01 | Encender firmware | Display visible y legible desde angulo de uso (frente inclinado ~7 grados) |
| Audio OUT | Conectar 1/4" TRS a amplificador | Señal sin ruido de fondo excesivo; sin crackles de conexion mecanica deficiente |
| USB-C Teensy | Conectar cable USB-C | Conexion fisica firme; el host reconoce el dispositivo |
| USB-A Host | Conectar teclado MIDI USB | El host reconoce el dispositivo MIDI |
| Pogo connector | Conectar accesorio modular (cuando disponible) | 6 pins conectan sin fuerza excesiva |
| Rigidez mecanica general | Aplicar fuerza de 20N en 4 esquinas del instrumento | Sin crujidos; sin deflexion visible en el top panel |
