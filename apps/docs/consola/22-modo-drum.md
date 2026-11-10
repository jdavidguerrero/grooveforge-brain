# Modo DRUM: racks, sampler y reloj de la consola

2026-11-10 · Juan · spec previa al código · ADR-007/008; contratos 14/16/18/21;
19 §12 D1/D2/D4. Alcance autorizado por la tarea DRUM, anterior a E5.

## 1. Propósito y criterio

Mode Select → DRUM → RACK: tocar y editar un kit de 16 pads, secuenciarlo y hacer
jam. Sampler, secuenciador y transporte viven **dentro de `gf-synth-host`**, junto
a las cuatro partes: un callback C++, un contador de muestras, ningún DSP Python.
E5 reutilizará esta base; DRUM no implementa SetState, ACCEPT ni arreglos E5.
La GUI sigue SYNTH (PySide6/QML, cliente local, modelos por frame, puente de
superficie inmediato). Todo queda en Pi/Mac, sin cuentas, nube ni telemetría remota.

El motor entrega sonidos candidatos bajo pedido: no escribe ritmos por Juan,
no cambia sus pasos, niveles ni sonidos al terminar un render. KEEP/LOAD explícito
incorpora una propuesta. Cambiar raíz vuelve a renderizar ENGINE; no afina
silenciosamente muestras importadas. Theory: `../theory/percusion-organica-dsp.md`,
`percusion-kits-808-organica.md` y `percusion-generos-tonalidad.md`.

## 2. Rack y almacenamiento

`GF_DRUM_DATA` / `--drum-data`: `/data/drum` en Pi (configurar explícitamente),
`~/.local/share/gf-drum` en desarrollo. Nunca samples de terceros en git.

```
<data>/racks/<id>.json
<data>/samples/<sha256>.wav
<data>/sources/<id>.json
<data>/jobs/<id>.json
<data>/last-drum.json
```

JSON UTF-8 `schema_version=1`, ID inmutable, nombre, raíz (`C`…`B`, sostenidos/
bemoles), modo, tags, fuentes, 16 pads, 8 patrones y mezcla. Rutas de WAV relativas
al data dir, hash SHA-256 del archivo completo. Validar hash, límites y contención
real (también symlinks) antes de publicar. WAV mono/estéreo, float32 a **48 kHz**
residentes; PCM16/24/32 y float32 de origen se decodifican/remuestrean fuera de RT.
El servicio usa resample_poly; el lector host admite PCM y remuestreo lineal
offline para manifiestos externos. No streaming ni lazy decoding en audio.

| Objeto | Campos / límites v1 |
|---|---|
| Rack | `schema_version`, `id`, `name`, `root`, `mode`, `tags`, `sources`, `pads[16]`, `patterns[8]`, `groups[4]`, `bpm`, `swing`, `selected_pattern`0..7 (opcional, default0) |
| Source | `id`, `kind:ENGINE/IMPORTED/RECORDED`, `name`, `licence_note` libre, receta/semillas/versiones si ENGINE. RECORDED reservado, sin captura |
| Pad | `name`, `source_id`, `category`, `gf_note?` (metadato GF Perc Map), `layers[0..4]`, `layer_mode:velocity/round_robin`, `params`, `recipe?` |
| Layer | `path`, `hash`, `velocity_min/max` inclusivos (1..127); RR rota por golpe dentro del rango. Velocidad sin rango se rechaza al cargar; no huecos ambiguos |
| Params | `tune` −24..24 st, `cents` −100..100; `attack/hold/decay` ms; `env_mode` AD/HD; `filter_mode` LP/HP, `cutoff` 20..20000 Hz, `resonance` 0..0.95; `start/end` normalizados, `fade_in/out` ms; `pan` −1..1; `send` 0..1; `velocity_curve` linear/soft/hard/fixed, `velocity_level/tone` 0..1; `choke` 0..16 (0=off); `group` 0..3; `level` 0..2; `mute/solo` bool |
| Group | `name`, `level` 0..2, `mute`; inicial KICKS/SKINS/WOODS/METALS. El pad puede cambiar grupo sin mover su muestra |

Ejemplo mínimo de referencia (el servicio completa 16 pads y patrones vacíos):

```json
{"path":"samples/0123456789abcdef…64hex.wav","hash":"0123456789abcdef…64hex","velocity_min":1,"velocity_max":127}
```

