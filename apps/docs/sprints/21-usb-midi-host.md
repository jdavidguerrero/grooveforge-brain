# Sprint 4.1 — USB-A Host MIDI Input

**Status:** Implementado — pendiente demo on-device
**Fecha:** Mayo 18, 2026
**Nivel de superpoderes:** Nivel 1 — "+ Teclado MIDI (USB-A host)"

---

## Theory

### 1. USB host vs device — dos controladores distintos en silicio

El Teensy 4.1 (SoC IMXRT1062) tiene dos controladores USB en hardware independiente:

- **USB device (UOTG0):** conectado al conector micro-USB (USB-C en el PCB de producción). 
  Responde a un host externo — en este caso el DAW o computadora. Este bus está configurado 
  como composite device (`USB_MIDI_AUDIO_SERIAL`): Audio Class + MIDI Class + CDC Serial.

- **USB host (UOTG1):** conectado a pads dedicados D+/D− en la cara inferior del Teensy 4.1.
  Puede alimentar y enumerar dispositivos externos. Los pads se conectan al conector USB-A del PCB.
  El pin 39 del Teensy funciona como "USB host mode select" — activado automáticamente por 
  `USBHost_t36` al llamar `myusb.begin()` (`01-architecture.md §3.3`).

Por qué esto importa: no hay conflicto de recursos entre el host y el device. Pueden operar
simultáneamente de forma independiente. El Teensy puede recibir MIDI de un teclado USB-A
mientras transmite audio USB al DAW, todo al mismo tiempo.

**Referencia:** IMXRT1062 Reference Manual, §42 (USB Controller).

### 2. USB MIDI clase-compliant

USB-IF define la USB Audio Device Class v1.0 que incluye la subclase MIDI Streaming (MS).
Un dispositivo "clase-compliant" implementa esta especificación sin requerir driver propietario
— el OS y `USBHost_t36` ya conocen el protocolo.

**Estructura del USB MIDI Event Packet (4 bytes):**

```
Byte 0: [Cable Number (3:7)] [Code Index Number CIN (0:3)]
Byte 1: MIDI status byte (ej. 0x90 = NoteOn ch1)
Byte 2: MIDI data byte 1 (note number 0-127)
Byte 3: MIDI data byte 2 (velocity 0-127)
```

**Por qué CIN (Code Index Number):** permite multiplexar hasta 16 cables MIDI virtuales 
sobre un único bulk endpoint. Un teclado típico usa cable 0. Un pedalboard MIDI puede 
usar cable 0 y cable 1 simultáneamente sobre la misma conexión física USB.

`USBHost_t36` parsea estos packets internamente y expone una API de alto nivel
(`getType()`, `getChannel()`, `getData1()`, `getData2()`). El `MidiHost` no necesita
parsear el nivel de paquetes USB raw.

**Referencia:** Universal Serial Bus Device Class Definition for MIDI Devices v1.0 (USB-IF, 1999).

### 3. Estructura de mensajes MIDI

Los tres tipos de mensaje usados en este sprint:

| Mensaje | Status Byte | Byte 1 | Byte 2 |
|---|---|---|---|
| Note On | 0x9n (n=channel) | note 0-127 | velocity 0-127 |
| Note Off | 0x8n | note 0-127 | velocity 0-127 |
| Control Change | 0xBn | CC number | value 0-127 |

**Running status y velocity 0:** El Standard MIDI 1.0 Specification §2.1 define que
un mensaje NoteOn con velocity=0 debe interpretarse como NoteOff. Esta optimización 
permite omitir el byte de status cuando el tipo no cambia, reduciendo el ancho de banda.
El `MidiHost` despacha el NoteOn con velocity=0 al callback `on_note_on` sin modificarlo —
el caller es responsable de tratar velocity=0 como NoteOff si corresponde. El sketch 21
implementa esta lógica en `on_note_on_cb()`.

**CC 74 — Brightness/Cutoff (GM2):** El General MIDI 2 §7.7 designa el Controller 74
como "Brightness" — históricamente mapeado a cutoff de filtro. Es el CC más utilizado
por teclados y DAWs para controlar el carácter tonal.

**CC 71 — Timbre/Harmonic Intensity (GM2):** El Controller 71 mapea a resonancia del filtro.
Menos común que CC 74 pero parte del estándar GM2.

