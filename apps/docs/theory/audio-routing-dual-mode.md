# Audio Routing Dual-Mode — Theory & Design Recommendation

> **GrooveForge Brain — Theory Document**
> **Author:** Hardware Engineer (Claude, based on SGTL5000 Rev 6.0 datasheet + project specs)
> **Date:** Mayo 2026
> **Status:** Draft — para revision del founder antes de actualizar specs canónicos
> **Refs:** SGTL5000 datasheet Rev 6.0 (Freescale/NXP), `01-architecture.md` v0.3, `03-filter-design.md` v0.1

---

## Resumen ejecutivo

**Recomendación: Opción 1 — switch analógico externo TS3A5017 (o equivalente DPDT/dual-SPDT) controlado por GPIO 27 del Teensy.**

El SGTL5000 NO puede resolver el switcheo de fuente del `LINE_IN` solo con ruteo interno: su ADC puede seleccionar entre `LINE_IN` y `MIC_IN` por I2C (`CHIP_ANA_CTRL->SELECT_ADC`), pero esto no ayuda porque ambas fuentes (retorno del filtro 2N3904 y jack externo de FX) llegan al mismo pin `LINE_IN`. Se necesita un switch analógico externo que seleccione cuál señal física llega a `LINE_IN_L/R` del codec.

El bypass analógico interno del SGTL5000 (`LINE_IN → HP_OUT` vía `CHIP_ANA_CTRL->SELECT_HP`) resuelve por completo el path de salida en modo FX: `LINE_OUT` se alimenta siempre del DAC sin necesidad de switch adicional. Solo la entrada necesita el componente externo.

El impacto en BOM es mínimo: 1 CI adicional (~$0.35 en qty 100), SOIC-8 o SOT-23-8, controlado por el GPIO 27 que ya está libre.

---

## 1. Análisis de la matriz de ruteo interna del SGTL5000

### 1.1 Entradas analógicas

Del datasheet Rev 6.0, Tabla 1 (Pin Definitions) y Sección "Analog Input Block" (p. 15):

| Pin | Nombre | Tipo | Specs |
|---|---|---|---|
| LINEIN_R / LINEIN_L | LINE_IN estéreo | Diferencial / AC-coupled | 29 kΩ input impedance (Tabla 4/5), 0.57 Vrms típico (VDDA=1.8V), 1.0 Vrms típico (VDDA=3.3V) |
| MIC | MIC_IN mono | Single-ended | 2.9 kΩ input impedance, mismo nivel máximo que LINE_IN |

**Observaciones críticas:**

- `LINE_IN` es **estéreo** (L+R). `MIC_IN` es **mono** (un solo pin físico).
- Ambas entradas pasan por un bloque de ganancia analógica compartido (`Analog Gain 0 to 22.5 dB`, visible en Figure 2 y Figure 8 del datasheet) antes del ADC.
- El ADC es estéreo cuando usa `LINE_IN`, y opera en modo mono-left cuando usa `MIC_IN` (`CHIP_ANA_POWER->ADC_MONO = 0` para mono left-only).
- La impedancia de `LINE_IN` es 29 kΩ — compatible con la salida del filtro 2N3904 (target ~1 Vrms según `03-filter-design.md §2.1`) y con fuentes de audio externas line-level estándar.
- La impedancia de `MIC_IN` es **2.9 kΩ** — un factor 10x menor que `LINE_IN`. Conectar una fuente line-level (600 Ω source impedance típica de un mixer) a `MIC_IN` con pad resistivo cargará la fuente y degradará el SNR. Además es mono.

### 1.2 Selección de fuente del ADC por I2C

Del datasheet, **Table 26. CHIP_ANA_CTRL 0x0024**, bit 2:

```
SELECT_ADC  [bit 2]  RW  reset=0x0
  0x0 = Microphone (MIC_IN)
  0x1 = LINEIN
```

**Conclusión:** El ADC puede seleccionar su fuente entre `LINE_IN` y `MIC_IN` por I2C. Sin embargo, esto no resuelve el problema de enrutamiento porque **ambas fuentes externas del proyecto** (retorno del filtro 2N3904 y jack de audio externo) se conectarían físicamente al mismo par de pines `LINEIN_L/R`. El SGTL5000 no tiene un mux interno adicional dentro del path de `LINE_IN`.

### 1.3 Bypass analógico interno LINE_IN → HP_OUT

