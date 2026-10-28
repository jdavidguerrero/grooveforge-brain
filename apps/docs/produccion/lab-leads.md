# Lab de leads: escalas, ritmos y sonido

> **Fecha:** 2026-10-28 (rev. 3: dialectos) · **Estado:** propuesta para Juan; el generador, los
> presets y los contextos D / E esperan su visto bueno · **Depende de:** `sesion-paleta-adn.md` §1b y §8,
> `protocolo-captura.md` §3 y §5, `ia-local-refinamiento.md` §2 · **Ficha:** `metadata/sesiones/2026-10-28_lead.json`

## 0. Lo que decidiste

| Tema | Decisión de Juan |
|---|---|
| Escalas | Experimentar con varias: húngara, hirajōshi, lidia, in sen y otras. **No hace falta quedarse en La menor natural**: explorar mayor y menor, y acordes prestados |
| Ritmo | Las rítmicas de §4 sí gustan. Melodías con notas **cortas, largas y mixtas** |
| "Notas largas" | No es una regla del ADN (venía de los referentes): se corrige en `sesion-paleta-adn.md` §1b |
| Sonido | Revisar otros presets de Prophet y OB-6 además de DESERT LEAD y GF MELODIC LEAD |
| ADN y dialectos (rev. 3) | El sonido tiene que cruzar subgéneros; escalas y rítmicas pueden ser de un solo dialecto. Melodic se separa de organic e indie con su propio contexto (F). Aplica a todos los roles (`sesion-paleta-adn.md` §1c) |

Consecuencia: en este lab **cada escala trae su propia progresión** (siempre con La como centro, 124 BPM, 4 compases).
Una escala solo se oye bien encima de acordes que la sostienen. El resto de la paleta sigue sobre Am – F – C – G.

## 1. Qué pasó en el check y por qué

Ningún lead pasó: "muy orientales y extendidos"; sin percusión, "notas demasiado largas y estridentes". Las causas:

| Lo que oíste | Causa en los patrones (`make_adn_clips.py`) | Causa en los presets (`preset_bank.cpp`) |
|---|---|---|
| **Extendidos** | `lead_adn_frase` y `lead_adn_profunda` llenan los 4 compases: **cero silencios** | Sustain 0,7–0,8 y release 350–400 ms: cada nota se pega a la siguiente |
| **Orientales** | `frase` sube y baja la pentatónica por grados, con notas largas y agudas: el cliché de la pentatónica | GF MELODIC LEAD trae vibrato fijo (LFO 5,5 Hz → pitch); la capa A de DESERT LEAD, un notch que barre lento |
| **Estridentes** | Registro La4–La5: los armónicos caen en 2–5 kHz, donde el oído es más sensible | Ataque de 3–5 ms en sierras, cutoff 1,8–2,5 kHz, **sync** en la capa B (Prophet), unison ×2 con 18 ¢ |

Lo "oriental" no estaba en la escala (la pentatónica no tiene nada exótico): estaba en **cómo** se tocaba. Por eso
probamos muchas escalas exóticas, pero con la rítmica de club de §4.

## 2. Escalas: qué es un modo y qué es "exótico"

Todas en La. Los grados se cuentan desde La (1 = La); en **negrita**, lo que cambia respecto de la menor natural.

Dos ideas para leer la tabla:

