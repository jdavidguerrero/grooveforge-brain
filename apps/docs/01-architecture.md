# 📐 GrooveForge Brain — OpenSpec v0.3

> **Canonical technical specification** for GrooveForge Brain.
> **Methodology:** OpenSpec — spec-first, source of truth for all implementation.
> **Parent:** GFD v3.0 — Master Strategy & Spec (SSoT)
> **Status:** v0.3 final — May 15, 2026
> **Author:** Juan Guerrero (GPROG)

---

## 0. Document Purpose

Esta OpenSpec define **qué es el GrooveForge Brain** a nivel técnico, no comercial. Todo el firmware, hardware design, software architecture y testing se deriva de este documento. Si hay conflicto entre este doc y código/diseño, este doc gana.

El GFD v3.0 (parent) tiene la estrategia de producto. Este doc tiene el contrato técnico.

---

## 1. Product Overview

**Name:** GrooveForge Brain
**Tagline:** *"Una pieza de hardware. Cinco niveles de superpoderes. Tú decidís hasta dónde llegar."*

### 🌟 Arquitectura modular de superpoderes incrementales (NORTH STAR)

```
NIVEL 0 — STANDALONE (out of the box, $599)
  6 emuladores + ladder filter discreto + 12 FX = synth boutique completo

NIVEL 1 — + TECLADO MIDI (USB-A host)
  Scale Lock + Chord Recognition + Auto-Harmonization + Beat Follower (TinyML)

NIVEL 2 — + SLAVES via pogo (futuro v1.x)
  E-Drum + Marimba + Air-Bamboo = ecosistema modular inteligente

NIVEL 3 — + WIFI (cloud)
  Patch search NL + progression suggester + style transfer (cloud)

NIVEL 4 — + DAW (GroovePilot VST3)
  Mix-aware + frequency conflict + layer suggestions + smart macros
```

Cada nivel es opcional. El producto base ya es completo — las capas son expansión, no requisito.

### Pricing

- Standard retail: **$599 USD**
- Pre-order primeros 200: **$549 USD**
- PTDJA student price: **$549 USD**
- Bundle con GroovePilot Pro 12mo: **$649 USD**
- BOM total qty 100: ~$110 USD
- Margen unitario @ $599 / $110 = **82%**
- Margen qty 500-1000: BOM ~$87, margen sube a **85%**

### Toolchain: PlatformIO (no Arduino IDE)

- Mismo workflow para Teensy 4.1 + ESP32-S3
- Dependency management via platformio.ini
- CI/CD friendly + VS Code integration
- Repos separados: brain-teensy/ y brain-esp32/

### Differentiators

- Único synth boutique con AI integrado por capas (no add-on)
- Discrete transistor ladder real (Moog 1970 design) a precio asequible
- Seis arquitecturas clásicas en un instrumento
- 12 signature FX diseñados para live performance
- Arquitectura modular pogo pin extensible
- Estética boutique craft (guadua + aluminum)
- Audio latency <1ms (Teensy bare-metal, sin OS jitter)
- Toolchain unificado: PlatformIO

---

## 2. System Architecture v3.0

```
┌──────────────────────────────────────────────────────────────────┐
│                    GrooveForge Brain v3.0                         │
├──────────────────────────────────────────────────────────────────┤
│                                                                    │
│  ┌────────────────┐  AUDIO BRAIN                                  │
│  │   Teensy 4.1   │  - Cortex-M7 @ 600MHz + FPU + SIMD            │
│  │                │  - Teensy Audio Library (synth engines C++)    │
│  │                │  - I2S nativo al SGTL5000 (MCLK perfecto)      │
│  │                │  - USB-C composite: Audio + MIDI + CDC         │
│  │                │  - USB-A host: teclado MIDI externo            │
│  └────────┬───────┘                                                │
│           │ I2S                                                    │
│           ▼                                                        │
│  ┌────────────────┐  AUDIO CODEC                                  │
│  │   SGTL5000     │  - 24-bit/48kHz                                │
│  │                │  - ADC + DAC en un chip                        │
│  └────────┬───────┘                                                │
│           │ Analog signal                                          │
│           ▼                                                        │
│  ┌────────────────┐  ANALOG LADDER FILTER                         │
│  │  2N3904 Ladder │  - 4-pole discrete transistor ladder           │
│  │  + TL072 x2    │  - Moog Minimoog 1970 design                   │
│  │  + CD4066      │  - SW bypass via GUI (CD4066, pin 25)           │
│  │                │  - 8 matched 2N3904 pairs                      │
│  └────────┬───────┘                                                │
│           ▼                                                        │
│  ┌────────────────┐  AUDIO OUTPUTS                                │
│  │  TRS L/R 1/4"  │  Line out balanced                            │
│  │  + Headphones  │  3.5mm TRS                                    │
│  └────────────────┘                                                │
│                                                                    │
│  ┌────────────────┐                                                │
│  │ Encoders ALPS  │  GPIO directo al Teensy                       │
│  │ Kailh switches │  4 botones (B1-B4) + 3 encoders ALPS EC11      │
│  │ WS2812B LEDs   │  16 LEDs: 12 ring ENC NAV + 4 keycap underglow │
│  │ Volume pot     │  ADC analog                                   │
│  └────────────────┘                                                │
│                                                                    │
│             ↕ UART 921600 (Bridge Protocol)                       │
│                                                                    │
│  ┌────────────────┐  NETWORK + UI CO-PROCESSOR                    │
│  │ ESP32-S3 con   │  - Módulo Waveshare ESP32-S3-Touch-LCD-1.28   │
│  │ GC9A01 1.28"   │  - Display GC9A01 240×240 integrado            │
│  │ integrado      │  - WiFi 802.11 b/g/n                          │
│  │                │  - Bluetooth 5.0 LE                           │
│  │                │  - OTA updates                                │
│  │                │  - Cloud sync presets/profiles                │
│  │                │  - Web server local config                    │
│  └────────────────┘                                                │
│                                                                    │
│  ┌────────────────┐  MODULAR EXPANSION                            │
│  │ Pogo connector │  6-pin magnetic lateral                       │
│  │                │  GrooveForge Protocol v0.1 (I2C 400kHz)        │
│  │                │  Slaves: E-Drum, Marimba, Air-Bamboo (futuro) │
│  └────────────────┘                                                │
│                                                                    │
│  ┌────────────────┐  POWER                                        │
│  │  USB-C 5V in   │  - LiPo 3000mAh + IP5306                       │
│  │  + LiPo backup │  - LC filter audio rail (10µH + 100µF)         │
│  │                │  - Analog/digital ground star separation       │
│  └────────────────┘                                                │
└──────────────────────────────────────────────────────────────────┘
```

