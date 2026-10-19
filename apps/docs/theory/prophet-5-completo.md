# Prophet-5 completo: POLY-MOD, WHEEL-MOD, sync y el panel en 8 controles por parada

> **Fecha:** 2026-10-16 · **Estado:** APROBADO por Juan ("implementa 1") · **Engine:** `src/engines/prophet_5.*`
> **Depende de:** theory/lfo-modulacion.md (LFO global, `LfoMod`), theory/04 de Studio §3.1 (4 encoders + SHIFT),
> sprint 07 (`apps/docs/sprints/07-prophet-5.md`, engine original).

## 1. El problema

El engine cubre ~40 % del panel del Prophet-5 rev 4. Falta lo que le da el carácter:

- **POLY-MOD**: el envelope del filtro y el OSC B modulan la frecuencia de A, el ancho de pulso de A y el filtro,
  **por voz y a velocidad de audio**. Hoy hay una aproximación (`setCrossmod`) que mueve la frecuencia de A con un
  seno calculado a 1 kHz: con un OSC B de 200 Hz eso es aliasing de control, no FM.
- **SYNC** de A a B, saw y pulse **a la vez** en A, PW manual, FINE, LO FREQ y KEYBOARD de B, NOISE en el mixer.
- **WHEEL-MOD**: la rueda reparte LFO / ruido a FREQ A, FREQ B, PW A, PW B y FILTER por separado.
- Keyboard tracking del filtro y GLIDE.

## 2. Por qué un oscilador propio (código custom)

| Necesidad | Teensy Audio Library | Oscilador propio |
|---|---|---|
| OSC B → FREQ A a velocidad de audio | `AudioSynthWaveformModulated` tiene entrada FM, pero el OSC B tendría que entrar por el grafo: 5 voces × mezclador de modulación | la salida de B está en la misma muestra |
| Sync A ← B | no existe | se reinicia la fase de A cuando B da la vuelta |
| Saw + pulse a la vez en A | dos objetos por oscilador | se suman en la misma muestra |
| Costo | ~6 objetos por voz | 1 objeto por voz |

Es la misma conclusión que ADR-005 para el DX7: lo que depende de la muestra anterior (FM y sync) no se puede
partir en bloques de 128 muestras.

**`Prophet5OscCore`** (lógica pura, testeable en `native`) + **`Prophet5Osc`** (subclase de `AudioStream` que
llama al core en `update()`): un objeto por voz que genera A, B y ruido ya mezclados. El filtro `MoogLadder4P` y el
VCA (`AudioEffectEnvelope`) siguen igual.

### 2.1 Por muestra

```
B:  fase_b += inc_b                       (inc_b fijo en el bloque: nota, intervalo, FINE, LO FREQ, KEYBOARD)
    b = saw_b·on + tri_b·on + pulse_b·on   (saw y pulse con polyBLEP, tri ingenuo)
    si fase_b dio la vuelta y SYNC: fase_a = 0
A:  m = pm_b·b + pm_env·env               (POLY-MOD, solo si el destino FREQ A está encendido)
    fase_a += inc_a · 2^(GF_PM_OCT · m)    (2^x con una aproximación polinómica de 3.er grado)
    pw_a = PW + (destino PW A ? 0.45·m : 0) + wheel/LFO
    a = saw_a·on + pulse_a·on
salida = a·nivel_A + b·nivel_B + ruido·nivel_N
```

- **polyBLEP** (Välimäki y Huovilainen, *Antialiasing Oscillators in Subtractive Synthesis*, IEEE SPM 2007): resta
  un polinomio de 2.º orden en las dos muestras alrededor de cada salto. Reduce mucho el aliasing de saw y pulse por
  ~10 operaciones.
- **Sync** sin corrección BLEP del reinicio: deja algo de aliasing en el salto de A (aceptable en la prueba; se mide).
- **Envelope del filtro como fuente**: el engine lo calcula a tasa de control (ya existe, por voz) y se pasa al core
  una vez por bloque.
- **Costo estimado:** ~60 ciclos por voz por muestra × 5 voces × 44 100 Hz ≈ 13 M ciclos/s ≈ **2–3 %** de CPU. Se
  quitan 10 `AudioSynthWaveform` y 5 mezcladores.

### 2.2 Lo que queda a tasa de control (update, ~1 kHz)

- POLY-MOD → FILTER (fuente envelope del filtro): multiplica el cutoff de la voz.
- **OSC B → FILTER a velocidad de audio no se hace**: `MoogLadder4P` calcula sus coeficientes por bloque y
  recalcularlos por muestra multiplica su costo (~8 % por voz). Queda anotado como limitación.
