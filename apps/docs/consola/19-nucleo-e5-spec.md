# Consola: spec del núcleo E5

> **Fecha:** 2026-11-10. **Estado:** propuesta para aprobación de Juan, antes de código.
> Base de lectura: `codex/nucleo-spec`, `ec83df0`. Solo documentación; no certifica hardware.
> Autoridades: [01 §3, E5](01-prototipo-mesa.md), [00 §4–§5.1](00-concepto.md),
> [07 §5](07-gui.md), ADR-007 **Decision**, ADR-008 **Decision** puntos 1–8.
> Los contratos nuevos de este documento son propuestas; las diferencias con otros specs quedan en §11.

## 1. Qué entrega E5

**Drums del sampler del Pi**, sobre un mismo kick: A suena en ROOM, B se oye en CUE,
Juan pulsa **ACCEPT** y B sustituye A en el uno de la próxima frase de **8 compases**.
SKIP pide otra opción; cada decisión deja registro. Primera pantalla Qt conectada:
**LIVE · SESSION** (ID legacy 5), con ROOM, NEXT/CUE y cuenta hasta la frase.
Es el alcance P2 de la [auditoría §8](auditoria/auditoria-fable-2026-11.md), no un set completo.

En E5 están habilitados **KICK y DRUMS**; DRUMS contiene clave, clap/snare y hats,
sin duplicarlos en PERC/TOP. Los ocho roles existen en el contrato para conservar
la GUI. PERC, TOP y las cuatro partes melódicas quedan vacíos/deshabilitados.
Dos candidatos como máximo por rol; A/B describen la comparación audible, no dos decks.
También la primera capa y el kick requieren ACCEPT explícito antes de arrancar.

Fuera: synths dentro del set, entradas externas, tocar/grabar FSR, editor de notas,
arreglo automático, tracks completos, transiciones SWAP/BREAK/CUT de varios roles,
roll/riser/subdrop/echo, EQ/envíos, biblioteca navegable, M1 entrenado, Link, HÍBRIDO,
ESTUDIO/CONTROLLER, MIDI de red, CV, PCB e instalación/despliegue. Las demás pantallas
conservan su alcance previo; no se presentan como cableadas por E5.
**Sin crossfader** ([00 §5.1](00-concepto.md), [03 §4.1](03-arquitectura-capas.md)).

Invariantes: **nada entra en ROOM sin ACCEPT**; sin decisión se repite lo aceptado,
sin cambiar de capa ni sección. El motor ofrece borradores, nunca compone un track
por Juan, pisa sus ediciones ni recupera un nivel que él movió
([motor v2 §1–§3 y §6](../produccion/motor-asistencia-v2.md); auditoría C8/P7).
Todo funciona offline dentro de Pi/Mac; no hay nube ni salida a terceros.

## 2. Procesos, propiedad e IPC

| Proceso | Propiedad y responsabilidad |
|---|---|
| `gf-sampler` (nombre propuesto, C++17) | Audio PipeWire, contador de muestras, capas ejecutadas, RAM de samples, colas de cambios y dos buses. Es ejecutor; devuelve hechos de audio, no elige material |
| `gf-nucleo` (Python de control) | **Único escritor del SetState** (§3), validación, aceptación, registro/checkpoint, adaptación MIDI, LEDs y supervisión lógica. Abre `Superficie` una sola vez; reconecta por identidad USB |
| `gf-motor` (worker Python separado) | Generadores existentes y backend en memoria (§6). Copia de contexto y borradores propios; sin autoridad sobre ROOM, reloj, niveles o GUI |
| GUI QML/PySide6 | Modelos Qt como proyecciones y estado local de navegación/foco. Envía intenciones; no genera ni mezcla audio |
| `gf-synth-host` (C++ existente, opcional en E5) | DSP, presets, parámetros y estado de sus cuatro partes según [14 §1–§4](14-synth-host-contrato.md). No es dueño del set; ausente no bloquea drums |

Audio+ conduce el grafo a 48 kHz. `gf-sampler` y, después de E5, el host synth
son clientes de ese mismo grafo. No abrir Audio+ a la vez por ALSA exclusivo y PipeWire.
En E5 no se conecta `mix_L/R` del host a sala ni se reenvía MIDI de superficie al
host: CC16–23 y notas de pads se solapan con controles/notas de synth.

Control local: AF_UNIX, UTF-8 **JSON por línea**, sockets modo 0600 bajo
`$XDG_RUNTIME_DIR/gf-{nucleo,sampler,motor}.sock`; en Mac sin XDG,
`/tmp/gf-<servicio>-<uid>.sock`. Un lock impide dos dueños del mismo set/sampler.
Se reutiliza el estilo de [14 §1 y §3](14-synth-host-contrato.md), no el protocolo
TCP con longitud binaria de `ClienteLive`. `hello` negocia `version=1`, capacidades,
`instance_id` UUID y, en audio, `audio_epoch`, frecuencia y quantum real.
`get_state` devuelve snapshot completo; `subscribe` acepta `state`, `clock`,
`meters`, `health`, `decision`. Un snapshot y la suscripción comparten un punto
`rev` para evitar huecos entre lectura y escucha.

Límites propuestos: 64 KiB/línea, 8 clientes por servicio, salida 256 KiB/cliente;
assets/MIDI completos se referencian por hash, no se meten en eventos de cuadro.
Cliente lento: coalescer clock/meters; si no cabe estado fiable, desconectar y
exigir snapshot. Cola de decisiones llena → `busy`, sin perder ACCEPT/STOP.

Sampler: `prepare_layer {layer_id,manifest_hash}` → `ready {layer_id,handle}` solo
después de cargar/validar todo; `schedule {op_id,audio_epoch,target_sample,changes}`
→ `scheduled` tras armar la cola; `cancel {op_id}` → `cancelled` o `too_late`.
`changes` es una lista de `{role,layer_id}` aplicada atómicamente. `applied`
incluye `op_id`, muestra exacta y capas resultantes. `set_level`, `set_mute`,
`set_cue`, `set_transport`, `get_state` completan el control de audio.
`set_cue {role,layer_id,on}` nunca añade al bus ROOM. Cada operación y respuesta
lleva ID correlacionado; los hechos del sampler llevan `audio_seq` creciente.

