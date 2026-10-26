# Auditoría de "vida analógica" de los engines (fase 2, tanda 2)

> **Fecha:** 2026-10-26 · **Estado:** auditoría de código + un arreglo (suavizado de filtros) · **Depende de:**
> `caracter-sonoro.md` §9–10 (manifiesto: "cero plástico, cero estéril").

Qué hace que un synth digital suene "plástico" y cómo está cada cosa en el código del Brain. Revisado leyendo el
código; lo que se oye se confirma en el hardware.

| Síntoma "digital" | Cómo está hoy | Veredicto |
|---|---|---|
| Osciladores que reinician la fase en cada nota (todas las notas arrancan igual) | `PolyBlepCore` (Juno, OB-6, ARP, Model D) y `Prophet5Osc` corren libres: la fase solo se pone en 0 al crear el oscilador | ✅ Bien |
| Afinación perfecta y estática | Cada oscilador tiene **deriva continua** (camino aleatorio filtrado, ~±0.45 ¢ por defecto, semilla distinta por voz) + **voice cards** fijas por voz (`voice_spread.h`) + **DRIFT** por nota (theory/09 §6) | ✅ Bien |
| Aliasing en saw / pulse | PolyBLEP en todas las formas con discontinuidad | ✅ Bien |
| Todas las voces idénticas | Voice cards: desafinación y cutoff distintos por voz | ✅ Bien |
| **Zipper** al mover cutoff / resonancia (escalones audibles, sobre todo con resonancia alta) | El ladder (`ZdfLadder`) toma el coeficiente nuevo de golpe; el SEM del OB-6 lo cambia una vez por bloque (2.9 ms) | ⚠️ **Se arregla** (abajo) |
| DX7 con la fase de los operadores en 0 en cada nota | Así funciona el TX81Z / DX7 con KEY SYNC: da ataques consistentes en la percusión tonal | ✅ Correcto para su rol |
| Silencio absoluto entre notas | No hay ruido de fondo; el hiss del BBD del Juno no está modelado | ➖ Opcional, más adelante (caracter-sonoro fase 2) |

## Arreglo: suavizado de coeficientes del filtro

- **Ladder** (Model D, Juno, Prophet, ARP): el coeficiente `G` y la resonancia `k` siguen a su valor nuevo con un
  suavizado de un polo por muestra (constante ≈ 1 ms). Es lo bastante corto para no ablandar el "slap" de una
  envolvente rápida y lo bastante largo para borrar los escalones de una perilla. El primer valor se toma directo.
- **SEM** (OB-6): `g` y `k` se interpolan dentro del bloque y los coeficientes derivados se recalculan cada 4
  muestras (≈ 0.1 ms), en lugar de saltar una vez por bloque.
- Costo: dos multiplicaciones-suma por muestra y por voz en el ladder; en el SEM, una división cada 4 muestras.

## Qué queda

Hiss opcional del BBD del Juno, y lo que salga de escuchar los pilotos en el hardware.
