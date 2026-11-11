# Consola: firmware de la Teensy única

> 2026-11-10 · ADR-008 §2 · implementación de prototipo, sin flash. Autoridad de cableado: [17](17-mapa-pines-teensy.md). Sustituye el reparto A/S de 06, conserva su protocolo musical.

## 1. Ubicación y alcance

Nuevo entorno **`consola_uno` en `apps/firmware-teensy`**: hereda el overlay privado alex6679 f731821 de `consola_usb` (UAC2 asíncrono, PCM16, 44.100 Hz nominales, 4 canales por sentido). Reutiliza `audio_graph` GF_CONSOLA y el parser `preset_link`. Los módulos de superficie y su runtime pasan a **`apps/consola/surface`**, biblioteca local compartida por `consola_s` y `consola_uno`; no se copia la captura ni el transporte M4. Mantenerlo en processor obligaría a trasladar el overlay/grafo ya medidos. Los engines/FX/UI del Brain no se enlazan por defecto. `consola_uno_brain` permite una parte Moog con `GF_CONSOLA_BRAIN`, sin panel Brain ni UART heredada; MIDI entrante canal 1 controla esa parte. No activar antes de medir margen.

USB compuesto **MIDI + Audio 4ch + Serial**, producto **«GF Consola»**. `superficie.py` cambia su filtro predeterminado; `--puerto 'GF Superficie'` sigue permitiendo usar la S antigua. Audio conserva el grafo medido: captura LINEA o SINTE en canales 1/2; canales 3/4 de captura quedan en silencio y reproducción 3/4 reservada. Cue usa reproducción 1/2. No se afirma que cuatro canales USB impliquen cuatro salidas analógicas del SGTL5000.

## 2. Tiempo real y bus compartido

Prioridad NVIC (menor número gana): USB **112**, DMA I2S **128** (core), FSR IntervalTimer **160**, AudioStream **208**. La prioridad del timer se fija **antes de begin()**. `IntervalTimer` comparte IRQ_PIT entre sus cuatro canales: se toma la prioridad más alta solicitada; STATS publica `pit_priority`. I2S asume `update_setup()` en su constructor global; `AudioMemory` sólo crea un IntervalTimer de respaldo si ningún I/O asumió la actualización. No hay evidencia de otro PIT activo en este grafo (véase §7), ni se enmascara el PIT desde loop.

ADC1 tiene **un único dueño**, `AdcScan` en la ISR de 50 µs. Cada trama de 400 µs programa FSR habilitados; CUE24 (canal1) y selector41 (canal10) se intercalan cada 10 ms. Una interrupción inicia la conversión y otra recoge R0: **no hay busy-wait**. Si no llega COCO en 100 µs, aborta HC0=31, incrementa `adc1_timeouts`, omite esa muestra y continúa. No fabrica un cero ni un golpe. El loop sólo lee buzones de CUE/selector. MASTER22 usa **ADC2 canal13**, IN26 ADC2 canal3; ADC2 pertenece al loop, con plazo DWT de **20 µs** y `adc2_timeouts`. Tampoco `consola_s` comparte ADC1 con pots. Se conserva la calibración/reloj asíncrono PJRC realizada por `analog_init` antes de setup, resolución12, sin averaging/trigger externo/continuous/compare/DMA ADC. No hay `analogRead` después de tomar posesión. No cambia ningún pin.

Wire18/19 a **400 kHz**: I2C sólo en setup/loop. El perfil decide qué dispositivos existen: mínimo **no sondea** ADS0x49, MCP0x22 ni DAC0x60. ADS0x48 convierte sólo AIN0/AIN2. NACK/fallo usa backoff exponencial **1, 2, 4, 8, 16, 32 s**, reiniciado después de una operación completa correcta. MCP libera notas al caer. Codec/CV aplican el mismo backoff. Todos los CC tienen cadencia máxima **100 Hz por control**; valores absolutos se coalescen, encoders/jog retienen pasos pendientes. Las actualizaciones locales de CUE/IN comparten esa cadencia.