- **Lo que define el carácter es la nota que distingue a la escala**, no la escala entera. La dórica es la menor
  natural con **un** cambio (Fa#); la lidia es la mayor con **un** cambio (Re#). Si esa nota no suena, no se oye el
  modo (Persichetti, cap. 2; Hewitt, cap. de escalas y modos).
- **Lo "oriental" viene de la segunda aumentada** (un salto de 3 semitonos entre notas vecinas de la escala, como Fa →
  Sol#) **y del semitono sobre la tónica** (La → Si♭). Cuantas más veces aparecen, más duran y más fuerte caen, más
  exótico suena.

### Familia 1 · menores (el ADN de siempre, con variantes)

| Escala | Notas | Grados | Progresión del lab | Carácter |
|---|---|---|---|---|
| Eólica (menor natural) | La Si Do Re Mi Fa Sol | 1 2 ♭3 4 5 ♭6 ♭7 | Am – F – C – G | la base: oscura por el Fa (♭6) |
| Dórica | La Si Do Re Mi **Fa#** Sol | 1 2 ♭3 4 5 **6** ♭7 | Am – **D** – C – G | menor pero con luz: el soul del house y del deep house |
| Menor con **V prestado** | eólica, y **Sol#** sobre el E | — | Am – F – C – **E** | la vuelta "empuja" hacia el Am: el remate exótico sin choque |

### Familia 2 · exóticas oscuras

| Escala | Notas | Grados | Progresión del lab | Carácter |
|---|---|---|---|---|
| Frigia | La **Si♭** Do Re Mi Fa Sol | 1 **♭2** ♭3 4 5 ♭6 ♭7 | Am – **B♭** – Am – Gm | oscura y tensa; el semitono La–Si♭ (indie tech, techno melódico) |
| Frigia dominante | La **Si♭ Do#** Re Mi Fa Sol | 1 **♭2 3** 4 5 ♭6 ♭7 | **A** – B♭ – Gm – A | flamenco / árabe: una segunda aumentada (Si♭ → Do#) |
| Menor húngara | La Si Do **Re# Mi** Fa **Sol#** | 1 2 ♭3 **♯4** 5 ♭6 **7** | Am – F – Am – **E** | la más "gitana": **dos** segundas aumentadas (Do → Re#, Fa → Sol#) |

### Familia 3 · japonesas (pentatónicas con semitonos)

Escalas de cinco notas con semitonos: a diferencia de la pentatónica menor, tienen choque y color propio. Los nombres
y las versiones varían entre fuentes; uso la versión más común en la teoría occidental (Burrows; Hewitt).

| Escala | Notas | Grados | Progresión del lab | Carácter |
|---|---|---|---|---|
| Hirajōshi | La Si Do Mi Fa | 1 2 ♭3 5 ♭6 | Am – F – Am – Em | cabe en La menor natural: exótica **por lo que quita** (sin Re ni Sol). Hipnótica |
| In sen | La **Si♭** Re Mi Sol | 1 **♭2** 4 5 ♭7 | Asus4 – B♭ – Asus4 – Gm | sin tercera: ni mayor ni menor. Suspendida, misteriosa |
| Kumoi | La Si Do Mi **Fa#** | 1 2 ♭3 5 **6** | Am – D – Am – D | la "dórica japonesa": más clara que la hirajōshi |

### Familia 4 · mayores y brillantes

| Escala | Notas | Grados | Progresión del lab | Carácter |
|---|---|---|---|---|
| Lidia | La Si Do# **Re#** Mi Fa# Sol# | 1 2 3 **♯4** 5 6 7 | A – **B** – F#m – B | flotante, de sueño (progressive, melodic) |
| Mixolidia | La Si Do# Re Mi Fa# **Sol** | 1 2 3 4 5 6 **♭7** | A – **G** – D – A | mayor con calle: disco, funk, house |
| Mayor con **iv prestado** | jónica; sobre el Dm, la menor paralela (con **Fa** natural) | — | A – D – **Dm** – A | alegre que se vuelve nostálgica en un compás |

Truco que no necesita progresión nueva: dentro de Am – F – C – G, si la melodía apoya un **Si sobre el acorde de F**,
ese compás suena **lidio** (Fa lidio tiene las mismas notas que La menor). Un modo también es *dónde* pones el acento.

## 3. Acordes prestados (intercambio modal)

Prestar un acorde es tomarlo de la escala paralela (La mayor ↔ La menor, misma tónica) por un compás. Es la forma más
simple de mezclar mayor y menor sin cambiar de tonalidad (Kostka & Payne, *Tonal Harmony*, cap. "Mode Mixture").

| Base | Acorde prestado | De dónde viene | Qué hace | En el lab |
|---|---|---|---|---|
| La menor | **E** (V mayor) | menor armónica | el Sol# empuja a La: cierre fuerte, de club | Familia 1 · Am – F – C – E |
| La menor | **D** (IV mayor) | dórica / La mayor | un rayo de luz en lo oscuro | Familia 1 · dórica |
| La menor | **A** (I mayor al final) | La mayor | la "tercera picarda": resolver feliz | para después (variación de remate) |
| La mayor | **Dm** (iv menor) | La menor | melancolía dentro de lo alegre | Familia 4 |
| La mayor | **F** y **G** (♭VI, ♭VII) | La menor | subida épica a la tónica (A – F – G – A) | para después |

**Regla para la melodía:** sobre el acorde prestado, la melodía usa **sus** notas (Sol# sobre el E, Fa sobre el Dm).
Así lo exótico (el Sol# de la húngara, por ejemplo) aparece **consonante**, sostenido por la armonía, en lugar de
chocar. Es la manera más limpia de "manejar" el color: el acorde lo pide.

## 4. Ritmo: cortas, largas y mixtas

En un track de club el lead compite con el bajo y la percusión por el mismo espacio. Un gancho se recuerda **primero
por su ritmo** y después por sus notas (Snoman, *Dance Music Manual*, cap. de melodía y riffs; Hooktheory I, cap. de
ritmo melódico). Principios: motivo de 1–2 compases que se repite (A A A B), silencios para que respire la percusión,
síncopa (entrar en la "y", anticipar el 1), notas largas o de tiempo fuerte en tonos del acorde, registro centrado en
La3–Mi5 y velocity con intención (cortas 70–85, acentos 95–105, ghosts 50).

Cuatro rítmicas (x = corta, X = acento, `=` = la nota sigue, `.` = silencio). Las tres de la rev. 1 te gustaron; se
agrega la **corta** para tener los tres tipos:

```
             1 . . . 2 . . . 3 . . . 4 . . .
corta        X . x . . x . x . . X . x . . .     solo cortas, 3-3-2 como pluck-lead; groove puro
mixta A      . . x x . . X = = . x . x . . .     "gancho": corta-corta-LARGA y respuesta corta; 1 compás
mixta B      x . x X = = = . . . . . . . . .     "pregunta" en el compás 1…
             . . . . . . x . x . x X = = = .     …"respuesta" en el 2; 2 compases
larga        X = = = = = . . . x . x . . . .     "profunda": una larga grave que dos cortas empujan; octava abajo
```

Cómo se arma cada clip: **la misma rítmica y el mismo contorno** (sube, baja, salta) en todas las escalas; las alturas
salen de la escala, y las notas largas y las que caen en la negra se ajustan al tono del acorde más cercano. Así, al
comparar dos escalas, lo único que cambia es el color.

**Dosis** (para la escala que gane): la misma frase con el color **completo**, **solo en el remate** (la nota que
distingue a la escala aparece una vez, al cerrar la vuelta) y **de paso** (corta, en tiempo débil, resuelve a la nota
de al lado). Es lo que propuse en la rev. 1 para la húngara, ahora aplicable a cualquier escala.

## 5. Sonido: menos estridencia, y qué presets probar

"Estridente" es casi siempre **demasiada energía entre 2 y 5 kHz** sostenida en el tiempo: ahí el oído es más
sensible (curvas de igual sonoridad, ISO 226).

| Causa | Qué hacer (parámetro del Brain) |
|---|---|
| Cutoff alto y fijo | bajar `cutoff_hz` a ~1,0–1,4 kHz y que el brillo lo dé la **envolvente del filtro** (`env_amt` alto, `f_decay` 150–250 ms, `f_sustain` bajo): las cortas "hablan", las largas se redondean solas |
| Resonancia | `reso` ≤ 0,2 |
| Ataque de 2–5 ms en sierra (clic) | 10–20 ms de ataque de amplitud |
| Sync / X-MOD | apagarlos o bajar el barrido |
| Unison / detune | ×2 con 8–12 ¢, o en una sola mitad del híbrido |
| Sustain / release largos | sustain 0,5–0,6, release 150–250 ms |
| Registro alto | una octava abajo; **cuerpo** con un poco de GRIT / drive (zona media) |
| Vibrato fijo | LFO → pitch en 0; vibrato con la rueda / MOVE |
| Velocity sin efecto en el filtro | `vel_cut` ~50–70 |

Referencias: Gordon Reid, *Synth Secrets* (Sound On Sound, 1999–2004), entregas sobre filtros y envolventes; Fred
Welsh, *Synthesizer Cookbook*, vol. 1; Snoman, *Dance Music Manual* (4.ª ed., 2019), cap. de leads.

### Presets de fábrica de Prophet y OB-6 revisados

Leídos de `preset_bank.cpp` (A = ataque de amplitud, S / R = sustain y release, cutoff y resonancia):

| Preset | Engine | A · S · R | Cutoff · reso | Lo que tiene | ¿Candidato? |
|---|---|---|---|---|---|
| **JD RUN LEAD BODY** | Prophet | 5 ms · 0,6 · 180 ms | 1,2 kHz · 0,3 | el cuerpo de tu lead de "Run"; corto, filtro bajo con mucha envolvente (0,55) | **sí**: el más cercano a lo que pides; trae sync suave (revisar) |
| **WARM KEYS** | Prophet | 2 ms · 0,4 · 400 ms | 0,7 kHz · 0,15 | redondo, tipo teclado; poco sustain | **sí**: lead-pluck oscuro, ideal para la rítmica corta |
| **POLY BRASS** | Prophet | 40 ms · 0,75 · 300 ms | 0,45 kHz · 0,1 | metal suave que se abre con la envolvente | **sí**: para la mixta y la larga |
| **JUMP BRASS** | OB-6 | 10 ms · 0,8 · 300 ms | 0,9 kHz · 0,1 | metal de OB, sin vibrato | **sí** |
| **GF BRASS HIT** | OB-6 | 15 ms · 0,3 · 250 ms | 0,9 kHz · 0,15 | corto, con PUMP y TAPE | **sí**: lead-stab para la corta |
| GF MELODIC LEAD | OB-6 | 5 ms · 0,7 · 400 ms | 2,5 kHz · 0,25 | unison, vibrato fijo, DRIFT | referencia (base que te gustó) |
| DESERT LEAD (capa) | OB-6 + Prophet | 3–5 ms · 0,75–0,8 · 350–400 ms | 1,8 / 2,2 kHz | notch que barre + sync | referencia (base que te gustó) |
| SYNC LEAD, XMOD LEAD, GF TAPE LEAD, GF ANTHEM SAW | Prophet / OB-6 | 2–8 ms · 0,75–0,8 | 2,0–3,0 kHz | sync, X-MOD o unison ancho | no: son la estridencia que quieres evitar |

Prefiltro sin grabar, máximo tres a la vez, con **mixta A en eólica**: (1) Prophet: JD RUN LEAD BODY · WARM KEYS ·
POLY BRASS; (2) OB-6: JUMP BRASS · GF BRASS HIT · GF MELODIC LEAD; (3) final: ganador Prophet · ganador OB-6 ·
DESERT LEAD. Los presets nuevos **JD LEAD …** (paso 3) se derivan de los ganadores con los cambios de la tabla de
arriba, sin borrar ningún preset. Límite conocido: el panel aún no carga un preset de engine dentro de una mitad de un
híbrido; una capa nueva se arma con `gf_presets.py` (`layers` / `layer-import`), con el puerto serie libre.

## 6. ADN y dialectos en los leads

`sesion-paleta-adn.md` §1c separa el **ADN** (lo que cruza subgéneros) de los **dialectos** (afro, organic, melodic,
indie, disco). En los leads queda así:

- **ADN:** el sonido (el preset que gane la ronda 1 tiene que servir en varios dialectos), la mezcla de cortas y
  largas, el registro medio, el color exótico como puntuación (la dosis).
- **Dialecto:** la escala con su progresión y la rítmica. Una escala que solo funciona en indie no se descarta: queda
  como "escala de indie".

Lo que espero que pase, **para comparar con lo que oigas** (tendencias, no reglas; los contextos de cada dialecto
están en `sesion-paleta-adn.md` §2):

| Dialecto | Contextos | Escalas que espero | Rítmicas que espero |
|---|---|---|---|
| afro | A, C | dórica, eólica, mixolidia | corta, mixta A (conversan con la percusión) |
| organic | A, D | hirajōshi, kumoi, frigia, menor húngara | larga, mixta A |
| melodic | B, F | eólica, lidia, menor con E prestado, menor húngara | larga, mixta B (pregunta-respuesta) |
| indie | B, E | frigia, in sen, frigia dominante | corta |
| disco | C | mixolidia, mayor con Dm prestado, dórica | corta, mixta A |

El resultado de las rondas 2–4 es un **mapa** (escala o rítmica × dialecto, con ✓ donde funcionó), no un único
ganador. Lo que tenga ✓ en varios dialectos es ADN de patrón.

## 7. Orden de la sesión (sin grabar; máximo tres candidatos por decisión)

| Ronda | Qué se decide | Con qué |
|---|---|---|
| 1 · sonido | el preset base (es ADN: tiene que pasar en dos contextos, uno B) | §5, con mixta A en eólica |
| 2 · mapa de escalas | por familia, las 3 escalas en cada contexto: ✓ o ✗ (no se elimina) | mixta A, con el preset ganador, sobre su progresión |
| 3 · favorita por dialecto | entre las que tienen ✓ en ese dialecto, la favorita (máximo 3 a la vez) | el contexto propio del dialecto (D, E, F; A o C para afro y disco) |
| 4 · ritmo | qué rítmicas sirven en cada escala favorita: ✓ o ✗ | las 4 rítmicas |
| 5 · dosis | completa · solo en el remate · de paso, por escala favorita | §4 |

Los ganadores se graban todos al final (`protocolo-captura.md` §3).

## 8. Cómo se prueba (paso 2)

| Qué | Cómo |
|---|---|
| Cada clip dura 4 compases, notas en rango MIDI, registro La3–Mi5 (Do3–Mi4 en la larga) | test (pytest) |
| Cada nota está en la escala del clip, salvo las del compás del acorde prestado, que están en ese acorde / su escala | test |
| Notas en la negra y notas largas (≥ 4 semicorcheas) son tonos del acorde de su compás | test |
| Las progresiones solo usan acordes de la escala **madre** (en las pentatónicas: hirajōshi → eólica, in sen → frigia, kumoi → dórica; en el compás prestado, la escala de donde viene) y cada acorde comparte al menos dos notas con la melodía | test |
| Silencio: la suma de duraciones ≤ 45 % (corta), ≤ 60 % (mixtas), ≤ 75 % (larga) de la vuelta | test |
| Misma rítmica en todas las escalas (mismos tiempos y duraciones; cambian solo las alturas) | test |
| Dosis "remate": la nota que distingue a la escala aparece una sola vez por vuelta, en el último compás; "de paso": dura ≤ 2 semicorcheas, en tiempo débil y resuelve por grado | test (cuando se generen, después de la ronda 3) |
| Que suene groovero, profundo y con la dosis justa de color | tú, en los contextos |

## 9. Estado y lo que sigue

**Hecho (2026-10-28):** `tools/paleta-lab/make_lead_lab.py` genera 48 leads (12 escalas × 4 rítmicas) y 12 clips de
acordes en `midi/adn/lead/lab/<familia>/<escala>__<rítmica>__<dialectos>.mid` (y `<escala>__acordes.mid`), con
`tests/test_lead_lab.py`. `fill_lab_set.py --leads` los carga en un set con pista **ACORDES**: escenas 1–6 los
contextos, después la mixta A de las 12 escalas (eólica primero: ronda 1) y luego las demás rítmicas por escala.

Detalles del generador que conviene saber al escuchar:

- La rítmica y el contorno son idénticos en todas las escalas; el ancla de cada compás es el tono del acorde más
  cercano a Mi4 (Fa#3 en la larga). Si una nota fuerte repetiría la anterior, toma el siguiente tono del acorde (hasta
  una tercera mayor) para que la frase no se quede pegada.
- En la mayor con iv prestado, el compás del Dm usa La menor (Fa y Do naturales); en la menor con V prestado, el
  compás del E usa la menor armónica (Sol#).
- La menor húngara no estaba en la tabla de §6: queda con dialectos melodic y organic (hipótesis).

Pendiente:

1. **Juan arma `GF_Lead_Lab.als`** (paso a paso en el README de `tools/paleta-lab/`) y se llena con
   `fill_lab_set.py --leads`.
2. **Presets JD LEAD** (§5), subidos con `gf_presets.py`, sin borrar ninguno.
3. ~~Contextos D · orgánico suave, E · hipnótico y F · melódico~~ — hechos: `make_adn_clips.py` los copia y
   `fill_lab_set.py` los pone en las escenas 4–6 (`GF_Paleta_Lab_v2.als`; el lab de leads los trae al llenarse).
4. **Dosis** (§4) de la escala ganadora, después de la ronda 3.

Los tres leads viejos (`lead_adn_frase`, `hook`, `profunda`) quedan como están, de referencia.
