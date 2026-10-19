# Sprint 46 · DX7 de 4 operadores (ADR-005)

> **Fecha:** 2026-10-16 · **Estado:** APROBADO por Juan ("implementa 2") · **Engine:** `src/engines/dx7.*`
> **Decisión de arquitectura:** ADR-005 (4 operadores, objeto de audio propio, 8 algoritmos, feedback en OP4).
> **Panel:** theory/04 de Studio §3.1 (4 encoders + SHIFT), theory/lfo-modulacion.md (LFO: PITCH y AMP en el DX7).

## 1. Qué cambia

El engine actual son 2 operadores con `frequencyModulation()` de `AudioSynthWaveformModulated`: modulación de
**frecuencia**, sin algoritmos ni feedback (auditoría 2026-05). Pasa a **4 operadores con modulación de fase**, como
los Yamaha DX100 / TX81Z / reface DX.

## 2. Teoría

### 2.1 Modulación de fase vs. de frecuencia

En FM de Chowning la portadora es `sin(ωc·t + I·sin(ωm·t))`: lo que se suma es **fase**. Si en cambio se modula la
frecuencia instantánea (lo que hace `frequencyModulation()`), la fase es la integral del modulador: el índice
efectivo pasa a ser `I·ωc/ωm`, cambia con la nota y el timbre no es estable a lo largo del teclado. Los DX hacen
modulación de fase: `out = sin(2π·(fase + m))`, con `m` la salida de los moduladores en ciclos.
(Chowning, *JAES* 21(7), 1973; Dexed / msfa, `fm_op_kernel`.)

### 2.2 Los 8 algoritmos (familia TX81Z)

La misma tabla que dibuja la cabeza (`screen_osc.cpp`, `ALG`). Un modulador siempre tiene número mayor que su
destino, así que basta calcular OP4 → OP3 → OP2 → OP1 en ese orden:

| Alg | Ruteo | Portadoras |
|---|---|---|
| 1 | 4 → 3 → 2 → 1 | 1 |
| 2 | 3 → 2, 4 → 2, 2 → 1 | 1 |
| 3 | 3 → 2 → 1, 4 → 1 | 1 |
| 4 | 4 → 3 → 1, 2 → 1 | 1 |
| 5 | 2 → 1, 4 → 3 | 1, 3 |
| 6 | 4 → 1, 4 → 2, 4 → 3 | 1, 2, 3 |
| 7 | 4 → 3 | 1, 2, 3 |
| 8 | — | 1, 2, 3, 4 |

La salida es la suma de las portadoras dividida por su cantidad, para que cambiar de algoritmo no salte de volumen.

### 2.3 Feedback en OP4

OP4 se modula a sí mismo con el **promedio de sus dos últimas salidas** (`fb · (y[n−1] + y[n−2]) / 2`): con una sola
muestra, el feedback alto oscila entre dos valores y se vuelve ruido inestable; el promedio es el truco del DX7.
FEEDBACK 0–7 → `fb = 2^(FEEDBACK − 7) · 0.5` ciclos (0 = apagado).

### 2.4 Nivel y envolvente por operador

- **LEVEL 0–99** → amplitud `2^((LEVEL − 99) / 8)` (~0.75 dB por paso, como el DX); 0 = mudo.
- Como modulador, la salida se escala a **2 ciclos** a escala completa (índice ≈ 12.6 rad, el máximo útil del DX).
- **Envolvente ADSR por operador**, lineal, calculada por muestra con incrementos fijos en el bloque. En una
  portadora es volumen; en un modulador es **brillo** (el "ataque metálico" del E.PIANO es un modulador con decay
  corto).

### 2.5 Costo y sin filtro por voz

Por muestra y por operador: avance de fase, seno por tabla (1024 puntos, interpolación lineal), envolvente y
multiplicación: ~30 ciclos. 6 voces × 4 operadores × 44 100 Hz × 30 ≈ 32 M ciclos/s ≈ **5 %** de CPU.

El engine actual tiene un `MoogLadder4P` por voz (~8 % cada uno, ~48 % en total). ADR-005 dice filtro OFF por
defecto: se reemplaza por **un solo filtro de tono a la salida** (el cutoff y la resonancia de home siguen
funcionando) y el FLT ENV AMT no aplica al DX7 (el brillo lo dan los envelopes de los moduladores, como en el
original).

## 3. Implementación (código custom)

- `dx7_core.{h,cpp}`: `Dx7VoiceCore`, lógica pura (native): 4 operadores, algoritmos, feedback, envelopes.
- `Dx7Voice` (subclase de `AudioStream`): una por voz, llama al core en `update()`.
- `DX7Engine`: 6 `Dx7Voice` → `AudioEffectEnvelope` (ENV · AMP) → mezcladores → `MoogLadder4P` de tono.
  Mantiene `noteOn` / `noteOff` / `update` / `setLfoMod`; los setters viejos (`setFmRatio`, `setFmDepth`, formas de
  onda) quedan como compatibilidad del sketch 28.

## 4. Panel (página OSC, NAV: OP 1 · OP 2 · OP 3 · OP 4)

| | P1 | P2 | ENC L | ENC R |
|---|---|---|---|---|
| Sin SHIFT | RATIO 0.5–16 (pasos de 0.5) | LEVEL 0–99 | ALGORITHM 1–8 (global) | FEEDBACK 0–7 (global) |
| SHIFT | ATTACK del operador | DECAY | SUSTAIN | RELEASE |

Parámetros: la plantilla `GF_OSC_SLOT` del DX7 (8 slots por operador); viajan como el resto de OSC (grupos 0 y 6).

## 5. Pruebas

| Qué | Cómo | Dónde |
|---|---|---|
| Salida finita en los 8 algoritmos, con feedback 7 y niveles 99 | test Unity | native |
| Algoritmo 8 con OP1 solo = seno a la frecuencia de la nota (cruces por cero) | test Unity | native |
| Un modulador con LEVEL 0 no cambia la portadora | test Unity | native |
| La envolvente de una portadora llega a 0 después del release | test Unity | native |
| Se oye | E.PIANO de arranque (alg 5), bajar el DECAY de OP2 / OP4 y oír cómo cambia el ataque | protoboard |
| CPU | el sketch49 la imprime con 6 voces | protoboard |

## 6. Qué no cubre

6 operadores y SysEx del DX7, velocity y keyboard scaling por operador, detune fino por operador, LFO AMS por
operador (el LFO → AMP es global) y el filtro por voz ("warmed FM").
