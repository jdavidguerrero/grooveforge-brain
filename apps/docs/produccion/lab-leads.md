# Lab de leads: manejar el color, no quitarlo

> **Fecha:** 2026-10-28 · **Estado:** propuesta para Juan (paso 1 de 4; el generador, los presets y los contextos D / E
> esperan su visto bueno) · **Depende de:** `sesion-paleta-adn.md` §1b y §8, `protocolo-captura.md` §3 y §5,
> `ia-local-refinamiento.md` §2 · **Ficha:** `metadata/sesiones/2026-10-28_lead.json`

## 1. Qué pasó en el check y por qué

Ningún lead pasó. Tus palabras: "muy orientales y extendidos"; sin percusión, "notas demasiado largas y estridentes".
Quieres leads que **mezclen notas largas y cortas, más grooveros, no tan extensos**, sin perder el color exótico:
manejar la pentatónica y la menor húngara sobre la base de La menor natural. Revisando los patrones y los presets, las
tres quejas tienen causas concretas:

| Lo que oíste | Causa en los patrones (`make_adn_clips.py`) | Causa en los presets (`preset_bank.cpp`) |
|---|---|---|
| **Extendidos** | `lead_adn_frase` y `lead_adn_profunda` llenan los 64 semicorcheas de los 4 compases: **cero silencios**. La nota más corta de `frase` es una corchea; el promedio, 5 semicorcheas | Sustain 0,7–0,8 y release 350–400 ms: cada nota se pega a la siguiente |
| **Orientales** | `frase` sube y baja la pentatónica por grados (La Do Re Mi Sol Mi La…) con notas largas y en registro alto: el cliché "asiático" de la pentatónica | GF MELODIC LEAD trae vibrato fijo (LFO 5,5 Hz → pitch 0,08); la capa A de DESERT LEAD, un notch que barre lento. Notas largas + vibrato + barrido = "desierto" |
| **Estridentes** | Registro La4–La5 (440–880 Hz): los armónicos caen en 2–5 kHz, la zona donde el oído es más sensible | Ataque de 3–5 ms en sierras, cutoff 1,8–2,5 kHz con poco recorrido de envolvente, **sync** en la capa B (Prophet), unison ×2 con 18 ¢ |

`lead_adn_hook` es el único que ya respira (ocupa ~60 % del tiempo y mezcla corcheas con semicorcheas): es el punto de
partida rítmico, no la frase.

## 2. Las escalas, en La

Los grados se cuentan desde La (1 = La). Las notas en **negrita** son las que cambian respecto a la menor natural.

| Escala | Notas | Grados | Qué carácter da |
|---|---|---|---|
| Pentatónica menor | La Do Re Mi Sol | 1 ♭3 4 5 ♭7 | Sin semitonos: nada choca, todo "funciona". Por eso se vuelve genérica, y con notas largas en registro alto suena a Oriente |
| Menor natural (eólica) | La Si Do Re Mi Fa Sol | 1 2 ♭3 4 5 ♭6 ♭7 | La base del ADN. El **Fa (♭6)** es la nota oscura y profunda; el Si, la que tensa hacia Do |
| Menor armónica | La Si Do Re Mi Fa **Sol#** | 1 2 ♭3 4 5 ♭6 **7** | Un salto de **segunda aumentada** (Fa → Sol#, 3 semitonos entre grados vecinos) |
| Menor húngara | La Si Do **Re#** Mi Fa **Sol#** | 1 2 ♭3 **♯4** 5 ♭6 **7** | **Dos** segundas aumentadas (Do → Re# y Fa → Sol#). Es el sonido "gitano / Medio Oriente" más fuerte |

**El color exótico es la segunda aumentada.** Un salto de tres semitonos entre dos notas *vecinas* de la escala no
existe en la menor natural ni en la mayor; el oído occidental lo asocia con el maqam Hijaz y la música gitana. Cuantas
más veces aparece, cuanto más dura y cuanto más fuerte cae en el compás, más "oriental" suena (Hewitt, cap. de escalas
y modos; Persichetti, cap. 2).

### Las notas exóticas contra la progresión Am – F – C – G

| Nota | Contra Am | Contra F | Contra C | Contra G | Uso seguro |
|---|---|---|---|---|---|
| **Re#** (♯4) | tensión de blues (la "blue note") | suena como Mi♭, la ♭7 de F: pasa | ♯9 de C, choca | choca | **de paso**: Re → Re# → Mi, corta, en tiempo débil sobre Am. Es la nota exótica más "familiar" (escala de blues) |
| **Sol#** (7) | sensible de La: resuelve fuerte | choca con la tercera (La) solo si se sostiene | choca | **choca** (semitono contra el Sol del acorde) | **remate**: Fa → Sol# → La, justo antes del 1 del Am que vuelve, con el Sol# corto |

Conclusión práctica: en esta progresión, el color húngaro vive **sobre el Am** o **cruzando de G a Am** (el remate de
la vuelta). Sobre F y C conviene la menor natural.

## 3. Niveles de color (lo que vamos a comparar)

La misma frase, con la misma rítmica, en cuatro niveles. Cambia solo qué notas usa y dónde:

