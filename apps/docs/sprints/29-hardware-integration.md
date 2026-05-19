# Sprint Intermedio — Integración Hardware

> **Tipo:** Integration sprint (no es un sprint de feature)
> **Estado:** EN PROGRESO · Mayo 2026
> **Objetivo:** Conectar físicamente todos los componentes y verificar que
>   el software ya escrito funciona en el hardware real.
> **Orden de integración:** Encoders/Botones → Bridge UART → MIDI Host
> **Spec de referencia:** `apps/docs/01-architecture.md §3.3` (pin mapping SSoT)

---

## Por qué este orden

La regla es simple: ir de lo más seguro a lo más complejo, y validar cada paso
antes de agregar el siguiente.

**Encoders y botones primero.** Son solo GPIO — no hay protocolo, no hay segunda
placa, no hay software de alto nivel involucrado. Si algo falla, el problema está
en el cableado y el multímetro lo resuelve en segundos. Fallar aquí no rompe nada.

**UART Bridge después.** Requiere que ambas placas estén encendidas y que el
firmware de cada una esté flasheado. Si el ESP32 no está funcionando correctamente,
no tiene sentido conectar el Teensy. Se verifica primero el ESP32 solo (con el
script Python), luego se agrega el Teensy.

**MIDI USB-A al final.** Requiere que el Teensy esté completamente configurado:
I2S funcional, Audio Library inicializada, USB host stack activo. El teclado MIDI
depende de que todo lo anterior esté correcto.

---

## Etapa 1 — Encoders, Botones y LEDs

### 1.1 Cómo funciona el EC11 físicamente

El encoder ALPS EC11 tiene dos grupos de pines físicamente separados en el cuerpo
del componente:

```
Vista inferior del EC11 (pines hacia arriba, componente boca abajo):

         [ cuerpo del encoder ]

    1    2    3    4    5
    |    |    |    |    |
    A   GND  GND   B   SW
         enc  enc      |
                       GND
                       sw

Grupo rotativo (3 pines):
  Pin 1 — A (fase A de cuadratura)
  Pin 2 — GND del encoder (GND mecánico, conectar a GND Teensy)
  Pin 3 — GND del encoder (mismo nodo que pin 2, algunos EC11 los unen internamente)

  En algunos EC11 el pinout es A-GND-B (3 pines centrales):
  Pin 1 — A
  Pin 2 — GND (nodo central)
  Pin 3 — B

Grupo switch (2 pines):
  Pin 4 — SW (un lado del pulsador)
  Pin 5 — GND del switch (el otro lado)
```

El GND del encoder (pin 2/3) y el GND del switch (pin 5) se conectan al mismo
GND del Teensy. No son el mismo nodo internamente en el componente, pero la
referencia de destino es la misma.

Con `INPUT_PULLUP` en Teensy no se necesitan resistencias de pull-up externas.
El pull-up interno de ~47 kOhm es suficiente para los encoders EC11 a esta
velocidad de giro.

### 1.2 Tabla de cableado — Encoders

| Cable | Desde (EC11) | Hacia (Teensy) | Color sugerido |
|---|---|---|---|
| ENC L — A | EC11-L pin A | GPIO 10 | Amarillo |
| ENC L — B | EC11-L pin B | GPIO 11 | Naranja |
| ENC L — GND enc | EC11-L pin GND | GND | Negro |
| ENC L — SW | EC11-L SW pin | GPIO 12 | Blanco |
| ENC L — GND sw | EC11-L SW GND | GND | Negro |
| ENC R — A | EC11-R pin A | GPIO 13 | Amarillo |
| ENC R — B | EC11-R pin B | GPIO 14 | Naranja |
| ENC R — GND enc | EC11-R pin GND | GND | Negro |
| ENC R — SW | EC11-R SW pin | GPIO 15 | Blanco |
| ENC R — GND sw | EC11-R SW GND | GND | Negro |
| ENC NAV — A | EC11-NAV pin A | GPIO 16 | Amarillo |
| ENC NAV — B | EC11-NAV pin B | GPIO 17 | Naranja |
| ENC NAV — GND enc | EC11-NAV pin GND | GND | Negro |
| ENC NAV — SW | EC11-NAV SW pin | GPIO 26 | Blanco |
| ENC NAV — GND sw | EC11-NAV SW GND | GND | Negro |