Máximo **256 MiB de PCM residente total**, incluyendo rack preparado, activo y
preview; rechazar antes de reservar si no cabe, conservar el rack anterior.
**64 voces totales** preasignadas (ROOM+CUE), robando la más vieja **del mismo bus** cuando se
agotan; CUE sin voz libre ni víctima CUE se rechaza sin cortar ROOM; contador visible. No representa las 64 voces *por bus* propuestas en19.
Cambio de rack: preparar en control, swap en inicio de bloque, cortar voces
anteriores, ACK y liberar fuera de audio. No hacer delete ni refcount final en RT.
Choke por bus: cerrado/abierto del mismo hat grupo1; cue jamás corta la sala.

ENGINE: APIs `gf_perc.generos.sintetizar_en_genero`, `caracteres.sintetizar_caracter`
y `kit.sintetizar_voz`. Se guardan receta, material, género, root/mode y semillas.
Los pads vacíos completan las 13 voces disponibles hasta16, sin inventar sonidos.
GF Perc Map es metadato (kick51, snare48, hats52/53…), no el mapa físico48–79.
IMPORTED: escanear carpeta local, categoría heurística editable, convertir y
opcionalmente recortar silencio, conservar nombre y nota de licencia por fuente.
Más de16 muestras forman varios racks; ningún archivo se pierde por llenar un kit.
Tribe puede importarse localmente con `licence_note="pending"`; esto no autoriza
redistribución ni decide licencia. No incluir GeneralSans en commits.

## 3. Secuenciador y transporte

16 lanes (una por pad), longitud independiente **1..32** semicorcheas, polimetría.
8 patrones A–H. Cada step: `velocity`0..127 (0=off), `probability`0..1,
`micro`−0.5..0.5 pasos. Semilla PRNG reproducible; probabilidad decide cada vuelta.
`set_steps` cambia una lane del patrón indicado; no copia ni reemplaza otros
patrones. GUI muestra16 pasos, página1/2 y lanes1–8/9–16.

Clock C++ sample-accurate a48k, contador absoluto u64 como string JSON, origen
PLAY, 4/4, BPM60..200. Frontera de step n =
`origin + round((n + swing_odd + micro) * 60*48000/(4*bpm))`;
swing0..0.5 retrasa impares. Calcular desde origen, no sumar intervalos redondeados.
Micro negativo puede anticipar un step: preparar el próximo evento antes de su
frontera; primer step anticipado se recorta al origen. Cambios de parámetros/
steps se aplican al inicio del bloque, sin tocar eventos ya sonados.

PLAY desde detenido empieza en el uno; PLAY corriendo es idempotente. STOP corta
voces ROOM y posición musical, no reproduce cola al reiniciar. Preview CUE sigue
independiente del transporte. BPM sólo editable detenido en v1; tap en el BPM superior abre el control auxiliar
(touch −/+ y encoder1, cierre por touch/foco hardware). Link reservado.
Cambio de patrón corriendo queda queued hasta el **siguiente compás estricto**,
reinicia todas sus lanes en esa frontera, sin disparos viejos posteriores ni doble
golpe. Detenido, selección inmediata. Transporte nunca depende de QTimer/MIDI clock.

REC ARM habilita overdub de golpes FSR/pads en patrón activo, cuantización
1/16 o1/32 (redondeo desde muestra de entrada; micro representa medios pasos).
No genera notas. Saturar la cola de hechos desarma REC y cuenta record_overruns,
con error visible; el golpe sigue audible. Golpe en step ocupado: conservar edición existente; desarmar no
borra nada. Edición explícita del step sí puede reemplazarlo. Audio emite hecho
`recorded` con patrón/lane/step/velocity/micro para actualizar/persistir el espejo.

## 4. DSP, buses y reglas RT

Resampling por voz con interpolación lineal y factor `2^((tune+cents/100)/12)`;
AD/HD, fades y ventana start/end, filtro sencillo LP/HP con resonancia acotada,
curva velocity→nivel/tono y paneo. Cuatro buses de grupo se suman a ROOM `mix_L/R`;
las salidas synth `part1..4_L/R` mantienen contrato. `cue_L/R` **sólo audición**,
con sus propias voces/choke, sin sumar a mix. `send` se conserva como envío
reservado (no hay reverb/echo nuevo ni retorno conectado en esta entrega).
Headroom mediante nivel de pad/grupo; clipping y voice steals contados, no limitador
oculto. Medidores de pads/grupos/room/cue desde audio, nunca desde faders.

