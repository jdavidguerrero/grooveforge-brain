# Consola: inventario de GroovePush en el monorepo

> **Fecha:** 2026-11-09. **Estado:** inventario; las decisiones que cambia van marcadas «para Juan». GroovePush se importó con su historial completo (61 commits) desde `github.com/jdavidguerrero/groovepush`; el repo original queda intacto. **No modifica** `00–06`.

## 1. Dónde quedó

| Antes (GroovePush) | Ahora |
|---|---|
| `apps/processor` | `apps/consola/processor` |
| `apps/gui` | `apps/consola/gui` |
| `apps/remote-script` | `apps/consola/remote-script` |
| `hardware/` | `apps/hardware/kicad/consola` |
| `specs/`, `docs/` | `apps/docs/consola/groovepush/` |
| `README`, `CLAUDE.md`, `ROADMAP`, `GAPS_AND_IMPROVEMENTS`, `.claude/` | `apps/consola/` |
| Sin commitear al 2026-11-09: reauditoría del 23-oct, `exports/`, cambio del `.kicad_pro` | Commit aparte en `apps/hardware/kicad/consola` |

## 2. Lo que GroovePush ya decidió y coincide con la consola

| ADR de GroovePush | Qué decidió | Encaja con |
|---|---|---|
| **0002** (2026-07-16) | Dos modos: controlador de Live y **standalone**. El **Pi 5 es el «sistema operativo»** del standalone (secuenciador y patrones); la Teensy queda como capa de sensores y LEDs. Audio: **Raspiaudio Audio+ V2**, solo salida; un Mic+ para entrada, pendiente | `00-concepto.md` §3, §4 y §10.1 |
| **0003** (2026-10-19) | GUI en **Qt 6 / QML** para la pantalla **7" 1280×720 multitouch**, con la identidad visual del Brain | La pantalla de Juan; mismas fichas de color que `studio-head` |
| **0004** (2026-10-19) | **+12 botones** (20 de función en total) **Kailh Choc V2** con LED SK6812, en los dos MCP23017; los mismos de la PCB-K del Brain | `00-concepto.md` §10.2.2 |

## 3. Lo que se reutiliza

| Pieza | Estado | Uso en la consola |
|---|---|---|
| **Firmware del processor** (Teensy 4.1): `LiveController`, `NeoTrellisLink`, encoders, faders, MCP23017, LEDs | Maduro: controlador de sesión y mezcla de Live por SysEx | La superficie, tal cual, en modos ESTUDIO e HÍBRIDO |
| **GUI Qt** (`ClipGridModel`, `TrackListModel`, `MixerModel`, `SerialController`) | Funciona; vistas de Session y Mix, maquetada para 800×480 | Base de la pantalla; ADR-0003 ya pide pasarla a 1280×720 |
| **Remote Script PushClone** | Rico: clips, mezcla, dispositivos, navegador, automatización | Modo ESTUDIO: control de Live. Convive con `GFMotor` y el de GroovePilot |
| **Placas del panel** (PCB-A, PCB-F, PCB-K1, PCB-K2, PCB-S) y contrato de conectores | Ruteadas con DRC 0/0/0, auditadas el 22 y el 23 de octubre | **La PCB de superficie que `00-concepto.md` §7 pensaba dibujar ya existe** |

## 4. Hallazgos que cambian el plan (para Juan)

1. **Las placas de GroovePush usan la portadora del Brain Studio.** Su Teensy va en la misma portadora, y la PCB-S se monta **en los zócalos del Audio Shield vacíos** para sacar I2C y la UART hacia la NeoTrellis M4. Con **dos Teensy**, la consola fabrica **la misma portadora dos veces**: una con Audio Shield (Brain) y otra con PCB-S (superficie). JLC fabrica mínimo 5 placas, así que ya vienen en el mismo pedido.
2. **Esto contradice la hipótesis de una sola Teensy** (`00-concepto.md` §10.2.3): la PCB-S usa los pines **20 y 21** como UART5 hacia la M4, y en el Audio Shield esos pines son **LRCLK y BCLK del I2S**. Con el diseño actual no caben audio y superficie en la misma Teensy. **Con lo ya diseñado, la consola es de dos Teensy.** Una sola exigiría rediseñar la PCB-S.
3. **Los MCP23017 tienen 14 entradas útiles, no 16** (GPA7 y GPB7 solo son salidas, por una errata del silicio). Con ADR-0004 los dos chips quedan llenos: 8 clics de encoder + 20 botones de función. Más botones necesitan un tercer MCP en 0x22. Se corrige `00-concepto.md` §10.2.2.
4. **Los pads son una NeoTrellis M4** (con su propio microcontrolador, por UART), no dos placas NeoTrellis I2C. A confirmar con Juan qué tiene en la mano.
5. **Piezos e IR se sacaron de GroovePush** (para un futuro módulo de e-drum). Los **FSR** del plan de la consola entran en el hueco de la placa analógica: hay que revisar si la PCB-A deja entradas analógicas libres.
6. **Conflicto de puerto:** el spec de GroovePush reservaba **TCP 9878** para un «Controller Bridge» en el plugin de GroovePilot. **9878 ya lo usa `GFMotor`.** El puente de GroovePush nunca se construyó; si se retoma, va en otro puerto.

## 5. Lo que está desactualizado (no se reescribe al importar)

| Dónde | Qué | Situación |
|---|---|---|
| `apps/consola/README.md`, `CLAUDE.md`, specs `push-controller-*`, ROADMAP fase D | El copiloto de mezcla con **IA en la nube** de GroovePilot (pairing, backend, cuota, Mix Score) | Prioridad de Juan desde 2026-10-26: IA **local**. Se reescribe cuando se especifique la consola |
| ROADMAP bloques 9–12 | Daemon `gp-bridged`, bus ZeroMQ | Sin empezar; se replantea con el motor local |
| `apps/consola/package.json`, `pnpm-workspace.yaml`, `Makefile` | Orquestación propia de GroovePush | Hay que integrarla al tooling del monorepo |
| `apps/consola/gui/CMakeLists 2.txt` | Duplicado suelto | Ya marcado en `GAPS_AND_IMPROVEMENTS.md` |

## 6. Siguientes pasos propuestos

1. Juan confirma el hallazgo 2 (dos Teensy, con la portadora repetida) y el 4 (qué NeoTrellis tiene).
2. Actualizar `00-concepto.md` y `01-prototipo-mesa.md` con estas piezas: el prototipo de mesa puede arrancar con el firmware y la GUI que ya existen.
3. Una sola fuente de verdad: un ADR del monorepo que diga que la consola es GroovePush + Brain, y qué specs de GroovePush siguen vigentes.
