# ESP32-S3 Firmware

Network + UI co-processor del GrooveForge Brain. Maneja display GC9A01, WiFi,
BLE, cloud sync, OTA updates, y actúa como UART slave en el Bridge Protocol.

## Specs relevantes

- `apps/docs/01-architecture.md` §3.3 — Pin Mapping ESP32-S3 (GPIO 43/44 UART, SPI display)
- `apps/docs/01-architecture.md` §4.2 — Tasks del ESP32-S3
- `apps/docs/02-bridge-protocol.md` — ESP32 es el slave del protocolo UART
- `apps/docs/04-ai-architecture.md` §2 — Layer 2 cloud AI (WiFi features)

## Stack

- PlatformIO + Arduino framework (ESP32 board package)
- LVGL v8+ para UI en display GC9A01 240×240 round
- TFT_eSPI como driver del display
- Módulo: Waveshare ESP32-S3-Touch-LCD-1.28

## Build & upload

```bash
cd apps/firmware-esp32
pio run -e esp32s3
pio run -e esp32s3 -t upload
```

## Monitor

```bash
pio device monitor -b 115200
```

## Tests

```bash
pio test -e native
pio test -e esp32s3
```

## Estructura de src/

```
src/
├── main.cpp              # setup() + loop()
├── display/              # LVGL + GC9A01 driver, screens, widgets
├── bridge/               # Bridge Protocol slave (UART @921600)
├── wifi/                 # WiFi manager, mDNS, reconnect
└── cloud/                # GroovePilot API client, OTA
```

## Pin mapping ESP32-S3 (01-architecture.md §3.3)

| GPIO | Función |
|---|---|
| 43 | UART TX → Teensy RX (pin 0) |
| 44 | UART RX ← Teensy TX (pin 1) |
| 7-12 | SPI display GC9A01 (interno al módulo) |
| 18 | Display backlight PWM |
| 13/14 | Touch I2C SDA/SCL (si touch version) |

## Constraints críticos

| Constraint | Valor | Fuente |
|---|---|---|
| WiFi connect | <5s a red conocida | `01-architecture.md` §5.2 |
| Bridge UART | 921600 baud, 8N1 | `02-bridge-protocol.md` §1.3 |
| OTA update | <5 min download+flash+reboot | `01-architecture.md` §5.2 |
| Display splash | <500ms post-boot | `01-architecture.md` §5.1 |

## Agente recomendado

Invocar **Firmware Engineer** para implementación C++/Arduino.
Invocar **Industrial Designer** para diseño de UI en display GC9A01.
