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

## 8. Validación offline (2026-10-18)

`apps/firmware-teensy/tools/offline/` compila los núcleos DSP reales del firmware en el Mac, los mide y renderiza
WAVs (`./build.sh && ./validate_engines out`; el reporte completo queda en `out/reporte.md`). No mide el grafo de la
Teensy ni su CPU real.

| Paso | Medición | Resultado | Estado |
|---|---|---|---|
| 3 · polyBLEP | Aliasing bajo 5 kHz, saw en C6 / C7 / G7 | −70 / −65 / −66 dB (ingenuo: −23 / −19 / −18 dB) | ✅ |
| 2 · AnalogEnv | Ataque de 100 ms a los 50 ms | 0.83 (RC hacia 1.5× ≈ 0.63; lineal 0.50) | ⚠️ demasiado curvo |
| 2 · AnalogEnv | Decay | 0.53 vs. RC puro 0.55 | ✅ |
| 2 · AnalogEnv | Re-disparo con la nota sonando | salta de 0.50 a 0.00 (click) | ❌ debe arrancar desde el nivel actual |
| 2 · AnalogEnv | Release desde cualquier etapa | continuo | ✅ |
| 2 · VCA | Envolvente del amplificador | sigue siendo `AudioEffectEnvelope` lineal en los 6 engines | ❌ falta `AnalogVca` por muestra |
| 1 · Ladder | Pendiente y −12 dB en el cutoff con k = 0 | −22 dB/oct (2→8 kHz), −12.1 dB en fc | ✅ |
| 1 · Ladder | Auto-oscilación vs. cutoff pedido | −70 ¢ a 110 Hz, −278 ¢ a 440 Hz, −859 ¢ a 1.76 kHz, −1913 ¢ a 5 kHz | ❌ la resonancia se corre hacia abajo |
| 4 · SEM SVF | LP / HP / BP / notch | −12.9 / +0 / −6.4 dB/oct, notch −80 dB | ✅ |
| 4 · Juno, Prophet | Filtro propio | siguen con el ladder Moog | ❌ paso 4 pendiente |
| DX7 | Bandas laterales vs. J_n(I) en dos notas | iguales a Bessel y sin cambiar con la nota | ✅ modulación de fase correcta |
| Chorus Juno | Velocidades I / II | 0.513 / 0.863 Hz | ✅ |
| Chorus Juno | Estéreo | mono (el original: dos BBD con LFO invertido, L / R) | ❌ |

Costo relativo en el Mac (ladder = 1): polyBLEP 0.18, SEM 0.62, oscilador del Prophet 0.89, voz DX7 1.81, chorus 1.13.

**Lo que falta para el objetivo, por impacto:**
1. Ladder: la frecuencia de resonancia cae con el cutoff (el término de la muestra anterior en el lazo de
   realimentación agrega retardo de fase). Corrección de afinación por tabla (medir f_osc / f_pedida e invertir) o
   integración ZDF. Afecta Model D, ARP, Juno y Prophet.
2. VCA exponencial por muestra (`AnalogVca`) y re-disparo desde el nivel actual en `AnalogEnv`.
3. Curva de ataque: carga RC hacia ~1.5× (constante ≈ 1.1) en vez de 3.2.
4. Chorus del Juno estéreo: requiere salida estéreo del engine y del grafo de audio (hoy mono).
5. Filtros OTA para Juno (IR3109 + HPF) y Prophet (CEM3320 / SSI2140).

## 9. Implementación de las brechas 1–6 (aprobado por Juan, 2026-10-18)

### 9.1 Ladder ZDF con carácter por chip (brechas 1 y 6)

La medición de §8 muestra que el modelo D'Angelo, tal como está discretizado, **no puede** afinarse con una tabla:
la resonancia nunca pasa de ~1.65 kHz (5 kHz pedidos suenan en 1.66 kHz; 14 kHz casi no oscilan) y sobremuestrear 2×
solo la lleva a ~3.3 kHz. La causa es el término de la muestra anterior en la realimentación (retardo de fase que
crece con la frecuencia). Se reemplaza el núcleo por un **ladder TPT / ZDF** (Zavalishin, *The Art of VA Filter
Design*, rev. 2.1, cap. 5 y §6.6 "cheap" nonlinear):

- 4 polos TPT idénticos (`G = g / (1 + g)`, `g = tan(π·fc/fs)`): afinación exacta en el régimen lineal, también con
  resonancia.