PipeWire nativo con `pw_core` conectado antes de `pw_filter_new` (fix existente);
12 salidas DSP, quantum real negociado, default256. JACK y offline conservados.
Cue se conecta explícitamente a USB Teensy playback1/2→SGTL5000; ROOM al Audio+.
PipeWire cruza48k al USB nominal44.1k. No autoconectar cue a sala ni mix+partes
simultáneamente. No modificar backend a costa de perder el core conectado.

Callback: trabajo acotado (16lanes×32steps,64voces), buffers/voces/colas fijos,
FTZ, sin locks, malloc/new/delete, JSON, IO/log, decodificación, resampling offline
ni Python. SPSC control→audio y audio→control; el único productor es control,
que serializa clientes. Swaps por punteros con ACK de retiro; liberar en control.
Cola llena→busy, nunca éxito falso. Hechos fiables con contrapresión; telemetría
coalescida30Hz y pérdidas contadas. Generación/import en worker Python separado,
no hilos de síntesis compitiendo con callback dentro del host.

## 5. Extensión del contrato14

Aditiva `drum_version=1`, capacidad `drum`; conservar `hello.version` y todos los
comandos synth. `hello` anuncia límite RAM/voces y nuevos puertos. Mismo socket,
JSONL, id/ok/error, límites actuales64KiB/256KiB y permisos0600. `pad`, `pattern`,
`lane`, `step`, `group` son **0-based**; `part` synth sigue1-based.

| cmd | Campos / semántica |
|---|---|
| `get_drum_state` | snapshot rack/pads/patrones/grupos, RAM, transport, sample_counter, playhead, queued, rev |
| `load_rack` | `path` relativo al data dir; preparar antes de swap; ACK aplicado |
| `set_pad_param` | `pad,param,value` validado por tabla del host; eco valor aplicado |
| `list_pad_params` | páginas de8, rangos/unidades/opciones; única tabla GUI |
| `pad_trigger` | `pad,velocity`1..127, `cue?` bool; pruebas/fallback inmediato, ACK aplicado |
| `set_pattern` | `pattern`0..7; ACK de armado; evento clock confirma cambio |
| `set_steps` | `pattern,lane,length,steps[32]`; copia POD validada en control |
| `transport` | `action:play/stop`, `bpm?`, `swing?`; `record_arm` con `armed,quantize`16/32 |
| `set_group` | `group,level?,mute?,name?`; `set_pad_sample` con `pad,layer` reemplaza sólo muestra por elección explícita |
| `list_racks` / `save_rack` | `name?`, `overwrite?`; listado local / snapshot atómico fsync+rename; sin overwrite implícito |
| `list_drum_samples` | `offset?,limit?` (default64,≤128), `has_more,next_offset`; páginas≤120KiB de manifiestos de fuentes locales, búsqueda/filtro GUI; sin audio embebido |
| `preview` | `layer` o `pad`, `stop?`; layer=decode control/raw, pad=DSP actual del pad; voces sólo cue, no cambiar rack |
| `import_folder` | `path,licence_note,trim?`; ACK job, eventos `drum_job` progress/done/error |
| `generate_variations` | `pad,count`1..8, `spread`0..1; ENGINE only; job, resultados sin cargar automáticamente |

Subscribe añade `drum`, `clock`, `drum_meters`, `drum_job`; `drum` entrega snapshot
al cargar rack y deltas `kind:pad_param/steps/group` al editar; `recorded` es fiable.
No reenviar los8patrones completos por cada giro de encoder; procesar otra
petición de un cliente sólo cuando su salida pendiente se haya drenado; clock a30Hz incluye running, posición y patrón activo/
queued, epoch/rev; métricas no sustituyen hechos. Jobs invocan CLI local instalado
con `--drum-kit-python`/`--drum-kit-script`; ningún shell/interpolación de rutas.
Errores: archivo/hash/formato/tipo/rango/RAM/cola/worker ausente. Falla conserva
estado anterior. Preview externo ocupa slot preparado con ACK de retiro propio.
MIDI de grafo **canal10** notas0..15 = pads0..15; así no choca con synth1..4 ni
superficie canal1. La GUI traduce notas físicas; no conectar GF Consola canal1
directamente al host en DRUM. MIDI timestamps se conservan dentro del callback.

## 6. Superficie y pantallas

