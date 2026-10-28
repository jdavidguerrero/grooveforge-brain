# Sesión de paleta por ADN (no por género)

> **Fecha:** 2026-10-28 · **Estado:** propuesta para la Sesión B · **Depende de:** `ia-local-refinamiento.md` §2–3
> (un núcleo y varias direcciones), `protocolo-captura.md` (montaje, tomas, ficha), `variants/studio/paleta-v1.md`
> (roles, híbridos), `percusion-groove.md` §7 (selección de percusión).

## 1. La idea

Juan no quiere una paleta por género ("el bajo de afro", "el bajo de indie"), sino un **ADN**: sonidos y maneras de
tocar que tienen matices de cada estilo (afro, organic, melodic / progressive, disco, indie tech) y que juntos
suenan a una sola persona. Por eso:

- **Un sonido se elige si funciona en contextos distintos**, no si es el mejor para un género. El criterio de
  `caracter-sonoro.md` ("encaja en el mismo mix") se aplica a tres mezclas que cruzan estilos.
- **Los patrones mezclan matices**: un bajo con la síncopa de la tambora y el rebote de octavas de disco, un stab
  francés con llamada 3-3-2, un arpegio progresivo acentuado como marimba de chonta.
- **Tribe (lo colombiano) es el hilo común** de los tres contextos: fue lo que Juan eligió en 34 de 37 percusiones.
  (Rev. 2026-10-28, §1d: Tribe es firma de marca; cada dialecto puede tener su kit.)

## 1b. Función: club (Juan, 2026-10-28)

Los tracks son **de club, bailables en su mayoría**. Lo que más pesa, en este orden:

1. **Un bajo groovy**: articulado, con ghost notes y síncopa, que se lleve con el kick. Es el centro del ADN.
2. **Percusión étnica muy nativa**: Tribe (lo colombiano) primero, djembes y Brasil como acentos; tocada, no
   programada en rejilla (microtiming y manos que alternan).
3. **Melodías profundas** encima de ese groove: motivos (menores o mayores, con acordes prestados), notas cortas,
   largas y mixtas, novenas y séptimas, espacio; hooks cortos que se repiten. (Rev. 2026-10-28: "notas largas" no es
   regla; las escalas se exploran en `lab-leads.md`.)

El disco / house pide otra cosa (stabs, octavas, brillo) y se trata como matiz del mismo ADN, no como otra paleta.

**Qué cambia en la escucha:**

- La pregunta deja de ser solo "¿encaja en el mix?": **"¿empuja la pista?"**. Se escucha también a volumen alto, con
  el kick y el bajo adelante.
- **Bajo para un sistema de club:** grave limpio y mono por debajo de ~100 Hz (sin chorus ni reverb en el sub), que
  se lea fuerte y deje lugar al kick (sidechain o notas fuera de las negras).
- **Tempo:** 122–126 BPM es la zona principal; lo que salga de ahí se marca como excepción.
- **Orden de las sesiones:** bajos → percusión tonal / étnica → melodías profundas (plucks, leads, hooks) → stabs y
  acordes → pads y texturas (material de break e intro, no de pista) → build / riser.
- Para más adelante (arreglo): frases de 8 / 16 / 32 compases, intro y outro mezclables por un DJ.

## 1c. ADN y dialectos (Juan, 2026-10-28, fase 2)

