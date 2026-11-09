# Convenciones del workspace de Juan

> **Fecha:** 2026-11-08. **Estado:** acordadas con Juan el 2026-11-08. **Aplica a:** sets de Live, template y motor (`motor-armado.md`). **No modifica** `00–06`.

## 1. Nombres de pista

```
FAMILIA VARIANTE [número]      todo en mayúsculas
CHORDS PLK   CHORDS SUS   BASS SUB   BASS MID   LEAD MAIN   LEAD AUX   PAD 2
```

- **Familia primero:** el papel en el arreglo. Agrupa y ordena, y el motor la usa para decidir.
- **Variante de una lista fija**, solo cuando la familia está en varias pistas.
- **Número** solo para repetidos del mismo tipo.

| Familia | Variantes | Nota |
|---|---|---|
| KICK | — | |
| BASS | — · **SUB** · **MID** | `BASS` a secas = una pista con la capa adentro (como TRESBASS: DX7 medio + Model D una octava abajo con pasabajos a 122 Hz). `BASS SUB` + `BASS MID` = dos pistas con el mismo MIDI: el SUB una octava abajo con pasabajos, el MID con pasaaltos, **el mismo corte y pendiente** en los dos para que la suma quede plana (crossover Linkwitz-Riley, Linkwitz 1976). SUB en mono y sin ataque |
| DRUMS | — | Clave, snare, clap y hats en un rack con cadenas de volumen |
| PERC, TOP | El instrumento: CONGA, TAMBORA, LLAMADOR, SHAKER, GUACHE… | Una pista por instrumento |
| CHORDS | **PLK** (punteado) · **SUS** (sostenido) | Las mismas siglas que los presets: JD PLK WOOD2 en `CHORDS PLK`, JD SUS DARK2 en `CHORDS SUS` |
| LEAD | **MAIN** · **AUX** | `LEAD AUX`, no `AUX LEAD`, para que quede junto al lead |
| PAD, ATMOS | Libre si hay varios (`ATMOS RIO`) | |
| STABS, PLUCKS | — | |
| Transiciones | ROLL, ROLL TRIBE, RISER, FILL, EAR CANDY, FX, SUBDROP | Las crea el motor |
| Técnicas | ARRANGE, SC GHOST, BRAIN | El motor no las arma |

**Nombres anteriores que siguen valiendo:** `CHORDS` = `CHORDS PLK`, `CHORDS 2` = `CHORDS SUS`, `AUX LEAD` = `LEAD AUX`, `MELODY` = `LEAD MAIN`.

## 2. En el armado

La variante cambia la presencia por defecto:

| Pista | Presencia |
|---|---|
| `BASS SUB` | **En todo el track** (Juan: «para darle ese low end»), menos el INTER, que es el silencio del subdrop |
| `BASS MID` | Como `BASS`: drops y parte del outro |
| `CHORDS PLK` | Build y drops |
| `CHORDS SUS` | Intro, break y outro |
| `LEAD MAIN` | Drops y break |
| `LEAD AUX` | Desde el segundo drop |

## 3. Pendiente

Que el motor cree sus pistas con estos nombres (`CHORDS PLK` o `CHORDS SUS` según el patrón, `LEAD MAIN` para el motivo) y con un color por familia.
