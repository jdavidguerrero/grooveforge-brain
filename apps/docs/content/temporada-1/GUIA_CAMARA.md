# Guía de cámara — posiciones, ángulos y cómo grabar sin ser experto

> Para los guiones de la Temporada 1. Empieza por **§0 Mi equipo**: qué va en cada soporte y cómo se configura.

---

## 0. Mi equipo

| Equipo | Dónde | Para qué |
|---|---|---|
| **iPhone XR** | **Brazo propio** para el teléfono, fijo toda la sesión | El **cenital** (P1 y P2) |
| **iPhone 13 Pro** (cuando esté libre) | **Trípode** con la lámpara LED | P3, P4, P5, P7 y macro |
| **Lámpara LED** del trípode | **A un costado, a 45°**, nunca detrás de la cámara | Luz principal sobre las manos o la cara |
| **Shure SM57** con antipop | Su propio brazo, a **5-8 cm** de la boca | Tu voz, por la interfaz a Ableton |
| **Interfaz + Ableton** | — | Audio del Brain (LINE OUT) + voz (SM57) + sincronización |
| **Mac + OBS** | — | P6: la pantalla del computador |

### 0a. Día con las dos cámaras (lo ideal)

- **El XR graba el cenital sin parar** durante cada bloque. Nunca se mueve.
- **El 13 Pro, en el trípode, graba lo demás**: la pantalla del Brain (P7), la toma de 45° (P3), tú a cámara (P5),
  los detalles en macro.
- **Las dos al mismo tiempo**: mientras haces un paso en el Brain, el XR graba tus manos desde arriba y el 13 Pro la
  pantalla. Una sola pasada en vez de dos.
- Cada guion trae, al final, la tabla **"Con dos cámaras"**: qué graba cada una en cada bloque.

### 0b. Día con solo el XR

- El XR en el brazo para todas las tomas del cenital.
- Para P3, P5 y P7 pasas el XR al trípode **una vez por bloque** (el plan de sesión de cada guion ya está ordenado
  para eso).
- Lo que el XR no puede: macro (acércate con el trípode a ~10 cm como mínimo) y teleobjetivo (solo recorte en la
  edición).

### 0c. Configuración de los iPhone (una vez)

En **Ajustes → Cámara**, en **los dos**:
1. **Grabar video → 4K a 30 fps.** Las dos igual: si una graba a 30 y la otra a 60, la edición se complica.
2. **Formatos → Alta eficiencia** (HEVC): ocupa la mitad. Una hora de 4K ≈ 10 GB.
3. **FPS automáticos / FPS automáticos con poca luz → desactivado.** Si no, la cámara baja los cuadros sola.

Solo en el **13 Pro**:
4. **Grabar video → Video HDR → desactivado.** Si no, sus colores no combinan con los del XR y en muchos editores se
   ven lavados.
5. **Control macro → activado**, y **Grabar video → Bloquear cámara → activado**: así no cambia de lente sola cuando
   te acercas. El macro lo eliges tú con el botón de la flor.

Antes de cada sesión: **las dos enchufadas al cargador** y en **modo avión**.

### 0d. La voz: SM57 por la interfaz

- **A 5-8 cm de la boca**, con el antipop, apuntando a la boca. Lejos se oye débil y con eco del cuarto.
- **Ganancia alta** en la interfaz (el SM57 tiene salida baja): tu voz normal debe llegar a unos **−12 dB** en
  Ableton, sin pasar de −6.
- **Pista propia en Ableton**, al lado de la del Brain. El micrófono de los iPhone queda solo como referencia para
  sincronizar.
- **Que se vea en P5 está bien**: da imagen de estudio.

**Voz en off: la mitad del guion no necesita tu cara.** Todo lo que va sobre P6 (pantalla del computador), sobre tus
manos o sobre la pantalla del Brain lo puedes grabar **después**, sentado, leyendo tranquilo con el SM57. En P5
graba solo lo esencial: el arranque, las opiniones, el cierre. En los guiones, las frases de las tomas que **no**
son P5 se pueden grabar como voz en off.

### 0e. Sincronizar todo: un aplauso

Al empezar **cada bloque**, con las dos cámaras, Ableton (y OBS si aplica) grabando: **un aplauso fuerte** delante de
las cámaras. En la edición alineas ese pico en todas las pistas y quedan sincronizadas.

**Alternativa para las tomas con Ableton:** con **Continuity Camera** (macOS Ventura o más nuevo, iOS 16 o más nuevo;
el XR es compatible), OBS usa el iPhone como cámara. Así grabas tu mano y Ableton **en la misma grabación**, ya
sincronizadas. Ojo: así graba en 1080p, sin margen para zoom: encuadra cerrado desde el principio.

