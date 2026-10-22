# EP1 — "Construyo mi synth para encontrar mi sonido" · guion para grabar

> Temporada 1, piloto. Duración objetivo: 11-12 min. Biblia: [`BIBLIA_T1_LIVESET.md`](BIBLIA_T1_LIVESET.md).
> Todo lo que aparece aquí existe hoy en el banco (foto del 2026-10-22: protoboard con la Teensy 4.1 + Audio Shield,
> 5 encoders, anillo de 12 LEDs, cabeza Pi 3B + 5" en PERFORM con el Model D a 800 Hz / RES 15, teclado MIDI,
> osciloscopio de kit, fuente de banco).
>
> Convenciones: **VO** = voz (en off o a cámara). **[A]** cenital macro · **[B]** banco/medición · **[C]** pantalla
> del computador · **[E]** audio directo · **[F]** a cámara · **[V]** vertical para Shorts.
> Los textos de VO son una guía: dilos con tus palabras, mantén los números.

---

## Antes de grabar (15 min)

- [ ] `pio run -e sketch49 -t upload` con el firmware del día; anotar el commit.
- [ ] Probar **en este orden**: teclado → nota; ENC L mueve el cutoff en PERFORM; Ableton con External Instrument
      (MIDI To = Teensy MIDI) toca el Brain; con el clock de Ableton, **LFO SYNC** sigue el tempo (en el monitor
      serie aparece `[UI] LFO SYNC ON (xxx BPM)`); el cutoff que giras aparece como automatización en Ableton.
- [ ] Si alguna de estas falla: **eso es contenido** (grábalo), pero el clímax se cambia por la variante B (§Clímax).
- [ ] LINE OUT → interfaz → pista de audio en Ableton, armada y grabando durante toda la sesión.
- [ ] Limpiar el banco solo lo justo: los cables quedan. La protoboard llena de cables **es** la imagen del canal.
- [ ] Un loop de Ableton listo: kick + hats + un clap, 120-126 BPM, 8 compases, en la tonalidad del bajo que vas a
      tocar. Una clip MIDI con una línea de bajo de 2 compases.

---

## 0:00-0:15 · Cold open

| Tiempo | Imagen | Audio |
|---|---|---|
| 0:00 | **[A]** macro de la mano en ENC L; la pantalla en PERFORM, el cutoff subiendo de 800 Hz y la onda abriéndose | Bajo del Model D sobre el loop, el filtro abriéndose **[E]** |
| 0:04 | Plano general del banco (como la foto) | Sigue el loop |
| 0:06 | **[F]** a cámara, el banco detrás | VO: *"Este es mi sintetizador. Lo estoy construyendo para encontrar mi sonido…"* |
| 0:11 | Vuelta al macro del encoder | VO: *"…y cuando lo tenga, va a salir de esta protoboard, a mis tracks y a mi live set."* |

## 0:15-0:50 · La promesa

**[F]** VO:
> "Produzco música electrónica. Y llevo tiempo buscando un sonido que sea mío, no el preset que usa todo el mundo.
> Además, el sonido siempre está en la computadora y mis manos en un mouse. Así que decidí construir mi propio
> instrumento: entenderlo por dentro y poder cambiarlo.
>
> Esta serie es el proceso, completo y abierto. En cada episodio construyo una parte, te muestro cómo funciona por
> dentro, con el código y las mediciones, y la uso en un track mío. Primero en el estudio, para encontrar mi sonido;
> después, cuando esté terminado y en su caja, en mi live set. Si algo falla, también lo vas a ver. Hoy: qué es esta
> cosa y si ya sirve para hacer música."

**B-roll** (sobre la VO): [A] el anillo de LEDs girando con NAV · [A] las teclas del teclado MIDI · [C] Ableton
con el loop.

## 0:50-3:00 · Por qué construirlo y no comprarlo

**[F]** + B-roll. Puntos (con tus palabras):
1. **Quiero un sonido mío.** Si lo construyo, lo puedo moldear hasta que suene como yo, no como un preset.
2. **Quiero perillas, no el mouse.** Producir y tocar es usar las manos: filtro, resonancia, envolventes.
3. **Quiero sonidos clásicos sin pagar lo que cuestan.** El Brain tiene 6 arquitecturas inspiradas en el Model D, el
   Juno-106, el Prophet-5, el OB-6, el DX7 y el ARP 2600 ("inspiradas en": son marcas de otros, ADR-002 de contenido). **[C]** `apps/firmware-teensy/src/engines/` (solo la lista de
   archivos).
4. **Quiero entender mi instrumento.** Si sé por qué suena así, lo puedo cambiar. Y no lo hago para venderlo: lo
   hago para mi música, y el proceso lo comparto completo.
5. **La honestidad:** "No suenan igual que los originales todavía. Lo medí, y lo vas a ver en el episodio 2."
   **[C]** `apps/docs/theory/fidelidad-engines.md` §0, la tabla (2 s, sin detenerse).

