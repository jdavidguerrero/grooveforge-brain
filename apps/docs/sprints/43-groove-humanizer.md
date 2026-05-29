# Sprint 43 — Groove Humanizer

> **Fase:** 6 — Layer 1 AI completo
> **Status:** 🟢 Done — pendiente verificación on-device
> **Refs:** `apps/docs/06-implementation-roadmap.md §Sprint 6.4`

## Theory

### Concepto central

El Smart Arpeggiator dispara notas exactamente en la grid de tiempo. Eso suena como una máquina — correcto pero sin vida. Los músicos humanos tienen micro-timing: tocan consistentemente antes o después del beat en patrones que reflejan su estilo. El Groove Humanizer aplica desviaciones de timing controladas para que el arpegio suene humano.

### Dos tipos de humanización

**Swing (shuffle):** el patrón 2:1 clásico del jazz y el funk. Los pares de corcheas se dividen en negra con puntillo + corchea en lugar de dos corcheas iguales. A 120 BPM con 67% swing: primer paso = 167ms, segundo = 83ms.

**Human (jitter):** ruido Gaussiano ~σ=15ms aplicado a cada paso. Investigaciones de Friberg & Sundström (2002) muestran que σ ≈ 10-20ms caracteriza el timing de pianistas expertos. Con σ pequeño suena "tight", con σ grande suena "sloppy".

### Por qué no aprender del usuario en v1

Un perfil de groove personalizado requiere ~100 notas para estabilizarse. En v1 usamos perfiles predefinidos (OFF/HUMAN/SWING) con un parámetro de intensidad (amount 0-1). Un perfil personalizado es sprint futuro.

### Integración con SmartArp

El humanizador modifica cuándo se dispara cada paso. SmartArp tiene `_last_step_ms` y calcula si `now - _last_step_ms >= period`. El humanizador agrega un offset:
```
adjusted_period = period + humanizer.offset_ms(step_parity, period)
```
step_parity (0=par, 1=impar) permite el efecto swing alternado.

### Referencias
- Friberg, A. & Sundström, A. (2002). "Swing ratios and ensemble timing in jazz performance." *Music Perception*, 19(3), 333-349.
- Prögler, J.A. (1995). "Searching for swing: Participatory discrepancies in the jazz rhythm section." *Ethnomusicology*, 39(1), 21-54.

## Wiring
N/A — sprint solo software.

## Implementation

### Archivos creados
| Archivo | Descripción |
|---|---|
| `src/ml/groove.h` | Clase GrooveHumanizer |
| `src/ml/groove.cpp` | Implementación |
| `src/ml/smart_arp.h` | Agregar set_groove(), _humanizer, _step_parity |
| `src/ml/smart_arp.cpp` | Agregar set_groove(); usar humanizador en update() |
| `platformio.ini` | +<ml/groove.cpp> en build_src_filter del [env:sketch] |

## Demo
A/B audible: con amount=0 el arp suena robótico; con amount=1 y SWING suena como jazz.

## Learnings
> Completar post-implementación.
