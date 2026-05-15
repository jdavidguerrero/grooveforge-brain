# Sprint 1.1 — Hello Tone

> **Fase:** 1 — Audio Core
> **Estimado:** 1 sesión (~3h)
> **Status:** 🔴 Pending
> **Refs:** `apps/docs/06-implementation-roadmap.md` §2 Sprint 1.1
> **Demo target:** tono 440Hz audible por jack del Audio Shield (SGTL5000)

---

## Theory

### ¿Qué es I2S y por qué los chips de audio lo usan?

I2S (Inter-IC Sound) es un protocolo serial sincrónico diseñado específicamente para
transferir audio digital entre chips. A diferencia de UART o SPI (que transfieren datos
genéricos), I2S habla el "idioma del audio": entiende canales izquierdo/derecho y
sample rates.

Un bus I2S tiene exactamente 3 señales:

```
BCLK  (Bit Clock)  ─── pulsa una vez por bit de audio transmitido
LRCLK (LR Clock)   ─── cambia cada sample: LOW = canal L, HIGH = canal R
DATA  (Serial Data) ── los bits de audio, uno por pulso de BCLK
```

Para un audio de 24 bits a 48kHz estéreo:
- LRCLK = 48.000 Hz (48k samples/segundo por canal)
- BCLK = 48.000 × 2 canales × 24 bits = **2.304 MHz**

### El problema del MCLK — por qué el Teensy 4.1 gana sobre el Pi

El SGTL5000 (codec de audio del Brain) necesita además un **MCLK (Master Clock)** —
un clock de referencia de alta velocidad del que deriva todas sus frecuencias internas.
El MCLK típico es 256× el LRCLK: para 48kHz → MCLK = **12.288 MHz**.

**El problema con Raspberry Pi:** el Pi 5 usa el chip RP1 para sus periféricos, que
no tiene un generador de MCLK nativo para I2S. Necesita un oscilador externo, y
sincronizar ese oscilador con el sample rate sin drift es complicado (la razón del
pivot v3.0 de este proyecto — ver `apps/docs/00-master-strategy.md` §5.3).

**Teensy 4.1:** el Cortex-M7 del Teensy tiene un generador de clock dedicado para I2S
(`I2S1_MCLK`) en el **pin 23**. La Teensy Audio Library lo configura automáticamente —
cero configuración extra, MCLK perfecto garantizado.

### AudioMemory: el pool de bloques de audio

La Teensy Audio Library no procesa sample por sample. Procesa en **bloques de 128
samples** (un bloque = 256 bytes a 16 bits). Esto reduce el overhead de las llamadas a
funciones y permite que la ISR de audio sea eficiente.

`AudioMemory(N)` reserva N bloques del pool compartido. Cada conexión entre objetos
(`AudioConnection`) usa bloques de este pool. Regla práctica:

```
bloques_necesarios ≈ (número de conexiones activas × 2) + 8 buffer
```

Para este sketch mínimo (1 oscilador → 2 salidas): `AudioMemory(8)` es suficiente.

### Diseño declarativo de la Teensy Audio Library

La Library usa un paradigma declarativo: primero declarás los objetos (osciladores,
filtros, outputs) y las conexiones entre ellos, y la Library construye el grafo de
procesamiento de audio. Es como cablear una modulares física en código:

```
[AudioSynthWaveform] ──── [AudioOutputI2S]
       (generador)              (DAC → SGTL5000)
```

El procesamiento ocurre en una ISR (Interrupt Service Routine) de alta prioridad que
se dispara automáticamente cada 128 samples. Tu `loop()` solo configura parámetros.

### USB composite: Audio + MIDI + CDC simultáneo

El flag `-DUSB_MIDI_AUDIO_SERIAL` en `platformio.ini` configura el Teensy como un
dispositivo USB composite con tres interfaces simultáneas:
- **USB Audio** — el Mac/PC ve al Brain como una interfaz de audio
- **USB MIDI** — el DAW puede enviar/recibir MIDI sin driver extra
- **USB CDC** — Serial monitor para debugging

Sin este flag, el Teensy es solo un serial device y no aparece como interfaz de audio.

### Referencias

- PJRC, "Teensy Audio Library" — https://www.pjrc.com/teensy/td_libs_Audio.html
- NXP, "SGTL5000 Datasheet" (AN3698) — I2S timing diagram, MCLK requirements
- `apps/docs/01-architecture.md` §6.2 — código de referencia hello-tone del OpenSpec

---

## Implementation

### Archivos