### 1.3 Tabla de cableado — Botones Kailh Choc V2

Los Kailh Choc V2 son switches mecánicos de dos terminales. No son polares.

| Cable | Desde (Kailh) | Hacia (Teensy) | Nota |
|---|---|---|---|
| B1 | Terminal 1 | GPIO 2 | Terminal 2 → GND |
| B2 | Terminal 1 | GPIO 3 | Terminal 2 → GND |
| B3 | Terminal 1 | GPIO 4 | Terminal 2 → GND |
| B4 | Terminal 1 | GPIO 5 | Terminal 2 → GND |

### 1.4 Tabla de cableado — LEDs WS2812B

```
        Teensy GPIO 28
              |
           [330 Ω]        <- resistor en serie, protege contra reflexiones
              |
           LED[0] DIN
           LED[0] DOUT ─── LED[1] DIN
                           LED[1] DOUT ─── ... ─── LED[15]
           
           LED[0] VCC ─┐
           LED[1] VCC ─┤── Rail 5V externo (NO pin Teensy)
           ...          |
           LED[15] VCC─┘

           LED[0] GND ─┐
           LED[15] GND─┴── GND Teensy
```

| Cable | Desde | Hacia | Nota |
|---|---|---|---|
| DIN | GPIO 28 → resistor 330 Ω → LED[0] DIN | — | Resistor obligatorio |
| VCC | Rail 5V externo | LED chain VCC (todos) | Nunca desde pin Teensy |
| GND | GND Teensy | LED chain GND (todos) | GND comun obligatorio |

Los 16 LEDs en consumo maximo (blanco, 255,255,255) pueden llegar a 960 mA.
El USB del computador entrega 500 mA tipico. Usar fuente externa de 5V/2A o
alimentar el Teensy desde una fuente con rails separados.

### 1.5 Build y upload — sketch de encoders

El sketch que valida encoders, botones y LEDs es el de Sprint 3.3. Al momento de
escribir este doc, el `build_src_filter` del env `sketch` apunta al sketch 21
(MIDI host). Cambiar temporalmente en `platformio.ini`:

```ini
# apps/firmware-teensy/platformio.ini
[env:sketch]
# Cambiar la linea build_src_filter a:
build_src_filter = +<sketches/20-encoders-buttons.cpp> +<ui/encoders.cpp> +<ui/buttons.cpp> +<ui/leds.cpp>
```

Luego:

```bash
cd apps/firmware-teensy
pio run -e sketch -t upload
pio device monitor -b 115200
```

### 1.6 Output esperado en Serial

```
[ENC L] delta: +1          <- girar ENC L clockwise (un detent)
[ENC L] delta: -1          <- girar ENC L counter-clockwise
[ENC L] push               <- presionar el eje del ENC L
[ENC R] delta: +1
[ENC R] delta: -1
[ENC R] push
[ENC NAV] delta: +1
[ENC NAV] delta: -1
[ENC NAV] push
[BTN] B1 pressed
[BTN] B1 held              <- mantener >1s
[BTN] B2 pressed
[BTN] B3 pressed
[BTN] B4 pressed
[LEDs] OK — 16 LEDs cyan   <- o el color de verificacion que use el sketch
```

El EC11 genera 4 pulsos crudos por detent mecanico. La libreria `Encoder.h`
acumula esos 4 pulsos y `readAndReset()` devuelve el total acumulado. Si el
sketch divide por 4, el output es `delta: +1` por click. Si muestra pulsos
crudos, es `delta: +4` por click — ambos son correctos.

### 1.7 Problemas comunes — Etapa 1

**El encoder gira pero no registra nada.**
El GND mecanico del encoder (pin central) no esta conectado. Sin ese GND, los
pines A y B flotan. Verificar con multimetro: continuidad entre el pin GND del
EC11 y el GND del Teensy.

**El encoder registra pero en direccion incorrecta.**
Los cables A y B estan invertidos. Intercambiarlos — no hay consecuencia electrica.