---

## 1. Dos palabras que lo explican todo

- **Ángulo = desde dónde mira la cámara.** Desde arriba, desde el frente inclinada, desde la altura de la mesa…
- **Plano = qué tan cerca está.** General (se ve todo), cerrado (una parte), macro (un detalle: un encoder, un LED).

Cada toma del guion dice las dos cosas con un solo código: **P1, P2, P3…**

---

## 2. Las 7 posiciones

| Código | Nombre | Cámara | Cómo se pone | Para qué sirve | Para qué NO sirve |
|---|---|---|---|---|---|
| **P1** | **Cenital general** | XR (brazo) | Mirando **recto hacia abajo**. Encuadre: protoboard + osciloscopio + pantalla | Mostrar qué hay y cómo está conectado; tus manos; explicar señalando | Leer la pantalla del Brain (refleja); LEDs con brillo (se ven planos) |
| **P2** | **Cenital cerrado** | XR (brazo) | **No se graba aparte:** es un **recorte 2× de P1 en la edición** (§2b) | Girar perillas, apretar botones, el anillo de LEDs desde arriba | Dar contexto |
| **P3** | **Frontal a 45°** | 13 Pro (trípode) | **Delante del banco**, a la altura de tu pecho, **inclinada hacia abajo** como si miraras la mesa sentado | **La toma "bonita"**: perillas con volumen, LEDs que brillan, pantalla legible | Mostrar conexiones de cables |
| **P4** | **Rasante** | 13 Pro (trípode bajo o apoyado en la mesa) | Casi a ras de la mesa, mirando de lado | B-roll dramático: el anillo, los cables, la Teensy. Muy bueno para Shorts | Explicar |
| **P5** | **A cámara** | 13 Pro (trípode), **cámara trasera** | **Frente a ti**, a la altura de tus ojos, a ~1 m. Tú mirando al lente | Cold open, promesa, opiniones, cierre | Mostrar detalles |
| **P6** | **Pantalla del computador** | OBS en el Mac | **No es una cámara**: grabas la pantalla | Ableton, código, KiCad, FreeCAD. **Aquí se ve la conexión con Live** | — |
| **P7** | **Pantalla del Brain** | 13 Pro (trípode) | **De frente** a la pantalla de 5", a su misma altura, a 30-40 cm (o con el 3× desde más lejos) | Leer PERFORM, las páginas OSC/ENV, los valores | — |
| **M** | **Macro** | 13 Pro, modo macro | A 2-5 cm de la pieza | Chips, pines, un LED, el texto de un componente | Todo lo demás |

**Para P5 con la cámara trasera** no te ves mientras grabas: haz una toma de prueba de 5 s, revisa el encuadre y
marca en el piso dónde va la silla.

### 2b. P1 y P2 son una sola toma: cenital fijo + zoom en la edición

El XR queda **fijo** en el brazo durante toda la sesión. De esa misma grabación salen las dos posiciones:

- **P1** = el cuadro completo.
- **P2** = un recorte de la mitad del cuadro en la edición (**zoom 2×**), por ejemplo solo los encoders y tu mano.

**Por qué no pierde calidad:** un cuadro 4K (3840 × 2160) tiene el doble de ancho y de alto que uno 1080p
(1920 × 1080). Si exportas el video en 1080p, recortar la mitad del cuadro deja una imagen 1080p completa.

**Cómo grabar el cenital:**
- **4K a 30 fps** (§0c).
- **Lente 1×**, sin zoom digital al grabar (el XR tiene un solo lente).
- **Foco y exposición bloqueados** en el centro de la protoboard. Desde arriba todo queda a casi la misma distancia.
- **Los encoders cerca del centro del cuadro**, no pegados al borde: el recorte necesita espacio.
- **El brazo rígido** (aluminio articulado, no cuello de ganso) y sujeto lejos de donde apoyas las manos: si no,
  vibra cada vez que aprietas un botón.

**Límites:**
- **Hasta 2× queda nítido.** Más allá (un LED, el texto de un chip) se ve borroso: para eso está el **macro (M)** del
  13 Pro. Sin el 13 Pro, acerca el trípode al final de la sesión.
- **El recorte no cambia el ángulo.** Los LEDs con brillo y las perillas con volumen salen de **P3** y **P4**.
- **Shorts verticales:** un vertical recortado del 4K horizontal sirve para el plano general, pero no admite zoom
  encima. Las tomas **🔲V** con zoom, mejor grabarlas también en vertical.

