# Teensy 4.1 Firmware

Audio brain del GrooveForge Brain. Corre bare-metal (sin OS), maneja synth engines,
FX, filter control, UI, Bridge Protocol master y USB composite.

## Specs relevantes

- `apps/docs/01-architecture.md` §3.3 — Pin Mapping (contrato de GPIO, NO cambiar sin consultar)
- `apps/docs/01-architecture.md` §4.1 — Synth engines y polyphony targets
- `apps/docs/02-bridge-protocol.md` — Teensy es el master del protocolo UART/USB-CDC
- `apps/docs/03-filter-design.md` — GPIO 25 → CD4066 bypass, GPIO 27 lee TPDT state
- `apps/docs/05-fx-architecture.md` — 12 FX signature, CPU budget por efecto

## Stack

- PlatformIO + Arduino framework + Teensyduino
- Teensy Audio Library (PaulStoffregen/Audio) para todos los engines y FX
- PaulStoffregen/USBHost_t36 para USB-A host MIDI (Nivel 1)
- USB type: `USB_MIDI_AUDIO_SERIAL` (composite: Audio + MIDI + CDC)

## Build & upload

Durante Fase 1 (sprints de sketches, sin `src/main.cpp` de producción aún):

```bash
cd apps/firmware-teensy
pio run -e sketch               # build del sketch activo
pio run -e sketch -t upload     # build + flash
```

El env `sketch` selecciona el sketch del sprint vía `build_src_filter` en
`platformio.ini`. `pio run -e teensy41` queda reservado para el firmware de
producción (cuando exista `src/main.cpp`).

## Monitor

```bash
pio device monitor -b 115200
```

## Tests

```bash
pio test -e native              # sin hardware (CI)
pio test -e teensy41            # on-device
```

## Estructura de src/

```
src/
├── main.cpp              # setup() + loop() — mínimo posible
├── sketches/             # sketches de prueba por sprint (no van a producción)
├── engines/              # synth engines (moog_model_d.cpp, juno_106.cpp, ...)
├── fx/                   # 12 signature FX
├── ml/                   # TinyML inference (Layer 1)
├── bridge/               # Bridge Protocol master (UART + USB-CDC)
├── ui/                   # encoders, buttons, LEDs WS2812B
└── usb/                  # USB-A host MIDI
```

## Constraints críticos

| Constraint | Valor | Fuente |
|---|---|---|
| Audio latencia | <1ms determinística | `01-architecture.md` §5.2 |
| CPU total | ≤60% en uso normal | `01-architecture.md` §5.2 |
| Engines @ 6 voces | ~30% CPU | `01-architecture.md` §4.1 |
| 5 FX simultáneos | ~45% CPU | `05-fx-architecture.md` §2 |
| AudioMemory total | ~400KB de 1MB RAM | `04-ai-architecture.md` §1.2 |
| TinyML tensor arena | ~200KB RAM | `04-ai-architecture.md` §1.2 |

## Agente recomendado

Invocar **Firmware Engineer** para implementación C++ y **DSP Engineer** para diseño
de algoritmos de audio. Invocar **ML Engineer** para integración de modelos TFLite Micro.
