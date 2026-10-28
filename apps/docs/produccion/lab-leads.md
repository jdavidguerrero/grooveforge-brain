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
DESERT LEAD.

**Presets JD LEAD (hechos 2026-10-28, antes del prefiltro).** `tools/paleta-lab/make_jd_leads.py` toma los originales
de la librería, aplica la tabla de arriba y escribe los JSON; se subieron con `gf_presets.py` sin borrar ninguno:

| Preset | Lugar | Parte de | Cambios |
|---|---|---|---|
| JD LEAD MELODIC | OB-6 A-14 | GF MELODIC LEAD | sin vibrato; unison ×2 con 10 ¢ (era ×3, 18 ¢); cutoff 1,2 kHz (2,5) con env 0,55 y decay 220 ms; reso 0,15; ataque 12 ms, sustain 0,55, release 200 ms; DRIFT 15, GRIT 30; `vel_cut` 65 |
| JD LEAD BODY | Prophet B-01 | JD RUN LEAD BODY | sin sync ni POLY-MOD a FREQ A (A en unísono con B, B +7 ¢); cutoff 1,1 kHz con env 0,55 y decay 250 ms; ataque 10 ms; `vel_cut` 65 |
| JD DESERT LD (capa) | capas A-10 | DESERT LEAD | A (OB-6): FLT MORPH 15 (era 50, el notch nasal), barrido del LFO 0,1 (0,3), cutoff 1,3 kHz; B (Prophet): sin sync ni POLY-MOD, mezcla 70 / 40; las dos con ataque 12 ms y release ~200 ms. MORPH 50 y B −3 dB como la original |

Entran al prefiltro como cuarta opción de su ronda (Prophet, OB-6, final con DESERT LEAD), cambiando por uno de los
de fábrica para seguir en tres: JD LEAD BODY en lugar de POLY BRASS, JD LEAD MELODIC en lugar de GF MELODIC LEAD y
JD DESERT LD contra DESERT LEAD en la final. Del ganador del prefiltro se derivan más versiones si hace falta.
Límite conocido: el panel aún no carga un preset de engine dentro de una mitad de un híbrido; una capa nueva se arma
con `gf_presets.py` (`layers` / `layer-import`), con el puerto serie libre.

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
`tests/test_lead_lab.py`. `fill_lab_set.py --leads` los **agrega** debajo de lo que ya tiene la sesión de Juan
(`GF_Paleta_Lab_clips.als` guardada como `GF_Lead_Lab.als` con una pista **ACORDES**): en cada escena el kick, el
lead y los acordes de su escala; primero la mixta A de las 12 escalas (eólica primero: ronda 1) y luego las demás
rítmicas. Juan los escucha con el kick (decisión 2026-10-28: sin bajo debajo; el Brain toca un sonido a la vez, así
que el bajo del Brain no suena junto con el lead).

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
2. ~~Presets JD LEAD~~ — hechos (§5): JD LEAD MELODIC, JD LEAD BODY y la capa JD DESERT LD.
3. ~~Contextos D · orgánico suave, E · hipnótico y F · melódico~~ — hechos: `make_adn_clips.py` los copia y
   `fill_lab_set.py` los pone en las escenas 4–6 (`GF_Paleta_Lab_v2.als`; el lab de leads los trae al llenarse).
4. ~~Ronda 3 de dialectos~~ — hecha (§10, resultado de la ronda de dialectos).
5. **Dosis** (§4) de las ganadoras: frigia dominante (completa en corta, remate o de paso en mixta) y el V
   prestado; la lidia y la eólica van completas.
6. **Refinar el sonido** por dialecto (JD LEAD BODY / WARM KEYS), después de las dosis.

Los tres leads viejos (`lead_adn_frase`, `hook`, `profunda`) quedan como están, de referencia.

## 10. Primera escucha (2026-10-28, `GF_Lead_Lab.als`, solo con kick)

Anotado en la ficha `metadata/sesiones/2026-10-28_lead.json` (campo `lab_leads`).