### 2.1 Audio Signal Path — Modo Synth vs Modo FX Processor

El GrooveForge Brain opera en **dos modos exclusivos**, alternados con doble-push
del encoder NAV (ver §3.4). Cada modo tiene un camino de señal distinto. El filtro
analógico 2N3904 vive en un **loop externo al SGTL5000**: el DAC alimenta la entrada
del filtro y el retorno del filtro entra al `LINE_IN`.

**Modo Synth** — el Teensy genera audio de síntesis; pasa por el filtro analógico:

```
Teensy engine ─I2S─▶ SGTL5000 DAC ─▶ LINE_OUT ─▶ [filtro 2N3904 ladder]
                                                         │
                          SGTL5000 LINE_IN ◀─ 74HC4053 ◀─┘
                                 │
                  ┌──────────────┴──────────────┐
                  ▼                             ▼
            ADC ─▶ Teensy                 jacks de salida
            (captura USB Audio,           (ruteo interno SGTL5000)
             análisis ML)
```

El filtro analógico está disponible para **todos los engines** (`03-filter-design.md §6.1`), cada uno con cutoff/resonance defaults calibrados para su carácter sonoro. En modo Synth, todos los engines pasan por el hardware de filtro; el CD4066 (GPIO 25) permite bypass por software. Los modelos ML (Beat Follower, Chord Recognizer, Key Detector) modulan el CV del filtro para comportamiento contextual (Sprint 5B.4).

**Modo FX Processor** (estilo Roland RMX-1000) — audio externo entra por el jack
`FX IN`; el Teensy aplica los 12 FX digitales; el filtro analógico queda fuera:

```
Jack FX IN 1/4" TRS ─▶ 74HC4053 ─▶ SGTL5000 LINE_IN ─▶ ADC ─▶ Teensy FX chain
                                                                     │
              jacks de salida ◀─ SGTL5000 ◀─ LINE_OUT ◀─ DAC ◀───────┘
```

**Switch de ruteo 74HC4053 (GPIO 27):** triple SPDT que selecciona la fuente del
`LINE_IN` del SGTL5000 — retorno del filtro (Synth) o jack `FX IN` (FX Processor).
Canal A = `LINE_IN_L`, canal B = `LINE_IN_R`, canal C spare. `GPIO 27` LOW = Synth,
HIGH = FX. Análisis completo: `apps/docs/theory/audio-routing-dual-mode.md`.

**Salida:** en ambos modos los jacks de salida se alimentan desde el SGTL5000 — el
ruteo interno por I2C selecciona la fuente según el modo. No requiere switch externo.

### Key architecture decisions

| Componente | v0.2 (anterior) | v0.3 (FINAL) | Razón |
|---|---|---|---|
| Audio brain | Pi Zero 2W (Linux) | **Teensy 4.1 (bare-metal)** | MCLK nativo, latencia <1ms, no Linux complexity |
| Display + WiFi | RP2040 + GC9A01 + ESP32 | **ESP32-S3 + GC9A01 integrado** | Un solo módulo, mejor integración |
| DSP framework | Pure Data Vanilla | **Teensy Audio Library C++** | Mejor performance, fit perfecto para 6 engines fijos |
| OS | Buildroot Linux | **Sin OS (bare-metal)** | Boot <2s, deterministic |
| AI inference | Local en Pi | **Vive en GroovePilot VST3** | AI no necesita estar en Brain |
| Filtro | 2N3904 ladder discreto | **2N3904 ladder discreto** | ✅ Mantiene authentic Moog sound |
| Codec | SGTL5000 | **SGTL5000** | ✅ Mismo, ahora con MCLK perfecto |