## 3:00-6:00 · Tour del banco

**[A]/[B]**, señalando cada cosa. Hablar mientras se señala; una frase por pieza.

| Señalas | VO (guía) | En pantalla [C] |
|---|---|---|
| La Teensy 4.1 | "El cerebro: un microcontrolador ARM Cortex-M7 a 600 MHz. Aquí corren los 6 synths y los efectos." | — |
| El Audio Shield | "La tarjeta de sonido: un codec SGTL5000. Sale por esta salida de línea a mis monitores." | — |
| ENC L / ENC R | "Cutoff y resonancia, siempre a mano. Es lo que más toco en vivo." | — |
| P1 / P2 | "Dos perillas que cambian según la página: envolventes, LFO, osciladores." | — |
| NAV + anillo | "Navegación. El anillo me dice dónde estoy sin mirar la pantalla." | — |
| B1-B6 | "Botones de páginas, el modo y SHIFT." | `apps/docs/adr/ADR-004-panel-studio.md` tabla de mapeo |
| La cabeza (Pi + 5") | "Y esta pantalla es una Raspberry Pi con Linux." | — |
| — | **La regla:** "Pero el sonido no pasa por la Pi. Los controles y el audio van directo a la Teensy; la pantalla solo dibuja lo que la Teensy le cuenta. Si la Pi se cuelga, el synth sigue sonando." | ADR-004 §1 (frase subrayada) |
| El cable USB-A | "Aquí entra el teclado MIDI, por el puerto USB host de la Teensy." | — |
| El osciloscopio de kit | "Y esto lo voy a usar mucho: para ver lo que oímos." | — |

Diagrama en pantalla (dibújalo o anímalo, sale del encabezado del sketch49):

```
teclado (USB host) ─┐
Ableton (USB) ──────┼─► Teensy 4.1: engine activo → SYNTH FX → SGTL5000 → LINE OUT
panel (encoders) ───┘            │
                                 └─ UART 921600 ─► Pi 3B + 5" (solo dibuja)
```
**[C]** `apps/firmware-teensy/src/sketches/49-studio-brain.cpp`, líneas 1-35 (el comentario de cabecera).

## 6:00-8:30 · Lo que ya hace

Ritmo rápido: cada punto, 20-30 s, con sonido.

1. **Cambiar de synth.** SHIFT + NAV push → ENGINE SELECT; mismo acorde en Model D → Juno → DX7. **[E]**
2. **La pantalla PERFORM.** "La onda que ves se calcula con el cutoff y la resonancia que estoy usando: si cierro el
   filtro, se redondea; si subo la resonancia, aparece el ringing." Mover ENC L y ENC R. Aclarar: *"es una
   ilustración, no el audio medido"* (theory/08 §3).
3. **Efectos del synth.** B4 → SYNTH FX: tape y reverb encendidos. "Cuatro efectos fijos, en el orden de un
   pedalboard, y entre los cuatro usan alrededor del 6 % del procesador." **[C]** theory/05 §2, la tabla.
4. **Presets.** SHIFT + B4 → PRESETS, cargar uno. "16 por engine, y sobreviven al apagado."
5. **Habla con Ableton.** "Cada perilla sale por MIDI. Ableton la graba como automatización." **[C]** theory/07 §3,
   la tabla de CC.

## 8:30-10:30 · Clímax: ¿ya sirve para hacer música?

**Variante A (si el clock y la automatización funcionaron en la prueba):**
1. **[C]+[A]** pantalla partida: Ableton reproduce el loop; la clip MIDI toca el bajo en el Brain.
2. VO: *"Ableton le manda el tempo, y el LFO del Brain se sincroniza solo."* Encender LFO SYNC, LFO → cutoff: el bajo
   "respira" a tempo. **[E]**
3. Grabar 8 compases tocando ENC L y ENC R a mano. **[E]**
4. Soltar las manos y reproducir: en Ableton aparece la curva; en la pantalla del Brain el cutoff se mueve solo. VO:
   *"Ya no lo estoy tocando yo."*
5. 15 s de música sin voz.

**Variante B (si algo falló):** el loop de Ableton + el bajo tocado desde el teclado MIDI, filtro a mano. Y contarlo:
*"La sincronización con Ableton no funcionó hoy: esto es lo que vi."* Mostrar el monitor serie. Eso se arregla en el
EP4.

## 10:30-11:45 · Lo que falta y lo que viene

**[F]** VO (honesto, sin dramatizar):
> "Esto es lo que todavía no está:
> — La primera parte de la temporada es el estudio: encontrar mi sonido y meterlo en tracks.
> — El **modo FX**, para meter todo mi set por los efectos del Brain en vivo: la página existe, el audio todavía no.
> — La **placa y la caja**: los PCBs están diseñados, pero no los mandé a fabricar. Una revisión encontró que uno de
>   los encoders está en un pin de la Teensy que tiene un LED, y eso puede hacer que falle. Te lo cuento en su
>   episodio.
> — Y los **tracks**: cada episodio deja una parte del set.
>
> El próximo episodio: los 6 synths, cuál es el sonido de mi primer track, y cuál todavía no suena como debería."

**[C]** la tabla de la temporada (§3 de la Biblia), los 9 episodios, 3 segundos.

## 11:45-12:00 · Cierre

**[F]** CTA único: *"Si quieres seguir el proceso, abajo está el link para dejarme tu correo. Y cuéntame en los
comentarios qué sonido quieres que construya."*
Último plano: [A] la mano soltando ENC L, el bajo sigue sonando, corte.

---

## Packaging

**Título principal:** *Construyo mi propio synth para encontrar mi sonido*
**A/B:**
1. *Estoy construyendo el synth de mis producciones (está en una protoboard)*
2. *6 synths clásicos en una placa de 30 dólares (y los toco con Ableton)*
3. *Dejé de buscar presets: construyo mi propio synth*
4. *Construyendo mi synth, episodio 1: encontrar mi sonido*

**Miniatura:** encuadre cerrado de tu foto: la protoboard en primer plano (los cables y el anillo azul encendido) y la
pantalla con PERFORM a la derecha, el "800 Hz" legible. Tu mano en ENC L. Fondo oscurecido para que el anillo y la
pantalla brillen. Texto (máx. 3 palabras): **"MI PROPIO SONIDO"**. Alternativa: **"HECHO A MANO"**.

**Descripción (primeras 2 líneas):** "Estoy construyendo mi propio synth para encontrar mi sonido: 6 motores
inspirados en synths clásicos sobre una Teensy 4.1. Primero para mis producciones, después para mi live set.
Episodio 1: qué es, cómo funciona y si ya sirve para hacer música con Ableton."

---

## Shorts del EP1 (grabar en [V] durante las mismas tomas)

### Short 1 · Mind-blow — "Mi synth graba su propia automatización" (35-45 s)
- **0-3 s:** pantalla partida: mano en la perilla | curva dibujándose en Ableton. VO: *"Giro esta perilla y Ableton
  la está escribiendo."*
- **3-30 s:** cada control del Brain manda su número de CC por USB; Ableton lo graba como automatización y después se
  lo devuelve. Truco: el Brain ignora su propio eco 150 ms, si no, el filtro se trababa en 83 Hz.
- **30-40 s:** reproducción, manos fuera, la pantalla del Brain moviéndose sola.
- **Archivos:** `variants/studio/theory/07-studio-midi-expresion.md` §3; commit `a3c588c`.
- Depende de la variante A del clímax.

### Short 2 · Build/ASMR — "El banco" (20-30 s, sin voz)
- **0-3 s:** macro del anillo de LEDs azul girando con NAV, el primer golpe del bajo.
- **3-25 s:** encoder → filtro abriéndose → pantalla PERFORM → teclas → plano general del banco. Solo el audio del
  Brain sobre el loop.
- Texto final: "Episodio 1 en el canal".

### Short 3 · Polémica — "¿Por qué no compro un Moog?" (40-50 s)
- **0-3 s:** a cámara, la protoboard en la mano. VO: *"Me dicen: ¿por qué no te compras un Moog?"*
- **3-35 s:** "Porque busco un sonido mío, y para eso lo quiero tocar, entender y cambiar. Esta placa cuesta unos 30 dólares y tiene 6 synths clásicos
  adentro. ¿Suena igual que un Moog? No. Lo medí: te enseño los números en el episodio 2."
- **35-45 s:** el bajo del Model D sonando, filtro abriéndose.
- **Archivos:** `theory/fidelidad-engines.md` §0.
- **Ojo:** no afirmar precios de los originales sin la fuente del día; no decir "suena igual".

---

## Plan de sesión (≈ 2 h 15 min)

| # | Setup | Toma | Para | Min |
|---|---|---|---|---|
| 1 | — | Pruebas de "Antes de grabar" (grabar en [C] el monitor serie por si falla algo) | clímax B | 15 |
| 2 | A+E+V | Mano en ENC L, cutoff abriendo, sobre el loop (5 tomas) | cold open, Short 2 | 10 |
| 3 | A+V | Anillo de LEDs, teclas, NAV, B1-B6 | B-roll, Short 2 | 10 |
| 4 | A+B | Tour señalando cada pieza (una pasada continua y otra por partes) | 3:00-6:00 | 20 |
| 5 | A+E | Engines, PERFORM, SYNTH FX, presets | 6:00-8:30 | 15 |
| 6 | C+A+E+V | Clímax: Ableton + Brain, grabar la toma, reproducir | clímax, Short 1 | 25 |
| 7 | C | Capturas: sketch49 cabecera, ADR-004, theory/05, 07, 08, fidelidad §0, `src/engines/` | todos | 15 |
| 8 | F+V | Cold open a cámara, promesa, por qué, lo que falta, cierre, Short 3 | todos | 25 |
