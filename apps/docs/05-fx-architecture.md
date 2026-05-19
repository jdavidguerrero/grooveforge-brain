# 🎛️ GrooveForge Brain — FX Architecture v0.1 (12 Signature Effects)

> **Canonical spec de la arquitectura de efectos del Brain.**
> **Parent:** GFD v3.0 — Master Strategy & Spec
> **Status:** v0.1 — Mayo 15, 2026
> **Stack:** Teensy 4.1 + Teensy Audio Library + custom DSP

---

## 0. Filosofía

Los efectos del GrooveForge Brain son **diferenciadores reales del producto**. No son los clásicos genéricos ("un reverb más, un delay más"). Cada efecto está diseñado para tener una identidad única que un DJ o productor reconozca y use en performance real.

Esta colección es **probada en vivo por Juan (GPROG) en shows reales** antes del shipping de v1.0. Si un efecto no aporta a un live act, se reemplaza.

## 0.1 Posicionamiento

Un boutique synth a $599 con 12 efectos signature ejecutándose en hardware dedicado (Teensy 4.1 bare-metal, latencia <1ms) es un argumento de venta **único en el mercado**:

- Behringer Pro-1 ($549): synth analog sin FX integrados
- Korg Minilogue XD ($649): solo delay + reverb básicos
- Moog Mavis ($349): sin FX digitales
- Organelle M ($699): FX via Pure Data (latencia variable)
- **GrooveForge Brain ($599): 12 signature FX en hardware dedicado**

## 0.2 Arquitectura FX (3 Layers)

La cadena de FX es **agnóstica a la fuente** — procesa de forma idéntica el audio
generado por un engine de síntesis (Modo Synth) o el audio externo del jack `FX IN`
(Modo FX Processor — ver `01-architecture.md §2.1`). El mismo código DSP corre en
ambos modos; solo cambia de dónde llega la señal al grafo de audio del Teensy.

```
Fuente:  Modo Synth        → Engine output (Moog/Juno/Prophet/etc)
         Modo FX Processor → Audio externo (jack FX IN → SGTL5000 ADC)
        ↓
[INSERT LAYER] — efectos in-line, mono-stereo
        ↓
[SEND LAYER] — aux sends paralelos (parallel mix)
        ↓
[MASTER LAYER] — bus master FX antes de salida
        ↓
SGTL5000 DAC → 1/4" TRS L/R out
```

---

## 1. Los 12 Signature Effects

### 1.1 🌊 CYMATIC RESONATOR (Insert Layer)

**Concepto:** Resonancia armónica basada en patrones cimáticos físicos. Cuatro filtros resonantes paralelos tuneados a las frecuencias de modos vibracionales naturales (modos 1:2:3:5, ratios pseudo-armónicos).

**Sound:** Brillo metalico/cristalino. Como meter el audio a través de un cristal vibrando.

**Use case live:** Build-ups, transiciones drone-to-drop, ambient pads.

**Stack técnico:**
- 4× AudioFilterBiquad (BP resonante, Q=20-60)
- AudioMixer4 (paralelo)
- LFO modulando ratios entre modos
- CPU: ~8%

**Parámetros user:**
- Density (cantidad modos activos 1-4)
- Tune (ratio entre modos)
- Resonance (Q de cada filtro)
- Mix (dry/wet)

---

### 1.2 ☁️ GRANULAR CLOUD (Insert Layer)

**Concepto:** Granulizador con "inteligencia" — analiza el incoming signal y ajusta densidad/pitch/duración de granos según contenido espectral.

**Sound:** De textura sutil shimmer a clouds completamente abstractas.

**Use case live:** Ambient transitions, breakdown atmospheres, vocal chops.

**Stack técnico:**
- AudioEffectGranular (Teensy Audio Library built-in)
- Sample buffer 32KB
- Control inteligente: amplitude envelope → density mapping
- CPU: ~12%

**Parámetros user:**
- Size (grain duration 5-500ms)
- Density (1-50 granos/sec)
- Pitch spread (random pitch variation)
- Position (deterministic vs random)
- Mix

---

### 1.3 👻 GHOST ECHO (Send Layer)

**Concepto:** Delay con AI — aprende el ritmo del usuario (Markov chain orden 2-3) y genera ecos coherentes con el groove en lugar de repeticiones literales.

**Sound:** Ecos que "saben" cuándo entrar. Más musical que un delay clásico.

**Use case live:** Live solos, vocal effects, dub-style call & response.

