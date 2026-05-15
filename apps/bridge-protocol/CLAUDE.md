# Bridge Protocol

Headers C compartidos entre firmware-teensy y firmware-esp32 que definen el protocolo
binario de comunicación. También aplica al canal USB-CDC (Teensy ↔ VST3).

## Specs relevantes

- `apps/docs/02-bridge-protocol.md` — especificación completa del protocolo (SSoT)

**Este directorio SOLO contiene headers derivados de ese spec. Si hay conflicto entre
el código y el spec, el spec gana.**

## Stack

- C headers (`.h`) — sin dependencias externas
- Compatible con Teensy 4.1 (ARM Cortex-M7) y ESP32-S3 (Xtensa LX7)
- Incluir desde cada firmware via `lib_deps = symlink://../bridge-protocol/include`

## Estructura

```
include/
└── protocol.h          # Frame struct, CMDs enum, CRC8 function, BridgeMaster/BridgeSlave

test/
└── test_protocol/
    ├── test_crc8.cpp           # CRC-8 test vectors conocidos
    ├── test_serialization.cpp  # Frame roundtrip todos los CMDs
    └── test_error_handling.cpp # NACK, timeout, SEQ rollover
```

## Frame format (02-bridge-protocol.md §1.1)

```c
typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t seq;
    uint8_t payload[256];
    uint8_t crc8;
} BridgeFrame;
```

- Little-endian, CRC-8 poly `0x07`
- UART: 921600 baud, 8N1
- Teensy es master, ESP32 es slave

## Tests

Los tests de este directorio se incluyen en ambas suites de firmware:

```bash
# Desde firmware-teensy
pio test -e native -f "test_protocol*"

# Desde firmware-esp32
pio test -e native -f "test_protocol*"
```

## Agente recomendado

Invocar **Firmware Engineer** para implementación del protocolo.
Consultar **02-bridge-protocol.md** antes de cualquier cambio — el frame format
es un contrato entre Teensy, ESP32 y el VST3 plugin.
