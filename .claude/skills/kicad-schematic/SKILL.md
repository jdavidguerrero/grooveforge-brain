# Skill: KiCad Schematic

Referencia operacional para schematic capture y PCB layout del GrooveForge Brain en KiCad.
Cross-ref: `apps/docs/03-filter-design.md` §7, `apps/docs/01-architecture.md` §3.

---

## Estructura del proyecto KiCad

```
apps/hardware/kicad/
├── grooveforge-brain.kicad_pro    # proyecto principal
├── grooveforge-brain.kicad_sch    # schematic top-level
├── sheets/                        # hierarchical sheets
│   ├── audio-codec.kicad_sch      # SGTL5000 + I2S
│   ├── filter-ladder.kicad_sch    # 2N3904 ladder filter (sub-PCB)
│   ├── microcontrollers.kicad_sch # Teensy 4.1 + ESP32-S3
│   ├── power.kicad_sch            # IP5306 + LiPo + LC filter
│   ├── ui.kicad_sch               # Encoders + switches + LEDs
│   └── connectors.kicad_sch       # Audio jacks + USB + Pogo
├── grooveforge-brain.kicad_pcb    # layout PCB
├── fp-lib-table                   # footprint libraries
├── sym-lib-table                  # symbol libraries
└── fab/                           # outputs de fabricación
    ├── gerbers/
    ├── drill/
    └── bom-jlcpcb.csv
```

---

## Convenciones de nets

Nets críticos — usar estos nombres exactos para consistencia:

| Net | Descripción |
|---|---|
| `AUDIO_IN` | Signal de entrada al filter ladder |
| `AUDIO_OUT` | Signal de salida del filter (post-bypass) |
| `FILTER_CV` | Control voltage → cutoff (0-5V) |
| `RESONANCE_FB` | Feedback de resonancia (output → input Stage 1) |
| `FILTER_BYPASS` | Control CD4066 desde Teensy GPIO 25 |
| `TPDT_STATE` | Lectura del toggle TPDT → Teensy GPIO 27 |
| `ANALOG_5V` | Rail de audio filtrado (post LC filter) |
| `DIGITAL_5V` | Rail digital principal |
| `ANALOG_GND` | Ground analógico (solo side SGTL5000 + filter) |
| `DIGITAL_GND` | Ground digital (Teensy, ESP32, LEDs) |
| `STAR_GND` | Punto único donde ANALOG_GND y DIGITAL_GND se unen |
| `I2S_MCLK` | Master clock SGTL5000 (Teensy pin 23) |
| `I2S_BCLK` | Bit clock (Teensy pin 21) |
| `I2S_LRCLK` | L/R clock (Teensy pin 20) |
| `I2S_TX` | Data Teensy → SGTL5000 (pin 22) |
| `I2S_RX` | Data SGTL5000 → Teensy (pin 13) |
| `UART_T2E_TX` | Teensy TX → ESP32 RX (Teensy pin 1, ESP32 GPIO 44) |
| `UART_T2E_RX` | Teensy RX ← ESP32 TX (Teensy pin 0, ESP32 GPIO 43) |

---

## Hierarchical sheets

Cada subsistema es un sheet separado para mantener el schematic manejable:

```
grooveforge-brain.kicad_sch (top-level)
├── [sheet] microcontrollers.kicad_sch
│   ├── Teensy 4.1 (U1)
│   └── ESP32-S3 Waveshare module (U2)
├── [sheet] audio-codec.kicad_sch
│   └── SGTL5000 (U3) + caps + clock
├── [sheet] filter-ladder.kicad_sch
│   ├── Q1-Q8: 2N3904 matched pairs
│   ├── TL072 x2 (U4, U5)
│   ├── CD4066 (U6)
│   └── TPDT toggle SW1
├── [sheet] power.kicad_sch
│   ├── IP5306 (U7) — carga + boost
│   ├── LiPo connector (J1)
│   └── LC filter (L1 10µH + C1 100µF)
├── [sheet] ui.kicad_sch
│   ├── ALPS EC11 encoders x2 (ENC1, ENC2)
│   ├── Kailh Choc V2 x6 (SW2-SW7)
│   ├── WS2812B x6 keycap (D1-D6)
│   ├── WS2812B x16 ring (D7-D22)
│   └── Action button + volume pot
└── [sheet] connectors.kicad_sch
    ├── TRS 1/4" L/R (J2, J3)
    ├── TRS 3.5mm x2 (J4, J5)
    ├── USB-C (J6)
    ├── USB-A host (J7)
    └── Pogo 6-pin (J8)
```