**Stack técnico:**
- AudioEffectDelay (Teensy built-in, hasta 1.4s)
- 64KB delay buffer
- Markov chain en código (~50KB models bundled)
- TinyML pattern detection trigger
- CPU: ~6%

**Parámetros user:**
- Time (delay base, sincronizado a BPM)
- Feedback (0-95%)
- Ghost amount (0% = classic delay, 100% = full Markov)
- Tape character (saturation + low-pass tail)

---

### 1.4 🌀 SPECTRAL SMEAR (Insert Layer)

**Concepto:** FFT-based frequency morphing. Toma el espectro del audio, lo difumina temporalmente, y lo recompone — "smears" las frecuencias en el tiempo.

**Sound:** Audio que se vuelve neblina espectral. Vocales se convierten en pads, pads se convierten en texturas.

**Use case live:** Drop transitions, vocal pad-ification, ambient washes.

**Stack técnico:**
- AudioAnalyzeFFT1024 (Teensy built-in)
- AudioSynthWaveform multi-osc bank (resíntesis)
- Frequency bin averaging buffer
- CPU: ~15%

**Parámetros user:**
- Smear time (50ms - 2s)
- Resolution (FFT band count 8-64)
- Spectral tilt (low vs high emphasis)
- Freeze (capture spectrum momentáneo)

---

### 1.5 📼 TAPE SATURATE (Master Layer)

**Concepto:** Saturación de cinta modelada con wow/flutter caótico. No es solo overdrive — incluye la modulación natural del transporte de cinta (LFO con drift).

**Sound:** Calidez vintage que envuelve al filter discreto. Color analog auténtico.

**Use case live:** Master bus saturation, vintage character on every patch.

**Stack técnico:**
- AudioEffectWaveshaper (custom curve: tape compression)
- AudioFilterStateVariable (HP/LP rolloff post-saturation)
- LFO drift modulando pitch ±2-15 cents
- CPU: ~5%

**Parámetros user:**
- Drive (0-12dB pre-saturation)
- Wow (LFO depth slow ~0.5Hz)
- Flutter (LFO depth fast ~6Hz)
- Age (rolloff hi-freq + noise floor)

---

### 1.6 ✂️ BIT SCULPT (Insert Layer)

**Concepto:** Bitcrusher inteligente que preserva la percepción de pitch incluso a bit-depths muy bajos. Algoritmo custom dithers strategically.

**Sound:** Lo-fi crujiente pero musical. Como Aphex Twin meets Daft Punk.

**Use case live:** Drops, glitch transitions, breaks digitales.

**Stack técnico:**
- AudioEffectBitcrusher (Teensy built-in) base
- Custom dither algorithm
- Sample rate reduction independiente
- CPU: ~4%

**Parámetros user:**
- Bits (16 → 1 bit)
- Sample rate (48kHz → 1kHz reduction)
- Sculpt (dither character)
- Mix (parallel processing)

---

### 1.7 🔔 MODAL REVERB (Send Layer)

**Concepto:** Reverb basada en modos resonantes de objetos físicos reales — campanas, guadua (bambú colombiano), placas de cristal, madera. Cada "modelo" es un set de filtros resonantes con tiempos de decay calibrados a mediciones reales.

**Sound:** Reverbs con identidad — "reverb de bambú colombiano" no existe en ningún otro producto.

**Use case live:** Signature sound, branding sonoro del Brain.

**Stack técnico:**
- 6-8× AudioFilterBiquad (modes paralelos)
- AudioMixer (densidad)
- Decay envelopes per-mode
- Modelos: Campana, Guadua, Cristal, Madera (futuro: cuero, metal)
- CPU: ~12%

**Parámetros user:**
- Material (campana/guadua/cristal/madera)
- Size (small/medium/large)
- Decay (200ms - 8s)
- Diffusion (modes count active)
- Mix

**Marketing hook único:** "Reverb de guadua colombiana — único en el mundo"

**Implementado en Sprint 2.7** — materiales: campana, guadua, cristal, madera.
CPU medido: 1.2%. Memoria: 5/11 bloques AudioMemory.
Doc: `apps/docs/sprints/12-modal-reverb.md`

---

### 1.8 🌗 PHASE CHORUS (Insert Layer)

**Concepto:** Chorus modelado como BOSS CE-2 vintage pero con LFO drift caótico (no perfectamente sinusoidal). Captura la imperfección del bucket-brigade delay analog.