**El push del encoder no responde.**
Los 2 pines del switch del EC11 no son polares. Medir continuidad con multimetro
al presionar el eje. Si hay continuidad al presionar pero el sketch no lo
detecta: verificar que el pin esta configurado como `INPUT_PULLUP` en el codigo.

**Los botones Kailh no responden.**
Los Choc V2 tienen dos terminales en la base. Verificar que una va al GPIO y la
otra al GND. No son polares.

**Los LEDs no encienden.**
Verificar primero que VCC viene del rail 5V externo, no de un pin del Teensy.
Segundo, verificar la orientacion del DIN: el LED tiene un lado DIN (entrada) y
un lado DOUT (salida hacia el siguiente). Solo DIN acepta datos. Tercero,
verificar que el primer LED de la cadena es el indice 0 en el codigo.

**Los LEDs encienden con colores incorrectos.**
WS2812B usa orden GRB (no RGB). Si el codigo envia RGB y los colores son
incorrectos, verificar el orden de bytes en `CRGB` de FastLED — FastLED lo
corrige automaticamente si el tipo de LED esta bien declarado.

---

## Etapa 2 — Bridge Protocol UART (Teensy ↔ ESP32-S3)

### 2.1 Por que solo 3 cables

UART asincrono no necesita clock compartido ni CS — por eso es ideal para
comunicacion entre microcontroladores que tienen dominios de clock independientes.
Los 3 cables son:

- TX del emisor → RX del receptor (cruzado)
- RX del receptor ← TX del emisor (cruzado)
- GND comun (el mas critico y el mas olvidado)

Sin GND comun, la tension de referencia es diferente entre las dos placas. Lo
que el Teensy emite como "3.3V = 1" puede ser leido por el ESP32 como una
tension ambigua si sus GNDs estan flotando entre si.

```
Teensy 4.1                        ESP32-S3 (Waveshare)
                                   
  GPIO 1 (TX) ─────────────────── GPIO 44 (RX)
  GPIO 0 (RX) ─────────────────── GPIO 43 (TX)
  GND         ─────────────────── GND
  
  Lineas cruzadas: el TX de uno va al RX del otro.
  Ambas placas operan a 3.3V — sin level shifter.
```

### 2.2 Tabla de cableado UART

| Cable | Desde | Hacia | Nota |
|---|---|---|---|
| TX→RX | Teensy GPIO 1 (TX) | ESP32 GPIO 44 (RX) | Cruzado — TX → RX |
| RX←TX | Teensy GPIO 0 (RX) | ESP32 GPIO 43 (TX) | Cruzado — RX ← TX |
| GND | Teensy GND | ESP32 GND | Obligatorio |

Teensy 4.1: GPIOs a 3.3V. Waveshare ESP32-S3: GPIOs a 3.3V. Conexion directa,
sin level shifter.

Baud rate del Bridge Protocol: **921600**. El monitor serial de PlatformIO
(USB-CDC) corre a 115200 — son dos buses distintos, no confundir.

### 2.3 Verificacion paso 1 — ESP32 solo (sin Teensy)

Antes de conectar el Teensy, verificar que el firmware ESP32 parsea frames
correctamente usando el script Python del Bridge Protocol test.

Requerimientos: ESP32 flasheado y conectado al computador via USB-C.

```bash
# Terminal 1 — monitor serial del ESP32
cd apps/firmware-esp32
pio run -e esp32s3 -t upload       # flashear si no esta flasheado
pio device monitor -b 115200

# Terminal 2 — enviar frames de prueba
cd tools/bridge-test
python3 send_frame.py --cmd ENGINE_CHANGED --engine 0 --name "MOOG MODEL D"
```

Output esperado en el monitor del ESP32:

```
[bridge] ENGINE_CHANGED — id=0 name=MOOG MODEL D
```

Verificar timeout de heartbeat:

```bash
# Enviar 5 heartbeats cada 1 segundo
python3 send_frame.py --cmd HEARTBEAT --count 5 --interval 1.0

# Dejar de enviar y esperar 3 segundos
# El ESP32 debe loguear:
# [bridge] connection DOWN — heartbeat timeout
```

Si el ESP32 no responde a los frames del script Python, el problema es el
firmware del ESP32 — no hay nada del Teensy involucrado todavia.

