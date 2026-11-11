# 23 · NeoTrellis: retorno animado SYNTH / DRUM

2026-11-10 · extensión explícita autorizada por la tarea LEDs1. Spec y teoría
escritos antes del código. No modifica los contratos fuente 17–22 ni 00–06.

## 1. Contradicción y alcance

19 §8 propone velocity=código de paleta 0–8; 21/22 usan CC20–22 RGB y note-on
48–79 como aplicación. Son incompatibles: **no interpretar implícitamente velocity
como paleta**. Esta extensión versionada vale sólo para SYNTH/DRUM. Conserva el
receptor RGB existente, con CC23=23 como opt-in; no implementa G05/E5 ni LEDs de
teclas no cableadas. Sin opt-in conserva exactamente el retorno RGB legado.
Canal16 (mido15), nunca eco musical en canal1. El protocolo M4 no cambia; véase la excepción de receptor en §6.

## 2. Dónde se dibuja y por qué

Pi publica estado y eventos; `PadLedReturn` dibuja en el **loop de Teensy**, a
34 ms (29,41 fps). Evaluación de 30–60 fps: 60 fps no aporta cuadros visibles
si el contrato limita el UART a30Hz; cuesta el doble. Se elige una sola evaluación
de32pads por slot de34ms, sólo transmitir si RGB cambió. No catch-up de cuadros
perdidos. Tiempo entero unsigned ms, sustracción modular, sin delay, float,
heap, Wire, FastLED, ISR ni sección con interrupciones desactivadas. Es código
custom; no es Teensy Audio Library oficial. Audio/DMA/USB conservan prioridades
de18 §2; no se toca ADC, pins, audio graph ni callbacks de audio Teensy.

Alternativas: render Pi facilita prototipos pero envía todos los cuadros y sufre
jitter Qt/USB; render M4 descarga Teensy pero exige desplegar otro firmware y
sincronizar allí heartbeat/protocolo. Teensy recibe estados compactos y sostiene
fades aun si Qt pierde un cuadro. Inconveniente: comparte CPU con audio; el coste
real y `loop_max_us<2000` de18 §5 necesitan banco físico, no se certifican con build.
Objetivo añadido: render32pads<100µs en T4.1; medir con DWT, audio activo y STATS.

UART Serial7 1Mbaud 8N1: 96 RGB7 +4 bytes BinaryProtocol =100 bytes, 1ms de
wire por cuadro; 1000/34=29.41kbit/s, **2.94%** de UART. `availableForWrite`
antes del write íntegro, sin espera, reintenta último cuadro si no cabe. Reserva
existente256B TX; estado del motor <2KiB fijo. USB snapshot32×7 mensajes=224
mensajes/896 bytes USB MIDI (12 bytes MIDI serial equivalentes por estado RGB
legado,21 por estado extendido); diferencias coalescidas≤29.41Hz, heartbeat2Hz.
Snapshot periódico1s y tras reconectar evitan estados antiguos; triggers con serial
no se repiten al reenviar. Techo de mensajes por frame:32×7 + controles comunes.

## 3. Extensión MIDI LED23

Todos los bytes son7bit. CC23=23 activa y **reinicia** escena/latches/seriales;
se envía al cambiar modo o puerto. CC24 modo0=vacío,1=SYNTH,2=DRUM (sweep
si cambia). CC25 brillo0..31, default28/127=22.05%; techo31/127=24.41%.
CC26 animaciones0/1. CC27 flags running1/record2/queued4. CC28 beat serial
módulo128: cambio inicia pulso80ms; no extrapola reloj. CC119 contador2Hz,
sólo mientras hay telemetría host reciente (<1s), timeout local1500ms.

Por pad, CC20/21/22 RGB7 normalizado0..127, CC29 flags, CC30 nivel0..127,
CC31 serial evento, luego note-on48+pad velocity1 aplica atómicamente:

| Flag | Significado |
|---|---|
|1|seleccionado; mínimo de luminosidad visible|
|2|mute, solo excluido, grupo mute o choked; atenúa y cancela flash|
|4|held (SYNTH) / step activo (DRUM)|
|8|root (SYNTH)|
|16|playhead (DRUM)|
|32|evento hit; nuevo serial inicia decay180ms, pico según CC30|

RGB por pad es color de rol/grupo. Nivel controla held/step/hit, no modifica audio.
Al desaparecer held: fade lineal180ms al nivel de escala. Serial evento por pad
0..127 cambia en cada hit host; resync en mismo modo no retrigger. Cambiar puerto
reinicia seriales explícitamente. Nota off/vel0 limpia pad. Rechazar flags fuera
0..63 y modos desconocidos. Brillo se aplica al **final** de composición, de forma
proporcional (preserva matiz), incluso blanco/sweep/fallo. Con animations OFF se
conservan held, raíz, selección, pasos, playhead y record/queued estáticos; sin
sweep, fades, flashes ni pulses. Brillo0 apaga también aviso ámbar.

Pérdida heartbeat: toda la grid ámbar fija; recuperación devuelve último estado
y Pi reenvía snapshot completo. Antes de primer heartbeat hay gracia1500ms desde
opt-in. Puerto/host caído nunca conserva un flash infinito. M4 reconectada recibe
último frame completo. Modo0 inactivo no muestra alerta de host. Sweep de arranque/cambio modo dura480ms (<600ms), aditivo
sin esconder estados musicales prioritarios.

## 4. Composición musical