**En la edición:** alterna cortes secos entre P1 y P2, o haz un acercamiento lento (de 1× a 1.5× en ~5 s) mientras
explicas algo: le da movimiento a una toma estática.

### 2c. Mostrar que está conectado a Live: **pantalla partida**

La cámara no puede mostrar "conectado a Ableton". Se graban **dos cosas a la vez**:
- el **cenital** (tu mano en la perilla) o **P3**, y
- **P6** (Ableton en el computador, con OBS).

En la edición se ponen **lado a lado** (o Ableton grande y la mano en una ventanita). Al girar la perilla se ve la
curva de automatización dibujándose al mismo tiempo. Se sincronizan con el aplauso (§0e).

---

## 3. Cómo hacer que se vean los LEDs y la pantalla

Los LEDs WS2812 y las pantallas son luz. En automático, la cámara expone para la mesa y los LEDs quedan como manchas
blancas, o la pantalla queda lavada.

1. **Baja la luz del cuarto** un poco (no a oscuras): cortina a la mitad, lámpara del techo apagada. La **lámpara del
   trípode a un costado** ilumina tus manos.
2. **Bloquea la exposición y el foco.** Mantén el dedo sobre el anillo de LEDs hasta que aparezca **"AE/AF
   bloqueado"** y **desliza el sol hacia abajo** (−1 a −1.5).
3. **Usa P3, P4 o M para los LEDs**, no el cenital: desde un lado se ve el brillo y el color.
4. **Pantalla del Brain:** P7, de frente. Si refleja la ventana o la lámpara, gira un poco la pantalla o la cámara
   hasta que el reflejo salga del cuadro.
5. **La luz nunca desde detrás de la cámara:** rebota en la pantalla y en el plástico de la protoboard.

### 3b. Prueba de parpadeo (30 s antes de cada sesión)

En Colombia la corriente es de **60 Hz**. Las lámparas LED, las pantallas y los WS2812 pueden verse en video con
**franjas que suben o bajan** o con parpadeo, aunque a simple vista no se note.

- Graba 10 s de la pantalla del Brain y del anillo de LEDs, con la lámpara encendida, y míralo en el teléfono.
- Si hay franjas: cambia la lámpara de intensidad (algunas parpadean menos al 100 %), usa la luz de la ventana, o
  graba con una **app de cámara manual** (por ejemplo Blackmagic Camera, gratis; revisa que sea compatible con tu
  modelo) a 30 fps con el **obturador en 1/60 o 1/120**. La app de cámara normal del iPhone no deja fijar el
  obturador.

---

## 4. Reglas para cada toma (lo que más mejora el video)

1. **Colchón de 3 segundos.** Antes de mover la mano, cuenta 3 en silencio. Al terminar, deja la mano quieta 3 más.
2. **Una idea por toma.** Si te equivocas, no sigas: di "otra vez", espera 3 segundos y repite la frase entera.
3. **Repite cada toma 2-3 veces.** La segunda casi siempre es la buena.
4. **Manos despacio.** En la cámara todo se ve más rápido de lo que es.
5. **No hables mientras acomodas.** Acomodas en silencio, después hablas.
6. **Lo que dices no tiene que ser exacto.** Dilo con tus palabras, pero **mantén los números** (−70 dB, 6 %, 16
   presets…). Si una frase no sale bien en cámara, déjala para la **voz en off** (§0d).
7. **El audio bueno es el de Ableton**: el del Brain (LINE OUT) y tu voz (SM57). El de los teléfonos solo sirve para
   sincronizar.
8. **Horizontal, en 4K, y exporta en 1080p**: así puedes recortar hasta 2× o sacar un vertical para los Shorts. Las
   tomas **🔲V** conviene grabarlas además en vertical.

---

## 5. Grabar por bloques, no por orden del guion

Cada guion termina con dos tablas:
- **Plan de sesión** (solo con el XR): ordenado por posición, para mover el teléfono lo menos posible.
- **Con dos cámaras**: qué graba el XR (cenital) y qué el 13 Pro (trípode) en cada bloque, al mismo tiempo.

El orden del video lo arma la edición. Cada toma tiene un número (**T1, T2, T3…**): **dilo en voz alta** al empezarla
("toma doce") o muéstralo con los dedos. Así encuentras cada toma rápido al editar.

---

## 6. Antes de cada sesión (5 minutos)

- [ ] La pantalla del Brain muestra **PERFORM** (el kiosk), no el escritorio ni el login de Debian. Si arrancó el
      escritorio, inicia el servicio del kiosk (`apps/studio-head/deploy/`) o reinicia la Pi.
