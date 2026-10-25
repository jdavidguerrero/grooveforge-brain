# Voz en off — cómo grabarla, y el texto del EP1 y del EP2

> 2026-10-25. El canal es **sin cara** (voz en off sobre las manos y el banco). Este documento reemplaza todas las
> filas **P5** de [`EP01_guion.md`](EP01_guion.md) y [`EP02_guion.md`](EP02_guion.md): lo que ahí se decía a cámara
> aquí va en off, sobre una imagen. Postproducción: [`SESION_1_MANOS_Y_POSTPRODUCCION.md`](SESION_1_MANOS_Y_POSTPRODUCCION.md).

---

## 1 · El método: primero el armado, después la voz

La voz se graba **sobre el video ya armado**, no antes. Así cada frase dura lo que dura su imagen.

1. **Armado en bruto** en Resolve: las tomas buenas en el orden de los bloques de abajo, sin pulir. Un **marcador**
   (tecla M) al inicio de cada bloque, con su número (`B1`, `B2`…).
2. **Mide cada bloque** (la duración entre marcadores) y compárala con la columna **≈ s** de abajo. A ritmo
   tranquilo se dicen unas **2,5 palabras por segundo**. Si el texto no cabe, recorta el texto, no el sonido.
3. **Graba la voz bloque por bloque**, mirando el video (método A, abajo).
4. **Ajuste fino:** mueves cada frase de voz a su sitio y recién ahí recortas la imagen.

> **Regla de oro:** los momentos marcados **🔊 sin voz** son del sonido. No hables encima: es lo que la gente vino a
> escuchar. Si hay algo que decir, va **antes** ("escucha…") o **después**.

### Método A (el recomendado): grabar en Resolve, en la página Fairlight

1. Conecta el SM57 a la interfaz y la interfaz al Mac. En Resolve: **Fairlight → Fairlight → Patch Input/Output**,
   conecta la entrada del micrófono a una pista nueva llamada `VOZ` (mono).
2. Arma la pista (botón **R**) y pon la ganancia en la interfaz: hablando normal, los picos en **−12 dBFS** (nunca en
   rojo).
3. Pon la línea de tiempo al inicio del bloque, **audífonos puestos** (que el audio del video no entre al
   micrófono), y graba. Ves la imagen mientras hablas.
4. Por bloque: **3 tomas seguidas**, diciendo "toma uno", "toma dos"… y 2 s de silencio entre cada una. Después te
   quedas con la mejor.

### Método B: grabar en Ableton

Si prefieres no configurar Fairlight: graba en Ableton una pista de audio por bloque, con el video del armado abierto
en otra ventana (exportado en baja resolución). Exporta `voz.wav` (48 kHz / 24 bit) y lo pones en Resolve. Funciona
igual, pero sincronizar te toma más tiempo.

### El micrófono y la sala

- **SM57 a 5-8 cm de la boca**, con el antipop, un poco de lado (apuntando a la comisura, no de frente: menos "p" y
  "s"). Es un micrófono dinámico: de cerca suena con cuerpo y casi no capta la sala.
- **Habitación callada:** sin ventilador ni aire acondicionado, el celular en silencio. Una cobija o un clóset abierto
  detrás del micrófono quitan eco.
- **Antes de la primera toma:** 10 s de silencio grabado ("tono de sala"). Sirve para tapar huecos en la edición.
- **Agua a temperatura ambiente**, nada de lácteos antes.

### Cómo decirlo

- **Háblale a una persona**, como si le explicaras el synth a un amigo en tu estudio. No es una locución de radio.
- Los textos de abajo son una guía: **dilos con tus palabras**, pero mantén los números exactos.
- Lee el bloque en voz alta una vez antes de grabarlo. Si una frase se te traba, cámbiala ahí mismo.
- Sonríe un poco al hablar: se nota en la voz.
- Si te equivocas, no pares: haz una pausa y repite la frase entera. Se corta en la edición.

### Después de grabar (en Fairlight, sobre la pista VOZ)

