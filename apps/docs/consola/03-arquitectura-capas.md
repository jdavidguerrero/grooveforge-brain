# Consola: arquitectura por capas, protocolo y sistema operativo del Pi

> **Fecha:** 2026-11-09. **Estado:** propuesta para refinar con Juan. **Base:** `00-concepto.md`, `02-inventario-groovepush.md`. Decisiones de Juan del 2026-11-09: **NeoTrellis M4** (con su propio micro) y **dos Teensy**. **No modifica** `00–06`.

## 1. Separación de funciones

| Capa | Procesador | Trabajo | Si falla la capa de arriba |
|---|---|---|---|
| Pads | **NeoTrellis M4** | 32 pads y sus LEDs, animaciones locales | Los pads siguen respondiendo |
| Superficie | **Teensy S** (portadora + PCB-S) | Encoders, faders, FSR, botones Choc y sus LEDs; junta todo | Sigue mandando MIDI a Live: la consola funciona como controlador aunque el Pi reinicie |
| Sonido | **Teensy A** (portadora + Audio Shield) | Synths del Brain, FX, CV; cue y entrada por el SGTL5000 | El sonido no se corta aunque se congele la pantalla |
| Cerebro | **Raspberry Pi 5** | Motor, reloj y secuenciador, sampler, mezcla, Link, registro, GUI Qt | — |

**Principio:** nada del tiempo real depende de Linux. El Pi decide; las capas de abajo ejecutan.

**Quién lleva el tiempo:** el reloj maestro vive en el callback de audio del Pi (lo más estable que tiene Linux). El Pi manda a la Teensy A las notas **con unos milisegundos de anticipación y su marca de tiempo**, para que samples y synths suenen juntos aunque el USB tarde. El motor trabaja por frases, así que siempre sabe lo que viene.

## 2. Protocolos (Juan, 2026-11-09: «a nivel rendimiento es lo que pesa»)

| Enlace | Protocolo | Por qué |
|---|---|---|
| Teensy S → **Live** | MIDI estándar (notas, CC) + **SysEx** con el Remote Script PushClone | Ya funciona; el SysEx es lo que hace transparente el control de Live desde el Push. No se toca |
| Teensy S y Teensy A ↔ **Pi** | **`bridge-protocol`** (tramas binarias con CRC-8, `02-bridge-protocol.md`) por **USB serie (CDC)** | Rendimiento: el SysEx codifica a 7 bits y viaja en paquetes USB-MIDI de 3 bytes útiles, y PJRC advierte que muchas tramas SysEx seguidas saturan el software del host. El CDC lleva binario puro sobre el USB de alta velocidad del Teensy. El protocolo ya está especificado y con tests |
| Teensy A ↔ Pi (audio) | USB audio (2 canales por sentido) | Ya activo en el firmware (`USB_MIDI_AUDIO_SERIAL`) |
| M4 ↔ Teensy S | UART (como en GroovePush) | Ya cableado en la PCB-S |

**A medir en el prototipo:** latencia y throughput de `bridge-protocol` por CDC frente a SysEx, con el mismo mensaje.

**Contradicción a resolver con Juan:** `02-bridge-protocol.md` define el enlace como UART a 921 600 baudios entre Teensy y ESP32. Usar las mismas tramas por CDC hacia el Pi es otro transporte del mismo protocolo; hace falta su visto bueno para documentarlo así (es spec SSoT).

## 3. Sistema operativo del Pi

Como el tiempo real duro está en las Teensy y el Pi planifica por adelantado, **el Pi necesita tiempo real «blando»**: que su callback de audio no se corte. Opciones:

| Opción | Estado para Pi 5 | Veredicto |
|---|---|---|
| **Raspberry Pi OS 64 bits + ajustes** (un núcleo reservado para el audio con `isolcpus`, gobernador `performance`, `threadirqs`, prioridad tiempo real en PipeWire o JACK) | Estable, mantenido; es lo que usa Zynthian (base Raspberry Pi OS) | **Empezar aquí** |
| La misma + **kernel PREEMPT_RT** | PREEMPT_RT entró al kernel principal en la versión 6.12, que también trae soporte inicial del Pi 5. Pero los kernels de la distribución no lo traen activado y usuarios del Pi 5 reportan tener que compilarlo y parcharlo | **Solo si E2 muestra cortes** con el kernel normal |
| **Patchbox OS** (Blokas, kernel RT) | Beta de 2024 que funciona en Pi 5 con problemas conocidos; mantenimiento incierto | No |
| **Elk Audio OS** (latencia bajo 1 ms) | La versión abierta solo soporta Pi 4 | No, por ahora |
| **Yocto o Buildroot** (imagen a medida) | Posible con capas para Raspberry Pi | **Más adelante**, cuando el diseño se congele: arranque rápido, imagen mínima y reproducible. Costo: curva de aprendizaje y compilaciones largas, malo para iterar con 10–18 h por semana |

**Algo de Yocto sin Yocto:** Raspberry Pi OS puede montar el sistema **de solo lectura** (overlay). En el escenario importa: si se corta la luz, la tarjeta no se corrompe.

**Prueba (se suma a E0 y E2 del prototipo):** `cyclictest` con kernel normal y con PREEMPT_RT, y 30 minutos de audio con `pw-top` en ambos. Se cambia de kernel solo si los números lo piden.

## Fuentes