- La realimentación se resuelve en la misma muestra: `y4 = (G⁴·x + Σ) / (1 + k·G⁴)`, con Σ la contribución de los
  estados. Sin retardo en el lazo.
- No linealidad en el nodo de suma (`u = tanh(d·(x − k·y4)) / d`): limita la auto-oscilación a una amplitud estable y
  comprime la resonancia con señal fuerte, como el par diferencial de entrada.
- **Carácter por chip** (misma topología: 4 polos en cascada con realimentación negativa global; cambian la
  saturación y la compensación de graves):

| Carácter | Engines | Drive del nodo | Compensación de graves con resonancia | Referencia |
|---|---|---|---|---|
| MOOG | Model D | 1.6 (satura antes: el "overdrive" del Moog) | 0 (pierde graves, 1 / (1 + k)) | Moog ladder, Stinchcombe 2008 |
| ARP 4072 | ARP 2600 | 1.3 | 0 | ARP 2600 service manual |
| IR3109 | Juno-106 | 1.1 | 0.35 | Roland IR3109 (OTA ×4) |
| CEM3320 | Prophet-5 | 1.0 | 0.5 | datasheet CEM3320 / SSI2140 |

  Los valores de drive y compensación son el punto de partida para los A/B, no mediciones de los chips.
- Costo: 4 etapas TPT + 1 tanh de Padé + una división por muestra, menos que el D'Angelo (5 tanh). El SEM SVF del OB-6
  ya era TPT (afinado).

### 9.2 Envolvente RC por muestra y VCA analógico (brechas 2, 3 y 4)

`dsp::AnalogEnv` pasa a coeficientes de un polo (carga y descarga RC), evaluable **por muestra** o a tasa de control:

- **Ataque:** carga hacia 1.5 × el pico y se corta en 1.0 (CEM3310 / contornos Moog); constante por engine.
- **Decay / release:** descarga exponencial con τ = t / 4.6 (llega al 1 % en el tiempo pedido), sin el término
  (1 − t) que forzaba una cola recta.
- **Re-disparo desde el nivel actual** (sin volver a 0): sin click.
- **`AnalogVca`** (subclase de `AudioStream`, código custom): la misma envolvente aplicada por muestra a la señal,
  con la API de `AudioEffectEnvelope` (attack / decay / sustain / release / noteOn / noteOff / isActive). Reemplaza a
  `AudioEffectEnvelope` en los 6 engines.

### 9.3 Ruta estéreo y chorus del Juno (brecha 5)

- `JunoChorusCore::render_stereo`: dos lecturas de la misma línea BBD con el LFO triangular **invertido** (L = +tri,
  R = −tri) y la señal seca en los dos canales, como los dos MN3009 del Juno-106.
- `audio_graph`: se agrega el lado derecho (`g_engine_mix_ra/rb → g_engine_bus_r → g_master_mix_r → I2S R`). Los
  engines mono se conectan a los dos lados; el Juno manda L y R distintos. Las fuentes mono (FX del modo FX, LINE IN,
  USB) entran a los dos masters con las mismas ganancias.
- Sketch49: la cadena de SYNTH FX se duplica para el lado R (~6 % más de CPU estimado, a medir).
- Sketch 28: conecta sus engines a los dos lados (sigue sonando igual en mono).

### 9.4 Resultado (validación offline, 2026-10-18)

| Brecha de §8 | Antes | Después |
|---|---|---|
| 1 · Resonancia del ladder | −859 ¢ a 1.76 kHz, −1913 ¢ a 5 kHz, sin oscilar sobre ~1.65 kHz | **±5 ¢ de 110 Hz a 12 kHz**, amplitud de auto-oscilación ~0.5 |
| 2 · VCA | `AudioEffectEnvelope` lineal | **`AnalogVca` RC por muestra** en los 6 engines (salto máximo 0.0075 por muestra) |
| 3 · Re-disparo | saltaba de 0.50 a 0 | **sigue desde 0.50** |
| 4 · Curva de ataque (a la mitad) | 0.83 | **0.63** (RC hacia 1.5×) |
| 5 · Chorus del Juno | mono | **estéreo**, correlación L/R 0.78 con un tono; grafo con lado derecho |
| 6 · Filtros Juno / Prophet | ladder Moog | **ZDF con carácter IR3109 / CEM3320** (compensación de graves y límite de lazo propios) |

