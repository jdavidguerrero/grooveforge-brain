# Sprint 42 — Smart Arpeggiator

> **Fase:** 6 — Layer 1 AI completo
> **Estimado:** 1 sesión (~3h)
> **Status:** 🟢 Done — pendiente verificación on-device
> **Refs:** `apps/docs/06-implementation-roadmap.md §Sprint 6.3`

---

## Theory

### Concepto central

Un arpegiador clásico sube y baja mecánicamente por las notas que el músico presiona. Un arpegiador "smart" hace lo mismo, pero tiene contexto musical: sabe en qué tonalidad estás (Key Detector), qué acorde sonás (Chord Recognizer) y a qué tempo vas (Beat Follower). Con ese contexto puede enriquecer el arpeggio con notas del acorde que el músico no presionó, y elegir el siguiente paso con transiciones que suenan musicalmente coherentes.

### Por qué Markov chain y no red neuronal

| Approach | Ventaja | Desventaja | Decisión |
|---|---|---|---|
| Red neuronal (LSTM) | Aprendido de música real | ~200KB tensor arena, latencia >20ms | No |
| Markov chain | CPU trivial (<0.1%), sin arena, musicalmente tunable | Patrones algo predecibles | Si |
| Reglas fijas (UP/DOWN) | Determinístico, cero CPU | No "smart", sin variación | Modo secundario |

La Markov chain modela qué tan probable es moverse de la nota actual a otra. En música, las probabilidades están condicionadas por el acorde detectado: los chord tones (raíz, tercera, quinta) tienen mayor probabilidad de ser visitados que notas de paso.

### Cómo funciona

**Construcción del "arp note set":**

En modo SMART, el conjunto de notas disponibles no son solo las que el músico presiona. Se enriquece con los chord tones del acorde detectado en el registro más cercano:

```
Músico presiona: E3, G3, C4  (C mayor)
ChordRecognizer detecta: C maj (root=C, type=major)
Chord tones: C, E, G
Arp note set (after enrichment): E3, G3, C4, G3, E4  → sorted → [E3, G3, C4, E4]
```

**Cadena de Markov:**

La transición desde la posición actual `i` en el arp_note_set sigue estas probabilidades:

```
i → i+1  (paso adelante):   40%  — movimiento melódico natural
i → i-1  (paso atrás):      20%  — retroceso ocasional
i → i    (repetir):         15%  — énfasis/stutter
i → i+2  (salto adelante):  15%  — salto pequeño
i → rand (aleatorio):       10%  — variedad
```

Este perfil produce movimiento mayoritariamente stepwise (lo que hace que la música suene "humana") con saltos esporádicos que dan interés.

**Timing:**

El paso del arp está sincronizado al BPM del Beat Follower:
```
step_ms = 60000 / (bpm * subdivision)
subdivision = 1 (negra), 2 (corchea), 4 (semicorchea)
```

Default: corchea (1/8 note) a 120 BPM = 250ms por paso.

**Gate length:**

El gate determina qué fracción del step period la nota suena antes de apagarse:
- gate = 0.7 → nota suena 175ms, silencio 75ms (staccato natural)
- gate = 1.0 → legato (nota dura hasta el siguiente paso)

```
noteOn  en t=0ms
noteOff en t=175ms  (gate 0.7 × 250ms)
next noteOn en t=250ms
```

### Relevancia para GrooveForge Brain

El Smart Arpeggiator convierte el Brain en un compositor en tiempo real. El usuario sostiene un acorde y el Brain genera una melodía que:
- Siempre está en la tonalidad detectada
- Usa notas del acorde actual
- Se sincroniza al BPM de lo que está tocando

Es el buy-reason Tier S del roadmap: "mantenés un acorde → arpegio inteligente, en escala, en tempo."

### Referencias

- Norris, J.R. "Markov Chains." Cambridge University Press, 1998. §1 — fundamentos de cadenas de Markov discretas.
- Huron, D. "Sweet Anticipation." MIT Press, 2006. §6 — movimiento melódico y expectativa.
- D'Errico, M. "The Arpeggiator." Sound on Sound, Jan 2012 — implementaciones históricas.

---

## Wiring

N/A — sprint solo software. No hay hardware nuevo. El arpeggiator opera sobre el path MIDI existente (USB-A host → MidiHost → sketch → engines).

---

## Implementation

### Archivos creados

| Archivo | Descripción |
|---|---|
| `src/ml/smart_arp.h` | Clase SmartArp — arpeggiador con Markov chain |
| `src/ml/smart_arp.cpp` | Implementación |
| `src/sketches/28-synth-navigator.cpp` | Integración: MIDI intercept + loop tick + AI context |

### Activación

El arp se activa automáticamente cuando el usuario entra en AI mode (hold ENC NAV 4s). En AI mode + notas sostenidas → arp activo. Sin notas → silencio. Al salir de AI mode → arp desactivado.

Division y gate son hardcodeados en v1 (1/8 note, gate=0.7). Parámetros expuestos en sprint posterior.

### Constraints respetados

| Constraint | Valor target | Estimado |
|---|---|---|
| CPU | ≤60% total | <0.1% adicional (solo aritmética enteros) |
| Latencia audio | <1ms | No afecta audio path |
| Timing step | ±2ms de jitter (loop granularity) | Aceptable para 1/8 note (250ms) |

---

## Demo

```bash
cd apps/firmware-teensy
pio run -e sketch -t upload
# Conectar teclado MIDI USB
# Activar AI mode (hold ENC NAV 4s)
# Mantener 2-3 notas sostenidas
# Esperado: arpegio inteligente, sincronizado al tempo
```

---

## Learnings

### Qué salió diferente al plan

- El `SmartArp` intercepta notas post-Scale Lock (`play_note`), no el raw MIDI — correcto: el arp opera sobre notas ya cuantizadas a la escala.
- La instancia `arp` en el sketch es `ARP2600` (engine de audio) — mismo nombre que el arpeggiator pero distinto tipo. No hubo colisión porque el arpeggiator se llama `g_smart_arp`.
- División y gate hardcodeados en v1 (EIGHTH, 0.7) — expuestos en Sprint 45 (AI Rack) como params ENC R.

### Pendiente on-device

- [ ] Verificar que el Markov chain suena musical (no predecible ni caótico)
- [ ] Verificar timing de steps con teclado MIDI conectado
- [ ] Verificar que noteOff del arp no deja notas colgadas entre engines

### Dependencias para el siguiente sprint

- Sprint 43 (Groove Humanizer) integrado directamente en SmartArp.
- Sprint 45 (AI Rack) expone modo/división/groove en la pantalla.

---

*Sprint 42 completado — pendiente verificación on-device*
*Siguiente: Sprint 43 — Groove Humanizer*
