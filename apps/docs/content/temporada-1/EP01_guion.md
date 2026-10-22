# EP1 — "Construyo mi synth para encontrar mi sonido" · guion para grabar

> Temporada 1, piloto · 🎹 Estudio · 11-12 min. Biblia: [`BIBLIA_T1_LIVESET.md`](BIBLIA_T1_LIVESET.md).
> **Posiciones de cámara (P1-P7) y cómo grabar:** [`GUIA_CAMARA.md`](GUIA_CAMARA.md).
>
> **Cómo leer este guion:** cada fila es una toma (**T1, T2…**). La columna **Posición** dice dónde va la cámara;
> **Qué haces / qué se ve** es la imagen; **Qué dices** es tu texto (dilo con tus palabras, mantén los números).
> "—" en "Qué dices" = no hablas, solo suena el Brain. **🔲V** = grábala también en vertical para los Shorts.
> Al final está el **plan de sesión ordenado por posición**: graba en ese orden, no en el del video.

---

## Antes de grabar (15 min)

- [ ] `pio run -e sketch49 -t upload`; anota el commit.
- [ ] La pantalla del Brain en **PERFORM** (no el login de Debian).
- [ ] Prueba, en este orden: teclado → nota; ENC L mueve el cutoff; Ableton toca el Brain (External Instrument,
      MIDI To = Teensy MIDI); con el clock de Ableton, **LFO SYNC** sigue el tempo (en el monitor serie:
      `[UI] LFO SYNC ON (xxx BPM)`); al girar el cutoff aparece la automatización en Ableton.
- [ ] Si algo falla: grábalo igual (es contenido) y usa la **variante B** del clímax.
- [ ] En Ableton: loop de batería de 8 compases (120-126 BPM), un clip MIDI con un bajo de 2 compases, pista de
      audio grabando el LINE OUT del Brain.

---

## 1 · Cold open (0:00-0:15)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T1 🔲V | **P2** cenital cerrado | Tu mano gira ENC L despacio; suena el bajo del Model D sobre el loop y el filtro se abre | — |
| T2 | **P7** pantalla del Brain | PERFORM: el cutoff sube desde 800 Hz y la onda se abre (mismo gesto que T1) | — |
| T3 | **P1** cenital general | El banco completo, tu mano sale del encoder | — |
| T4 | **P5** a cámara | Tú, el banco a un lado | "Este es mi sintetizador. Lo estoy construyendo para encontrar mi sonido…" |
| T5 | **P3** frontal 45° | La protoboard con el anillo de LEDs encendido | "…y cuando esté listo, va a salir de esta protoboard: a mis tracks y a mi live set." |

## 2 · La promesa (0:15-0:50)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T6 | **P5** | A cámara | "Produzco música electrónica. Y llevo tiempo buscando un sonido que sea mío, no el preset que usa todo el mundo." |
| T7 | **P5** | A cámara | "Además, el sonido siempre está en la computadora y mis manos en un mouse. Así que decidí construir mi propio instrumento: entenderlo por dentro y poder cambiarlo." |
| T8 | **P5** | A cámara | "Esta serie es el proceso, completo. En cada episodio construyo una parte, te muestro cómo funciona por dentro y la uso en un track mío. Primero en el estudio; después, cuando esté en su caja, en vivo. Si algo falla, también lo vas a ver." |
| T9 | **P5** | A cámara | "Hoy: qué es esta cosa, y si ya sirve para hacer música." |
| B-roll (va encima de T6-T9) | **P2**, **P4**, **P6** | P2: NAV girando con el anillo · P4: el anillo de LEDs de cerca · P6: Ableton con el loop | — |

## 3 · Por qué construirlo (0:50-3:00)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T10 | **P5** | A cámara | "¿Por qué no compro un synth y ya? Cinco razones." |
| T11 | **P5** | A cámara (levanta un dedo por razón) | "Uno: quiero un sonido mío. Si lo construyo, lo puedo moldear hasta que suene como yo." |
| T12 | **P5** | A cámara | "Dos: quiero perillas. Producir es usar las manos: filtro, resonancia, envolventes." |
| T13 | **P6** pantalla del computador | La carpeta `apps/firmware-teensy/src/engines/` con los archivos de los 6 motores | "Tres: quiero sonidos clásicos sin pagar lo que cuestan. Adentro hay seis motores inspirados en el Model D, el Juno-106, el Prophet-5, el OB-6, el DX7 y el ARP 2600." |
| T14 | **P5** | A cámara | "Cuatro: quiero entender mi instrumento. Si sé por qué suena así, lo puedo cambiar. Y no lo hago para venderlo: lo hago para mi música, y el proceso lo comparto completo." |
| T15 | **P6** | `apps/docs/theory/fidelidad-engines.md` §0, la tabla, 3 segundos | "Y cinco, la honestidad: todavía no suenan igual que los originales. Lo medí, y lo vas a ver en esta serie." |