Del datasheet, **Table 26. CHIP_ANA_CTRL 0x0024**, bit 6:

```
SELECT_HP  [bit 6]  RW  reset=0x0
  0x0 = DAC (headphone recibe desde DAC)
  0x1 = LINEIN (headphone recibe desde LINE_IN, bypassing ADC y DAC)
```

Del texto funcional (p. 15, sección "Headphone"):

> "The line input is routed to the headphone output by writing `CHIP_ANA_CTRL->SELECT_HP`. This selection bypasses the ADC and audio switch and routes the line input directly to the headphone output to enable a very low power pass through."

Y del datasheet p. 14, nota explícita:

> "It should be noted that the **analog bypass from Line input to headphone output does not go through the audio switch**."

**Observaciones críticas:**

- El bypass `LINE_IN → HP_OUT` existe y es una ruta analógica directa, no digital.
- Esta ruta **solo aplica a HP_OUT**, no a `LINE_OUT`.
- `LINE_OUT` se alimenta **siempre desde el DAC** (`I2S_IN → DAC → LINE_OUT`), sin bypass analógico directo desde `LINE_IN`.
- En modo bypass, solo los controles de volumen analógico del headphone (`CHIP_ANA_HP_CTRL`) y su mute afectan la señal. El DAC volume control no afecta.

### 1.4 Audio Switch interno (CHIP_SSS_CTRL)

Del datasheet, **Table 20. CHIP_SSS_CTRL 0x000A** (p. 33):

```
DAC_SELECT  [bits 5:4]  RW  reset=0x1
  0x0 = ADC
  0x1 = I2S_IN  (default — DAC recibe desde Teensy I2S)
  0x2 = Reserved
  0x3 = DAP output

I2S_SELECT  [bits 1:0]  WO  reset=0x0
  0x0 = ADC  (I2S_DOUT envía salida del ADC al Teensy)
  0x1 = I2S_IN
  0x2 = Reserved
  0x3 = DAP output
```

El Audio Switch es un crossbar digital que conecta las fuentes digitales (ADC output, I2S_IN) a los destinos digitales (DAC, I2S_DOUT hacia Teensy, DAP). En operación normal del proyecto:
- `DAC_SELECT = 0x1`: Teensy → I2S → DAC → LINE_OUT / HP_OUT
- `I2S_SELECT = 0x0`: ADC (que captura el audio del filtro) → I2S_DOUT → Teensy

### 1.5 Conclusión del análisis

**El SGTL5000 NO puede resolver el switcheo del modo Synth↔FX solo con ruteo interno.**

El chip provee:
- Un mux ADC (`SELECT_ADC`) entre `LINE_IN` y `MIC_IN` — no útil porque el conflicto está en los pines físicos de `LINE_IN`.
- Un bypass analógico `LINE_IN → HP_OUT` — útil para el path de salida en modo FX (ver Sección 4), pero no resuelve la entrada.

Lo que falta es un **mux de señal externo** que decida qué fuente física llega a los pines `LINEIN_L/R`.

---

## 2. Diagrama de signal path completo

### 2.1 Modo A — Synth

```
Teensy (DSP)
  │
  │ I2S (pin 7 → SAI1 OUT1A)
  ▼
SGTL5000 DAC
  │
  │ LINEOUT_L/R (analog, ~1.0 Vrms @ VDDA=3.3V)
  ▼
[switch analógico externo — MUX posición A]
  │
  │ (retorno del filtro seleccionado)
  ▼
[Input buffer TL072] → [2N3904 ladder 4-pole 24dB/oct] → [Output buffer TL072]
  │
  │ [CD4066 software bypass / bypass path]
  ▼
ANALOG_SWITCH_OUT (~1 Vrms)
  │
  │ (fuente del jack de salida 1/4" L/R)
  ├──────────────────────────────► Jack 1/4" L/R OUT (audio filtrado al usuario)
  │
  │ (también vuelve al SGTL5000 para captura)
  ▼
[switch analógico externo — entrada LINEIN_L/R]
  │
  │ LINEIN_L/R (selección = Modo A, fuente = retorno filtro)
  ▼
SGTL5000 ADC
  │
  │ I2S_DOUT (pin 8 → SAI1 IN1)
  ▼
Teensy (captura USB Audio / monitoring)

Nota: En Modo Synth con filtro bypass activo (CD4066),
el path es: DAC → CD4066 → LINEIN → ADC → Teensy
```