- WHEEL-MOD: fuente = mezcla de la salida del LFO global y un ruido de control; cantidad = rueda (CC 1).
- Keyboard tracking del filtro (OFF / HALF / FULL): cutoff × (f_nota / 261.6 Hz)^{0, 0.5, 1}.
- GLIDE: portamento exponencial de la frecuencia de la nota (0–2 s).
- El LFO global (página LFO) sigue entrando por `setLfoMod` como en los demás engines.

## 3. El panel en el Brain (4 encoders + SHIFT, theory/04 §3.1)

### 3.1 OSC (B1), NAV: OSC A · OSC B · MIXER

| Parada | P1 | P2 | ENC L | ENC R | SHIFT P1 | SHIFT P2 | SHIFT L | SHIFT R |
|---|---|---|---|---|---|---|---|---|
| **OSC A** | FREQ −24…+24 ST | PW 5–95 % | SAW off/on | PULSE off/on | SYNC off/on | — | — | — |
| **OSC B** | FREQ −24…+24 ST | FINE ±50 CT | PW 5–95 % | KEYBOARD off/on | SAW | TRI | PULSE | LO FREQ |
| **MIXER** | OSC A 0–100 | OSC B 0–100 | NOISE 0–100 | GLIDE 0–100 | FILTER KBD off/half/full | — | — | — |

### 3.2 LFO / MOD (B3), NAV: LFO · POLY-MOD · WHEEL-MOD

**Cambio para todos los engines:** NAV en la página LFO / MOD pasa a elegir la **parada** (LFO y, en los engines
que la tienen, su modulación propia). La forma del LFO se mueve a **SHIFT + P1** en la parada LFO. Los engines sin
paradas de MOD tienen solo LFO.

| Parada | P1 | P2 | ENC L | ENC R | SHIFT P1 | SHIFT P2 |
|---|---|---|---|---|---|---|
| **LFO** | RATE / división | → CUTOFF | → PITCH | → PWM | **SHAPE** | — |
| **POLY-MOD** | FILT ENV 0–100 | OSC B 0–100 | FREQ A off/on | PW A off/on | FILTER off/on | — |
| **WHEEL-MOD** | SOURCE LFO ↔ NOISE | FREQ A off/on | FREQ B off/on | PW A off/on | PW B off/on | FILTER off/on |

Los interruptores se manejan girando: a la derecha enciende, a la izquierda apaga.

En el Prophet, **la rueda deja de sumar vibrato genérico**: hace lo que diga WHEEL-MOD, como en el original.

## 4. Parámetros (`gf_params.h`)

| Grupo | Índice | Qué |
|---|---|---|
| 0 OSC | `parada · 4 + slot` (slots 0–3) | igual que hoy |
| **6 OSC_SHIFT** | `parada · 4 + (slot − 4)` | slots 4–7 (capa SHIFT) |
| **7 MOD** | `parada_mod · 8 + slot` | paradas de MOD (0 = POLY-MOD, 1 = WHEEL-MOD), 8 slots |

La plantilla `GF_OSC_SLOT` pasa a 8 slots por parada y se agrega `GF_MOD_SLOT[engine][2][8]`. Los interruptores son
discretos de 2 valores (0 / 1); FILTER KBD, de 3.

## 5. Compatibilidad

`setOscAWaveform` / `setOscBWaveform` / `setCrossmod` / `setOscMix` / `setOscBInterval` siguen existiendo (el sketch
28 los usa): una forma de onda enciende solo esa forma; `setCrossmod` es POLY-MOD OSC B → FREQ A.

## 6. Cómo se prueba

| Qué | Cómo | Dónde |
|---|---|---|
| Core: salida finita y en rango con todas las combinaciones | test Unity | native |
| Sync: la fase de A vuelve a 0 cuando B da la vuelta | test Unity | native |
| POLY-MOD FREQ A cambia la frecuencia de A (cruces por cero) | test Unity | native |
| polyBLEP: sin saltos mayores a 2 entre muestras en saw | test Unity | native |
| Mapeo de las paradas y de los interruptores | tests de `page_params` | native |
| Se oye | sync con FREQ de A barriendo; POLY-MOD FILT ENV → FREQ A (el "pew" de campana) | protoboard |
| CPU | el sketch49 la imprime cada 5 s | protoboard |

## 7. Qué no cubre

UNISON, VINTAGE, velocity y aftertouch, OSC B → FILTER a velocidad de audio, REV 1/2/3 del filtro y BLEP en el
reinicio del sync.