**CC 0 — Bank Select (GM §5.4):** Selección de banco de presets. En el sketch de prueba
se reinterpreta como selector de engine activo (no es el comportamiento de producción).

**Referencia:** MIDI 1.0 Detailed Specification v4.2.1 (MIDI Manufacturers Association, 1996).
General MIDI 2 Specification v1.2 (MMA, 1999).

### 4. `USBHost_t36` task loop — por qué no puede estar en una ISR

`USBHost_t36` usa un modelo de polling cooperativo. `myusb.Task()` avanza la state machine
del bus USB:

1. Verifica el registro de estado del controlador EHCI (IMXRT1062 USB2)
2. Maneja transfers completados (bulk IN para MIDI)
3. Despacha eventos de conexión/desconexión (enumeration, detach)
4. Llama callbacks de usuario (los registered por los `USBDriver` derivados como `MIDIDevice`)

**Por qué no puede vivir en una ISR:** internamente hace allocations de memoria dinámica
(para los transfer descriptors del EHCI) y llama callbacks de usuario arbitrarios que 
pueden tener latencia variable. Ambas operaciones son incompatibles con un contexto de 
interrupción en bare-metal.

`poll()` debe llamarse en cada iteración de `loop()`. En el peor caso (burst de mensajes MIDI
tras presionar múltiples teclas simultáneamente), procesa todos los mensajes pendientes
en el buffer antes de retornar — esto mantiene la latencia baja sin bloquear el audio ISR.

**Referencia:** PaulStoffregen, USBHost_t36 source — `ehci.cpp`, función `USBHS_IRQHandler()`.

### 5. Latencia USB MIDI → audio: análisis de la cadena

El target del spec es <5ms p95 (`01-architecture.md §5.2`). La cadena completa:

```
Teclado → USB packet (bulk IN)
  ↓  ~1ms   — polling interval del EHCI host controller (1ms microframe)
myusb.Task() — procesa transfer completado
  ↓  <0.5ms — overhead de USBHost_t36
midi1.read() → on_note_on_cb() → engine.noteOn()
  ↓  <0.5ms — callback + cálculo de frecuencia
AudioStream ISR — genera el sample en el próximo bloque (128 samples @ 44100Hz)
  ↓  2.9ms  — tamaño del audio block: 128/44100 = 2.902ms
SGTL5000 DAC → analog out
─────────────────
Total teórico: ~5ms worst case
```

El cuello de botella es el audio block size (128 samples = 2.9ms). Reducir a 64 samples
bajaría la latencia a ~3ms pero duplicaría el riesgo de xruns. Con 128 samples el budget
de CPU por bloque es 2.9ms — suficiente para todos los engines y FX del spec.

**Referencia:** `01-architecture.md §5.2` — target <5ms p95.

---

## Implementación

### Archivos creados

**`apps/firmware-teensy/src/usb/midi_host.h`**

Clase `MidiHost` que envuelve `USBHost + MIDIDevice` de `USBHost_t36`:
- Interfaz de callbacks: `on_note_on()`, `on_note_off()`, `on_cc()`, `on_pitch_bend()`
- `init()`: activa el controlador USB host
- `poll()`: non-blocking, consume todos los mensajes MIDI pendientes
- `is_connected()`: detecta presencia de dispositivo enumerado
- `device_name()`: retorna el product string del descriptor USB

Nota de API: `is_connected()` y `device_name()` no son `const` porque `USBDriver::operator bool()`
y `USBDriver::product()` no son métodos const en `USBHost_t36` v0.2. El estado de conexión
puede mutar en cualquier call — se justifica que no sean `const`.

**`apps/firmware-teensy/src/usb/midi_host.cpp`**

Implementación de `MidiHost`:
- Detección de connect/disconnect por comparación de estado previo con `_was_connected`
- Pitch bend: conversión de 14-bit unsigned (0x0000–0x3FFF, centro en 0x2000) a signed int16_t
  (-8192 a +8191) con `((data2 << 7) | data1) - 8192`
- Loop `while (_midi.read())` en `poll()` para agotar el buffer completo en cada tick

**`apps/firmware-teensy/src/sketches/21-usb-midi-host.cpp`**

Sketch de prueba con selección de engine en tiempo de compilación via `#define ACTIVE_ENGINE`:
- `ACTIVE_ENGINE 0` = MoogModelD (default)
- `ACTIVE_ENGINE 1` = Juno106
- `ACTIVE_ENGINE 2` = Prophet5

