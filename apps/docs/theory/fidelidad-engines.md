# Fidelidad de los engines: el programa para llegar al ~95 % de oído

> **Fecha:** 2026-10-16 · **Estado:** APROBADO por Juan ("quiero llegar a lo máximo teóricamente… arranca con la
> implementación") · **Después:** tests A/B contra grabaciones de los originales.
> **Depende de:** theory/lfo-modulacion.md, theory/prophet-5-completo.md, sprints/46-dx7-4-operadores.md, ADR-003
> (filtro digital `MoogLadder4P`), ADR-005 (DX7).

## 0. Punto de partida (estimación, 2026-10-16)

| Engine | Panel | Sonido | Lo que más lo aleja |
|---|---|---|---|
| Prophet-5 | ~85 % | ~65 % | filtro ladder en vez de CEM3320 / SSI2140, envelopes lineales |
| Model D | ~55 % | ~60 % | osciladores con aliasing, envelopes lineales, sin sección MODULATION |
| Juno-106 | ~50 % | ~50 % | una forma a la vez, sin HPF, filtro distinto al IR3109 |
| OB-6 | ~35 % | ~35 % | filtro de 4 polos en vez de SEM de 2 polos con morph, sin X-MOD |
| ARP 2600 | ~25 % | ~40 % | sin ruteos, 4 voces (el original es monofónico) |
| DX7 (4 op) | ~80 % | ~70 % | sin velocity, keyboard scaling ni detune por operador |

El 100 % no es verificable sin referencias y el último tramo pide modelado de componentes que no cabe en la Teensy con
6 voces. El objetivo es el **máximo teórico que entra en ≤ 60 % de CPU** (`01` §5.2), y después medir con A/B.

## 1. Paso 1 · Filtro ladder barato (requisito de todo lo demás)

**Problema:** `MoogLadder4P::processSample` llama `tanhf` **7 veces por muestra** (tanh(V0), tanh(V1) y tanh(V2) se
calculan dos veces) a ~200 ciclos cada una (medido 2026-10-15): ~8 % de CPU por voz, 48 % con 6 voces.

**Solución:**
1. Calcular cada tanh **una sola vez**: 5 por muestra (V0–V3 y la entrada con feedback).
2. Reemplazar `tanhf` por un **aproximante de Padé [7/6]**:
   `tanh(x) ≈ x·(135135 + x²(17325 + x²(378 + x²))) / (135135 + x²(62370 + x²(3150 + 28x²)))`, recortado a ±1 para
   |x| > 4.97. Error < 2·10⁻⁵ en |x| < 4 (Baker & Graves-Morris, *Padé Approximants*, 1996). ~12 multiplicaciones y una
   división (~25 ciclos en el Cortex-M7 con FPU).
3. Resultado esperado: **~1 % de CPU por voz** (8× menos). Se mide on-device.

Test: el aproximante contra `tanhf` (error), y el filtro completo contra la versión con `tanhf` (diferencia de salida
< −60 dB con una saw, misma auto-oscilación con k = 4).

## 2. Paso 2 · Envelopes con curva analógica (todos los engines)

**Problema:** el envelope del filtro de cada engine es una máquina de estados **lineal** copiada 6 veces, y el VCA usa
`AudioEffectEnvelope` (también lineal). Los ADSR analógicos cargan y descargan un capacitor por una resistencia: las
curvas son **exponenciales**. Es la diferencia más audible de "punch" (Pirkle, *Designing Software Synthesizer
Plug-Ins*, cap. 6; el ADSR del Prophet / OB usa CEM3310, el del Juno IR3R01, ambos RC).

**Solución:** `dsp::AnalogEnvelope`, un único ADSR exponencial (código custom):
- Cada etapa es un filtro de un polo hacia un **objetivo exagerado**: el ataque apunta a 1.3 y se corta en 1.0
  (curva cóncava como el CEM3310); decay y release apuntan a sustain / 0 con un piso de −80 dB.
- Coeficiente por etapa: `c = exp(−ln((1 + overshoot) / overshoot) / (t · fs))`, calculado cuando cambia el tiempo.
- Se usa en dos formas: **por muestra** dentro de un objeto de audio `AnalogVca` (reemplaza a `AudioEffectEnvelope`)
  y **a tasa de control** para los envelopes del filtro (reemplaza las 6 máquinas lineales).
- Parámetro de carácter por engine: el "overshoot" del ataque (Moog más blando, Prophet / OB más rápido).

Test: cada etapa llega a su destino en el tiempo pedido (±5 %), la curva es exponencial (a mitad del tiempo de
decay ya recorrió más de la mitad del camino), re-disparar no salta (continuidad) y el release funciona desde
cualquier etapa.

## 3. Paso 3 · Osciladores sin aliasing (Moog, Juno, OB-6, ARP)

**Problema:** `AudioSynthWaveform` genera saw y pulse ingenuos: en notas agudas los armónicos por encima de Nyquist se
reflejan como frecuencias inarmónicas.

**Solución:** un oscilador de voz genérico con **polyBLEP** (el mismo método del oscilador del Prophet), `BlepOsc`:
saw, pulse con PW, tri, square y sine, **varias formas sumadas** (para el Juno) y salida de sync. Cada engine arma
su voz con 1–3 `BlepOsc` dentro de un objeto de audio por voz (como `Prophet5Osc`), lo que además baja la cantidad
de objetos del grafo.

## 4. Paso 4 · Un filtro por familia de chip

| Engine | Original | Modelo |
|---|---|---|
| Model D, ARP 2600 | ladder de transistores (Moog), 4012 / 4072 (ARP) | `MoogLadder4P` (Paso 1) + saturación de entrada |
| Juno-106 | IR3109: 4 OTA en cascada + feedback, más HPF pasivo de 4 posiciones | **`OtaLadder`**: 4 polos de un OTA (`tanh` en la entrada de cada etapa, Huovilainen 2004) + feedback; HPF de un polo con 4 frecuencias |
| Prophet-5 | CEM3320 (rev 3) / SSI2140 (rev 4): 4 OTA en cascada | **`OtaLadder`** con otro carácter: saturación más suave, compensación de ganancia de la resonancia |
| OB-6 | SEM (Oberheim): state-variable de 2 polos, LP ↔ notch ↔ HP continuo, BP | **`SemSvf`**: SVF de Chamberlin con integradores trapezoidales (Zavalishin, *The Art of VA Filter Design*, 2018, cap. 4) y mezcla continua LP/HP |

Referencias: Huovilainen, *Non-linear digital implementation of the Moog ladder filter*, DAFx 2004; Zavalishin 2018;
datasheets CEM3320 y SSI2140.

## 5. Paso 5 · Completar el panel de cada engine

| Engine | Qué se agrega |
|---|---|
| Model D | sección MODULATION (VCO 3 / noise → pitch y filtro, cantidad por la rueda), rangos de octava 32'–2' por VCO, keyboard control del filtro (OFF / 1/3 / 2/3), DECAY = RELEASE, GLIDE, overdrive del mixer |
| Juno-106 | saw + pulse + sub juntas, PWM MAN / LFO / ENV, rango 16' / 8' / 4', HPF 0–3, VCA ENV / GATE, DELAY del LFO, chorus I / II / I+II |
| OB-6 | forma continua saw ↔ pulse por VCO, X-MOD (envelope del filtro y VCO 2 → frecuencia, forma y filtro de VCO 1), sync, filtro con morph y BP |
| ARP 2600 | **monofónico**, VCO 3, S&H, sync y una página **PATCH** con ~8 ruteos preconfigurados (S&H → filtro, ring mod, VCO 3 → pitch, etc.) en lugar de cables (decisión tomada siguiendo la recomendación, 2026-10-16) |
| Prophet-5 | UNISON, VINTAGE (deriva y desafinación por voz), OSC B → FILTER a velocidad de audio (cabe con el Paso 1) |
| DX7 | detune por operador, velocity y keyboard scaling por operador, AMS por operador |

## 6. Paso 6 · Expresión y "analogness" (todos)

Velocity y aftertouch hacia filtro y VCA, modos POLY / MONO / LEGATO / UNISON, deriva lenta de afinación por voz y
diferencias entre voces, saturación suave en el mixer. Esto también se diseña aparte cuando llegue.

## 7. Orden y puertas

Cada paso termina con: tests native verdes, build de sketch49 y del sketch 28, despliegue y **CPU medida on-device**
con el engine más pesado. El panel nuevo de cada engine se documenta en su propia tabla antes del código (como el
Prophet). Si un paso pasa del presupuesto de CPU, se para y se decide (menos voces, oversampling o simplificar).
