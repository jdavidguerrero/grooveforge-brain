# LFO de los engines: una cantidad por destino

> **Fecha:** 2026-10-16 · **Estado:** PROPUESTO, implementación de prueba (sketch48 y sketch49) aprobada por Juan
> **Depende de:** UI v3 (página LFO / MOD, `variants/studio/design/claude-design-prompt-osc-lfo.md`),
> theory/04 de Studio (la navegación es del Teensy), ADR-005 (DX7 de 4 operadores).
> **No modifica:** `00`–`06`. El `param_id` sigue el esquema del sketch 28, no el `02` (que solo fija 2 B).

## 1. El problema

Los engines no tienen LFO: la única modulación es el envelope del filtro. La UI v3 diseñó una página
LFO / MOD con **un destino a la vez** (CUTOFF · PITCH · AMP · PWM) y una sola profundidad. Ese modelo
no deja mover dos cosas juntas (un vibrato leve y el filtro "respirando"), que es justo lo que hacen
casi todos los sonidos clásicos. Además mezclaba dos ideas: qué destinos existen y cuál edito ahora.

## 2. Qué hacían los originales

| Original | Fuente de modulación | Destinos del LFO | Fuente |
|---|---|---|---|
| Minimoog Model D | VCO 3 en rango LO o ruido (no hay LFO) | Pitch y filtro, cantidad por la rueda | Moog, *Minimoog Model D Owner's Manual* (2016), "Modulation" |
| Juno-106 | 1 LFO triangular con RATE y DELAY | Pitch (DCO), PWM, filtro: **un fader por sección** | Roland, *Juno-106 Owner's Manual* (1984), pp. 10–13 |
| Prophet-5 | LFO saw/tri/square + ruido (WHEEL-MOD) y POLY-MOD | Pitch A/B, PWM A/B, filtro | Sequential, *Prophet-5 rev 4 User's Guide* (2020), §4 |
| OB-6 | 1 LFO con 5 formas | Pitch, PWM, filtro, amplitud | Sequential, *OB-6 User's Guide* (2016), "LFO" |
| DX7 | 1 LFO con 6 formas y DELAY | Pitch (PMD) y amplitud por operador (AMD; en un modulador cambia el timbre) | Yamaha, *DX7 Operation Manual* (1983), "LFO" |
| ARP 2600 | VCO 2 en rango bajo, S&H | Cualquier entrada (semimodular) | ARP, *2600 Owner's Manual* (1971) |

Patrón común: **LFO → pitch, PWM, cutoff (y a veces amplitud); envelope → filtro y amplificador**. Ninguno
modula un envelope con el LFO: eso es de las matrices modernas (Serum, Vital) y queda fuera.

## 3. Decisión: un LFO, una cantidad por destino (modelo Juno-106)

```
          ┌─ cantidad CUTOFF ─> filtro          (barrido)
LFO ──────┼─ cantidad PITCH  ─> oscilador       (vibrato)
          └─ cantidad PWM/AMP ─> ancho de pulso o nivel
```

| | Cantidad por destino (elegida) | Destino único (UI v3 original) | Matriz de modulación |
|---|---|---|---|
| Mover varias cosas a la vez | sí | no | sí |
| Qué hay que entender | "cuánto LFO va acá" | un selector + una perilla | fuentes × destinos |
| Fidelidad a los clásicos | alta (cada engine habilita sus destinos) | media | baja |
| Celdas de UI | 3 cantidades + RATE = las 4 celdas de la página | 2 + 2 con SHIFT | no cabe en 2 encoders |
| CPU | 1 LFO + 3 multiplicaciones | igual | más |

**Un solo LFO global por engine, no uno por voz:** así eran el Juno, el Prophet (WHEEL-MOD), el OB-6 y
el DX7. Todas las voces vibran en fase, la sincronía al BPM es trivial y cuesta una sola evaluación.

