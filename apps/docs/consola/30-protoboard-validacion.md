# 30 · Validación en protoboard antes de pedir PCB-M

2026-11-12. La parte digital de PCB-M (Teensy, Audio Shield, ADS1115, MCP23017) ya corre en el prototipo de mesa con
el firmware `consola_uno`. Lo que se fabrica por primera vez sin haber funcionado nunca es el **frente analógico**,
las **salidas CV** y el **MIDI**. Este doc los prueba en protoboard con los mismos valores del esquema
(`kicad/consola/tools/schgen/pcb_m.py`, hojas 02, 05 y 07), para no descubrir un error en la placa ensamblada.

## 1. Piezas (versión de agujero pasante de lo que va en PCB-M)

| Pieza | Cant. | En PCB-M | Para protoboard | Nota |
|---|---:|---|---|---|
| Amplificador operacional cuádruple rail-to-rail | 1–2 | MCP6004 (SOIC) | **MCP6004-I/P** (DIP-14) | Alternativa: 2 × MCP6002 (DIP-8). No usar LM324/TL072: no llegan a los rieles con 3,3 V |
| Optoacoplador (solo para C, opcional) | 1 | 6N138 | **6N138** (DIP-8) | Común en tiendas locales |
| DAC I2C | 1 | MCP4725 | **Módulo MCP4725** (CJMCU-4725 o Adafruit) | Dirección del módulo: 0x60 o 0x62 según el puente A0; anotarla |
| Diodo Schottky | 2 | BAT54S | **BAT43, BAT85 o 1N5819** | Recorte de protección a los rieles |
| Diodo de señal | 1 | 1N4148W | **1N4148** | Inverso en la entrada MIDI |
| Resistencias ¼ W | — | — | 33 k, 12 k, 5,1 k, 1 k, 100 k, 10 k ×4, 20 k, 47 k, 220 Ω, 4,7 k, 100 Ω, 47 Ω ×2 (o 33 Ω y 10 Ω, D12) | Un surtido de ¼ W sirve |
| Condensadores | — | — | 1 µF, 4,7 µF, 10 µF ×3, 100 nF ×4 | Electrolíticos o cerámicos; respetar polaridad de los electrolíticos |
| Jacks | 2–3 | Pared trasera | 1 × 6,35 TRS (LINE IN), 2 × 3,5 TRS (MIDI IN/OUT) | Los mismos que se comprarán para la consola |
| Cable MIDI TRS tipo A o adaptador DIN ↔ TRS | 1 | — | — | Para conectar un equipo MIDI real |

Ya disponibles: Teensy 4.1 + Audio Shield con `consola_uno`, protoboard y cables, multímetro, la Mac con Ableton.

## 2. Pruebas

Alimentar desde la Teensy (3,3 V y 5 V de VUSB). Probar **un solo canal** (izquierdo) del frente analógico: el otro es
idéntico.

### A. Entrada de línea (hoja 02)

Circuito: jack → divisor de pad 33 k / 12 k / 5,1 k (tomas 0, −10, −20 dB) → 1 µF → 1 k → nodo con 100 k a VREF y
recorte Schottky a 3,3 V y AGND → seguidor MCP6004 → 4,7 µF → LINE IN L del Audio Shield. VREF = divisor 10 k / 10 k
de 3,3 V con 10 µF, bufferado con otra sección del MCP6004.

| Medida | Cómo | Pasa si |
|---|---|---|
| VREF | Multímetro | 1,60–1,70 V |
| Reposo de la salida del seguidor | Multímetro, sin señal | ≈ VREF |
| Ganancia de cada toma de pad | Tono de 1 kHz desde la Mac por la interfaz → LINE IN; grabar el canal USB de la Teensy en Ableton | 0, ≈ −9 y ≈ −20 dB respecto a la toma 0 |
| Recorte | Subir el nivel hasta recortar | Recorta limpio, simétrico; la Teensy no se reinicia |
| Ruido | Entrada en corto, grabar 30 s | Sin zumbido de 60 Hz ni silbido nuevo frente a la entrada directa del shield |

### B. Salida CV (hoja 05)

Circuito: módulo MCP4725 (VOUT 0–3,3 V) → MCP6004 no inversor, ganancia 1 + 10 k/20 k = 1,5 → 0–4,95 V. El
amplificador se alimenta de **5 V**.

| Medida | Cómo | Pasa si |
|---|---|---|
| Rango | Escribir 0, 2048 y 4095 al DAC (comando de banco o sketch) | ≈ 0 V, ≈ 2,47 V, ≈ 4,9 V |
| Linealidad | 5 puntos intermedios | Error < 1 % |
| Ruido del riel | Alimentar el amplificador desde el mismo 5 V que los LEDs encendidos (NeoTrellis o tira) y medir la salida en CA | Si el rizado es visible (> 5 mV), agregar filtro RC 10 Ω + 47 µF o regulador al riel del amplificador en PCB-M |

### C. MIDI (hoja 07) — opcional

**2026-11-12: el MIDI por hardware queda sin poblar en PCB-M** (Juan): MIDI va por un conversor USB conectado a la
Pi. Esta prueba solo hace falta si algún día se puebla el bloque; no bloquea el pedido.


MIDI IN (TRS tipo A): anillo → 220 Ω → LED del 6N138; punta → cátodo; 1N4148 inverso en paralelo; base a GND con
10 k; salida con pull-up de 4,7 k a 3,3 V → RX de la Teensy. MIDI OUT: TX → resistencia → punta; 3,3 V → resistencia
→ anillo (33 Ω y 10 Ω según D12).

| Medida | Cómo | Pasa si |
|---|---|---|
| MIDI IN | Tocar desde un teclado o interfaz MIDI | Notas y CC llegan sin pérdidas a 31 250 baud (monitor serie o MIDI de la consola) |
| Ráfaga | Mandar un clock MIDI + acordes rápidos | Sin notas colgadas |
| MIDI OUT | Conectar a una interfaz MIDI de la Mac | Las notas que envía la Teensy llegan a Ableton |
| Polaridad TRS | Probar con el cable tipo A | Funciona; si no, el equipo es tipo B: documentar |

## 3. Resultado

Anotar cada medida en `mediciones/protoboard-<fecha>.md`. Cualquier cambio de valor o de circuito se pasa a
`pcb_m.py` antes del layout. **PCB-M no se pide hasta que A y B pasen** (C solo si se puebla el MIDI).
