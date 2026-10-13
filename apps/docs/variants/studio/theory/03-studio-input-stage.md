# Studio · Etapa de entrada y CV de la portadora — validación antes del pedido

> Theory doc · 2026-10-13 · Variante Studio (ADR-002 / ADR-004)
> Circuito: `apps/hardware/kicad/studio/portadora/` · Simulación: `apps/hardware/kicad/studio/sim/`
> Contexto: `02-studio-pcb.md` §8.1

La portadora la ensambla JLCPCB, así que la etapa de entrada no pasa por protoboard antes del pedido.
Este documento define **qué tiene que cumplir**, **cómo se valida sin hardware** (simulación SPICE) y
**qué pasa si aun así falla** (plan B en la placa).

---

## 1. El circuito (por canal; R entre paréntesis)

```
IN_L ─┬─ 32.4k (R4) ─┬─ 12.4k (R5) ─┬─ 5.23k (R6) ─ GND        escalera del pad (rev. §8)
      │              │ TAP10        │ TAP20
      └──────────────┴──────────────┴──► SW1 (0 / −10 / −20 dB) ─► PAD_L
PAD_L ─ 1 µF (C5) ─ 1k (R10) ─┬─ PROT_L ─► (+) U2A MCP6004, seguidor ─► BUF_L ─ 2.2 µF (C8) ─► LINE IN L
                              ├─ BAT54S (D2) a +3V3 y GND
                              └─ 100k (R12) ─ VREF

+3V3 ─ 10k (R14) ─┬─ 10k (R15) ─ GND        VREF_DIV, 10 µF (C10)
                  └─► (+) U2C, seguidor ─► VREF = 1.65 V

CV_PWM (pin 29) ─ 4.7k ─┬─ 4.7k ─┬─► (+) U2D, seguidor ─ 1k (R20) ─► CV OUT (J14)   (rev. §8)
                       100 nF    100 nF
```

U2 es un solo MCP6004, con 4 amplificadores operacionales: L, R, VREF y CV.

| Parte | Por qué está |
|---|---|
| Escalera del pad | Lleva fuentes calientes (+4 dBu, Eurorack) al rango del codec |
| C5 1 µF | Bloquea la DC de la fuente, porque la señal se re-centra en VREF |
| R10 1k + BAT54S | Protección: limita la corriente y los diodos recortan a ~0.3 V más allá de los rieles |
| R12 100k a VREF | Centra la señal en 1.65 V, a mitad de la alimentación de 3.3 V. Sin esto, el seguidor de una sola fuente recortaría los semiciclos negativos |
| Seguidor U2A | Impedancia alta hacia el pad y baja hacia el codec. Así el pad atenúa lo mismo sin importar qué carga ponga el SGTL5000 |
| C8 2.2 µF | Acople hacia el LINE IN, que tiene su propia polarización interna |
| U2C (VREF) | Fuente de 1.65 V de baja impedancia, compartida por L y R |
| RC ×2 + U2D | Filtra el PWM (≈36.6 kHz) hasta dejar una DC limpia de 0–3.3 V para el add-on del ladder (ADR-003) |

## 2. Niveles de referencia

| Fuente típica | Nivel | Vpp |
|---|---|---|
| Consumo (celular, interfaz −10) | −10 dBV = 0.316 Vrms | 0.89 |
| Synth / mixer de línea | 0 dBu = 0.775 Vrms | 2.19 |
| Profesional | +4 dBu = 1.228 Vrms | 3.47 |
| Eurorack | ±5 V | 10 |

El SGTL5000 tiene un fondo de escala medido por PJRC (comentario de `lineInLevel` en `control_sgtl5000.cpp`, Teensy
Audio Library oficial) de **3.12 Vpp con `lineInLevel(0)`** y **1.33 Vpp con `lineInLevel(5)`**. El
firmware usa el nivel 5.

> ⚠ **Discrepancias:**
> - El comentario de `audio_graph.cpp:86` dice que `lineInLevel(5)` = 0 dBu, pero 1.33 Vpp son
>   0.47 Vrms = −4.3 dBu.
> - En `02-studio-pcb.md` §8.1 dice que +4 dBu ≈ 4.4 Vpp; son 3.47 Vpp.
>
> Ninguna de las dos cambia el diseño. Se señalan aquí y no se corrigen sin confirmación.

El techo analógico del seguidor es de 1.65 V ± ~1.6 V, unos 3.2 Vpp, y coincide con el fondo de escala de
`lineInLevel(0)`. Por eso el diseño apunta a que **el codec sature antes que la parte analógica**: el
recorte digital es predecible, mientras que el de los diodos y el amplificador ensucia la señal.

## 3. Análisis a mano (lo que la simulación tiene que confirmar)

**Carga sobre el pad.** El seguidor tiene impedancia casi infinita, pero R10 + R12 (101 kΩ hasta VREF,
que en AC es tierra) sí cargan la toma elegida:

| Toma | Z de salida de la toma | Ideal | Con 101 kΩ de carga |
|---|---|---|---|
| −10 dB | 32.4k ∥ 14.95k = 10.2 kΩ | −10.0 dB | 101/(101+10.2) → **−10.8 dB** |
| −20 dB | 42.6k ∥ 4.75k = 4.27 kΩ | −20.0 dB | 101/(101+4.27) → **−20.4 dB** |

Es un error chico, pero sistemático, y se corrige recalculando la escalera con la carga incluida.

Subir R12 a 1 MΩ no conviene. La corriente de fuga en inversa del BAT54 (hasta ~2 µA según su datasheet)
por 1 MΩ desplazaría la polarización hasta ~2 V. Con 100 kΩ el desplazamiento queda en ≤ 0.2 V en el peor
caso, y típicamente es mucho menor.

**Pasa-altos.**
- C5 con ~110 kΩ: f_c ≈ 1.4 Hz.
- C8 con la entrada del codec: si esa entrada fuera de 10 kΩ, f_c ≈ 7 Hz y a 20 Hz caería ~0.5 dB. La
  impedancia de entrada del LINE IN no está confirmada (el `02` usa ~29 kΩ "a verificar"), así que se
  simula con 10 k, 29 k y 100 kΩ.

**Protección.** Con ±12 V de pico en la posición de 0 dB, por R10 circulan ≈ (12 − 3.6)/1k ≈ 8 mA de pico.
Esa corriente la toman los BAT54S (máximo 200 mA), no la entrada del MCP6004 (máximo ±2 mA, entre
VSS − 1.0 V y VDD + 1.0 V, según el datasheet de Microchip). El diodo del lado de +3V3 inyecta esa
corriente en el riel. La Teensy consume ~100 mA de ese riel, así que la absorbe sin que suba.

## 4. Criterios de paso

| # | Qué | Criterio |
|---|---|---|
| C1 | Niveles del pad, 1 kHz | Las tomas −10 y −20 dB, a ±0.5 dB de lo nominal |
| C2 | Respuesta en frecuencia | ±0.5 dB de 20 Hz a 20 kHz (con el codec a 29 kΩ). Con 10 kΩ, informar |
| C3 | Techo | En 0 dB, con 3.0 Vpp en LINE IN: THD < 0.1 % (sin recorte analógico antes del FS de `lineInLevel(0)`) |
| C4 | Sobretensión | ±12 V de pico en 0 dB. En la entrada del MCP6004, dentro de [VSS − 1.0, VDD + 1.0] V y ≤ 2 mA. En el BAT54S, < 200 mA |
| C5 | VREF | 1.65 V ± 2 % en reposo y con señal en los dos canales |
| C6 | CV OUT | Escalón de 0 a 100 % de duty: llega al 1 % del valor final en < 10 ms. Ripple del PWM < 1 mVpp |

## 5. Método

- **ngspice**, el que trae KiCad (`libngspice.0.dylib`), controlado desde Python con ctypes. No se instala
  nada.
- **Modelo del MCP6004:** modelo por parámetros del datasheet de Microchip:
  - GBW de 1 MHz;
  - slew rate de 0.6 V/µs;
  - A_OL de 112 dB;
  - salida a 25 mV de los rieles;
  - entrada en modo común de VSS − 0.3 a VDD + 0.3 V.

  Es suficiente para verificar niveles, ancho de banda, techo y protección. **No** es el macromodelo
  oficial: no reproduce el ruido real ni la THD fina del chip. El macro oficial de Microchip se puede
  agregar después, con aprobación de la descarga.
- **BAT54:** parámetros ajustados a su datasheet:
  - V_F ≤ 240 mV a 0.1 mA, ≤ 320 mV a 1 mA y ≤ 400 mV a 10 mA;
  - C_d ≤ 10 pF.

  Queda IS = 9 nA, N = 1, RS = 2 Ω.
- **Valores y topología, desde el esquemático:** el script exporta el netlist de la portadora con
  `kicad-cli`, comprueba que cada red del circuito simulado siga teniendo sus conexiones (incluidos
  JP2–JP5) y toma los valores de ahí. Si alguien cambia una resistencia o una red en KiCad, la simulación
  usa el valor nuevo o se detiene diciendo qué red cambió. **Correrlo antes de cada pedido.**
- **Fuente:** impedancia de 100 Ω.
- **Codec:** C8 seguido de R_in (10 k, 29 k o 100 kΩ, según el caso).

## 6. Plan B en la placa (si la etapa falla al llegar)

| Ref | Tipo | Une | Uso |
|---|---|---|---|
| JP2 / JP3 | Solder jumper, abierto | PROT_L → BUF_L / PROT_R → BUF_R | Saltar los seguidores |
| JP4 | Solder jumper, abierto | VREF_DIV → VREF | VREF sale del divisor directo |
| JP5 | Solder jumper, abierto | CV_FILT → CV_BUF | CV OUT sin buffer |
| TP1–TP5 | Test points | VREF, BUF_L, BUF_R, CV_BUF, GND | Diagnóstico con multímetro |

**Uso:** los jumpers se cierran **solo con U2 retirado** (o pedido como DNP). Con el chip montado, un
jumper cerrado conecta su salida directo a su entrada.

