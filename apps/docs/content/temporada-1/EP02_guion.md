# EP2 — "Del INIT al bajo: encontrando mi sonido en el Juno" · guion para grabar

> Temporada 1 · 🎹 Estudio · 12-13 min. Biblia: [`BIBLIA_T1_LIVESET.md`](BIBLIA_T1_LIVESET.md).
> **Posiciones de cámara (P1-P7):** [`GUIA_CAMARA.md`](GUIA_CAMARA.md). Formato igual al EP1: cada fila es una toma
> (**T**), con su posición, lo que se ve y lo que dices. **🔲V** = también en vertical.
>
> Resultado: la **línea de bajo** y los **acordes** del primer track, hechos en el motor inspirado en el Juno-106 y
> guardados como presets. Cameo de GroovePilot en los acordes (ADR-002, enmienda del 2026-10-22).

---

## Antes de grabar (20 min, sin cámara)

- [ ] **Prueba del Juno (5 min).** SHIFT + NAV push → ENGINE SELECT → Juno-106. Toca graves y acordes; recorre WAVE,
      SUB, RANGE, CHORUS y HPF en la página OSC. Si falla, anota síntoma + commit (material para el EP4) y usa el
      **plan B**: bajo en el Model D, acordes en el Prophet. El guion no cambia.
- [ ] Ableton: tempo y tonalidad del track (ej. 122 BPM, La menor), loop de batería de 8 compases, pista MIDI con
      External Instrument (MIDI To = Teensy MIDI), **pista de audio armada** con el LINE OUT del Brain.
- [ ] La línea de bajo de 2 compases en el piano roll.
- [ ] GroovePilot listo para pedir progresiones. Si no está disponible: **plan B de los acordes** (tocarlos tú).
- [ ] Preset de partida: **INIT** del Juno (si no está, el de fábrica más neutro; dilo en cámara).
- [ ] Pantalla del Brain en PERFORM.

**Dato que define el episodio:** el Brain toca **un motor a la vez**. Por eso el bajo se graba como audio y después
se hacen los acordes. Se muestra como el flujo de un synth de hardware real.

**Truco para que se oiga cada cambio:** el loop de 2 compases del bajo queda en repetición en Ableton durante toda la
sección del bajo. Haces **un cambio por toma** y dejas sonar 2 compases (≈ 4 s a 122 BPM) antes y después.

---

## 1 · Cold open (0:00-0:15)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T1 🔲V | **P3** frontal 45° | El loop terminado (batería + bajo + acordes); tu mano en ENC L | — |
| T2 | **P7** pantalla del Brain | Corte seco: la pantalla en INIT; suena una sierra plana | — |
| T3 | **P5** a cámara | Tú | "Así termina este episodio. Y así empieza: con un sonido vacío. Hoy busco el bajo y los acordes de mi primer track en un solo motor: el inspirado en el Juno-106." |

## 2 · El track (0:15-1:00)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T4 | **P6** pantalla del computador | Ableton con la batería sonando | "El track va a 122, en La menor. [Tus datos.] Ya tengo la batería." |
| T5 | **P5** | A cámara | "Le falta lo que lo hace mío: un bajo con cuerpo y unos acordes que respiren." |
| T6 | **P5** | A cámara | "Elegí este motor porque el Juno original es simple: un oscilador, un sub, un filtro, un chorus. Con poco tienes que decidir, y eso es lo que quiero mostrar." |

## 3 · El bajo, paso a paso (1:00-5:30)

Para cada paso graba **dos tomas**: **P2** (la mano haciendo el cambio, hablando; sale del cenital fijo con zoom en la
edición) y **P7** (la pantalla del Brain
mostrando el valor, sin hablar). En la edición se alternan. La página es OSC (B1); los marcados con SHIFT se ven
manteniendo SHIFT.

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T7 | **P2** + **P7** | INIT tal cual, 2 compases | "Punto de partida: una sierra con el filtro abierto. Suena a manual." |
| T8 | **P2** + **P7** | WAVE → SQR (o PULSE) | "Cambio a onda cuadrada: tiene menos armónicos que la sierra. Más redonda, y deja espacio arriba." |
| T9 | **P2** + **P7** | RANGE → 16' | "Una octava más abajo, sin tocar el piano roll." |
| T10 | **P2** + **P7** | SUB arriba | "El sub es otra cuadrada, una octava más abajo. Aquí está el peso." |
| T11 | **P2** + **P7** | En PERFORM: cierras el cutoff con ENC L, resonancia baja con ENC R | "Cierro el filtro hasta que el bajo deja de pelear con el kick." |
| T12 | **P2** + **P7** | ENV (B2): FLT ENV AMT arriba, decay corto | "El envelope abre el filtro un instante en cada nota. Ese golpecito es el *pluck*." |
| T13 | **P2** + **P7** | VCA → GATE | "El Juno tiene un truco: el volumen se prende y se apaga como un órgano, sin envelope. Las notas quedan más secas." |
| T14 | **P2** + **P7** | SHIFT: CHORUS OFF, HPF 0 | "En el bajo, el chorus apagado: los graves quietos y en el centro. Y el filtro de graves en cero, para no quitarle cuerpo." |
| T15 (opcional) | **P2** + **P7** | LFO (B3): LFO lento al cutoff, LFO SYNC | "Un movimiento lento, al tempo de Ableton." |