### 2.4 Verificacion paso 2 — Con Teensy conectado

Una vez cableado el UART (los 3 cables de la seccion 2.2):

```bash
# Flashear el sketch de bridge test en el Teensy
cd apps/firmware-teensy

# En platformio.ini [env:sketch], cambiar build_src_filter al sketch de bridge:
# build_src_filter = +<sketches/bridge-heartbeat.cpp> +<bridge/bridge_master.cpp>
pio run -e sketch -t upload

# Monitor del Teensy (USB-CDC, 115200):
pio device monitor -b 115200
```

En el monitor del ESP32 (otra terminal):

```
[bridge] HEARTBEAT received — seq=0
[bridge] HEARTBEAT received — seq=1
[bridge] HEARTBEAT received — seq=2
```

Sequence number incrementa en cada frame. Si se pierden numeros de secuencia
(0, 1, 5, 6...) hay errores de CRC — verificar GND comun.

### 2.5 Verificacion paso 3 — Bidireccional

El Teensy es master del protocolo: envia comandos y espera ACK del ESP32 slave.
Verificar que el ACK llega de vuelta:

Output esperado en el monitor del Teensy:

```
[bridge] sent ENGINE_CHANGED — seq=0
[bridge] ACK received — seq=0
[bridge] sent ENGINE_CHANGED — seq=1
[bridge] ACK received — seq=1
```

Si el Teensy no recibe ACK: el cable de regreso (ESP32 GPIO 43 TX → Teensy
GPIO 0 RX) no esta conectado o esta invertido.

### 2.6 Problemas comunes — Etapa 2

**No llega nada al ESP32.**
El error mas comun es que TX y RX estan en el orden correcto (TX→TX) en lugar
de cruzado (TX→RX). Intercambiar los dos cables y volver a probar. Verificar
tambien que el firmware del ESP32 tiene el Bridge Protocol slave inicializado
(el carrusel LVGL del Sprint 3.4 ya lo incluye).

**CRC errors — frames llegan pero se rechazan.**
Falta el GND comun entre las dos placas. Conectar un cable de GND Teensy a GND
ESP32 y volver a intentar.

**Baud mismatch — caracteres ilegibles en el monitor.**
El monitor de PlatformIO (USB-CDC) es a 115200. El UART del Bridge Protocol
entre Teensy y ESP32 es a 921600. Son dos buses separados. El monitor muestra
el trafico USB-CDC del Teensy, no el trafico UART del Bridge — eso es correcto.

**El ESP32 se resetea al conectar el cable UART.**
No conectar ningun cable al GPIO 0 del ESP32-S3. GPIO 0 es el pin de boot mode
del ESP32. Un pull-down al conectar fuerza el modulo a modo de programacion y
no bootea el firmware. El Waveshare ESP32-S3 usa GPIO 43/44 para UART — nunca
GPIO 0/1.

**El ACK no llega de vuelta al Teensy.**
El cable de retorno (ESP32 TX → Teensy RX) no esta conectado. Verificar que
hay 3 cables, no 2. El error tipico es conectar solo TX→RX en una direccion y
olvidar el GND o el cable de regreso.

---

## Etapa 3 — MIDI Host USB-A

### 3.1 Conexion fisica del conector USB-A

El Teensy 4.1 expone el USB host en pads en la parte inferior de la PCB, cerca
del extremo opuesto al conector micro-USB. Son pads SMD pequenos — requieren
soldar un conector USB-A female tipo through-hole o usar un adaptador.

Para prototipar sin soldar permanentemente, opciones:

- Cable USB-A female con los wires expuestos (los cuatro conductores del estandar
  USB: rojo=5V, negro=GND, verde=D+, blanco=D-)
- Hub USB pasivo con los terminales del cable expuestos

```
Teensy 4.1 — pads USB host (underside, extremo opuesto al micro-USB):

  [PAD VBUS] ─── 5V (rail externo — el teclado lo necesita para operar)
  [PAD D-]   ─── cable blanco del USB-A female
  [PAD D+]   ─── cable verde del USB-A female
  [PAD GND]  ─── GND Teensy

  El teclado MIDI USB-A conecta en el conector female.
```