En modo bypass:
- el audio sigue funcionando;
- se pierde la independencia del pad respecto del codec (el pad vuelve a cargarse con la entrada del
  SGTL5000);
- la protección queda con la referencia del divisor (5 kΩ en vez de la salida del buffer).

**Diagnóstico al llegar la placa**, con el multímetro y sin señal:
- TP1 (VREF), TP2 (BUF_L) y TP3 (BUF_R) dan 1.65 V;
- TP4 (CV_BUF) sigue al duty del pin 29.

Si eso se cumple, la etapa está viva.

## 7. Qué NO cubre esta validación

- El ruido real y la THD fina del MCP6004 (para eso hace falta el macro oficial o una medición).
- La red interna del Audio Shield en LINE IN: si ya trae capacitor de acople, C8 queda en serie con él.
  Funciona igual, pero hay que verificarlo en el esquemático del shield Rev D.
- Layout, acoples y ESD de los jacks.
- Clones: en el BOM de JLC hay que pedir el **Microchip MCP6004-I/SL** original, no un "MCP600x" de otro
  fabricante. Las hojas de datos de los clones difieren, por ejemplo en los límites de entrada.

## 8. Resultados

Corrida del 2026-10-13. Tablas completas en `apps/hardware/kicad/studio/sim/results.md`.

**Primera corrida (valores de la rev 0.1): fallaron C1 y C6.**

| # | Rev 0.1 | Causa |
|---|---|---|
| C1 | −10.96 / −20.44 dB ❌ | La carga de R10 + R12 (101 kΩ) sobre la toma, como se predijo en §3 |
| C6 | 1 % en 11.0 ms ❌ | Dos polos RC de 10k/100 nF (τ = 1 ms cada uno) |

**Cambios aplicados en el esquemático:**
- R5/R8: 10.2k → **12.4k**;
- R6/R9: 4.75k → **5.23k** (R4/R7 siguen en 32.4k; búsqueda E96 con la carga incluida);
- R18/R19: 10k → **4.7k**.

**Segunda corrida: todo pasa.**

| # | Resultado | |
|---|---|---|
| C1 | −10.09 / −20.11 dB a 1 kHz | ✅ |
| C2 | +0 / −0.09 dB de 20 Hz a 20 kHz con R_in = 29 k (con 10 k: −0.56 dB a 20 Hz) | ✅ |
| C3 | 3.0 Vpp en LINE IN con 0.001 % de THD; recorte desde ~3.25 Vpp | ✅ |
| C4 | Con ±12 V: entrada del MCP6004 entre −0.38 y 3.68 V, corriente en el pin ~0, 9 mA por los BAT54S | ✅ |
| C5 | VREF = 1.650 V, sin ripple medible con señal | ✅ |
| C6 | 1 % en 5.2 ms; ripple de 0.35 mVpp a 50 % de duty | ✅ |

**Lecturas:**
- **El techo analógico (~3.25 Vpp) queda apenas por encima del fondo de escala de `lineInLevel(0)`
  (3.12 Vpp).** El codec satura primero, como se buscaba, pero el margen es de ~0.1 V: depende de que VREF
  esté centrada (R14/R15 al 1 %).
- **Con +4 dBu (3.47 Vpp) en la posición de 0 dB, recorta la parte analógica.** Las fuentes profesionales
  van en −10 dB. En −10 dB, +4 dBu llega como 1.1 Vpp: cabe en `lineInLevel(0)` con 9 dB de margen y en
  `lineInLevel(5)` (1.33 Vpp) con 1.6 dB.
- **Con ±12 V, el LINE IN recibe ±1.63 V alrededor de su polarización.** Es el recorte de los diodos más el
  riel del seguidor, y queda dentro de la alimentación del codec. Los 9 mA que entran al +3V3 los absorbe
  el consumo de la Teensy.
- **Si la entrada del codec resultara ser de ~10 kΩ,** subir C8/C9 a 4.7 µF deja los 20 Hz en −0.15 dB.
- **CV final de 3.24 V con 100 % de duty:** el riel del seguidor (−25 mV) más la caída en R20 contra 100 kΩ.
- **Lo que esto no prueba:** el ruido y la THD real del MCP6004 (§7). Para eso queda la medición con la
  placa, con el sketch de dBFS en LINE IN.

## Referencias

- Microchip, *MCP6001/1R/1U/2/4 — 1 MHz, Low-Power Op Amp*, DS20001733: tabla de máximos absolutos y
  características AC/DC.
- Nexperia / onsemi, *BAT54 series Schottky barrier diodes*: V_F, I_R y C_d.
- PJRC, Teensy Audio Library, `control_sgtl5000.cpp`: fondo de escala de `lineInLevel` y
  `lineOutLevel`, medidos.
- NXP, *SGTL5000 datasheet*: impedancia de entrada del LINE IN (pendiente de confirmar).
- D. Self, *Small Signal Audio Design*, 3.ª ed., cap. 2 (niveles de línea) y cap. 17 (entradas y
  protección).
