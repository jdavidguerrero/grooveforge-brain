# Consola: lector de superficie en el Pi

> **Fecha:** 2026-11-09. **Estado:** diseño e implementación de prototipo autorizados por Juan; sin hardware. Base: `06-teensy-s-e4.md` §3, `03-arquitectura-capas.md` §4.1 y `07-gui.md` §3. No modifica esos contratos.

## 1. Contrato y eventos

`tools/consola/superficie.py` recibe MIDI USB de S en canal 1 (`channel=0` en mido). `decodificar(msg)` es puro, acepta objetos con atributos MIDI y devuelve una dataclass inmutable o `None`. Ignora otros canales, tipos y números. Índices Python desde cero, incluidos FSR, encoders y manos; pad = fila × 8 + columna.

| Entrada | Evento y campos |
|---|---|
| Notas 36–39 | `Fsr(indice, presionado, velocidad)` |
| Notas 48–79 | `Pad(indice, presionado, velocidad)` |
| Notas 80–107 | `Boton(nombre, presionado)` |
| CC 16–23 | `Encoder(indice, delta)` |
| CC 24–27 | `Fader(indice, valor)` |
| CC 28–29 | `Expresion(indice, valor)` |
| CC 30/31/32 | `Perilla(nombre, valor)`, MASTER/CUE/IN |
| CC 33–34 | `Mano(indice, valor)` |
| Notas 108–109 | `ManoPresencia(indice, presente)` |

Absolutos y velocidades se dividen por 127: rango 0..1, sin unidades ni curvas del motor. Esto evita repetir conversiones MIDI en cada consumidor. Encoders conservan incrementos firmados: 0..63 positivos, 64..127 menos 128; 1 = +1, 127 = −1, cero no produce evento. No se inventa posición absoluta.

Note-on con velocidad positiva representa presión/entrada; note-off o note-on con velocidad cero representa liberación/salida (velocidad normalizada cero). Presencia es un evento de entrada/salida independiente del CC: proximidad cero también puede representar el extremo lejano y no basta para inferir ausencia. No se filtran repeticiones ni se inventa estado al conectar.

### Nombres de botones: tabla única `BOTONES`

| Notas | Nombres, en orden |
|---|---|
| 80–87 | ENCODER_1 … ENCODER_8 |
| 88–93 | MCP1_8 … MCP1_13 |
| 94–101 | PLAY, STOP, REC, LOOP, UNDO, NEW, SHIFT, METRO |
| 102–107 | MCP2_8 … MCP2_13 |

**Pendientes de Juan:** 06 no asigna nombres a las doce funciones adicionales ni publica un nombre USB de S. Juan autoriza estos identificadores físicos (índice lógico de entrada, no pin GPIO MCP) y el filtro predeterminado `Teensy`; las funciones del panel final siguen sin asignar. `BOTONES` permite cambiar todos los nombres en un lugar. `--puerto` cambia el filtro; cero coincidencias o varias producen error para no abrir otra Teensy por accidente.

## 2. Hilos y ciclo de vida

`Superficie` importa mido al abrir, selecciona una entrada por subcadena y registra un callback. Callback → decodificador → `SimpleQueue` segura entre hilos → consumidor. La cola sin límite no espera capacidad; el callback no imprime, no usa serial ni ejecuta motor/GUI. El consumidor debe drenar con frecuencia: si se detiene, crece la memoria. Una cola limitada obligaría a perder liberaciones o bloquear el callback.

`poll(timeout=0)` devuelve un evento o `None`; un timeout positivo espera como máximo ese tiempo. La iteración espera eventos hasta cerrar. `close()` cierra el puerto y encola un marcador de fin para despertar al consumidor; admite context manager. Un lock breve ordena cierre/callback sin trabajo de usuario bajo el lock. Un solo consumidor por instancia; no hay reconexión automática.

## 3. Diagnóstico y uso

`Stats(puerto).consultar()` importa pyserial al usarlo, abre CDC con timeout y envía `@S STATS\n`. Analiza la línea `@S PROTOTYPE` con los campos enteros `loop_max_us`, `fsr_hz`, `scan_gap_max_us`, `ads0_sps`, `ads1_sps`, `events_s`, `i2c_errors`, `drops`. Exige todos los campos y rechaza respuestas incompletas. El formato exacto procede de `prepare_stats()` del firmware. `reset()` envía `@S RESET\n`: reinicia contadores, no estado musical. Serial funciona fuera del callback MIDI.

```sh
cd tools/consola
python superficie.py --listar
python superficie.py --monitor --puerto Teensy
python superficie.py --stats /dev/ttyACM1
uv run pytest
```

mido usa python-rtmidi como backend; ambas dependencias y pyserial se cargan únicamente en operaciones de hardware. Pruebas con mensajes simples y puertos falsos cubren cada entrada, extremos, canal, liberación, orden de cola, cierre y diagnóstico; también corre la regresión del reproductor. No demuestran latencia USB ni funcionamiento físico.

## 4. Límites y diferencias documentales

No cubre motor, GUI, significado de LIVE/MIXER, macros, escala/theremin, mezcla MASTER/CUE/IN, LEDs, bridge-protocol, firmware, calibración ni reconexión. La aplicación decide cómo usar los eventos.

**Diferencias señaladas, sin modificar los documentos:** 03 §2 describe bridge por CDC, mientras 06 §3 define MIDI USB y diagnóstico textual para el prototipo y reserva bridge para el futuro. 03 §4.1 deja dos canales ADS #2 libres, pero 07 §3 y 06 los usan para manos. Esta entrega implementa expresamente el contrato de prototipo de 06 solicitado por Juan, sin redefinir la arquitectura final.

## Fuentes

- [Mapa MIDI, teoría de captura y CDC](06-teensy-s-e4.md), §2–§3.
- [Arquitectura y alcance MVP](03-arquitectura-capas.md), §2 y §4.1.
- [GUI y sensores de mano](07-gui.md), §3.
- [Formato CDC del firmware custom, `prepare_stats()` y `poll_serial()`](../../consola/processor/src/consola_s/main.cpp).
- Juan, conversación de esta entrega (2026-11-09): nombres físicos provisionales y filtro `Teensy` autorizados; funciones finales y nombre USB pendientes.