**Regla RT (G16/G18):** ningún Python/Qt/motor en la ruta del callback. JSON,
disco, síntesis de samples, remuestreo previo, logs y destrucción viven en control.
Callback C++: buffers/voces/eventos preasignados, trabajo acotado, sin locks,
heap, filesystem ni llamadas bloqueantes; colas SPSC de capacidad fija en ambas
direcciones. Intercambio de handles/punteros; liberar material retirado fuera de
audio tras confirmación. Telemetría acotada como en [16, «Arquitectura y tiempo real»](16-synth-host.md).

Servicios separados, `Restart=on-failure`; watchdog propuesto de **2 s** para
sampler/núcleo/motor. Solo alimentar si avanza audio/responde control o progresa
el bucle correspondiente; generación con plazo musical, no heartbeat fingido.
GUI conserva el servicio de [13, «systemd»](13-gui-pi.md); perderla no detiene audio.
Motor caído: repetir ROOM y conservar candidatos ya preparados. Núcleo caído:
sampler repite y puede ejecutar **solo** operaciones ACCEPT ya armadas; no genera otras.

Checkpoint atómico con fsync en `/data/sets/<set_id>/state.json`, escrito por núcleo,
tras decisiones/aplicaciones y niveles con debounce ≤100 ms. Tras reinicio del
núcleo: leer checkpoint + JSONL, consultar sampler y reconciliar por epoch/op_id;
no confundir un `scheduled` con un `applied`. Sampler conserva consulta de operaciones
durante su vida, incluso sin cliente. El núcleo publica el estado reconstruido.
GUI reconecta y pide snapshot; no reproduce su cola de intenciones anterior.
Si reinicia audio/Pi: nuevo epoch, cargar último ROOM confirmado y sus niveles,
conservar propuestas/ediciones en disco, **arrancar detenido**. PLAY explícito
reanuda en el uno; ACCEPT pendientes anteriores quedan cancelados con motivo.
No se promete continuidad de fase ni de colas de voces tras fallo de audio; si
muere el Pi, hay silencio (ADR-008 **Consequences**, auditoría G16).

## 3. Estado del set

Esquema lógico versionado. `u64` = entero 0..2⁶⁴−1, `f64` = número finito,
`Id` = UUID string, `Hash` = SHA-256 hexadecimal; `T?` admite null.
En JSON, u64 usa **string decimal** para no perder precisión en JavaScript/QML;
contadores pequeños e índices usan enteros JSON. IDs inmutables; mapas por rol
contienen siempre estas ocho claves, en orden de columna:
`KICK, DRUMS, PERC, TOP, BASS, CHORDS, LEAD, PAD`.

| Tipo | Campos exactos |
|---|---|
| `SetState` | `schema_version:int=1`, `set_id:Id`, `rev:u64`, `room_title:string`, `next_title:string`, `genre:string`, `key_name:string`, `clock:Clock`, `section:Section`, `roles:map<Role,RoleState>`, `layers:map<Id,Layer>`, `transition:Transition?`, `mix:Mix`, `health:map<string,Health>` |
| `Clock` | `audio_epoch:Id`, `sample_rate:int=48000`, `quantum:int>0`, `sample_counter:u64`, `transport:"stopped"\|"running"`, `origin_sample:u64`, `bpm:f64[60,200]`, `beats_per_bar:int=4`, `phrase_bars:int=8`, `bar:u64`, `beat:int[1,4]`, `phrase:u64`, `bar_in_phrase:int[1,8]`, `next_phrase_sample:u64`, `bars_to_phrase:int[1,8]` |
| `Section` | `index:u64`, `name:string`, `start_bar:u64`; inicial `1,"LIVE",1`; no cambia sola |
| `RoleState` | `enabled:bool`, `room:Id?`, `next:Id?`, `cue:Id?`, `options:list<Id>[0..2]`, `options_epoch:u64`, `pending:Pending?`, `generation:Generation`, `level:f64[0,1]`, `mute:bool`, `manual_level:bool` |
| `Pending` | `op_id:Id`, `command_id:Id`, `layer_id:Id`, `audio_epoch:Id`, `target_bar:u64`, `target_sample:u64?`, `status:"preparing"\|"scheduled"` |
| `Generation` | `job_id:Id?`, `status:"idle"\|"running"\|"failed"`, `context_hash:Hash?`, `target_phrase:u64?`, `deadline_sample:u64?`, `error:string?` |
| `Layer` | `id:Id`, `role:Role`, `title:string`, `source:"engine"\|"juan"\|"library"`, `parent_id:Id?`, `content_hash:Hash`, `manifest_path:string`, `pattern_bars:int>0`, `energy:int[1,5]`, `ready:bool`, `edited_by_juan:bool`, `context_hash:Hash` |
| `Transition` (reservado) | `id:Id`, `kind:"SWAP"\|"BREAK"\|"CUT"`, `status:"proposed"\|"armed"\|"running"\|"aborting"`, `start_bar:u64`, `end_bar:u64`, `step:int>=0`, `roles:list<Role>`, `accept_ids:list<Id>`, `manual_roles:list<Role>` |
| `Mix` | `groups:map<Group,GroupState>`, `master:f64[0,1]`, `cue_level:f64[0,1]`, `cue_room_mix:f64[0,1]` |
| `GroupState` | `level:f64[0,1]`, `mute:bool`, `manual_level:bool` |
| `Health` | `status:"ok"\|"warn"\|"down"\|"off"`, `instance_id:Id?`, `error:string?` |

Claves de health: `sampler`, `motor`, `surface`, `synth`; su ausencia física es off/down,
no material musical vacío. `ready` exige residencia en el epoch actual; tras reiniciar
audio se invalida hasta recargar. Section.index cuenta secciones decididas por Juan,
no frases; `bar-start_bar+1` es el compás dentro de sección.

`room` es lo que audio confirmó ejecutado; `next` es el candidato seleccionado,
`cue` lo que audio confirmó en audífonos (un rol a la vez en E5); son referencias
distintas. Elegir NEXT no lo acepta. Al programar ACCEPT, `pending` congela capa
y destino; al `applied`, ROOM toma esa capa y se limpia pending. Un solo pendiente
por rol. GENERATE no cambia room/next/cue/pending; publica nuevas options solo si
su contexto sigue vigente, conservando todas las capas referenciadas.

