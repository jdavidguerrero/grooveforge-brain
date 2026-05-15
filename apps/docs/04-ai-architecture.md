# 🧠 GrooveForge Brain — AI Architecture v0.1 (3-Layer Spec)

> **Canonical spec de la arquitectura AI del Brain.**
> **Parent:** GFD v3.0 — Master Strategy & Spec
> **Sibling:** OpenSpec v0.3
> **Status:** v0.1 — Mayo 15, 2026

---

## 0. Resumen ejecutivo

El GrooveForge Brain implementa una **arquitectura de superpoderes incrementales**: el instrumento standalone ya es completo a $599, y cada capa adicional (teclado MIDI, slaves, WiFi, DAW) le agrega capacidades de AI. El AI no es un feature monolítico — es una serie de "poderes" que se desbloquean según el ecosistema conectado.

Esta es la promesa de marketing Y la arquitectura técnica simultáneamente.

## 0.1 La arquitectura modular: superpoderes incrementales

```
NIVEL 0 — STANDALONE (out of the box)
├── 6 emuladores clásicos (Moog/Juno/Prophet/OB-6/DX7/ARP)
├── Discrete 2N3904 ladder filter analog
├── Procesador de efectos (12 signature FX)
└── Encoders + buttons + display
   → Synth boutique completo a $599

NIVEL 1 — + TECLADO MIDI (USB-A host)
├── + Scale Lock (TinyML)
├── + Chord Recognition (TinyML)
├── + Auto-Harmonization (TinyML)
├── + Beat Follower (TinyML)
└── + Genre Fingerprint (TinyML)
   → Synth inteligente que entiende lo que tocás

NIVEL 2 — + SLAVES via pogo (futuro v1.x)
├── + E-Drum: percusión inteligente con velocity ML
├── + Marimba electrónica: mallets aware
├── + Air-Bamboo: gesture controller con IMU
└── + Brain conoce contexto de todos
   → Ecosistema instrumental modular inteligente

NIVEL 3 — + WIFI (ESP32-S3 cloud)
├── + Patch search natural language
├── + Progression suggester
├── + Style transfer
└── + Community patches
   → AI cloud-augmented

NIVEL 4 — + DAW (GroovePilot VST3 via USB-C)
├── + Mix-aware sound
├── + Frequency conflict detection
├── + Layer suggestions
└── + Smart macros bidireccionales
   → AI co-pilot completo para producción musical
```

Cada nivel es **independiente y opcional**. El usuario compra el Brain como Nivel 0 (synth boutique completo) y le da "poderes" agregando hardware o conectándolo al ecosistema GroovePilot.

## 0.2 Marketing narrative

> **Standalone:** un synth boutique con seis arquitecturas clásicas y un filtro analógico discreto real, por $599.
>
> **Conectá un teclado MIDI** → el Brain detecta lo que tocás y se vuelve un asistente musical inteligente en tiempo real.
>
> **Conectá los slaves GrooveForge** (E-Drum, Marimba, Air-Bamboo) → toda la familia de instrumentos comparte un cerebro inteligente.
>
> **Conectálo al WiFi** → acceso a la inteligencia de GroovePilot cloud: búsqueda de presets por descripción, sugerencias de progresiones, style transfer.
>
> **Conectálo al DAW** → GroovePilot VST3 lo convierte en el co-pilot definitivo para tu producción musical.
>
> Una pieza de hardware. Cinco niveles de superpoderes. Tú decidís hasta dónde llegar.

## 0.3 Mapeo de capacidades AI a capas técnicas

Los "poderes" se distribuyen entre **tres capas técnicas** de AI, cada una con propósito, latencia y stack distintos:

```
LAYER 3 (DAW Bridge) — GroovePilot VST3 en Ableton
    ↑↓ USB-C composite (Audio + MIDI + CDC)
LAYER 2 (Cloud AI) — GroovePilot cloud API
    ↑↓ WiFi 802.11 b/g/n (ESP32-S3)
LAYER 1 (TinyML on-device) — Teensy 4.1 + TFLite Micro
```

---

## 1. LAYER 1 — TinyML on-device (Teensy 4.1)

### 1.1 Stack técnico

- **Runtime:** TensorFlow Lite Micro 2.14+
- **Acelerador:** CMSIS-NN (ARM optimized DSP)
- **MCU:** Teensy 4.1 (Cortex-M7 @ 600MHz, 1MB RAM, 8MB flash)
- **Lenguaje:** C++ con PlatformIO
- **Modelos:** TFLite quantized int8 o int16

### 1.2 Capacidad disponible

De los 1MB de RAM del Teensy 4.1:
- ~400KB para Teensy Audio Library + engines
- ~200KB para state + UI buffer
- ~200KB para TinyML tensor arena
- ~200KB margen seguridad

De los 8MB de flash:
- ~2MB para firmware base + Audio Library
- ~1MB para presets factory + genre profiles
- ~500KB para modelos TFLite bundled
- ~4.5MB margen / futuro growth

