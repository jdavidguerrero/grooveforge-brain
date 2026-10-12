# Barrido de specs — Studio (2026-10-12)

> Qué quedó desactualizado después de las decisiones de octubre: panel de 5 encoders + 6 botones +
> pots IN/VOL (ADR-004), filtro add-on (ADR-003), cabeza Pi + GUI (ADR-002), DX7 de 4 operadores
> (ADR-005) y lo que salió de la GUI y del enclosure validado.
>
> **Regla:** `00–06` no se editan sin autorización explícita. Cada fila dice qué cambiar y quién
> lo autoriza.
>
> **Aplicado el 2026-10-12** (autorizado: "fusionar y aplicar todo"): fusión de `feat/studio-head`;
> `01` §header/3.1/3.2/3.4/3.6/4.1; `02` §2.10; `05` §1.13; `06` §10B; ADR-002 rev. 1, ADR-004 y
> ADR-005 (acciones); theory doc de la cabeza; `CLAUDE.md` raíz, de firmware y de hardware. Quedan
> abiertas las decisiones de §6 (van al theory doc del PCB). Al revisar `05` contra `01` §3.4 apareció
> una diferencia en la GUI (ENC R de Cymatic y Phase Chorus): se corrigió el catálogo de `apps/studio-head`.

---

## 0. Antes de todo: las ramas

`feat/studio-head` (7 commits) **no está fusionada a `main`**. Ahí viven el ADR-005, el diseño de
Claude Design actualizado (OSC/LFO, splash), el prompt OSC/LFO y la §5.4 del theory doc de la
cabeza. `feat/studio-controls` (sketch46) sale de `main`, así que no los ve.

**Propuesta:** fusionar `feat/studio-head` a `main` antes de corregir specs, para que las
correcciones partan de un solo estado.

## 1. Panel de controles (ADR-004)

| Dónde | Estado | Qué cambiar |
|---|---|---|
| `01` §3.3 pines | ✅ al día (P1, P2, B5, B6, IN 24, VOL 22, pad 25, CV 29/33) | — |
| `01` §3.4 controles (tabla ENC L/R/NAV/B1–B4) | 🟡 tabla del comercial con una nota de Studio | Agregar filas de P1, P2, B5, B6, IN y la tabla de mapeo por modo del ADR-004 |
| `01` §3.1 BOM | 🔴 solo comercial: EC11 ×3, Choc ×4, WS2812B-2020 ×16, pot de volumen | Agregar una BOM de Studio: EC11 ×5, Choc V2 ×6, LEDs ×18, RV09 ×2 (IN, VOL), palanca de pad de 3 posiciones, jacks del panel trasero |
| `01` §3.1 BOM y enclosure | 🔴 **contradicción**: el BOM dice WS2812B-2020; el enclosure validado dice SK6812-MINI-E | Decidir el LED (ver §6). Afecta al pedido de JLCPCB |
| Cantidad de LEDs | 🔴 **contradicción**: ADR-004 y `01` §3.3 dicen 18 (12 del anillo + 6 keycaps); el enclosure dice "PCB-K: 6 Choc + 16 LED" | Aclarar en el theory doc del PCB qué placa lleva el anillo de NAV |
| `01` §3.6 Top Panel PCB ("~130×25 mm, ENC1, ENC2, NAV, B1–B4", 2-PCB approach) | 🔴 solo comercial | Describir las placas de Studio: PCB-K, PCB-A, 2× PCB-V, portadora, alimentación (enclosure) |
| `01` §3.2 alimentación (LiPo + IP5306) | 🟡 Studio no usa batería: USB-C 5 V 3 A, polyfuse, TVS y puente VUSB–VIN cortado | Sumar la alimentación de Studio |
| ADR-004 · acciones | 🟡 3 (iteración de UI) y 4 (validación del enclosure) están hechas; 1 (firmware 5 enc + 6 botones) está a medias (sketch46) | Marcarlas |
| `apps/firmware-teensy/CLAUDE.md` | 🔴 dice "GPIO 25 → CD4066 bypass, GPIO 27 lee TPDT" | Pin 25 = pad (ADR-004), 27 = 74HC4053; no necesita autorización de specs |
| `apps/hardware/CLAUDE.md` | 🔴 estructura KiCad del comercial (hoja filter-ladder, main 4 capas 180×100) | Sumar el proyecto de Studio y la regla de ensamblado JLCPCB |

## 2. Cabeza Studio (ADR-002) y la Pi

