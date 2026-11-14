# Consola: panel — controles, entradas y salidas

> **Fecha:** 2026-11-11. **Estado:** aprobado por Juan (2026-11-11) y entrada del prompt de Claude Design
> (`design/claude-design-prompt-consola-enclosure.md`). **Base:** diseño de GroovePush v2
> (`groovepush/docs/design/claude-design-2026-10-19-v2/`, `kicad/consola/panel/panel-coordinates.csv`),
> `00-concepto.md` §10.2, `07-gui.md` §3, `17`, `18`, `24-placa-principal.md` (D1–D7).

## 1. Qué cambia frente a GroovePush v2

GroovePush era un controlador de Ableton con pantalla inclinada y la Pi atornillada detrás de la pantalla. La
consola agrega lo que hace falta para producir y tocar sin computador, y Juan pide (2026-11-11):

- **Diseño plano**, estilo MPC / Ableton Push: cara superior horizontal, sin cuña.
- **Pi separada de la pantalla** con un cable DSI más largo, para no acumular calor detrás del módulo: la Pi 5 lleva
  Active Cooler, **HAT NVMe** y **Audio+ V2**, y va en una zona con ventilación propia.
- **Todos los botones del doc 17** (36 entradas) contemplados en el panel, con el 4.º MCP.

## 2. Inventario de controles

| Control | Cant. | Placa | Estado |
|---|---|---|---|
| Pantalla 7" DSI táctil | 1 | módulo Waveshare | Existe |
| Encoders EC11 con clic (bajo la pantalla) | 8 | PCB-A | Existe |
| Pads NeoTrellis M4 8×4 | 32 | M4 | Existe |
| Faders RS6011 60 mm | 4 | PCB-F | Existe |
| Teclas Kailh Choc V2 con LED | 20 | PCB-K1 / K2 | Existe (K2 pasa a navegación, §3) |
| **Teclas nuevas** con LED (§3) | 6 | **nueva** (PCB-M o PCB-N) | **Nuevo** |
| **Rueda jog** (encoder con clic, perilla grande Ø30–40) | 1 | **nueva** | **Nuevo** |
| **Pads FSR con velocity** (FSR 402 + actuador de silicona) | 4 | base rígida **PCB-P** | **Nuevo** |
| **FSR de expresión** (presión sostenida) | 2 | base rígida PCB-P | **Nuevo** |
| **Sensores de mano Sharp GP2Y0A41** (miran hacia arriba, 4–30 cm) | 2 | soporte + cable | **Nuevo** |
| **Perillas** MASTER, CUE, IN (10 kΩ lineal) | 3 | **nueva** | **Nuevo** |
| **Selector de pad de entrada** 0 / −10 / −20 dB (3 posiciones) | 1 | **nueva** | **Nuevo** |

## 3. Teclas

**Los modos se eligen desde la pantalla** (Juan, 2026-11-11): no se cambia de SYNTH a LIVE en medio de un tema, así
que no necesitan tecla directa. Las 5 teclas de **PCB-K2** (columna a la izquierda de la pantalla) pasan a ser la
**navegación**, que es justo lo que conviene tener junto al touch.

| Grupo | Placa | Teclas | Función |
|---|---|---|---|
| Navegación (5) | K2 | BACK · MENU · ◀ · ▶ · BROWSE | MENU abre el selector de modos y ajustes; BROWSE abre racks y presets; ◀ ▶ cambian de pestaña |
| Transporte (5) | K1 | PLAY · STOP · REC · LOOP · METRO | |
| Modificador (1) | K1 | SHIFT | SHIFT + pad = acento |
| Edición (5) | K1 | UNDO · DELETE · DUPLICATE · QUANTIZE · NEW | |
| Página / octava (4) | K1 | PAGE ◀ ▶ · OCT − + | |
| **Motor en vivo (3)** | **nueva** | CUE · FILL · TAP | CUE escucha lo siguiente por audífonos; TAP parpadea al tempo |
| **Sección (3)** | **nueva** | BUILD · DROP · BREAK | Cambio de sección del set (`00` §10.2.2), cuantizado a la frase; SHIFT = al siguiente compás. Recetas de Juan por set; función especificada después de E5 (`pendientes.md`) |

**MCP 0x23:** CUE, FILL, TAP, BUILD, DROP, BREAK + clic del jog + pedal = **8 de 14**. Total: 8 clics + 20 + 6 + jog +
pedal = **36**. Todas las teclas Choc V2 con LED (una sola familia de keycaps y moldes): hacen falta
**6 Choc más** que las 20 de Juan (pedir 10 con repuestos, mismo modelo).

Contrato v1 implementado en18 §3: cuatro MCP, 36 entradas. El pinout provisional de0x23 y los nombres heredados de K2 se señalan allí. Las recetas de sección y los LEDs de teclas siguen pendientes.

## 4. Entradas y salidas

| Lado | Conector | De dónde |
|---|---|---|
| Trasero | USB-C de alimentación (**única**, fuente oficial del Pi 27 W) — extensión de panel | Pi 5 |
| Trasero | Salida de sala L/R, 2 × jack 6,35 mm (cable RCA → jacks, D5) | Audio+ V2 |
| Trasero | LINE IN L/R, 2 × jack 6,35 mm, con el frente analógico de la portadora del Brain (§6) | PCB-M (códec) |
| Trasero | LINE OUT L/R del códec, 2 × jack 6,35 mm (salida del modo procesador de FX, §6) | PCB-M (códec) |
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
| **PCB-N** (motor en vivo) | Nueva **si** las 6 teclas, el jog y las perillas no caen sobre PCB-M | Choc V2 + LEDs (continúa la cadena), jog, 3 perillas, selector; un JST-SH a PCB-M |
| **PCB-P** (pads de fuerza) | Nueva, sencilla | Base plana y rígida para 6 FSR (sin componentes activos) y un JST a PCB-M |