DRUM: filas0/1 pads0..15; filas2/3 steps0..15 de lane seleccionada y página
actual (16..31 en página2). Color de pad por grupo, idle tenue, selección clara;
pasos en ese color con brillo proporcional a velocity, apagados fuera de length.
Playhead blanco sobre el step sólo en PLAY; STOP lo quita. Record añade rojo en
pad seleccionado. Queued añade pulso blanco en borde, estático si animations OFF.
Mute/grupo mute/solo excluido/choked atenúan todo lo que les corresponde.

**No deducir golpes desde playhead, edición, pad press ni medidor.** Audio host
publica en `clock.led_hits[16]` contadores acumulativos, `led_velocity[16]`,
`led_choked[16]`. El contador aumenta al primer sample no cero de una voz ROOM
que pasa mute/solo y ganancia de grupo, después de probabilidad y choke. Cue no
lo aumenta. Voz choked antes de sonar no produce flash; choke posterior conserva
el hecho y atenúa el pad. Estado fijo POD por voz/pad, SPSC clock existente,
JSON sólo en control; sin cola nueva ni asignación RT. Si múltiples golpes de
un pad ocurren entre snapshots se representa el último (coalescible, no musical).
Baseline al entrar/reconectar: no animar historia. Timestamp/latencia visual depende
de telemetry30Hz+GUI34ms+UART34ms; no es un reloj ni ACK de audio.

SYNTH: escala por `ScaleKeyboard`, roots más brillantes; held en color de su parte
original y velocity enviada, aunque cambie parte/layout antes del release. El
observador LED del transporte nota éxito de envío, sin cambiar routing/refcounts;
no afirma voz audible o sustain DSP. Release180ms. Cambio parte se ve por color
de escala y selección del primer pad del borde de esa parte. Clock confirmado
del host produce pulse de borde en cada beat (4 semicorcheas), sin QTimer musical.

## 5. Settings y validación antes de implementación

`local_settings.py`: `led_brightness`0..31 default28, `led_animations` bool default
true, guardado QSaveFile junto a Hands/velocity; inválidos usan defaults. Settings
incluye touch y soft keys encoder5/6, sin mover pad velocity encoder4.

Unity native: tiempo/rollover, decay y release, snapshots, serial dedup, cap
proporcional, fps/diffs, heartbeat/recovery, animations off, overload/coalescing.
Pytest: mensajes canal16 y rangos, selección/steps/páginas, sólo hits host, snapshots,
heartbeat freshness, preferencias persistidas, integración sin tocar ruta musical.
Preview offline ejecuta **el mismo C++** con fixtures del generador Python y
exporta HTML con timeline; no imitación del algoritmo en JS/Python.
Regresiones: `pio run -e consola_uno`, `pio test -e native` en firmware-teensy,
pytest GUI completo; CMake/ctest host por cambios de telemetría. No se flashea.
Banco pendiente: brillo visual/corriente, pérdidas UART, jitter visual, audio USB
activo y `loop_max_us`, build no demuestra ausencia de xruns en hardware.

Referencias de contrato: [19 §8](19-nucleo-e5-spec.md#8-retorno-de-leds-g05),
[18 §2/5](18-teensy-unica-firmware.md), [17](17-mapa-pines-teensy.md),
[21](21-modo-synth.md), [22](22-modo-drum.md). Implementación M4 primaria:
`processor/lib/neotrellis_m4/UartInterface/UartInterface.cpp` y
`NeoTrellisController::applyGridColors7bit`; BinaryProtocol 100bytes.
La rampa lineal e interpolación son funciones acotadas por duración, no filtros
DSP; unsigned modular usa el contrato de C++17 [conv.integral]/[expr.add].

## 6. Excepción justificada: receptor M4 existente

Revisión de `NeoTrellisController.cpp` antes de editarlo: el bulk7 llama
`applyPadColor7bit`, que escribe `lastPadUpdateMs`, y el siguiente bulk rechaza
cada pad durante100ms (`PAD_SUPPRESS_MS`). Con34ms sólo acepta uno de cada3
cuadros (~9.8fps), y puede perder el cuadro final dejando una cola vieja. Además
`gamma7To8(pow(x,2.2))` seguido por `setBrightness(63)` vuelve negro todo RGB7
pequeño;31/127 termina ~3/255, no cerca del24%. Son impedimentos funcionales,
no una razón para llevar el motor a la M4.

**Excepción mínima sólo con `GF_CONSOLA_S_M4`:** bulk7 no suprime cuadros y
convierte el RGB7 ya acotado como nivel PWM lineal, compensando el scaler global
NeoPixel. `raw8 = min(255, floor(min(v7,31)*255*256/(127*64)))`; Adafruit
NeoPixel brightness63 usa `(raw8*64)>>8`, resultado máximo62/255=24.31%,
valor28→56/255=21.96%. Green white-balance0.92 existente sólo reduce ese canal.
El techo también se cumple con payload malformado. No cambia press/release,
velocity, framing, baud, otros comandos ni el build GroovePush sin ese define.
No añadir animaciones, asignaciones o esperas M4; sigue mostrando32NeoPixels
(~960µs allí, sin enmascarar IRQ de Teensy). `ConsolaLed.h` permite probar el
mapeo entero nativamente. Hay que desplegar el build M4 de consola para ver la
cadencia/niveles nuevos; receptor antiguo seguirá aceptando el mismo UART pero
retendrá supresión/gamma. Preview muestra RGB7 del motor, sin afirmar luminancia
física; ganancia de pantalla×3 sólo ayuda a inspección en monitor.
