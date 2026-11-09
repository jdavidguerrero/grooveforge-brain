# Consola: Teensy S · firmware E4

> **Fecha:** 2026-11-09. **Estado:** implementación de prototipo, sin flash ni mediciones físicas. Alcance autorizado: `03-arquitectura-capas.md` §4.1. No modifica los specs SSoT.

## 1. Contrato y decisiones

S recoge controles y envía MIDI USB estándar al Pi. Firmware custom C++17, sin Teensy Audio Library ni audio. El entorno GroovePush conserva fuentes, dependencias y comportamiento. E4 del plan antiguo habla de una Teensy y NeoTrellis I2C: la tarea y §4.1 posteriores autorizan dos Teensy y M4 por UART. La nota de aprobación CDC pendiente en §2 quedó superada por la enmienda de `02-bridge-protocol.md` §1.3.

### Pines de S

| Uso | Pines / dirección |
|---|---|
| Encoders 1–8 A/B | 2/3, 4/5, 9/10, 11/12, 22/24, 26/30, 31/32, 34/35 |
| Wire SDA/SCL | 18/19, 400 kHz; todo a 3,3 V y tierra común |
| MCP #1/#2 | 0x20/0x21; 14 entradas cada uno: 0–6, 8–14; nunca 7/15 |
| M4 Serial5 TX/RX | 20/21, 1 Mbaud; framing GroovePush existente |
| FSR velocity 1–4 | 14/A0, 15/A1, 16/A2, 17/A3, liberados al mover faders al ADS |
| MASTER | 23/A9 |
| CUE / IN | 38/A14 y 39/A15, cables directos autorizados: protoboard sin portadora; la PCB final debe rutearlos |
| ADS #1 | 0x48, canales 0–3: faders 1–4 |
| ADS #2 | 0x49, canales 0–1: expresión 1–2; 2–3: Sharp GP2Y0A41 manos 1–2 |
| LEDs | 28 reservado, sin animaciones locales en S |
| Reserva S↔A | Serial2 RX/TX 7/8, sync 37 (cable directo; el 13 es el LED de la placa); sin `begin` ni pulsos |
| MIDI DIN reservado | Serial1 RX/TX 0/1 (USB reemplaza UART al Pi); sin driver DIN |
| Pedal / triggers reservados | Pedal 6; triggers 25/A11, 27/A13, 40/A16, 41/A17; MCP se sondea sin INT. Sin acondicionamiento ni lectura |

**Actualización autorizada:** este prototipo usa protoboard sin portadora. Las marcas NC del mapa GroovePush no limitan el cableado directo; la nueva placa debe sacar CUE/IN y las reservas anteriores. Se mantienen intactos encoders, Wire, M4 y LED 28. Las reservas son de función, no implementaciones de conectores ni protección.

## 2. Captura y teoría

FSR: 3,3 V → FSR 402 → ADC → ≈10 kΩ → GND. ADC nativo de 12 bits. Cada pad tiene umbral inicial 160, máximo 3000 y liberación 100 (ajustar individualmente con su actuador). Se muestrea el grupo cada 400 µs (objetivo 2,5 kHz/pad). Al cruzar umbral, se captura el máximo durante 3000 µs (Juan, 2026-11-09: de 4 a 3 ms para dejar margen al objetivo de <5 ms). `velocity = 1 + 126 × (pico − umbral)/(máximo − umbral)`, saturada 1–127. El pico evita que la primera muestra mida solo el inicio del ataque; el ADS no alcanza para los cuatro pads rápidos. Una sola nota por golpe; al caer a liberación se envía note-off. No se rearma hasta liberar y cumplir 12 ms desde el ataque. Si se libera dentro de la ventana se emite on al cerrarla y off en la muestra siguiente. Resta de tiempos unsigned tolera wrap de micros.

El ADC se calibra en setup; en ISR se leen los registros ADC1 directamente (canales 7/8/12/11 para pines 14–17), evitando los callbacks de yield de analogRead. El escaneo FSR va en un IntervalTimer; solo ADC y máquina de estados dentro de la interrupción. Una cola fija lleva eventos a loop: MIDI, I2C y texto nunca corren en ISR. Overflow y separación máxima entre scans son visibles en STATS. Esto permite verificar el requisito de ≥2 kHz incluso con I2C ocupado; hay que medirlo en placa.

