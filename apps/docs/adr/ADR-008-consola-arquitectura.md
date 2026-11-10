# ADR-008: Arquitectura de la GrooveForge Console — synths en el Pi, una sola Teensy, USB audio asíncrono

**Status:** Accepted (Juan, 2026-11-10)
**Date:** 2026-11-10
**Deciders:** Juan Guerrero (owner)
**Relacionados:** ADR-005 (DX7 de 4 operadores, enmendado aquí), ADR-006 (gemelo), ADR-007 (GUI QML + PySide6),
`consola/03-arquitectura-capas.md`, `consola/auditoria/auditoria-fable-2026-11.md`, mediciones
`e3-usb-async-20261109.md` y `gemelo-pi5-20261110.md`.
**Afecta (sin modificar SSoT):** `00-master-strategy.md:174` (el audio en Linux del Pi «no era el camino»: aquel
intento era el SGTL5000 con MCLK desde el Pi; aquí la sala usa un PCM5102A y está medida), `01-architecture.md` §4.1
(DX7 de 6 operadores como modo), `03-arquitectura-capas.md` (capas, dos Teensy, protocolos) y `06-teensy-s-e4.md`.

---

## Context

La consola (producto personal de Juan: producir más rápido y tocar sets de música generada) se pensó con dos Teensy:
**A** para los synths del Brain y **S** para la superficie (`03` §4). Tres mediciones del 2026-11-09/10 cambian el
cuadro:

1. **El Brain toca un sonido a la vez** (ADR-006, auditoría G10) y el set pide BASS, CHORDS, LEAD y PAD juntos.
2. **El gemelo del Brain corre en el Pi 5 con margen:** cuatro partes con FX en **~21 % de un núcleo** (p99 22 %,
   máx. 24 %, 0 bloques fuera de plazo); ningún engine pasa del 10 % (`gemelo-pi5-20261110.md`).
3. **El USB audio de la Teensy quedó resuelto** con la biblioteca asíncrona de alex6679 (entorno `consola_usb`):
   **0 saltos en 60 min en cada sentido**; la deriva real entre relojes es ~56 ppm y se absorbe sin tirar muestras
   (`e3-usb-async-20261109.md`).

Además: los reinicios espontáneos del Pi del 2026-11-09 fueron **por alimentación** (Juan, 2026-11-10), y Juan
eligió **NVMe de 128 GB en un HAT** como almacenamiento.

## Decision

1. **Los synths del set corren en el Pi** con el gemelo del Brain (mismo código DSP que el VST y el firmware), en un
   proceso de audio en C++ sin interfaz (`gf-synth-host`): cuatro partes (BASS, CHORDS, LEAD, PAD), un canal MIDI por
   parte, cliente de PipeWire en el grafo del Audio+. Nada de Python en la ruta de audio (auditoría G18).
2. **Una sola Teensy** con tres trabajos: **superficie** (lo que hoy es la S), **audio I/O** con el SGTL5000 (cue en
   audífonos y entrada de línea, USB asíncrono de 4 canales por sentido) y **CV** (filtro 2N3904 y synths externos).
   Puede tocar **una parte del Brain en vivo** con su filtro analógico si sobra CPU. Reemplaza la decisión de dos
   Teensy de `03` §4. Requiere un mapa de pines nuevo (I2S1 7/8/20/21/23 e I2C 18/19 del SGTL5000 chocan con el
   Serial5 de la M4 y la perilla MASTER de la S).
3. **Audio:** sala por el Audio+ (PCM5102A, reloj principal del grafo de PipeWire a 48 kHz); cue y entrada por el
   SGTL5000 de la Teensy; USB asíncrono (`consola_usb`) como camino único Teensy ↔ Pi. El enlace UART A↔S desaparece.
4. **DX7 de 6 operadores como modo** (enmienda a ADR-005, Juan 2026-11-10): se agrega un modo **DX7 6-OP** fiel al
   original (32 algoritmos, envolventes de 4 etapas rate/level por operador, escalado por teclado, sensibilidad a la
   velocity, feedback en el operador 6, importación de SysEx). **FM 4-OP se mantiene** (edición simple, Brain Studio).
   Theory doc antes del código.
5. **Almacenamiento:** NVMe de 128 GB en HAT; arranque desde NVMe; raíz de solo lectura y partición `/data` para sets,
   registro, samples y modelos.
6. **Alimentación:** fuente oficial de 27 W para el Pi y presupuesto de consumo por riel **obligatorio antes de la
   placa** (auditoría G30); journal persistente en el Pi para diagnosticar cortes.
7. **Nombres y controles:** BRAIN = synth (partes, engines, presets); ENGINE = motor generativo. ACCEPT, GENERATE,
   SKIP y DROP son teclas de pantalla (clic de encoder o touch), no botones dedicados.
8. **Control del host de synths:** la GUI y el núcleo hablan con `gf-synth-host` por un socket local (JSON por líneas):
   cargar preset por parte, fijar parámetro, leer estado y medidores, y **listar parámetros por engine en páginas de
   8** (el host es la única fuente de los metadatos de parámetros: nombre, rango, unidad, página, banco). Spec
   detallado en el doc del host.

## Options Considered

| Opción | Veredicto | Por qué |
|---|---|---|
| Dos Teensy, synths en la A (decisión anterior) | ❌ | Una parte de synth a la vez; multitimbralidad en la Teensy choca con el tope de CPU (engines ~30 %, total ≤ 60 %) y con 1 MB de RAM |
| Multitimbralidad en la Teensy A | ❌ | Como mucho dos partes recortando voces; semanas de firmware con riesgo |
| **Synths en el Pi + una Teensy** | ✅ | Medido: 4 partes en ~21 % de un núcleo; una placa, un firmware, un cable |
| Teensy solo superficie y todo el audio en el Pi (interfaz multicanal) | ❌ por ahora | Un solo reloj, pero hardware nuevo y se pierden el SGTL5000 (cue, línea) y la CV |
| Audio renderizado del gemelo como samples | Complemento | Útil para tracks de biblioteca; no permite tocar ni editar en vivo |

## Consequences

- **Se pierde** el principio «el sonido no depende de Linux» para los synths del set: si el Pi se cae, la sala calla
  (ya pasaba con los samples). Mitigación: proceso de audio separado con watchdog y reanudación del estado.
- **Se gana** polifonía de partes, edición en vivo de las cuatro partes, el mismo sonido en Mac (VST) y consola, una
  placa más simple.
- El audio del gemelo **no es idéntico bit a bit** entre Mac (Clang) y Pi (GCC); falta medir si la diferencia es
  audible. La fidelidad frente al Brain físico sigue sin medir.
- Docs a actualizar: `03-arquitectura-capas.md` (capas, protocolos), `06-teensy-s-e4.md` y un mapa de pines único de
  la Teensy; `pendientes.md` (el DX7 6-OP sale de pendientes).
- Siguiente medición: el host real con 4 partes + GUI del synth + sampler en el Pi durante 60 min (puerta de ADR-007
  más 0 xruns).
