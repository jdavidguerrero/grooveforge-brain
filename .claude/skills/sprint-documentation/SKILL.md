# Skill: Sprint Documentation

Template operacional para documentar cada sprint del GrooveForge Brain siguiendo la
filosofía educational-first. Cross-ref: `apps/docs/06-implementation-roadmap.md`.

---

## Principio fundamental

**El theory doc se escribe ANTES de escribir código.**

Si ya hay código y no hay theory doc → escribir el theory doc primero antes de seguir.
El code sin documentación de por qué funciona así no es educational-first.

---

## Estructura de archivos

```
apps/docs/
├── sprints/
│   ├── 01-hello-tone.md          # Sprint 1.1
│   ├── 02-multi-osc-adsr.md      # Sprint 1.2
│   ├── 03-matching-jig.md        # Sprint 1.3
│   └── ...
└── theory/
    ├── i2s-protocol.md           # Conceptos reutilizables
    ├── transistor-ladder.md
    └── tinyml-quantization.md
```

Nomenclatura de sprint docs: `NN-nombre-corto.md` donde `NN` es el número de sprint de
`06-implementation-roadmap.md`.

---

## Template completo de Sprint Doc

```markdown
# Sprint NN — Nombre del Sprint

> **Fase:** N — Nombre de la fase
> **Estimado:** X sesiones (~Xh)
> **Status:** 🔴 Pending / 🟡 In Progress / 🟢 Done
> **Refs:** [spec del sprint en 06-implementation-roadmap.md]

---

## Theory

### Concepto central

[Párrafo de 2-4 oraciones con la idea principal. Empezar con la intuición física/musical,
no con la matemática. Ejemplo: "Un filtro pasa-bajos es como un colador de arena —
deja pasar las frecuencias bajas (granos finos) y bloquea las altas (piedras grandes)."]

### Por qué lo hacemos así

[La decisión de diseño: por qué este approach vs las alternativas. Con tradeoffs explícitos.]

| Approach | Ventaja | Desventaja | Decisión |
|---|---|---|---|
| Opción A | ... | ... | ✅ Elegida |
| Opción B | ... | ... | ❌ Descartada |

### Cómo funciona (más profundo)

[Aquí va la matemática si aplica, la topología del circuito, el pseudocódigo del algoritmo.
Siempre después de la intuición, nunca antes.]

#### [Subsección si aplica]

[Diagramas ASCII, ecuaciones, flow diagrams]

### Relevancia para GrooveForge Brain

[Cómo este concepto conecta específicamente con el producto. No abstracto — concreto:
"esto es lo que hace que el Moog Model D suene distinto a un synth digital".]

### Referencias

- [Autor], "[Título]" — [capítulo/sección específica]
- [Datasheet o paper si aplica]
- [Link si es recurso online verificado]

---

## Wiring (Cableado)

> Etapa previa al código. Documenta el montaje y la verificación del hardware
> **antes** de flashear. Si el sprint no toca hardware nuevo: marcar
> `N/A — sprint solo software` y explicar por qué.

### Hardware del sprint

[Tabla: Componente | Cantidad | Notas — el BOM mínimo de este sprint.]

### Tabla de conexiones

[Tabla: Pad/pin del módulo | Teensy pin | Señal | Requerido en este sprint.
Citar el pin mapping de `01-architecture.md` §3.3.]

### Diagrama

[Diagrama ASCII de las conexiones físicas entre módulos.]

### Montaje paso a paso

[Pasos numerados de armado en protoboard. Regla: empezar sin energía,
GND primero, alimentación después, señales al final.]

### Verificación pre-flash

[Checklist de continuidad/voltaje ANTES de energizar: GND común,
sin shorts 3.3V↔GND, cada señal a su pin correcto.]

---

## Implementation

### Archivos creados / modificados

| Archivo | Descripción |
|---|---|
| `apps/firmware-teensy/src/sketches/NN-nombre.cpp` | Sketch principal del sprint |
| `apps/firmware-teensy/src/engines/moog_model_d.cpp` | Si aplica |
| `apps/firmware-teensy/test/test_NN/test_nombre.cpp` | Tests del sprint |

### Código principal

[Snippet del código más relevante, con comentarios explicando las partes no-obvias.
No pegar el archivo completo — solo la parte que ilustra la decisión clave.]

```cpp
// Ejemplo: AudioMemory y por qué este número
AudioMemory(20);  // 20 bloques × 256 bytes = 5KB
                  // Calculado: 3 osc + mixer + env + vca + 2 conexiones = ~14 bloques
                  // + 6 buffer de seguridad