| Control | DRUM |
|---|---|
| M4 notas48..63 | Pads1..16, golpe inmediato + selección visual; velocity física si disponible |
| M4 notas64..79 | Steps1..16 de lane seleccionada/página actual, conmutar off/velocity100 |
| FSR36..39 | Kick/snare/hat/perc, asignación de pad por categoría, velocity íntegra; detector18 umbrales600/400 ADC12 |
| CC16..23 | Parámetros del pad/página, steps o mixer según pantalla, relativo y clamp por tabla host |
| Jog CC35/clic | Browser desplaza/carga; SHIFT+jog foco auxiliar touch como SYNTH |
| Faders CC24..27 | Grupos KICKS/SKINS/WOODS/METALS; no restaurar nivel al generar/cargar sonido |
| PLAY/STOP/REC | Transporte/record arm del host; botones del lector18, sin reasignar notas80..122 |
| Manos CC33/34 +108/109 | Cutoff/send del pad elegido, gate presencia; expresión28/29 equivalente sin gate |
| MASTER/CUE/IN | Misma ruta RoomControls de SYNTH; no inventar @GF MASTER |

Golpes por MIDI directo antes del frame; fallback socket independiente con ACK
reader y error visible. Navegación/encoders y LEDs por frame,≤30Hz retorno MIDI.
Reutilizar formato RGB canal16 de SYNTH21 para pads/steps/playhead; deduplicar.
La entrega añade receptor RGB mínimo en surface::poll (canal16), snapshot M4
a≤30Hz y brillo máximo31/127; no cambia notas ni callbacks del Brain. No se
afirma feedback físico hasta compilar/flash y medir Teensy+M4 (contradicción §9).

Diseño autoritativo: `design/claude-design-drum/Console Drum Screens.html`, extraer
`script[type="__bundler/template"]` JSON y notas; no reinterpretar página empaquetada.
Geometría1280×720: TopBar56, contenido568, SoftKeys96; x=8+i160, columnas152.

| ID diseño / registro | Contenido |
|---|---|
|46 / DrumRack|Header44, pads2×8 de152×144 gap8; selección20; parámetros8×168; Pads/Steps/Perform/Browse/Mixer/Variations/Save/Back|
|46b/46c / DrumRackStates, DrumRackEmpty|RAM/archivo/loading visibles y no reproducibles; rack vacío con acciones de carga/import/generación|
|47 / DrumPad|Waveform columnas1–5; pitch/env/filter/velocity a derecha; 4layers; handles touch seleccionan, encoders editan|
|48/48b / DrumBrowser, DrumBrowserEmpty|Sources312, sounds952; búsqueda/categoría/material; preview siempre CUE; vacío explica import local|
|49 / DrumImport|Progreso/skips columnas1–5, RAM/licence6–8; volver al jam no cancela job|
|50/50b / DrumSteps, DrumProbability|A–H44, controles32,8lanes54 gap4; capas VEL/PROB/MICRO;32steps en2páginas|
|51 / DrumPerform|A–H; grid4×8 (2filas pads,2steps);4FSR con últimos golpes; mute/solo hold+pad|
|52 / DrumMixer|4grupos312×140;8strips152×404 banco1/2; regroup explícito|
|53 / DrumSave|Rack/pattern, nombre/tags/key, fuentes/licence y teclado; overwrite explícito|
|54 / DrumVariations|Original+progreso120,8candidatos152×404; cue/KEEP/discard; sólo ENGINE|
|55 / ModeSelect|4tarjetas284, gap16; Controller/Standalone/Synth/Drum; RAM/rack DRUM reales|

English UI. Todos los destinos aparentes usan TouchTarget/TouchController y tienen
equivalente hardware; touch auxiliar, no notas musicales en pads. Preview/datos
fake rotulados; errores no se muestran como éxito. Screenshots de cada entrada.

## 7. Persistencia y recuperación

Guardar rack/patrón es explícito; checkpoint de trabajo atómico en control con
params/steps/grupos/último rack. Tras reinicio restaurar sólo datos válidos;
`running=false`, `record_arm=false`, queued vacío y voces vacías **siempre** (D4).
No reemitir comandos antiguos al reconectar GUI. Rechazo de archivo/RAM conserva
rack activo; fallo de worker conserva candidatos ya listos y ediciones de Juan.
GUI caída no corta secuenciador; host caído silencia y watchdog reinicia detenido.
Sin promesa de continuidad de fase. Guardado con debounce≤100ms; fallo de disco
visible, STOP sigue disponible.