**Timeout real de Wire:** Teensyduino1.160.0 `WireIMXRT` no tiene `setWireTimeout`; `Stream::setTimeout` no limita `endTransmission/requestFrom`. `surface/tools/wire_timeout.py` crea un **framework privado en .pio/build**, conserva el driver PJRC y añade `setWireTimeout(250)` / `getWireTimeoutCount()`. El plazo de **250 µs** cubre adquisición de bus y transferencia juntos; pin-low también250µs. Recuperar bus ocupado añade como máximo nueve pulsos de10µs más10µs iniciales; siguiente intento respeta backoff. El script valida los sitios que parchea y nunca modifica `~/.platformio`. Se aplica a uno/full/brain/diag y `consola_s`; consola/USB/sketch49 conservan su framework. `wire_timeout_us`, `wire_timeouts`, `retry_max_us` y retries codec/CV en STATS permiten contrastarlo. **Objetivo medible: loop_max_us<2000 en régimen estable**, pendiente de Pi/Teensy; el arranque y la recuperación SGTL con `enable()` oficial (5+400ms) quedan fuera de régimen estable y sí aparecen en el máximo si ocurren durante loop. Hacer `@S RESET` después de setup/recuperación antes de medir. No se afirma que un build mida latencia.

Un solo par de pull-ups, 3,3 V; revisar paralelo de módulos/shield (17 §2). Sin accesos concurrentes al bus desde callbacks MIDI. CV por MCP4725, escritura volátil, nunca EEPROM; `@GF CV <0–4095>` controla 0x60, inicialmente cero. El segundo DAC 0x61 queda reservado.

## 3. Pines y MIDI

Se aplica 17 completo: encoders **2/3, 4/5, 6/9, 10/11, 12/30, 31/32, 33/34, 35/36**; jog **37/40**; M4 **Serial7 RX28/TX29** (cruzar TX/RX); MASTER **22**, CUE **24**, IN **26**; FSR **14–17**; LEDs data **13** (reservado, sin animaciones que enmascaren audio); selector de pad analógico **41**. I2S **7/8/20/21/23**. Desaparece UART A↔S; 29 deja de ser PWM CV. DIN 0/1 y triggers 25/27/38/39 permanecen reservados.

No hay contradicción de asignación en 17. Restricciones del shield reutilizado: **sin microSD SPI ni flash opcional**, porque 6/10/11/12/13 son superficie; no montar el pot de volumen opcional del shield en 15 (FSR2). CUE usa el buzón del planificador ADC1 descrito arriba. La escalera del selector no tenía resistencias/umbrales definidos: se fija aquí como contrato de prototipo, calibrable antes de PCB.

Canal MIDI 1; se conservan FSR notas 36–39, M4 48–79, encoders CC16–23, jog CC35, faders CC24–27, expresión CC28–29, MASTER/CUE/IN CC30/31/32, manos CC33/34 y presencia notas108/109. MCP usa orden GPIO **0–6, 8–14**; GPA7/GPB7 no son entradas:

> **Pendiente (2026-11-11):** el reparto físico cambió (17 §3 corregido). Esta tabla describe el firmware actual y se
> reescribe con el próximo cambio de firmware; los rangos de notas 80–122 se conservan.

| Chip | Índices lógicos → notas y función |
|---|---|
| 0x20 | 0–7 → 80–87 clics encoder; 8 → 88 JOG; 9–13 → 89–93 Kailh1–5 |
| 0x21 | 0–13 → 94–107 Kailh6–19 (nombres históricos del lector conservados) |
| 0x22 | 0 → 110 Kailh20; 1–11 → 111–121 LEFT, RIGHT, BACK, PAGE_LEFT, PAGE_RIGHT, OCT_MINUS, OCT_PLUS, MENU, SEARCH, SESSION_ARRANGE, DUPLICATE; 12 → 122 PEDAL; 13 libre |

La nota110 salta 108/109 para no colisionar con las manos. El pedal usa nota on/off, sin invertir ni convertir a sustain. La semántica de Kailh conserva los identificadores históricos de 08: no se reasignan PLAY/STOP/etc. sin otro contrato del panel.

Selector: común a 41; tres posiciones a GND, mitad de 3,3 V (divisor 10k/10k), 3,3 V; masa común, nunca 5 V. ADC12: umbrales **1365/2730**, histéresis **±80 cuentas**. Posiciones 0/1/2 representan 0/−10/−20 dB y envían **CC36 = 0/64/127** inicial y al cambiar. Solo informa la posición física: no aplica otra atenuación digital. El lector añade `SelectorPad(db)`.