Cada `Layer` apunta a un manifiesto inmutable bajo `/data`, con `schema_version`,
`id`, `role`, `notes` (`pitch:int[0,127]`, `start_beat:f64>=0`,
`duration_beats:f64>0`, `velocity:int[1,127]`), `samples` (nota→ruta relativa,
hash, frecuencia, frames, ganancia, choke), semillas y versión del generador.
Editar crea otro ID/hash y conserva el borrador; el worker recibe copias, nunca
permiso de escritura sobre material de Juan. Persistir también los manifiestos
descartados que aparezcan en el registro (§9).

Grupos fijos: `RHYTHM=[KICK,DRUMS,PERC,TOP]`, `BASS=[BASS]`,
`MUSIC=[CHORDS,LEAD,PAD]`, `FX=[]` en E5. FX se reserva para retornos futuros,
no es un noveno rol. Ganancia ROOM = role.level × group.level × master,
con mute de rol/grupo; cero es silencio, sin −Inf en JSON. Todo cambio manual
pone `manual_level=true`; generación y ACCEPT preservan estos valores.

**Proyección Qt** ([09 §3](09-gui-fase1.md), [11, «Archivos, componentes y fuentes»](11-gui-fase2.md)):

| Interfaz existente | Fuente propuesta |
|---|---|
| `cells`, 32 filas `row*8+channel` | fila 0 ROOM; 1 NEXT; 2/3 opciones 0/1. `roleName/title/subtitle` desde capa y estado; `steps` = resumen de 16 pasos del compás visible; `level` del rol. `cellState`: playing/muted, queued/cue/idle, option/loading/empty según referencias; ausente = empty |
| `channels`, 8 | `level` del rol; `cue` confirmado, `selected` local. `meter/peak` desde sampler, no inferidos del fader. EQ/filter/echo/verb sin conectar, rotulados/deshabilitados |
| `options`, 4 tarjetas de LAYER | dos candidatos del rol seleccionado; huecos 2/3 deshabilitados. `letter/title/subtitle/energy`, `notes:{s,n,l}` resumidos desde manifiesto; `optionState`, hint/encoder derivados, nunca aceptación local |
| `softKeys`, 8 | `label/value/roleName/lit/turning/enabled/position/action` calculados por contexto de pantalla y §5. `enabled` requiere candidato ready y salud válida |
| `groups`, 4 | títulos/members fijos, level desde Mix, meter real; FX sin señal en E5 |
| Propiedades `bpm/bar/phrase/section`, títulos, `liveState` | Clock/Section y títulos. `phrase` = número absoluto de frase; `bar_in_phrase` y `bars_to_phrase` se exponen para sustituir los textos demo fijos. loading/empty/offline afectan propuestas, no borran ROOM |
| `connections`, toasts | salud real; ENGINE caído ≠ BRAIN caído. BRAIN = synth, ENGINE = motor (ADR-008 **Decision**, punto 7) |

`screen`, foco, rol seleccionado y navegación pertenecen a cada GUI, no al set.
`pressEncoder`, `triggerAction`, `turnEncoder` y touch se traducen en los mismos
comandos; no mutan ROOM de forma optimista. `SnapshotModel.queue_update/flush`
permanece como interfaz. Socket callbacks encolan, y `afterAnimating` aplica
snapshot/deltas una vez por cuadro: máximo un `dataChanged` por modelo y una
notificación agrupada de propiedades. `rev` creciente; hueco → nuevo snapshot.
No se cuentan frases con un QTimer.

Se conserva la unión por cuadro de [13, «Cola por cuadro»](13-gui-pi.md) y
[15, «Alcance y transporte»](15-gui-synth.md). El `SurfaceBatcher.apply` actual
es **mapeo visual de demo**: en E5 se sustituye por adaptación de intenciones y
estado confirmado, sin duplicar el consumidor físico en la GUI. Actualmente
no admite `SelectorPad`; no se le atribuye ese soporte. Los modelos `SynthState`
(parts/cells/presets/values) siguen alimentándose de 14, no de las opciones del
motor. Metadatos y parámetros del host tienen un único dueño allí; en E5 sus
ediciones no incorporan capas al set ni se conectan a ROOM.

## 4. Reloj, cuantización y latencia

Maestro: **contador de frames de audio del grafo conducido por Audio+**, observado
por el callback del sampler ([03 §1](03-arquitectura-capas.md), [04 §2](04-reproductor-e2.md)).
Nunca reloj de pared, MIDI Clock, Teensy, GUI ni worker. `sample_counter` avanza
por frames procesados; `origin_sample` fija el uno musical de PLAY. Un reinicio
o discontinuidad del grafo cambia `audio_epoch` e invalida destinos antiguos.

E5: 4/4, 122 BPM de demo, tempo fijado mientras corre. Frase = 8 compases;
bar/phrase empiezan en 1. Fronteras de compás k desde el origen:
`origin_sample + round(k * 4 * 60 * 48000 / bpm)`, k desde 0. Calcular desde
el origen con acumulador racional/fijo; no sumar longitudes redondeadas por compás.
Frases en k múltiplo de 8; el bucle del patrón puede durar 16 compases sin cambiar
la rejilla de aceptación. Al sustituir una capa empieza su compás 1.

ACCEPT toma la **primera frontera de frase estrictamente posterior** a la muestra
actual confirmada por audio. En la propia frontera apunta a la siguiente.
Debe llegar armada con margen mínimo de **2 quantums**; si ya no cabe, `too_late`,
sin cambio ni traslado silencioso a otra frase. La respuesta informa `target_bar`
y `target_sample`; un éxito de control significa scheduled, nunca applied.
Sampler divide el bloque en la muestra destino aunque caiga dentro del quantum.
Con transporte detenido, ACCEPT prepara la entrada en `origin_sample` del próximo
PLAY explícito; responde `result.status="prepared"`, pending preparing,
`target_bar=1`, `target_sample=null`. PLAY fija un origen futuro con margen de
2 quantums y arma juntas esas autorizaciones; entonces pasa a scheduled.
Mientras está detenido, audio sigue contando frames pero la posición musical
queda en bar1/phrase1/beat1. No arranca por aceptar. STOP cancela pendientes y detiene cue;
PLAY posterior reinicia en un uno, conservando ROOM aceptado.