**Registros SGTL5000 en Modo A:**
- `CHIP_ANA_CTRL->SELECT_ADC = 0x1` (LINEIN)
- `CHIP_ANA_CTRL->SELECT_HP = 0x0` (DAC → HP_OUT)
- `CHIP_SSS_CTRL->DAC_SELECT = 0x1` (I2S_IN → DAC)
- `CHIP_SSS_CTRL->I2S_SELECT = 0x0` (ADC → I2S_DOUT)
- GPIO 27 → LOW (switch analógico externo selecciona: retorno filtro → LINEIN)

### 2.2 Modo B — FX Processor

```
Jack 1/4" SEND del mixer externo (line-level, ~1 Vrms)
  │
  ▼
[switch analógico externo — MUX posición B]
  │
  │ LINEIN_L/R (selección = Modo B, fuente = jack externo)
  ▼
SGTL5000 ADC (stereo, 24-bit, SNR 90dB)
  │
  │ I2S_DOUT (pin 8 → Teensy)
  ▼
Teensy — aplica FX digitales (12 FX disponibles)
  │
  │ I2S (pin 7 → DAC)
  ▼
SGTL5000 DAC
  │
  ├──► HP_OUT (opcional: monitoring por headphone)
  │
  │ LINEOUT_L/R (~1.0 Vrms)
  ▼
Jack 1/4" RETURN del mixer externo
  (señal procesada con FX — filter 2N3904 NO está en el path)

Nota: El filtro 2N3904 queda fuera del path en Modo B.
El switch analógico externo tiene su posición B abierta hacia el filtro.
```

**Registros SGTL5000 en Modo B:**
- `CHIP_ANA_CTRL->SELECT_ADC = 0x1` (LINEIN — misma configuración)
- `CHIP_ANA_CTRL->SELECT_HP = 0x0` (DAC → HP_OUT para monitoring)
- `CHIP_SSS_CTRL->DAC_SELECT = 0x1` (I2S_IN → DAC)
- `CHIP_SSS_CTRL->I2S_SELECT = 0x0` (ADC → I2S_DOUT)
- GPIO 27 → HIGH (switch analógico externo selecciona: jack externo → LINEIN)

**Observación importante:** Los registros del SGTL5000 son idénticos en ambos modos. El cambio de modo es puramente por el estado del switch analógico externo (GPIO 27) y por el firmware del Teensy (qué DSP corre: engines vs FX chain).

---

## 3. Comparativa de opciones

### Opción 1 — Switch analógico externo DPDT (RECOMENDADA)

**Componente:** Texas Instruments TS3A5017 (SPDT triple, equivalente funcional a DPDT + 1) o NLAS4051 (SPST × 8) o simplemente un **74HC4052** (dual 4:1 mux, overkill) o más económico: **TS5A3159** (SPDT 1 canal, necesitarías 2 para estéreo).

La opción más limpia para estéreo es un **74HC4053** (triple SPDT, en DIP-16 o SOIC-16) o el **NLAS4053** (equivalente low-voltage):

```
74HC4053 — 3 canales SPDT independientes
  Canal A: LINEIN_L — selecciona entre [filtro retorno L] y [jack externo L]
  Canal B: LINEIN_R — selecciona entre [filtro retorno R] y [jack externo R]
  Canal C: libre para uso futuro (ej. ruteo de salida alternativo)
  Control: pin A (una línea de control para los 3 canales) → GPIO 27 Teensy
```

**Funcionamiento:**
- GPIO 27 = LOW → Modo Synth: LINEIN_L/R conectado al retorno del filtro 2N3904
- GPIO 27 = HIGH → Modo FX: LINEIN_L/R conectado al jack externo del mixer

**Ventajas:**
- Estéreo nativo — sin degradación de calidad vs mono
- THD+N del 74HC4053 @ Vcc=5V: typ. -70 dBc (suficiente para el target del proyecto)
- On-resistance: ~35 Ω típico — insignificante vs 29 kΩ de LINE_IN
- Sin cambio de configuración I2C en el cambio de modo
- Control simple: 1 GPIO, 1 bit
- JLCPCB SMT disponible (LCSC: ver Tabla BOM)
- Costo bajo: ~$0.35 qty 100