## 4. Prototipo mínimo y CDC

Banco comunicado por Juan (2026-11-10): Wire 18/19 ya responde con SGTL5000 0x0A, CJMCU-2317 0x20/21 (solo GPA0–GPA6 cableados) y ADS de dirección fija 0x48 (fader A0, Sharp A2). No se trasladó ni se modificó el sketch50 que vive en otra rama.

`GF_CONSOLA_MINIMAL=1` predeterminado; el entorno `consola_uno_full` lo pone a 0 y activa el mapa completo. Cinco encoders **incluido jog** = encoder1–4 + jog; dos FSR; **ningún pot** (MASTER22 está al aire en esta mesa). Los periféricos I2C excluidos no se sondean. No se puede autodetectar si un pot/FSR analógico está cableado: las máscaras desactivan entradas flotantes.

| Conectar | Cableado mínimo |
|---|---|
| Teensy 4.1 + SGTL5000 shield compatible T4 | I2S7/8/20/21/23, I2C18/19, alimentación y GND; USB al Pi/host |
| Encoder1–4 + jog | A/B 2/3, 4/5, 6/9, 10/11, 37/40; común GND |
| MCP0x20/21 | SDA18/SCL19, 3,3 V, GND, RESET alto; A2/A1/A0=000/001; módulos CJMCU-2317, solo GPA0–GPA6 cableados y habilitados; clics encoder1–4 en GPA0–GPA3, pulsadores a GND |
| FSR1/2 | 3,3 V → FSR → 14/15; cada ADC → 10k → GND |
| MASTER (opcional, deshabilitado) | extremos 3,3 V/GND, cursor22; habilitar `-DGF_CONSOLA_WIRED_POTS=1` sólo al cablearlo |
| ADS0x48 | 3,3 V/GND, SDA18/SCL19, dirección fija 0x48 sin pin ADDR en el módulo de Juan; fader1 cursor AIN0 (extremos 3,3 V/GND); Sharp1 salida AIN2 |
| Sharp1 | alimentación5 V, ≥10µF local y GND común; verificar salida dentro de 0–3,3 V antes del ADS |
| M4 | TX M4 → RX28; RX M4 ← TX29; GND común, alimentación adecuada; firmware M4 existente de press/release |

En mínimo el Sharp se traslada de ADS0x49 AIN2 a **0x48 AIN2**, manteniendo CC33/nota108. Los otros canales ADS se omiten. El clic jog está en GPB1 según 17 y no está cableado en esta mesa: queda desactivado en mínimo; no se reasigna a otro GPIO. Sin tercer MCP, segundo ADS, DAC, CUE/IN o selector conectados no se generan eventos falsos ni logs repetidos. STATS informa máscaras habilitadas y presencia; ausencia M4 significa sin respuesta válida, no diagnóstico eléctrico.

Mismo CDC: `@S STATS` y `@S RESET` mantienen los campos de 06/08 y agregan presencia/máscaras, CPU/memoria audio y errores codec/DAC. `@GF RUTA LINEA|SINTE` (LINEA inicial; SINTE silencio sin Brain), `@GF CUE 0|1` (1 inicial), `@GF USB` mantiene contadores async; **`@GF IN 0–15`** llama `lineInLevel` (5 inicial; número mayor = menor sensibilidad), **`@GF HP 0–127`** volumen de audífonos (64 inicial). En completo CUE/IN envían sus CC y actualizan HP/IN; MASTER sigue siendo CC para la sala del Pi. OK confirma el estado solicitado; la escritura física se atiende en loop y su disponibilidad se consulta en STATS. Respuestas acotadas/fraccionadas sin esperar al host. RESET reinicia los contadores históricos de superficie; los errores codec/CV y máximos de audio añadidos se acumulan desde arranque. Sin protocolos bridge nuevos.

## 5. Verificación y puerta física