Clock a núcleo ≤1 bloque de antigüedad; publicación a GUI 60 Hz como máximo,
meters 30 Hz y health/cpu 1 Hz. Presupuesto **propuesto, por medir**: evento ya
recibido en Pi → validación/estado de intención ≤8 ms p95; publicación/IPC
≤4 ms p95; espera al cuadro ≤16,7 ms. Medir también el total, no sumar percentiles:
evento recibido → `frameSwapped` **p95 <32,667 ms**, además USB físico → Pi <5 ms
de [01 §3, E4](01-prototipo-mesa.md). Registrar tiempo hasta scheduled por separado.
La espera musical de ACCEPT (hasta una frase) no es latencia de control.
Prueba de muestra destino y DAC aparte: `frameSwapped` no mide luz ni audio.

Link queda fuera de E5 ([01 §3, E6](01-prototipo-mesa.md)). Después podrá proponer
tempo/fase a un adaptador que convierta al dominio de muestras de Audio+; no
reemplaza su reloj físico. Autoridad Link/local, saltos, slew y pendientes durante
cambios requieren otro contrato; no se añade por parecer trivial (G09).

## 5. Comandos, eventos y superficie

Petición al núcleo: `{id:int64,client_id:Id,command_id:Id,set_id:Id,cmd:string,args:object}`.
Respuesta: `{id,ok:bool,rev:u64,result:object}` o `{id,ok:false,error:string}`.
Errores estables: `invalid`, `stale`, `not_ready`, `busy`, `too_late`, `unsupported`,
`device_down`, `storage_error`, `indeterminate`. Misma `command_id` y payload devuelve el resultado
previo sin repetir; mismo ID con payload distinto → invalid. Deduplicación durable
durante todo el set, recuperada del log; también en sampler por op_id/epoch.
Otra pulsación tiene otro ID: ACCEPT sobre el mismo pendiente devuelve su destino,
sin otro armado; sobre otra capa → busy. No usar «toggle» remoto.

| `cmd` y `args` | Efecto / validación |
|---|---|
| `CUE {role,layer_id,on:bool,options_epoch:u64}` | Selecciona NEXT y auditiona solo por CUE; on=false detiene ese cue. Exige rol habilitado y layer ready del contexto vigente. Nuevo cue reemplaza el previo; no exige haberlo aceptado |
| `ACCEPT {role,layer_id,options_epoch:u64}` | Exige capa ofrecida/seleccionada, ready y sin pending incompatible; audición previa no obligatoria. Registra intención durable, arma swap en §4, publica pending; ROOM cambia solo por applied |
| `SKIP {role,layer_id,options_epoch:u64}` | Descarta esa oferta, detiene su cue si lo tiene y solicita reemplazo. No borra material/log ni cancela un ACCEPT: pending implica busy |
| `GENERATE {role}` | Solicita otra tanda (§6); no sustituye material ni borra el cue/pendiente. Un job por rol; repetición mientras corre devuelve job vigente |
| `SET_LEVEL {target:"role"\|"group"\|"master"\|"cue",name:string,value:f64[0,1]}` | name = Role, Group, MASTER o CUE según target; envío absoluto, validación estricta. Solo se publica nivel confirmado por audio; marca manual_level para rol/grupo |
| `SET_MUTE {target:"role"\|"group",name:string,mute:bool}` | Silencia/reabre una capa **ya aceptada**, sin cargar otra; aplicada al próximo bloque con rampa breve |
| `PLAY {}` / `STOP {}` | Transporte explícito; arrancar exige audio ready y material aceptado; repetir STOP no tiene otro efecto |
| `DROP {}` | Acción reservada/deshabilitada en E5: unsupported. No significa borrar capa ni aceptar todo; significado musical pendiente en §11 |
| `TRANSITION_START {kind,roles,accept_ids,start_bar,end_bar}` | Contrato reservado para SWAP/BREAK/CUT: solo capas autorizadas por ACCEPT; E5 devuelve unsupported, sin mutar transition |
| `TRANSITION_ABORT {transition_id}` | Reservado; E5 unsupported. Futuro: detener pasos pendientes, conservar cambios ya ejecutados y respetar takeover manual; detalle musical en §11 |
| `VIEW_SHOWN {view_rev:u64,screen_name:string,role:Role,options_epoch:u64,layer_ids:list<Id>}` | Confirmación de cuadro mostrado para OPTIONS_SHOWN y contexto de clics físicos; no muta material musical |
| `GET_COMMAND {command_id:Id}` | Resultado y fase más reciente de esa decisión; permite resolver un timeout antes de repetir |

Además del tipo/rango: comprobar set/epoch, pertenencia al rol, hash/contexto,
capacidad de audio y versión de options. No aceptar rutas arbitrarias ni IDs de
otra sesión. Validar antes de efectos. Orden FIFO por cliente y serialización en
núcleo; intents discretos no se coalescen. Faders pueden coalescer valores absolutos;
encoders suman deltas en orden sobre el último destino pendiente, sin perder pasos.

Eventos: `{event:string,set_id,rev,audio_epoch,sample_counter,command_id:Id?,data:object}`.
`state` lleva snapshot o cambios con `base_rev`; `decision` lleva requested,
scheduled, applied, rejected o cancelled y destino real; `clock`, `meters`,
`health` son telemetría. Reconexión consulta resultado por command_id antes de
reintentar. UI deshabilita oferta no preparada; perder motor no inventa opciones.