---

## 3. Hardware Specification

### 3.1 Bill of Materials (qty 100)

| Componente | Costo | Notas técnicas |
|---|---|---|
| **Teensy 4.1** | $32.00 | Cortex-M7 @ 600MHz, 1MB RAM, FPU, native I2S |
| **ESP32-S3 con GC9A01 1.28" integrado** | $22.00 | Waveshare ESP32-S3-Touch-LCD-1.28 o equivalente |
| SGTL5000 IC standalone | $4.00 | Chip bare en PCB producción + caps + clock |
| Oscilador 12.288 MHz DIP-4 3.3V | $2.00 | Opcional — Teensy provee MCLK nativo |
| ALPS EC11 encoders x3 | $6.30 | ENC L + ENC R + ENC NAV — todos con push integrado |
| Knobs aluminum CNC x2 + 1 central | $1.80 | ENC L/R: knobs estándar; ENC NAV: knob más prominente |
| Kailh Choc V2 Brown switches x4 | $1.20 | B1-B4 (dual set keycaps: negro instalado + transparente en sobre) |
| Keycaps PBT rectangular x4 negro | $1.20 | Instalados de fábrica — sin serigrafía (display da contexto) |
| Keycaps PBT rectangular x4 transparente | $0.80 | En sobre (dual set) |
| Keycap circular custom PETG/PBT x1 | $0.50 | ENC NAV — diseño Juan, icono ● grabado láser |
| WS2812B-2020 SMD x16 | $1.20 | 12 ring ENC NAV + 4 keycap underglow — cadena única, 1 pin Teensy |
| Volume pot 10kΩ log + knob | $1.00 | Panel mount + aluminum knob |
| USB-C + USB-A connectors | $0.80 | USB-C device, USB-A host |
| Audio jacks 1/4" TRS x2 (output L/R) | $3.60 | Switched ground |
| Audio jacks 1/4" TRS x2 (FX IN L/R) | $3.60 | Entrada Modo FX Processor — estéreo |
| Audio jack 3.5mm x1 (headphones) | $0.60 | Headphones |
| **74HC4053 triple SPDT analog switch** | **$0.35** | Ruteo audio Synth/FX — GPIO 27 |
| LiPo 3000mAh + IP5306 + LC filter | $9.00 | Battery operation + audio rail filter |
| **2N3904 NPN x12 (matched pairs)** | **$0.60** | 8 ladder + 4 spare/exp converter |
| **TL072 dual opamp x2** | **$0.80** | Input/output buffers + CV converter |
| **CD4066 quad analog switch** | **$0.30** | Software bypass control |
| ~~Toggle switch TPDT panel-mount~~ | ~~$0.50~~ | ~~Eliminado — bypass 100% software vía CD4066~~ |
| **Capacitors 1nF polystyrene x4 + passives** | **$1.30** | Filter timing caps + trimmers + resistors |
| ~~Resonance pot 10kΩ log~~ | ~~$0.80~~ | ~~Eliminado — resonance controlada por ENC R (digital)~~ |
| **Filter PCB sub-block 30x40mm** | **$0.50** | 2-layer |
| Pogo connector 6-pin magnetic | $1.50 | Würth o equivalente |
| PCB principal 4-layer 180x100mm | $6.00 | Audio + digital + power planes |
| Enclosure PETG print | $5.00 | Color-stable PETG |
| Aluminum top panel CNC | $6.00 | 2mm anodizado, Bogotá |
| Guadua panel finished | $2.50 | Local sourcing + sealing |
| Passives + cables + screws + misc | $4.50 | Resistores + caps + standoffs |
| **Subtotal componentes** | **~$113** | Incluye 74HC4053 + jack FX IN 1/4" (Modo FX Processor) |
| Ensamblaje + QC + filter cal | $10.00 | Filter cal 5-7 min/unidad |
| **TOTAL per unit (qty 100)** | **~$123** | |

> **Volumen 500-1000u:** BOM baja a ~$90
> **Volumen 1000+:** BOM baja a ~$78

### 3.2 Power Architecture

```
USB-C 5V input ──┬──▶ IP5306 (charge)
                 │       │
                 │       ▼
                 │   LiPo 3.7V 3000mAh
                 │       │
                 │       ▼
                 │   IP5306 (boost 5V)
                 │       │
                 ▼       ▼
              ┌──────────────┐
              │  Power Rail   │
              │     5V        │
              └──────┬───────┘
                     │
           ┌─────────┼─────────┬─────────┐
           ▼         ▼         ▼         ▼
        Teensy 4.1  ESP32-S3  LCD/LEDs  USB-A host
        (5V→3.3V    (5V→3.3V             (5V passthrough)
        internal)   internal)

        ┌──────────────────────┐
        │   LC Filter audio    │
        │   10µH + 100µF       │
        │   MANDATORY          │
        └──────────┬───────────┘
                   ▼
              ┌──────────┐
              │ SGTL5000 │
              │ + 2N3904 │ ladder filter
              │ + TL072  │
              └──────────┘

        Analog GND ━━━ star ground point ━━━ Digital GND
```