**Desventajas:**
- Componente adicional en BOM y footprint en PCB
- Switching pop/click posible (ver Sección 6 — mitigaciones)
- El 74HC4053 opera a VCC 2V–6V. A 3.3V (Teensy GPIO), Ron sube levemente (~50Ω) pero sigue siendo aceptable

**Alternativa de mayor calidad analógica:** Vishay DG2303 o Maxim MAX4544 (SPDT de precisión, Ron <10Ω, THD -80dBc). Costo: ~$0.80–1.20 qty 100. Justificado solo si los tests de THD muestran degradación con el 74HC4053.

### Opción 2 — MIC_IN con pad resistivo

**Funcionamiento:** El jack externo de FX se conecta al pin `MIC_IN`. En Modo FX, el firmware cambia `CHIP_ANA_CTRL->SELECT_ADC = 0x0` (Microphone). En Modo Synth, `SELECT_ADC = 0x1` (LINEIN, que recibe el retorno del filtro).

**Limitaciones críticas:**

1. **Mono:** `MIC_IN` es un solo pin físico. El modo FX Processor del producto es inspirado en el Roland RMX-1000, que trabaja con señal estéreo del insert send del mixer. Una entrada mono degrada el posicionamiento estéreo del audio procesado — incompatible con el posicionamiento boutique $599.

2. **Impedancia incompatible:** `MIC_IN` tiene 2.9 kΩ de impedancia de entrada vs 29 kΩ de `LINE_IN`. Un pad resistivo para adaptar el nivel (ej. -20dB para convertir 1 Vrms line → 100 mVrms mic-level) cargaría la fuente del mixer y no resuelve el problema de impedancia.

3. **SNR degradado:** El path `MIC_IN → ADC → I2S` tiene SNR 85 dB (VDDA=1.8V, Tabla 5) vs `LINEIN → ADC` con SNR 90 dB (VDDA=3.3V, Tabla 6). La diferencia es 5dB — audible en un contexto de producto boutique.

4. **Ganancia incorrecta:** MIC GAIN está en 0/20/30/40 dB steps. Para señal line-level se requeriría 0 dB, pero la ganancia de micrófono tiene más noise a ese setting (datasheet p. 40: "At 0 dB setting the THD can be slightly higher than other paths").

**Conclusión Opción 2:** Descartada. Mono + SNR inferior + impedancia incompatible son incompatibles con el target del producto.

### Opción 3 — Uso del bypass interno SELECT_HP + ruteo de salida alternativo

**Hipótesis:** Usar la ruta `LINE_IN → HP_OUT` interna del SGTL5000 para el Modo FX, y usar `LINEOUT` para el Modo Synth.

**Problema:** Esta ruta no resuelve el conflicto de entrada. El jack externo de FX aún necesita llegar a `LINE_IN`, y el retorno del filtro 2N3904 también necesita llegar a `LINE_IN`. El mux sigue siendo necesario.

**Uso correcto del SELECT_HP:** El `SELECT_HP` SÍ es útil para el path de monitoreo por headphone en Modo FX (permite escuchar la señal externa antes del procesamiento, útil para soundcheck). No resuelve el problema de routing de entrada.

---

## 4. Path de salida — análisis completo ambos modos

**Pregunta del brief:** ¿El LINE_OUT necesita switcheo? ¿O puede alimentarse siempre desde la misma fuente?

### Modo A (Synth)

La señal procesada por el filtro 2N3904 tiene dos destinos:
1. El jack físico 1/4" L/R de salida (audio filtrado audible para el usuario)
2. El retorno al SGTL5000 `LINE_IN` para captura ADC → Teensy (USB Audio / monitoring)

En la arquitectura propuesta, el retorno del filtro ya llega a `LINE_IN` por el switch (Modo A). Para la salida física al jack, la señal del filtro puede alimentarse directamente desde el output buffer del filtro — no necesita pasar por el SGTL5000 de nuevo antes del jack.

**Pregunta implícita:** ¿El SGTL5000 `LINE_OUT` alimenta los jacks de salida en Modo Synth?

Según el spec actual (`01-architecture.md` §2 y §3.1), los jacks de salida (`Audio jacks 1/4" TRS x2`) son "Line out balanced". En la arquitectura de audio path:

```
Teensy → I2S → SGTL5000 DAC → LINE_OUT → [filtro 2N3904] → jacks 1/4"
```