## 4 · Tour del banco (3:00-6:00)

Toda esta sección es **P1** (cenital general) con tu dedo señalando, más un detalle en **P2** o **P3** de cada pieza
para cortar encima. Graba primero la pasada completa en P1 hablando, y después los detalles sin hablar.

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T16 | **P1** | Señalas la Teensy | "El cerebro: un microcontrolador ARM Cortex-M7 a 600 MHz. Aquí corren los seis synths y los efectos." |
| T17 | **P1** | Señalas el Audio Shield | "La tarjeta de sonido. Sale por esta salida de línea a mis monitores." |
| T18 | **P1** | Señalas ENC L y ENC R | "Cutoff y resonancia, siempre a mano. Es lo que más toco." |
| T19 | **P1** | Señalas P1 y P2 | "Dos perillas que cambian según la página: envolventes, LFO, osciladores." |
| T20 | **P1** | Señalas NAV y el anillo | "Navegación. El anillo me dice dónde estoy sin mirar la pantalla." |
| T21 | **P1** | Señalas los botones B1-B6 | "Botones para las páginas, el modo y SHIFT." |
| T22 | **P1** | Señalas la pantalla | "Y esta pantalla es una Raspberry Pi con Linux." |
| T23 | **P1** | Pones la mano sobre la Teensy | "Pero el sonido no pasa por la Pi. Los controles y el audio van directo a la Teensy. La pantalla solo dibuja lo que la Teensy le cuenta: si la Pi se cuelga, el synth sigue sonando." |
| T24 | **P1** | Señalas el cable USB del teclado | "Aquí entra el teclado MIDI." |
| T25 | **P1** | Señalas el osciloscopio | "Y esto lo voy a usar mucho: para ver lo que oímos." |
| T26-T31 | **P2** / **P3** | Detalles sin hablar: Teensy, shield, ENC L girando, P1/P2, anillo de NAV (P3, LEDs con brillo), botones apretándose | — |
| T32 | **P6** | Diagrama (abajo) dibujado o animado, y el encabezado de `src/sketches/49-studio-brain.cpp` (líneas 1-35) | (se usa bajo T23) |

```
teclado (USB host) ─┐
Ableton (USB) ──────┼─► Teensy 4.1: motor → efectos → tarjeta de sonido → LINE OUT
panel (perillas) ───┘            │
                                 └─ cable serie ─► Raspberry Pi + pantalla (solo dibuja)
```

## 5 · Lo que ya hace (6:00-8:30)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T33 | **P2** + **P7** | SHIFT + NAV push → ENGINE SELECT; mismo acorde en Model D → Juno → DX7 (graba P2 y P7 en dos pasadas) | "Puedo cambiar de motor. El mismo acorde: Moog… Juno… DX7." |
| T34 | **P7** | PERFORM; giras ENC L y ENC R | "Esta onda se calcula con el cutoff y la resonancia que uso: si cierro el filtro, se redondea; si subo la resonancia, aparece el *ringing*. Es una ilustración, no el audio medido." |
| T35 | **P2** | B4 → SYNTH FX, enciendes TAPE y REVERB | "Tiene cuatro efectos fijos, en el orden de un pedalboard. Los cuatro juntos usan alrededor del 6 % del procesador." |
| T36 | **P7** | SHIFT + B4 → PRESETS, cargas uno | "Presets: dieciséis por motor, y sobreviven al apagado." |
| T37 | **P6** | `variants/studio/theory/07-studio-midi-expresion.md` §3, la tabla de CC | "Y habla con Ableton: cada perilla sale por MIDI con su número, así que Ableton la puede grabar." |

## 6 · Clímax: ¿ya sirve para hacer música? (8:30-10:30)

**Variante A** (si en la prueba funcionaron el clock y la automatización). Aplaude al empezar (sincronización).

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T38 | **P6** | Ableton reproduce el loop; el clip MIDI toca el bajo en el Brain | "Ableton toca el bajo en mi synth…" |
| T39 | **P2** | Enciendes LFO SYNC con LFO → cutoff; el bajo "respira" a tempo | "…y le manda el tempo: el LFO se sincroniza solo." |
| T40 🔲V | **P2** + **P6** a la vez | Grabas 8 compases girando ENC L y ENC R; en Ableton se dibuja la automatización. **En edición: pantalla partida** | "Ahora grabo ocho compases moviendo el filtro." |
| T41 🔲V | **P7** + **P6** | Sueltas las manos; reproduces: el cutoff se mueve solo en la pantalla del Brain y en Ableton | "Y ya no lo estoy tocando yo." |
| T42 | **P3** | 15 s de música, el banco con los LEDs, sin manos | — |