**Critical rules:**
- LC filter (10µH + 100µF) entre digital 5V y audio rail
- Star ground: analog GND y digital GND conectan en UN punto físico cerca del SGTL5000
- Teensy y ESP32-S3 corren de digital 5V (regulación interna a 3.3V)
- SGTL5000 + filter discreto corren de audio 5V filtered
- Battery debe soportar pico ~500mA (Teensy + ESP32 + LEDs + WiFi TX simultáneo)

### 3.3 Pin Mapping

**Teensy 4.1 GPIO usage:**

| Pin | Function | Conexión |
|---|---|---|
| 23 | MCLK out | SGTL5000 master clock (nativo desde Teensy I2S1_MCLK) |
| 7 | I2S1_TX_DATA0 (OUT1A) | SGTL5000 DAC input |
| 8 | I2S1_RX_DATA0 (IN1) | SGTL5000 ADC output |
| 20 | I2S1_LRCLK | SGTL5000 LRCLK |
| 21 | I2S1_BCLK | SGTL5000 BCLK |
| 18 | I2C1 SDA | SGTL5000 control |
| 19 | I2C1 SCL | SGTL5000 control |
| 0 | UART RX | ← ESP32-S3 TX |
| 1 | UART TX | → ESP32-S3 RX |
| 2-5 | GPIO | 4 Kailh Choc V2 switches B1-B4 (pulled-up) |
| 10-11 | GPIO | ENC L: A / B (rotary phases) |
| 12 | GPIO | ENC L: SW (push — filter bypass toggle) |
| 13 | GPIO | ENC R: A |
| 14 | GPIO | ENC R: B |
| 15 | GPIO | ENC R: SW (push — resonance reset a 0) |
| 16 | GPIO | ENC NAV: A |
| 17 | GPIO | ENC NAV: B |
| 26 | GPIO | ENC NAV: SW (push — confirm / AI·Action / double-push = mode switch) |
| 25 | GPIO | Filter bypass control (CD4066 enable — ENC L push via firmware) |
| 27 | GPIO | 74HC4053 mode select — ruteo audio Synth/FX (LOW=Synth, HIGH=FX) |
| 28 | WS2812B DIN | 16 LEDs cadena única: 12 ring ENC NAV + 4 keycap underglow |
| A0 (14) | ADC | Volume pot read |
| 36 | GPIO | Pogo INT in (slave interrupt) |
| 37/38 | I2C2 SDA/SCL | Pogo bus master (slaves) |
| 39 | GPIO | USB host mode select |

> **Nota I2S (Teensy 4.x):** los pines de datos I2S son 7 (`SAI1 OUT1A`, TX → DAC)
> y 8 (`SAI1 IN1`, RX ← ADC) — los del Audio Shield Rev D2. La Rev C / Teensy 3.x
> usaba 22/13; con este mapping los pines 13 y 22 quedan libres.

**ESP32-S3 (Waveshare ESP32-S3-Touch-LCD-1.28):**

| Pin | Function | Conexión |
|---|---|---|
| GPIO 17 | UART TX (Serial1) | → Teensy RX (pin 0) |
| GPIO 16 | UART RX (Serial1) | ← Teensy TX (pin 1) |
| GPIO 43 | UART0 TX (Serial debug) | USB-UART bridge interno — NO usar para bridge |
| GPIO 44 | UART0 RX (Serial debug) | USB-UART bridge interno — NO usar para bridge |
| GPIO 7/8/9/10/11/12 | SPI display | GC9A01 (internal connection) |
| GPIO 13 | Touch I2C SDA | Touch controller (si touch version) |
| GPIO 14 | Touch I2C SCL | Touch controller |
| GPIO 18 | Display backlight PWM | GC9A01 backlight |
| (Internal) | WiFi + BT | 802.11 b/g/n + BT 5.0 LE |

### 3.4 UI Controls Spec — Panel v5 Final

> **SSoT:** Notion "Hardware UI Spec — CERRADO (Panel v5 Final)" · Mayo 17, 2026

**Filosofía:** minimalista boutique — 3 encoders + 4 botones + 1 vol pot. Más cercano al OP-1 que al Elektron.

#### Inventario de controles