Entrada USB MIDI **canal 1** según [18 §3](18-teensy-unica-firmware.md) y
`tools/consola/superficie.py`: Pad 48–79 (índice fila×8+columna), clics encoder
80–87; giros CC16–23 firmados; faders CC24–27 → RHYTHM/BASS/MUSIC/FX.
MASTER CC30; CUE/IN CC31/32 conservan el control local HP/IN de 18 §4, sin
duplicar atenuación digital. No confundir `@GF CUE 0|1` (ruta USB a códec) con
elegir una capa CUE. `@GF HP`, `IN`, `RUTA`, `CV`, `USB` y `@S STATS/RESET`
conservan su contrato; CDC se atiende fuera de callbacks y se identifica por
serial USB, sin asumir `/dev/ttyACM0`. `SelectorPad` CC36 es posición física,
sin segunda atenuación. FSR/manos/expresión se ignoran musicalmente en E5.

Propuesta de mapa de **LIVE · SESSION**: pad fila0 selecciona rol; fila1 selecciona
NEXT y CUE on/off; filas2/3 seleccionan y escuchan opción0/1. Solo presión (flanco
press tras release); mantener/repetir note-on no dispara decisiones. Releases
actualizan captura, no apagan una capa. Touch en celda hace la misma selección/cue,
sin tocar notas. Los pads **no aceptan ni saltan** en esta propuesta (contradicción §11).

Soft keys de LIVE · SESSION propuestas: clics **ENC1 CUE A, ENC2 CUE B,
ENC3 ACCEPT, ENC4 SKIP, ENC5 GENERATE, ENC6 DROP (disabled), ENC7 MUTE,
ENC8 STOP**; índice humano 1–8. Giro de cada encoder conserva nivel de su rol,
sin disparar su clic. LAYER existente conserva 1–4 cue y 5 Generate/6 Accept/
7 Skip/8 Back si se abre como detalle; huecos sin candidato deshabilitados.
Touch llama la misma acción con IDs explícitos. Comando transporta la selección
visible capturada al pulsar, no «el rol seleccionado» que pueda cambiar después.
Una única GUI activa registra `VIEW_SHOWN` después de `frameSwapped`; núcleo guarda
ese contexto de entrada fuera de SetState y resuelve allí clics/pads a IDs explícitos.
Cambio de rol por pad actualiza contexto y foco proyectado juntos; GUI lo confirma
al mostrar. Contexto obsoleto → stale, nunca aceptar otra opción. Sin GUI conectada,
ACCEPT contextual queda bloqueado; STOP/faders siguen activos y ROOM continúa.
Mapa sujeto a aprobación (§11), sin reasignar las notas Kailh históricas de 18 §3.
El mínimo de 18 §4 solo tiene cuatro clics cableados: CUE A/B + ACCEPT/SKIP
siguen físicos; GENERATE/STOP por touch hasta completar cableado.

## 6. Motor sin Live y anticipación

Backend propuesto **`ClienteSetMemoria`**, inyectado en `Motor(live=…,carpeta_live=…)`;
no hereda la conexión TCP. Imita métodos, retornos y `ErrorLive`, no una instalación
de Live. Referencias: `gf_motor/live.py`, `motor.py::proponer_patrones_drums`,
`poner_patron_drums`, `generar_drums`; `tests/live_simulado.py` sirve como guía de
formas, pero abre TCP y no es el backend de producción.

Interfaz mínima E5: `sesion()` → `{tempo,tracks:[{index,name}],file_path:""}`;
`indice_pista(nombre)` → int/null (duplicado → error);
`crear_pista_midi(nombre)` → int; `poner_tempo(bpm)` → None;
`escribir_clip(pista,inicio,longitud,notas,nombre,color=None)` → None;
`leer_clip(pista,inicio)` → `{start,length,notes,name,color}`/null;
`listar_clips(pista)` → lista sin notes; `clips_con_notas(pista)` → lista con notes;
`pista_sin_clips(pista)` → bool. Son beats de negra y los campos de nota de §3.
Pistas KICK/DRUMS son copias del contexto; BASS vacío. Escrituras solo a borradores
del job; solapar una copia de clip de Juan da ErrorLive o exige pista nueva.
No emular carga exitosa de plugins/racks de Ableton: métodos no soportados fallan
con ErrorLive; ese error no se convierte en layer ready.

El worker combina generadores puros ya existentes (patrón y WAV), no invoca la
cadena completa de armado de Live ni interpreta `.adg` como sampler. Selección
de sonidos y ensamblado de samples→notas ocurren en el job. Los métodos de Motor
que escriben/cargan racks se mantienen fuera del camino E5. Adaptar orquestación,
persistencia y rutas es trabajo futuro, no afirmar que basta cambiar el constructor:
Motor hoy carga «último estado» y escribe dataset/Library con defaults del Mac.
E5 debe aislarlos por set/job bajo `/data`, nunca reutilizar ese último estado global.

`generate {job_id,role,context_hash,manifest_refs,target_phrase,deadline_sample}`
en socket del worker devuelve ACK inmediato; después `generated` con manifiestos
o `failed`. Al empezar la frase n se ejecutan pedidos de Juan para n+1; GENERATE
o SKIP durante n puede usar ese mismo plazo si cabe, o informar target n+2.
Antes de PLAY se prepara la primera comparación sin reloj en marcha.
No se encadena generación de otros roles ni hay recarga infinita: una petición
autoriza una tanda; repetir ROOM no pide automáticamente otro arreglo (C8).

Precalcular: candidatos MIDI, semillas/variación, sonidos, conversión a 48 kHz,
mapa de notas/choke, energía/rasgos de presentación y tabla de eventos por muestra;
sampler carga todo a RAM. **Antes del inicio del último compás de n** debe existir
ACK ready para ofrecerlo durante n con entrada posible en n+1 (auditoría G20, §2.3 y §4.5). No sintetizar,
cargar disco ni inferir en el golpe. Una frase a 122 BPM dura ≈15,74 s; es margen
de planificación, no garantía derivada de los 0,34 s medidos de un rack.

Job tardío, fallo, muestra ausente o RAM insuficiente → no ofrecer candidato,
mostrar loading/error y mantener ROOM. No sustituir por silencio ni por una
opción anterior presentada como nueva. Resultado con contexto obsoleto se archiva,
no publica; cambio de contexto material requiere nuevo job. Candidatos preparados
que sigan siendo válidos pueden aceptarse aunque el worker esté caído.

## 7. Sampler/reproductor necesario