| Dónde | Estado | Qué cambiar |
|---|---|---|
| ADR-002 | 🔴 9 menciones de "Pi 2B": título, decisión, criterios en la 2B | Pi 3 Model B v1.2 (1.2 GHz), `disable-bt` para el UART PL011, mediciones: 30 fps, 23–40 % de un núcleo. **Pendiente de tu ok (dijiste que lo autorizabas tras probar)** |
| Theory doc de la cabeza | 🔴 15 menciones de 2B (UART "la 2B no tiene Bluetooth", §11 medido en la 2B) | Igual que arriba; §8 ya está implementado en `deploy/` (sin sd_notify ni watchdog) |
| Theory doc de la cabeza | 🟡 `config.txt` dice "el overlay que indique Freenove" | Es `dtoverlay=vc4-kms-dsi-7inch` (medido en la 3B; la autodetección no funciona) |
| `01` header | 🟡 "cabeza Raspberry Pi" sin modelo | Mencionar la 3B |

## 3. GUI de Studio vs. protocolo

La GUI ya dibuja datos que **el bridge no tiene definidos**. Hoy los inventa el modo demo.

| Dato que pide la GUI | ¿Está en `02`? | Qué falta |
|---|---|---|
| Espectro de 32 bandas (PERFORM, SPECTRUM) | 🔴 "frame propuesto, ADR-002" | Comando y formato |
| Medidor IN L/R + clip, pad, ganancia IN | 🔴 no | Comando periódico de entrada |
| FX MAIN: flujos de ONDA (480 B), ESPECTRO (64 B) y FIRMA por FX | 🔴 no | Comandos por vista; el diseño FX Viz §E ya define tamaños y tasas |
| FX: slot, on/off, HOLD, ruteo, DRY/WET + 3 parámetros por slot | 🟡 en parte (sprint 36) | Revisar contra la GUI |
| OSC: plantilla por engine (`stops`, `slots` con nombre / unidad / rango) | 🔴 no | Hoy es una tabla estática en la cabeza; decidir si viaja por el bridge |
| DX7: ratio / level por operador, algoritmo, feedback | 🔴 no (y el engine es de 2 operadores) | Depende del engine de ADR-005 |
| LFO: forma, rate, sync, división, profundidad por destino, fase a 30 Hz | 🔴 no (el LFO no existe en el firmware) | Spec del LFO antes del comando |
| Presets: banco del engine, cargado, guardar | 🟡 revisar | — |
| Ajustes (MIDI, clock, calibración del CV) | 🔴 no hay spec | ADR-006 propuesto |

**Propuesta:** una sección nueva en `02` (o un anexo de Studio) con estos comandos, escrita
cuando toque la tarea 38 (hilo de I/O). Hoy alcanza con dejar la lista.

## 4. Engines y FX

| Dónde | Estado | Qué cambiar |
|---|---|---|
| `01` §4.1 engines | 🔴 DX7 = "AudioSynthWaveformPWM x6"; OB-6, DX7 y ARP figuran como updates futuros, pero los 6 están implementados | DX7 de 4 operadores con objeto propio (ADR-005); estado real de cada engine. **Pendiente de tu ok** |
| `01` §4.1 polifonía | 🟡 Juno 8 / Moog 6 | Firmware: Moog mono, Juno 6 (la auditoría de mayo ya lo marcó) |
| `05` | 🟡 lista los parámetros de cada FX pero no cuáles van en ENC R / P1 / P2 (acción 2 del ADR-004) | La GUI usa los tres primeros "Parámetros user" sin Mix; formalizarlo |
| LFO de los engines | 🔴 no está en ningún spec; PERFORM P2 ("LFO→CUTOFF") no mueve nada | Theory doc del LFO antes del firmware |

## 5. Estrategia y roadmap

| Dónde | Estado | Qué cambiar |
|---|---|---|
| `00` | 🟢 tiene el aviso de ADR-003 y dice que Studio no se comercializa | — |
| `06` roadmap | 🔴 no menciona Studio ni la cabeza Pi | Sumar el track de Studio (cabeza, panel, PCB) sin mover las fases del comercial |
| `CLAUDE.md` raíz | 🟡 el mapa de specs lista ADR-002/003/004 | Sumar ADR-005 (y ADR-006 si se aprueba) |

## 6. Decisiones abiertas para el PCB (van al theory doc)

1. **LED:** SK6812-MINI-E (montaje invertido, ideal para iluminar el keycap a través del PCB) o
   WS2812B-2020. Si JLCPCB ensambla, los dos se pueden; el MINI-E es el que pide el enclosure.
2. **Reparto de placas:** el enclosure pide 6 (PCB-K, PCB-A, 2× PCB-V, portadora, alimentación) y
   sugiere simplificar (ítem 7 de su revisión).
3. **Codec:** Audio Shield como módulo sobre la portadora (lo que asume el enclosure) o SGTL5000
   en la placa (lo que dice el BOM del comercial).
4. **Capas:** paneles en 2 capas; portadora en 2 o 4 (el audio está en el Audio Shield, que ya trae
   su propia placa).
5. **Presupuesto de corriente:** Pi 3B (hasta ~2.5 A en picos) + Teensy + pantalla + 18 LEDs
   (hasta ~1 A en blanco pleno) supera los 3 A del USB-C: el firmware tiene que limitar el brillo
   de los LEDs.