| ID | Tipo | Turn | Push |
|---|---|---|---|
| ENC L | ALPS EC11 | Synth: Cutoff / FX: Dry-Wet | Filter bypass toggle |
| ENC R | ALPS EC11 | Synth: Resonance / FX: Depth/param principal | Resonance reset a 0 |
| ENC NAV | ALPS EC11 | Navegar menú / páginas / FX | 1× confirm/AI·Action · 2× rápido = modo SYNTH↔FX |
| B1-B4 | Kailh Choc V2 Brown | — | Synth: OSC/ENV/LFO/PRESET · FX: INSERT/SEND/MASTER/slot |
| VOL | Pot 10kΩ log | Volumen salida (siempre, no cambia con modo) | — |

#### Modo SYNTH

```
ENC L    → Cutoff del filtro analógico discreto 2N3904
ENC R    → Resonance
ENC NAV  → Navegar páginas del synth (OSC / ENV / LFO/MOD / ENGINE / PRESET)
ENC L push  → Filter bypass CD4066 ON/OFF (pin 25)
ENC R push  → Resonance reset a 0
ENC NAV push → Confirm / AI·Action (escala al nivel activo: 0=menú, 1=Scale Lock, 3=cloud, 4=DAW)
B1=OSC · B2=ENV · B3=LFO/MOD · B4=PRESET/ENGINE
```

#### Modo FX (RMX-1000 inspired)

```
ENC L    → Dry/Wet del FX activo
ENC R    → Parámetro principal del FX activo (definido por cada FX)
ENC NAV  → Scroll entre los 12 FX disponibles
ENC NAV push → Activar/desactivar FX seleccionado
B1=INSERT · B2=SEND · B3=MASTER · B4=FX slot favorito
```

**Cambio de modo:** doble push ENC NAV (≤ 2 s)

#### LED ring ENC NAV (12× WS2812B-2020 SMD)

| Estado | Animación | Color |
|---|---|---|
| Synth mode | Pulso suave | Teal `#1D9E75` |
| FX mode | Pulso suave | Purple `#534AB7` |
| Cambio de modo | Sweep 360° | Teal → Purple |
| IA procesando | Giro rápido | Teal brillante |
| Sugerencia lista | 3 pulsos + fijo | Teal |
| DAW conectado | Fijo | Purple |
| Error / sin conexión | Pulso lento | Rojo |
| Scale Lock activo | Fijo | Verde |

#### ENC R mapping por FX (parámetro principal en FX mode)

| FX | ENC L (Dry/Wet) | ENC R (Depth/param) |
|---|---|---|
| Cymatic Resonator | Mix | Resonance (Q) |
| Granular Cloud | Mix | Size (grain duration) |
| Ghost Echo | Mix | Feedback |
| Spectral Smear | Mix | Smear time |
| **Tape Saturate** | **Mix** | **Drive** |
| Bit Sculpt | Mix | Bits |
| Modal Reverb | Mix | Decay |
| Phase Chorus | Mix | Depth |
| Pitch Mosaic | Mix | Interval 1 |
| Spring + Plate | Mix | Decay |
| Glitch Stutter | Mix | Pattern division |
| Sub Genesis | Mix | Sub level |

### 3.5 Discrete 2N3904 Ladder Filter Design

**Topology:** 4-pole low-pass transistor ladder (Moog Minimoog 1970 reference)

Ver doc separado: **🎚️ Filter Design Spec v0.1** para detalles completos.

**Specifications:**
- Cutoff range: 20Hz - 20kHz (5+ decades)
- Resonance: 0 (flat) to self-oscillation
- V/oct tracking: ±10 cents over 5 octaves (post-calibration)
- Bypass: CD4066 software bypass controlado por GUI (Bridge Protocol → Teensy pin 25)
- Input impedance: 100kΩ
- Output level: matched a SGTL5000 ADC input range (~1Vrms line level)
- THD @ 1kHz, 0dBu: <0.5% (low resonance), <2% (high resonance, intentional)
- Self-oscillation threshold: 80% resonance pot ±5%

**Engine routing per engine:**

| Engine | Filter default | Sound | User override |
|---|---|---|---|
| Moog Model D | ON | Auténtico Moog | Yes |
| Juno-106 | ON | Juno + Moog warmth | Yes |
| Prophet-5 | ON | Prophet + Moog warmth | Yes |
| OB-6 | ON | OB-6 + Moog warmth | Yes |
| DX7 | **OFF** | Clean FM | Yes (toggle ON for "warmed FM") |
| ARP 2600 | ON | ARP + Moog warmth | Yes |

### 3.6 Top Panel PCB Architecture

> **Decisión:** 2026-05-27. Documentado por primera vez aquí.

#### Problema

Los encoders (ENC1, ENC2, ENC NAV) y los botones (B1-B4) están montados sobre el top
panel de aluminio, que tiene **BODY_H_F=42mm de altura**. La PCB principal (Teensy) está
en el fondo del PETG body. Conectar componentes del top panel directamente a la main PCB
requeriría cables cortos en ángulo o una PCB rígida que no cabe en el espacio.

#### Decisión: 2-PCB approach

