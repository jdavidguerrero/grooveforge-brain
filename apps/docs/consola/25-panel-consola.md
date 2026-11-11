# Consola: panel — controles, entradas y salidas

> **Fecha:** 2026-11-11. **Estado:** propuesta para Juan y entrada del prompt de Claude Design
> (`design/claude-design-prompt-consola-enclosure.md`). **Base:** diseño de GroovePush v2
> (`groovepush/docs/design/claude-design-2026-10-19-v2/`, `kicad/consola/panel/panel-coordinates.csv`),
> `00-concepto.md` §10.2, `07-gui.md` §3, `17`, `18`, `24-placa-principal.md` (D1–D7).

## 1. Qué cambia frente a GroovePush v2

GroovePush era un controlador de Ableton con pantalla inclinada y la Pi atornillada detrás de la pantalla. La
consola agrega lo que hace falta para producir y tocar sin computador, y Juan pide (2026-11-11):

- **Diseño plano**, estilo MPC / Ableton Push: cara superior horizontal, sin cuña.
- **Pi separada de la pantalla** con un cable DSI más largo, para no acumular calor detrás del módulo: la Pi 5 lleva
  Active Cooler, **HAT NVMe** y **Audio+ V2**, y va en una zona con ventilación propia.
- **Todos los botones del doc 17** (41 entradas) contemplados en el panel, con el 4.º MCP.

## 2. Inventario de controles

| Control | Cant. | Placa | Estado |
|---|---|---|---|
| Pantalla 7" DSI táctil | 1 | módulo Waveshare | Existe |
| Encoders EC11 con clic (bajo la pantalla) | 8 | PCB-A | Existe |
| Pads NeoTrellis M4 8×4 | 32 | M4 | Existe |
| Faders RS6011 60 mm | 4 | PCB-F | Existe |
| Teclas Kailh Choc V2 con LED | 20 | PCB-K1 / K2 | Existe (rótulos nuevos, §3) |
| **Teclas nuevas** con LED (§3) | 10 | **nueva** (PCB-M o PCB-N) | **Nuevo** |
| **Rueda jog** (encoder con clic, perilla grande Ø30–40) | 1 | **nueva** | **Nuevo** |
| **Pads FSR con velocity** (FSR 402 + actuador de silicona) | 4 | base rígida **PCB-P** | **Nuevo** |
| **FSR de expresión** (presión sostenida) | 2 | base rígida PCB-P | **Nuevo** |
| **Sensores de mano Sharp GP2Y0A41** (miran hacia arriba, 4–30 cm) | 2 | soporte + cable | **Nuevo** |
| **Perillas** MASTER, CUE, IN (10 kΩ lineal) | 3 | **nueva** | **Nuevo** |
| **Selector de pad de entrada** 0 / −10 / −20 dB (3 posiciones) | 1 | **nueva** | **Nuevo** |

## 3. Teclas: las 20 existentes y las 10 nuevas

Rótulos de las 20 existentes, adaptados a la consola (mismas posiciones eléctricas):

| Grupo | Teclas |
|---|---|
| Modos (K2) | LIVE · SYNTH · DRUM · SEQ · MIX (antes SESSION, NOTE, DEVICE, BROWSE, MIX) |
| Transporte (K1) | PLAY · STOP · REC · LOOP · METRO |
| Modificador (K1) | SHIFT |
| Edición (K1) | UNDO · DELETE · DUPLICATE · QUANTIZE · NEW |
| Página / octava (K1) | PAGE ◀ · PAGE ▶ · OCT − · OCT + |

**10 nuevas en el MCP 0x23** (con 3 entradas libres):

| Grupo | Teclas | Por qué |
|---|---|---|
| Navegación (junto al jog) | BACK · MENU · ◀ · ▶ · BROWSE | El jog reemplaza ↑ ↓ OK; BROWSE abre racks y presets sin tocar la pantalla |
| Motor en vivo | CUE · FILL · TAP | `00` §10.2.2; ACCEPT, SKIP, GENERATE y DROP siguen como teclas de pantalla (ADR-008 §7) |
| Clic del jog | 1 | |
| Pedal (jack trasero) | 1 | |

Esto reemplaza la lista de 11 de 17 §3 (`LEFT, RIGHT, BACK, PAGE_LEFT, PAGE_RIGHT, OCT_MINUS, OCT_PLUS, MENU,
SEARCH, SESSION_ARRANGE, DUPLICATE`), que repetía teclas que ya existen en K1/K2 (PAGE, OCT, DUPLICATE, SESSION).
**Al aprobarse, se corrigen 17 §3 y el mapa de notas de 18** (D1). Total: 8 clics + 20 + 10 + jog + pedal = **40**.

## 4. Entradas y salidas

| Lado | Conector | De dónde |
|---|---|---|
| Trasero | USB-C de alimentación (**única**, fuente oficial del Pi 27 W) — extensión de panel | Pi 5 |
| Trasero | Salida de sala L/R, 2 × jack 6,35 mm (cable RCA → jacks, D5) | Audio+ V2 |
| Trasero | LINE IN L/R, 2 × jack 6,35 mm | PCB-M (códec) |
| Trasero | CV 1–2, 2 × jack 3,5 mm | PCB-M |
| Trasero | MIDI IN / OUT, 2 × jack TRS 3,5 mm (tipo A) | PCB-M |
| Trasero | Pedal, jack 6,35 mm | PCB-M |
| Trasero | E-drum, DB15 o RJ45 (reservado, tapa ciega) | PCB-M |
| Trasero | USB-A × 1 para teclado MIDI (extensión de panel) | Pi 5 |
| Trasero | Ethernet (extensión de panel o acceso de servicio) | Pi 5 |
| Frente | Audífonos cue, jack 6,35 mm (y 3,5 mm opcional) | PCB-M (HP out) |
| Interno | Teensy USB, M4 USB, alimentación de LEDs (3 de los 4 USB-A del Pi, `24` §5) | Pi 5 |
| Interno | DSI largo (pantalla ↔ Pi), FFC 22 pines 0,5 mm | Pi 5 |

## 5. Placas: cuáles hacen falta

| Placa | Estado | Contenido |
|---|---|---|
| PCB-A, PCB-F, PCB-K1, PCB-K2 | Existen (sin cambios eléctricos; serigrafía de K1/K2 nueva) | §2 |
| **PCB-M** | Nueva (spec 24) | Teensy, códec, ADS ×2, MCP 0x23, MCP4725 ×2, MIDI, jacks traseros |
| **PCB-N** (navegación) | Nueva **si** las 10 teclas, el jog y las perillas no caen sobre PCB-M | 10 Choc V2 + LEDs (continúa la cadena), jog, 3 perillas, selector; un JST-SH a PCB-M |
| **PCB-P** (pads de fuerza) | Nueva, sencilla | Base plana y rígida para 6 FSR (sin componentes activos) y un JST a PCB-M |

Claude Design decide la ubicación; con eso se cierra si PCB-N existe o se integra en PCB-M.
