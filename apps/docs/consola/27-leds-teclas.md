# 27 · LEDs de las 26 teclas — spec y teoría

2026-11-11. Spec escrito antes de implementación, bajo autorización de la tarea
keyleds1. Extiende 23 sin cambiar el retorno M4 ni los SSoT 00–06.

## 1. Cadena PCB-K partida

DIN pin13 → 74AHCT1G125 a 5 V → J1.5 en la placa izquierda → LED0.
GND común; OE activo bajo. No alimentar LEDs desde 3,3 V ni unir las ramas USB
de 24 §5. No usar SD/SPI del shield (17/18). SK6812MINI-E **RGB, GRB de 24 bits**,
800 kHz. Orden lógico = físico, índice base cero; coordenadas exactas del CSV v1,
mm desde izquierda/frente.

Orden autorizado por Juan (2026-11-12) para PCB-K en **dos placas**, según
[28 §7](28-pcb-k.md#7-costo-una-placa-o-dos). J1 recibe LED_DIN desde PCB-M en la
izquierda: la cadena empieza en BACK, baja por navegación, serpentea motor y
sección, pasa por SHIFT/UNDO y baja por edición hasta NEW. Un único hilo de datos
en el cable une DOUT de NEW (16) con DIN de PLAY (17) en la placa frente-derecha;
sigue transporte y termina subiendo escenas hasta SCENE1_OCT+ (25).
Los índices 0–16 están en la izquierda y 17–25 en la derecha. El recorrido gráfico
es ilustrativo; el cobre se valida al rutear. LED27 conserva CC102–111, colores,
brillos y limitador de corriente de §2–4.

| LED | Ref CSV | x | y |
|---|---|---:|---:|
|0|BACK|64|305|
|1|MENU|64|286.5|
|2|NAV_L|64|268|
|3|NAV_R|64|249.5|
|4|BROWSE|64|231|
|5|TAP|58|174|
|6|FILL|39|174|
|7|CUE_ENG|20|174|
|8|BUILD|20|155|
|9|DROP|39|155|
|10|BREAK|58|155|
|11|SHIFT|20|132.5|
|12|UNDO|39|132.5|
|13|DELETE|98|160.5|
|14|DUPLICATE|98|145.5|
|15|QUANTIZE|98|130.5|
|16|NEW|98|115.5|
|17|PLAY|132|39.5|
|18|STOP|151|39.5|
|19|REC|170|39.5|
|20|LOOP|189|39.5|
|21|METRO|208|39.5|
|22|SCENE4_PAGE_R|242|115.5|
|23|SCENE3_PAGE_L|242|130.5|
|24|SCENE2_OCT-|242|145.5|
|25|SCENE1_OCT+|242|160.5|

## 2. LED27, extensión independiente del canal 16

Canal16 (mido15), sólo CC; no usa notas48–79, CC20–31 ni UART M4.
CC102=27 activa/reinicia sólo teclas (todas off, latches inválidos, gracia1500ms).
CC23=23 sigue reiniciando exclusivamente pads. Firmware antiguo ignora LED27.

| CC | Valor / significado |
|---|---|
|102|27 opt-in teclas; otros rechazados|
|103|índice0–25; inválido invalida la transacción|
|104|estado0 off,1 dim,2 on,3 blink,4 pulse; inválido invalida|
|105|paleta0 teal,1 violet,2 blue,3 amber,4 white; inválido invalida|
|106|1 aplica atómicamente índice/estado/paleta; requiere los tres desde último apply|
|107|tempo LSB7 (BPM entero30–240)|
|108|tempo MSB7; aplica tempo si rango válido|
|109|serial de TAP módulo128; cambio reinicia fase local de beat|
|110|heartbeat GUI2Hz, timeout1500ms apaga todas las teclas|
|111|running0/1; con1 TAP usa sólo eventos CC28 del host|

CC25 brillo0–31 y CC26 animations0/1 compartidos con23; teclas también escuchan
CC28 beat serial (nuevo serial inicia flash80ms, sin repetir snapshots).
Pi envía snapshot completo en conexión/cambio modo y cada1s, diferencias a34ms
(≤29,42Hz), controles coalescidos y heartbeat incluso LIVE sin host de audio.
Máximo añadido snapshot:26×4 + opt-in +4 controles +heartbeat=110 mensajes.
Reintentar snapshot completo tras fallo parcial; se reinicia serial TAP al opt-in.
Los mensajes nunca se reenvían al M4 ni se interpretan en canal1.

## 3. Color y estados

RGB8 lineal previo a brillo: violet=(150,40,255), según diseño cuerpo v5 §7;
teal=(37,81,66), aproximadamente40% de (93,202,165), para compensación perceptual.
Blue=(61,140,255), amber=(233,173,80), white=(255,255,255), este último para banco.
Navegación/edición/SHIFT violeta. Transporte, motor y sección teal.
Columna escenas usa la propiedad existente ConsoleButtons.sceneColor:
LIVE teal, SYNTH blue, DRUM amber. No afirma escena ejecutada: esas intenciones
siguen PENDING. Ajustar compensación sólo con keycap real y medición diurna.

Dim=32/255, on=255/255, blink alterna on/dim cada250ms; pulse triangular
32…255 en1000ms. Animations OFF convierte blink/pulse a on. Sin gamma extra.
Por defecto teclas dim; navegación/edición on (acciones disponibles); SHIFT on
sólo mientras está mantenida. Sección armada blink, las demás dim, sin inventar
confirmación de ejecución. PLAY/STOP/REC reflejan el reloj existente en SYNTH/
DRUM cuando la telemetría es reciente; LOOP/METRO quedan dim si no hay estado.
TAP: beat host confirmado cuando running,80ms on/resto dim; detenido usa el BPM
actual de ConsoleButtons/drum.clock (incluye tap tempo), reinicia fase al TAP.
No genera MIDI clock ni modifica audio. Sin heartbeat desaparece todo (incluido
SHIFT/arm); al recuperar se ve último estado y llega snapshot GUI.

## 4. Tiempo real y electricidad

Bit-bang mantendría IRQ enmascaradas unos780µs (26×24×1,25µs), incompatible con
18 §2. WS2812Serial usa UART/DMA pero pin13 no es TX admitido en T4.1 y Serial7
ya pertenece a M4. OctoWS2811 permite cualquier pin en T4.x. Se usa su esquema
QTIMER4 canales0–2 → XBAR → **tres DMA**, reducido a GPIO2 bit3 (pin13), con
cuadro completo precalculado y sin interrupción DMA ni relleno ISR. El driver
custom conserva licencia/atribución PJRC; no es Teensy Audio Library oficial.
No instancia OctoWS2811: la versión instalada tiene esperas en show() y un ';'
en busy() que ignora DMA incompleto. El driver comprueba complete y plazo sin
esperar, devuelve false si ocupado y conserva el cuadro más nuevo para reintento.

624 bits/780µs de wire +300µs reset; evaluación34ms, sin catch-up. Buffers estáticos:
624 máscaras32bit=2496B RAM2 alineada/cache flush +32B máscara, tres TCD DMA.
Cada bit tiene DMA set alto, clear de ceros a0,30µs y clear final a0,75µs;
período1,25µs (misma temporización PJRC), comprobar tolerancias del lote en banco.
No toca I2S, PIT, prioridades NVIC, USB ni Serial7. GPIO sólo escribe bit3 de
SET/CLEAR (otros pines del puerto intactos). No heap, delay, spin, noInterrupts,
Wire ni audio calls en el driver. Reservar QTIMER4/XBAR req30/31/94; no añadir
PWM/tone/otra biblioteca Octo en esos recursos. Audio conserva112/128/160/208.

Brillo final clamp31/127=24,41%, default28/127=22,05%. Todo canal final≤62/255,
incluidos blanco, banco y payload inválido. Estimación conservadora por cuadro:
I_LED_uA=26000 + ceil(20000×sum(RGB8)/255), 1mA reposo/LED y20mA/canal.
**Hard limiter500000µA**, reserva100mA de la rama600mA de24 §5 para Sharp×2,
buffer y margen. Si excede, multiplicar todos los canales por
floor((500000−26000)×255/20000)/sum(RGB), truncando; recalcular estimación.
Hasta blanco en todas las teclas da≤406mA con el cap; limitador permanece como
segunda barrera ante futuras paletas. Modelo estimado, no sensor de corriente:
medir lote/fuente; polyfuse0,75A sigue necesario según24.

## 5. Pruebas, preview y puerta física

Para la partición, una tabla C++ y otra Python definen referencias y centros CSV.
El índice se deriva de su posición: así TAP no depende de un literal que pueda
quedar obsoleto. Un test compila y consulta la tabla C++ y compara las 26 entradas
con Python, la tabla de §1 y las 26 teclas del CSV (sin duplicados, mismo orden y
mismas posiciones); también verifica la columna LED de 28 §4. Unity comprueba que
sólo TAP recibe el tratamiento de beat y que STOP conserva su estado normal.
La preview usa las referencias para etiquetas y distingue el cable NEW → PLAY.
Esto valida direccionamiento y render, no continuidad eléctrica ni timing físico.

Antes de código/tests: Unity valida protocolo aislado de pads/canal1, apply
atómico y rechazo, estados/beat/rollover/timeout, fps, colores/cap, limitador
incluso entrada blanca255 y serial dedup. Pytest valida las26 referencias únicas,
modo/SHIFT/arm/TAP/brillo, diffs/snapshot/reconexión y coalescing. Preview genera
cuadros ejecutando el renderer C++ real con fixtures del generador Pi, situados
en el CSV (y invertida para vista frontal). No valida luminancia física.

Banco CDC `@GF KEYLEDS N` N=1…26: sólo primeros N ciclan teal/violet/blue/amber/
white cada1s, estados on, con los mismos cap/limiter; expira en60s. N=0 vuelve
al estado Pi; resto de la cadena siempre negro. Rechazar texto extra/rango inválido.
Brillo Settings (incluso0) sigue aplicando durante banco. Reset sin Pi=negro.
`@S STATS` añade `key_led_ua`, estimación del último cuadro (no medida física).

Verificar builds consola_uno/full, native completo, pytest GUI completo.
Puerta física pendiente: osciloscopio13 tras buffer, GRB/timing/latch, corriente
USB ramo≤0,6A, Audio dúplex≥60min con GUI/FSR/M4, cero xruns/drops y
loop_max_us<2000; medir duración de render/submit con DWT. No flashear aquí.

Fuentes: [23](23-leds-neotrellis.md), [17](17-mapa-pines-teensy.md),
[18 §2](18-teensy-unica-firmware.md#2-tiempo-real-y-bus-compartido),
[24 §5](24-placa-principal.md#5-presupuesto-de-consumo-g30--estimado-a-medir-en-banco),
[25 §7](25-panel-consola.md), diseño cuerpo v5 §7 y CSV citado en §1.
Referencias primarias: [PJRC OctoWS2811](https://www.pjrc.com/teensy/td_libs_OctoWS2811.html),
[implementación T4](https://github.com/PaulStoffregen/OctoWS2811/blob/master/OctoWS2811_imxrt.cpp),
[WS2812Serial](https://github.com/PaulStoffregen/WS2812Serial/blob/master/WS2812Serial.cpp),
[SK6812MINI-E datasheet](https://www.adafruit.com/product/4960) (verificar el lote).
La fuente efectiva del driver es el paquete local Teensyduino1.160.0, no master.