- [ ] Cables que no se usan, fuera del cuadro. Los que van al Brain, saliendo **hacia arriba del encuadre del
      cenital**, no cruzando la protoboard.
- [ ] Los dos iPhone: cargando, en modo avión, con espacio (≈ 10 GB por hora cada uno).
- [ ] Lámpara a un costado; **prueba de parpadeo** (§3b).
- [ ] Exposición y foco bloqueados en cada cámara.
- [ ] Ableton grabando: LINE OUT del Brain + SM57 (voz a −12 dB). OBS listo para P6.
- [ ] **Aplauso** al empezar cada bloque.

---

## 7. La mesa: qué dejar y dónde (escenografía)

La regla: **tres capas, una cosa protagonista en cada una.** Así la imagen tiene profundidad y el ojo sabe dónde mirar.

```
            PARED / FONDO
   ┌──────────────────────────────────────────────┐
   │  fuente de banco   osciloscopio   pantalla del Brain   marco GrooveMakers │  ← capa 3: fondo (contexto, "taller")
   │                                                                          │
   │                    PROTOBOARD DEL BRAIN                                  │  ← capa 2: protagonista
   │                                                                          │
   │                    TECLADO MIDI                                          │  ← capa 1: primer plano (tus manos)
   └──────────────────────────────────────────────┘
                              TÚ
```

| Objeto | ¿Se queda? | Dónde | Por qué |
|---|---|---|---|
| **Protoboard del Brain** | Sí, protagonista | Centro de la mesa | Es el episodio |
| **Teclado MIDI** | **Sí** | Delante de la protoboard, hacia ti | Se ve que el instrumento se toca. En el cenital, el borde de las teclas entra por abajo del cuadro con tus manos |
| **Osciloscopio** | **Sí** | Fondo, cerca de la pantalla del Brain | Da "taller" y, si le conectas la punta al LINE OUT (acople AC), **la onda se mueve con la música**: un fondo vivo |
| **Fuente de banco** | Sí, si cabe | Fondo, en una esquina | Da taller. Si el display distrae, apágalo o déjalo fuera del cuadro del cenital |
| **Fuente conmutada grande (la plateada)** | No | Fuera del cuadro | No aporta y refleja la luz |
| **Pantalla de 7" (la del Push)** | Opcional | Fondo, a un lado | Úsala como **marco digital con el logo de GrooveMakers** (§7b). No la presentes como parte del Push todavía |
| **Papeles, tazas, cables sueltos** | No | Fuera | En cámara todo se ve más desordenado de lo que es |

**El cenital (XR)** sigue encuadrando la protoboard, el osciloscopio y la pantalla del Brain. Si el teclado es muy
largo, que solo entren las teclas centrales por el borde de abajo.

### 7a. Qué enfoca el trípode (13 Pro), toma por toma

| Posición | Dónde va el trípode | Qué entra en el cuadro | Enfoque y exposición |
|---|---|---|---|
| **P3** frontal 45° | Delante y un poco a un lado del teclado (izquierda o derecha, la que no tape la lámpara), a la altura de tu pecho, mirando hacia abajo | **Las tres capas**: teclado (abajo, desenfocado está bien), protoboard (al centro), pantalla del Brain + osciloscopio + marco (al fondo) | Foco en la protoboard; exposición bloqueada en el anillo de LEDs, −1 |
| **P4** rasante | Trípode bajo o el teléfono apoyado en la mesa, al costado de la protoboard | El anillo de LEDs y los encoders en primer plano, la pantalla del Brain desenfocada atrás | Foco en el anillo |
| **P5** a cámara | Frente a ti, a la altura de tus ojos, a ~1 m. Si la mesa está contra la pared, gira la silla de lado y pon el trípode en el costado libre | Tú de la cintura para arriba; **detrás o al lado: el osciloscopio, la pantalla del Brain o el marco del logo**, no una pared vacía | Foco en tu cara; exposición en tu cara |
| **P7** pantalla del Brain | De frente a la pantalla, a su misma altura, a 30-40 cm (o con el 3× desde más lejos) | Solo la pantalla, llenando el cuadro | Foco y exposición en la pantalla, −0.5 si se ve lavada |
| **M** macro | A 2-5 cm de la pieza | Un chip, el anillo, los pines | Foco tocando la pieza en la pantalla del teléfono |

### 7b. El marco de GrooveMakers

Un marco en la mesa con el logo (impreso o en la pantalla de 7") aparece en P3 y P5 en todos los episodios: es la
firma visual del canal sin necesidad de intro. Prompt para diseñarlo junto con la marca:
[`../marca/claude-design-prompt-groovemakers.md`](../marca/claude-design-prompt-groovemakers.md).