## 8. Plan de pruebas

Host: CMake Release+ctest existente; WAV PCM16/24/32/float, hash/path inválido,
RAM rechazo antes de swap; exactitud de sample a122BPM con bloques distintos,
polimetría, swing/micro negativos, prob0/1, selección layers/RR, choke aislado por
bus, cambio A→B en bar y sin evento viejo, record sin pisar edits, STOP/restart.
Hook allocation/free como host_rt durante sampler/clock/swaps; no prueba syscalls.
Benchmark Mac64voces activas, media/p99/máximo de callback/tiempo de bloque,
flags y quantum registrados (estimación de CPU, no certificación Pi).

Servicio: WAV y hashes reproducibles, raíz conservada en variantes, licencia
persistida, trim/saltos de decode, más16 muestras sin pérdida, cancel/error worker
sin editar rack. GUI: cliente real/fake, ACK/errores/reconexión, todos los QML,
touch y mapa FSR/M4/encoders/LED, capturas46–55. Regresiones:
`python3 -m pytest apps/consola/gui-console/tests -q`, tools/consola pytest,
perc-synth pytest y firmware `pio test -e native` sin cambios.

Pi:60min host synth+64voces+GUI+USB cue, quantum256;0xruns, CPU media≤50%,
p99≤70%, máximo≤90% de un núcleo, RAM≤256MiB; ADR-00760fps y1000ev/s.
Loopback confirma aislamiento cue/room y latencia FSR/pad separada. En256 sólo
el quantum dura5.33ms; p95<5ms no certificado.128 opt-in después de medir RT.

## 9. Preguntas para Juan y contradicciones (sin resolver fuentes)

- ¿Aprobar64voces totales o exigir64por bus como propuesta19§7? Esta entrega
  fija64totales por el alcance de la tarea; E5 debe revisar antes de ampliar.
- ¿Afinación ENGINE estrictamente raíz en todas las voces o regla de género
  existente (kick quinta si raíz fuera41.2–65.4Hz; congas grados; ruidos sin nota)?
  El render conserva API/criterio vigente y registra frecuencias; no altera recetas.
- ¿Retorno LED RGB21 o paleta19§8? Ambos canal16 pero velocity significa otra
  cosa. Se reutiliza21 y se añade receptor en la biblioteca surface; el documento18
  conserva el alcance previo. No se declara recepción física sin flash/medición.
- ¿Send a qué FX y política de solo/mute mientras se cambia modo? Send reservado;
  no inventar efecto audible. ¿Preview de varias layers o sólo layer elegida?
- ¿Guardar PATTERN separado del rack y bancos de más4layers/RR? Formato v1 guarda
  los8patrones con el rack; la pestaña permite guardar copia explícita del patrón.
- ¿Licencia Tribe y otros imports? Nota libre no valida derechos; nada redistribuido.
- 14§6 excluye sampler/grabación; extensión de §5 documenta DRUM sin grabación
  de audio. 19§2/§7 describe gf-sampler separado: D1 prevalece; no reescribir19.
- 19§7 protege voces graves y exige no duplicar kick/bajo al sustituir capas.
  DRUM es un rack libre con robo oldest del mismo bus y choke configurable; no
  certifica esa política musical E5. E5 debe incorporarla antes de reutilizarlo
  para capas ROOM aceptadas.
- 07§5/E5 usan RHYTHM/BASS/MUSIC/FX; DRUM usa KICKS/SKINS/WOODS/METALS editables.
  Son contextos distintos; no sustituir el mixer de E5 ni los SSoT00–06.
- Diseño Browser vacío dice `/data/samples`, tarea pide racks en `/data` y default
  configurable. UI muestra directorio efectivo; no mover archivos de Juan.
- Diseño54 asigna clic de encoder a audición de cada tarjeta y a soft keys distintas
  en la misma columna. Provisional: clic simple=CUE tarjeta; SHIFT+clic=soft key,
  touch conserva ambas acciones. Confirmar con Juan; no cambia el diseño fuente.
- Diseño47 incluye RESO/env AD-HD; sampler simple no equivale a filtro analógico.
  Pitch por resampling cambia duración, contrario a re-render afinado de la teoría:
  son controles manuales distintos, sin prometer síntesis live ni time-stretch.
- CLAUDE de apps/consola describe GroovePush/Live/serial y nube; ADR007/008 y esta
  tarea definen consola local. No modificar ese archivo ni inferir servicios cloud.
