# Sprint 44 — Velocity Curve Learn

> **Fase:** 6 — Layer 1 AI completo
> **Status:** 🟢 Done — pendiente verificación on-device
> **Refs:** `apps/docs/06-implementation-roadmap.md §Sprint 6.5`

## Theory

### Concepto central

MIDI velocity va de 0 a 127. Los teclados traducen la fuerza de toque a ese rango, pero distintos jugadores tienen perfiles muy diferentes: un pianista clásico toca forte/piano con mucha dinámica; un productor de beats puede tocar siempre en un rango medio comprimido. Si el Brain usa el velocity raw, un jugador con poco rango dinámico escuchará el synth siempre al mismo volumen.

La curva de velocity aprende el rango real del usuario en sus primeras N notas y extiende ese rango para ocupar todo el espectro 0-127. Es calibración adaptativa, no ML en sentido estricto.

### Algoritmo

1. Acumular histograma de velocity (128 buckets) sobre primeras 64 notas.
2. Calcular percentil 5 (P5, jugador suave) y percentil 95 (P95, jugador fuerte).
3. Curva: mapeo lineal [P5, P95] → [0, 127]. Velocidades fuera del rango se clipean.
4. Se recalibra si el usuario toca "fuera de perfil" (nuevo P95 > 1.2× anterior).

### Por qué percentiles y no min/max

El min/max es sensible a outliers: si el usuario golpea una tecla accidentalmente muy fuerte, el P95 no cambia, pero el max sí, y toda la curva se comprimiría innecesariamente.

### Referencias
- Repp, B.H. (1997). "The aesthetic quality of a quantitatively average music performance." *Music Perception*, 14(4), 419-444.
- Shaffer, L.H. (1981). "Performances of Chopin, Bach, and Bartók." *Cognitive Psychology*, 13(3), 326-376.

## Wiring
N/A — sprint solo software.

## Implementation

### Archivos creados
| Archivo | Descripción |
|---|---|
| `src/ml/velocity_learn.h` | Clase VelocityCurve |
| `src/ml/velocity_learn.cpp` | Implementación |
| `src/sketches/28-synth-navigator.cpp` | Integrar record()/apply() en midi_note_on; g_vel_curve.reset() en setup() |
| `platformio.ini` | +<ml/velocity_learn.cpp> en build_src_filter del [env:sketch] |

## Demo
Dos usuarios con fuerza de toque distinta → el Brain se adapta a cada uno. El usuario tímido escucha el synth con el mismo rango dinámico que el usuario fuerte.

## Learnings
> Completar post-implementación.