Pendiente on-device: CPU real con 6 voces (ladder ZDF, VCA por muestra y la segunda cadena de SYNTH FX) y A/B contra
grabaciones de los originales para ajustar drive, compensación y límite de lazo de cada carácter.

## 10. Segunda vuelta de sonido: comportamiento de voz analógica (2026-10-18)

Con osciladores, filtros y envelopes ya en su sitio (§9.4), lo que separa al engine del original es cómo se
comportan **las voces entre sí** y **los detalles de cada chip**. Lote 1 (este):

| Comportamiento | En el original | Implementación |
|---|---|---|
| **Voice cards** | Cada voz es un circuito distinto: afinación y cutoff con pequeñas diferencias fijas (el VINTAGE del Prophet rev 4 imita esto) | `dsp::VoiceSpread` por voz, determinista: desafinación y cutoff fijos por voz |
| **Asignación rotativa** | Prophet, OB-6 y Juno rotan las voces: dos notas iguales seguidas suenan en voces distintas | `_findFreeVoice()` busca desde la voz siguiente a la última usada |
| **Saturación por etapa** | En el ladder Moog cada par de transistores satura, no solo la entrada | `stage_sat` en `ZdfLadder`: término cúbico suave a la salida de cada etapa (no toca el estado: la afinación con señal chica no cambia) |
| **Curva de envelope por chip** | Juno-106: envelope generado por el MCU, ataque casi lineal; Minimoog: contorno RC muy curvo; CEM3310 (Prophet, OB): intermedio | Sobrepaso del ataque por engine en `AnalogEnv` y `AnalogVca` |

Valores de partida (estimados, se ajustan en el A/B):

| Engine | Desafinación por voz | Cutoff por voz | Sobrepaso del ataque | Saturación por etapa |
|---|---|---|---|---|
| Prophet-5 | ±4 ¢ | ±6 % | 0.5 (CEM3310) | 0.02 (CEM3320) |
| OB-6 | ±2 ¢ | ±3 % | 0.5 | — (SEM) |
| Juno-106 | ±0.3 ¢ (DCO) | ±4 % | 3.0 (casi lineal) | 0.02 (IR3109) |
| Model D | VCO 2/3 ±2 ¢ fijos | — (mono) | 0.3 | 0.05 |
| ARP 2600 | ±2 ¢ | ±3 % | 0.6 | 0.05 |

**Lotes siguientes** (cada uno con su medición offline): DX7 con envolventes Yamaha de 4 rates / levels, 8 formas
del TX81Z y detune por operador; Model D con el VCO 3 como modulador y el overdrive del mixer; OB-6 con forma de onda
continua y sync; ARP monofónico con S&H; etapa de salida (acople en AC y saturación) y color del ruido por engine.

## 11. Tercera vuelta: secciones que faltan por engine (aprobado por Juan, 2026-10-19)

"Termina de implementar todo para probar en el hardware". Cierra los lotes anunciados en §10 y las secciones de panel
que faltaban (theory/07 de Studio, ítem 6). Regla de interfaz: lo que no cabe en la página OSC va a **paradas de MOD**
de la página LFO / MOD (`GF_MOD_SLOT`, hasta 2 por engine), igual que POLY-MOD / WHEEL-MOD del Prophet. Así reciben
CC (54–61 y 110–117, theory/07 §3), se guardan en el preset y la cabeza las dibuja sin pantallas nuevas.

### 11.1 DX7: envolventes Yamaha, formas del TX81Z, detune y escalado de teclado

