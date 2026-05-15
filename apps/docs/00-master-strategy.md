# 📋 GrooveForge Brain — GFD v3.0 (Master Strategy & Spec)

> **Single Source of Truth (SSoT)** for all GrooveForge Brain strategy, architecture, and product decisions.
> **Status:** v3.0 final — May 15, 2026
> **Author:** Juan Guerrero (GPROG)
> **Children documents:**
> - 📐 OpenSpec v0.3 — Technical Spec
> - 🧠 AI Architecture v0.1 — 3-Layer Spec
> - 🎛️ FX Architecture v0.1 — 12 Signature Effects
> - 🎚️ Filter Design Spec v0.1 — Discrete 2N3904 Ladder
> - 🔌 Bridge Protocol v0.1 — Communication Spec
> - 📖 Implementation Roadmap v0.1 — Phase & Sprint Spec

---

## 1. Product Vision

**Name:** GrooveForge Brain
**Tagline:** *"Una pieza de hardware. Cinco niveles de superpoderes. Tú decidís hasta dónde llegar."*

**Core promise:**
> *"No pagues $2000-3000 por un Moog Subsequent, Prophet-5, Juno-60 reissue u OB-6 reissue. GrooveForge Brain te da seis arquitecturas (Moog Model D, Juno-106, Prophet-5, OB-6, DX7, ARP 2600) pasadas por filtro analógico discreto ladder real — mismo design del Moog Minimoog 1970. Más AI co-pilot integrado que se desbloquea según el ecosistema conectado. Por $599."*

---

## 2. Target Market

**Primary users:**
- Productores intermedios buscando boutique synth feel sin gastar $2000-3000
- Estudiantes Pete Tong Academy (partnership en proceso)
- Live act performers
- Makers que quieren expandir con slaves modulares

**Secondary users:**
- DJs producing original tracks
- Sound designers
- Music educators

**Geographic focus:**
- Phase 1: USA + Europe (online direct sales)
- Phase 2: LATAM (Bogotá-based)
- Phase 3: Worldwide

---

## 3. Pricing & Business Model

### 3.1 Pricing Tiers

| Tier | Price | Description |
|---|---|---|
| Pre-order (first 200) | $549 USD | Early supporter discount |
| PTDJA student price | $549 USD | Partnership with Pete Tong Academy |
| Standard retail | $599 USD | Main SKU |
| Bundle GP Pro 12mo | $649 USD | Brain + GroovePilot Pro subscription |

### 3.2 Cost Structure (BOM)

- BOM qty 100: ~$110 USD
- BOM qty 500-1000: ~$87 USD (volume discounts + amortized tooling)
- BOM qty 1000+: ~$75 USD

### 3.3 Margins

| Volume | BOM | Retail | Margin |
|---|---|---|---|
| 100 | $110 | $599 | 82% |
| 500 | $87 | $599 | 85% |
| 1000+ | $75 | $599 | 87% |

### 3.4 Financing Strategy

- **No external investment until 50-100 paying users**
- Pre-orders financian batch 1 manufacturing
- GroovePilot SaaS revenue subsidies Brain hardware dev
- Bootstrap model — slow but sustainable

---

## 4. 🌟 NORTH STAR: Modular Architecture of Incremental Superpowers

El GrooveForge Brain implementa una **arquitectura de capacidades incrementales**: el instrumento standalone ya es completo a $599, y cada elemento adicional conectado le agrega un "superpoder" inteligente.

### 4.1 The 5 Levels

```
NIVEL 0 — STANDALONE (out of the box, $599)
├── 6 emuladores clásicos (Moog/Juno/Prophet/OB-6/DX7/ARP)
├── Discrete 2N3904 ladder filter analog
├── Procesador de efectos (12 signature FX)
└── Synth boutique completo standalone

NIVEL 1 — + TECLADO MIDI (USB-A host)
└── Brain detecta lo que tocás → AI musical en vivo (TinyML)
   Scale Lock, Chord Recognition, Auto-Harmonization, Beat Follower

NIVEL 2 — + SLAVES via pogo (futuro v1.x)
└── E-Drum, Marimba, Air-Bamboo = ecosistema modular

NIVEL 3 — + WIFI (ESP32-S3 cloud)
└── Patch search NL, progression suggester, style transfer

NIVEL 4 — + DAW (GroovePilot VST3)
└── AI co-pilot completo para producción musical
```

**Cada nivel es independiente y opcional.** El usuario compra el Brain como Nivel 0 (synth standalone completo) y decide hasta dónde llegar agregando hardware o conectando al ecosistema GroovePilot.

### 4.2 The North Star as Decision Filter

De ahora en adelante, cualquier decisión de producto se filtra contra: **"¿Qué nivel del Brain está mejorando esta decisión?"**

Si no aporta a ningún nivel claro → scope creep → NO.