El pad VBUS debe venir del rail 5V externo, no de la salida 5V del USB del
computador que alimenta el Teensy — si el teclado consume corriente al arrancar,
puede causar un brownout en el Teensy.

### 3.2 Build y upload — sketch MIDI host

El sketch 21 implementa el MIDI host completo con el MoogModelD engine:

```bash
cd apps/firmware-teensy

# platformio.ini [env:sketch] — build_src_filter ya configurado para sketch 21:
# build_src_filter = +<sketches/21-usb-midi-host.cpp> +<usb/midi_host.cpp> +<engines/moog_model_d.cpp>
pio run -e sketch -t upload
pio device monitor -b 115200
```

Para escalar directamente al feature completo (Scale Lock + MIDI + AI):

```bash
# Usar env dedicado sketch24:
pio run -e sketch24 -t upload
pio device monitor -b 115200
```

El env `sketch24` ya tiene el `build_src_filter` configurado con:
`sketches/24-scale-lock.cpp`, `usb/midi_host.cpp`, `ml/key_detector.cpp`,
`ml/scale_lock.cpp` y `engines/moog_model_d.cpp`.

### 3.3 Output esperado en Serial

Al conectar el teclado al conector USB-A:

```
[MIDI] connected: <nombre del teclado>
```

Al presionar una tecla (sketch 21, sin Scale Lock):

```
[MIDI] NoteOn ch:1 note:60 vel:100
[engine] note_on: 60 vel:100
```

Al presionar una tecla (sketch 24, con Scale Lock activo):

```
[MIDI] NoteOn ch:1 note:61 vel:100
[key] detected: C_maj (confidence: 0.87)
[lock] 61 -> 60 (Db->C in C_maj)
[engine] note_on: 60 vel:100
```

El Scale Lock cuantiza la nota 61 (Db) a la nota 60 (C) porque Db no pertenece
a la escala de C mayor. Esto es el feature "never play a wrong note".

Para activar y desactivar el Scale Lock en tiempo real, el sketch 24 usa CC MIDI 64
(Sustain Pedal): valores 0-63 activan el lock, valores 64-127 lo bypass.

Para ajustar el filtro del MoogModelD con el teclado MIDI:
- CC 74 (Brightness): cutoff del filtro (20 Hz - 8 kHz, escala logaritmica)
- CC 71 (Resonance): resonancia del filtro (0.7 - 4.0)

### 3.4 Prueba de integracion total

Con los 3 pasos completados (encoders + UART + MIDI), la prueba final conecta
todo en paralelo usando el sketch 24 mas el Bridge Protocol activo:

```bash
pio run -e sketch24 -t upload
```

Lo que debe ocurrir simultaneamente:
1. Teclado MIDI → nota MIDI → Scale Lock → MoogModelD → audio por SGTL5000
2. ENC L → cutoff del filtro cambia en tiempo real
3. ENC R → resonancia del filtro cambia en tiempo real
4. Cambio de engine detectado → frame ENGINE_CHANGED via UART → carrusel
   del ESP32 cambia de vista

### 3.5 Problemas comunes — Etapa 3

**El teclado no se detecta ("connected" no aparece).**
Los pads D+ y D- del Teensy son pequenos (< 1mm de diametro). Verificar
continuidad entre el pad y el cable con multimetro en modo continuidad. El
teclado debe ser USB MIDI class-compliant — la mayoria de teclados MIDI modernos
lo son (no requieren driver en Windows/Mac/Linux).

**"connected" aparece pero no llegan notas.**
El USB host stack del Teensy llama a `midi_host.poll()` en `loop()`. Si el
`loop()` esta bloqueado o tarda demasiado, el poll no se ejecuta. Verificar que
no hay delays bloqueantes en el sketch.

**Audio silencioso despues del NoteOn.**
El SGTL5000 debe estar inicializado antes de cualquier nota. El sketch llama
a `engine.begin(0.6f)` en `setup()`, que inicializa el codec. Si el Teensy no
tiene el Audio Shield de PJRC conectado (que incluye el SGTL5000), el audio
no funcionara — el chip de codec no esta en el Teensy 4.1 base, solo en el
Audio Shield de PJRC o en el PCB propio del proyecto.