Manejo de diferencias de interfaz entre engines:
- `MoogModelD::noteOff()` y `Juno106::noteOff()` — sin parámetro (monofónicos)
- `Prophet5::noteOff(uint8_t midi_note)` — con nota (polifónico, voice allocation interna)

Mapeo de CCs:
- CC 74 → cutoff en escala logarítmica: `f = 20 * 1000^(value/127)` Hz
  Justificación: la percepción de frecuencia es logarítmica (igual-loudness contours).
  Una escala lineal concentraría el 90% del rango perceptual en el 10% final del pot.
- CC 71 → resonance: Q = 0.7 + (value/127) * 7.3 (rango 0.7–8.0)

### Ajuste a `platformio.ini`

`build_src_filter` del env `sketch` actualizado a:
```
+<sketches/21-usb-midi-host.cpp> +<usb/midi_host.cpp> +<engines/moog_model_d.cpp>
```

Para probar Juno o Prophet: modificar el filter para incluir el `.cpp` del engine correspondiente
y cambiar `ACTIVE_ENGINE`.

### Resultado del build

```
Flash:  40968 bytes code + 9900 data (de 8MB)
RAM1:   21184 variables + 37704 code (de 512KB)
RAM2:   23904 variables (de 524KB)
CPU budget disponible: amplio — USBHost_t36 no agrega carga al audio path
```

`USBHost_t36` no requiere entrada en `lib_deps` — está incluida en el bundle de
Teensyduino y el LDF la resuelve via `#include <USBHost_t36.h>`.

---

## Demo criteria

Para validar el sprint en hardware:

- [ ] Enchufar teclado MIDI USB al conector USB-A del Teensy 4.1
- [ ] Serial muestra `[MidiHost] device connected: <nombre>` o `(name not available)`
- [ ] Tocar nota en el teclado → audio sale por SGTL5000 (headphones o 1/4" out)
- [ ] Soltar nota → release del envelope audible
- [ ] CC 74 desde 0 a 127 → cutoff sube de 20Hz a 20kHz audiblemente
- [ ] CC 71 desde 0 a 127 → resonance sube de Q 0.7 a Q 8.0 audiblemente
- [ ] NoteOn con velocity 0 desde el teclado → trata como NoteOff (sin nota colgada)
- [ ] Desenchufar teclado → Serial muestra `[MidiHost] device disconnected`
- [ ] Reconectar teclado → funciona sin reset del Teensy

---

## Learnings

_Completar post-demo on-device._

**API gotcha — USBHost_t36 `const`:** `MIDIDevice` hereda `operator bool()` y `product()` de
`USBDriver` donde ambos son métodos no-const. Intentar hacer `is_connected()` o `device_name()`
como métodos `const` en la clase wrapper genera error de compilación. Diseño correcto:
exponer como no-const y documentar el motivo.

**`product()` retorna `const uint8_t*`:** no `const char*`. El cast correcto es
`reinterpret_cast<const char*>()` — ambos son byte arrays, el contenido del descriptor USB
es UTF-8/ASCII, así que el reinterpret es seguro para imprimir pero no para operaciones de
string que asuman null-termination (el descriptor USB puede tener padding con 0x00 al final).

**Un AudioOutputI2S por programa:** el patrón de selección de engine via `#define ACTIVE_ENGINE`
en lugar de múltiples instancias en runtime es la solución correcta para sketches de prueba.
El firmware de producción usa un grafo de audio unificado con routing configurable — ese diseño
es Sprint 5.x (firmware main.cpp).

---

## Refs

- `apps/docs/01-architecture.md §3.3` — pin mapping USB-A host
- `apps/docs/01-architecture.md §1` — Nivel 1 superpoderes
- `apps/docs/01-architecture.md §5.2` — latencia target <5ms p95
- IMXRT1062 Reference Manual §42 — USB Controller (NXP)
- PaulStoffregen/USBHost_t36 — github.com/PaulStoffregen/USBHost_t36
- USB Device Class Definition for MIDI Devices v1.0 — USB-IF, 1999
- Standard MIDI 1.0 Specification v4.2.1 — MMA, 1996
- General MIDI 2 Specification v1.2 — MMA, 1999