**Examples:**
- ¿Agregar pads silicona tipo Push? → No aporta a ningún nivel → ❌ NO
- ¿Agregar segundo encoder bank? → Mejora nivel 0 + 4 → 🟡 evaluar
- ¿Pantalla táctil grande? → No aporta inteligencia → ❌ overengineering
- ¿Input audio externo? → Mejora nivel 0 (procesador efectos) → 🟡 evaluar

---

## 5. Hardware Architecture v3.0

### 5.1 System Overview

```
TEENSY 4.1 (Audio Brain)
├── SGTL5000 via I2S nativo (MCLK perfecto, sin overlays)
├── Synth engines en Teensy Audio Library (C++)
├── Discrete 2N3904 ladder filter analógico
├── Encoders ALPS + Kailh Choc V2 + WS2812B LEDs via GPIO
├── USB-C composite: USB Audio + USB MIDI + CDC
├── USB-A host: teclado MIDI externo
├── Latencia audio <1ms determinística
└── UART 921600 ↔ ESP32-S3 (Bridge Protocol)

ESP32-S3 con GC9A01 1.28" integrado (AUX)
├── Módulo Waveshare ESP32-S3-Touch-LCD-1.28
├── Display GC9A01 round 240×240 (integrado en módulo)
├── WiFi 802.11 b/g/n + Bluetooth 5.0 LE
├── OTA updates Teensy + ESP32
├── Cloud sync (presets, genre profiles)
├── Web server local para config
└── SIN inferencia AI local (AI vive en VST3)
```

### 5.2 Critical Hardware Decisions

| Decision | Status | Reasoning |
|---|---|---|
| Audio brain: Teensy 4.1 | ✅ | MCLK nativo perfecto, <1ms latency, mature Audio Library |
| Display + network: ESP32-S3 con GC9A01 integrado | ✅ | Un módulo, mejor integración |
| DSP framework: Teensy Audio Library | ✅ | Mejor para 3-6 engines fijos vs Pure Data flexible |
| OS: Sin OS (bare-metal) | ✅ | Boot <2s, deterministic |
| AI inference: Cloud + VST3 (no local heavy) | ✅ | AI no necesita estar en Brain |
| Filter: Discrete 2N3904 ladder | ✅ | THE Moog signature sound |
| Codec: SGTL5000 | ✅ | Probado, mature, Teensy Audio compatible |
| Toolchain: PlatformIO | ✅ | CI/CD friendly, unified workflow |

### 5.3 Pivot History

- **v1.0 (initial):** Teensy 4.1 + slaves via RP2040
- **v2.0 (pivot):** Pi Zero 2W + RP2040 (simplification attempt — failed due to MCLK problems)
- **v3.0 (current):** Teensy 4.1 + ESP32-S3 (return to original Teensy-centric, improved with WiFi via ESP32-S3)

**Critical pivot trigger (May 15, 2026):**
After 6 hours debugging Pi 5 RP1 MCLK issues for SGTL5000, confirmed that Linux audio on Pi was not the right path. Combined with re-analysis that AI lives in GroovePilot VST3 (not Brain hardware), pivoted back to Teensy-centric architecture.

---

## 6. Differentiators (Why $599 is justified)

### 6.1 Market Comparison

| Product | Price | Analog filter | AI | Polyphony | Connectivity |
|---|---|---|---|---|---|
| Moog Mavis | $349 | ✅ ladder | ❌ | Mono | CV only |
| Korg NTS-1 | $99 | ❌ digital | ❌ | Mono | MIDI USB |
| Arturia MicroFreak | $349 | ✅ SP | ❌ | 4 voces | MIDI USB |
| Behringer Pro-1 | $549 | ✅ Curtis | ❌ | Mono | MIDI USB |
| Korg Minilogue XD | $649 | ✅ | ❌ | 4 voces | MIDI USB |
| Organelle M | $699 | ❌ digital | ❌ | Variable | USB+WiFi |
| Modal Argon8 | $799 | ❌ digital | ❌ | 8 voces | MIDI USB |
| **GrooveForge Brain** | **$599** | **✅ 2N3904 ladder** | **✅ 3-layer** | **6-8 voces** | **USB-C + WiFi + BT + USB-A host** |
| Push 3 Standalone | $999 | ❌ | ❌ | 8 voces | Standalone Linux |

### 6.2 Unique Selling Points

Único synth que combina:
- AI nativo integrado por capas (no add-on)
- Discrete transistor ladder authentic (Moog 1970)
- 6 arquitecturas clásicas en un instrumento
- 12 signature FX en hardware dedicado
- TinyML on-device para features instantáneas
- Connectivity completa (USB-C + WiFi + BT + USB-A host)
- Build quality boutique (guadua + aluminum)
- Latencia <1ms determinística

---

## 7. Roadmap Overview (15 months)