**El Scale Lock no cuantiza.**
Verificar que CC 64 esta en valor 0-63 (lock activo). Si el teclado envia CC 64
con valor > 63 al conectarse (algunos teclados lo hacen como reset), el lock
arranca en modo bypass. Enviar CC 64 con valor 0 manualmente para activarlo.

---

## Tabla resumen — orden de integracion

| Paso | Que conectar | Sketch / Script | Criterio de pass |
|---|---|---|---|
| 1a | ENC L (5 cables: A, B, GND enc, SW, GND sw) | sketch encoders | Serial: `delta: +1` al girar CW |
| 1b | ENC R + ENC NAV (10 cables adicionales) | mismo sketch | Los 3 encoders responden |
| 1c | Botones B1-B4 (8 cables: 2 por boton) | mismo sketch | Serial: `Bx pressed` y `Bx held` |
| 1d | LEDs WS2812B (DIN con resistor, VCC externo, GND) | mismo sketch | 16 LEDs encienden en color de verificacion |
| 2a | UART — ESP32 solo (sin Teensy) | `send_frame.py` → ESP32 | `ENGINE_CHANGED` en monitor ESP32 |
| 2b | UART — con Teensy (3 cables: TX, RX, GND) | sketch bridge | `HEARTBEAT received seq=0,1,2...` en ESP32 |
| 2c | UART — bidireccional | mismo sketch | `ACK received` en monitor Teensy |
| 3a | MIDI USB-A (D+, D-, VBUS, GND) | sketch21 | `[MIDI] connected: <teclado>` |
| 3b | MIDI + Scale Lock | sketch24 | Notas cuantizadas en Serial + audio |
| 3c | Todo junto | sketch24 + bridge activo | MIDI → Scale Lock → UART → display carousel |

---

## Notas de seguridad electrica

**No conectar 5V a pines GPIO del Teensy 4.1.** Los GPIOs del Teensy 4.1 toleran
hasta 3.3V. Aplicar 5V destruye el pin permanentemente. El Teensy 4.1 tiene
protecion en sus pines USB y 5V internos, pero NO en los pines GPIO de usuario.

**GND comun siempre.** Cualquier comunicacion entre dos placas (UART, I2C, SPI,
cualquier protocolo) requiere GND compartido entre ambas. Sin GND comun, las
tensiones son relativas a referencias distintas y los datos se corrompen o no
llegan.

**Alimentar LEDs WS2812B desde rail 5V externo.** 16 LEDs al maximo de
brillo (R=255, G=255, B=255) consumen hasta 960 mA. El USB de un computador
entrega 500 mA en total para toda la placa. Usar una fuente de 5V/2A separada.
El cable entre la fuente y los LEDs debe tener capacidad para 1A — no usar
cable fino de protoboard.

**Resistor 330 Ohm en el DIN del WS2812B.** El resistor en serie en la linea
de datos protege contra reflexiones de señal que pueden corromper el protocolo
NZR del WS2812B. Sin el resistor, el primer LED puede mostrar comportamiento
intermitente especialmente con cables largos (> 10 cm).

**No conectar nada al GPIO 0 del ESP32-S3.** GPIO 0 es el pin de boot mode del
ESP32. Un pull-down al conectar o encender fuerza el modulo a modo de
programacion flash y el firmware no bootea. El Waveshare ESP32-S3-Touch-LCD-1.28
usa GPIO 43 (TX) y GPIO 44 (RX) para UART — usar esos, nunca GPIO 0.

**Desconectar la alimentacion antes de cablear.** Aunque las placas toleran el
hotplug en la mayoria de los casos, conectar cables mientras hay tension puede
causar cortocircuitos momentaneos si el cable toca dos pines a la vez. Para
UART especialmente, conectar primero el GND, luego TX, luego RX.

---

## Learnings

*Esta seccion se completa despues de la integracion real. Que salio distinto al
plan, que sorprendio, que cambio en el procedimiento.*

- [ ] Fecha de integracion fisica:
- [ ] Problemas encontrados no documentados arriba:
- [ ] Ajustes al cableado:
- [ ] Cambios al sketch necesarios:
- [ ] Tiempo real por etapa (1a, 1b, 1c, 1d, 2a, 2b, 2c, 3a, 3b, 3c):