| PCB | Nombre | Dimensión estimada | Contenido | Conexión |
|-----|--------|--------------------|-----------|----------|
| Main PCB | `groovebrain-main` | 168×88mm | Teensy 4.1, SGTL5000, power, jacks, USB | — |
| Top Panel PCB | `groovebrain-panel` | ~130×25mm | ENC1, ENC2, ENC NAV, B1-B4, WS2812B ring | JST ribbon → main PCB |

**Top Panel PCB** es una PCB delgada (~25mm de alto) que se monta bajo el aluminio:
- Footprints EC11 para los 3 encoders (ENC NAV tiene 4 pads extra para WS2812B data + 5V)
- Footprints Kailh Choc V2 para los 4 botones (retroiluminados — 2 pads extra por switch)
- Header JST-SH 1.0mm (o similar) de N pines hacia la main PCB
- 4 agujeros M2.5 para tornillos que fijan la PCB bajo el panel de aluminio

**ENC NAV vs ENC1/ENC2 en la misma PCB:**
- ENC NAV footprint: 3 pines encoder + 2 pines LED ring (data WS2812B + GND)
- ENC1/ENC2 footprint: solo 3 pines encoder — el pad de ring simplemente no se popula
- Una sola PCB sirve para las 3 posiciones de encoder

#### Layout del panel v0.5 (SSoT)

```
Y_panel (0=trasero, 100=frontal)
  100 ──────────────────────────────────────────────── frente del instrumento
        [VOL]                               (X=22, Y=14)
   22   ──────── [ENCNAV]  ────────────────            (X=90, Y=22)
        ───── LED ring ────                            (X=90, Y=22, Ø32mm)

   44   [ENC1]  [B2]       DISPLAY   [B4]  [ENC2]     Y=44: botones inferiores
        X=22    X=52        X=90      X=128  X=158
   55   [ENC1]                               [ENC2]    Y=55: encoders laterales
   60           DISPLAY                                Y=60: display center
   66   [ENC1]  [B1]       DISPLAY   [B3]  [ENC2]     Y=66: botones superiores

  ...
    0 ──────── panel trasero
```

**Asignación de botones:**

| ID | Posición | X_panel | Y_panel | Modo SYNTH | Modo FX |
|----|----------|---------|---------|------------|---------|
| B1 | col izq, superior | 52mm | 66mm | OSC | INSERT |
| B2 | col izq, inferior | 52mm | 44mm | ENV | SEND |
| B3 | col der, superior | 128mm | 66mm | LFO/MOD | MASTER |
| B4 | col der, inferior | 128mm | 44mm | PRESET/ENGINE | FX slot fav |

#### Módulo display (Waveshare ESP32-S3-Touch-LCD-1.28) — dimensiones verificadas por datasheet

| Parámetro | Valor | Fuente |
|-----------|-------|--------|
| Vidrio OD (LENS OD) | Ø38.51mm | Waveshare datasheet |
| Viewing area activa | Ø35.67mm | Waveshare datasheet |
| PCB circular | Ø41mm | Waveshare datasheet |
| Stack TP+LCD height | 3.58mm | Waveshare datasheet |

**Concepto de instalación (corrección v0.5):** El módulo va **debajo** del panel de aluminio:
- El cutout Ø36mm en el aluminio es una **ventana** — ni el vidrio (38.51mm) ni el PCB (41mm) pasan por él
- El vidrio del módulo se presiona contra la **cara inferior** del aluminio, mirando hacia arriba
- El labio del vidrio (38.51mm OD) impide que el módulo caiga por el cutout (36mm < 38.51mm)
- Desde arriba: el usuario ve el display circular recesado 2mm (grosor del aluminio) — efecto estético limpio
- El **display bracket (Part 05)** retiene el módulo desde abajo, presionándolo contra el aluminio:
  - Cavity circular ≥ Ø41mm para el PCB
  - Altura = BRACKET_H = 15mm (desde PCB principal hasta cara inferior del top panel)

Los pines UART (GPIO43/44) del ESP32-S3 se cablan directamente a la main PCB (Teensy)
via cable que desciende por el interior del bracket y el PETG body.
El Top Panel PCB **no lleva** conector para la pantalla.

#### Prototipo v1 vs PCB v1.0

| Fase | Implementación |
|------|---------------|
| **Prototipo v1** (actual) | Cables directos de cada encoder/botón a la main PCB. Sin Top Panel PCB. Complejidad de cableado: ~18 señales. |
| **PCB v1.0** | Top Panel PCB fabricada. JST ribbon de 18-22 pines. Cableado limpio. |

---

## 4. Software Architecture

### 4.1 Teensy 4.1 (Audio Brain)

**Framework:** PlatformIO + Arduino framework + Teensyduino + Teensy Audio Library

**Synth engines (v1.0 ship con 3, expandible):**