**Variante B** (si algo falló): T38 con el bajo tocado desde el teclado y el filtro a mano (P2), y luego:

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T43 | **P6** | El monitor serie con el error | "La sincronización con Ableton hoy no funcionó. Esto es lo que vi. Lo arreglo y te lo muestro más adelante." |

## 7 · Lo que falta (10:30-11:45)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T44 | **P5** | A cámara | "Esto es lo que todavía no está. La primera parte de la temporada es el estudio: encontrar mi sonido y meterlo en tracks." |
| T45 | **P5** | A cámara | "El modo FX, para meter mi set por los efectos del Brain en vivo: la página existe, el audio todavía no." |
| T46 | **P5** | A cámara | "La placa y la caja ya están diseñadas, y vas a ver cómo: el modelado, los esquemáticos, las PCB. [Elige la verdad del día: 'las placas vienen en camino' / 'todavía no las mandé a fabricar'.]" |
| T47 | **P5** | A cámara | "Una revisión encontró que uno de los encoders está en un pin de la Teensy que tiene un LED, y eso puede hacer que falle. Te lo cuento en su episodio." |
| T48 | **P5** | A cámara | "El próximo episodio arranco desde un sonido vacío en el motor del Juno, y no paro hasta tener el bajo y los acordes de mi primer track." |
| T49 | **P6** | La tabla de la temporada (Biblia §3), 3 s | (bajo T48) |

## 8 · Cierre (11:45-12:00)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T50 | **P5** | A cámara | "Si quieres seguir el proceso, abajo está el link para dejarme tu correo. Y cuéntame en los comentarios qué sonido quieres que construya." |
| T51 | **P2** | Tu mano suelta ENC L; el bajo sigue sonando; corte | — |

---

## Packaging

**Título principal:** *Construyo mi propio synth para encontrar mi sonido*
**A/B:** *Estoy construyendo el synth de mis producciones (está en una protoboard)* · *6 synths clásicos en una placa
de 30 dólares (y los toco con Ableton)* · *Dejé de buscar presets: construyo mi propio synth*

**Miniatura:** toma **P3** (frontal 45°) con exposición baja: la protoboard con el anillo de LEDs brillando, la
pantalla PERFORM legible al fondo, tu mano en ENC L. Texto: **"MI PROPIO SONIDO"**. (Saca una foto fija en P3
durante la sesión, es más fácil que sacarla del video.)

**Descripción:** "Estoy construyendo mi propio synth para encontrar mi sonido: 6 motores inspirados en synths
clásicos sobre una Teensy 4.1. Primero para mis producciones, después para mi live set. Episodio 1: qué es, cómo
funciona y si ya sirve para hacer música con Ableton."

---

## Shorts (salen de las mismas tomas)

| Short | Tomas | Cómo se arma | Qué dices (nuevo, grábalo en P5 o en off) |
|---|---|---|---|
| **1 · "Mi synth graba su propia automatización"** (40 s) | T40, T41 (verticales) | Pantalla partida vertical: mano arriba, Ableton abajo | Arranque: "Giro esta perilla… y Ableton la está escribiendo." Cierre: "Cada perilla sale por MIDI con su número. Ableton la graba y después la toca sola." |
| **2 · "El banco" (ASMR)** (25 s) | T1, T26-T31, T42 | Sin voz, solo el audio del Brain; cortes cada 2-3 s | — |
| **3 · "¿Por qué no compro un Moog?"** (45 s) | T10-T15 + T1 | Tú a cámara con cortes al banco | Arranque: "Me dicen: ¿por qué no te compras un Moog?" Cierre: "Porque busco un sonido mío. ¿Suena igual que un Moog? No. Lo medí: te enseño los números en la serie." |

---

## Plan de sesión, ordenado por posición (≈ 2 h 15)

| Orden | Posición | Tomas | Min |
|---|---|---|---|
| 1 | — | Pruebas de "Antes de grabar" (graba P6 del monitor serie por si algo falla) | 15 |
| 2 | **P6** (pantalla del computador) | T13, T15, T32, T37, T49 + el loop de Ableton para el B-roll | 15 |
| 3 | **P1** (cenital general) | T3, T16-T25 (el tour, de corrido y por partes) | 20 |
| 4 | **P2** (cenital cerrado) | T1, T26-T31 (detalles), T33, T35, T39, T51, B-roll de NAV | 20 |
| 5 | **P2 + P6** a la vez | Aplauso, T38, T40 (y T43 si hace falta) | 15 |
| 6 | **P7** (pantalla del Brain) | T2, T33, T34, T36, T41 | 15 |
| 7 | **P3** (frontal 45°) | T5, T42, detalles con LEDs, **foto de la miniatura** | 15 |
| 8 | **P4** (rasante) | B-roll del anillo y la Teensy | 10 |
| 9 | **P5** (a cámara) | T4, T6-T12, T14, T44-T48, T50 + las frases nuevas de los Shorts | 25 |
