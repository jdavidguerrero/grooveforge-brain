# Skill: PlatformIO Build

Referencia operacional para builds, uploads, monitor y tests con PlatformIO en el
proyecto GrooveForge Brain (Teensy 4.1 + ESP32-S3).

---

## Comandos esenciales

### Teensy 4.1

```bash
cd apps/firmware-teensy

pio run -e teensy41                    # build only
pio run -e teensy41 -t upload          # build + flash (Teensy conectado)
pio run -e teensy41 -t clean           # limpiar build artifacts
pio device monitor -b 115200           # serial monitor
pio device monitor -b 115200 --filter=time  # con timestamp

pio test -e native                     # tests nativos (sin hardware, corre en CI)
pio test -e teensy41                   # tests on-device (Teensy conectado)
pio test -e teensy41 -f "test_bridge*" # filtrar tests por nombre
```

### ESP32-S3

```bash
cd apps/firmware-esp32

pio run -e esp32s3
pio run -e esp32s3 -t upload
pio device monitor -b 115200

pio test -e native
pio test -e esp32s3
```

### Shortcuts útiles

```bash
# Build ambos desde raíz (si configurás Nx targets)
pnpm run build:teensy
pnpm run build:esp32

# Ver puertos disponibles
pio device list

# Info del board
pio boards teensy41
pio boards esp32s3
```

---

## Anatomía del `platformio.ini`

### Teensy 4.1 — `apps/firmware-teensy/platformio.ini`

```ini
[env:teensy41]
platform = teensy
board = teensy41
framework = arduino

lib_deps =
    PaulStoffregen/Audio
    PaulStoffregen/Encoder
    PaulStoffregen/Bounce2
    PaulStoffregen/USBHost_t36

build_flags =
    -DUSB_MIDI_AUDIO_SERIAL    ; composite USB: Audio + MIDI + CDC (CRÍTICO)
    -std=gnu++17               ; C++17

monitor_speed = 115200

; Environment para tests nativos (sin hardware)
[env:native]
platform = native
test_framework = unity
build_flags =
    -std=gnu++17
    -DNATIVE_TEST              ; flag para mockear hardware en tests
```

**`-DUSB_MIDI_AUDIO_SERIAL` es obligatorio** — habilita el USB composite con Audio + MIDI + CDC simultáneo.

### ESP32-S3 — `apps/firmware-esp32/platformio.ini`

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

lib_deps =
    lvgl/lvgl @ ^8.3.0
    Bodmer/TFT_eSPI @ ^2.5.0

build_flags =
    -std=gnu++17
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1  ; habilita CDC serial por USB
    -DCORE_DEBUG_LEVEL=3

upload_speed = 921600
monitor_speed = 115200

[env:native]
platform = native
test_framework = unity
build_flags =
    -std=gnu++17
    -DNATIVE_TEST
```

---

## Agregar dependencias

```ini
; Por nombre en el registry (preferido)
lib_deps =
    PaulStoffregen/Audio
    bodmer/TFT_eSPI @ ^2.5.0

; Por URL de repositorio
lib_deps =
    https://github.com/user/repo.git

; Por path local (para apps/bridge-protocol/include/)
lib_deps =
    symlink://../bridge-protocol
```

Para la librería compartida `bridge-protocol`, usar path local:
```ini
lib_deps =
    symlink://../bridge-protocol/include
```

---

## Debug

### Serial debug básico

```cpp
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);  // esperar hasta 3s
    Serial.println("GrooveForge Brain boot");
}
```

### Debug de audio (CPU y memoria)

```cpp
// En loop(), cada segundo:
if (millis() - last_debug > 1000) {
    Serial.printf("CPU: %.1f%% | Mem: %d blocks peak\n",
        AudioProcessorUsageMax(),
        AudioMemoryUsageMax());
    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();
    last_debug = millis();
}
```

---

## Troubleshooting frecuente

### Teensy no detectado en macOS

```bash
# Verificar que el driver esté instalado
ls /dev/cu.usbmodem*

# Si no aparece: presionar el botón de reset del Teensy
# Si persiste: reinstalar Teensyduino desde pjrc.com/teensy/td_download.html
```

### Error: "USB type not set correctly"

En `build_flags` asegurarse de tener exactamente:
```ini
build_flags = -DUSB_MIDI_AUDIO_SERIAL
```
No mezclar con otros `-DUSB_*` flags.

### ESP32 upload falla

```bash
# Verificar el puerto
pio device list

# Si falla el auto-detect, especificar puerto manualmente
pio run -e esp32s3 -t upload --upload-port /dev/cu.usbserial-XXX

# Si hay error de bootloader: mantener presionado BOOT mientras conectás
```

### Tests nativos no compilan (hardware dependencies)

Usar el flag `-DNATIVE_TEST` en el env native y mockear las dependencias:

```cpp
#ifdef NATIVE_TEST
    // Mock de AudioMemory para tests nativos
    void AudioMemory(int blocks) {}
#else
    #include <Audio.h>
#endif
```

---

## Estructura de archivos esperada

```
apps/firmware-teensy/
├── platformio.ini
├── src/
│   ├── main.cpp              # entry point
│   ├── engines/
│   ├── fx/
│   ├── ml/
│   ├── bridge/
│   ├── ui/
│   └── usb/
├── include/                  # headers del proyecto
├── lib/                      # libs locales (no registry)
└── test/
    ├── test_bridge/
    ├── test_engines/
    └── test_fx/
```

---

## Referencias

- PlatformIO docs: https://docs.platformio.org
- Teensy boards: https://docs.platformio.org/en/latest/boards/teensy/teensy41.html
- ESP32-S3 boards: https://docs.platformio.org/en/latest/platforms/espressif32.html
- `apps/docs/01-architecture.md` §6 — Build instructions del proyecto