El `LINE_OUT` del SGTL5000 alimenta el input del filtro. La salida de audio al usuario sale del output del filtro, no de `LINE_OUT` directamente. Entonces los jacks de salida 1/4" en Modo A están alimentados por el output buffer del filtro (TL072).

En Modo B (FX Processor), el filtro no está en el path. El output al jack de retorno del mixer (RETURN) necesita venir del DAC del SGTL5000. Si los jacks 1/4" están conectados físicamente al output del filtro, entonces en Modo B no habría señal en esos jacks (el filtro no está procesando nada).

**Esto es una decisión de arquitectura que requiere confirmación del founder (ver Sección 7).**

Dos sub-opciones para el path de salida en Modo B:

**Sub-opción B1 (más simple):** Los jacks 1/4" se conectan directamente a `LINE_OUT` del SGTL5000 (no después del filtro). En Modo A, `LINE_OUT` es la entrada del filtro, pero la señal filtrada se escucha en los jacks a través de un tap diferente. En Modo B, `LINE_OUT` lleva la señal FX al jack. Desventaja: en Modo A el usuario escucha la señal SIN filtrar (solo la que sale de LINE_OUT).

**Sub-opción B2 (recomendada):** Los jacks 1/4" se conectan al output del filtro (TL072 output buffer). En Modo B, se añade un segundo switch (usando el canal C libre del 74HC4053) que bypasea el filtro y conecta `LINE_OUT` directamente al output buffer de salida → jack. En Modo A, el path va por el filtro como es hoy. El switch de bypass es diferente al CD4066 (que maneja el loop interno del SGTL5000).

La Sub-opción B2 es conceptualmente la más limpia pero requiere un segundo switch en el path de salida (el canal C del 74HC4053 ya disponible). El CD4066 sigue siendo el bypass del loop filtro↔SGTL5000 ADC. El canal C del 74HC4053 manejaría el tap de salida al jack.

**Conclusión path de salida:** Con el 74HC4053, el canal C disponible puede manejar el ruteo de salida al jack sin componente adicional. No se necesita un CI extra para la salida.

---

## 5. Recomendación final

**Componente:** **74HC4053** (triple SPDT analog mux/demux)
**Fabricante:** Texas Instruments, Nexperia, ON Semiconductor (múltiples fuentes)
**Package:** SOIC-16 (preferido para audio — menor Ron que TSSOP equivalente) o TSSOP-16 si espacio en PCB es crítico
**Control:** GPIO 27 del Teensy (ya libre según `01-architecture.md §3.3` línea 268)

**Configuración de canales:**

| Canal 74HC4053 | Pin control | Modo Synth (LOW) | Modo FX (HIGH) |
|---|---|---|---|
| Canal A (SPDT) | Pin A — GPIO 27 | Retorno filtro L → LINEIN_L | Jack externo L → LINEIN_L |
| Canal B (SPDT) | Pin A — GPIO 27 | Retorno filtro R → LINEIN_R | Jack externo R → LINEIN_R |
| Canal C (SPDT) | Pin A — GPIO 27 | Filtro output → Jack 1/4" out | LINE_OUT SGTL5000 → Jack 1/4" out |

Los tres canales comparten el mismo pin de control (pin A del 74HC4053), que se conecta a GPIO 27. Un solo bit cambia todo el ruteo de modo.

**Firmware:** El doble-push del ENC NAV que cambia de modo debe:
1. Mutear los outputs del SGTL5000 (`CHIP_ANA_CTRL->MUTE_LO = 1`, `CHIP_ANA_CTRL->MUTE_HP = 1`)
2. Cambiar GPIO 27
3. Esperar 1ms (tiempo de commutación del switch)
4. Desmutear

Este procedimiento elimina el pop/click de switching (ver Sección 6).

---

## 6. Riesgos eléctricos y mitigaciones

### 6.1 Pop/click al cambiar de modo

**Causa:** Al conmutar el switch analógico, la señal en el `LINEIN` cambia abruptamente. El SGTL5000 tiene AC coupling en su entrada (capacitores de series recomendados en la app note), lo que puede generar un transitorio.

**Mitigación principal:** Mute digital antes del switch, como se describió en la Sección 5. El SGTL5000 tiene Zero Cross Detect (ZCD) en el ADC (`CHIP_ANA_CTRL->EN_ZCD_ADC`), que puede usarse para sincronizar cambios de volumen con el cruce por cero — pero para el cambio de modo es más simple y robusto el mute explícito.