**Sound:** Movimiento de chorus que respira, no robotic. "Vivo".

**Use case live:** Pads, leads, anything that needs analog "feel".

**Stack técnico:**
- AudioEffectChorus (Teensy built-in)
- AudioSynthWaveformPWM (LFO con drift caótico)
- Saturation pre/post chorus
- CPU: ~5%

**Parámetros user:**
- Rate (0.1-10Hz)
- Depth (0-100%)
- Drift (LFO chaos amount)
- Voices (1-4 chorus voices stacked)

---

### 1.9 🎭 PITCH MOSAIC (Insert Layer) [v1.0 simplified / v1.1 enhanced]

**Concepto:** Pitch shifter polifónico que genera 3 voces armonizadas en escalas seleccionadas. Combina con Layer 1 TinyML scale detection.

**Sound:** Una voz se convierte en chord completo. Polyphonic pitch magic.

**Use case live:** Vocal harmonization, mono synth → polyphonic, instant pads.

**Stack técnico (v1.0 simplified):**
- AudioEffectPitchShift x2 (Teensy built-in, granular based)
- AudioMixer4
- TinyML key detection (Layer 1)
- CPU: ~18% (v1.0), ~25% (v1.1)
- Limitación v1.0: pitch shift quality bueno pero no perfecto

**Parámetros user:**
- Interval 1 (semitones, -24 a +24)
- Interval 2 (semitones)
- Scale lock (chromatic vs scale-aware via TinyML)
- Mix

**v1.1 enhanced (free update Q1 2027):** algoritmo phase-vocoder custom para mejor calidad.

---

### 1.10 🌸 SPRING + PLATE (Master Layer)

**Concepto:** Dual reverb modeled — spring tank vintage (Hammond/Fender style) + plate reverb classic. Routing flexible: serial, parallel, o crossfade entre ambos.

**Sound:** El reverb vintage que todos quieren. Spring para drama, plate para silk.

**Use case live:** Master bus reverb, lead lines, vocal-style sounds.

**Stack técnico:**
- AudioEffectFreeverb x2 (Teensy built-in)
- Custom tuning per algoritmo
- Crossfade matrix
- CPU: ~10%

**Parámetros user:**
- Algorithm (spring / plate / blend)
- Decay (0.5-10s)
- Damping (HF rolloff)
- Pre-delay (0-100ms)
- Mix

---

### 1.11 🎰 GLITCH STUTTER (Insert Layer)

**Concepto:** Buffer stutter con AI pattern detection. No es solo "repetir buffer aleatoriamente" — TinyML detecta beats y crea stutters musicalmente coherentes.

**Sound:** Glitches que suenan intencionales. Hardware-style buffer manipulation.

**Use case live:** Drops, breaks, transitions agresivas, "DJ-style" effects.

**Stack técnico:**
- AudioEffectDelay (32KB buffer)
- Tap automation engine
- TinyML beat detection (Layer 1)
- Stutter patterns: 1/4, 1/8, 1/16, 1/32, random
- CPU: ~7%

**Parámetros user:**
- Pattern (1/4 - 1/32 division)
- Probability (0-100% chance per beat)
- Pitch (stutter pitch shift ±12 semitones)
- Mix

---

### 1.12 ⚡ SUB GENESIS (Master Layer)

**Concepto:** Sub-octave generator con análisis de pitch del input + saturación analógica modelada. Genera una octava sub coherente y la satura.

**Sound:** Bass weight masivo sin EQ surgery. El "thump" que falta.

**Use case live:** Live bass enhancement, sub-bass synthesis, dub style.

**Stack técnico:**
- AudioFilterStateVariable (LP @ 200Hz para pitch tracking)
- Pitch tracking algorithm
- AudioSynthWaveform (sub-octave generator)
- AudioEffectWaveshaper (saturation)
- CPU: ~4%

**Parámetros user:**
- Sub level (0-12dB)
- Octaves (1 down, 2 down)
- Drive (sub saturation)
- Cutoff (LP filter del sub)
- Mix

---

## 2. CPU Budget Analysis

| Layer | Efectos simultáneos típicos | CPU |
|---|---|---|
| Insert (1 activo) | Granular Cloud | ~12% |
| Send (2 activos) | Ghost Echo + Modal Reverb | ~18% |
| Master (2 activos) | Tape Saturate + Spring/Plate | ~15% |
| **Total worst case** | 5 FX + engine + filter analog control | **~45% Teensy 4.1** |