**Sonido.** Todos los del prefiltro le gustaron a Juan, en este orden ("buen inicio para explorar"):
1. JD LEAD BODY · 2. JUMP BRASS · 3. GF BRASS HIT, WARM KEYS y JD RUN LEAD BODY (empate) · 4. JD DESERT LD.
Los dos primeros son de la familia "redondo, filtro bajo": uno derivado (§5) y uno de fábrica sin vibrato ni sync.

**Ritmo.** "El patrón definitivamente es mixto, aunque hay algunas cortas y largas": 4 mixta A y 8 mixta B de 16
clips; 3 cortas y 2 largas.

**Mapa escala × rítmica** (✓ = le gustó; † = escuchado por error con GF SUNRISE PLUCK, no con un lead):

| Familia | Escala | corta | mixta A | mixta B | larga | Dialecto esperado (§6) |
|---|---|---|---|---|---|---|
| 1 · menores | eólica | | ✓ | ✓ | | afro, melodic |
| | dórica | | | ✓ | ✓ | afro, disco |
| | menor con V prestado | | | ✓ | ✓ | melodic |
| 2 · exóticas | frigia | | | ✓ | | organic, indie |
| | frigia dominante | ✓ † | | ✓ | | indie |
| | menor húngara | | | | | melodic, organic |
| 3 · japonesas | hirajōshi | | ✓ | | | organic |
| | in sen | | | ✓ | | indie |
| | kumoi | ✓ | ✓ | ✓ | | organic |
| 4 · mayores | lidia | | ✓ | | | melodic |
| | mixolidia | | | | | afro, disco |
| | mayor con iv prestado | ✓ | | | | disco |

Lo que dice el mapa:

- **Kumoi** es la que más rítmicas aguanta (3 de 4); las **japonesas** pasan las tres. Juntas con la eólica y las
  menores con acordes de la familia 1, son el núcleo más probable.
- **La menor húngara no pasó en ninguna rítmica**, aunque fue el punto de partida del lab: su color completo es
  demasiado. Queda para la **dosis** (§4: solo en el remate o de paso), no como escala de una frase entera.
- **La mixolidia tampoco**: lo "mayor con calle" no es de Juan por ahora; lo mayor que pasa es lidio (flotante) y la
  mayor con el Dm prestado (en corta).
- Faltan los **dialectos**: se escuchó solo con kick, así que las etiquetas del nombre siguen siendo hipótesis. La
  ronda 3 (§7) los confirma escuchando los clips que gustaron sobre los contextos de cada dialecto.

### Ronda de dialectos (ronda 3, preparada 2026-10-28)

`GF_Lead_Dialectos.als` (copia de `GF_Paleta_Lab_clips.als`; `fill_lab_set.py --dialectos <ficha>`): los 16 clips que
gustaron sobre la percusión de cada dialecto, 16 escenas por bloque, cada una con kick, perc, top, lead y acordes.

| Escenas | Dialecto | Perc (cuerpo) | Top |
|---|---|---|---|
| 4–19 | afro | `djembe_sang_v2` (de C) | `afro_house_groove_v2` (de A) |
| 20–35 | organic | `cumbia_afro_v4` (de A) | `organic_shaker_wood_v4` (de D) |
| 36–51 | melodic | `prog_offbeat_rim_v3` (F) | `prog_rolling16_v4` (F) |
| 52–67 | indie | `tambora_hipnotica` main (E) | `tambora_hipnotica` intro (E) |
| 68–83 | disco | `djembe_sang_v2` (C) | `samba_house_v4` (C) |

Afro y organic se arman **puros** cruzando los contextos A, C y D (cuerpo y top del mismo dialecto). Disco no tiene
perc propia en la selección: su bloque usa C (afro + disco), así que un ✓ ahí puede deberse al afro. Preset de la
ronda: JD LEAD BODY (el primero de Juan), el mismo en los cinco bloques. En cada bloque se marca ✓ / ✗ por clip; con
eso se completa la columna de dialectos del mapa de §10.

### Ronda de dialectos con kits (rev. 2026-10-28, reemplaza a la de arriba para escuchar)

Juan: Tribe vuelve organic / afro todos los bloques; cada dialecto con su kit y Tribe en dosis
(`sesion-paleta-adn.md` §1d). `GF_Lead_Dialectos_kits.als` (copia de `GF_Lead_Dialectos.als` con las pistas KIT
DJEMBE, KIT BRASIL y KIT 808; `fill_lab_set.py --dialectos <ficha> --kits`; dosis de `make_tribe_dosis.py`):

