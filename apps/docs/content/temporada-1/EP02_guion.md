# EP2 — "Del INIT al bajo: encontrando mi sonido en el Juno" · guion para grabar

> Temporada 1 · 🎹 Estudio. Duración objetivo: 12-13 min. Biblia: [`BIBLIA_T1_LIVESET.md`](BIBLIA_T1_LIVESET.md).
> Resultado: la **línea de bajo** y los **acordes** del primer track, hechos en el motor inspirado en el Juno-106 y
> guardados como presets. Cameo de GroovePilot en los acordes (ADR-002, enmienda del 2026-10-22).
>
> Convenciones: **VO** voz · **[A]** cenital macro · **[C]** pantalla del computador · **[E]** audio directo ·
> **[F]** a cámara · **[V]** vertical. Lo que dice la VO es guía: dilo con tus palabras, mantén los datos.

---

## Antes de grabar (20 min, fuera de cámara)

- [ ] **Prueba del Juno (5 min).** SHIFT + NAV push → ENGINE SELECT → Juno-106. Toca notas graves y acordes;
      recorre WAVE, SUB, RANGE, CHORUS y HPF en la página OSC. Si algo falla, **anota el síntoma y el commit** (sirve
      para el EP4) y usa el **plan B**: bajo en el Model D, acordes en el Prophet. El guion se mantiene igual.
- [ ] Ableton: tempo y tonalidad del track decididos (por ejemplo 122 BPM, La menor), un loop de batería de
      8 compases, una pista MIDI con External Instrument (MIDI To = Teensy MIDI) y **una pista de audio armada** con
      la entrada del LINE OUT del Brain.
- [ ] La línea de bajo de 2 compases escrita en el piano roll (o tocada en el teclado y grabada).
- [ ] GroovePilot listo para pedirle progresiones de acordes, con la forma de uso que tengas hoy. Si no está
      disponible, el **plan B de los acordes** es tocarlos a mano (el guion lo contempla).
- [ ] Preset de partida: **INIT** del banco del Juno (se crea solo si el banco está vacío; si no, carga el de
      fábrica más neutro y dilo en cámara).

**Dato técnico que define el episodio:** el Brain toca **un motor a la vez** (solo el motor activo procesa audio,
encabezado del sketch49). Así que el flujo es el de un synth de hardware real: **grabar el bajo como audio, después
cambiar de sonido y grabar los acordes.** Eso se muestra, no se esconde.

---

## 0:00-0:15 · Cold open

| Tiempo | Imagen | Audio |
|---|---|---|
| 0:00 | **[A]+[C]** el loop terminado: batería, bajo y acordes, la mano en ENC L | El resultado final **[E]** |
| 0:05 | Corte seco a la pantalla del Brain en INIT | Una nota de diente de sierra plana, "de manual" |
| 0:08 | **[F]** | VO: *"Así termina este episodio. Y así empieza: con un sonido vacío. Hoy busco el bajo y los acordes de mi primer track en un solo motor, el inspirado en el Juno-106."* |

## 0:15-1:00 · El track

**[F]+[C]** (Ableton en pantalla):
> "El track va a 122 [tu tempo], en La menor [tu tonalidad]. Ya tengo la batería. Le falta lo que lo hace mío: un bajo
> con cuerpo y unos acordes que respiren. Elegí este motor porque el Juno original es simple: un oscilador, un sub,
> un filtro, un chorus. Con poco hay que tomar decisiones, y eso es lo que quiero mostrar."

## 1:00-5:30 · El bajo, paso a paso

Cada paso: **un cambio, 2 compases de sonido [E]**, la pantalla del Brain visible. El espectador tiene que *oír*
cada decisión. Página OSC (B1 en el panel; los slots con SHIFT están marcados).

