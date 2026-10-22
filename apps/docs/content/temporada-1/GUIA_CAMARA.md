# Guía de cámara — posiciones, ángulos y cómo grabar sin ser experto

> Para los guiones de la Temporada 1. Equipo asumido: un **brazo** que apunta hacia abajo sobre la mesa (como en la
> foto del 2026-10-22), un **celular o cámara**, el **computador** para grabar la pantalla, y la **interfaz de
> audio** grabando el LINE OUT del Brain en Ableton.

---

## 1. Dos palabras que lo explican todo

- **Ángulo = desde dónde mira la cámara.** Desde arriba, desde el frente inclinada, desde la altura de la mesa…
- **Plano = qué tan cerca está.** General (se ve todo), cerrado (una parte), macro (un detalle: un encoder, un LED).

Cada toma del guion dice las dos cosas con un solo código: **P1, P2, P3…**

---

## 2. Las 7 posiciones

| Código | Nombre | Cómo se pone | Para qué sirve | Para qué NO sirve |
|---|---|---|---|---|
| **P1** | **Cenital general** | El brazo arriba, la cámara mirando **recto hacia abajo**. Encuadre como tu foto: protoboard + osciloscopio + pantalla | Mostrar qué hay y cómo está conectado; tus manos trabajando; explicar señalando | Leer la pantalla del Brain (refleja); ver los LEDs con brillo (se ven planos) |
| **P2** | **Cenital cerrado** | El mismo brazo, **más bajo** (o zoom 2×), solo la zona de los encoders y la mano | Girar perillas, apretar botones, el anillo de LEDs desde arriba | Dar contexto (no se sabe dónde estamos) |
| **P3** | **Frontal a 45°** | La cámara **delante del banco**, a la altura de tu pecho, **inclinada hacia abajo** como si miraras la mesa sentado. Con el brazo reorientado o con el celular en un trípode pequeño | **La toma "bonita"**: perillas con volumen, LEDs que brillan, pantalla del Brain legible | Mostrar conexiones de cables |
| **P4** | **Rasante** | La cámara **apoyada en la mesa**, casi a ras, mirando de lado a los componentes | B-roll dramático: el anillo de LEDs, los cables, la Teensy en primer plano. Muy bueno para Shorts | Explicar (casi no se lee nada) |
| **P5** | **A cámara** | La cámara **frente a ti**, a la altura de tus ojos, a ~1 m. Tú mirando al lente | Hablarle a la gente: cold open, promesa, cierre, opiniones | Mostrar detalles |
| **P6** | **Pantalla del computador** | **No es una cámara**: grabas la pantalla con OBS (o QuickTime en el Mac) | Ableton, código, KiCad, FreeCAD, documentos. **Aquí se ve la conexión con Live** | — |
| **P7** | **Pantalla del Brain** | La cámara **perpendicular a la pantalla de 5"** (de frente, a la misma altura que la pantalla), a 30-40 cm | Leer PERFORM, las páginas OSC/ENV, los valores | — |

### El truco para mostrar que está conectado a Live: **pantalla partida**

La cámara no puede mostrar "conectado a Ableton". Lo que se hace es grabar **dos cosas a la vez**:
- **P2 o P3** (tu mano en la perilla), y
- **P6** (Ableton en la pantalla del computador).

En la edición se ponen **lado a lado** (o Ableton grande y la mano en una ventanita). Al girar la perilla se ve la
curva de automatización dibujándose en Ableton al mismo tiempo. Así se entiende la conexión sin explicarla.

**Para sincronizar las dos grabaciones:** al empezar, **aplaude una vez** delante de la cámara con Ableton grabando.
En la edición alineas el pico del aplauso en el audio de las dos grabaciones y quedan sincronizadas.

---

## 3. Cómo hacer que se vean los LEDs y la pantalla

Los LEDs WS2812 y las pantallas son luz. Si la cámara está en automático, ajusta la exposición para la mesa y los
LEDs quedan como manchas blancas, o la pantalla queda lavada.

1. **Baja la luz del cuarto** un poco (no a oscuras): cierra la cortina a la mitad o apaga la lámpara del techo.
2. **Bloquea la exposición y el foco.** En el iPhone: mantén el dedo sobre el anillo de LEDs hasta que aparezca
   **"AE/AF bloqueado"** y **desliza el sol hacia abajo** (−1 a −1.5). En Android: modo Pro o "bloquear exposición".
3. **Usa P3 o P4 para los LEDs**, no P1: desde un lado se ve el brillo y el color; desde arriba se ven planos.
4. **Pantalla del Brain:** P7, de frente. Si refleja la ventana, gira un poco la pantalla o la cámara hasta que el
   reflejo salga del cuadro.
5. **Una sola fuente de luz principal, de lado** (la ventana o una lámpara a un costado). La luz desde arriba de la
   cámara rebota en la pantalla.

---

## 4. Reglas para cada toma (lo que más mejora el video)

1. **Colchón de 3 segundos.** Antes de mover la mano, cuenta 3 en silencio. Al terminar, deja la mano quieta 3 más.
   Esos segundos son los que permiten cortar limpio en la edición.
2. **Una idea por toma.** Si te equivocas, no sigas: di "otra vez", espera 3 segundos y repite la frase entera.
3. **Repite cada toma 2-3 veces.** La segunda casi siempre es la buena.
4. **Manos despacio.** En la cámara todo se ve más rápido de lo que es.
5. **No hables mientras acomodas.** Acomodas en silencio, después hablas.
6. **Lo que dices no tiene que ser exacto.** Las frases del guion son una guía: dilas con tus palabras, pero
   **mantén los números** (−70 dB, 6 %, 16 presets…).
7. **Graba el audio del Brain siempre** en Ableton (LINE OUT → interfaz), aunque la cámara también grabe. En la
   edición se usa el de Ableton: suena mucho mejor.
8. **Horizontal y en 4K** si tu cámara puede. De un video 4K horizontal se puede recortar un vertical para los Shorts.
   Las tomas marcadas **🔲V** conviene grabarlas además con el celular en vertical.

---

## 5. Grabar por posición, no por orden del guion

Mover el brazo y reencuadrar cuesta tiempo. Por eso **cada guion termina con un plan de sesión ordenado por
posición**: primero todas las tomas P1, después todas las P2, y así. El orden del video lo arma la edición.

Cada toma tiene un número (**T1, T2, T3…**). Al grabarla, **di el número en voz alta** al principio ("toma doce") o
muéstralo con los dedos. Así, al editar, encuentras cada toma rápido.

---

## 6. Antes de cada sesión (5 minutos)

- [ ] La pantalla del Brain muestra **PERFORM** (el kiosk), no el escritorio ni el login de Debian. Si arrancó el
      escritorio, inicia el servicio del kiosk (`apps/studio-head/deploy/`) o reinicia la Pi.
- [ ] Cables que no se usan, fuera del cuadro. Cables que van al Brain, saliendo **hacia arriba del encuadre de
      P1**, no cruzando la protoboard.
- [ ] Batería y espacio del celular. Modo avión (las notificaciones arruinan tomas).
- [ ] Exposición y foco bloqueados en cada posición nueva.
- [ ] Ableton grabando el LINE OUT; OBS listo para P6.
- [ ] Aplauso de sincronización al empezar.