**Mitigación secundaria:** El 74HC4053 tiene tiempos de conmutación de ~10 ns (a 3.3V Vcc) — el transitorio eléctrico es mucho más corto que el tiempo de mute. El pop viene del transitorio de carga, no del switch en sí.

### 6.2 Crosstalk entre modos

**Causa:** Cuando el 74HC4053 está en Modo A, el pin del jack externo de FX queda en el estado "off" del switch. La atenuación del 74HC4053 en off-state es típicamente -70 dBc a 1kHz — suficiente para que el jack externo no se acople al path del filtro.

**Consideración adicional:** Si el jack externo de FX está desconectado en Modo A, el pin input del switch estará flotante o en GND (depende del jack con switched ground del BOM). Los jacks 1/4" del BOM son "Switched ground" (`01-architecture.md §3.1`), lo que significa que el pin activo se cortocircuita a GND cuando no hay plug. Esto es lo ideal — el input del switch en Modo A verá GND en el canal no seleccionado.

### 6.3 Niveles de señal — compatibilidad con LINEIN

Del datasheet, Tabla 6 (VDDA=3.3V): `LINEIN Input Level` típico = **1.0 Vrms**.

El output del filtro 2N3904 es ~1 Vrms según `03-filter-design.md §2.1`. Coincidencia exacta — sin atenuación ni ganancia adicional necesaria.

Los jacks de audio externo de un mixer típico (send de insert): 0 dBu = 0.775 Vrms. Esto está dentro del rango de `LINE_IN`. Para señales hot (+4 dBu profesional = 1.23 Vrms), está dentro del máximo absoluto de `LINE_IN` de 2.83 Vpp ≈ 1.0 Vrms RMS @ VDDA=3.3V. Recomendación: colocar un pad resistivo de -3dB a la entrada del jack externo para headroom adicional sin cambio de impedancia significativo (red de dos resistores: 330Ω en serie + 820Ω a GND, atenuación -3dB, impedancia resultante ~680Ω compatible con mixer pro).

### 6.4 On-resistance del switch y noise

El Ron del 74HC4053 a 3.3V es ~50Ω típico. Contra 29kΩ de LINEIN, forma un divisor que atenúa < 0.01 dB — completamente despreciable.

El noise del switch (shot noise del canal) es insignificante a estas impedancias de fuente.

### 6.5 Impedancia de source hacia el switch

El output buffer del filtro es un TL072 con impedancia de salida típica < 100 Ω @ 1kHz (opamp en feedback). Esto es compatible con el switch sin reflexiones ni pérdida de drive.

El mixer externo tiene típicamente 600Ω output impedance. También compatible.

### 6.6 DC offset en el switch

El 74HC4053 es un analog switch bilateral AC+DC. Las entradas de audio llevan acoplamiento AC (capacitores de serie) antes del switch para eliminar DC offset — verificar que los caps AC de acoplamiento del input del filtro y del jack externo estén antes del switch, no después. Esto también elimina cualquier DC offset que pudiera generar clicks.

---

## 7. Impacto en BOM

### Componente nuevo

| Componente | Función | Qty | Costo unit. qty 100 | Package | LCSC Part# |
|---|---|---|---|---|---|
| 74HC4053 (Nexperia o TI) | Triple SPDT analog switch — ruteo de modo dual | 1 | ~$0.35 | SOIC-16 | C6498 (Nexperia 74HC4053D) |

**Costo adicional por unidad: $0.35**
**BOM total filtro se incrementa de ~$5.40 a ~$5.75**
**BOM total sistema de ~$110 a ~$110.35** — impacto < 0.4%, dentro de margen.

### Pasivos adicionales (pad resistivo entrada FX)

| Componente | Valor | Qty | Costo | Notas |
|---|---|---|---|---|
| Resistor metal film 1% | 330Ω | 2 (L+R) | ~$0.02 | Serie en jack externo |
| Resistor metal film 1% | 820Ω | 2 (L+R) | ~$0.02 | Shunt a GND |

**Costo adicional pasivos: ~$0.04**

### Capacitores AC coupling (si no están ya en el diseño)

Los caps de acoplamiento AC en el LINEIN del SGTL5000 ya están previstos (el datasheet los menciona en el typical application). Si los caps están después del filtro (ya existen en el path del filtro 2N3904), no se necesitan adicionales. Verificar durante el schematic capture en KiCad.

