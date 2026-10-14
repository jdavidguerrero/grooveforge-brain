# Studio — Theory doc del PCB (panel y portadora)

> **Tipo:** theory doc (precede al esquemático, `CLAUDE.md`).
> **Variante:** Brain · Studio (personal). **Decisiones base:** ADR-002, ADR-004, `01` §3.3 (pines),
> `01` §3.6 (placas de Studio), enclosure validado (`variants/studio/design/README.md`).
> **Fecha:** 2026-10-13
> **Fabricación:** JLCPCB, 2 capas, con LEDs y SMD ensamblados (BOM + CPL con part numbers de LCSC).

---

## 0. Qué resuelve

Define **cuántas placas hay, qué lleva cada una, a qué altura va, cómo se conectan y cómo se
alimentan**, con los números que salen de las piezas reales (EC11 y Choc V2 medidos el 2026-10-12/13).
Deja listas las decisiones para dibujar los esquemáticos en KiCad.

**No cubre:** el layout fino (eso es KiCad), el modelo del enclosure en Fusion 360, ni el firmware.

## 1. Las piezas que mandan

| Pieza | Datos que importan | Fuente |
|---|---|---|
| **ALPS EC11 con push** (variante L=25) | Cuerpo 0–5 mm desde la base; **buje liso** 5–12 mm, **Ø6.8**; eje liso 12–15 mm; eje estriado Ø6 de 10 mm; punta a **26.5 mm en reposo** (25 apretado: **push de 1.5 mm**); pines 3.5 mm | datasheet + calibre de Juan |
| **Kailh Choc V2** | Se engancha en placa de **1.2–1.3 mm**; la carcasa baja **2.2 mm** bajo la placa hasta el PCB; footprint: centro Ø5.0, 2 pines Ø1.2, guía Ø1.6, **ventana de LED a 4.93 mm del centro** | datasheet Kailh |
| **Aluminio del panel** | 2 mm, cara superior = **0**, inferior = **−2**; rebaje de 0.8 mm bajo cada Choc (queda 1.2 mm para el clip) | enclosure |
| **RV09** (IN y VOL, a comprar) | Vertical, eje estriado Ø6 del mismo estriado que el EC11, largo 25 mm desde la base | §5 |

## 2. Por qué no alcanza una sola placa de panel

El Choc exige su PCB a **−3.4 mm** (1.2 de clip + 2.2 de carcasa). El cuerpo del EC11 mide 5 mm y
tiene que quedar bajo el aluminio. Si los dos compartieran placa, a −3.4, el cuerpo del EC11 llegaría a
+1.6 mm: atravesaría el aluminio. Hacen falta **dos niveles**.

La salida que no obliga a cortar ni cambiar los encoders es **bajar la placa de los EC11 y usar el buje
liso como guía** a través del aluminio (sin tuerca: el encoder lo sostiene su placa atornillada a
separadores).

## 3. Apilado (aluminio arriba = 0)

| Elemento | Altura | Comentario |
|---|---|---|
| Cara superior del aluminio | 0 | |
| Borde del buje del EC11 | **0.0** | Al ras del aluminio; lo tapa el faldón del knob |
| Punta del eje en reposo | **+14.5** | 13.0 mm apretado; los 10 mm estriados quedan completos sobre el panel |
| Cara inferior del aluminio | −2.0 | |
| **PCB-K** (cara superior) | **−3.4** | Choc V2; espesor 1.6 → cara inferior en −5.0 |
| LEDs SK6812-MINI-E (cara inferior de PCB-K) | −5.0 a ~−6.0 | Montaje invertido: brillan hacia arriba por agujeros de PCB-K |
| Tope del cuerpo del EC11 | −7.0 | 2.0 mm de aire bajo PCB-K (1.0 bajo los LEDs) |
| **PCB-A** (cara superior) | **−12.0** | Base de los EC11 y de los RV09; separadores M3 de **10 mm** (medida estándar) desde la cara inferior del aluminio |
| Pines del EC11 / fin del apilado | ~−17.1 | En la franja de los encoders (y = 20–75 mm) la cuña tiene ~44 mm por dentro (rev. +15 mm) |
| Pantalla Freenove (vidrio + placa, 7.6 mm) | −2.5 a −10.1 | Junta de 0.5 bajo el aluminio. En y 76.9–152.9 (centro corrido a 114.9, rev. 2026-10-14): 0.9 mm de aire frente al borde trasero de PCB-K (y 76) y 1.9 mm sobre PCB-A |
| Pi 3B (atornillada a la pantalla) | hasta −40.4 | 38.4 mm desde el vidrio en USB/Ethernet (§4.5) |
| Piso interior (PETG 3 mm) | z = 3 sobre la base | Enclosure 43 / 78 mm (rev. +15 mm): ver alturas libres en §4.5 |

