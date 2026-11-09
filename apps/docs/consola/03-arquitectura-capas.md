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
