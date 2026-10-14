# Resultados · etapa de entrada y CV de la portadora

Generado por `input_stage_sim.py` (ngspice de KiCad; MCP6004 por parámetros del datasheet, no el macro oficial).
Valores leídos del esquemático de la portadora (topología verificada): escalera 33k / 12k / 5.1k · C5 1uF · R10 1k · R12 100k · C8 4.7uF · RC del CV 4.7k / 100nF ×2.

## C1 · Niveles del pad (1 kHz) y C2 · Respuesta 20 Hz–20 kHz

| Pad | R_in codec | Ganancia @1 kHz | Error vs nominal | 20 Hz | 20 kHz | Plano ±0.5 dB |
|---|---|---|---|---|---|---|
| 0 dB | 10 k | -0.11 dB | -0.11 dB | -0.15 dB | -0.00 dB | sí |
| 0 dB | 29 k | -0.11 dB | -0.11 dB | -0.04 dB | -0.00 dB | sí |
| 0 dB | 100 k | -0.11 dB | -0.11 dB | -0.03 dB | -0.00 dB | sí |
| 10 dB | 10 k | -10.36 dB | -0.36 dB | -0.14 dB | -0.00 dB | sí |
| 10 dB | 29 k | -10.36 dB | -0.36 dB | -0.04 dB | -0.00 dB | sí |
| 10 dB | 100 k | -10.36 dB | -0.36 dB | -0.02 dB | -0.00 dB | sí |
| 20 dB | 10 k | -20.33 dB | -0.33 dB | -0.15 dB | -0.00 dB | sí |
| 20 dB | 29 k | -20.33 dB | -0.33 dB | -0.04 dB | -0.00 dB | sí |
| 20 dB | 100 k | -20.33 dB | -0.33 dB | -0.03 dB | -0.00 dB | sí |

**C1 ✅** (tomas −10/−20 a ±0.5 dB) · **C2 ✅** (±0.5 dB con R_in = 29 k)

## C3 · Techo (pad en 0 dB, 1 kHz, R_in = 29 k)

| Entrada (Vpp) | Salida en LINE IN (Vpp) | THD |
|---|---|---|
| 2.00 | 1.97 | 0.001 % |
| 2.60 | 2.57 | 0.001 % |
| 3.00 | 2.96 | 0.001 % |
| 3.12 | 3.08 | 0.001 % |
| 3.20 | 3.16 | 0.001 % |
| 3.30 | 3.25 | 0.060 % |
| 3.50 | 3.25 | 2.393 % |
| 4.00 | 3.25 | 7.909 % |

**C3 ✅** (3.0 Vpp sin recorte, THD < 0.1 %)

## C4 · Sobretensión (±12 V pico, 1 kHz, pad en 0 dB)

| Qué | Valor | Límite | |
|---|---|---|---|
| Entrada del MCP6004 (min / max) | -0.38 V / 3.68 V | [−1.0, 4.3] V | ✅ |
| Corriente en la entrada del MCP6004 (pico) | 0.0 µA | ≤ 2 mA | ✅ |
| Corriente por R10 / BAT54S (pico) | 8.98 mA | < 200 mA | ✅ |
| Corriente inyectada al +3V3 (pico) | 8.96 mA | informativo | ✅ |
| Salida en LINE IN (min / max) | -1.63 V / 1.62 V | informativo | ✅ |

**C4 ✅**

## C5 · VREF

- En reposo: VREF = 1.6500 V; nodo PROT = 1.6500 V
- Con 3 Vpp en L: VREF entre 1.6500 y 1.6500 V (ripple 0.00 mVpp)

**C5 ✅** (1.65 V ± 2 %)

## C6 · CV OUT (PWM 36.6 kHz → RC 4.7k/100nF ×2 → seguidor)

- Escalón de 0 → 100 % de duty: final 3.241 V; al 1 % en **5.2 ms** ✅ (< 10 ms)
- Ripple con 50 % de duty: **348 µVpp** ✅ (< 1 mVpp); media 1.633 V

**C6 ✅**
