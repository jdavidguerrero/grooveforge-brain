# Consola: GUI del synth (31–45)

2026-11-10 · ADR-007/008 · contrato 14 sin modificaciones. Documento previo al código.

## Alcance y decisiones

PARTS 31/31b; plantilla FILTER 32/32b/32c; OSC 33; ENV 34/34b;
LFO 35; FM 36/36b (4 y 6 operadores); GROOVE 37; FX 38; MACROS 39;
HYBRID 40/41 (mitades A/B); PRESETS 42/42b; SETTINGS 43/44/45.
Se conservan IDs legacy del registro; nombres Synth* identifican estas pantallas.
Chrome 56/568/96, columnas x=8+i·160, ancho152. Textos ingleses.
BRAIN significa synth; ENGINE, motor generativo. Tema y fuentes locales existentes.

`synth_client.py` usa QLocalSocket, JSON UTF-8 por líneas, IDs correlacionados,
timeout de 500 ms y reconexión exponencial limitada. Los callbacks encolan mensajes;
el mismo `afterAnimating` que drena SurfaceBatcher aplica el lote synth una vez
por cuadro. No hay asyncio, audio, sondeo periódico de parámetros ni trabajo DSP.
Al conectar: hello, subscribe, get_state; después list_params y get_params por parte.
El socket se selecciona por CLI; sin socket la revisión usa snapshots explícitos
obtenidos del mismo fake. Para desarrollo integrado, el fake corre como proceso aparte.

`list_params(engine)` es la única fuente de páginas, bancos y metadatos. La GUI
no completa parámetros ausentes. Banco de hasta ocho, huecos deshabilitados.
Giro relativo → valor absoluto: valor actual + delta·step; SHIFT multiplica por
0,1 solo continuos/bipolares; enum/bool usan pasos enteros. Recorte a min/max.
Clic dispara soft key, independiente del giro. Página/banco envuelven; FM permite
focus de cada operador; seis operadores ocupan dos bancos de cuatro pares ratio/level.
Los gráficos leen valores; los IDs semánticos del fake son provisionales (véase abajo).

Sin respuesta a una petición en 500 ms: NOT RESPONDING, últimos valores al 45 %,
edición bloqueada, Retry. LOADING PRESET dura desde load_preset hasta su respuesta;
no promete cambio al próximo compás ni ofrece cancelación que el host no soporta.
Error del host se muestra y conserva el preset anterior. Reconectar resincroniza.

## Fake y límites

`fake_synth_host.py`: proceso Unix multicliente con los comandos/eventos de 14,
cuatro partes, ocho engines, metadatos realistas, bancos, presets en memoria,
medidores 30 Hz y CPU 1 Hz. No sintetiza audio ni importa SysEx ni persiste presets.
Settings muestra estado del host cuando existe y muestras claramente rotuladas para
USB/red/disco/temperatura; acciones de sistema/audio sin contrato quedan deshabilitadas.
La parte hardware se muestra desconectada: no se inventa un transporte Teensy.

## Diferencias y contradicciones abiertas

- El diseño usa dos USB (Superficie + Brain); ADR-008 dice una Teensy con ambos
  trabajos. No se reconstruye esa topología ni se presenta como detectada.
- Settings diseña NVMe 256 GB y tasas 44,1/96 kHz; ADR-008 fija 128 GB y 48 kHz.
  Son muestras de diseño, no configuración descubierta; controles no operativos.
- Diseño promete CUE, cambio al próximo compás, cancelación, favoritos, tags al
  guardar, swap/link de mitades y guardar layer: contrato 14 no los permite.
- Diseño FM 4-OP ampliado por petición explícita a DX7 6-OP, 32 algoritmos y R1–R4/
  L1–L4. Tabla de rutas independiente de metadatos de controles. ADR-008 habla de
  feedback en OP6; DX7 real tiene ubicaciones distintas y bucles multióperador
  (algoritmos 4/6). Se registra, sin redefinir el DSP. Diagrama muestra retorno real.
- list_params no fija IDs, binding gráfico, mitades ni operadores. Propuesta exacta:
  añadir a cada param `semantic` opcional (`filter.cutoff`, `op.2.rate.1`, etc.),
  `scope` opcional (`A`/`B`), y a cada página `graph` opcional (tipo y mapa semantic→id).
  Hasta acordarlo, gráfico especializado usa IDs del fake; controles siempre genéricos.
- Respuestas set_param/load_preset/set_engine no fijan claves de payload y `param`
  no tiene topic de suscripción. Propuesta: set_param devuelve `{part,param,value}`;
  comandos de parte devuelven `{part:{...estado}}`; param acompaña topic `state`.
  El cliente tolera estado de parte directo o anidado y vuelve a pedir valores.
- Propuestas de comandos adicionales (no enviados): `import_sysex {part,path}`;
  `get_settings`; `set_audio {sample_rate,block}`; `set_output {part,output}`;
  `cancel_load {request_id}`; `set_favorite {name,favorite}`;
  `save_preset` con `tags`; `set_hybrid {part,half,engine}`.
  Audición requiere instancia/puerto CUE separado para conservar ROOM.
