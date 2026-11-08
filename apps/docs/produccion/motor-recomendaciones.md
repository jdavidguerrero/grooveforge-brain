# Motor: recomendaciones de voces y anchura

> **Fecha:** 2026-11-08. **Estado:** aprobado por Juan el 2026-11-08 («no tengo vocales aún, sería la recomendación y cómo aplicarlas… revisar esto también, a nivel de recomendación»); primera entrega en prueba. **Base:** `motor-detalles.md` (huecos del lead), `motor-armado.md`, `motor-mezcla.md`, `convenciones-workspace.md`. **No modifica** `00–06`.

## 1. Objetivo

Leer la **carga** de cada sección del arreglo y sugerir, con su razón, dónde cabe una voz (y de qué tipo) y qué pistas conviene duplicar o abrir. **Solo recomienda:** no escribe nada en Live. Reglas, sin ML; más adelante, el registro de lo que Juan acepta o descarta es el dato para aprender.

## 2. La carga, sección por sección

Se lee el MIDI que suena en cada sección de `ARRANGE`:

| Medida | Cómo | De qué pistas |
|---|---|---|
| **Bandas ocupadas** | Fracción del tiempo con alguna fundamental en cada banda: grave (< 175 Hz), medio-grave (175–350), medio (350–700), agudo (> 700) | Las melódicas: BASS, LEAD, CHORDS, PAD, ATMOS, STABS, PLUCKS (no la percusión: sus notas son pads, no alturas) |
| **Silencio del lead** | Fracción de semicorcheas sin nota de `LEAD*` | LEAD, LEAD MAIN, LEAD AUX, MELODY |
| **Densidad de percusión** | Notas por compás | PERC, TOP, DRUMS, FILL |

## 3. Voces

| Si en la sección… | Recomienda | Por qué |
|---|---|---|
| El lead calla al menos el 60 % | **Frase o hook** | Hay espacio para que se entienda una palabra |
| El lead calla menos del 60 % | **Chop rítmico en los huecos del lead**, o nada | Una frase competiría con el lead |
| El agudo (> 350 Hz) está más ocupado que el medio-grave | **Voz masculina** o canto grave | Fundamental masculina ≈ 85–330 Hz: deja libre al lead |
| El medio-grave está más ocupado | **Voz femenina** o chop agudo | Fundamental femenina ≈ 175–1000 Hz: va por encima |
| Intro o break con percusión densa (≥ 8 notas por compás) | Además, **coro tribal** corto, en llamada y respuesta | El ADN orgánico de Juan |

Los rangos de voz son los de la fundamental hablada y cantada (Titze, *Principles of Voice Production*, 1994).

**Cómo aplicarlas** (se muestra junto a la recomendación): llevar la voz a La (transponer o warp, regla de la fundamental), pasaaltos a ~150 Hz, de-esser, y envíos **B · Plate** y **F · Echo**. Para un chop: Simpler en modo *Slice* o, más adelante, el chop automático del motor (cortes por sílaba, cada uno llevado a una nota de la escala, MIDI en los huecos del lead).

## 4. Duplicación y anchura

Por rol presente en el set:

| Rol | Recomendación | Cómo |
|---|---|---|
| KICK, BASS, BASS SUB | **Mono**, sin duplicar | Utility con *Bass Mono* (≈ 120 Hz). En mono (club, celular) lo ancho del grave se cancela |
| BASS MID | Mono abajo; puede abrirse arriba | Utility *Bass Mono* + ancho leve |
| LEAD MAIN | Al centro; **octava arriba** bajita si la sección está vacía en el agudo | Capa o pista con el mismo MIDI +12 |
| LEAD AUX | **Doble con MIDI**: misma melodía, 10–20 ms corrida, velocity distinta, paneada al lado opuesto | Segunda pista |
| STABS, PLUCKS, CHORDS PLK | **Haas**: un lado retrasado 10–20 ms | Delay del rack de rol: sin sync, L 1 ms, R 15 ms, feedback 0, mezcla 100 % |
| CHORDS SUS, PAD, ATMOS | **Ancho** 120–140 %, o chorus | Utility (Width) o el chorus del preset |
| PERC, TOP | **Sin Haas**: paneo en pareja (mezcla de partida) y envío A Office | Para más ancho: separar las manos **L y R** de Tribe a lados opuestos (el kit ya alterna L/R por semicorchea), o doblar con otra toma corrida 5–10 ms |
| DRUMS | Al centro, sin Haas | El ancho lo da el envío A Office |

**Por qué no Haas en la percusión** (Juan, 2026-11-08): en un golpe corto, la copia retrasada no se funde: se oye como doble golpe (flam). Y en mono, un shaker o un hat (ruido de banda ancha) más su copia a 10 ms suma con muescas cada 100 Hz: suena «fasero». Lo que sí abre sin riesgo es que cada lado suene distinto: las manos L y R de Tribe, o dos tomas.

**Límite del Haas:** de 30–40 ms en adelante deja de oírse como ancho y se oye como eco, y en mono crea filtro peine (Haas, 1951). Siempre se revisa la mezcla en mono.

## 5. Rack de rol (`GF ROL.adg`)

Plantilla que guarda Juan una vez: **EQ Eight → Compressor → Saturator → Delay → Utility**, todo por defecto. El motor la clona por rol y enciende o apaga cada dispositivo (tabla del rack, `motor-mezcla.md` §4); el Delay solo se enciende donde la tabla de §4 pide Haas.

## 6. No cubre

Generar voces (no se sintetizan con IA: calidad y derechos dudosos), el chop automático (cuando Juan tenga material) y escribir la duplicación en Live (por ahora es recomendación).
