# Push Clone - Monorepo con Integración Ableton Live

Este proyecto implementa un controlador MIDI tipo Push con Teensy 4.1 y NeoTrellis M4, integrado con el [sistema de integración MIDI con Ableton Live](https://github.com/jdavidguerrero/ableton_midi_api_integration).

## 🎯 Integración con Ableton Live

### Comandos MIDI Implementados

Basándose en tu documentación de integración, este hardware implementa:

- **Session View**: Clip launching, stopping, scene management
- **Transport Control**: Play, stop, record, loop
- **Navigation**: Track/scene selection
- **LED Feedback**: Real-time visual updates from Ableton

### Flujo de Datos

1. **NeoTrellis M4** detecta presión en pads (32 pads 4x8)
2. **UART** envía comandos SysEx a Teensy 4.1
3. **Teensy 4.1** convierte a USB MIDI usando tu protocolo
4. **Ableton Live** recibe comandos MIDI via tu Remote Script
5. **Feedback LED** regresa por el mismo camino para actualización visual

## 🏗️ Estructura del Monorepo

```
push_clone_processor/
├── src/
│   ├── teensy/              # Código para Teensy 4.1
│   │   ├── main.cpp         # Programa principal con USB MIDI
│   │   └── Hardware.cpp     # Configuración de hardware
│   └── neotrellis_m4/       # Código para NeoTrellis M4
│       └── main.cpp         # Programa principal del NeoTrellis
├── include/
│   ├── teensy/              # Headers específicos Teensy
│   ├── neotrellis_m4/       # Headers específicos NeoTrellis
│   └── shared/              # Configuración compartida
├── lib/
│   ├── teensy/              # Librerías específicas Teensy
│   │   └── UartHandler/     # Comunicación UART con NeoTrellis
│   └── neotrellis_m4/       # Librerías específicas NeoTrellis
│       ├── NeoTrellisController/ # Control de la matriz de pads
│       └── UartInterface/   # Comunicación UART con Teensy
├── platformio.ini           # Configuración PlatformIO para monorepo
└── PIN_DISTRIBUTION.md      # Documentación de pines
```

## 🔌 Comunicación UART

### Protocolo SysEx Personalizado

- **Delimitadores**: F0 (inicio) y F7 (fin)
- **Formato**: `F0 [COMANDO] [DATOS...] F7`
- **Baud Rate**: 115200
- **Buffer**: 64 bytes

### Comandos UART

- **CMD_CLIP_TRIGGER**: Lanzamiento de clips
- **CMD_LED_CLIP_STATE**: Actualización de estado LED
- **CMD_TRANSPORT**: Comandos de transporte
- **CMD_NAVIGATION**: Navegación de tracks/scenes

## 🎛️ Hardware Support

### NeoTrellis M4 (32 Pads)

- **Grid 4x8**: 32 pads con feedback visual RGB
- **Estados LED**: Empty, Loaded, Playing, Recording, Selected, Triggered
- **Integración**: Comunicación directa con Teensy via UART

### Teensy 4.1

- **USB MIDI**: Comunicación nativa con Ableton Live
- **UART**: Comunicación bidireccional con NeoTrellis
- **I2C**: Soporte para expansión futura

## 🚀 Compilación

### Teensy 4.1
```bash
pio run -e teensy41
```

### NeoTrellis M4
```bash
pio run -e neotrellis_m4
```

### Ambos
```bash
pio run
```

## 🔧 Configuración

### 1. Hardware
- Conectar Teensy 4.1 y NeoTrellis M4 via UART (pines 0 y 1)
- Alimentar ambos dispositivos
- Conectar Teensy 4.1 a PC via USB

### 2. Software
- Instalar tu [Remote Script de Ableton](https://github.com/jdavidguerrero/ableton_midi_api_integration)
- Configurar MIDI en Live Preferences
- Seleccionar PushClone como Control Surface

### 3. MIDI Mapping
- **Input**: Teensy 4.1 USB MIDI
- **Output**: Teensy 4.1 USB MIDI
- **Control Surface**: PushClone

## 📊 Comandos MIDI Implementados

Basándose en tu documentación, este hardware soporta:

- **Transport**: Play, Stop, Record, Loop (0x50-0x5F)
- **Clips**: Trigger, Stop, Delete, Duplicate (0x10-0x1F)
- **Tracks**: Volume, Pan, Mute, Solo (0x20-0x2F)
- **Devices**: Parameters, Banks, Presets (0x40-0x4F)
- **Navigation**: Track/Scene selection (0xB0-0xBF)
- **LED Feedback**: Clip states, Transport states (0x80-0x8F)

## 🔄 Flujo de Integración

### Clip Launching
1. Usuario presiona pad en NeoTrellis
2. NeoTrellis envía `CMD_CLIP_TRIGGER` via UART
3. Teensy recibe y convierte a USB MIDI
4. Ableton recibe comando via tu Remote Script
5. Ableton ejecuta acción y envía feedback
6. Teensy recibe feedback y envía a NeoTrellis
7. NeoTrellis actualiza LED del pad

### Transport Control
1. Comandos de transporte se envían directamente via USB MIDI
2. Teensy procesa y envía a Ableton
3. Feedback visual se maneja localmente

## 🎵 Casos de Uso

### Session View
- **Clip Launching**: Presionar pads para lanzar clips
- **Scene Management**: Navegación entre escenas
- **Visual Feedback**: Estados de clips en tiempo real

### Mixer View
- **Track Control**: Volumen, pan, mute, solo
- **Send Control**: Efectos y routing
- **Crossfader**: Control de mezcla

### Device View
- **Parameter Control**: Encoders para parámetros de dispositivos
- **Rack Management**: Navegación de cadenas de racks
- **Macro Control**: Control de macros de racks

## 🔍 Debugging

### Serial Monitor
- **Teensy**: 115200 baud
- **NeoTrellis**: 115200 baud

### LED Test
- Al iniciar, ambos dispositivos ejecutan test de LEDs
- Pads se iluminan en secuencia
- Estado final: todos los pads en estado "Loaded"

## 🔮 Próximos Pasos

1. **Implementar encoders**: Para control de parámetros de dispositivos
2. **Implementar faders**: Para control de volumen de tracks
3. **Implementar botones capacitivos**: Para transporte y navegación
4. **Expansión I2C**: Para más dispositivos NeoTrellis

## 📚 Referencias

- [Integración MIDI con Ableton Live](https://github.com/jdavidguerrero/ableton_midi_api_integration)
- [Documentación de la API](https://github.com/jdavidguerrero/ableton_midi_api_integration/blob/main/PushClone_API_Documentation.md)
- [Comandos MIDI de Teensy](https://github.com/jdavidguerrero/ableton_midi_api_integration/blob/main/Teensy_MIDI_Commands_Reference.md)

---

**Integrado con ❤️ para el sistema de control MIDI de Ableton Live**