### 1.3 Features Layer 1 (instantáneas, sin internet)

| Feature | Tamaño modelo | Inference | UX |
|---|---|---|---|
| **Scale Lock** | 12KB | 4ms | Detecta key, snap notas a escala elegida |
| **Chord Recognition** | 45KB | 8ms | Mano izq → nombre acorde en display |
| **Auto-Harmonization** | 80KB | 12ms | Genera segunda voz armónica en tiempo real |
| **Beat Follower** | 30KB | 6ms | Tempo detection desde lo que tocás |
| **Genre Fingerprint** | 100KB | 18ms | Sugiere genre profile matching tu estilo |
| **Velocity Curve Learn** | 8KB | 3ms | Adapta curva velocity a tu fuerza de toque |
| **TOTAL** | **~275KB** | | Cabe en 8MB flash sin problema |

### 1.4 Cuándo se ejecuta cada modelo

- **Scale Lock:** trigger por usuario (button "Lock Scale")
- **Chord Recognition:** continuamente cuando hay 3+ notas simultáneas
- **Auto-Harmonization:** trigger por usuario (button "Harmonize")
- **Beat Follower:** continuamente background, actualiza tempo cada 4 bars
- **Genre Fingerprint:** cada 30 segundos en background
- **Velocity Curve Learn:** primer minuto de uso, calibra al usuario

### 1.5 Pipeline de entrenamiento

1. Datasets públicos: Lakh MIDI Dataset, Million Song Subset, Maestro
2. Training en Google Colab / local con TensorFlow
3. Quantization int8 con TFLite converter
4. Validation: latencia + accuracy + memory footprint
5. Bundle en firmware como C arrays via xxd

---

## 2. LAYER 2 — Cloud AI (via ESP32-S3 WiFi)

### 2.1 Stack técnico

- **Cliente:** ESP32-S3 con HTTPS + WebSocket
- **API:** GroovePilot cloud REST + WebSocket (compartido con VST3)
- **Auth:** API key bundle device + signed challenge
- **Backend:** Python FastAPI + Anthropic Claude API (o models propios)
- **Latencia:** async, 200-1000ms aceptable

### 2.2 Features Layer 2

| Feature | Endpoint | Trigger | Output |
|---|---|---|---|
| **Patch Search NL** | POST /search/patches | Encoder + text input | 5 presets matching descripción |
| **Progression Suggester** | POST /suggest/progression | Tap action button | 4-8 bar chord progression |
| **Style Transfer** | POST /transfer/style | Encoder + select style | Modified preset/MIDI sequence |
| **Community Patches** | GET /community/trending | Background sync | New patches por género |
| **Genre Profile Update** | GET /profiles/{id} | Background daily | Updated genre profile JSON |
| **OTA Firmware** | GET /firmware/check | Background weekly | Update available notification |

### 2.3 Privacy & data

- **Audio NUNCA sale del Brain.** Solo metadata: tempo, key, chord progression, genre tags
- User puede toggle Cloud Features ON/OFF
- Anonymous usage stats opcional (opt-in)
- Patches que el usuario comparte: explicit consent

### 2.4 Offline degradation

Sin WiFi:
- Layer 1 sigue 100% funcional
- Layer 2 features muestran "Offline" en display
- Cached patches y profiles siguen disponibles
- Cuando vuelve WiFi: sync automático

---

## 3. LAYER 3 — DAW Bridge (GroovePilot VST3)

### 3.1 Stack técnico

- **Conexión:** USB-C composite device (Audio + MIDI + CDC Serial)
- **Protocolo:** Bridge Protocol v0.1 sobre USB-CDC
- **VST3 host:** Ableton Live 11+ (otros DAWs en v2.0)
- **Bridge process:** GroovePilot VST3 plugin (JUCE C++)

### 3.2 Features Layer 3

| Feature | Direction | UX |
|---|---|---|
| **Mix-Aware Sound** | DAW → Brain | Brain ajusta filter/level según mix completo |
| **Frequency Conflict** | DAW → Brain | Display rojo cuando hay choque frecuencia |
| **Layer Suggestions** | DAW → Brain | "Add sub bass" → action button carga preset |
| **Smart Macros** | DAW ↔ Brain | Encoders rebindean según contexto DAW |
| **DAW Sync** | DAW → Brain | Tempo, key, bars, beats sincronizados |
| **Send Audio to GP** | Brain → DAW | USB Audio out del Brain analizable por GP |
| **Receive Suggestions** | DAW → Brain | Mix score, suggestions ranking |

### 3.3 Modos de operación

- **Standalone:** No DAW. Solo Layer 1 + 2 activos.
- **Surface mode:** Brain conectado, synth engines silenciados, controles → macros AI.
- **Hybrid mode:** Brain conectado, synth engines + AI macros simultáneos.

---

## 4. Flow completo: MIDI keyboard como input