Detalles completos en **📖 Implementation Roadmap v0.1**.

| Phase | Period | Outcome |
|---|---|---|
| **Phase 0** — Foundation | Week 1-2 (May 2026) | Monorepo + CLAUDE.md + agents + skills |
| **Phase 1** — Audio Core | Week 3-6 (Jun 2026) | First engine + analog filter sounding |
| **Phase 2** — Multi-engine + FX | Week 7-14 (Jul-Aug 2026) | 3 engines + 6-8 FX signature |
| **Phase 3** — UI + Display | Week 15-20 (Sep-Oct 2026) | Encoders + buttons + GC9A01 fully working |
| **Phase 4** — MIDI + TinyML | Week 21-28 (Nov-Dec 2026) | USB-A host + 3 TinyML models |
| **Phase 5** — WiFi + Cloud | Week 29-32 (Jan 2027) | ESP32-S3 + cloud sync |
| **Phase 6** — DAW Bridge | Week 33-40 (Feb-Mar 2027) | GroovePilot VST3 integration |
| **Phase 7** — PCB + Manufacturing | Week 41-48 (Apr-May 2027) | PCB v0.1 → prototypes → batch 1 |
| **Phase 8** — Shipping | Week 49+ (Jun 2027+) | Pre-orders → assembly → fulfillment |

---

## 8. Operations & Brand

### 8.1 Brand Ecosystem

- **GroovePilot** (SaaS) — AI mix/master analysis copilot · groovepilot.co
- **GrooveMakers** (Community) — User community · groovemakers.co
- **GrooveForge** (Hardware) — Open-source modular instruments
- **GrooveForge Brain** — The first product of the hardware line

### 8.2 Visual Identity

- **Primary colors:** Dark anthracite + green/teal accents + purple AI accents
- **Typography:** Satoshi + General Sans
- **Materials:** Aluminum CNC + guadua (Colombian bamboo) + dark PETG
- **Vibe:** Boutique craft + technical precision + Latin American identity

### 8.3 Distribution Strategy

- **Phase 1:** Direct online (groovepilot.co/brain) + pre-orders
- **Phase 2:** PTDJA student program + select boutique resellers
- **Phase 3:** Reverb + Sweetwater + other premium retailers
- **No mass retail** (Guitar Center, Amazon) — preserve boutique positioning

---

## 9. Risks & Mitigations

| Risk | Probability | Impact | Mitigation |
|---|---|---|---|
| Teensy 4.1 supply crisis | Low | High | PJRC supply stable. Backup: STM32H7. |
| ESP32-S3 supply crisis | Low | Medium | Backup: RP2040 + display separated |
| Audio quality SGTL5000 insufficient | Low | Medium | Upgrade path: AKM AK4493 in v2.0 |
| FCC/CE certification blocking exports | High | Medium | Initially "educational use" disclaimer + Colombia direct |
| Burnout (Juan solo founder) | Medium | High | Hard 30-day pause gates + partner time protected |
| Patent claims (Moog ladder) | Low | Critical | Original Moog patent expired (1971+25 years). Verify Roland/Korg patents. |
| Manufacturing delays | High | Medium | JLCPCB SMT proven, 4-6 week lead time accounted |

---

## 10. Decision Log

| Date | Decision | Status | Reasoning |
|---|---|---|---|
| Early 2026 | Single SKU strategy ($449 initial) | ✅ | Avoid feature fragmentation |
| Mar 2026 | Discrete 2N3904 ladder over LM13700 | ✅ | Authentic Moog character |
| Apr 2026 | Pi Zero 2W as audio brain | ❌ Reverted | MCLK issues with Pi 5 RP1 |
| May 15, 2026 | **PIVOT v3.0: Teensy 4.1 + ESP32-S3** | ✅ FINAL | After 6h MCLK debug + AI architecture re-analysis |
| May 16, 2026 | PlatformIO as toolchain (not Arduino IDE) | ✅ | CI/CD + dependency management |
| May 16, 2026 | Pricing $599 (up from $549) | ✅ | AI features justify boutique pricing |
| May 16, 2026 | North Star: modular superpowers architecture | ✅ | Marketing + technical frame |
| May 16, 2026 | 12 signature FX defined and viability-checked | ✅ | Differentiation real, CPU budget OK |

---

## 11. Document History

| Version | Date | Changes |
|---|---|---|
| v1.0 | Mar 2026 | Initial — Teensy + slaves architecture |
| v2.0 | Apr 2026 | Pivoted to Pi Zero 2W (later reverted) |
| **v3.0** | **May 15, 2026** | **FINAL: Teensy 4.1 + ESP32-S3 + North Star clarified** |

---

*End of GFD v3.0 — Master Strategy & Spec*
*Single Source of Truth · Juan Guerrero (GPROG) · Bogotá, Colombia*