### 3.1 Destinos habilitados por engine

Las cantidades son tres: CUTOFF, PITCH y un **tercer destino que depende del engine**.

| Engine | CUTOFF | PITCH | 3.º destino | Por qué |
|---|---|---|---|---|
| 0 MODEL D | ✓ | ✓ | — | El Moog no tiene PWM ni modulación de amplitud |
| 1 JUNO-106 | ✓ | ✓ | PWM | Fader LFO de la sección PWM |
| 2 PROPHET-5 | ✓ | ✓ | PWM | WHEEL-MOD → PW A/B |
| 3 OB-6 | ✓ | ✓ | PWM | El OB-6 también tiene AMP; con una sola celda se elige PWM, que es lo más usado |
| 4 DX7 | — | ✓ | AMP | Sin filtro en el original; AMD = amplitud o timbre |
| 5 ARP 2600 | ✓ | ✓ | PWM | VCO 2 tiene PWM |

Un destino que no existe aparece apagado en la pantalla y su encoder no hace nada. Las **formas** son las
mismas en todos los engines (SINE, TRI, SAW, SQR, S&H): restringirlas (el Juno solo tenía triángulo) no
aporta nada musical y complica la página. Es una concesión consciente.

## 4. El LFO (código custom, `apps/firmware-teensy/src/dsp/lfo.{h,cpp}`)

- **Acumulador de fase** en `[0, 1)`: `phase += rate_hz · dt`. Se evalúa a **tasa de control** (1 kHz en
  el sketch48), no por muestra: un LFO de 20 Hz tiene 50 puntos por ciclo, y la entrada al motor se
  suaviza con la rampa de `AudioSynthWaveformDc::amplitude(v, ms)` para que no haya escalones audibles
  (Pirkle, *Designing Software Synthesizer Plug-Ins in C++*, 2015, cap. 5, "LFO").
- **Salida bipolar** `[-1, 1]`: SINE, TRI, SAW (rampa ascendente), SQR y S&H (un valor nuevo por ciclo con
  un xorshift32, determinista para los tests).
- **RATE libre:** 0.05–20 Hz en escala logarítmica (los encoders avanzan un 6 % por paso).
- **SYNC:** `rate = BPM / 60 / beats`, con `beats` ∈ {1/4 = 1, 1/8 = 0.5, 1/16 = 0.25, 1/8T = 1/3}.
  El BPM sale del MIDI clock del USB host (24 ppq). Sin clock se usa 120. Con SYNC, P1 elige la división.
- **Corre libre** (como el Juno): no se reinicia con cada nota. El DELAY por nota (Juno, DX7) queda para la
  fase 2 porque no tiene celda libre en la página.

## 5. Cómo llega cada cantidad al sonido

| Destino | Cantidad `a` (0–1) → efecto | En el sketch48 (Teensy Audio Library oficial) |
|---|---|---|
| CUTOFF | ±4 octavas · `a` · lfo | entrada de control de `AudioFilterStateVariable` (`octaveControl(7)`), sumada al envelope del filtro |
| PITCH | ±12 · `a²` semitonos · lfo (cuadrática: los valores chicos dan vibratos finos) | entrada FM de `AudioSynthWaveformModulated` (`frequencyModulation(1)`) |
| PWM | ancho = base ± 0.45 · `a` · lfo | entrada de forma de `AudioSynthWaveformModulated` (PULSE y TRIANGLE_VARIABLE) |
| AMP | ganancia = 1 − `a` · (1 + lfo) / 2 | `AudioEffectMultiply` con un DC |

**Rueda de modulación (CC 1):** suma vibrato como en los originales: `a_pitch_efectiva = min(1, a + 0.3 · rueda)`.

El envelope es **uno solo para amplificador y filtro** (como el Juno-106). Su cantidad hacia el filtro es
FLT ENV AMT: 0 a +5 octavas en el sketch48. En los engines reales es la escala propia de cada uno
(`setFilterEnvAmount`: suma hasta 6000–8000 Hz al cutoff).

