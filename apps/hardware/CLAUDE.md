# Hardware

Archivos KiCad para el PCB del GrooveForge Brain: schematic, layout, fabricación.

## Specs relevantes

- `apps/docs/03-filter-design.md` — topología del filter, BOM, matching procedure,
  calibración 5-pasos, layout rules (§7)
- `apps/docs/01-architecture.md` §3 — Hardware Spec completo (BOM, power arch, pin mapping)

## Stack

- KiCad 8+ (schematic + PCB layout)
- JLCPCB para fabricación (gerbers + BOM con LCSC part numbers)
- CNC local Bogotá para aluminum top panel

## Cómo abrir

```bash
cd apps/hardware/kicad
# Abrir grooveforge-brain.kicad_pro en KiCad
```

## Estructura

```
kicad/
├── grooveforge-brain.kicad_pro
├── grooveforge-brain.kicad_sch     # top-level schematic
├── sheets/                         # hierarchical sheets
│   ├── audio-codec.kicad_sch
│   ├── filter-ladder.kicad_sch     # ← el más crítico
│   ├── microcontrollers.kicad_sch
│   ├── power.kicad_sch
│   ├── ui.kicad_sch
│   └── connectors.kicad_sch
├── grooveforge-brain.kicad_pcb
└── fab/
    ├── gerbers/
    ├── drill/
    └── bom-jlcpcb.csv
```

## PCB specs

- Main PCB: 4-layer, 180×100mm
- Filter sub-block: 2-layer, 30×40mm
- Stack: Top (signals) / GND plane / Power planes / Bottom (digital)

## Constraints críticos del filter

| Spec | Valor | Fuente |
|---|---|---|
| Caps de timing | 1nF polystyrene (NO ceramic) | `03-filter-design.md` §3.2 |
| Star ground | Un único via ANALOG_GND ↔ DIGITAL_GND | `03-filter-design.md` §7.2 |
| LC filter | 10µH + 100µF entre digital 5V y audio rail | `01-architecture.md` §3.2 |
| Decoupling TL072 | 100nF ceramic + 10µF electrolytic en cada Vcc | `03-filter-design.md` §7.2 |

## Agente recomendado

Invocar **Hardware Engineer** para schematic capture, layout, BOM review y calibración.
Consultar `apps/.claude/skills/kicad-schematic/SKILL.md` para convenciones de nets y ERC.