```
[Teclado MIDI externo (USB)]
        ↓ USB-A host
[Teensy 4.1] ──────────────┐
        ↓                  │
   ┌────┴────┐              │
   ↓       ↓              ↓
[Synth] [Layer 1   ]    [USB-C out]
[engines][TinyML AI]    [MIDI + Audio]
   ↓       ↓              ↓
[Audio  [Display]      [GroovePilot VST3]
 out]   [chord, key,    ↓
         scale, beat,  [Layer 3 AI]
         suggestions]   ↓
   ↓                  [DAW: mix-aware,
[1/4" jacks            suggestions, etc.]
 L/R out]

         [ESP32-S3 WiFi]← trigger por usuario
            ↓
         [Layer 2 cloud AI]
            ↓
         [Patches, progressions, style]
```

---

## 5. Justificación del precio $599

### 5.1 Comparativa de mercado

| Producto | Precio | Filter analóg | AI | Polifonía | Comunicación |
|---|---|---|---|---|---|
| Moog Mavis | $349 | ✅ ladder | ❌ | Mono | CV |
| Korg NTS-1 | $99 | ❌ digital | ❌ | Mono | MIDI USB |
| Arturia MicroFreak | $349 | ✅ SP | ❌ | 4 voces | MIDI USB |
| Behringer Pro-1 | $549 | ✅ Curtis | ❌ | Mono | MIDI USB |
| Korg Minilogue XD | $649 | ✅ | ❌ | 4 voces | MIDI USB |
| Organelle M | $699 | ❌ digital | ❌ | Variable | USB+WiFi |
| Modal Argon8 | $799 | ❌ digital | ❌ | 8 voces | MIDI USB |
| **GrooveForge Brain** | **$599** | **✅ 2N3904 ladder** | **✅ 3-layer** | **6-8 voces** | **USB-C + WiFi + BT + USB-A host** |
| Push 3 Standalone | $999 | ❌ | ❌ | 8 voces | Standalone Linux |

### 5.2 Único en el mercado

Ningún competidor combina:
- AI nativo integrado (no add-on)
- Discrete transistor ladder authentic (Moog 1970)
- 6 arquitecturas clásicas en un instrumento
- TinyML on-device para features instantáneas sin internet
- Connectivity completa (USB-C + WiFi + BT + USB-A host)
- Build quality boutique (guadua + aluminum)

### 5.3 El AI es el diferenciador del precio

Sin AI, el Brain sería un boutique synth más en el mercado de $549. Con AI 3-layer:
- Layer 1 (TinyML): features instantáneas que ningún synth tiene
- Layer 2 (Cloud): expansibilidad continua via WiFi
- Layer 3 (DAW): integración profunda con producción musical

Esto justifica los $50-100 USD adicionales sobre competidores boutique sin AI.

---

## 6. Roadmap AI

### v1.0 ship features
- [ ] Layer 1: Scale Lock + Chord Recognition + Beat Follower
- [ ] Layer 2: Patch Search NL + Genre Profile Update
- [ ] Layer 3: Mix-Aware Sound + Basic Macros

### v1.1 (Q1 2027) free update
- [ ] Layer 1: Auto-Harmonization
- [ ] Layer 2: Progression Suggester
- [ ] Layer 3: Frequency Conflict Detection

### v1.2 (Q2 2027) free update
- [ ] Layer 1: Genre Fingerprint + Velocity Curve Learn
- [ ] Layer 2: Style Transfer + Community Patches
- [ ] Layer 3: Layer Suggestions + Smart Macros

### v2.0 (2028+) possibilities
- [ ] Layer 1: small Generative model on-device
- [ ] Layer 2: voice control ("hey brain, make this warmer")
- [ ] Layer 3: Multi-DAW support (Logic, FL Studio, Bitwig)

---

## 7. Riesgos

| Riesgo | Mitigación |
|---|---|
| TFLite Micro performance insuficiente | CMSIS-NN aceleración + quantización agresiva |
| Modelos no caben en 8MB flash | Streaming desde SD card opcional |
| Cloud API costos altos por user | Free tier limitado + paid GP Pro acceso ilimitado |
| Privacy concerns con cloud | Audio NUNCA sale del Brain, solo metadata |
| Latencia WiFi excesiva | Async UX + Layer 1 fallback siempre disponible |

---

## 8. Métricas a trackear

- TinyML inference time (target <20ms p99)
- TinyML memory footprint (target <400KB total)
- Cloud API latency (target <800ms p95)
- Feature usage analytics (anonymous, opt-in)
- Accuracy métricas por modelo (target >85%)

---

## 9. Documentos relacionados

- GFD v3.0 — Master Strategy & Spec (parent)
- OpenSpec v0.3 — Technical Spec
- Bridge Protocol v0.1 — Communication Spec
- Filter Design Spec v0.1 — Analog Filter Spec
- FX Architecture v0.1 — 12 Signature Effects

---

*End of AI Architecture v0.1*
*GrooveForge Brain · 3-Layer AI Spec · Juan Guerrero (GPROG)*