### 5.1 En los engines reales (sketch49)

Cada engine recibe un `LfoMod` (`src/engines/lfo_mod.h`) con la modulación ya convertida por el sketch, y la
aplica en su `update()` (~1 kHz) sobre las voces que suenan:

| Campo | Valor | Qué hace el engine |
|---|---|---|
| `cutoff_mul` | `2^(4 · a · lfo)` | multiplica el cutoff ya modulado por el envelope |
| `pitch_ratio` | `2^(st · lfo / 12)` | multiplica la frecuencia de todos los osciladores de la voz |
| `pwm` | `a · lfo` (−1..1) | `pulseWidth(0.5 + 0.45 · pwm)`; solo se oye en `WAVEFORM_PULSE`, como en los originales |
| `amp` | `1 − a · (1 + lfo) / 2` | ganancia de salida (solo el DX7 la usa) |

Solo el engine activo recibe modulación; al cambiar de engine los demás vuelven a `LfoMod{}` (neutro).
**Costo:** pocas operaciones por voz y por milisegundo; lo caro sigue siendo el filtro `MoogLadder4P` (~8 % de
CPU por voz, medido el 2026-10-15). Con 6 voces del Juno son ~48 % solo de filtros: hay que medir el peor engine
con el LFO encendido antes de cerrar el número de voces (`01` §5.2, ≤60 %).

## 6. Controles (UI v3 + esta enmienda)

Desde el 2026-10-16 (theory/04 de Studio §3.1) las páginas de edición usan los 4 encoders: lo que antes era
SHIFT + P1 / P2 pasó a ENC L / R, y la capa SHIFT queda libre. En las vistas, ENC L / R siguen siendo el filtro.

| Página | P1 | P2 | ENC L | ENC R | NAV | NAV push |
|---|---|---|---|---|---|---|
| PERFORM / SPECTRUM | FLT ENV AMT | LFO → CUTOFF | — (GLIDE, fase 2) | — (DRIVE, fase 2) | vista | — |
| AI | — (ARP RATE, fase 2) | — (HUMANIZE, fase 2) | — | — | vista | — |
| ENV | ATTACK | DECAY | SUSTAIN | RELEASE | **AMP ⇄ FILTER** (qué envelope se edita) | — |
| OSC | slot 1 (WAVE) | slot 2 | slot 3 | slot 4 (plantilla del engine, `GF_OSC_SLOT`) | oscilador | — |
| **LFO / MOD** | RATE (o división con SYNC) | → CUTOFF | → PITCH | → 3.er destino | **forma** | SYNC |

- **LFO → CUTOFF de PERFORM es el mismo valor** que la celda → CUTOFF de LFO (un valor por destino).
- **ENV con dos envelopes (2026-10-16, pedido de Juan):** amplificador y filtro por separado, como el Model D,
  el Prophet-5 y el OB-6; NAV elige cuál editan P1/P2. En el Juno-106 original era uno solo: acá se separan
  igual, por consistencia entre engines.
- **OSC** edita la plantilla de cada engine (la misma de `osc_catalog.cpp` de la cabeza, copiada en
  `GF_OSC_SLOT` de `gf_params.h`). El DX7 tiene página de operadores y queda sin edición en esta prueba.
- Quién hace qué: StudioUi (navegación) devuelve `Action::LfoShape` y `Action::LfoSync`; un módulo nuevo,
  `src/ui/page_params.{h,cpp}`, traduce P1/P2 de la página visible en cambios del patch. Los dos son lógica
  pura testeable en `native`.

## 7. Parámetros en el bridge

`PARAM_CHANGED` (0x14) con `param_id = ((0x10 + engine) << 8) | (grupo << 4) | índice` y valor normalizado
0–1, el esquema del sketch 28. Las conversiones viven en un header compartido,
`apps/bridge-protocol/include/gf_params.h`, para que el Teensy y la cabeza no puedan desalinearse.

