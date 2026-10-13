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
| Borde del buje del EC11 | **+0.5** | Asoma 0.5 mm; lo tapa el faldón del knob |
| Punta del eje en reposo | **+15.0** | 13.5 mm apretado; los 10 mm estriados quedan completos sobre el panel |
| Cara inferior del aluminio | −2.0 | |
| **PCB-K** (cara superior) | **−3.4** | Choc V2; espesor 1.6 → cara inferior en −5.0 |
| LEDs SK6812-MINI-E (cara inferior de PCB-K) | −5.0 a ~−6.0 | Montaje invertido: brillan hacia arriba por agujeros de PCB-K |
| Tope del cuerpo del EC11 | −6.5 | 1.5 mm de aire bajo PCB-K (0.5 bajo los LEDs) |
| **PCB-A** (cara superior) | **−11.5** | Base de los EC11 y de los RV09 |
| Pines del EC11 / fin del apilado | ~−16.6 | En la franja de los encoders (y = 20–75 mm) la cuña tiene ~29 mm por dentro |

**Agujeros en el aluminio que cambian respecto del enclosure:** EC11 pasa de Ø7.2 con rosca M7 a
**Ø7.1 liso** (buje de 6.8 + 0.3 de juego). El resto (Choc 14×14 con rebaje 16×16×0.8, NAV Ø32 con
difusor, salida de aire) queda igual. El agujero de los RV09 se fija con el modelo (§5).

**El knob:** necesita un orificio ≥ 10 mm de profundo y su faldón tiene que quedar a ≥ 2 mm del aluminio
para que el push baje 1.5 mm sin tocar.

## 4. Las 3 placas

Coordenadas del panel: x desde el borde izquierdo, y desde el borde frontal (enclosure).

### 4.1 PCB-K — teclas y LEDs (−3.4 mm)

- **6 Choc V2:** B1–B4 en (60/80/100/120, 62), B5 en (60, 32), B6 en (120, 32).
- **18 SK6812-MINI-E** en la cara inferior: 6 bajo las ventanas de LED de los Choc y **12 en anillo**
  alrededor de NAV (90, 32), radio ~13 mm, mirando al difusor de Ø32.
- **5 agujeros de ~7.5 mm** para los bujes de los EC11: ENC L (32, 62), ENC R (148, 62), P1 (32, 32),
  P2 (148, 32), NAV (90, 32).
- Contorno aproximado: x 18–162, y 14–76. Agujeros de montaje hacia los espárragos PEM M3 del aluminio.
- **Ensamblado JLCPCB en una sola cara (la inferior):** LEDs y condensadores. Los Choc se sueldan a mano
  (o en socket hot-swap, a decidir).

### 4.2 PCB-A — encoders y pots (−11.5 mm)

- **5 EC11** bajo sus agujeros (mismas coordenadas de §4.1).
- **2 RV09** en dos brazos que suben por los costados de la pantalla hasta (15, 96) y (165, 96). Los
  brazos tienen que pasar por fuera del módulo Freenove: **medir el módulo antes de cerrar el contorno**
  (tarea 44.61).
- Contorno aproximado: franja x 6–174, y 18–78, más brazos x 6–24 y x 156–174 hasta y ~106.
- Soporte: separadores M3 hasta los espárragos del aluminio (9.5 mm entre PCB-A y la cara inferior del
  aluminio).
- Todo through-hole: se suelda a mano o con el ensamblado THT de JLCPCB.

### 4.3 Portadora — en el piso

- **Teensy 4.1 + Audio Shield Rev D** como módulos sobre headers (decisión 2026-10-12).
- **Alimentación** (§7), **conectores del panel trasero** (§8), **UART a la Pi** (pines 0/1 de la Teensy ↔
  GPIO14/15 de la Pi, 3.3 V directo, GND común), **CV OUT** (pin 29), **lectura del pad** (pin 25).
- Ubicación del enclosure: 72 × 50 mm en x 103–175, y 76–126. Probablemente crezca al absorber la placa
  de alimentación: revisar contra la Pi 3B, que va en el mismo piso.

## 5. RV09 para IN y VOL

Vertical, eje estriado de 6 mm con el mismo número de estrías que el EC11 (para usar el mismo knob),
**largo 25 mm desde la base**, en PCB-A a −11.5 mm: la punta queda a +13.5, a la par de los encoders.
El modelo exacto se elige en LCSC antes de comprar (y define el agujero del aluminio).

## 6. Conexiones entre placas

### 6.1 PCB-K ↔ PCB-A

Separación entre la cara inferior de PCB-K (−5.0) y la superior de PCB-A (−11.5): **6.5 mm**. Opciones:

| Opción | Pro | Contra |
|---|---|---|
| **Conector placa a placa de 1.27 mm**, altura de apilado ~6.5 mm | Sin cables; JLCPCB lo monta | Hay que encontrar uno con esa altura exacta en LCSC |
| Cable corto con JST-SH | Tolerante a errores de altura | Un cable más que armar |

Por PCB-K pasan 9 señales: B1–B6, datos de LEDs, 5 V y GND.

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
| PAD (palanca de 3 posiciones) | 0 / −10 / −20 dB, y lectura en el pin 25 | §8.1 |
| MIDI IN (TRS 3.5, tipo A) | Optoacoplador → UART de la Teensy | **Ver §10, decisión 1** |
| CV OUT (TRS 3.5) | PWM del pin 29 → RC de 2 polos → buffer | Para el add-on del ladder (ADR-003) |
| USB-C datos de la Teensy | USB del módulo | Prolongador al panel trasero |
| USB-A host | Pines de USB host de la Teensy | |

### 8.1 Pad y protección de la entrada (ADR-004, acción 7)

El SGTL5000 acepta como máximo **3.12 Vpp** en LINE IN (con `lineInLevel` al mínimo de ganancia).
Una fuente de +4 dBu (≈ 4.4 Vpp) o un mixer de DJ caliente lo superan.

**Divisor de 3 tomas** sobre una escalera de ~47 kΩ, con la palanca eligiendo la toma:

| Toma | Resistencias (E96) | Relación | Atenuación |
|---|---|---|---|
| 0 dB | entrada directa | 1 | 0 dB |
| −10 dB | (10.2 k + 4.75 k) / 47.35 k | 0.316 | −10.0 dB |
| −20 dB | 4.75 k / 47.35 k | 0.100 | −20.0 dB |

(escalera: 32.4 k → 10.2 k → 4.75 k → GND)

- **Carga:** la impedancia de salida de la toma (hasta ~11 kΩ) contra la entrada del SGTL5000 (~29 kΩ,
  verificar en el datasheet) agrega ~2–3 dB de atenuación y la hace depender del codec. Hay dos salidas:
  recalcular la escalera más baja (~10 kΩ total), o poner un **buffer** rail-to-rail de 3.3 V (opamp doble)
  después del pad, que además aísla la protección. **Se recomienda el buffer.**
- **Protección:** resistencia en serie de 1 kΩ + diodos Schottky dobles (BAT54S) a 3.3 V y GND antes del
  buffer.
- **Lectura del pad:** la palanca es de **dos polos (DP3T)**: un polo elige la toma de audio y el otro
  conecta un divisor que da 3 voltajes distintos al pin 25 (ADC).

## 9. LEDs

- **18 × SK6812-MINI-E** en cadena; mismo protocolo que el WS2812B.
- **Nivel lógico:** alimentados a 5 V, piden V_IH ≥ 0.7 × V_DD = 3.5 V, y la Teensy entrega 3.3 V. Va un
  **buffer 74AHCT1G125** alimentado a 5 V en la portadora, junto al pin 28, y una resistencia de 330 Ω en
  serie en la línea de datos.
- Desacople: 100 nF por LED (o uno cada dos, según el layout) + 100 µF en la entrada de PCB-K.
- Antes del pedido: verificar el part number y el stock en LCSC.

## 10. Decisiones abiertas

1. **MIDI IN por TRS necesita un RX de UART, y no queda ninguno libre.** Los RX de la Teensy 4.1 son
   0 (puente con la Pi), 7 (I2S), 15 (ENC R SW), 16 (NAV A), 21 (I2S), 25 (pad), 28 (LEDs) y 34 (P2 A).
   Opciones (todas cambian `01` §3.3, que necesita autorización):
   - **Mover la lectura del pad del 25 a otro pin analógico** y usar el 25 como RX6 para MIDI. Requiere
     liberar un analógico: por ejemplo, B6 del 41 al 6 (libre si el Audio Shield no tiene la memoria
     opcional) y el pad al 41 (A17).
   - Usar MIDI solo por USB (host USB-A y USB de la Teensy) y sacar el jack TRS del panel trasero.
2. **Conector PCB-K ↔ PCB-A** (§6.1): placa a placa de 6.5 mm o cable corto.
3. **Choc soldados o en socket hot-swap.** El socket se monta en la cara inferior, donde también van los
   LEDs; hay que verificar que el socket y el LED no choquen bajo cada tecla.
4. **Buffer de entrada** (§8.1): recomendado.
5. **Acople de LINE OUT** del Audio Shield (§8): verificar el esquemático del shield Rev D.
6. **Módulo Freenove y cable DSI:** medirlos antes de cerrar los brazos de PCB-A y la posición de la Pi.

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