- Barras/curvas son visualizaciones de control, no medición de respuesta DSP.
  Se simplifican sombras y routing; no blur vivo. Falta certificar Pi/hardware.

## Validación prevista

Round-trip de todos los comandos y errores; framing fragmentado/múltiple; eventos,
timeout/reconexión; bancos de todos los engines; integridad de 32 algoritmos;
QML sin warnings, geometría y encoder fino; capturas 1280×720 con fuentes reales.
Suite completa `python3 -m pytest apps/consola/gui-console/tests -q`.
La fuente para verificar rutas DX7 es la tabla pública MSFA de Google:
https://github.com/google/music-synthesizer-for-android/blob/master/app/src/main/jni/fm_core.cc
El HTML empaquetado se extrae leyendo JSON `__bundler/template`; sus notas QML y
script `renderVals` aportan geometría y muestras, sin depender del loader externo.

## Resultado y uso

25 vistas nuevas (`Synth*`, IDs36–60), ninguna pantalla omitida. Galería y
comparación por vista: `gui-console/screenshots/synth-index.html` y
`SYNTH-COMPARISON.md`; manifiesto con número del diseño, archivo y tamaño.
OSC33 edita ARP software: control de la parte física no está en 14. MACROS usa
los cuatro controles contiguos que entrega list_params; el contrato no tiene un
campo para reservar el encoder par. ENV y respuesta FX simplifican trazos/handles.
HYBRID conserva descripciones de mitades como muestras de diseño: 14 no las reporta.

`main.py --fake-synth --screen SynthParts` lanza el fake aparte y lo termina al
cerrar. `--synth-socket /ruta/gf-synth.sock` conecta al host real; `--preview
--screen SynthDx7Focus` permite revisión determinista sin socket. F3=Parts,
F4=Audio; PageUp/Down=página; clic3=banco; SHIFT=fino. Girar no dispara la soft key.
Al girar, se acumula el absoluto final por parámetro y se envía una vez por cuadro;
Guardar vacía primero esas ediciones. Medidores no resetean celdas ni presets.
El fake tarda120ms al cargar presets y acepta `--delay-ms` para probar fallos.

Archivos: cliente/estado/catálogo (`synth_client.py`, `synth_state.py`,
`synth_catalog.py`); fake (`fake_synth_host.py`); tabla FM (`fm_algorithms.py`);
cinco pantallas `screens/Synth*.qml`; cinco widgets nuevos y ParameterPage existente;
registro/main/cola/chrome; setup Pi agrega QtNetwork; tests `test_synth.py`,
herramienta de capturas, README y PNG/manifiesto/galería/comparación.
No se cambió 14, no se tocaron los clones USB/host ni se agregó GeneralSans.ttf.

Precisión de propuesta para gráficos (extensión opcional, **sin cambiar 14**):

```json
{"name":"FILTER","graph":{"type":"filter","bindings":{"cutoff":"host_cutoff_id","resonance":"host_res_id"}},"banks":[[]]}
```

Para FM: `graph.type="fm"`, `operators=6`, `algorithm="host_algo_id"`,
`feedback="host_fb_id"`, y `operator_pages=["OP 1",...,"OP 6"]`.
Cada param puede declarar `semantic` y `scope`; estos campos requieren acuerdo
con el host. Hoy se reconocen IDs del fake; si faltan, se indica visualización
no disponible y se conservan los controles genéricos. No se presentan valores
predeterminados como si fueran una medición del host.

Bloqueo del entorno: AF_UNIX bind falla con `PermissionError: Operation not
permitted`. Los tests Unix quedan explícitamente skipped; `--stdio` es un harness
de protocolo con proceso Python separado y transporte inyectado solo en tests.
Valida parser, todos los comandos/eventos, timeout500ms y reconexión por pipes;
no demuestra el transporte Unix ni broadcast multicliente en esta máquina.
La GUI de producción sigue usando QLocalSocket. Pi5/performance/audio pendientes.

El commit documental se intentó antes del código y falló con
`.git/index.lock: Operation not permitted`; cambios sin commit, sin push.
Mensajes previstos (con párrafo final obligatorio):
`docs(console): specify synth GUI and host integration` y
`feat(console): add metadata-driven synth screens and host client`, ambos con
`Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

Verificación final: `python3 -m pytest apps/consola/gui-console/tests -q` →
**201 passed, 12 skipped in 30.08s** (incluye regresiones, carga QML sin warnings,
geometría, fuentes locales y las pruebas nuevas). Los12 skips corresponden
exclusivamente a AF_UNIX: comandos, ocho engines, timeout/reconexión, framing y
broadcast. Esas mismas rutas, salvo broadcast multicliente, se probaron por pipes
con proceso aparte. Se verificó también que un bind fallido del fake nunca borre
el socket/archivo de otro servicio. Capturas finales:25 PNG1280×720, cero warnings;
System corregido sin solapamientos. `git diff --check`, compilación Python y sintaxis
Bash correctas. Rama `codex/gui-synth`, HEAD006684d; commit vuelto a intentar y
bloqueado por el sandbox. Solo12 archivos existentes modificados y46 nuevos dentro
del alcance autorizado. General Sans ignorada y sin seguimiento de Git.
