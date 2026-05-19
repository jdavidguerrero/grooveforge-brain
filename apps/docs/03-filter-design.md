# 🎚️ GrooveForge Brain — Filter Design Spec v0.1

> **Discrete 2N3904 transistor ladder filter specification.**
> **Parent:** GFD v3.0 — Master Strategy & Spec
> **Sibling:** OpenSpec v0.3
> **Status:** v0.1 — Mayo 15, 2026
> **Reference design:** Moog Minimoog 1970 (Bob Moog's original transistor ladder)

---

## 0. Filosofía del diseño

El filter analógico es **el corazón sonoro del GrooveForge Brain**. Es lo que justifica el posicionamiento "Moog quality por $599" — no es un emulación digital, es el mismo circuito que usó Bob Moog en 1970.

Cualquier productor que conoce el sonido Moog reconoce inmediatamente la diferencia entre un filter digital (incluso el mejor IIR/FIR) y un filter discreto con transistores reales. Ese reconocimiento es nuestro diferenciador.

---

## 1. Topology

### 1.1 Architecture overview

```
Audio in ──[buffer TL072]──▶ Stage 1 (Q1/Q2 differential pair)
                                    │
                                    ▼ + cap 1nF polystyrene
                              Stage 2 (Q3/Q4)
                                    │
                                    ▼ + cap 1nF polystyrene
                              Stage 3 (Q5/Q6)
                                    │
                                    ▼ + cap 1nF polystyrene
                              Stage 4 (Q7/Q8)
                                    │
                                    ▼
                            [Output buffer TL072]
                                    │
                                    ├──▶ Resonance feedback to Stage 1
                                    │
                                    ▼
                            [CD4066 software bypass]
                                    │
                                    ▼
                            [TPDT hardware bypass toggle]
                                    │
                                    ▼
                              Audio out (to SGTL5000 ADC)

  Cutoff CV ───▶ [TL072 exp converter] ───▶ Stage emitter currents (Q1-Q8)
  Resonance pot ────────────────────────▶ Feedback amount (output → input)
  V/oct trim ──────────────────────────▶ Exp converter scale
  Cutoff range trim ───────────────────▶ Exp converter offset
  Resonance trim ──────────────────────▶ Max feedback gain
  Bypass level trim ───────────────────▶ Bypass path unity-gain matching
  Teensy GPIO 25 ──▶ [CD4066 enable] ──▶ Software bypass control
```

### 1.2 Por qué transistor ladder?

- **4-pole low-pass (24dB/oct):** la steepness signature del Moog sound
- **Voltage-controlled:** permite modulation rápida via CV (LFO, envelope)
- **Self-oscillating resonance:** característica de Moog que digital filters emulan pobremente
- **Analog warmth:** distorsión harmonic suave bajo alta resonance
- **No aliasing:** no hay sample rate limitations

### 1.3 Por qué 2N3904 (no especializado)?

- **Disponibilidad:** 2N3904 es el transistor más común del mundo, supply infinito
- **Costo:** $0.05/unidad en cantidad
- **Matching:** procesando 100-150 transistores podemos obtener 8 pairs con ΔVbe<2mV
- **Performance:** suficiente noise/distortion specs para audio quality boutique
- **Reproducibility:** cualquier maker puede fabricar este filter sin componentes raros

Alternatives consideradas y descartadas:
- **CA3046 / CA3086 transistor array:** mejor matching de fábrica, pero EOL (discontinuado)
- **THAT300P:** matched pairs garantizados, pero $5/pair = no escala
- **MAT12 / SSM2210:** ideal pero $15+ y disponibilidad incierta

---

## 2. Specifications

### 2.1 Audio specifications

| Spec | Target | Notes |
|---|---|---|
| Cutoff range | 20Hz - 20kHz | 5+ decades |
| Resonance | 0 (flat) to self-oscillation | Self-osc threshold 80% pot |
| V/oct tracking | ±10 cents over 5 octaves | Post-calibration |
| Input impedance | 100kΩ | Standard line-level |
| Output level | ~1Vrms | Matched a SGTL5000 ADC input |
| THD @ 1kHz, 0dBu | <0.5% (low res), <2% (high res) | Intentional character |
| Roll-off | 24dB/oct | 4-pole architecture |

### 2.2 Bypass behavior

| Bypass type | Latency | Audio path |
|---|---|---|
| Software (CD4066) | <1ms | DAC → CD4066 → ADC (bypassing filter) |
| Hardware (TPDT) | 0ms (true bypass) | DAC → ADC directo (cable connection) |

Hardware TPDT siempre toma precedencia (master override).

---

## 3. Bill of Materials (per unit)

### 3.1 Active components

| Component | Qty | Purpose | Cost |
|---|---|---|---|
| 2N3904 NPN transistor (matched) | 8 | Ladder differential pairs (Q1-Q8) | $0.40 |
| 2N3904 NPN transistor (spare/converter) | 4 | Exp converter CV input | $0.20 |
| TL072 dual opamp | 2 | Input/output buffers + CV converter | $0.80 |
| CD4066 quad analog switch | 1 | Software bypass control | $0.30 |

### 3.2 Passive components

| Component | Qty | Purpose | Cost |
|---|---|---|---|
| Capacitor 1nF polystyrene | 4 | Timing caps between stages | $0.60 |
| Resistor 1% metal film (various) | 30 | Bias network + signal path | $0.30 |
| Trimmer 10kΩ multi-turn | 4 | Calibration (cutoff, V/oct, res, bypass) | $1.00 |
| Pot 10kΩ logarítmico panel | 1 | Resonance control user | $0.80 |

### 3.3 Control & UI

| Component | Qty | Purpose | Cost |
|---|---|---|---|
| Toggle switch TPDT panel | 1 | Hardware bypass master | $0.50 |

### 3.4 PCB

| Component | Qty | Purpose | Cost |
|---|---|---|---|
| Filter PCB sub-block 30x40mm | 1 | 2-layer dedicated filter PCB | $0.50 |

**Total filter BOM: ~$5.40 per unit**

---

## 4. Transistor Matching Procedure

### 4.1 Theory

Vbe (voltage base-emitter) at constant collector current is the key parameter that determines how a transistor responds in a differential pair configuration. If Q1 and Q2 have different Vbe at the same current, the pair will have offset and distortion that translates to filter non-linearity.

**Target:** ΔVbe < 2mV at 100µA collector current, 25°C.

### 4.2 Matching jig (Arduino-based)

```
                  +5V
                   │
                  [33kΩ]
                   │
                   ├──── Vbase
                  [Q test]  (in DIP socket / ZIF)
                   │
                  ─── (collector to Vcc through 47kΩ to set 100µA)
                   │
                  Veb (to Arduino ADC)
                   │
                  GND
```

Arduino sketch reads Vbe via ADC, displays in mV, logs to serial.

### 4.3 Procedure

1. Insert transistor #1 into socket
2. Read Vbe (e.g. 612.4 mV)
3. Press button → store value with index
4. Insert transistor #2 → read → store
5. Repeat for batch of 100 transistors (~30 min)
6. Software sorts and groups into pairs with ΔVbe < 2mV
7. Output: 8-10 matched pairs labeled (e.g. "Pair A: T034 + T067 = 612.4/612.8 mV")
8. Reject rate: 10-15% of batch

### 4.4 Tools needed

- Arduino UNO/Nano
- DIP-8 socket o ZIF socket for TO-92
- 2× pulsador momentáneo
- 2× LED + resistores 330Ω
- LCD 16x2 (opcional, mejor UX que solo Serial)
- Breadboard grande
- 100-150 2N3904 transistores

---

## 5. Calibration Procedure

Each filter (after assembly) needs 5-step calibration to meet spec.

### 5.1 Cutoff range (R_offset trim)

**Goal:** sweep 20Hz to 20kHz with CV from 0V to +5V.

1. Apply 0V CV, sine 220Hz signal input
2. Adjust R_offset trim until cutoff is at 20Hz (signal heavily attenuated)
3. Apply +5V CV, verify cutoff at ~20kHz (signal mostly unattenuated)
4. Iterate as needed

### 5.2 V/oct tracking (R_voct trim)

**Goal:** ±10 cents tracking over 5 octaves.

1. Apply CV at A1 reference (110Hz)
2. Set cutoff just above 110Hz, measure phase shift
3. Apply CV at A6 reference (1760Hz)
4. Verify cutoff at corresponding frequency
5. Adjust R_voct until error within 10 cents per octave
6. Iterate

### 5.3 Resonance (R_res trim)

**Goal:** self-oscillation at 80% resonance pot.

1. Disconnect input signal
2. Turn resonance pot to 100%
3. Adjust R_res until filter starts self-oscillating
4. Reduce pot to 80% — should be just at threshold
5. Verify oscillation amplitude is reasonable (not clipping)

### 5.4 Bypass level (R_bypass trim)

**Goal:** unity gain between filter active and bypass (no level jump on switch).

1. Apply 1Vrms test signal at 1kHz
2. Filter active, no resonance: measure output
3. Activate bypass: measure output
4. Adjust R_bypass until levels match within 0.5dB
5. Verify across frequency sweep (no spectral imbalance)

### 5.5 Pass/fail verification

Final QA:
- Sweep 20Hz-20kHz, no clicks/discontinuities
- Self-oscillation clean (no clipping at 80% pot)
- Bypass transition silent (no audible click)
- THD measured at 1kHz, 0dBu, low res: <0.5%

**Time per unit:** 5-7 minutes with calibration jig.

---

## 6. Engine Integration

### 6.1 Filter routing per engine

| Engine | Filter default | Reason | User override |
|---|---|---|---|
| Moog Model D | ON | Authentic Moog character | Yes |
| Juno-106 | ON | Juno + Moog warmth (hybrid sound) | Yes |
| Prophet-5 | ON | Prophet + Moog warmth (hybrid sound) | Yes |
| OB-6 | ON | OB-6 + Moog warmth (hybrid sound) | Yes |
| DX7 | **OFF** | Clean FM, no analog coloring | Yes (toggle ON for "warmed FM") |
| ARP 2600 | ON | ARP + Moog warmth (hybrid sound) | Yes |

Per-engine default stored in Teensy firmware. Teensy controls CD4066 via GPIO 25. Hardware TPDT toggle position siempre toma precedencia (master override).

### 6.2 CV control

Filter cutoff is controlled by CV from Teensy DAC (PWM filtered + opamp buffer):

```
Teensy PWM @ 200kHz ──[LP filter 10kHz]──[TL072 buffer]──▶ Filter CV in
        (0-3.3V)              ↓
                          (0-5V scaled via opamp gain)
```

Resonance is user-controlled directly via panel pot.

### 6.3 Interacción filtro analógico ↔ filtros digitales internos por engine

Cuando el filtro analógico está activo (CD4066 no en bypass, GPIO 25 = LOW), la señal de
todos los engines lo atraviesa. Cada engine tiene además su propio filtro digital interno.
La interacción entre ambos depende del tipo de filtro del engine:

| Engine | Filtro digital interno | Topología | Cuando analógico activo |
|---|---|---|---|
| Moog Model D | Ladder 4-pole (24dB/oct) | **Mismo tipo** que el analógico | **BYPASS digital** |
| Juno-106 | State-variable 2-pole | Diferente | Mantener activo — sonido híbrido intencional |
| Prophet-5 | State-variable 2-pole | Diferente | Mantener activo — sonido híbrido intencional |
| OB-6 | State-variable 2-pole | Diferente | Mantener activo — sonido híbrido intencional |
| ARP 2600 | State-variable multimode | Diferente | Mantener activo — sonido híbrido intencional |
| DX7 | Sin filtro (FM puro) | N/A | N/A (analógico OFF por default — §6.1) |

**Rationale MoogModelD — bypass digital obligatorio:**

Dos filtros ladder en serie (digital 4-pole + analógico 4-pole) producen un equivalente de
8 polos (48dB/oct). El corte resultante es tan agresivo que destruye el carácter del engine
— prácticamente silencia todo por encima del cutoff. El firmware desactiva el filtro digital
del Moog Model D siempre que el analógico esté activo, dejando el 2N3904 como filtro único.

**Rationale engines restantes — apilado intencional:**

Los filtros state-variable (Juno-106, Prophet-5, OB-6) y ARP tienen topologías distintas al
ladder. Apilarlos con el 2N3904 no duplica la pendiente de la misma forma — el digital moldea
el timbre base del engine (su identidad sonora), y el 2N3904 agrega coloración analógica encima.
Esto es la propuesta de valor: "motor digital con carácter analógico Moog".

**Implementación en firmware (engine_manager.cpp — sprint pendiente):**

```cpp
// Llamar en cambio de engine y en cambio de estado del bypass analógico
void sync_digital_filter_state() {
    bool analog_active = (digitalRead(PIN_FILTER_BYPASS) == LOW); // GPIO 25 LOW = activo

    if (current_engine == ENGINE_MOOG_MODEL_D) {
        // Bypass digital cuando analógico activo — evita 8-pole (48dB/oct)
        moog_engine.setDigitalFilterEnabled(!analog_active);
    }
    // Todos los demás engines: filtro digital siempre activo independiente del analógico
}
```

**User override (v2.0):** futuro toggle "expert mode" permite forzar digital+analógico
apilados en MoogModelD para experimentación — no en v1.0.

---

## 7. PCB Layout Guidelines

### 7.1 Filter PCB sub-block

- 2-layer PCB, 30×40mm
- Top layer: signal traces (audio path)
- Bottom layer: ground plane (analog GND)
- Connection to main PCB via 8-pin header (audio in, audio out, CV, +5V analog, -5V analog, GND, bypass control, TPDT state)

### 7.2 Critical layout rules

- **Star ground:** all analog GND traces converge at ONE physical point near SGTL5000
- **Digital/analog separation:** filter PCB ground plane connects to analog GND only
- **Power filtering:** LC filter (10µH + 100µF) between digital 5V and filter +5V
- **Trace length:** signal path minimized (shorter = less noise pickup)
- **Decoupling:** 100nF ceramic + 10µF electrolytic at each opamp Vcc/Vee pin
- **Trimmer access:** mount trimmers on top side with calibration access through enclosure top

---

## 8. Risks & Mitigations

| Risk | Probability | Impact | Mitigation |
|---|---|---|---|
| 2N3904 matching reject rate >20% | Medium | Low | Increase batch from 150 to 200 transistors |
| Calibration time per unit >10 min | Medium | Medium | Automate via Arduino jig + scripts |
| Filter noise floor too high | Low | High | Star ground discipline + power supply LC filter |
| Temperature drift in cutoff | Medium | Low | 2N3904 has reasonable thermal coupling in lab conditions |
| Trimmers drift over time | Low | Low | Use Bourns 3296 multi-turn (high quality) |
| Self-oscillation amplitude too high | Low | Medium | Limit via R_res trim + soft-clip at output buffer |

---

## 9. Future enhancements (v2.0+)

- **24V analog supply:** higher headroom, lower noise (current 5V is compromise for boutique simplicity)
- **Voltage-controlled resonance:** additional CV input for resonance modulation
- **Per-stage stereo:** dual-mono filter for true stereo processing
- **Filter morphing:** continuous morph between LP/BP/HP/Notch via additional mixing
- **Saturation stages:** controlled distortion between ladder stages for tube-like character

---

## 10. References

- **Moog Music patent US3475623** (1969-1971): "Electronic Synthesizer" — Bob Moog's original ladder filter patent (expired 1996)
- **Sound on Sound** "The Moog Ladder Filter" technical article series
- **Will Pirkle**, "Designing Software Synthesizer Plug-Ins in C++" (digital reference for comparison)
- **Hutchins, Bernie**, "Musical Engineer's Handbook" — analog synth design references
- **CircuitSalad** open-source Moog ladder filter design (community-validated reference)

---

## 11. Documentos relacionados

- GFD v3.0 — Master Strategy (parent)
- OpenSpec v0.3 — Technical Spec
- FX Architecture v0.1 — Effects que interactúan con el filter
- Implementation Roadmap — Sprint 1.3 (matching) + Sprint 1.4 (filter protoboard)

---

*End of Filter Design Spec v0.1*
*GrooveForge Brain · Discrete 2N3904 Ladder Filter · Juan Guerrero (GPROG)*