**Pausa de ingeniería (≈ 1 min)**

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T16 | **P5** | A cámara | "¿Por qué este bajo no suena a synth digital barato? Dos razones." |
| T17 | **P6** | `apps/docs/theory/fidelidad-engines.md` §8, la fila de polyBLEP resaltada | "Uno: el oscilador está hecho para no generar *aliasing*, esos tonos falsos que aparecen en los agudos. Lo medí: bajó de menos 23 a menos 70 decibeles." |
| T18 | **P6** | §9.4, la fila 6 (filtros Juno / Prophet) | "Dos: el filtro tiene el carácter del chip del Juno original, no el de un Moog." |
| T19 (opcional) | **P6** | Snippet ≤ 20 líneas de `src/dsp/polyblep_core.h` | "Para los que les gusta el código: esta es la parte que corrige cada salto de la onda." |

**Grabar y guardar el bajo**

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T20 | **P6** | Ableton grabando 8 compases en la pista de audio | "El Brain toca un motor a la vez, como un synth de hardware de verdad. Así que el bajo lo grabo como audio, y el Brain queda libre para el siguiente sonido." |
| T21 | **P2** + **P7** | SHIFT + B4 → PRESETS → NAV push sobre SAVE; aparece USER nn | "Lo guardo. Todavía no le puedo poner nombre desde el panel: está en la lista de pendientes." |

## 4 · Los acordes (5:30-9:30)

**La idea: cameo de GroovePilot** (aplaude al empezar si grabas P6 y cámara a la vez)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T22 | **P5** | A cámara | "Para los acordes le pedí ideas a GroovePilot, el otro proyecto en el que trabajo. Le di la tonalidad y el tempo, y me propuso cuatro progresiones." |
| T23 🔲V | **P6** | Las 4 opciones, 4 compases cada una, con un piano de Ableton. **Rótulo en edición, todo el segmento: "Idea MIDI: GroovePilot · Elección y edición: yo"** | "Escucha. Uno… dos… tres… cuatro." |
| T24 🔲V | **P6** | Eliges una y la editas en el piano roll: un voicing, una nota, un acorde más largo. Que se vea el mouse | "Me quedo con esta. Le cambio esto… y esto." |
| T25 | **P5** | A cámara | "La IA me dio un punto de partida. El sonido y la decisión son míos. Y ahora los toco con mi synth." |

**Plan B (sin GroovePilot):** T23-T25 se reemplazan por P2: tocas los acordes en el teclado y grabas el MIDI.
Dices: *"Para los acordes, toco hasta encontrar algo."* Sin rótulo.

**El sonido del acorde** (mismo método: P2 + P7 por paso; el MIDI de los acordes en repetición)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T26 | **P2** + **P7** | INIT o duplicas el preset del bajo; WAVE → SAW+PLS, RANGE 8' | "Sierra y pulso juntos: más ancho para acordes." |
| T27 | **P2** + **P7** | ENV: attack lento, release largo | "El acorde entra despacio y se queda. Que respire." |
| T28 | **P2** + **P7** | Cutoff a la mitad, FLT ENV AMT bajo | "Que no tape al bajo de abajo ni al lead que va a venir arriba." |
| T29 | **P2** + **P7** | SHIFT: HPF 1-2 | "Aquí sí le quito graves: el bajo ya los tiene." |
| T30 🔲V | **P2** + **P7** | SHIFT: CHORUS OFF → I → II → I+II, 2 compases cada uno | "Y ahora, el sonido por el que el Juno es famoso: el chorus. Apagado… uno… dos… los dos." |

**Pausa de ingeniería del chorus (≈ 1 min)**

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T31 | **P5** | A cámara | "Ponte audífonos para esto." |
| T32 | **P6** | `fidelidad-engines.md` §9.3, o un dibujo simple: dos líneas de retardo, una a cada lado, con el LFO en sentidos opuestos | "El Juno original tiene dos líneas de retardo, una a la izquierda y otra a la derecha, y un LFO que las mueve en sentidos opuestos. Por eso el sonido se abre en estéreo." |
| T33 | **P6** | §9.4, la fila del chorus (mono → estéreo, correlación 0.78) | "Al principio el mío era mono. Lo medí, lo arreglé, y ahora es estéreo." |
| T34 | **P2** | CHORUS OFF ↔ I+II, alternando cada 2 compases | — |
| T35 (opcional) | **P2** | B4 → SYNTH FX: un poco de TAPE y REVERB corto | "Un poco de cinta y de reverb. Los cuatro efectos juntos usan alrededor del 6 % del procesador." |
| T36 | **P6** | Grabas los acordes como audio; SAVE del preset (P2 rápido) | "Grabo los acordes y los guardo." |