```

### Constraints respetados

| Constraint | Valor target | Valor medido | Pass |
|---|---|---|---|
| CPU Teensy | ≤60% | XX% (AudioProcessorUsageMax()) | ✅/❌ |
| AudioMemory | ≤400KB | XX bloques × 256B | ✅/❌ |
| Latencia audio | <1ms | No medible en protoboard | N/A |

### Decisiones de implementación

[Por qué se eligió este approach para la implementación específica. Diferente de la
decisión teórica — acá es la decisión práctica de código.]

---

## Demo

### Qué valida este demo

[Qué hito demostrable produce este sprint. Debe ser audible, visible o medible —
no "el código compila".]

### Cómo reproducirlo

```bash
# Instrucciones exactas, copiables
cd apps/firmware-teensy
pio run -e teensy41 -t upload
pio device monitor -b 115200
# Conectar headphones al jack del Audio Shield
# Esperado: tono 440Hz continuo
```

### Evidencia a capturar

- [ ] Grabación de audio (Audacity, QuickTime) — archivo en `apps/docs/sprints/demos/NN-demo.wav`
- [ ] Foto/video del hardware (si hay componentes físicos)
- [ ] Screenshot del serial monitor con métricas CPU/memoria
- [ ] Medición con multímetro (si es hardware)

---

## Tests

### Tests escritos

| Test | Archivo | Tipo | Status |
|---|---|---|---|
| Parámetros dentro de rango | `test_NN/test_params.cpp` | native | ✅ |
| Output no NaN | `test_NN/test_output.cpp` | native | ✅ |
| CPU budget | `test_NN/test_cpu.cpp` | on-device | ✅ |

```bash
# Correr tests
cd apps/firmware-teensy
pio test -e native    # CI
pio test -e teensy41  # on-device
```

---

## Learnings

> Esta sección se completa DESPUÉS de la implementación.
> Vacía hasta que el sprint esté Done.

### Qué salió diferente al plan

[Sorpresas, problemas, cambios de approach durante la implementación]

### Qué tomaría diferente

[Si lo hiciera de nuevo, ¿qué cambiaría?]

### Dependencias para el siguiente sprint

[Qué necesita estar listo antes de empezar el sprint siguiente]

### Tiempo real vs estimado

- Estimado: X sesiones
- Real: X sesiones
- Delta: +/-X (razón: ...)

---

*Sprint NN completado: [fecha]*
*Siguiente sprint: [nombre del siguiente sprint]*
```

---

## Checklist de completitud

Antes de marcar un sprint como 🟢 Done:

- [ ] **Theory escrito antes del código** (si no, retroactivamente documentar)
- [ ] Todas las referencias citadas con capítulo/sección específica
- [ ] Sección "Por qué lo hacemos así" tiene alternativas consideradas
- [ ] Sección Wiring completa (o marcada `N/A — sprint solo software`, justificada)
- [ ] Demo reproducible con comandos exactos
- [ ] Evidencia capturada (audio grabado o foto/video)
- [ ] Tests escritos y pasando
- [ ] Constraints medidos (CPU%, memoria, latencia)
- [ ] Learnings completados post-implementación
- [ ] Tiempo real registrado

---

## Theory docs reutilizables (apps/docs/theory/)

Si el concepto del sprint es fundacional y se va a referenciar en múltiples sprints,
crear un theory doc en `apps/docs/theory/` y referenciar desde el sprint doc:

```markdown
## Theory

Ver `apps/docs/theory/i2s-protocol.md` para el concepto completo de I2S.

En resumen para este sprint: [2-3 oraciones de contexto específico del sprint]
```

Theory docs existentes:
- (vacío al inicio — se van creando a medida que se documentan los sprints)

---

## Ejemplo: Sprint 1.1 — Hello Tone

El sprint 1.1 (Hello World Audio) debería documentar:

**Theory:** Qué es I2S, cómo el Teensy genera MCLK nativo (vs Pi que necesita oscilador
externo), qué es AudioMemory, diseño declarativo de la Teensy Audio Library.

**Implementation:** sketch 440Hz con `AudioSynthWaveform` + `AudioOutputI2S` +
`AudioControlSGTL5000`. `platformio.ini` con `USB_MIDI_AUDIO_SERIAL`.

**Demo:** tono 440Hz audible en el jack del Audio Shield.

**Refs:** `apps/docs/06-implementation-roadmap.md` §2 Sprint 1.1, Teensy Audio Library docs.

---

## Referencias

- `apps/docs/06-implementation-roadmap.md` — lista de sprints y sus theory sections
- `apps/docs/01-architecture.md` — constraints que deben medirse en cada sprint
- Teensy Audio Library: https://www.pjrc.com/teensy/td_libs_Audio.html