Claude Design decide la ubicación; con eso se cierra si PCB-N existe o se integra en PCB-M.

## 6. Entrada de audio y modo procesador de FX

La Teensy **sí tiene entrada**: LINE IN del SGTL5000 con la perilla IN y el selector de pad. Sirve para el futuro
**modo procesador de FX** (audio externo → FX → salida). PCB-M **reutiliza el frente analógico de la portadora del
Brain Studio** (`kicad/studio/portadora`), ya diseñado y auditado: jacks 6,35 mm NRJ3HF-1, conmutador de pad 0 / −10 /
−20 dB (DP3T), buffers MCP6004 con referencia VREF, protección BAT54S y perilla IN. Se copia la hoja, no se rediseña.

Dos caminos para el modo FX, sin hardware extra:
- **FX en la Teensy** (baja latencia, < 3 ms): LINE IN → FX de la Teensy → **LINE OUT del códec** (por eso los jacks
  de LINE OUT quedan poblados).
- **FX en el Pi** (más FX, más latencia): LINE IN → USB asíncrono → Pi → Audio+.

## 7. Distribución v1 (Juan, 2026-11-11) — fijada

Coordenadas: `apps/hardware/kicad/consola/panel/panel-coordinates-console-v1.csv` (x desde el borde izquierdo, y desde
el frente, mm). Panel de **340 × 340 mm**; cuerpo macizo impreso **en una pieza** en una cama local de 35 × 35 (si el
servicio pide más margen, 336 o dos piezas con unión oculta). Se diseñó aquí a escala, no en Claude Design, que solo
hace el cuerpo.

| Zona | Contenido | Por qué |
|---|---|---|
| Eje central | Pantalla, 8 encoders (paso 26, PCB-A sin cambios), NeoTrellis, 4 FSR de velocity, transporte al frente | Lo que se mira y se toca a la vez, centrado |
| Alrededor de la rejilla | Izquierda: DELETE · DUPLICATE · QUANTIZE · NEW; derecha: **escenas 1–4** en modo controller / OCT ± y PAGE ◀ ▶ en SYNTH y DRUM | Alineadas con las 4 filas de pads (paso 15), como Push |
| Izquierda | Columna de navegación junto a la pantalla (BACK · MENU · ◀ · ▶ · BROWSE); motor (CUE · FILL · TAP) y sección (BUILD · DROP · BREAK); SHIFT · UNDO; 2 FSR de expresión; **IN + selector de pad** en la esquina delantera | La mano que dirige el set; la entrada se ajusta y se deja |
| Derecha | Jog Ø40 junto a la pantalla; 4 faders paso 20; **CUE + MASTER** en la esquina delantera (MASTER en la esquina, protegido); audífonos en el frente bajo CUE | La mano que mezcla y escucha |
| Esquinas traseras | Sharp horizontales; arriba a la izquierda queda libre como zona de gesto | |

Ajustes aplicados: faders 5 mm más abajo (13 mm de aire con el encoder 8), transporte 4 mm más al frente y keycaps del
transporte 1 mm más bajos (los FSR de velocity quedan a 25 mm), un solo keycap de **13,5 mm** para las 26 teclas (un
molde), LED por zona (verde = tocar, violeta = navegar/editar; escenas cambian de color con el modo).

**Altura de PCB-A (2026-11-13).** El EC11 de Juan (Alps, eje estriado Ø6) mide **28 mm** de la base del cuerpo a la
punta del eje, no los 20 supuestos. PCB-A baja de −12 a **−20 mm** (cara superior de la placa = base del encoder) con
separadores más largos; el eje asoma **8 mm** sobre el aluminio y el casquillo queda bajo la chapa. El layout de
PCB-A no cambia. Verificado en FreeCAD sin choques (PCB-A en z 63,4–65; pantalla desde z 68,2; Pi hasta z 66 en otra
zona). Perillas Ø18 con agujero estriado de ≤ 7 mm de profundidad.

**Pantalla bajo la chapa (Juan, 2026-11-12).** El prompt de diseño v5 pedía el vidrio a ras del aluminio, pero la
ventana de la chapa es de 158 × 91 (área activa más margen) y el vidrio del módulo mide 189,32 × 120,24: el vidrio
queda **debajo** de la chapa, apoyado contra ella, con el aluminio tapando el borde negro. Se conserva así (protege y
sujeta el vidrio); el modelo de FreeCAD ya lo tiene con la cara del vidrio en z = 83 (2 mm bajo la cara superior).

**Placas (propuesta a cerrar en el esquema):** una **PCB-K única** bajo el panel a −3,4 mm con las 26 Choc, sus LEDs y
los MCP, con ventanas para la M4, los encoders, los faders y los FSR (como la placa principal de un Push), en lugar
de K1/K2 y varias placas sueltas; PCB-A (encoders) sin cambios; **PCB-F nueva** (paso 20); **PCB-P** en una sola tira
con los 6 FSR; jog, perillas, selector y audífonos montados en el panel y cableados a PCB-M (sin placa).