No usar el patrón fijo ni callback Python de E2 como motor E5 (G03/G18).
Una capa ROOM por rol, loop propio y voces separadas para CUE; dos buses estéreo:
`room_L/R` → Audio+; `cue_L/R` → reproducción USB Teensy **1/2** → SGTL5000.
Reproducción 3/4 reservada, captura de línea fuera de E5 ([18 §1](18-teensy-unica-firmware.md)).
PipeWire hace el cruce 48 kHz ↔ USB nominal 44,1 kHz con dispositivo asíncrono;
el USB no conduce el tiempo musical. No asumir 48 kHz en el códec ni cuatro
salidas analógicas. Verificar ruteo por nombres/identidad, sin autoconectar cue a sala.

`schedule` debe expresar «cambia en compás N» resuelto a muestra absoluta; quitar
disparos futuros del patrón viejo y empezar el nuevo en esa muestra, sin doble
golpe ni hueco. KICK no se incluye al sustituir DRUMS: permanece el mismo.
Choke hats independiente por bus; cola vieja de percusión puede decaer con rampa
de retirada ≤5 ms, sin disparos nuevos. KICK/BASS futuros deben retirar voz grave
vieja antes de disparar la nueva: nunca dos kicks ni dos bajos en ROOM (00 §5.1).
Cue opcionalmente suma ROOM a nivel `cue_room_mix` (inicial 0); no duplica kick
desde una opción DRUMS. CUE siempre aislado de ROOM, incluso al mutear grupos.

Samples/loops decodificados a float, 48 kHz y canales conocidos antes de ready;
referencias de manifiesto validadas dentro de /data, hashes verificados. Voces y
RAM acotadas: propuesta E5 64 voces/bus, 256 MiB total de samples residentes y
4096 eventos por capa de 16 compases; rechazar preparación que exceda, sin robar
voces graves para ocultar sobrecarga. Precargar capas activas/cue/opciones/pending,
sin streaming de disco. Mezcla con headroom de kit validado y contador de clipping;
no prometer un limitador nuevo. Medidores de ambos buses y contador de xruns reales.

## 8. Retorno de LEDs (G05)

**Extensión propuesta**, aún no soportada por 18: USB MIDI del Pi a «GF Consola»,
**canal 16** (mido 15), separado del canal 1 del Brain opcional. Note-on:
nota **48–79 = pad**, **80–107 y 110–122 = tecla física**; velocity = código de
paleta/estado, no fuerza. Note-off o velocity0 = apagado. No devolver el mensaje
como entrada de superficie. Sin SysEx ni bridge nuevo para E5.

| Velocity | Estado mostrado |
|---|---|
| 0 / 1 / 2 | apagado / color de rol tenue / color de rol fuerte |
| 3 / 4 / 5 | blanco CUE / violeta opción / ámbar no disponible |
| 6 / 7 / 8 | blanco pulsante pending / color de rol pulsante / rojo error |

Pad ROOM usa color de su columna (tenue si mute); NEXT blanco si cue, pulsante
si scheduled, tenue si ready; opciones violetas. Teclas sin rol usan teal como
color base; soft key ACCEPT física se ilumina solo si está habilitada. Animaciones
locales no bloqueantes, brillo limitado al 20–25 % de 03 §4.1. La Teensy reenvía
estado de pad a la M4 por su UART actual y actualiza teclas por su driver; no hay
animaciones síncronas que enmascaren audio (18 §2 y §5).

Núcleo envía diferencias ≤30 Hz y snapshot completo tras reconectar/cambiar mapa.
Propuesta heartbeat **CC119 en canal16**, valor contador módulo128, 2 Hz; tras
1,5 s sin heartbeat, indicador de desconexión ámbar local. Al recuperar, snapshot
reemplaza LEDs viejos. Sin ACK musical por LED: el estado del set sigue llegando
por socket. Tecla LED no cableada se reporta como capacidad ausente; no bloquea
audio, pero G05 exige demostrar al menos retorno a pads reales.

## 9. Registro del set para M1 (G26)

Un escritor (núcleo), **JSONL append-only por set**:
`/data/sets/<set_id>/events.jsonl`; assets/manifiestos junto al set o referenciados
por hash en `/data/samples`. Sin rutas `/Volumes/...` por defecto en Pi.
Cada línea: `schema_version:int`, `set_id:Id`, `seq:u64`, `event_id:Id`,
`type:string`, `utc:string?` ISO8601, `monotonic_ns:u64`, `clock:Clock`,
`rev:u64`, `command_id:Id?`, `source:string`, `context:object`, `payload:object`.
UTC puede ser null/no fiable offline; epoch+muestras y seq ordenan sin RTC.

Contexto completo en decisiones: género/tonalidad/BPM, sección/frase/compás,
ROOM por rol con hash, niveles/mutes, NEXT/CUE, **lista y orden de ofertas realmente
presentadas**, rasgos/energía/semillas, versión de motor/modelo (E5: rules),
opción elegida/descartada, origen pad/encoder/touch, tiempo de audición y destino.
Referencias resolubles a manifiestos inmutables con notas y samples; nunca solo
un nombre de preset. Presentada ≠ preparada: GUI confirma `OPTIONS_SHOWN`
con IDs/rev al cuadro mostrado. CUE mide ruteo activo, no prueba que Juan oyera.

Registrar `SET_START/END`, `OPTIONS_READY/SHOWN`, `CUE_START/STOP`,
`ACCEPT_REQUESTED/SCHEDULED/APPLIED/REJECTED/CANCELLED`, `SKIP`,
`GENERATE_REQUESTED/FAILED`, niveles/mutes (incluido takeover), transporte,
fallos/reconexiones/restarts y discrepancias de recuperación. Transiciones futuras:
tipo/plan/ACCEPTs/pasos/abort. Ediciones futuras: ID/hash del borrador y revisión
de Juan + diferencia de notas, sin borrar originales (motor v2 §6).
Resumen técnico cada 10 s: xruns, clipping, CPU, temperatura/throttling y STATS;
no guardar medidores de 30 Hz ni audio por defecto.