Tests nativos: mapa MCP completo/pedal/libre y ausencia de colisiones, ladder con histéresis, producto USB, configuración mínima, parser IN/HP/CV y recuperación/backoff. Mantener ambas suites native y tests Python; builds `consola_uno`, `consola_usb`, `consola`, `sketch49`, `consola_s`, sin upload. Código oficial PJRC: Wire/ADC/Audio/I2S/SGTL5000/Encoder; custom: superficie y política de polling/ruteo; terceros: overlay USB alex6679.

La validación 0 dropouts de [E3](mediciones/e3-usb-async-20261109.md) corresponde a `consola_usb`, **no certifica este firmware combinado**. Después de cablear: audio dúplex ≥60 min + todos los controles, cero drops de cola/USB, scan ≥2kHz y gap≤500µs, carga total≤60%, reconexión de cada I2C y M4. Medir latencia y calibrar FSR/Sharp/ladder; falta hardware para aprobar la puerta. No usar animaciones LED síncronas. No cambia ningún SSoT00–06.


## 6. Resultado de escritorio anterior (antes del fallo físico)

Se escribió este documento antes del código. Commit del documento bloqueado por `Unable to create .git/index.lock: Operation not permitted`; por la misma restricción quedan **documento y código sin commits** sobre `codex/teensy-unica`, HEAD `2695044`. Sin push, upload ni cambios al checkout host.

| Verificación final | Salida real |
|---|---|
| firmware-teensy: `pio test -e native` | **174 test cases: 174 succeeded**, 12,851 s |
| processor: `pio test -e native` | **11 test cases: 11 succeeded**, 0,595 s |
| tools/consola: `python3 -m pytest -q` | **450 passed**, 1,41 s |
| `sketch49` | SUCCESS, 5,523 s |
| `consola` | SUCCESS, 4,736 s |
| `consola_usb` | SUCCESS, 2,292 s |
| `consola_uno` | SUCCESS, 1,304 s |
| `consola_uno_full` | SUCCESS, 1,286 s |
| `consola_uno_brain` | SUCCESS, 1,326 s |
| processor: `consola_s` | SUCCESS, 1,602 s |
| `git diff --check` | Sin errores |

La última pasada de builds es incremental, posterior a las compilaciones completas. `consola_uno`: FLASH código **81.300 B**, RAM1 variables **20.288 B**, RAM2 **52.512 B**. `nm` confirma ausencia de símbolos de engines en el binario predeterminado y `MoogModelD::begin` en el opt-in. `objdump` verifica el descriptor fuerte `usb_string_product_name`, 22 bytes, tipo 3, texto UTF-16 **GF Consola**. No se conectó ni se flasheó hardware para estas comprobaciones. Las compilaciones completas heredan warnings de flags `-fno-rtti` en C; los entornos Brain también tienen warnings existentes de `strncpy`.

Para reproducir aquí, usar el ejecutable `~/.platformio/penv/bin/pio` y el prefijo `PLATFORMIO_CORE_DIR=/tmp/gf-unica-pio-core PLATFORMIO_SETTING_ENABLE_TELEMETRY=no` desde cada proyecto. Ese core temporal referencia paquetes/plataformas instalados en lectura, y Unity procede de la caché local; no se parcheó `~/.platformio`. Processor usa Encoder oficial incluido en Teensyduino y la biblioteca local por `symlink://../surface`.

Logs: `/tmp/gf-unica-firmware-native.log`, `/tmp/gf-unica-processor-native.log`, `/tmp/gf-unica-python.log`, `/tmp/gf-unica-firmware-builds-final.log`, `/tmp/gf-unica-consola_s.log`. Compilaciones completas de los tres entornos nuevos: `/tmp/gf-unica-uno-builds.log`.

**Pendiente físico:** probar este firmware combinado bajo audio dúplex, medir jitter/latencia/carga total y reconexión. Los pines de 17 se respetan; las salvedades del shield y ADC1 se explican en §2–3. CV requiere conectar un MCP4725; la mesa actual no lo tiene. La falta de esos ensayos no se oculta tras el resultado E3 del firmware anterior.


## 7. Corrección tras la prueba física del 10 de noviembre