| # | Cambio | Dónde | VO (guía) |
|---|---|---|---|
| 1 | INIT tal cual | — | "Punto de partida: una sierra con el filtro abierto. Suena a manual." |
| 2 | WAVE → SQR (o PULSE) | OSC, slot WAVE | "La cuadrada tiene menos armónicos que la sierra: más redonda, deja espacio arriba." |
| 3 | RANGE → 16' | OSC, slot RANGE | "Una octava abajo, sin tocar el piano roll." |
| 4 | SUB arriba | OSC, slot SUB | "El sub es una cuadrada una octava más abajo. Aquí está el peso." |
| 5 | Cutoff abajo con ENC L, RES baja | home / ENC L-R | "Cierro el filtro hasta que el bajo deja de pelear con el kick." |
| 6 | FLT ENV AMT arriba + decay corto | ENV (B2) | "El envelope abre el filtro un instante en cada nota: ese es el *pluck*." |
| 7 | VCA → GATE | OSC, slot VCA | "El Juno tiene un truco: el volumen on/off como un órgano, sin envelope. Notas más cortas y secas." |
| 8 | CHORUS OFF, HPF 0 | OSC con SHIFT | "En el bajo, chorus apagado: los graves, en mono y quietos. Y el HPF en 0 para no quitarle cuerpo." |
| 9 | (opcional) LFO lento al cutoff, LFO SYNC | LFO (B3) | "Un movimiento lento, al tempo de Ableton." |

**Pausa de ingeniería (≈ 1 min, [C]):** por qué este bajo no suena "digital barato":
- El oscilador usa **polyBLEP**: el aliasing de una sierra en C6 bajó de **−23 dB a −70 dB**
  (`apps/docs/theory/fidelidad-engines.md` §8). Mostrar la tabla 3 s.
- El filtro del Juno tiene su propio carácter (IR3109) dentro del ladder ZDF (§9.4, fila 6).
- Snippet opcional ≤ 20 líneas de `src/dsp/polyblep_core.h` (política de apertura).

**Grabar el bajo:** 8 compases a la pista de audio **[C]**. VO: *"El Brain toca un motor a la vez, como un synth de
hardware de verdad. Así que el bajo lo grabo como audio, y el Brain queda libre para el siguiente sonido."*

**Guardar el preset:** SHIFT + B4 → PRESETS → NAV push sobre SAVE. Queda como **USER nn**. VO honesto: *"Todavía no
le puedo poner nombre desde el panel: está en la lista de pendientes."* (theory/06 §4, fase 2).

## 5:30-9:30 · Los acordes

### 5:30-6:45 · La idea: cameo de GroovePilot

**[C]** + VO:
> "Para los acordes le pedí ideas a GroovePilot, el otro proyecto en el que trabajo. Le di la tonalidad y el tempo y
> me propuso cuatro progresiones. Escucha."

- Pasar las 4 opciones rápido, 4 compases cada una, con un piano simple de Ableton (no con el Brain todavía).
- Elegir una y **editarla en el piano roll**: cambiar un voicing, mover una nota, alargar un acorde. Que se vea la
  mano.
- Rótulo en pantalla durante todo el segmento: **"Idea MIDI: GroovePilot · Elección y edición: yo"**.
- VO: *"Me quedé con esta, y le cambié esto y esto. La IA me dio un punto de partida; el sonido y la decisión son
  míos. Y ahora los toco con mi synth."*

**Plan B (sin GroovePilot):** tocas los acordes en el teclado y grabas el MIDI. Quitas el rótulo. El resto igual.

### 6:45-9:30 · El sonido del acorde

Volver a INIT o duplicar el preset del bajo. Mismo método: un cambio, 2 compases.

| # | Cambio | VO (guía) |
|---|---|---|
| 1 | WAVE → SAW+PLS, RANGE 8' | "Sierra y pulso juntos: más ancho para acordes." |
| 2 | Attack lento, release largo (ENV) | "El acorde entra despacio y se queda: que respire." |
| 3 | Cutoff a la mitad, FLT ENV AMT bajo | "Que no tape al bajo de abajo ni al lead que vendrá arriba." |
| 4 | HPF 1-2 | "Aquí sí le quito graves: el bajo ya los tiene." |
| 5 | **CHORUS I → II → I+II** (2 compases cada uno) | "El sonido por el que el Juno es famoso." |

**Pausa de ingeniería del chorus (≈ 1 min, [C]):**
- El original tiene **dos líneas de retardo BBD con el LFO invertido**, una a la izquierda y otra a la derecha. El
  Brain hace lo mismo: dos lecturas de la misma línea, con el LFO en sentidos opuestos
  (`fidelidad-engines.md` §9.3).
- Al principio el chorus del Brain era **mono**, y lo medí: ahora es estéreo, con correlación L/R 0.78 (§9.4).
- Velocidades I y II: 0.513 y 0.863 Hz (§8).
- **Momento de audífonos:** pedirle al espectador que se ponga audífonos y alternar chorus OFF ↔ I+II.