Antes de armar un ACCEPT: fsync de REQUESTED con contexto; antes de contestar
éxito: fsync de SCHEDULED. Audio conserva op_id y resultado si el núcleo cae
entre ambos. APPLIED se recupera de audio y se persiste; REQUESTED sin prueba
de armado no se reenvía automáticamente al reiniciar. Otros eventos se vacían
≤1 s; esa es la ventana declarada de pérdida ante corte, niveles ≤100 ms de §2.
Fallo/timeout después de enviar schedule → indeterminate hasta consultar op_id:
no contestar «rechazado» si todavía puede sonar. Si ya quedó armado y falla fsync,
conservar pending visible y avisar; intentar cancelación confirmada, sin suponerla.
Línea final truncada se ignora al leer; no reescribir el log. Error/disco lleno:
avisar y rechazar nuevas aceptaciones/generaciones registrables, sin cortar ROOM;
STOP y mute siguen disponibles. Anotar laguna al recuperar, nunca inventar datos.

El **Mac tira** registro y manifiestos por Ethernet/SSH (`rsync`), nunca el Pi
empuja ni sube a internet (auditoría §2.4 G26 y §7). Sets cerrados copiados con
hash/último seq verificados; de un set activo se importa solo prefijo de líneas
completas y se vuelve a recoger al cerrar. Deduplicar por set_id+seq/event_id.
No borrar en Pi por copiar; respaldo/retención los decide Juan. Este dataset
habilita M1; no autoriza entrenarlo ni tomar SKIP como rechazo de algo no mostrado.

## 10. Plan de pruebas y puerta E5

Estas son **pruebas a implementar después de aprobar el spec**, no resultados.

| Nivel | Casos / criterio observable |
|---|---|
| Unitario: estado | Solo ACCEPT incorpora capa; CUE/SKIP/GENERATE sin efecto en ROOM. Repetir 100 frases sin ACCEPT conserva capas/sección. IDs duplicados, stale y concurrentes no duplican; edición de Juan y faders sobreviven generación/aceptación |
| Unitario: tiempo | Muestra anterior/en/frontera posterior, evento dentro de bloque, margen 2 quantums, patrón16/frase8, STOP/PLAY y cambio de epoch. 10000 frases a 122 BPM sin deriva acumulada; cambio exactamente en muestra prevista |
| Unitario: fallos | No ready/tarde/RAM/colas llenas; cancelación que gana/pierde carrera con applied; crash entre REQUESTED, armado, SCHEDULED y APPLIED; recuperación sin doble entrada ni aceptar por inferencia |
| Integración Mac | Núcleo+worker+sampler offline con reloj inyectado, MIDI/CDC falsos, sockets locales y Qt real: A ROOM/B CUE distintos, ACCEPT → pending → applied; log reconstruye contexto y candidatos; 1000 ev/s y coalescencia sin perder decisiones. Matar GUI/motor/núcleo/audio por separado y verificar §2 |
| Transporte Mac | JSON fragmentado/UTF-8, reconexión, cliente lento, línea/tipos/IDs inválidos, múltiples clientes, permissions y rechazo de doble dueño. Si bind AF_UNIX está bloqueado, socketpairs heredados prueban parser; listener queda explícitamente sin verificar (como 15/16) |

**Demo física Pi, necesaria para aprobar E5:**

1. Arranque offline, Audio+ a 48 kHz; primera pantalla con datos reales, sin
   contadores/textos de muestra. Cargar/aceptar kick y A, escuchar B en Teensy
   USB1/2. Grabación de ambos buses verifica que B no aparece en sala antes de ACCEPT.
2. **20 ACCEPT** repartidos entre compases1–8, más casos junto a frontera:
   todos los scheduled entran en la muestra objetivo del sampler; error digital
   ≤1 muestra. `too_late` visible y sin ejecución diferida. Loopback sala confirma
   bar correcto y ausencia de doble kick; documentar latencia DAC medida aparte.
3. **15 min con Juan**, ≥10 SKIP/GENERATE, ≥5 frases completas sin decidir:
   ROOM/sección no avanzan solos. LEDs de pads distinguen ROOM/CUE/pending y
   restauran snapshot al reconectar. Juan confirma que quiere seguir (01 §3 E5).
4. **60 min** de sampler + motor por frases + GUI + cue USB, quantum256
   (128 informativo): **0 xruns**, 0 clips/drops de cola, 0 clics de cue,
   sin throttling. Callback sampler: media≤50 %, p99≤70 %, máx≤90 % de un núcleo;
   registrar también RAM máxima y parámetros reales. Son puertas propuestas E5;
   no extrapolar la prueba ALSA E2 ni el USB de otro firmware.
5. Compuerta ADR-007 **Decision**: LIVE/MIXER animando, **1000 eventos/s**,
   60 fps sostenidos, p99 QSG<16,7 ms, GUI<25 % de un núcleo y latencia
   evento recibido→frameSwapped p95<32,667 ms. Medir también budgets §4;
   rotación/touch físicos y fuentes reales como [13, «Harness»](13-gui-pi.md).
   Gate insuficiente/59,94 Hz se reporta, no se redondea a PASS.
6. Matar GUI/motor: ROOM continúa sin xruns; matar núcleo: solo pending ya aceptado
   puede entrar y reconecta con estado correcto; matar sampler: silencio declarado,
   watchdog inicia proceso ≤5 s, carga estado y queda detenido. Desconectar superficie:
   sigue audio, no hay press fantasma al volver. Corte de alimentación conserva
   decisiones fsync y recupera sin reproducir pending antiguo.
7. Cada decisión con contexto resoluble y fases scheduled/applied diferenciadas;
   Mac recoge set completo y verifica hashes/seq sin red externa. Confirmar puerta
   del firmware combinado de [18 §5](18-teensy-unica-firmware.md), no sustituirla
   por los 0 drops del antiguo `consola_usb` (ADR-008 **Context**).

## 11. Decisiones de Juan y contradicciones

**Decisiones creativas/operativas pendientes** (no bloquean escribir este spec;
sí su aprobación o la ampliación correspondiente):

- Aprobar mapa de LIVE · SESSION de §5 y color/feedback de §8. ¿Se conserva
  ACCEPT/SKIP adicional en pads de 01, además de las soft keys ya decididas en 07?