Juan se mueve entre subgéneros con una sola marca, pero hay cosas que funcionan en afro y no en melodic o indie. Eso
no es un defecto del patrón: es que cada subgénero es un **dialecto** del mismo ADN (la versión práctica de "un núcleo
y varias direcciones", `ia-local-refinamiento.md` §2). Como una persona que habla igual de voz y de acento, pero no
habla igual en una fiesta que en una reunión.

| | Qué es | Ejemplos | Regla |
|---|---|---|---|
| **ADN** | lo que se queda en todos los subgéneros | el **sonido** (presets y capas), la manera de tocar (cortas y largas, ghost notes, silencios, registro), el color exótico como puntuación, Tribe como firma de marca (§1d) | tiene que **cruzar** contextos (§2) |
| **Dialecto** | lo que cambia con el subgénero | escala y progresión, densidad, largo de la frase (afro: 1–2 compases que conversan con la percusión; melodic: un motivo que evoluciona en 8–16), cuánto color, qué patrón de bajo o de acordes | puede ganar en **un solo** dialecto; se marca con él |

Dialectos: **afro**, **organic**, **melodic** (melodic / progressive), **indie** (indie tech) y **disco** (disco /
house). Se aplica a **todos los roles**, no solo a los leads: un bajo `rodante` puede ser de indie y melodic, y un
`cumbia`, de afro y organic, y los dos son de Juan.

Lo que sale de la fase 2 es un **mapa** (filas: patrones o escalas; columnas: dialectos; ✓ donde funcionó). Lo que
aparece en varias columnas es ADN de patrón; lo que aparece en una sola es dialecto. Las dos cosas son material propio.

## 1d. Kits por dialecto (Juan, 2026-10-28)

Juan: Tribe "era como un tema más de marca mía a nivel Colombia", y no sabe qué tanto sus instrumentos aplican a los
tops de indie dance o progressive; el 808 se queda. Escuchar todo en Tribe vuelve organic / afro cualquier dialecto,
así que **el kit también es dialecto**. Tribe pasa a ser **firma de marca**: en organic y afro puede ser el kit
entero; en los demás entra como **dosis** (un elemento: guache, cáscara de tambora, maracón), igual que el color
exótico en los leads (`lab-leads.md` §4).

Revisión de los kits que hay (`kits/*.json`, `make_perc_patterns.py`, `make_gf_racks.py`):

| Dialecto | Qué pide el top (tendencia, no regla) | Kit propuesto | Patrones que ya existen en ese kit |
|---|---|---|---|
| afro | congas / djembes que conversan, shaker, llamada 3-3-2 | **GF Djembe Dunun** (+ Tribe) | `djembe_sang`, `afro_house_groove`, `afro_dunun_roll` |
| organic | madera, shakers suaves, congas tocadas | **Tribe** (+ congas / bongos más adelante) | `cumbia_afro`, `organic_shaker_wood`, `organic_soft_conga` |
| melodic / progressive | semicorcheas que ruedan (shaker o hat cerrado), algo abierto en el contratiempo, rim o perc escasa, aire | **GF Brasil** arriba (ganzá / caxixi rodando, pandeiro y triángulo en el contratiempo) + **GF 808 Perc** (rim) | `prog_rolling16` (Brasil), `prog_offbeat_rim` (808) |
| indie (dance / tech) | más seco y recto (swing 50–52), toms, cowbell, rim, clap, hats electrónicos | **GF 808 Perc** | `tribal_toms`, `indie_metal_rim` |
| disco / house | pandereta y conga en el contratiempo, cowbell, shaker con swing | **GF Brasil** + **GF 808 Perc** | `samba_house` (Brasil), `house_conga`, `disco_tamb_cowbell` (808) |

**Hueco encontrado: los hats.** El mapa común (`percusion-groove.md` §3, notas 36–50) no tiene hat cerrado, hat
abierto ni ride, que son la base del top en progressive e indie dance. Los samples sí están: el rack **GF 808 Perc**
trae *Closed Hi Hat 808* y *Open Hi Hat 808* (en las notas 51+, sin rol), y GF Brasil trae el **triángulo** abierto
y cerrado (un "hat" orgánico). Agregar roles de hat al mapa es un cambio de `percusion-groove.md` §3: queda propuesto,
no hecho.

**Tribe en todos, en dosis (Juan, 2026-10-28: "probar algo de Tribe en todos, en menor dosis, pero mantenerlo").**
Cada dialecto suena con su kit y **un** elemento de Tribe encima, suave (ghost, velocity baja), elegido para que haga
lo que ese dialecto ya pide del top. En organic Tribe sigue siendo el kit entero.

| Dialecto | Kit (cuerpo · top) | Dosis de Tribe | Por qué ese |
|---|---|---|---|
| afro | Djembe: `djembe_sang` · `afro_house_groove` | **llamador** en los contratiempos | es el que lleva el pulso en la cumbia; en afro house ese lugar es la conga abierta |
| organic | Tribe: `cumbia_afro` · `organic_shaker_wood` | (todo Tribe) | — |
| melodic | 808 `prog_offbeat_rim` · Brasil `prog_rolling16` | **maracón** largo en los contratiempos | aire, sin sumar otra semicorchea al ganzá que ya rueda |
| indie | 808: `tribal_toms` · `indie_metal_rim` | **cáscara de tambora** cada 3 semicorcheas | polirritmo seco y de madera, hipnótico (`tambora_hipnotica`) |
| disco | 808 `house_conga` · Brasil `samba_house` | **guacharaca** en semicorcheas, con ghosts | raspa como un hat con swing: el disco colombiano |

En la ronda cada dialecto va **dos veces**: kit solo y kit + dosis de Tribe, con el mismo lead. Así se decide de oído
si la firma suma o estorba en cada uno.

**Resultado (2026-10-28, `GF_Lead_Dialectos_kits.als`): la dosis queda en los cuatro dialectos.** Juan: "le dio
un groove con un toque sutil, era lo que quería". Tribe es la firma: kit entero en organic y un elemento suave en los
demás, con la tabla de arriba.

## 2. Contextos de escucha (los mismos para todos los sonidos)

Cada uno usa la selección de percusión de Juan (`midi/perc/seleccion/` o `favoritos/`), todo en Tribe, con un kick
4/4 de Kick 3. A–C cruzan dos dialectos; D–F (fase 2) aíslan uno:

| Contexto | Perc (cuerpo) | Top (brillo) | Dialectos |
|---|---|---|---|
| **A · tierra** | `cumbia_afro_v4` peak (organic) | `afro_house_groove_v2` peak (afro) | organic + afro |
| **B · noche** | `tribal_toms_v2` peak (indie tech) | `prog_rolling16_v3` peak (progressive) | indie + melodic |
| **C · fiesta** | `djembe_sang_v2` main_q (afro) | `samba_house_v4` peak (disco) | afro + disco |
| **D · orgánico suave** | — | `organic_shaker_wood_v4` peak | organic |
| **E · hipnótico** | `tambora_hipnotica` main (`favoritos/indie_tech`) | poco o nada | indie |
| **F · melódico** | `prog_offbeat_rim_v3` peak (un top de la selección, como cuerpo) | `prog_rolling16_v4` peak | melodic |

Antes, melodic quedaba encerrado en B junto con indie; con F cada dialecto tiene al menos un contexto propio o
compartido (afro: A, C · organic: A, D · indie: B, E · melodic: B, F · disco: C).

**Reglas de elección (rev. 2026-10-28, fase 2):**

- **Sonidos** (presets, capas): sin cambios. Pasan si encajan en **al menos dos contextos, y uno de ellos es B** (el
  más oscuro, el que más filtra). Un sonido que solo funciona en un dialecto queda como **color de ese dialecto**,
  marcado, no como ADN.
- **Patrones** (ritmo, escala, progresión): pasan **por dialecto**. Un patrón que funciona en un contexto de un
  dialecto queda en la paleta con esa marca; si funciona en dos o más dialectos, es ADN de patrón.

(La regla de "dos contextos y uno es B" se escribió para la Sesión B, cuando se elegía todo junto; en la fase 2 se
separa porque un patrón puede ser de un solo dialecto sin dejar de ser de Juan.)

## 3. Patrones a tocar (MIDI)

La menor, 124 BPM, **una sola progresión para todos los roles**: Am – F – C – G (i – VI – III – VII), un compás por
acorde, 4 compases que se repiten. Son puntos de partida para tocar y variar, no partes cerradas.

| Rol | Patrón | Matices que junta | Dialecto esperado |
|---|---|---|---|
| bass | `bass_adn_tambora` | síncopa afro / colombiana, deja libre el 1 para el kick, ghost notes | afro, organic |
| | `bass_adn_octavas` | rebote de octavas de disco con empujes a contratiempo | disco, afro |
| | `bass_adn_rodante` | semicorcheas rodantes de indie / progressive, fuera de los tiempos del kick | indie, melodic |
| | `bass_adn_cumbia` | el bajo de la cumbia (1 y 3 con anticipación) llevado al 4/4 | organic, afro |
| chords | `chords_adn_contratiempo` | stab francés / disco en los contratiempos, con un ghost | disco |
| | `chords_adn_respiro` | acorde largo que respira (deja lugar al PUMP / sidechain), melódico | melodic |
| | `chords_adn_llamada` | llamada 3-3-2 de la percusión afro llevada a los acordes | afro |
| pluck | `pluck_adn_marimba` | arpegio en 3-3-2 como marimba de chonta | organic, afro |
| | `pluck_adn_motivo` | motivo de 4 notas que se desarrolla (progressive / organic) | melodic, organic |
| | `pluck_adn_arp16` | arpegio de semicorcheas progresivo con acentos 3-3-2 | melodic, indie |
| perc tonal | `perc_adn_logdrum` | log drum de afro house, grave y sincopado | afro |
| | `perc_adn_madera` | tresillo / clave en dos alturas (madera, metal) | organic, afro |
| pad / textura | `pad_adn_progresion` | la progresión en acordes enteros | melodic |
| | `pad_adn_dron` | La + Mi sostenidos 8 compases: movimiento sin tocar nada (textura) | indie, organic |
| lead | `lead_adn_frase` | frase pentatónica con notas largas, pregunta y respuesta | — (reemplazado por el lab) |
| | `lead_adn_hook` | hook de 2 compases que se repite (el gancho del drop) | — (reemplazado por el lab) |
| | `lead_adn_profunda` | melodía profunda: novenas y séptimas sobre la progresión, notas largas, descenso | — (reemplazado por el lab) |
| build | `build_adn_move` | 8 compases: una nota sostenida con **MOVE (CC 19)** de 0 a 100 % (filtro, ecos, reverb), para GF NOISE RISER o cualquier pad | melodic, indie |

Siguen existiendo los clips **neutros** de medición (`midi/<rol>/`, protocolo §2): esos miden; estos se tocan.

### Capas híbridas

Las 8 capas de fábrica entran como candidatos. Cada una tiene un clip `capa_<n>_<nombre>` que **elige la capa con CC
18** al comenzar y toca su patrón tres veces con **MORPH (CC 17) en 0, 50 y 100 %** (12 compases): solo A, las dos,
solo B.

| Capa | Rol | Patrón |
|---|---|---|
| 1 WOODBASS (DX7 + Model D) | bass | `bass_adn_tambora` |
| 2 KALIMBA (DX7 + OB-6) | pluck | `pluck_adn_marimba` |
| 3 FRENCH STAB (Juno + Prophet) | chords | `chords_adn_contratiempo` |
| 4 ACID ARP (ARP + Model D) | pluck / bass | `pluck_adn_arp16` una octava abajo |
| 5 DESERT LEAD (OB-6 + Prophet) | lead | `lead_adn_frase` |
| 6 SHAKER ZAP (ARP + DX7) | perc | `perc_adn_madera` |
| 7 ROLLER SUB (Model D + Juno) | bass | `bass_adn_rodante` |
| 8 RETRO BRASS (Prophet + Juno) | chords | `chords_adn_respiro` |

## 4. Candidatos por rol (presets y capas)

| Rol | Presets | Capas |
|---|---|---|
| bass | GF AFRO SUB, GF ROLLING BASS, GF OCTAVE BASS, GF GRIT BASS, GF POCKET BASS, JD RUN FUNKBASS | WOODBASS, ROLLER SUB |
| chords | GF FILTER STAB, GF HOUSE KEYS, GF DISCO STRINGS, GF FM DISCO STAB | FRENCH STAB, RETRO BRASS |
| pluck | GF SUNRISE PLUCK, GF TALKING PLUCK, GF CHONTA, GF GLASS ARP, GF ORGANIC ARP | KALIMBA, ACID ARP |
| perc tonal | GF LOG DRUM, GF KALIMBA TINE, GF METAL PERC | SHAKER ZAP |
| pad / textura | GF BAMBOO PAD, GF NOCTURNE PAD, GF STRING SWELL, GF DARK DRONE, GF NOISE RISER | — |
| lead | GF ANTHEM SAW, GF TAPE LEAD | DESERT LEAD |
| build | GF NOISE RISER (y pads con MOVE) | — |

Meta de la Sesión B (sin cambios): unos 10 sonidos al final. Una sesión por rol, en el orden de §1b, empezando por
**bass**.

## 5. El set de Live

`tools/paleta-lab/make_adn_clips.py` escribe todo en `GrooveForge_Dataset_v1/midi/adn/`:

```
midi/adn/contexto/<A_tierra|…|F_melodico>/{perc,top}_tribe.mid       (copias de la selección; D sin perc)
midi/adn/contexto/kick_4x4.mid                                        (C1 en las negras, para Kick 3)
midi/adn/<bass|chords|pluck|perc|pad|lead>/<patron>.mid
midi/adn/capas/capa_<n>_<nombre>.mid                                  (CC 18 + CC 17 + notas)
```

El `.als` lo arma Juan una vez (Live no tiene una forma estable de generarlo desde afuera), siguiendo el README de
esa carpeta: pistas KICK (Kick 3), PERC y TOP (Tribe), BRAIN MIDI (salida a la Teensy), BRAIN AUDIO (entradas 1/2,
para grabar) y una escena por contexto. Se guarda como `GF_Paleta_Lab.als` y se reutiliza en todas las sesiones.
Con los contextos D–F (fase 2) el set lleno es `GF_Paleta_Lab_v2.als` (copia de `GF_Paleta_Lab_clips.als`, que
queda como estaba): escenas 1–6 los contextos, desde la 7 los patrones.

## 6. Qué se anota

En la ficha de sesión (`gf_session.py`), cada decisión lleva el **contexto** (`A tierra`, `B noche`, `C fiesta`) y,
al final, en qué contextos pasó cada elegido. Ese cruce es el dato que dice qué es ADN y qué es color.

Fase 2: el dialecto sale del contexto (tabla de §2), así que basta con anotar el contexto en cada decisión. Al final de
cada rol se arma el mapa de §1c (patrón × dialecto). El "dialecto esperado" de la tabla de §3 es una hipótesis para
comparar con lo que oiga Juan, no una regla.

## 7. Cómo se prueba

| Qué | Cómo |
|---|---|
| Patrones en La menor sobre la progresión, dentro de su largo, sin notas fuera de rango MIDI | test (pytest) |
| Los graves de los bajos `tambora` y `rodante` no caen en las negras (el kick) | test |
| `build_adn_move` sube MOVE (CC 19) de 0 a 127 en 8 compases | test |
| Cada clip de capa elige su capa: CC 18 al tick 0 con el valor que `layer_from_cc` lleva a esa capa; MORPH 0 / 64 / 127 | test |
| Los contextos (A–F) copian los archivos de la selección o de favoritos; D no tiene perc | test |
| Que suenen bien juntos | Juan, en la sesión |

## 8. Fase 2: refinamiento (lo que dejó el check general, 2026-10-28)

| Rol | Resultado del check | Para la fase 2 |
|---|---|---|
| bass | JD SUB MID (capa: GF AFRO SUB + Juno de ROLLER SUB), `bass_adn_octavas`; pasa en A, B y C | variaciones sobre JD SUB MID (sonido y patrón) |
| perc tonal | GF METAL PERC ("super étnico"), con KALIMBA TINE y SHAKER ZAP; `perc_adn_madera` | variaciones sobre METAL PERC |
| pluck | GF TALKING PLUCK + `arp16`; SUNRISE + `motivo` (organic suave) y KALIMBA (hipnótico) | menos brillo, ataque más suave, más cuerpo; contextos D y E |
| chords | GF HOUSE KEYS (con los tres patrones), GF FILTER STAB (`respiro`, `contratiempo`), FRENCH STAB | variaciones sobre HOUSE KEYS |
| pad | GF NOCTURNE PAD y GF DARK DRONE (OB-6) con `pad_adn_dron`: algo oscuros; los del Juno, muy brillantes | un brillo intermedio entre OB-6 y Juno |
| build | GF NOISE RISER (ARP 2600) con `build_adn_move` | refinar desde ahí |
| lead | ninguno: "muy orientales y extendidos"; sin percusión, "notas demasiado largas y estridentes"; base que interesa: DESERT LEAD y GF MELODIC LEAD | **lab dedicado de leads** (abajo) |

**Lab de leads** (Juan: de lo más importante). No se trata de sacar lo "oriental" sino de **manejarlo**: escalas como
la pentatónica y la menor húngara combinadas con la base menor natural (por ejemplo, la segunda aumentada solo en el
remate de una frase), frases cortas que **mezclen notas largas y cortas** y sean más grooveras (que funcionen como gancho de pista), menos estridentes, y presets que partan de DESERT LEAD y
GF MELODIC LEAD. Contextos nuevos para probar: D · orgánico suave y E · hipnótico. Diseño del lab (rev. 2: doce
escalas en cuatro familias, cada una con su progresión, acordes prestados, rítmicas corta / mixta / larga y otros
presets de Prophet y OB-6): `lab-leads.md`.