**Efectos del synth (opcional, 30 s):** B4 → SYNTH FX: TAPE un poco y MODAL REVERB corto. *"Los cuatro efectos juntos
usan alrededor del 6 % del procesador."* (theory/05 §2).

Grabar los acordes como audio. Guardar el preset.

## 9:30-11:30 · El loop

- **[C]+[E]** las tres pistas juntas: batería, bajo, acordes. 8 compases sin voz.
- **[A]** la mano moviendo ENC L sobre los acordes mientras suena el loop (si quieres, reproduce el MIDI de los
  acordes en vivo en vez del audio, y toca el filtro).
- VO corta: *"Dos sonidos, un motor, y ya suena a mí."*

## 11:30-12:30 · Cierre

**[F]**:
> "Bajo y acordes del primer track, guardados como presets. Lo que aprendí hoy: con un motor simple decides más
> rápido. Y que tocar un motor a la vez te obliga a trabajar como con un synth de verdad: grabas y sigues.
>
> El próximo episodio salgo del estudio: te muestro cómo diseñé la caja de este synth, y por qué el primer diseño no
> cerraba."

CTA: *"Si quieres seguir el proceso, déjame tu correo. Y dime en los comentarios qué motor quieres que pruebe
después."*

---

## Packaging

**Título principal:** *Del INIT al bajo: encontrando mi sonido en el Juno*
**A/B:**
1. *Hice el bajo y los acordes de mi track con UN solo motor*
2. *El chorus que hizo famoso al Juno-106, en mi synth casero*
3. *Diseño de sonido desde cero en un synth que construí yo*
4. *Le pedí acordes a una IA. Me quedé con uno y lo cambié*

**Miniatura:** pantalla del Brain con la página OSC (WAVE, SUB, CHORUS) a la izquierda, onda de PERFORM a la derecha;
mano en un encoder. Texto: **"DESDE CERO"**.

---

## Shorts del EP2

### Short 1 · Mind-blow — "El chorus que hizo famoso al Juno" (40 s)
- **0-3 s:** acorde seco → el mismo con CHORUS I+II. Texto: "🎧 con audífonos".
- **3-35 s:** dos retardos, el LFO invertido entre izquierda y derecha; el mío era mono y lo arreglé (correlación
  0.78). Alternar OFF ↔ ON.
- **Archivos:** `fidelidad-engines.md` §9.3-9.4.

### Short 2 · Build/ASMR — "De INIT a bajo en 30 segundos" (30 s, sin voz)
- **0-3 s:** el INIT plano.
- **3-30 s:** los 9 pasos del bajo, 3 s cada uno, con el nombre del cambio en pantalla y la mano en el encoder. Termina
  con el loop.

### Short 3 · Polémica — "Le pedí acordes a una IA" (45 s)
- **0-3 s:** *"Le pedí los acordes de mi track a una IA."* (pausa) *"Y esto es lo que hice."*
- **3-40 s:** las 4 propuestas, cuál elegí, qué le cambié a mano, y cómo suena en mi synth. Mensaje: la IA da ideas,
  la decisión es tuya.
- **40-45 s:** *"¿Usarías IA para esto o no? Te leo."*
- **Ojo:** sin plan B (si no usaste GroovePilot), este Short se cambia por *"Mi synth solo toca un motor a la vez (y
  está bien)"*: 28 filtros a la vez pasaban del 200 % de CPU (encabezado del sketch49).

---

## Plan de sesión (≈ 2 h)

| # | Setup | Toma | Para | Min |
|---|---|---|---|---|
| 1 | — | Prueba del Juno y de GroovePilot (anotar todo) | plan A/B | 20 |
| 2 | A+C+E+V | Bajo: los 9 pasos, 2 compases cada uno, grabando audio | 1:00-5:30, Short 2 | 25 |
| 3 | C+E | Grabar el bajo como audio, guardar preset | 5:30 | 5 |
| 4 | C+V | GroovePilot: 4 opciones, elegir, editar | 5:30-6:45, Short 3 | 10 |
| 5 | A+C+E+V | Acordes: 5 pasos, chorus OFF / I / II / I+II | 6:45-9:30, Short 1 | 20 |
| 6 | A+C+E | El loop completo + mano en el filtro | 9:30-11:30, cold open | 10 |
| 7 | C | Capturas: fidelidad §8, §9.3-9.4, theory/05 §2, theory/06 §4, sketch49 encabezado | pausas | 10 |
| 8 | F+V | Cold open, el track, cierre, Short 3 a cámara | todos | 20 |