| Nivel | Nombre | Qué cambia | Para qué parte del track |
|---|---|---|---|
| **0** | pentatónica | solo La Do Re Mi Sol | referencia (lo que ya no te gustó, pero con la rítmica nueva) |
| **1** | menor | menor natural; el Fa y el Si aparecen en tiempo débil | el loop del drop: el ADN "limpio" |
| **2** | remate húngaro | nivel 1 + **una** segunda aumentada al final de la vuelta (Fa → Sol# → La) y, como mucho, un Re# de paso | el drop con firma: lo exótico como puntuación |
| **3** | húngara completa | la menor húngara en el compás de Am (Re# y Sol#), menor natural en el resto | un momento: break, segunda vuelta, fill; no para 32 compases |

Reglas que cumplen los niveles 2 y 3 (es lo que "maneja" el color):

1. **Corta y en tiempo débil:** una nota exótica dura como mucho una corchea y no cae en la negra (salvo el Sol# en
   nivel 3 sobre Am).
2. **Resuelve por grado:** Re# va a Mi, Sol# va a La. Nunca se queda colgada.
3. **Una vez por vuelta** (4 compases) en el nivel 2.
4. **Sin vibrato en la nota exótica:** el vibrato lento sobre la segunda aumentada es lo que más la vuelve "desierto".

En la sesión: con máximo tres candidatos por decisión (`protocolo-captura.md` §5) se elige primero la **rítmica** en
nivel 1, y después el **nivel de color** (1 · 2 · 3) sobre la rítmica ganadora. El nivel 0 queda como referencia.

## 4. Una frase de lead de club: el ritmo primero

En un track de club el lead compite con el bajo y la percusión por el mismo espacio rítmico. Un gancho funciona porque
**se recuerda por su ritmo** y luego por sus notas (Snoman, *Dance Music Manual*, cap. de melodía y riffs; Hooktheory
I, cap. de ritmo melódico). Lo que eso pide:

| Principio | Cómo se ve en la rejilla de 16 semicorcheas |
|---|---|
| **1–2 compases que se repiten** | motivo de 1 compás (A) o de 2 (A + respuesta); la vuelta de 4 compases es A A A B o A A' A B' |
| **Mezcla corta-larga** | varias notas cortas (semicorchea / corchea, como un pluck) que **desembocan en una larga** (negra con punto o blanca). Lo largo es el premio, no la regla |
| **Silencio** | ~40–50 % del tiempo sin nota: deja sonar la tambora y el shaker. Ninguna vuelta llena los 4 compases |
| **Síncopa** | entrar en la "y" o en la última semicorchea antes del tiempo; anticipar el 1 del compás siguiente una semicorchea |
| **Lo largo en tono del acorde** | la nota larga cae en La, Do, Mi (Am), Fa / La (F), Do / Mi (C), Sol / Si (G) o en la 9.ª / 7.ª del acorde para lo "profundo" |
| **Registro más bajo** | el centro en La3–Mi5 en lugar de La4–La5; lo "profundo" una octava abajo |
| **Velocity con intención** | cortas a 70–85, las del acento y la larga a 95–105; un ghost a 50 que con `vel_cut` se oscurece |

Tres rítmicas para comparar (x = corta, X = acento, `=` = la nota sigue sonando, `.` = silencio):

```
            1 . . . 2 . . . 3 . . . 4 . . .
gancho      . . x x . . X = = . x . x . . .     corta-corta-LARGA, respuesta corta; 1 compás que se repite
pregunta    x . x X = = = . . . . . . . . .     pregunta en el compás 1…
 respuesta  . . . . . . x . x . x X = = = .     …respuesta en el 2 (desplazada); 2 compases
profunda    X = = = = = . . . x . x . . . .     una larga grave y dos cortas que la empujan; 1 compás, octava abajo
```

Las notas se eligen después, sobre esta rejilla, y el nivel de color (§3) decide cuáles. El ritmo no cambia entre
niveles: así lo que comparas es solo el color.

## 5. Qué vuelve estridente un lead (y qué hacer en el Brain)

"Estridente" es casi siempre **demasiada energía entre 2 y 5 kHz** sostenida en el tiempo. Ahí el oído es más sensible
(curvas de igual sonoridad, ISO 226): a igual nivel, esa banda se siente más fuerte y cansa antes.

| Causa | Por qué | Qué hacer (parámetro del Brain) |
|---|---|---|
| Cutoff alto y fijo | la sierra deja pasar todos sus armónicos todo el tiempo | bajar `cutoff_hz` (de 1,8–2,5 kHz a ~1,0–1,4 kHz) y que el **brillo lo dé la envolvente del filtro**: `env_amt` alto, `f_decay` corto (150–250 ms), `f_sustain` bajo. Las cortas "hablan", las largas se redondean solas |
| Resonancia | un pico estrecho que realza justo la zona sensible | `reso` ≤ 0,2 |
| Ataque muy rápido | 3–5 ms en una sierra = un clic de banda ancha en cada nota | ataque de amplitud 10–20 ms (sigue sonando corto, pierde el clic) |
| Sync (Prophet, capa B) | el sync agrega armónicos altos y agresivos: es su función | apagarlo o bajar el barrido; dejar el Prophet como cuerpo |
| Unison / detune | las voces desafinadas baten y llenan los agudos | ×2 con 8–12 ¢, o unison solo en una de las mitades del híbrido |
| Sustain / release largos | las notas se solapan y el agudo nunca descansa | sustain 0,5–0,6, release 150–250 ms |
| Registro | en La4–La5 los armónicos 3.º–6.º ya están en 1,3–5 kHz | tocar una octava abajo y sumar **cuerpo** con un poco de GRIT / drive (armónicos en la zona media, no arriba) |
| Vibrato fijo | ondula justo el agudo y, en notas largas, suena "desierto" | sacarlo del preset (LFO → pitch en 0) y dejarlo para la rueda / MOVE |
| Velocity sin efecto en el filtro | las cortas suenan igual de brillantes que los acentos | `vel_cut` ~50–70: los ghost notes se oscurecen, los acentos muerden |

Referencias: Gordon Reid, *Synth Secrets* (Sound On Sound, 1999–2004), entregas sobre filtros, resonancia y
envolventes; Fred Welsh, *Synthesizer Cookbook*, vol. 1 (leads: envolvente de filtro vs. cutoff fijo); Snoman, *Dance
Music Manual* (4.ª ed., 2019), cap. de leads.

### Los presets "JD LEAD …" que saldrían de aquí (paso 3)

| Nuevo | Parte de | Cambios principales |
|---|---|---|
| JD LEAD DESIERTO SUAVE (nombre a elegir) | DESERT LEAD (capa A OB-6 + B Prophet) | B sin sync y con cutoff más bajo; A sin el barrido lento del notch o mucho más leve; ataque 10–15 ms; release ~200 ms |
| JD LEAD MELÓDICO REDONDO (nombre a elegir) | GF MELODIC LEAD (OB-6, A-08) | sin vibrato fijo; cutoff ~1,2 kHz con envolvente de filtro corta; unison 10 ¢; GRIT un poco más para el cuerpo; `vel_cut` ~60 |

Límite conocido: el panel aún no carga un preset de engine dentro de una mitad de un híbrido, así que la capa nueva se
arma con `gf_presets.py` (`layers` / `layer-import`), con el puerto serie de la Teensy libre.

## 6. Contradicciones que hay que decidir (no las resuelvo yo)

1. **`sesion-paleta-adn.md` §1b** describe las melodías profundas como "motivos menores, **notas largas**, novenas y
   séptimas, espacio", y §3 define `lead_adn_frase` como "frase pentatónica con notas largas". Lo que dijiste en el
   check (mezclar largas y cortas, no tan extensos) contradice "notas largas" como regla. Propuesta: que §1b diga
   "notas largas **como remate** de frases cortas". ¿Lo cambio así?
2. **Test actual:** `test_patrones_en_la_menor_y_en_su_largo` exige que **todas** las notas de los patrones ADN estén
   en la menor natural. Los niveles 2 y 3 (Re#, Sol#) lo romperían si van en `make_adn_clips.py`. Propuesta: un
   generador aparte (`tools/paleta-lab/make_lead_lab.py`) con sus propios tests, que escribe en
   `midi/adn/lead/lab/`; los tres leads viejos quedan como están, de referencia.

## 7. Cómo se prueba (paso 2)

| Qué | Cómo |
|---|---|
| Cada clip dura su largo (4 compases), notas en rango MIDI, registro dentro de La3–Mi5 (Do3 en "profunda") | test (pytest) |
| Hay silencio: la suma de duraciones ≤ 60 % de la vuelta | test |
| Mezcla corta-larga: ≥ 50 % de las notas ≤ 2 semicorcheas y al menos una ≥ 4 semicorcheas cada 2 compases | test |
| Nivel 0 solo pentatónica; nivel 1 solo menor natural | test |
| Nivel 2: exactamente una segunda aumentada (Fa → Sol#) por vuelta, el Sol# ≤ 2 semicorcheas, seguido de La | test |
| Nivel 3: Re# y Sol# solo en el compás de Am; cada uno resuelve por grado (Re# → Mi, Sol# → La) | test |
| Las tres rítmicas son iguales entre niveles (mismos tiempos y duraciones, cambian solo las alturas) | test |
| Que suene groovero, profundo y con la dosis justa de color | tú, en los contextos A–C y los nuevos D / E |

## 8. Lo que viene después de tu visto bueno

1. **Generador** (`make_lead_lab.py`): 3 rítmicas × 4 niveles = 12 clips en `midi/adn/lead/lab/`, con tests; después
   `fill_lab_set.py` sobre una **copia nueva** de tu set (`GFExploration Project/`), sin tocar el que tienes abierto.
2. **Presets JD LEAD** (§5), subidos con `gf_presets.py`, sin borrar ninguno.
3. **Contextos D · orgánico suave** (`organic_shaker_wood`, solo top) y **E · hipnótico** (`tambora_hipnotica`, poco
   top) en el set.

Sesiones de elección sin grabar; los ganadores se graban todos al final (`protocolo-captura.md` §3).