ADS: driver pequeño custom de registros sobre Wire, sin dependencia ADS adicional. Single-shot a 860 SPS, PGA ±4,096 V; se inicia conversión y se vuelve al loop. Después de 1300 µs se comprueba OS y se lee solo si terminó. Alterna canales por chip y transacciones entre chips; ausentes o fallos se reintentan con backoff. Rango útil 0–26400 (≈3,3 V). Alternativa Adafruit ADS1X15: API conocida, pero agrega dependencia y no hace falta para tres registros. Los tres pots se leen a 1 kHz fuera de ISR; MASTER comparte ADC1 con FSR y protege solo su conversión. CUE/IN usan ADC2 y no necesitan enmascarar la ISR. Wire no espera conversiones ADS, pero su driver PJRC tiene timeouts síncronos ante fallos de bus (hasta decenas de ms); el timer sigue escaneando y STATS registra el impacto en loop. Filtro EMA α=1/4, deadband de 2 unidades MIDI respecto al último envío, con llegada a extremos 0/127; primera lectura comunica estado inicial.

Encoders: cuatro transiciones por detent; CC relativo complemento a dos de 7 bits (1 = +1, 127 = −1), acumulación conserva fracciones. Botones pull-up y debounce estable de 5 ms, lectura por banco de 16 bits; cada loop atiende un chip cada 1 ms. Las 28 entradas se exponen: 8 clics, 20 funciones; el firmware no interpreta SHIFT ni acciones del motor.

### Manos Sharp (GUI §3)

Los GP2Y0A41 se alimentan a 5 V con ≥10 µF local por sensor y tierra común; ADS a 3,3 V. El firmware no confunde sus actualizaciones físicas (~16,5 ms según Sharp) con la salida de control elegida de **25 Hz**. En ADS #2 se escanean los cuatro canales, pero cada mano procesa una muestra cada ≥40 ms; EMA α=1/4 y deadband de 2 CC atenúan ruido y tráfico.

Calibración inicial por sensor: 30 cm ≈0,4 V → 3200 cuentas ADS; 4 cm ≈2,65 V → 21200 cuentas, PGA ±4,096 V (125 µV/cuenta). Son valores iniciales de la curva típica, no una medida precisa de centímetros: medir ambos extremos con la mano y ajustar. CC = 0 lejos, 127 cerca, interpolación lineal en voltaje filtrado (no lineal en distancia). El Pi convierte a macro/escala; no se generan notas theremin aquí.

Presencia entra dentro de esos extremos; permanece hasta salir de [2800,21600] cuentas (histéresis 50 mV). Dos actualizaciones consecutivas confirman un cambio. Al entrar se fuerza el CC actual; mientras ausente no se emite CC; al salir se manda note-off y CC cero una sola vez. Muestras eléctricamente inválidas (<0 o >26400) no alimentan el filtro y fuerzan salida tras dos actualizaciones. Si el ADS deja de entregar muestras durante 200 ms, la presencia se libera y el filtro se reinicia. Las variaciones en voltaje fuera de 4–30 cm pueden ser ambiguas: el Sharp no permite garantizar presencia física ni distinguir siempre una mano demasiado cercana; el flag es una estimación por ventana calibrada. No se promete precisión fuera de rango.

Los tests nativos cubren extremos CC, cadencia 40 ms, rechazo de ruido/deadband, histéresis y confirmación, salida por falta de muestras, entradas inválidas y wrap de millis. No prueban reflectividad, luz ambiente ni alimentación.

## 3. USB y mapa MIDI

USB `MIDI+Serial`, canal **1** (mido `channel=0`). Nombre USB propio **«GF Superficie»** (`src/consola_s/usb_names.c`) para distinguirla de la Teensy A en el Pi. Sin SysEx ni driver propio para E4.

| Control | Mensaje |
|---|---|
| FSR 1–4 | Note 36–39, velocity calculada; off velocity 0 |
| M4 pad 0–31 | Note 48–79, velocity 100; off 0; índice fila × 8 + columna |
| Encoders 1–8 | CC 16–23 relativo; cero no envía |
| Faders 1–4 | CC 24–27 absoluto |
| Expresión 1–2 | CC 28–29 absoluto |
| MASTER / CUE / IN | CC 30 / 31 / 32 absoluto |
| MCP #1, 14 entradas | Note 80–93; primeras 8 = clics encoders, siguientes 6 funciones |
| Manos 1–2 | CC 33–34 absoluto de proximidad; presencia Note 108–109 on 127 / off 0 |
| MCP #2, 14 entradas | Note 94–107; primeras 8 = PLAY, STOP, REC, LOOP, UNDO, NEW, SHIFT, METRO; siguientes 6 funciones |

Botones: on 127, off 0. Orden físico MCP: 0,1,2,3,4,5,6,8,9,10,11,12,13,14. Esta es la mesa con dos MCP, no el panel final de tres chips.