| Archivo | Descripción |
|---|---|
| `apps/firmware-teensy/src/sketches/01-hello-tone.cpp` | Sketch principal — tono 440Hz |
| `apps/firmware-teensy/platformio.ini` | Config ya existente con `USB_MIDI_AUDIO_SERIAL` |

### Código

Exactamente el código de referencia de `apps/docs/01-architecture.md` §6.2:

```cpp
// apps/firmware-teensy/src/sketches/01-hello-tone.cpp
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

AudioSynthWaveform    sine;
AudioOutputI2S        audioOut;
AudioControlSGTL5000  codec;

AudioConnection c1(sine, 0, audioOut, 0);  // sine → canal L
AudioConnection c2(sine, 0, audioOut, 1);  // sine → canal R

void setup() {
    AudioMemory(8);           // 8 bloques × 256 bytes = 2KB
    codec.enable();
    codec.volume(0.5);        // 50% — protege los oídos en el primer test
    sine.begin(WAVEFORM_SINE);
    sine.frequency(440.0f);   // La4 — referencia universal de tuning
    sine.amplitude(0.8f);     // 80% del rango dinámico
}

void loop() {
    // Nada — el audio corre en ISR
    // Opcional: reportar CPU/memoria cada segundo para validar
    static uint32_t last = 0;
    if (millis() - last > 1000) {
        Serial.printf("CPU: %.1f%% | Mem: %d blocks\n",
            AudioProcessorUsageMax(),
            AudioMemoryUsageMax());
        AudioProcessorUsageMaxReset();
        AudioMemoryUsageMaxReset();
        last = millis();
    }
}
```

### Decisiones de implementación

- **440Hz (La4):** frecuencia universal de afinación. Si suena, el sistema I2S funciona.
  Fácil de verificar con cualquier app de afinación.
- **`AudioMemory(8)`:** mínimo para 2 conexiones (sine→L + sine→R). Siempre más que el
  mínimo necesario para evitar audio dropouts silenciosos.
- **`codec.volume(0.5)`:** protección auditiva. Ajustar al gusto después de confirmar que
  funciona.
- **WAVEFORM_SINE:** la onda más limpia (sin armónicos). Si hay distorsión, es del sistema,
  no del contenido de audio.

### Pasos para flashear

```bash
cd apps/firmware-teensy
# Compilar
pio run -e teensy41
# Flashear (Teensy conectado via USB-C)
pio run -e teensy41 -t upload
# Monitor serial (otra terminal)
pio device monitor -b 115200
```

---

## Demo

### Qué valida este demo

Que el Teensy 4.1 puede generar audio digital, enviarlo via I2S nativo al SGTL5000,
y que el SGTL5000 lo convierte a señal analógica audible. Es el primer "audio path" end-to-end.

Si suena → I2S funciona, MCLK está bien, codec inicializa correctamente.
Si no suena → revisar conexiones físicas del Audio Shield al Teensy.

### Cómo reproducirlo

```bash
cd apps/firmware-teensy
pio run -e teensy41 -t upload
# Conectar headphones al jack 3.5mm del Audio Shield
# Esperado: tono continuo de 440Hz
# En el serial monitor: "CPU: ~1.5% | Mem: 2 blocks"
```

### Evidencia a capturar

- [ ] Grabación de audio de 5-10 segundos:
  ```bash
  # macOS — usar QuickTime → Nueva grabación de audio → seleccionar "Teensy Audio"
  # Guardar como: apps/docs/sprints/demos/01-hello-tone-demo.wav
  ```
- [ ] Screenshot del serial monitor mostrando CPU% y memoria

### Criterios de pass

- [ ] Tono 440Hz audible en headphones sin distorsión
- [ ] CPU < 5% (baseline sin engines ni FX)
- [ ] `AudioMemoryUsageMax()` ≤ 4 blocks
- [ ] El dispositivo aparece como "Teensy Audio" en las preferencias de sonido del Mac

---

## Tests

Para este sprint, el test principal es el demo audible. Tests unitarios formales
arrancan en Sprint 1.2 (hay más lógica que testear).

```bash
# Build limpio para verificar que compila sin warnings
cd apps/firmware-teensy
pio run -e teensy41 2>&1 | grep -E "warning:|error:"
```

---

## Learnings

> Completar después de la implementación.

### Qué salió diferente al plan

[pendiente]

### Qué tomaría diferente

[pendiente]

### Tiempo real vs estimado

- Estimado: ~3h
- Real: [pendiente]

---

*Siguiente sprint: [02-multi-osc-adsr.md](02-multi-osc-adsr.md) — Multi-OSC + ADSR envelope*