## 5 · El loop (9:30-11:30)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T37 | **P6** | Las tres pistas sonando juntas, 8 compases | — |
| T38 🔲V | **P3** | Tu mano mueve ENC L sobre los acordes (reproduce el MIDI de los acordes en vivo, no el audio, para poder tocar el filtro) | — |
| T39 | **P4** rasante | El anillo de LEDs y las perillas con el loop sonando | — |
| T40 | **P5** | A cámara | "Dos sonidos, un motor, y ya suena a mí." |

## 6 · Cierre (11:30-12:30)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T41 | **P5** | A cámara | "Bajo y acordes del primer track, guardados. Lo que aprendí: con un motor simple decides más rápido. Y tocar un motor a la vez te obliga a trabajar como con un synth de verdad: grabas y sigues." |
| T42 | **P5** | A cámara | "El próximo episodio salgo del estudio: te muestro cómo diseñé la caja de este synth, y por qué el primer diseño no cerraba." |
| T43 | **P5** | A cámara | "Si quieres seguir el proceso, déjame tu correo. Y dime en los comentarios qué motor quieres que pruebe después." |

---

## Packaging

**Título principal:** *Del INIT al bajo: encontrando mi sonido en el Juno*
**A/B:** *Hice el bajo y los acordes de mi track con UN solo motor* · *El chorus que hizo famoso al Juno-106, en mi
synth casero* · *Le pedí acordes a una IA. Me quedé con uno y lo cambié*

**Miniatura:** foto en **P3**: la pantalla del Brain con la página OSC (WAVE, SUB, CHORUS) y tu mano en un encoder,
exposición baja para que se lean los valores. Texto: **"DESDE CERO"**.

---

## Shorts (salen de las mismas tomas)

| Short | Tomas | Cómo se arma | Qué dices (nuevo) |
|---|---|---|---|
| **1 · "El chorus que hizo famoso al Juno"** (40 s) | T30, T32-T34 | Texto "🎧 con audífonos" al inicio; acorde seco → con chorus | Arranque: "Escucha este acorde… y ahora con el chorus del Juno." Cierre: "Dos retardos, uno a cada lado, con el LFO al revés. El mío era mono. Ya no." |
| **2 · "De INIT a bajo en 30 segundos"** (30 s, sin voz) | T7-T14 (verticales de P2) | 3 s por paso, el nombre del cambio escrito en pantalla, termina con el loop | — |
| **3 · "Le pedí acordes a una IA"** (45 s) | T22-T25 + T30 | Tú a cámara, las 4 opciones, la edición, el resultado en el Brain | Arranque: "Le pedí los acordes de mi track a una IA… y esto es lo que hice." Cierre: "La IA da ideas, la decisión es tuya. ¿Tú la usarías para esto? Te leo." |

Sin GroovePilot, el Short 3 pasa a: **"Mi synth solo toca un motor a la vez (y está bien)"**, con T20 y la frase:
*"Con todos los filtros encendidos a la vez, el procesador pasaba del 200 %."* (encabezado del sketch49).

---

## Plan de sesión, ordenado por posición (≈ 2 h)

| Orden | Posición | Tomas | Min |
|---|---|---|---|
| 1 | — | Prueba del Juno y de GroovePilot | 20 |
| 2 | **Cenital fijo** (P2 en la edición), con el loop sonando | T7-T15, T21, T26-T30, T34-T35 | 20 |
| 2b | **P7** (pantalla del Brain): se repiten los mismos pasos | T7-T15, T26-T30 | 20 |
| 3 | **P6** (pantalla del computador) | T4, T17-T20, T23-T24, T32-T33, T36, T37 | 20 |
| 4 | **P3** (frontal 45°) | T1, T38, **foto de la miniatura** | 10 |
| 5 | **P4** (rasante) | T39 | 5 |
| 6 | **P7** solo | T2 | 2 |
| 7 | **P5** (a cámara) | T3, T5, T6, T16, T22, T25, T31, T40-T43 + frases de los Shorts | 20 |

**Consejo para P2 + P7:** si tienes una sola cámara, haz primero **todos los pasos con el cenital fijo** (el bajo
completo, de INIT a T15, y los acordes), guarda los presets, vuelve a cargar INIT y repite **todos los pasos en P7**.
Es más rápido que mover la cámara en cada paso. Con dos cámaras (el celular en P7 y otra en el cenital), un solo
paso basta.