CDC **solo prototipo**: `@S STATS\n` informa máximo loop en µs, Hz de scans/pad, máximo intervalo de scan, lecturas ADS exitosas/s por chip, eventos MIDI generados/s, errores I2C y drops de cola. Tasas desde arranque o último `@S RESET\n`. RESET reinicia contadores, no estado musical. Recepción acotada y respuesta transmitida por fragmentos si hay espacio; sin esperas de host.

La consola final usará bridge-protocol por CDC cuando se asignen los comandos de consola. No se inventan CMD, ni GET_STATUS/rol, ni se confunde el XOR de la M4 con CRC-8 del bridge.

El firmware S no maneja brillo en pin 28. El entorno adicional `consola_s_m4` limita brillo M4 a 63/255 (24,7 %) y envía press/release mediante el CMD_GRID_PAD_PRESS ya existente del transporte M4 (payload columna, fila, estado). El entorno M4 heredado permanece igual. Su firmware original solo transmite press: S lo admite como pulso de 10 ms, sin inventar una liberación física. Para estados sostenidos usar `consola_s_m4`. No son comandos bridge.

## 4. Validación y límites

Unity nativo prueba ventana/pico, mapeo y saturación velocity, histéresis, retrigger y wrap de tiempo; también EMA/deadband, CC relativo y la lógica de manos descrita arriba. No demuestra ruido eléctrico, timing de ISR ni latencia USB. Compilar todos los entornos existentes es la regresión; nunca ejecutar upload.

En placa, después de calibrar: registrar STATS mientras se mueven todos los controles; mínimo scan ≥2 kHz/pad, intervalo máximo ≤500 µs, cero drops. Con mido abrir el puerto MIDI clase estándar y registrar notas/CC; medir golpe→evento (<5 ms, objetivo E4) con referencia eléctrica y reloj compartido. La ventana consume 3 ms del presupuesto: no se afirma pasar sin medir. Comparar 10 golpes suaves y 10 fuertes, buscar rangos separados. CPU de audio ≤60 % y ausencia de clics se verifican en A con el sistema completo; S no tiene AudioProcessorUsageMax. No se marca E4 aprobado sin esa demo.

Fuera de alcance: flash, mido/GUI del Pi, synths/FX, semántica LIVE/MIXER, bridge final, UART A↔S activo, MIDI DIN, pedal, triggers, diseño PCB, animaciones LED y calibración física.

## Fuentes

- [Sharp GP2Y0A41, hoja de datos y curva típica](https://global.sharp/products/device/lineup/data/pdf/datasheet/gp2y0a41sk_e.pdf).
- [TI ADS1115, registros §8 y conversión single-shot §7.4](https://www.ti.com/lit/ds/symlink/ads1115.pdf).
- [PJRC, asignación de pines Teensy 4.1](https://www.pjrc.com/teensy/pinout.html).
- [Interlink, guía FSR: actuadores y divisor](https://courses.media.mit.edu/2022spring/mas836/readings/fsrguide.pdf).
- Contratos locales: `PIN_MAP.md`, `03-arquitectura-capas.md` §4.1, `01-prototipo-mesa.md` E4 y `02-bridge-protocol.md` §1.3/5.4.

## 5. Resultado de verificación de esta entrega

- `pio run -e consola_s`: SUCCESS, RAM1 variables 12 608 B; RAM2 17 568 B. Sin upload.
- `pio test -e native`: 10/10 casos Unity pasan.
- `pio run -e consola_s_m4 -e neotrellis_m4`: ambos SUCCESS.
- `test_faders_teensy` y `test_encoders_teensy`: SUCCESS.
- `teensy41`, `test_mcp_encoder_buttons_teensy` y `test_mcp_extra_buttons_teensy`: comprobación bloqueada al resolver `Adafruit MCP23017 Arduino Library@^2.3.2`, ausente en la caché y DNS de registry inaccesible. No se afirma regresión completa.

En este sandbox los comandos usan `PLATFORMIO_CORE_DIR=/tmp/gf-e4-pio`, con toolchains instalados en solo lectura y bibliotecas recuperadas de la caché local. El comando sin esa variable falla al crear `~/.platformio/platforms.lock`, fuera de las rutas escribibles. No se cambió el entorno global ni se reemplazaron dependencias del proyecto.

La creación de rama en el worktree original también está bloqueada: el git-common-dir vive fuera de la ruta escribible. La entrega se conserva en una rama `codex/teensy-s-e4` de un repositorio temporal y un bundle; el worktree original conserva su HEAD. La actualización de protoboard autoriza CUE/IN y permite asignar las reservas sin conflicto.