**Headroom restante:** ~55% para engine polyphony + TinyML inference + UI processing.

**Conclusión:** Todos los 12 efectos son viables. El usuario puede combinar hasta 5 FX simultáneamente sin xruns.

## 3. Mapeo de Efectos por Modo

### Modo Standalone
- 1 Insert + 1 Send + 1 Master simultáneo (preset-based selection)
- Encoders 1+2 controlan parámetros del FX activo
- BTN5 toggle FX on/off (legacy del GFD v1.0)

### Modo Live Act (Live Performance Mode)
- Hasta 5 FX simultáneos (1 Insert + 2 Send + 2 Master)
- Mapping a botones para hands-on control
- FX chains guardables como performances

### Modo Surface (con GroovePilot VST3)
- VST3 puede automatizar parámetros FX via Bridge Protocol
- AI cloud sugiere combinaciones de FX según contexto
- Mix-aware: el Brain recibe instrucciones del DAW

## 4. Roadmap

### v1.0 ship (Q3 2027)
- [x] Tape Saturate — Sprint 2.3 ✅ CPU 3%
- [x] Phase Chorus — Sprint 2.4 ✅ CPU 1.1%
- [x] Bit Sculpt — Sprint 2.5 ✅ CPU 0.6%
- [x] Sub Genesis — Sprint 2.6 ✅ CPU 0.8%
- [x] Modal Reverb (campana + guadua + cristal + madera) — Sprint 2.7 ✅ CPU 1.2%
- [ ] Cymatic Resonator — Sprint 2.8
- [ ] Granular Cloud — Sprint 2.9
- [ ] Spring + Plate — Sprint 2.10
- [ ] Ghost Echo (v1.0 sin Markov, v2.0 con TinyML) — Sprint 2.11

### v1.1 free update (Q1 2027)
- [ ] Spectral Smear
- [ ] Glitch Stutter (requiere TinyML beat detection — Sprint 4.5+)
- [ ] Pitch Mosaic simplified (requiere TinyML key detection — Sprint 4.3+)

### v1.2 free update (Q2 2027)
- [ ] Pitch Mosaic enhanced (phase-vocoder custom)
- [ ] Ghost Echo v2.0 — Markov chain rhythm prediction (requiere TinyML beat detection — Sprint 4.5+)
- [ ] FX chains community sharing (cloud Layer 2)

## 5. Live Testing Protocol (Juan/GPROG)

Cada efecto debe ser probado en al menos 2 contextos antes de pasar el QA:

1. **Studio test** — A/B contra efectos comerciales similares (DJ environment)
2. **Live show test** — Probado en gig real con audiencia, evaluar feel
3. **Future live act test** — Cuando esté listo para live act como GPROG

Feedback loop:
- ¿Suena diferenciado vs el equivalente comercial?
- ¿Aporta a la performance o es solo academic?
- ¿La latencia es imperceptible en context live?
- ¿Los parámetros son intuitivos en pánico de show?

Si cualquier efecto no pasa este filtro, se rediseña o reemplaza antes del v1.0 ship.

## 6. Marketing Hooks por Efecto

| Efecto | Hook de marketing |
|---|---|
| Cymatic Resonator | "Cristal vibrando dentro de tu mix" |
| Granular Cloud | "Donde las texturas viven" |
| Ghost Echo | "El delay que sabe a dónde va" |
| Spectral Smear | "Frecuencias que se difuminan en el tiempo" |
| Tape Saturate | "Calidez de cinta de los 70s" |
| Bit Sculpt | "Lo-fi inteligente, no random" |
| **Modal Reverb (guadua)** | **"Reverb de guadua colombiana — único en el mundo"** |
| Phase Chorus | "Chorus que respira como el CE-2" |
| Pitch Mosaic | "Una voz, tres armonías, escala automática" |
| Spring + Plate | "El reverb vintage que querés" |
| Glitch Stutter | "Glitches que suenan intencionales" |
| Sub Genesis | "Sub-bass weight, sin EQ surgery" |

## 7. Documentos relacionados

- GFD v3.0 — Master Strategy (parent)
- OpenSpec v0.3 — Technical Spec
- AI Architecture v0.1 — TinyML que power Ghost Echo + Glitch Stutter + Pitch Mosaic
- Filter Design Spec v0.1 — Discrete 2N3904 ladder filter (complementa todos los FX)

---

*End of FX Architecture v0.1*
*GrooveForge Brain · 12 Signature Effects · Juan Guerrero (GPROG)*
