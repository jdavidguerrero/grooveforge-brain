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

## 1b. Función: club (Juan, 2026-10-28)

Los tracks son **de club, bailables en su mayoría**. Lo que más pesa, en este orden:

1. **Un bajo groovy**: articulado, con ghost notes y síncopa, que se lleve con el kick. Es el centro del ADN.
2. **Percusión étnica muy nativa**: Tribe (lo colombiano) primero, djembes y Brasil como acentos; tocada, no
   programada en rejilla (microtiming y manos que alternan).
3. **Melodías profundas** encima de ese groove: motivos menores, notas largas, novenas y séptimas, espacio; hooks
   cortos que se repiten.

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

## 2. Tres contextos de escucha (los mismos para todos los sonidos)

Cada uno junta dos géneros de la selección de percusión de Juan (`midi/perc/seleccion/`), todo en Tribe, con un kick
4/4 de Kick 3:

| Contexto | Perc (cuerpo) | Top (brillo) | Cruce |
|---|---|---|---|
| **A · tierra** | `cumbia_afro_v4` peak (organic) | `afro_house_groove_v2` peak (afro) | organic + afro |
| **B · noche** | `tribal_toms_v2` peak (indie tech) | `prog_rolling16_v3` peak (progressive) | indie + progressive |
| **C · fiesta** | `djembe_sang_v2` main_q (afro) | `samba_house_v4` peak (disco) | afro + disco |

**Regla de elección:** un candidato pasa si encaja en **al menos dos contextos, y uno de ellos es B** (el más
oscuro, el que más filtra). Lo que solo funciona en uno queda como color, no como ADN.

## 3. Patrones a tocar (MIDI)

La menor, 124 BPM, **una sola progresión para todos los roles**: Am – F – C – G (i – VI – III – VII), un compás por
acorde, 4 compases que se repiten. Son puntos de partida para tocar y variar, no partes cerradas.

| Rol | Patrón | Matices que junta |
|---|---|---|
| bass | `bass_adn_tambora` | síncopa afro / colombiana, deja libre el 1 para el kick, ghost notes |
| | `bass_adn_octavas` | rebote de octavas de disco con empujes a contratiempo |
| | `bass_adn_rodante` | semicorcheas rodantes de indie / progressive, fuera de los tiempos del kick |
| | `bass_adn_cumbia` | el bajo de la cumbia (1 y 3 con anticipación) llevado al 4/4 |
| chords | `chords_adn_contratiempo` | stab francés / disco en los contratiempos, con un ghost |
| | `chords_adn_respiro` | acorde largo que respira (deja lugar al PUMP / sidechain), melódico |
| | `chords_adn_llamada` | llamada 3-3-2 de la percusión afro llevada a los acordes |
| pluck | `pluck_adn_marimba` | arpegio en 3-3-2 como marimba de chonta |
| | `pluck_adn_motivo` | motivo de 4 notas que se desarrolla (progressive / organic) |
| | `pluck_adn_arp16` | arpegio de semicorcheas progresivo con acentos 3-3-2 |
| perc tonal | `perc_adn_logdrum` | log drum de afro house, grave y sincopado |
| | `perc_adn_madera` | tresillo / clave en dos alturas (madera, metal) |
| pad / textura | `pad_adn_progresion` | la progresión en acordes enteros |
| | `pad_adn_dron` | La + Mi sostenidos 8 compases: movimiento sin tocar nada (textura) |
| lead | `lead_adn_frase` | frase pentatónica con notas largas, pregunta y respuesta |
| | `lead_adn_hook` | hook de 2 compases que se repite (el gancho del drop) |
| | `lead_adn_profunda` | melodía profunda: novenas y séptimas sobre la progresión, notas largas, descenso |
| build | `build_adn_move` | 8 compases: una nota sostenida con **MOVE (CC 19)** de 0 a 100 % (filtro, ecos, reverb), para GF NOISE RISER o cualquier pad |

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
midi/adn/contexto/<A_tierra|B_noche|C_fiesta>/{perc,top}_tribe.mid   (copias de la selección)
midi/adn/contexto/kick_4x4.mid                                        (C1 en las negras, para Kick 3)
midi/adn/<bass|chords|pluck|perc|pad|lead>/<patron>.mid
midi/adn/capas/capa_<n>_<nombre>.mid                                  (CC 18 + CC 17 + notas)
```

El `.als` lo arma Juan una vez (Live no tiene una forma estable de generarlo desde afuera), siguiendo el README de
esa carpeta: pistas KICK (Kick 3), PERC y TOP (Tribe), BRAIN MIDI (salida a la Teensy), BRAIN AUDIO (entradas 1/2,
para grabar) y una escena por contexto. Se guarda como `GF_Paleta_Lab.als` y se reutiliza en todas las sesiones.

## 6. Qué se anota

En la ficha de sesión (`gf_session.py`), cada decisión lleva el **contexto** (`A tierra`, `B noche`, `C fiesta`) y,
al final, en qué contextos pasó cada elegido. Ese cruce es el dato que dice qué es ADN y qué es color.

## 7. Cómo se prueba

| Qué | Cómo |
|---|---|
| Patrones en La menor sobre la progresión, dentro de su largo, sin notas fuera de rango MIDI | test (pytest) |
| Los graves de los bajos `tambora` y `rodante` no caen en las negras (el kick) | test |
| `build_adn_move` sube MOVE (CC 19) de 0 a 127 en 8 compases | test |
| Cada clip de capa elige su capa: CC 18 al tick 0 con el valor que `layer_from_cc` lleva a esa capa; MORPH 0 / 64 / 127 | test |
| Los contextos copian los archivos de la selección | test |
| Que suenen bien juntos | Juan, en la sesión |

## 8. Fase 2: refinamiento (lo que dejó el check general, 2026-10-28)

| Rol | Resultado del check | Para la fase 2 |
|---|---|---|
| bass | JD SUB MID (capa: GF AFRO SUB + Juno de ROLLER SUB), `bass_adn_octavas`; pasa en A, B y C | variaciones sobre JD SUB MID (sonido y patrón) |
| perc tonal | GF METAL PERC ("super étnico"), con KALIMBA TINE y SHAKER ZAP; `perc_adn_madera` | variaciones sobre METAL PERC |
| pluck | GF TALKING PLUCK + `arp16`; SUNRISE + `motivo` (organic suave) y KALIMBA (hipnótico) | menos brillo, ataque más suave, más cuerpo; contextos D y E |
| lead | ninguno: "muy orientales y extendidos"; base que interesa: DESERT LEAD y GF MELODIC LEAD | **lab dedicado de leads** (abajo) |

**Lab de leads** (Juan: de lo más importante). No se trata de sacar lo "oriental" sino de **manejarlo**: escalas como
la pentatónica y la menor húngara combinadas con la base menor natural (por ejemplo, la segunda aumentada solo en el
remate de una frase), frases cortas que funcionen como gancho de pista, y presets que partan de DESERT LEAD y
GF MELODIC LEAD. Contextos nuevos para probar: D · orgánico suave y E · hipnótico.