| Engine | Status | Implementation |
|---|---|---|
| Moog Model D | Ship v1.0 | AudioSynthWaveform x3 + AudioMixer4 + AudioEffectEnvelope + filter routing |
| Juno-106 | Ship v1.0 | AudioSynthWaveform + sub osc + AudioEffectChorus + filter routing |
| Prophet-5 | Ship v1.0 | AudioSynthWaveform x2 + cross-mod + filter routing |
| OB-6 | v1.1 (Q1 2027) free update | AudioSynthWaveform x2 + AudioSynthWaveformDc + filter |
| DX7 | v1.2 (Q2 2027) free update | AudioSynthWaveformPWM x6 (FM operators) + filter bypass |
| ARP 2600 | v1.3 (Q3 2027) free update | AudioSynthWaveform + AudioEffectRingMod + filter |

**Polyphony targets:**
- Moog Model D: 6 voces simultáneas
- Juno-106: 8 voces simultáneas
- Prophet-5: 5 voces simultáneas
- CPU usage @ 6 voces simultáneas: ~30% Teensy 4.1
- Headroom para FX adicionales: ~70%

### 4.2 ESP32-S3 (Network + UI Co-processor)

**Framework:** PlatformIO + Arduino (ESP32 board package) + LVGL

**Tasks:**
- WiFi management (connect, reconnect, OTA check)
- Bluetooth LE (MIDI BLE optional, file transfer)
- HTTP client to grooveforge.com cloud API
- Local web server para configuration (port 80, mDNS as `groovebrain.local`)
- Display driver GC9A01 (rendering UI based on state from Teensy)
- UART communication con Teensy @ 921600 baud (Bridge Protocol)

### 4.3 Bridge Protocol v0.1

Protocolo binario sobre UART (Teensy ↔ ESP32) y USB-CDC (Teensy ↔ VST3 host).

```
[CMD][LEN][SEQ][PAYLOAD][CRC8]
```

Ver doc separado: **🔌 Bridge Protocol v0.1 — Spec**

**Communication paths:**

```
VST3 plugin ←─ USB-CDC ─→ Teensy 4.1 ←─ UART ─→ ESP32-S3 ←─ WiFi ─→ GroovePilot cloud
```

Teensy es el master del Bridge Protocol. ESP32 actúa como proxy network layer.

### 4.4 OTA Updates

- ESP32-S3 maneja WiFi connectivity
- Check de updates contra https://updates.grooveforge.com
- ESP32 descarga firmware delta + checksum
- ESP32 puede flashear Teensy via UART + reset GPIO (Teensy bootloader UART)
- ESP32 flashea su propio firmware (OTA partition)
- Rollback automático si boot falla 3 veces consecutivas

---

## 5. Acceptance Criteria

### 5.1 Hardware acceptance (per unit, prod line)

- [ ] Boot completo a synth selector en <2 segundos
- [ ] Audio out detectable en 1/4" L/R con test tone (-12dBFS sin distortion)
- [ ] USB-C reconocido como composite device (Audio + MIDI + CDC) en macOS/Win/Linux
- [ ] USB-A host detecta teclado MIDI clase-compliant
- [ ] WiFi conecta a network conocido en <5 segundos
- [ ] Battery dura ≥2.5h en modo standalone con volumen 75%
- [ ] LiPo charge time <3h desde 0% a 100%
- [ ] LC filter mide >40dB atenuación noise digital en audio rail @ 100MHz
- [ ] Display GC9A01 muestra splash en <500ms post-boot
- [ ] 6 Kailh switches responden con WS2812B feedback inmediato
- [ ] 2 encoders track ±1 detent precision
- [ ] Action button + LED ring funcional
- [ ] Pogo connector multimetro: continuidad 6 pines, no shorts
- [ ] **Filter: cutoff sweep audible 20Hz-20kHz**
- [ ] **Filter: self-oscillates con resonance al 80% pot**
- [ ] **Filter: software bypass CD4066 sin clicks audibles al switch (GUI → pin 25)**
- [ ] **Filter: V/oct tracking ±20 cents over 5 octaves (post-calibration)**

### 5.2 Software acceptance (per release)

- [ ] Boot time Teensy + ESP32 < 2 segundos
- [ ] Audio latency: USB MIDI input → audio out <5ms p95
- [ ] Engine polyphony: Moog 6 voces, Juno 8 voces, Prophet 5 voces sin xruns
- [ ] CPU usage Teensy ≤60% en uso normal
- [ ] Bridge Protocol latency: command → ack <50ms p95
- [ ] OTA update completo (download + flash + reboot) <5 min
- [ ] OTA rollback funciona si boot falla 3 veces
- [ ] Pogo slave hot-plug detected en <5 segundos
- [ ] WiFi reconnect automático tras disconnect

### 5.3 Audio quality acceptance

- [ ] THD+N audio path total <0.1% @ 1kHz, -12dBFS (filter bypass)
- [ ] SNR audio path >90dB A-weighted
- [ ] Frequency response: ±0.5dB de 20Hz-20kHz (filter bypass)
- [ ] Channel crosstalk L/R <-80dB @ 1kHz
- [ ] Noise floor con todo conectado silenciado: <-95dBFS
- [ ] Filter resonance peak: ≥+20dB sobre nominal at self-oscillation
- [ ] Filter V/oct tracking: ±20 cents over 5 octaves (post-calibration)