**Agujeros en el aluminio que cambian respecto del enclosure:** EC11 pasa de Ø7.2 con rosca M7 a
**Ø7.1 liso** (buje de 6.8 + 0.3 de juego). El resto (Choc 14×14 con rebaje 16×16×0.8, NAV Ø32 con
difusor, salida de aire) queda igual. El agujero de los RV09 se fija con el modelo (§5).

**El knob:** necesita un orificio ≥ 10 mm de profundo y su faldón tiene que quedar a ≥ 2 mm del aluminio
para que el push baje 1.5 mm sin tocar.

## 4. Las placas (3 + la trasera, §4.4)

Coordenadas del panel: x desde el borde izquierdo, y desde el borde frontal (enclosure).

### 4.1 PCB-K — teclas y LEDs (−3.4 mm)

- **6 Choc V2:** B1–B4 en (60/80/100/120, 62), B5 en (60, 32), B6 en (120, 32).
- **18 SK6812-MINI-E** en la cara inferior: 6 bajo las ventanas de LED de los Choc y **12 en anillo**
  alrededor de NAV (90, 32), radio ~13 mm, mirando al difusor de Ø32.
- **5 agujeros de 8.0 mm** para los bujes de los EC11 (Ø6.8 + ±0.6 de tolerancia de alineación entre placas): ENC L (32, 62), ENC R (148, 62), P1 (32, 32),
  P2 (148, 32), NAV (90, 32).
- Contorno aproximado: x 18–162, y 14–76.
- **Sujeción:** PCB-K cuelga de los 6 Choc enganchados en el rebaje del aluminio (como un teclado con placa).
  Los separadores de PCB-A la atraviesan por agujeros de paso de ~6.5 mm sin tocarla.
- **Ensamblado JLCPCB en una sola cara (la inferior):** LEDs y condensadores. Los Choc se sueldan a mano
  (o en socket hot-swap, a decidir).

### 4.2 PCB-A — encoders y pots (−12.0 mm)

- **5 EC11** bajo sus agujeros (mismas coordenadas de §4.1).
- **2 RV09** en dos brazos que suben por los costados de la pantalla hasta (15, 96) y (165, 96). Los
  brazos tienen que pasar por fuera del módulo Freenove: **medir el módulo antes de cerrar el contorno**
  (tarea 44.61).
- Contorno aproximado: franja x 6–174, y 18–78, más brazos x 6–24 y x 156–174 hasta y ~106.
- Soporte: separadores M3 de 10 mm atornillados a los espárragos PEM del aluminio, tuerca M3 debajo de PCB-A.
- Todo through-hole: se suelda a mano o con el ensamblado THT de JLCPCB.

### 4.3 Portadora — en el piso, bajo la Pi (rev. 2026-10-14)

- **Teensy 4.1 + Audio Shield Rev D** como módulos sobre headers (decisión 2026-10-12), **lado a lado**, no
  apilados (2026-10-13): el shield va en sus propios zócalos 1×14 (J15/J16) y la portadora solo cablea
  7, 8, 18–21, 23, 3V3 y GND. Así los pines 10–13 (ENC L/R) y 15 (push de ENC R) no tocan el slot SD ni
  el footprint MEM, se reusa el shield ya soldado y se ganan ~12 mm de altura. Costo: ~9 pistas de
  I2S/I2C de 3–4 cm; MCLK (11.3 MHz) corta y sobre plano de GND.
- **Alimentación** (§7), **jacks traseros** (§4.4), **UART a la Pi** (pines 0/1 de la Teensy ↔ GPIO14/15
  de la Pi, 3.3 V directo, GND común), **CV OUT** (pin 29).
- **Ubicación:** en el piso bajo la Pi (§4.5), con la hilera de jacks contra la pared trasera. Objetivo
  ≤ 100 × 100 mm (tarifa base de JLCPCB). Alto del conjunto: separador 5 + placa 1.6 + zócalo 8.5 + Teensy
  1.6 + piezas ~3 ≈ **20 mm sobre el piso**.

### 4.4 Jacks traseros: una hilera soldada a la portadora (decisión 2026-10-14)

El diseño de Claude Design ponía los jacks en dos filas (z ≈ 16 y 42) y una tapa de servicio para la Pi en
x 38–92. Con la Pi atornillada a la pantalla y sus puertos hacia el costado, **la tapa se elimina** y la
pared trasera queda libre; con el enclosure 15 mm más alto (§3), la portadora llega a la pared:

- **Hilera baja, soldada a la portadora:** IN L/R y OUT L/R (1/4" TRS), PHONES (3.5 TRS) y CV OUT (3.5).
  Jacks **horizontales de PCB** (eje paralelo a la placa), los más comunes y baratos de LCSC. Las x salen
  del layout.
- **Arriba, de panel con cable:** USB-C Teensy, USB-A host, USB-C 5 V y la palanca PAD (cableada al header
  SW1 de 2×4, junto a la escalera R4–R9).
- ~~Placa trasera vertical (J20/J17, IDC 2×8)~~: se dibujó el 2026-10-14 y se descartó el mismo día; no
  llegó a commit.

### 4.5 Raspberry Pi y alturas libres (medido 2026-10-14)

La Pi 3B **va atornillada detrás del módulo Freenove** (120.8 × 76 × 7.6 mm; 38.4 mm con la Pi). En el
enclosure (pantalla centrada en (90, 114.9), §3): Pi en x ~43–132, y ~92–149, con USB y Ethernet hacia la
**derecha** (hasta x ~138), el HDMI hacia atrás y el GPIO hacia el frente (medido sobre foto, ±2 mm).

Altura libre sobre el piso (z = 3) con el enclosure de +15 mm:

| Zona | Libre |
|---|---|
| Bajo USB/Ethernet (x ~120–138; 38.4 mm de hondo) | ~20 mm adelante (y 92) → ~32 mm atrás |
| Bajo el resto de la Pi (~31 mm de hondo con el GPIO) | ~29 mm adelante → ~43 mm atrás |
| Franja delantera del módulo (y 77–92) y fuera de él | más de 30 mm |

La portadora (~20 mm) cabe en cualquier lugar bajo la Pi; bajo USB/Ethernet adelante solo pistas y piezas
bajas. Los puertos de la Pi quedan adentro (mantenimiento con la caja abierta): la Pi usa WiFi y se
alimenta por el GPIO (J6, §7).

## 5. RV09 para IN y VOL

Vertical, eje estriado de 6 mm con el mismo número de estrías que el EC11 (para usar el mismo knob),
**largo 25 mm desde la base**, en PCB-A a −12.0 mm: la punta queda a +13.0, igual que un EC11 apretado y
1.5 mm por debajo de uno en reposo (con knobs iguales casi no se nota).
El modelo exacto se elige en LCSC antes de comprar (y define el agujero del aluminio).

## 6. Conexiones entre placas

### 6.1 PCB-K ↔ PCB-A

Separación entre la cara inferior de PCB-K (−5.0) y la superior de PCB-A (−12.0): **7.0 mm**.
**Decisión (2026-10-13): cable corto con JST-SH de 10 pines y conectores de entrada lateral (SM10B)** en
el borde de cada placa. Dos conectores verticales enfrentados no caben en 7 mm, y el cable tolera errores
de altura.

Por PCB-K pasan 9 señales: B1–B6, datos de LEDs, 5 V y GND.

**Orden de pines (rev. 2026-10-14, igual en PCB-K y PCB-A, cable 1:1):**

| Pin | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| Señal | B4 | B6 | B3 | GND | +5V | GND | LED_DIN | B2 | B5 | B1 |

El orden sale del ruteo de PCB-K. J1 está en el borde trasero, centrado (pin 10 al oeste). Cada línea de
botón baja por una vía a F.Cu y se abre hacia su tecla sin cruzar a ninguna otra:
- B1, B5 y B2 van al oeste;
- B4, B6 y B3 van al este;
- LED_DIN baja al anillo;
- +5V y GND quedan en el centro, cada uno con su vía.

Con el orden anterior (+5V, GND, LED_DIN, GND, B1…B6) hacían falta ~14 cruces entre capas. Los pines de la
Teensy no cambian.

### 6.2 Panel → portadora (un solo cable desde PCB-A)

| Grupo | Señales | Pines de la Teensy (`01` §3.3) |
|---|---|---|
| Botones | B1, B2, B3, B4, B5, B6 | 2, 3, 4, 5, 40, 41 |
| LEDs | DIN | 28 (vía buffer, §9) |
| ENC L / ENC R / NAV | A, B, SW ×3 | 10/11/12 · 13/14/15 · 16/17/26 |
| P1 / P2 | A, B, SW ×2 | 30/31/32 · 34/35/9 |
| Pots | IN, VOL (cursor) | 24 (A10), 22 (A8) |
| Alimentación | 5 V (LEDs), 3.3 V (pots), GND ×3 | — |

**24 señales + 5 de alimentación ≈ 30 pines:** cable plano **IDC 2×15 de 2.54 mm** (robusto, se arma
a mano). Los botones y los encoders usan los pull-ups internos de la Teensy; se agrega un condensador de
10 nF por pin de encoder en PCB-A para filtrar rebote y ruido del cable (a validar con el sketch46).

## 7. Alimentación (portadora)

```
USB-C 5 V (solo CC, 2× 5.1 kΩ) ─► polyfuse 3 A ─► TVS 5 V ─┬─► Pi 3B (5 V del header GPIO, pines 2/4)
                                                          ├─► Teensy VIN (puente VUSB–VIN cortado)
                                                          └─► 5 V de los LEDs (100 µF en PCB-K)
                                       Teensy 3.3 V ─► Audio Shield · pots
```

- Alimentar la Pi por el header salta su polyfuse interno: el polyfuse de la portadora cumple esa función.
- **Presupuesto:** Pi 3B hasta ~2.5 A en picos + Teensy ~0.1 A + LEDs. 18 LEDs en blanco pleno llegan a
  ~1 A, así que **el firmware limita su consumo** (por ejemplo, `FastLED.setMaxPowerInVoltsAndMilliamps(5,
  400)`). La pantalla Freenove toma su energía de la Pi.
- Desacople: 100 µF + 100 nF en la entrada; 10 µF junto a la Teensy.

## 8. Audio y panel trasero (portadora)

| Conector | Señal | Notas |
|---|---|---|
| OUT L / OUT R (1/4" TRS) | LINE OUT del Audio Shield | Verificar si el shield trae capacitores de acople en LINE OUT; si no, 10 µF + 47 kΩ a GND |
| IN L / IN R (1/4" TRS) | → pad → protección → LINE IN del shield | §8.1 |
| PAD (palanca DP3T de 3 posiciones) | 0 / −10 / −20 dB en L y R | §8.1 (la posición no se lee) |
| PHONES (TRS 3.5) | Salida de audífonos del SGTL5000 (HP L / HP R / **HP_VGND**) | El común es una tierra virtual de ~1.5 V: el sleeve va a HP_VGND, **nunca a GND ni al chasis** (jack de cuerpo plástico, aislado) |
| CV OUT (TRS 3.5) | PWM del pin 29 → RC de 2 polos → buffer | Para el add-on del ladder (ADR-003) |
| USB-C datos de la Teensy | USB del módulo; **MIDI por USB** | Prolongador al panel trasero |
| USB-A host | Pines de USB host de la Teensy; controladores MIDI USB | |

### 8.1 Pad y protección de la entrada (ADR-004, acción 7)

El SGTL5000 acepta como máximo **3.12 Vpp** en LINE IN (con `lineInLevel` al mínimo de ganancia).
Una fuente de +4 dBu (≈ 4.4 Vpp) o un mixer de DJ caliente lo superan.

**Divisor de 3 tomas** sobre una escalera de ~47 kΩ, con la palanca eligiendo la toma:

| Toma | Resistencias (E96) | Relación | Atenuación |
|---|---|---|---|
| 0 dB | entrada directa | 1 | 0 dB |
| −10 dB | (12.4 k + 5.23 k) / 50.0 k, cargada con 101 kΩ | 0.316 | −10.1 dB (simulado) |
| −20 dB | 5.23 k / 50.0 k, cargada con 101 kΩ | 0.100 | −20.1 dB (simulado) |

(escalera: 32.4 k → 12.4 k → 5.23 k → GND. Recalculada el 2026-10-13 con la carga de R10 + R12 hacia
VREF; la rev 0.1 usaba 10.2 k / 4.75 k y daba −11.0 / −20.4 dB. Validación: `03-studio-input-stage.md`.)

- **Carga:** la impedancia de salida de la toma (hasta ~11 kΩ) contra la entrada del SGTL5000 (~29 kΩ,
  verificar en el datasheet) agrega ~2–3 dB de atenuación y la hace depender del codec. Hay dos salidas:
  recalcular la escalera más baja (~10 kΩ total), o poner un **buffer** rail-to-rail de 3.3 V (opamp doble)
  después del pad, que además aísla la protección. **Se recomienda el buffer.**
- **Protección:** resistencia en serie de 1 kΩ + diodos Schottky dobles (BAT54S) a 3.3 V y GND antes del
  buffer.
- **Lectura del pad: no se lee.** La entrada es estéreo, así que la palanca DP3T usa un polo para L y
  otro para R; leer la posición pediría un tercer polo (3P3T, poco común en palanca). La GUI oculta el
  chip de PAD, como prevé el diseño v3. El pin 25 queda libre.
- **Cadena por canal** (esquemático de la portadora): jack → escalera → palanca → 1 µF → 1 kΩ → BAT54S a
  3.3 V / GND sobre un nodo polarizado a VREF (1.65 V, 100 kΩ) → seguidor MCP6004 → 2.2 µF → LINE IN.
  Las protecciones van sobre el nodo polarizado: sobre el nodo de 0 V recortarían los semiciclos
  negativos.

## 9. LEDs

- **18 × SK6812-MINI-E** en cadena; mismo protocolo que el WS2812B.
- **Nivel lógico:** alimentados a 5 V, piden V_IH ≥ 0.7 × V_DD = 3.5 V, y la Teensy entrega 3.3 V. Va un
  **buffer 74AHCT1G125** alimentado a 5 V en la portadora, junto al pin 28, y una resistencia de 330 Ω en
  serie en la línea de datos.
- Desacople: 100 nF por LED (o uno cada dos, según el layout) + 100 µF en la entrada de PCB-K.
- Antes del pedido: verificar el part number y el stock en LCSC.

## 10. Decisiones abiertas

1. ~~MIDI IN por TRS~~ — **resuelto 2026-10-13: MIDI solo por USB** (USB-A host y USB de la Teensy).
   No quedaba ningún RX de UART libre (0, 7, 15, 16, 21, 25, 28 y 34 están ocupados); el mapa de pines
   de `01` §3.3 no cambia. El hueco del jack TRS en el panel trasero pasa a ser **PHONES**.
2. ~~Conector PCB-K ↔ PCB-A~~ — resuelto: JST-SH 10 de entrada lateral (§6.1).
3. **Choc soldados o en socket hot-swap.** El socket se monta en la cara inferior, donde también van los
   LEDs; hay que verificar que el socket y el LED no choquen bajo cada tecla.
4. ~~Buffer de entrada~~ — resuelto: MCP6004 (seguidores L y R, VREF y CV OUT en el mismo integrado).
5. **Acople de LINE OUT** del Audio Shield (§8): verificar el esquemático del shield Rev D.
6. ~~Módulo Freenove y Pi~~ — **medido 2026-10-14** (§4.5). Consecuencias: enclosure +15 mm, pantalla
   corrida 2.5 mm hacia atrás (choque del vidrio con PCB-K), sin tapa de servicio. Los brazos de PCB-A
   (x 6–24 y 156–174) quedan 5.6 mm fuera del módulo.
7. **Jacks horizontales de la portadora:** elegir modelos de LCSC (1/4" TRS y 3.5 TRS) al hacer el layout.

## 10B. Esquemáticos (rev 0.1, 2026-10-13)

`apps/hardware/kicad/studio/{pcb-k,pcb-a,portadora}` — ERC sin violaciones en los tres; PDFs en
`apps/hardware/kicad/studio/docs/`. Notas que salieron al dibujarlos:

- **El slot microSD del Audio Shield no se usa:** sus pines 10–13 quedan sin conexión en J15/J16.
- **Raspberry Pi:** IDC 2×5 a los pines 1–10 de su header (5 V en 2 y 4, GND en 6 y 9, TXD en 8, RXD en 10).
  Cable con dos hilos por 5 V y por GND.
- **Jacks traseros:** ~~la portadora crece hasta la pared o jacks de panel~~ → **resuelto 2026-10-14:**
  placa trasera vertical con los jacks soldados (§4.4).

## 11. Próximo paso

Proyecto KiCad en `apps/hardware/kicad/studio/` con un esquemático por placa (`pcb-k`, `pcb-a`,
`portadora`) y sus símbolos y footprints (Choc V2, EC11, RV09, SK6812-MINI-E, IDC 2×15). Orden: PCB-K y
PCB-A primero (son las más simples y validan el apilado), después la portadora.

## Referencias

- ALPS Alpine, EC11 series datasheet (EC111 con push, recorrido 1.5 mm).
- Kailh, Choc V2 datasheet (dimensiones y footprint recomendado).
- Opsco / Normand, SK6812MINI-E datasheet (V_IH, montaje invertido).
- NXP, SGTL5000 datasheet (LINE IN: nivel máximo e impedancia).
- PJRC, Teensy 4.1 pinout (UART) y Audio Adaptor Board Rev D (esquemático).
- Texas Instruments, SN74AHCT1G125 datasheet (traslado de 3.3 V a 5 V).
