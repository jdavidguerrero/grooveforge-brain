# Studio · Presets del track "Run" (prueba de sonido)

> **Fecha:** 2026-10-25 · **Estado:** implementado en el banco de fábrica (`firmware-teensy/src/ui/preset_bank.cpp`) ·
> **Fuente:** tabla de 6 presets de dos engines que pasó Juan · **Depende de:** theory/06 (presets), theory/09 (GROOVE).

## 1. Por qué van partidos en dos

Los 6 presets son de **dos engines a la vez** (A + B con mezcla A/B). Las capas son la fase 3 de
`docs/theory/caracter-sonoro.md` y todavía no existen: hoy el Brain toca un engine por vez. Cada preset quedó partido en
sus dos mitades, cada una en el banco de su engine. Para el track: grabar cada mitad en su pista de Ableton (la misma
parte MIDI) y mezclarlas allá. Cuando lleguen las capas, cada par se junta en un preset híbrido.

| Preset | Mitad A (engine · nombre) | Mitad B (engine · nombre) | Mezcla A / B en Ableton |
|---|---|---|---|
| 01 GF_Run_FunkBass | MODEL D · JD RUN FUNKBASS | ARP 2600 · JD RUN BASS BITE | 75 / 25 → −2.5 dB / −12 dB |
| 02 GF_Run_HybridLead | DX7 · JD RUN PLUCK | PROPHET-5 · JD RUN LEAD BODY | 40 / 60 → −8 dB / −4.4 dB |
| 03 GF_Horizon_Pedal | OB-6 · JD HORIZON NOTCH | JUNO-106 · JD HORIZON JUNO | 65 / 35 → −3.7 dB / −9.1 dB |
| 04 GF_Disco_RapStab | JUNO-106 · JD RAP STAB JUNO | PROPHET-5 · JD RAP STAB P5 | 50 / 50 → −6 dB / −6 dB |
| 05 GF_Vocal_VelvetPad | JUNO-106 · JD VELVET JUNO | DX7 · JD VELVET RHODES | 60 / 40 → −4.4 dB / −8 dB |
| 06 GF_Tribal_ZapPerc | ARP 2600 · JD ZAP PERC ARP | DX7 · JD ZAP PERC WOOD | 60 / 40 → −4.4 dB / −8 dB |

Las macros de la tabla original son las de GROOVE: INDIE GRIT = **GRIT** (CC 15), ORGANIC DRIFT = **DRIFT** (CC 14),
HYBRID MORPH = **MORPH** (CC 17, sin efecto hasta las capas).

## 2. Qué no se pudo traducir tal cual

| Pedido | Qué quedó | Alternativa |
|---|---|---|
| ARP con HPF interno a 180 Hz (01) | El ARP no tiene HPF; la mitad B usa LP a 1.2 kHz | HPF 180 Hz en la pista de Ableton de JD RUN BASS BITE |
| Velocity → cutoff al 80 % en el Moog (01) | El Model D ignora la velocity, como el original | POCKET (CC 16) le devuelve la velocity, pero también acorta el release; si hace falta, un parámetro VEL → CUTOFF es trabajo nuevo |
| Mezcla A/B dentro del preset | Volumen en Ableton (tabla de arriba) | Fase de capas |
| DX7 de 2 operadores (02, 06) | Algoritmo 1 (4 → 3 → 2 → 1) con OP3 y OP4 en 0 | — |
| Ratio 1 : 3.14 (06) | Se guarda 3.14; al girar el encoder salta a pasos de 0.5 | — |
| Prophet: "Osc 2 +2 ST con sync" (02) | En el Prophet-5 la esclava del sync es la A: A a +2 ST, B a 0, POLY-MOD FILT ENV → FREQ A al 15 % | — |
| LFO del OB-6 a 0.15 Hz "un ciclo cada 2 compases" (03) | 0.15 Hz (≈ 3.5 compases a 126 BPM); 2 compases serían 0.26 Hz | Cambiar RATE en la página LFO si se quiere exacto |
| Notch del OB-6 (03) | FLT MORPH al 50 % (0 = LP, 50 = notch, 100 = HP) | — |
| Ataque de 12 ms del "Bwau" (04) | En el envelope del FILTRO (12 ms); el AMP arranca en 2 ms para que se oiga el barrido | — |
| "Resonance %" | Posición normalizada sobre el rango de resonancia de cada engine | — |
| Juno "PWM suave" (03) | PULSE con LFO → PWM al 50 % a 0.6 Hz | — |

## 3. Cómo se prueba

- Test native `test_presets_del_track`: los 12 presets existen en su banco y los valores clave (2 operadores del DX7,
  DRIFT 60 % del VELVET JUNO, cutoff y drive del RUN FUNKBASS) quedan como dice esta tabla.
- En el hardware: cargar cada mitad, tocar la parte MIDI del track y ajustar a oído; lo que se guarde se puede pasar
  después a estos valores de fábrica.