---

## 6. Build Instructions (Dev)

### 6.1 Bootstrap dev environment

```bash
# Instalar VS Code + PlatformIO extension
# https://platformio.org/install/ide?install=vscode

# Clone repos
git clone git@github.com:gprog/grooveforge-brain.git
cd grooveforge-brain
```

### 6.2 First audio test (Teensy + Audio Shield)

```cpp
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

AudioSynthWaveform    sine;
AudioOutputI2S        audioOut;
AudioControlSGTL5000  codec;

AudioConnection c1(sine, 0, audioOut, 0);
AudioConnection c2(sine, 0, audioOut, 1);

void setup() {
    AudioMemory(8);
    codec.enable();
    codec.volume(0.5);
    sine.begin(WAVEFORM_SINE);
    sine.frequency(440);
    sine.amplitude(0.8);
}

void loop() {}
```

`platformio.ini`:
```ini
[env:teensy41]
platform = teensy
board = teensy41
framework = arduino
lib_deps =
    PaulStoffregen/Audio
    PaulStoffregen/Encoder
    PaulStoffregen/Bounce2
monitor_speed = 115200
build_flags =
    -DUSB_MIDI_AUDIO_SERIAL
```

Flash al Teensy 4.1. Conectar headphones al jack del Audio Shield. Tono 440Hz inmediato.

### 6.3 ESP32-S3 display test

```cpp
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

void setup() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(2);
    tft.setCursor(40, 100);
    tft.print("GROOVEFORGE");
    tft.setCursor(70, 130);
    tft.print("BRAIN v3.0");
}

void loop() {}
```

---

## 7. Roadmap & Milestones (overview)

Detalles en doc separado: **📖 Implementation Roadmap v0.1**

| Milestone | Target | Status |
|---|---|---|
| Teensy 4.1 + Audio Shield primer tono via Audio Library | Semana 1 | 🔴 Pending |
| Filter discreto 2N3904 protoboard + matching jig Arduino | Semana 2-3 | 🔴 Pending |
| Primer engine Moog Model D en Teensy Audio Library | Semana 4 | 🔴 Pending |
| A/B test: filter digital vs filter discreto analog | Semana 5 | 🔴 Pending |
| ESP32-S3 display + UART communication con Teensy | Semana 6 | 🔴 Pending |
| Engine #2 (Juno-106) + Engine #3 (Prophet-5) | Mes 3-4 | 🔴 Pending |
| PCB v0.1 schematic completo en KiCad | Mes 5 | 🔴 Pending |
| PCB v0.1 fabricated + prototype hand-soldered | Mes 6 | 🔴 Pending |
| End-to-end prototype completo | Mes 7 | 🔴 Pending |
| Pre-order page abierta | Mes 8 | 🔴 Pending |
| Alpha 10 units enviadas a beta testers | Mes 9 | 🔴 Pending |
| Manufactura batch 1 JLCPCB SMT | Mes 11 | 🔴 Pending |
| Shipping batch 1 | Mes 12 | 🔴 Pending |
| v1.1 OB-6 engine free update | Mes 14 | 🔴 Pending |

---

## 8. Risks & Mitigations

| Riesgo | Probabilidad | Impacto | Mitigación |
|---|---|---|---|
| Teensy 4.1 supply crisis | Baja | Alto | PJRC supply stable. Alternative: STM32H7 con custom audio framework. |
| ESP32-S3 supply crisis | Baja | Medio | Alternativa: RP2040 + display GC9A01 separado |
| Teensy Audio Library limitations | Baja | Medio | Alternativa: bare-metal con CMSIS-DSP. PJRC mantiene library activamente. |
| Filter discreto matching más complejo de lo esperado | Media | Bajo | Jig procedure documentado. 10-15% reject rate aceptable. |
| Audio quality SGTL5000 insuficiente | Baja | Medio | Upgrade path: AKM AK4493 + ADC separado en v2.0 |
| FCC/CE certification block exports | Alta | Medio | Sell desde Colombia con "educational use" disclaimer |
| Burnout Juan por scope | Media | Alto | Gates duros de pausa cada 30 días |

---

## 9. Document History

| Version | Date | Changes |
|---|---|---|
| v0.1 | Mayo 2026 | Initial draft — Pi Zero 2W + RP2040 + LM13700 dual-SKU |
| v0.2 | Mayo 2026 | Single SKU + discrete 2N3904 ladder filter (Pi Zero 2W kept) |
| **v0.3** | **Mayo 15, 2026** | **PIVOT FINAL: Teensy 4.1 + ESP32-S3 + discrete 2N3904 ladder. Pi Zero 2W eliminado. Teensy Audio Library para engines. Retail $599.** |

---

*End of GrooveForge Brain — OpenSpec v0.3 FINAL*
*Canonical technical spec · GrooveForge Brain v3.0*
*Juan Guerrero (GPROG) · Bogotá, Colombia*