Lo mismo que en la postproducción §5: pasa altos en 80 Hz, compresor suave 3:1, de-esser si las "s" pican. La música,
**12-18 dB por debajo** de la voz cuando hablas, y arriba cuando suena sola. Mezcla final: −14 LUFS, picos por debajo
de −1 dBTP.

---

## 2 · EP1 — "Construyo mi synth para encontrar mi sonido"

**Imagen** = qué tomas van debajo (números del guion). **≈ s** = duración aproximada del texto.

| Bloque | Imagen | Qué dices | ≈ s |
|---|---|---|---|
| **B1 · Cold open** | T1-T3: la mano abre el filtro; el banco completo | 🔊 sin voz los primeros 6-8 s. Después: "Este es mi sintetizador. Lo estoy construyendo para encontrar mi sonido. Y cuando esté listo, va a salir de esta protoboard: a mis tracks y a mi live set." | 12 |
| **B2 · La promesa** | B-roll: NAV con el anillo, LEDs de cerca, Ableton con el loop | "Produzco música electrónica. Y llevo tiempo buscando un sonido que sea mío, no el preset que usa todo el mundo. Además, el sonido siempre está en la computadora, y mis manos en un mouse. Así que decidí construir mi propio instrumento: entenderlo por dentro, y poder cambiarlo." | 18 |
| **B3 · La serie** | El banco desde el trípode, las manos tocando | "Esta serie es el proceso, completo. En cada episodio construyo una parte, te muestro cómo funciona por dentro, y la uso en un track mío. Primero en el estudio; después, cuando esté en su caja, en vivo. Si algo falla, también lo vas a ver. Hoy: qué es esta cosa, y si ya sirve para hacer música." | 20 |
| **B4 · Por qué construirlo** | Macros de la Teensy y los encoders; T13 y T15 (pantalla) donde se nombran | "¿Por qué no compro un synth y ya? Cinco razones. Uno: quiero un sonido mío; si lo construyo, lo puedo moldear hasta que suene como yo. Dos: quiero perillas; producir es usar las manos. Tres: quiero sonidos clásicos sin pagar lo que cuestan; adentro hay seis motores, inspirados en el Model D, el Juno-106, el Prophet-5, el OB-6, el DX7 y el ARP 2600. Cuatro: quiero entender mi instrumento; si sé por qué suena así, lo puedo cambiar. Y cinco, la honestidad: todavía no suenan igual que los originales. Lo medí, y lo vas a ver en esta serie." | 40 |
| **B5 · Tour del banco** | T16-T25: el dedo señalando, con las **etiquetas** congeladas (`media/video/etiquetas/t1ep01/`) | Las frases de T16-T25 del guion, una por pieza, cada una cuando el dedo llega. La más importante, sobre la mano en la Teensy: "El sonido no pasa por la Pi. Los controles y el audio van directo a la Teensy. La pantalla solo dibuja lo que la Teensy le cuenta: si la Pi se cuelga, el synth sigue sonando." | 60-75 |
| **B6 · Lo que ya hace** | T33-T37 | Las frases de T33-T37 del guion. 🔊 Entre "Moog…", "Juno…" y "DX7" deja sonar cada motor 2 s sin voz. | 35 |
| **B7 · Clímax** | T38-T42 (Ableton + manos) | "Ableton toca el bajo en mi synth, y le manda el tempo: el LFO se sincroniza solo. Ahora grabo ocho compases moviendo el filtro…" 🔊 los 8 compases sin voz. Después: "…y ya no lo estoy tocando yo." 🔊 T42 sin voz. | 15 + música |
| **B8 · Lo que falta** | B-roll lento: rasante del anillo, el monitor, la tabla de la temporada (T49) | "Esto es lo que todavía no está. La primera parte de la temporada es el estudio: encontrar mi sonido y meterlo en tracks. El modo FX, para pasar mi set por los efectos del Brain en vivo: la página existe, el audio todavía no. La placa y la caja ya están diseñadas, y vas a ver cómo. Y una revisión encontró que uno de los encoders está en un pin de la Teensy que tiene un LED, y eso puede hacer que falle. Te lo cuento en su episodio." | 30 |
| **B9 · El próximo** | T2 (pantalla del Brain) o la pantalla en INIT | "En el próximo episodio arranco desde un sonido vacío en el motor del Juno, y no paro hasta tener los acordes de mi primer track." | 8 |
| **B10 · Cierre** | T51: la mano suelta ENC L, el bajo sigue | "Si quieres seguir el proceso, suscríbete. Y cuéntame en los comentarios qué sonido quieres que construya." 🔊 4 s de música y corte. | 8 |