Evidencia de entrada: [medición Pi](mediciones/synth-pi-20261110.md). Con FSR activo enumera pero CDC/MIDI no avanzan; la misma imagen con `GF_DIAG_NO_FSR` avanza. En HEAD5ded9db `read_adc1` y `read_adc2` esperan COCO **sin ningún plazo**; la ISR puede por tanto retener el procesador indefinidamente si ADC no termina. Éste es el defecto de vivacidad comprobable en código y la ruta aislada por el A/B. **El disparador exacto de la falta de COCO no está demostrado**: no hay dump de ADC ni reproducción con placa en esta sesión. No se presenta la sospecha MASTER como causa probada: ese HEAD ya deshabilitaba IRQ_PIT alrededor de MASTER/CUE/selector, y el `analogRead(14)` de setup precedía al timer. Fijar prioridades por sí solo tampoco demuestra una solución.

Se contrastó el paquete instalado `framework-arduinoteensy1.160.0`: `cores/teensy4/analog.c` (`pin_to_channel`, `analog_init`, `analogRead`), `IntervalTimer.cpp`, `AudioStream.cpp:initialize_memory/update_setup`, `libraries/Audio/output_i2s.cpp`, `libraries/ADC/ADC.cpp`. MASTER22 ADC1/2 canal13; CUE24 sólo ADC1 canal1; IN26 sólo ADC2 canal3; selector41 ADC1/2 canal10. La tabla de ADC incluida sí expone ADC2 para41 aunque PJRC `analogRead(41)` elige ADC1. El SGTL usa su ADC de audio por I2S, no ADC1 del MCU. El overlay alex6679 no sustituye analog.c ni IntervalTimer.cpp. Fuentes primarias: [core PJRC](https://github.com/PaulStoffregen/cores/blob/master/teensy4/analog.c), [ADC incluido](https://github.com/pedvide/ADC/blob/master/ADC.cpp). La versión instalada prevalece sobre cambios posteriores de esas ramas.

El nuevo planificador elimina las esperas y todos los accesos concurrentes al mismo ADC; tests nativos inyectan COCO ausente permanente, recuperación y wrap; verifican tramas2,5kHz, slots lentos100Hz y velocity. El fake Wire ejecuta el **AdsScanner de producción**: máscara vacía=0 transacciones, sólo AIN0/2, NACK/read-fault, backoff y recuperación. Cadencia se prueba con10000 actualizaciones/s y conservación de todos los detents. `fsr0_sps`…`fsr3_sps` cuentan muestras reales, además de `fsr_hz` (tramas), errores ADC, `fsr_timer`, `adc1_cfg/gc` y prioridad PIT. Una frecuencia de tramas correcta con SPS0 **no** aprueba FSR.

`consola_uno_diag` se **conserva como A/B de aislamiento** (sin timer, con trazas y espera inicial de USB); no sirve para probar pads. `GF_DIAG_NO_FSR` está ausente de `consola_uno/full/brain`; esas imágenes no incluyen las trazas diag ni su espera USB. La salida existente de audio_graph al arrancar se conserva.

Pots: filtro EMA y deadband2 MIDI, más techo100Hz, no pueden identificar eléctricamente una entrada flotante. Por eso mínimo las deja deshabilitadas; completo supone cableado completo. Al habilitar un cursor, conectar ambos extremos; si puede quedar desconectado, poner pull-down externo (p.ej.100k a GND, verificando carga/calibración). No usar un pin al aire como detector de pot. No hay cambios de pin que Juan deba recablear.

Prueba pendiente con firmware de producción: FSR1/2 en14/15 con divisor10k, Sharp0x48 AIN2, `@S RESET`, `@S STATS` después de60s y audio dúplex. Esperar `pot_mask=0`, `ads_mask=1`, `mcp_mask=3`, `cv_enabled=0`, `cv_errors=0`, `ads1_sps=0`, `fsr_timer=1`, muestras de cada pad≥2000/s, `adc1_timeouts=adc2_timeouts=0`, cero drops y loop_max<2000µs. Enviar notas36/37 con velocity y liberación; CC33/nota108 de Sharp; SYNTH transforma el segundo FSR en nota38 (doc21). Si aparece timeout, guardar STATS completo: aún falta determinar el disparador físico original. Resultados de escritorio de esta corrección: ver medición enlazada (no reemplazan el ensayo físico).