| Comportamiento | En el original | Implementación |
|---|---|---|
| **Envolvente en dB** | El EG del DX7 / TX81Z avanza lineal en **nivel logarítmico** (atenuación): el decay y el release suenan exponenciales; el ataque se acerca al máximo cada vez más lento en dB (sube rápido al principio en amplitud) | `Dx7VoiceCore`: estado en dB de atenuación (0 = máximo, 96 = silencio). Ataque: `att ← att·k` (aproximación exponencial en dB); decay, decay 2 y release: pendiente lineal en dB; amplitud = 2^(−att/6.02) con exp2 rápido |
| **Tiempos** | Rates 0–99 | Se mantienen ms del panel: DECAY y RELEASE = tiempo para caer **60 dB**; ATTACK = tiempo de 96 dB a 0.1 dB |
| **Decay 2** (D2R del TX81Z) | Con la tecla apretada el nivel sigue bajando desde D1L | D2 RATE global (0 = sostiene, si no ms para 60 dB). Simplificación: global, no por operador |
| **8 formas de onda** (W1–W8 del TX81Z) | W1 seno · W2 seno "cuadrado" (sin·\|sin\|) · W3 medio seno · W4 medio W2 · W5 seno ×2 en la primera mitad · W6 W2 ×2 en la primera mitad · W7 \|seno ×2\| en la primera mitad · W8 \|W2 ×2\| en la primera mitad | Una forma por operador, calculada sobre la tabla de seno (sin tablas nuevas) |
| **Detune** | −3…+3 por operador | 2 ¢ por paso (estimado, se ajusta en el A/B) |
| **Escalado de nivel por teclado** | Atenúa operadores en las notas agudas | KBD LEVEL 0–99: hasta −12 dB por octava sobre C3, solo en **moduladores** (el uso clásico: el brillo no se dispara en el agudo). Simplificación: global |
| **Escalado de rate** (RS 0–3) | Envolventes más rápidas en el agudo | Tiempos × 2^(−(nota − 48)/12 · RS/3): con RS 3 se duplica la velocidad por octava |

Panel: parada MOD 1 **WAVE / DETUNE** = WAVE OP1–OP4 | DETUNE OP1–OP4; parada MOD 2 **KBD / EG** = KBD LEVEL ·
KBD RATE · D2 RATE.

### 11.2 Model D: MODULATION, overdrive del mixer, ruido rosa, keyboard control

| Comportamiento | En el original | Implementación |
|---|---|---|
| **MODULATION** | Fuente = mezcla VCO 3 ↔ NOISE (MOD MIX); destinos OSC MOD (afinación de VCO 1–2) y FILTER MOD; cantidad = rueda de modulación | VCO 3 se lee a tasa de control (su fase se integra en el engine); con OSC MOD la rueda deja de hacer el vibrato genérico y hace lo del original (como WHEEL-MOD en el Prophet) |
| **VCO 3 KBD / LO** | OSC 3 CONTROL off: VCO 3 no sigue el teclado (dron / LFO); LO: rango sub-audio | Toggles: con KBD off, VCO 3 queda en su frecuencia de C3 + intervalo; con LO divide por 100 (0.1–20 Hz típico) |
| **Overdrive del mixer** | Subir los niveles del mixer satura la entrada del filtro (el truco del "feedback loop") | OVERDRIVE 0–100: ganancia de entrada 1–4× antes del `fast_tanh` de entrada del ladder (ya existe `set_drive`): sumado al DRIVE del panel |
| **Ruido WHITE / PINK** | Switch en el panel | `AudioSynthNoisePink` (Teensy Audio Library) + selector |
| **FILTER KBD** | Dos switches: 1/3 y 2/3 → 0, 1/3, 2/3, 1 | 4 posiciones: cutoff × (f_nota / f_C3)^k |

Panel: parada MOD **MODULATION** = MOD MIX · OSC MOD · FILTER MOD | VCO3 KBD · VCO3 LO · OVERDRIVE · FILTER KBD.
El ruido es una **parada propia de la página OSC** (NOISE: P1 LEVEL · P2 WHITE / PINK), como en el MIXER del
original: NAV la selecciona igual que a los VCO (corrección tras la prueba en el hardware, 2026-10-19).

### 11.3 OB-6: forma continua y sync

| Comportamiento | En el original | Implementación |
|---|---|---|
| **Forma continua** | Perilla de forma de saw a pulse (con el ancho de pulso aparte) | `AudioSynthPolyBlep::setShapeMix(m)`: salida = (1 − m)·saw + m·pulse, polyBLEP en los dos bordes. WAVE de la página OSC pasa a SHAPE 0–100 |
| **Sync** | VCO 2 se sincroniza a VCO 1 | Sync entre objetos de audio: el maestro (VCO 1, creado antes, así su `update()` corre primero en el mismo ciclo) anota en qué muestra y fracción da la vuelta; el esclavo reinicia la fase ahí y corrige el salto con un polyBLEP de un lado (aproximado: un poco más de aliasing que el BLEP de dos lados) |

Panel: SHAPE reemplaza a WAVE en VCO 1 y 2; SYNC en la parada X-MOD (sexto slot, libre).