- PREEMPT_RT y Pi 5: [It's FOSS, Linux 6.12](https://itsfoss.com/news/linux-kernel-6-12/), [foro LinuxCNC, Pi 5 con 6.12 RT](https://forum.linuxcnc.org/9-installing-linuxcnc/54969-rpi-5-with-6-12-preempt-rt-using-posix-non-realtime), [TME, tiempo real en Raspberry Pi](https://www.tme.eu/en/news/library-articles/faq/page/76024/does-the-raspberry-pi-support-real-time-deterministic-operations/).
- Elk Audio OS: [hardware soportado](https://elk-audio.github.io/elk-docs/html/intro/supported_hw.html), [versiones de elk-pi](https://www.github.com/elk-audio/elk-pi/releases).
- Patchbox OS: [beta Bookworm ARM64 2024](https://community.blokas.io/t/beta-patchbox-os-bookworm-arm64-2024-04-04/5163), [kernel RT](https://community.blokas.io/t/real-time-kernel/1134).
- Teensy por USB: [PJRC, USB MIDI](https://www.pjrc.com/teensy/td_midi.html).

## 4. Decisiones de Juan (2026-11-09) y alcance del MVP

**Decidido:**
- **Conexión con el Mac por Ethernet con cable** (aislada por transformador, misma red para Link, motor y MIDI por red). Wi-Fi no se usa para tocar. Un USB directo trasero solo si una medición lo pide.
- **Dos Teensy con firmware separado**: A (audio) y S (superficie e I/O). Se **rediseña la PCB** para tener menos placas: una placa principal con las dos Teensy reemplaza a las dos portadoras, la PCB-S y la PCB-A. Las placas de botones y faders de GroovePush se mantienen.
- **Enlace directo A ↔ S** por UART con `bridge-protocol` (tercer transporte del mismo protocolo), tierra común y **un pin de sincronía reservado**. Permite pad → sonido en menos de 1 ms y seguir sonando si el Pi se reinicia.
- **Construir en orden: primero un MVP**, con la placa preparada para crecer (conectores y huellas reservadas, sin poblar).

### 4.1 MVP (versión 1)

| Bloque | Incluye |
|---|---|
| Pi 5 | Motor, secuenciador, sampler, **mezcla**, GUI Qt 7", Ethernet |
| Audio | Audio+ V2 (sala); SGTL5000 en la Teensy A (cue y entrada de línea) |
| Teensy A | Synths y FX del Brain; USB al Pi; enlace con S |
| Teensy S | 8 encoders; **ADS1115 #1**: 4 faders; **ADS1115 #2**: crossfader + 2 FSR de expresión + 1 libre; ADC propio (rápido): 4 FSR de velocity + perillas **MASTER** (sala, volumen digital en el Pi: el Audio+ no tiene volumen propio), **CUE** (amplificador de audífonos del SGTL5000, vía el enlace con A; Shift + CUE = mezcla de cue, cuánto de la sala se oye en audífonos) e **IN** (ganancia de la entrada de línea del SGTL5000, como el pot IN del Brain Studio); NeoTrellis M4, botones Choc (MCP23017), LEDs al 20–25 %; USB al Pi; enlace con A |
| Modos | LIVE y **MIXER básico** (volumen por grupo, cue por canal, crossfader). HÍBRIDO y ESTUDIO después |

**Alcance del mixer (Juan, 2026-11-09):** el MVP es un **mixer interno**: mezcla los canales que genera la consola dentro del Pi, sin hardware extra. Mezclar **CDJs o Traktor** (cuyo audio llega analógico desde la tarjeta del laptop) pide **una entrada estéreo por deck**; el SGTL5000 tiene una sola entrada estéreo de línea. Esas entradas van en la v2 con un códec multicanal (CS42448: 6 entradas, 8 salidas) en el conector SAI2 reservado. Un mixer de DJ para fuentes externas (ruido bajo, margen, entradas balanceadas) es casi un proyecto propio: la idea del mixer estilo Xone de Juan.

**Prototipo con lo que hay:** las 2 placas MCP23017 del Push; crossfader con cualquier potenciómetro deslizante lineal B10K (45 o 60 mm) o uno de los 4 faders Alps (B = curva lineal; el crossfader definitivo se elige para la caja); CV con **MCP4725** (1 salida de 12 bits, hasta 2 en el mismo I2C: cutoff y resonancia) en lugar del MCP4728 de 4 salidas. Marimba y e-drum quedan para después.

### 4.2 Preparado en la placa, sin poblar (para versiones siguientes)

| Ampliación | Qué se deja en la placa | Teensy |
|---|---|---|
| **CV/Gate** (MCP4728: 4 CV + 2 gates) para el filtro analógico y synths externos | Huella del MCP4728 en el I2C, huellas del amplificador y protección por salida, conector a una placa de jacks | A |
| **Mixer completo**: ecualizador aislador de 3 bandas + filtro de una perilla por canal, envíos, bancos | Solo firmware: usa los encoders y faders del MVP | — |
| **4 entradas de trigger** (piezo, zona de pad y aro) para e-drum y **la marimba** | Conector con 8 pines analógicos de S + huellas del acondicionamiento (resistencia, diodos, pico) | S |
| **MIDI DIN in/out + clock + sync analógico** | Huellas del optoacoplador de entrada y de los jacks TRS | S |
| **Segundo códec o códec multicanal** (segundo SGTL5000 o CS42448): todo el audio en la Teensy A, con un solo reloj, sin el Audio+ | El segundo puerto I2S (SAI2) de A llevado a un conector. Se decide después de E3 (clics del cue con dos relojes). Dos SGTL5000 en una Teensy piden otra dirección I2C para uno | A |
| **Pedal** (aceptar o saltar) | Un jack TRS a un pin libre de S | S |
| **Pad de entrada** 0 / −10 / −20 dB (como el del Brain Studio, ADR-004) | Huellas del atenuador y del interruptor antes de la entrada de línea; lectura en un pin analógico | A (lectura en S) |
| **Línea de 5 V propia para LEDs** | Puente y pads para separar la alimentación de los LEDs | — |

**Regla para el diseño de la placa:** cada ampliación tiene sus **pines reservados en el mapa** desde la versión 1, aunque no se usen. Nada de la versión 1 puede ocupar un pin reservado.

**Siguiente paso de diseño** (después de E3 y E4, con mediciones): el mapa de pines de A y S con las reservas, y el esquema de la placa principal.