---

## 8. Impacto en pin mapping

### Teensy 4.1

| Pin | Function actual | Cambio |
|---|---|---|
| 27 | Libre (TPDT eliminado) | **GPIO output — Mode Switch control → 74HC4053 pin A** |

**Entrada actualizada propuesta para `01-architecture.md §3.3`:**

```
| 27 | GPIO | Mode switch control (74HC4053 pin A) — LOW=Synth, HIGH=FX Processor |
```

No hay otros pins afectados. La cadena I2S, I2C, UART, WS2812B, encoders y botones quedan inalterados.

---

## 9. Decisiones que requieren confirmación del founder

Hay dos puntos que trascienden la decisión de hardware y requieren confirmación antes de actualizar los specs canónicos:

**Decisión 1 — Path de salida en Modo FX:**

¿Dónde está físicamente conectado el jack 1/4" de salida?

- Opción A (Sub-opción B2 recomendada): El jack sale del output buffer del filtro 2N3904. El canal C del 74HC4053 bypasea el filtro en Modo FX conectando LINE_OUT del SGTL5000 al jack. El CD4066 queda exclusivamente para el bypass del loop LINEIN↔ADC.
- Opción B (más simple, menos flexible): El jack sale directamente de LINE_OUT del SGTL5000 en ambos modos. En Modo Synth, LINE_OUT alimenta el filtro Y el jack (el usuario escucha la señal sin filtrar en el jack, mientras el filtro procesa — lo cual es incorrecto para un synth). Esta opción no es viable para Modo Synth.
- Opción C: Existen dos jacks físicos separados — uno al output del filtro (para Modo Synth) y uno a LINE_OUT del SGTL5000 (para Modo FX/RETURN del mixer). Esto evita el switch de salida pero implica un panel con más jacks. El BOM actual tiene "Audio jacks 1/4" TRS x2" — probablemente L/R, no Source A/Source B.

**Decisión 2 — El jack de input externo de FX (SEND del mixer):**

El BOM actual tiene `Audio jacks 3.5mm x2` ("Headphones + line in"). Si "line in" es el jack de audio externo para Modo FX, es 3.5mm TRS mono/estéreo. Un insert send de mixer profesional típicamente es 1/4" TRS. Si el target de usuario es DJ/productor con mixer DJ (Allen & Heath Xone, Pioneer DJM), el send suele ser 1/4" TRS o RCA. Confirmar si el 3.5mm jack es suficiente para el target de usuario, o si se necesita un jack 1/4" adicional/alternativo.

---

## 10. Resumen de registros SGTL5000 por modo

| Registro | Bit Field | Modo Synth | Modo FX | Notas |
|---|---|---|---|---|
| CHIP_ANA_CTRL (0x0024) | SELECT_ADC [2] | 0x1 (LINEIN) | 0x1 (LINEIN) | Igual ambos modos |
| CHIP_ANA_CTRL (0x0024) | SELECT_HP [6] | 0x0 (DAC) | 0x0 (DAC) | Igual — HP monitoring del DAC siempre |
| CHIP_ANA_CTRL (0x0024) | MUTE_LO [8] | 0x0 (unmute) | 0x0 (unmute) | Solo mute durante transición de modo |
| CHIP_SSS_CTRL (0x000A) | DAC_SELECT [5:4] | 0x1 (I2S_IN) | 0x1 (I2S_IN) | Igual ambos modos |
| CHIP_SSS_CTRL (0x000A) | I2S_SELECT [1:0] | 0x0 (ADC) | 0x0 (ADC) | Igual ambos modos |
| GPIO Teensy 27 | — | LOW | HIGH | Controla 74HC4053 |

**Conclusión clave:** El firmware no necesita reconfigurar el SGTL5000 al cambiar de modo (excepto el mute transiente). Todo el cambio de modo se reduce a un bit de GPIO. Esto simplifica el firmware y elimina posibles glitches por reconfiguracion de registros I2C durante el cambio.

---

*End of audio-routing-dual-mode.md*
*GrooveForge Brain — Theory Document*
*Basado en SGTL5000 datasheet Rev 6.0 (Freescale/NXP, 11/2013)*
*Specs del proyecto: 01-architecture.md v0.3, 03-filter-design.md v0.1*