| Grupo | Índice | Parámetro | Normalización |
|---|---|---|---|
| 0 OSC | `oscilador · 4 + slot` | slot de la plantilla (los del engine en el oscilador 0) | lineal en el rango del slot; formas: índice |
| 1 ENV | 0–3 · 4–7 | A, D, S, R del amplificador · del filtro | lineal en el rango del engine (tabla del sketch 28) |
| 2 FILTER | 0 · 1 | cutoff, resonancia | lineal en el rango del engine (igual que hoy) |
| 2 FILTER | **2** | FLT ENV AMT | 0–1 = 0 a +5 oct |
| **3 LFO** | 0 | RATE | `log(hz / 0.05) / log(400)` |
| 3 LFO | 1 | SHAPE | `forma / 4` |
| 3 LFO | 2 | DIV | `div / 3` |
| 3 LFO | 3 · 4 · 5 · 6 | cantidad CUTOFF · PITCH · AMP · PWM | 0–1 |

- **SYNC** viaja en `UI_STATE.flags` (bit 1), que ya existe: lo alterna la navegación.
- El BPM viaja en `BEAT_DETECTED` (0x82, `bpm × 10`).
- La **fase** no viaja: la cabeza la integra con el RATE que conoce (solo es para el punto animado).
- **Contradicción señalada, no resuelta acá:** en el sketch 28 el grupo 3 "LFO" guarda cosas distintas
  por engine (el chorus del Juno, el `osc_mix` del Prophet). Al integrar este LFO en el sketch 28 esos
  parámetros tienen que moverse a otro grupo. Lo decide Juan cuando se haga esa integración.

## 8. La pantalla LFO / MOD (cabeza)

```
┌ LFO · JUNO-106 ─────────────────────────────────────────────────────────────┐
│ [∿][△][⩘][⊓][S&H]   2.40 HZ  FREE · 0.42 S        CUTOFF   PITCH    PWM      │
│  ╭╮  ╭╮                                            ▕▓▓▕     ▕▒▕      ▕▓▓▓▕   │
│ ╯  ╰╯  ╰  ● (fase)                                 ▕▓▓▕     ▕▒▕      ▕▓▓▓▕   │
│ 0        0.42 S        0.83 S                       40 %    0.3 ST    60 %    │
├────────────┬────────────┬────────────┬────────────────────────────────────────┤
│ P1 RATE    │ P2 →CUTOFF │ SH →PITCH  │ SH →PWM   (o NOT USED)                  │
└────────────┴────────────┴────────────┴────────────────────────────────────────┘
```

Se reemplazan las fichas de destino y el medidor único por **tres medidores**, uno por cantidad, que se
mueven juntos con la fase. Un destino que el engine no tiene se dibuja en gris con "—".

## 9. Cómo se prueba

| Qué | Cómo | Dónde |
|---|---|---|
| Formas, rango, SYNC, S&H determinista | tests Unity del LFO | `pio test -e native` |
| P1/P2 por página, destinos apagados por engine, normalización | tests Unity de `page_params` | `pio test -e native` |
| Roundtrip de las conversiones de `gf_params.h` | tests Unity | `pio test -e native` |
| Se oye | en LFO: P2 mueve el filtro, SHIFT+P1 da vibrato, SHIFT+P2 mueve el PWM (Juno) o el nivel (DX7) | protoboard |
| Se ve | la pantalla de la Pi muestra los 3 medidores y la forma que elige NAV | Pi 3B |

## 10. Qué no cubre

DELAY del LFO, LFO por voz, LFO → envelope, matriz de modulación, GLIDE / DRIVE de PERFORM, edición de OSC
desde los encoders, FX y ML en el sketch49, y la integración en el sketch 28 (cabeza ESP32).