---

## ERC checklist (antes de layout)

Correr `Inspect → Electrical Rules Checker` y resolver:

- [ ] Nets unconnected: 0 errores (todos los pines conectados o con flag `PWR_FLAG`)
- [ ] Power pins sin source: agregar `PWR_FLAG` en `ANALOG_5V`, `DIGITAL_5V`, `ANALOG_GND`, `DIGITAL_GND`
- [ ] Pines bidireccionales: Teensy I2C/UART declarados como `Bidirectional`
- [ ] Capacitores de desacople presentes: 100nF + 10µF en cada Vcc del SGTL5000 y TL072
- [ ] STAR_GND correctamente tipeado como `Power`

---

## PCB layout: reglas críticas

Derivadas de `03-filter-design.md` §7 y `01-architecture.md` §3.2:

### Separación analog/digital

```
┌─────────────────────────────────────────────┐
│              PCB principal 4-layer           │
│  ┌────────────┐    ┌───────────────────────┐│
│  │ ANALOG ZONE│    │    DIGITAL ZONE       ││
│  │ SGTL5000   │    │  Teensy 4.1           ││
│  │ Filter PCB │    │  ESP32-S3             ││
│  │ TL072 x2   │    │  WS2812B              ││
│  └─────┬──────┘    └──────────────────────┘│
│        │ STAR_GND (punto único)             │
│        └──────────────────────────────────  │
└─────────────────────────────────────────────┘
```

### Reglas de routing

- **Layer stack 4-layer:**
  - Top: signal traces (audio + digital)
  - L2: ground plane (continuo, sin cortes bajo el audio path)
  - L3: power planes (ANALOG_5V y DIGITAL_5V separados)
  - Bottom: digital signals
- **Star ground:** ANALOG_GND y DIGITAL_GND se unen en UN via físico cerca del SGTL5000
- **LC filter:** montar L1 y C1 lo más cerca posible del pin Vcc del SGTL5000
- **Audio traces:** mínimo 0.3mm width, máxima longitud ~50mm antes de buffer
- **Decoupling:** cada opamp TL072 necesita 100nF ceramic a 1mm del pin Vcc
- **Filter sub-PCB:** conectar al main PCB via header 8-pin, no trazas directas
- **WS2812B:** bulk cap 100µF cerca de cada grupo de 4 LEDs

### Clearances

- Audio signal a digital trace: mínimo 0.5mm
- ANALOG_GND a DIGITAL_GND: no conectar (solo en el star point)
- Trimmers de calibración: orientados hacia la apertura del enclosure (acceso desde arriba)

---

## BOM export para JLCPCB

```
File → Fabrication Outputs → BOM

Formato CSV con columnas:
Comment, Designator, Footprint, LCSC Part #
```

Para componentes con LCSC part number:
- SGTL5000: C2796585
- TL072: C7484
- CD4066: C5199
- 2N3904: C20526
- 1nF polystyrene cap: buscar "1nF polystyrene" — puede requerir sourcing externo

Guardar como `apps/hardware/kicad/fab/bom-jlcpcb.csv`.

---

## Checklist pre-fabricación

- [ ] ERC: 0 errores
- [ ] DRC (Design Rule Check): 0 errores con reglas JLCPCB (min trace 0.127mm, min clearance 0.127mm)
- [ ] Gerbers generados: F_Cu, B_Cu, F_Silkscreen, B_Silkscreen, F_Mask, B_Mask, Edge_Cuts
- [ ] Drill file: Excellon format
- [ ] BOM: LCSC part numbers verificados con stock actual en JLCPCB
- [ ] Dimensiones PCB: 180×100mm (main), 30×40mm (filter sub-block)
- [ ] Footprints verificados físicamente contra datasheet de cada componente

---

## Referencias

- `apps/docs/03-filter-design.md` §7 — Layout rules del filter sub-block
- `apps/docs/01-architecture.md` §3 — Hardware spec y BOM completo
- KiCad docs: https://docs.kicad.org
- JLCPCB design rules: https://jlcpcb.com/capabilities/pcb
- SGTL5000 datasheet: NXP document AN3698