**Total de voz:** ≈ 4-5 min en un video de 10-12. El resto es sonido y manos, y está bien así.

**Cambios respecto al guion:** B9 ya no promete el bajo (el EP2 quedó solo con los acordes; ver abajo). B8: elige
qué dices de las placas según la verdad del día ("vienen en camino" / "todavía no las mandé a fabricar"). B10: el link
del correo, solo cuando exista la lista; mientras tanto, "suscríbete".

---

## 3 · EP2 — "Del INIT al acorde: encontrando mi sonido en el Juno"

Lo que grabaste: GroovePilot genera los acordes → eliges y editas → motor Juno → OSC → ENV → LFO → efectos →
**chorus** → el sonido final sonando. El bajo queda para otro episodio. Si en la toma no hiciste algún paso (por
ejemplo el HPF), salta su bloque.

| Bloque | Imagen | Qué dices | ≈ s |
|---|---|---|---|
| **B1 · Cold open** | El sonido final sonando (cenital o trípode), 6 s; **corte seco** a la pantalla en INIT con el acorde plano | 🔊 el final sin voz. Sobre el INIT: "Así termina este episodio. Y así empieza: con un sonido vacío. Hoy busco el sonido de los acordes de mi primer track, en un solo motor: el inspirado en el Juno-106." | 12 |
| **B2 · El track** | OBS: Ableton con la batería sonando | "El track va a [tempo], en [tonalidad]. Ya tengo la batería. Le faltan unos acordes que respiren." | 8 |
| **B3 · La idea: GroovePilot** | OBS: GroovePilot generando; **rótulo púrpura** `ia-01` todo el segmento | "Para los acordes le pedí ideas a GroovePilot, una herramienta de IA en la que trabajo. Le di la tonalidad y el tempo, y me propuso progresiones." 🔊 deja sonar cada opción 2-4 s. | 12 + música |
| **B4 · La decisión** | OBS: eliges una y la editas en el piano roll (que se vea el mouse) | "Me quedo con esta. Le cambio esto… y esto. [Di qué cambiaste y por qué: una nota, un voicing, un acorde más largo.] La IA me dio un punto de partida. El sonido y la decisión son míos. Y ahora, los toco con mi synth." | 15 |
| **B5 · Por qué el Juno** | SHIFT + NAV push → ENGINE SELECT → Juno; INIT sonando | "Elegí este motor porque el Juno original es simple: un oscilador, un sub, un filtro, un chorus. Con poco tienes que decidir. Punto de partida: una sierra con el filtro abierto. Suena a manual." 🔊 2 compases del INIT. | 14 |
| **B6 · OSC** | B1: WAVE, RANGE, SUB (cenital + pantalla) | "Sierra y pulso juntos: más ancho para acordes." 🔊 2 compases. Opcional, sobre el valor en pantalla: "El oscilador está hecho para no generar *aliasing*, esos tonos falsos en los agudos. Lo medí: bajó de menos 23 a menos 70 decibeles." | 6 (+10) |
| **B7 · ENV** | B2: attack lento, release largo | "El acorde entra despacio y se queda. Que respire." 🔊 2 compases. | 5 |
| **B8 · Filtro** | ENC L / ENC R, cutoff a la mitad | "El filtro a la mitad: que no tape al bajo de abajo, ni al lead que va a venir arriba." 🔊 2 compases. | 6 |
| **B9 · HPF** (si lo hiciste) | SHIFT en OSC: HPF 1-2 | "Y le quito graves: eso es trabajo del bajo." 🔊 2 compases. | 4 |
| **B10 · LFO** | B3: LFO lento al cutoff, SYNC | "Un movimiento lento, al tempo de Ableton. Ahora el acorde se mueve con el track." 🔊 2 compases. | 6 |
| **B11 · Efectos** | B4: SYNTH FX, TAPE y REVERB | "Un poco de cinta, y de reverb. Los cuatro efectos juntos usan alrededor del 6 % del procesador." 🔊 2 compases. | 7 |
| **B12 · El chorus** (el momento del episodio) | SHIFT en OSC: CHORUS OFF → I → II → I+II | **Antes:** "Ponte audífonos para esto. El sonido por el que el Juno es famoso: el chorus." Después, solo los nombres, justo antes de cada cambio: "Apagado…" 🔊 "Uno…" 🔊 "Dos…" 🔊 "Los dos." 🔊 | 8 + música |
| **B13 · Cómo funciona el chorus** | El chorus alternando OFF ↔ I+II (T34); o el dibujo de las dos líneas de retardo | "El Juno original tiene dos líneas de retardo, una a la izquierda y otra a la derecha, y un LFO que las mueve en sentidos opuestos. Por eso el sonido se abre en estéreo. Al principio, el mío era mono. Lo medí, lo arreglé, y ahora es estéreo." | 18 |
| **B14 · Guardar** | SHIFT + B4 → PRESETS → SAVE | "Lo guardo. Todavía no le puedo poner nombre desde el panel: está en la lista de pendientes." | 6 |
| **B15 · El resultado** | El sonido final sonando, tus manos en el filtro; los B-roll | 🔊 **sin voz, 20-30 s.** Es el clímax. | 0 |
| **B16 · Lo que aprendí** | B-roll lento: rasante del anillo, la pantalla del Brain | "Un motor, un sonido, y ya suena a mí. Lo que aprendí: con un motor simple decides más rápido. Y el Brain toca un motor a la vez, como un synth de hardware: grabas el sonido, y sigues con el siguiente." | 14 |
| **B17 · Cierre** | La mano suelta el encoder, el acorde sigue; pantalla final | "[El próximo episodio: según lo que grabes después, el bajo o la caja del synth.] Si quieres seguir el proceso, suscríbete. Y dime en los comentarios qué motor quieres que pruebe después." 🔊 4 s y corte. | 10 |