- ¿DROP abre el selector de transición, pide llegar al drop o ejecuta un plan ya
  aceptado? ¿Qué autoriza un ACCEPT durante SWAP/BREAK/CUT, qué se conserva al
  abortar y qué pasos se detienen al tocar un fader/pad? E5 los deja deshabilitados.
- Tras probar drums: asignación musical separada de DRUMS/PERC/TOP, longitud
  posterior 8/16 y cómo se elige sección. No inferir arrangement de la frase.
- Confirmar comportamiento de recuperación detenido con PLAY explícito y política
  de retención/respaldo del registro. Aprobar la frontera local Pi↔Mac sin terceros.
- Después de la demo: si el flujo/energía de propuestas y el cue sirven al criterio
  de Juan; las métricas de rendimiento no sustituyen esa decisión.

**Diferencias encontradas; no se modifican ni se declaran resueltas aquí:**

| Fuente | Diferencia y límite de esta propuesta |
|---|---|
| 01 §3 E5; 00 §10.2/§10.2.2; 07 §5; 09 §5; auditoría §3 C3 | Pads para ACCEPT/SKIP, botones dedicados y clics contextuales compiten. §5 propone mapa E5 con soft keys; no enmienda las fuentes |
| 03 §1/§4 y 00 §3 frente a ADR-008 **Decision**, puntos1–3; 17 §4; 18 §1/§3 | Dos Teensy, UART A↔S y «nada RT depende de Linux» frente a una Teensy y synths/sampler en Pi. ADR-008 declara el reemplazo; la redacción vieja sigue pendiente de actualización |
| 03 §2 frente a 08 §1/§3 y 18 §3/§4 | Bridge binario por CDC frente a MIDI USB + @S/@GF textual; también GF Superficie frente a GF Consola. §5 propone reutilizar 18 para E5, no migra bridge |
| 01 §3 E2; 04 §2/§4; ADR-007 **Context** frente a ADR-008 **Decision**, punto1 y auditoría §2.3 G18 | Reproductor Python/ALSA de prueba frente a audio C++/PipeWire. Mediciones E2 no certifican sampler E5; traslado propuesto en §2/§7 requiere implementación posterior |
| 00 §5/§5.1 frente a motor v2 §3 reglas1/4; auditoría §3 C8 y §8 P7 | «Siempre siguiente», track derivado y transición ejecutada sola frente a no avanzar ni generar track completo. §1/§6 restringen E5 a pedidos por rol y ACCEPT; autorización de planes multirrol sigue abierta |
| Motor v2 §3 regla5 frente a auditoría §3 C9, §2.4 G26 y ADR-008 **Decision**, punto5 | «Nada fuera del Mac» literal frente a motor/registro en Pi. La tarea pide local Pi/Mac y recogida por Mac; se propone §9 sin ampliar a terceros, pendiente armonizar texto fuente |
| 09 §3/§5, 11 «Qué funciona y qué es fake», código `fake_data.py`/`surface_batch.py` frente a G02/G04 | GUI actual usa frases/títulos/textos fijos, cuatro opciones y mapeos visuales locales; `phrase` no define inequívocamente el anillo de 8 posiciones. §3 propone número absoluto + campos explícitos y dos ofertas; no afirmar cableado existente |
| 14 §3/§4 y 16 «Recuperación y cobertura del contrato» | subscribe no enumera param aunque hay eventos param; host no tiene transporte/tempo ni cue/cambio cuantizado. §2 conserva host independiente y §1 excluye synth del set: no inventar comandos de 14 ni ofrecer integración musical completa |
| 00 §5.1 (~32 compases SWAP) frente a muestras GUI `SWAP · 8 PH` y 09 §5 | Duración/etapas de transición y datos visuales no forman un contrato ejecutable común. Reservado; ningún motor E5 corre ese plan |
| 08 §1/§2/§4 frente a lector actual y 18 §3/§4 | 08 describe S, puerto antiguo, sin SelectorPad/reconexión/LED; código añade GF Consola, botones extra y selector, pero SurfaceBatcher no lo admite. Adaptación propuesta explícita en §3/§5/§8; no atribuir soporte bidireccional al firmware actual |
| ADR-007 **Decision** frente a 13 «Harness y lectura del resultado» | Gate no define tolerancia para59,94 Hz ni simultáneas/alternadas; harness usa dos texturas. Conservar números literales y reportar interpretación/limitación, sin aprobar por equivalencia supuesta |

Los puntos técnicos nuevos se aprueban con este documento; no autorizan código
hasta el visto bueno de Juan. Ningún spec SSoT00–06 ni contrato14 se cambia aquí.

## 12. Decisiones de Juan (2026-11-10)

| # | Decisión | Efecto sobre este spec |
|---|---|---|
| D1 | **El sampler y el secuenciador van dentro de `gf-synth-host`**, no en un `gf-sampler` aparte | Un solo proceso de audio en tiempo real y un solo reloj para synths, sampler y transporte. §2 y §7 se leen sobre el host: los comandos `prepare_layer`, `schedule`, `cancel`, `set_cue`, `set_transport` se agregan a su contrato (`14`), versionado. Motivo: la prueba del 2026-11-10 mostró cortes con varios procesos compitiendo por la CPU |
| D2 | **El modo DRUM va antes que E5** | DRUM (racks de samples + secuenciador por pasos para jam, spec `22-modo-drum.md`) construye el sampler, el reloj y el transporte que E5 reutiliza. E5 se implementa después, sobre esa base |
| D3 | **ACCEPT y SKIP solo como teclas de pantalla** (clic de encoder o touch), no en pads | Cierra la contradicción C3: `01-prototipo-mesa.md` y `00-concepto.md` quedan desactualizados en ese punto |
| D4 | **Tras un reinicio la consola vuelve detenida y espera PLAY** | Confirma §2: carga lo último aceptado y no suena nada hasta PLAY |

**Con la recomendación de Claude, pendientes de que Juan las confirme:** DROP deshabilitado en E5 (después abrirá
el selector de transición); alcance E5 = KICK + DRUMS, frase de 8 compases, máximo 2 opciones por rol; registro del set
local en el Pi y recogido por el Mac, sin terceros; mapa de LEDs de §8.