| Escenas | Dialecto | Kit | Dosis de Tribe (en PERC) |
|---|---|---|---|
| 4–21 | afro | KIT DJEMBE: `djembe_sang` + `afro_house_groove` | llamador |
| 22–37 | organic | PERC / TOP Tribe: `cumbia_afro` + `organic_shaker_wood` | (todo Tribe) |
| 38–55 | melodic | KIT BRASIL `prog_rolling16` + rim de KIT 808 (`prog_offbeat_rim`) | maracón |
| 56–73 | indie | KIT 808: `tribal_toms` + `indie_metal_rim` | cáscara de tambora |
| 74–91 | disco | KIT BRASIL `samba_house` + KIT 808 `house_conga` | guacharaca |

Cada bloque (menos organic) abre con **REF kit solo** y **REF kit + tribe** (eólica mixta A): ahí se decide si la
firma suma en ese dialecto. Los clips usan el mapa común: KIT BRASIL y KIT 808 tienen que tener los racks **GF Brasil**
y **GF 808 Perc** (`kits/racks/`), no los originales.

### Resultado de la ronda de dialectos (2026-10-28, `GF_Lead_Dialectos_kits.als`)

Anotado en la ficha (`lab_leads.ronda_dialectos`). ✓ = pasa en ese dialecto. Juan alternó el sonido (JD LEAD BODY en
unos, WARM KEYS en otros: "es un tema de refinar"), así que el mapa es de **escala y rítmica**, no de preset.

| Clip | afro | organic | melodic | indie | disco | Dialectos |
|---|---|---|---|---|---|---|
| menor con V prestado · mixta B | ✓ | ✓ | ✓ | ✓ | ✓ | **5** |
| frigia dominante · corta | | ✓ | ✓ | ✓ | ✓ | 4 |
| frigia dominante · mixta B | ✓ | ✓ | ✓ | ✓ | | 4 |
| eólica · mixta A | ✓ | ✓ | ✓ | | | 3 |
| eólica · mixta B | ✓ | ✓ | ✓ | | | 3 |
| menor con V prestado · larga | ✓ | ✓ | ✓ | | | 3 |
| hirajōshi · mixta A | ✓ | ✓ | ✓ | | | 3 |
| **lidia · mixta A** | ✓ | ✓ | ✓ | | | 3 |
| mayor con iv prestado · corta | ✓ | ✓ | ✓ | | | 3 |
| kumoi · mixta A | ✓ | | ✓ | | | 2 |
| kumoi · corta | ✓ | | | ✓ | | 2 |
| dórica · mixta B | ✓ | | ✓ | | | 2 |
| dórica · larga | | ✓ | | | | 1 |
| frigia · mixta B · in sen · mixta B · kumoi · mixta B | | | | | | 0 |

Lo que dice:

- **Afro, organic y melodic aceptan casi lo mismo** (10–11 de 16): con la base de Juan, esos tres dialectos son una
  sola zona armónica. Lo que los separa es el kit, no la escala.
- **Indie y disco son exigentes** y piden **tensión**: el Sol# del V prestado y la frigia dominante (la segunda
  aumentada), mejor en corta. El color "oriental" que molestaba en frases largas **sí sirve ahí**, en notas cortas.
  Disco sigue usando la perc de C (afro + disco), así que sus 2 ✓ son la lectura más débil.
- **ADN de leads**: la **menor con V prestado en mixta B** cruza los cinco dialectos; la **frigia dominante** (corta
  y mixta B) cruza cuatro. Son el hilo común de la marca en lo melódico.
- **Lidia** (Juan: "me gustó mucho en casi todos") pasa en afro, organic y melodic, no en indie ni disco: es el color
  **brillante** de la paleta, el contrapeso de las menores.
- **Kumoi bajó** (de 3 rítmicas con kick a 2 dialectos parciales) y la **frigia** y la **in sen** no pasan con
  percusión: con kick solo sonaban bien, con kit se pierden.
- Sin respuesta todavía sobre **Tribe en dosis** (REF kit solo / kit + tribe).