**Total de voz:** ≈ 3 min en un video de 7-9 min.

**Rótulo de la IA** (`media/video/tercios-inferiores/t1ep02/ia-01-…`): todo el tiempo que se vea GroovePilot (B3 y
B4). Es la regla de ADR-002: la IA siempre rotulada, y la decisión es tuya.

**Packaging, ajustado a lo grabado:**
- **Título:** *Del INIT al acorde: encontrando mi sonido en el Juno*
- **A/B:** *El chorus que hizo famoso al Juno-106, en mi synth casero* · *Le pedí acordes a una IA. Me quedé con uno y
  lo cambié*
- **Shorts:** 1 · "El chorus del Juno" (B12 + B13, con "🎧 con audífonos" al inicio) · 2 · "De INIT a acorde en 30 s"
  (B5-B11, 3 s por paso, el nombre del cambio escrito en pantalla, sin voz) · 3 · "Le pedí acordes a una IA" (B3, B4
  y el resultado).

---

## 4 · Orden de trabajo sugerido

1. Armado en bruto del **EP1** con marcadores B1-B10 → grabar la voz del EP1 (≈ 45 min, con las 3 tomas por bloque).
2. Lo mismo con el **EP2**, B1-B17 (≈ 30 min).
3. Las dos voces en la misma sesión si puedes: así suenan igual (misma distancia, misma sala, misma ganancia). Anota
   la ganancia de la interfaz y la distancia al micrófono para repetirlas en los próximos episodios.
4. Mándame la lista de marcadores con las duraciones reales: ajusto los textos que no quepan.