### 11.4 ARP 2600: mono, sync, VCO 3 y página PATCH

| Comportamiento | En el original | Implementación |
|---|---|---|
| **Monofónico** | El 2600 es mono, con prioridad a la última nota | MONO: una voz, prioridad última nota, vuelve a la nota anterior al soltar (pila de 8 notas); legato sin redisparo si hay glide |
| **Sync** | VCO 2 sincronizado a VCO 1 | El mismo sync de §11.3 |
| **VCO 3 como modulador** | En el 2600 el VCO 3 en rango bajo modula afinación y filtro desde la matriz | VCO 3 a tasa de control (tri, 0.1–30 Hz): VCO3 → PITCH (±12 st) y VCO3 → CUTOFF (±4 oct) |
| **S&H** | Muestrea el ruido con su reloj | Ya existe: forma S&H del LFO global (theory/lfo-modulacion.md) con destino CUTOFF / PITCH; los presets S&H BURBLE lo usan |
| **Ruido** | Generador de ruido con color continuo | Mezcla blanco / rosa (NOISE COLOR 0–100) |

Panel: parada MOD **PATCH** = MONO · SYNC · VCO3 RATE · VCO3 → PITCH | VCO3 → CUTOFF · NOISE COLOR.

### 11.5 Juno-106: rango, VCA ENV / GATE, retardo del LFO

| Comportamiento | En el original | Implementación |
|---|---|---|
| **RANGE 16' · 8' · 4'** | Selector de octava del DCO | Slot SHIFT 6 de OSC: −12 / 0 / +12 st |
| **VCA ENV / GATE** | En GATE el VCA abre y cierra con la tecla, sin envolvente | Slot SHIFT 7: GATE = envolvente del VCA 1 ms · sustain 1 · release 8 ms (sin click) |
| **LFO DELAY** | La profundidad del LFO sube en rampa tras una nota nueva (si no había otras apretadas) | Parada MOD **LFO**: DELAY 0–3000 ms; el sketch escala las cantidades del LFO con la rampa |

### 11.6 Etapa de salida y velocity

- **Acople AC + saturación de salida**: cada engine termina en un VCA / mezclador y un amplificador de salida que no
  es ideal. En el bus del engine: filtro pasa altos de 1 polo a 8 Hz (quita el DC del medio seno del DX7, del pulse
  asimétrico y del ruido de los LFO) y `fast_tanh` suave (drive 1.2, compensado) — `OutputStage`, un objeto por
  canal antes de las cadenas de FX.
- **Velocity por engine** (theory/07 §2): Juno sin velocity (el 106 no tiene); Model D sin velocity (el original no
  tiene); Prophet, OB-6, ARP → VCA y cantidad del envelope del filtro (ya estaba en los engines); DX7 ya la tiene
  (índice de modulación). Se aplica en el sketch49: al Model D y al Juno les llega velocity 1. Aftertouch: global
  en el sketch (theory/07).

### 11.7 Vista de panel (cabeza)

Cuarta vista de la home (después de PERFORM · AI · SPECTRUM): **PANEL**, el panel completo del engine en una
pantalla, solo lectura, con todas las secciones (OSC por parada, MOD, ENV AMP y FILTER, FILTER, LFO, SYNTH FX) en
tarjetas, como la serigrafía del original. Navegable (corrección tras la prueba en el hardware, 2026-10-19): NAV
recorre las tarjetas (pasada la última o antes de la primera, sigue a la vista vecina) y NAV push abre la página de
la tarjeta con su parada elegida (OSC, LFO / MOD, ENV AMP / FILTER, SYNTH FX; FILTER va a PERFORM). B1–B4 vuelven a
PANEL en la misma tarjeta.

### 11.8 Cómo se valida

| Qué | Cómo |
|---|---|
| EG Yamaha: caída lineal en dB, 60 dB en el tiempo pedido | test native de `Dx7VoiceCore` |
| Formas W1–W8: medio ciclo en cero para W3–W8, simetría de W1 / W2 | test native |
| Sync: el esclavo reinicia en cada vuelta del maestro | test native del core polyBLEP |
| Forma continua: m = 0 igual a saw, m = 1 igual a pulse | test native |
| Mono del ARP: prioridad última nota y vuelta a la anterior | test native (lógica de pila aparte del engine) |
| Todo lo demás | A/B en el hardware, `tools/offline` para lo medible |
