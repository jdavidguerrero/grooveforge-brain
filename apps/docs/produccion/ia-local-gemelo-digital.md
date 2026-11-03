# El gemelo como gemelo digital: qué le aporta a la IA local

Fecha: 2026-11-03. Estado: exploración. **Prioridad de Juan: primero la IA local, con el VST como fuente para correr
instancias del Brain y entrenar modelos. El gemelo completo (§9) queda en el roadmap para después.** No cambia los specs
00–06, no modifica ADR-006 y no autoriza implementar nada todavía.

Depende de: `ia-local-refinamiento.md` (etapas, §8), `motor-asistencia.md` (contrato y métricas),
`auditoria-datasets-adn-2026-10-28.md` (§9–§10), `grabadora-automatica.md`, ADR-006,
`specs/gemelo-plugin-v1.md` y `sprints/gemelo-auditoria-motores-2026-11-01.md`.

## 1. Dictamen

1. **El gemelo abarata el audio, y eso cambia qué experimentos se pueden hacer.** Grabar un clip del Brain toma unos
   20 s. Renderizarlo en el plugin tomaría menos de 1 s (§3). Los experimentos que necesitan muchas pruebas sonoras
   dejan de depender de sesiones de grabación.
2. **El cuello de botella pasa a ser el oído de Juan.** Renderizar los 122 presets con 6 clips tomaría unos 8 minutos;
   escucharlos, 3,5 horas. Por eso lo que más vale es lo que usa renders para descartar y deja que Juan escuche poco:
   sigue el límite de tres opciones por decisión de `motor-asistencia.md`.
3. **Hoy es un modelo digital, sin enlace con el hardware.** No hay medición contra el Brain físico, y el plugin no
   comparte todo su DSP con el firmware (§3). Esto no bloquea todos los usos: cada uso exige un nivel de fidelidad
   distinto (§6).
4. **Hay dos líneas que se apoyan entre sí.** La de IA local empieza por la audición renderizada y por reimprimir tomas
   tocadas en el Brain (§8), y mantiene el ML personalizado al final. La de gemelo completo usa arquitectura IoT para
   conectar los dos cuerpos en ambos sentidos (§9). El firmware ya reporta parte de su estado, y el primer espejo se
   puede demostrar sin flashear.

## 2. Qué significa «gemelo digital»

Grieves y Vickers describen el gemelo como una representación virtual de un sistema físico que se usa durante todo su
ciclo de vida para predecir y entender su comportamiento. Kritzinger et al. lo clasifican por cómo fluyen los datos
entre lo físico y lo digital. Esa clasificación viene de manufactura; aplicada al Brain queda así:

| Nivel | Flujo de datos | En el Brain | Estado |
|---|---|---|---|
| Modelo digital | Manual en ambos sentidos | Importar un GFP a mano y comparar de oído | **Aquí estamos** |
| Sombra digital | Físico → digital, automático | El Brain publica preset, MIDI y CC; el plugin los replica o los reimprime | No existe |
| Gemelo digital | Automático en ambos sentidos | Además, el plugin controla al Brain | Objetivo (§9); hoy fuera de v1 por spec §2 |

**La IA local puede funcionar sin el tercer nivel**, con dos lazos por lotes. El tercer nivel es un objetivo propio de
innovación, y la ruta para alcanzarlo está en §9:

```text
LAZO DE PRODUCCIÓN (cada decisión)
Juan toca en el Brain ─► MIDI + snapshot ─► el gemelo renderiza opciones ─► Juan escucha ≤3
        ▲                                                                       │
        └────────── preset elegido sube al Brain ◄── registro de la decisión ◄──┘

LAZO DE CALIBRACIÓN (cada cambio de firmware o de DSP)
Mismo preset + mismo MIDI ─► Brain + grabadora ──┐
                          └► gemelo (render) ────┴─► medir diferencia ─► corregir o etiquetar
```

## 3. Estado real

| Capacidad | Estado | Evidencia |
|---|---|---|
| Velocidad | 0,8–3,6 % de un núcleo: 10 s de audio en 0,08–0,36 s, según el modo | Arnés `gf_review` (fuera del repo), Mac M1 Pro, 48 kHz, 6 notas |
| Operar sin interfaz | Posible: el arnés instancia el plugin y le pasa presets y MIDI | Mismo arnés |
| Espacio de parámetros | 105–116 parámetros movidos por modo; incluye discretos | Mismo arnés. Es un espacio mixto, no continuo |
| Repetibilidad | Indicios: audio idéntico tras guardar y restaurar, y semillas fijas en el ruido revisado | **Falta probar el hash entre corridas** |
| Formatos | Importa GFP/GFL. Exporta todo menos el Prophet | `plugin_processor.cpp`; la exportación de Prophet sigue pendiente |
| `gf-render` y manifiesto | No existen | Spec §9; no hay `apps/gf-render` ni `libs/` |
| Grabadora y `@GF PLAY` | Documentados, no implementados | `grabadora-automatica.md` §1 |
| Paridad con el firmware | Parcial: comparte núcleos, pero las rutas de voz son propias de cada `*_preview.cpp` | Auditoría 2026-11-01 |
| Fidelidad contra el Brain físico | **Sin medir** | No se hizo A/B con hardware |
| Audio propio en el dataset | Sin corpus: los 102 archivos son copias de tomas dentro de `snapshots/` | Revisado hoy |

**Cuentas de §1.** Un clip de 8 compases a 124 BPM con cola dura unos 17 s. La grabadora prevé 180 tomas por hora, unos
20 s por toma. En el peor modo medido (HYBRID, 3,6 %), un clip tomaría unos 0,6 s; 122 presets × 6 clips = 732 clips:
**unos 8 minutos en un hilo, contra unas 4 horas de grabación**. Es un orden de magnitud: no incluye cargar el preset,
escribir el WAV ni los FX más costosos que pueda tener cada preset. El Mac M6 previsto no se ha medido.

## 4. Qué aporta el gemelo que una grabación no da

| Propiedad | Qué permite |
|---|---|
| **Etiquetas exactas** | Cada audio viene con su patch, MIDI, semilla y versión. Es el par audio–patch que `ia-local-refinamiento.md` §5 echaba en falta |
| **Contrafactuales** | Cambiar solo una variable (un parámetro, una nota, el contexto) con todo lo demás idéntico |
| **Más rápido que tiempo real** | Probar miles de candidatos y mostrar solo unos pocos |
| **Entorno seguro** | Una propuesta se prueba sin tocar el flash del Brain ni el set original |
| **Trazabilidad** | Con hash repetible, «lo que sonó» se puede demostrar y reconstruir |

El gemelo **no es diferenciable**: es C++ con oscilaciones discontinuas y parámetros discretos. Las técnicas DDSP que
optimizan por gradiente no se le aplican directamente. Hayes et al. señalan además problemas de optimización en esa
familia de métodos. Las opciones realistas son búsqueda sin gradiente o una red que aproxime al sintetizador.

## 5. Qué se puede aplicar

La columna «Fidelidad» remite a los niveles de §6.

| # | Aplicación | Qué resuelve | Baseline que debe superar | Fidelidad | Veredicto |
|---|---|---|---|---|---|
| A | **Audición renderizada** | Oír una opción en contexto sin grabar parte por parte, como obligó el estudio de acordes | Flujo actual de grabar | 0 | **Empezar aquí** |
| B | **Reimprimir la toma** | Juan toca en el Brain; con MIDI y snapshot, el gemelo reimprime esa interpretación con otro preset | Volver a grabar | 0–1 | **Empezar aquí** |
| C | **Fichas medidas** | Calcular registro útil, ataque, brillo y choque con el kick. `key_range` está vacío en los 118 presets auditados | Etiquetar a mano | 1 | Siguiente |
| D | **Vigilante de regresión** | Renderizar un corpus fijo en cada commit y avisar si un preset cambió | Tests actuales, que no miden timbre | 0 | Siguiente; no es ML |
| E | **Catálogo con embeddings** | Buscar por descripción o por parecido con renders de los presets propios | Rol, etiquetas y carpetas | 1 | Etapa 2 de `ia-local` |
| F | **Variaciones acotadas** | Muestrear cerca de un preset elegido, descartar por medidas y presentar tres | Editar a mano | 1 | Etapa 3 |
| G | **Probar macros XY antes de exponerlas** | Medir si una interpolación cambia de forma suave o da saltos | Macros actuales | 1 | Si aparece la necesidad |
| H | **Agente con herramienta de render** | Un modelo local propone un patch, lo renderiza, mide y corrige antes de mostrarlo | D + F sin agente | 1 | Después de F |
| I | **Preferencias por duelos** | Elegir la siguiente comparación A/B con pocas escuchas, sobre 2–4 macros | Favoritos del mismo rol primero | 1 | Experimento |
| J | **Audio → patch** | Dada una referencia, encontrar un punto de partida | Recuperar el preset más cercano del catálogo | 2 | Investigación |
| K | **Datos para `04`** | Audio con tonalidad y acordes exactos, tomados del MIDI | Datos del plan `04` | 2 | Requiere decisión de Juan |
| L | **Corrección de la salida física** | Modelar el codec y la salida con un EQ o, si no basta, un modelo pequeño | Sin corrección | 2 | Solo si se mide diferencia audible |

### Notas sobre las que importan

**B es la que más encaja con tu criterio.** Una revisión de ADR-006 del 2026-10-29 dice «Tocar es del Brain». Luego
retomaste el plugin, pero esa idea se puede conservar aquí: la interpretación sale del hardware y el gemelo aporta
impresión y alternativas. También cubre la captura de MIDI/CC y presets
que pide `ia-local-refinamiento.md` §7. Falta definir cómo registrar el snapshot junto al MIDI.

**F necesita límites musicales.** Un índice de algoritmo o un toggle no se interpola como un control continuo
(`ia-local` §5). Cada variación debe poder explicarse como un cambio pequeño. Las medidas solo descartan lo que está
roto o choca con el contexto; no puntúan si algo «suena a Juan».

**H es una versión verificable de lo que ya pide `ia-local` §3.** Convertir «más cálido» en dos alternativas es mejor
si el agente comprueba qué cambió en el audio antes de afirmarlo. Sigue el contrato de `ia-local` §4: propuesta,
validación, vista previa, aplicar sobre copia y deshacer.

**J se hace por etapas.** Primero recuperar del catálogo; después ajustar con una búsqueda sin gradiente, como CMA-ES,
comparando audio con una pérdida multirresolución; la red neuronal va al final (Yee-King et al.; Barkan et al.). Timbres
parecidos admiten varios patches, así que se evalúa el audio obtenido, no los parámetros. Una sola nota no captura
modulaciones ni el comportamiento en otros registros.

**L puede resultar innecesaria.** ADR-006 estima que la salida colorea poco, pero nadie lo ha medido. Wright et al.
muestran que una red puede emular distorsión analógica fuerte en tiempo real, un problema mucho más difícil que este;
por eso se prueba primero lo más simple.

### Lo que el gemelo no resuelve

- **Preferencias e identidad:** los renders no contienen el gusto de Juan. Ese dato solo sale de sus escuchas y decisiones.
- **Interpretación y microtiming humanos:** salen de tocar.
- **Costo en la Teensy:** un porcentaje de CPU en el Mac no predice el presupuesto de ≤60 %. Se mide on-device.
- **Add-on analógico y panel físico:** fuera del modelo por diseño.
- **E-Drum y saturación neuronal en la Teensy:** dependen de sensores o capturas reales y de benchmarks on-device.

## 6. Qué fidelidad exige cada uso

La auditoría de motores deja el A/B físico para el final, por instrucción de Juan. Estos niveles permiten avanzar sin
cambiar ese orden:

| Nivel | Condición | Qué habilita | Regla |
|---|---|---|---|
| 0 | Render repetible con procedencia | A, B, D | Es un boceto: la impresión final puede seguir saliendo del Brain |
| 1 | Paridad digital por engine (firmware contra plugin, en nativo) | C, E, F, G, H, I | Vale para los engines que hayan pasado la comparación; se etiqueta |
| 2 | Medición contra el Brain físico (ADR-006 fase 4) | J, K, L | Las pruebas finales siempre usan tomas reales, no solo renders |

Una medida útil y barata: **¿la decisión se sostiene?** Cuando Juan elige entre opciones renderizadas, las vuelve a
escuchar desde el Brain con el mismo MIDI. Si la elección se mantiene, el gemelo basta para ese tipo de decisión. Esto
no sustituye a la fase 4 de ADR-006, y con pocos casos solo sirve como señal.

## 7. Riesgos propios

| Riesgo | Cómo se manifiesta | Mitigación |
|---|---|---|
| **Divergencia** | Un preset elegido en el gemelo suena distinto en el Brain | Etiquetar `source: render`, comparar por engine y usar D |
| **Patches inviables** | Una búsqueda encuentra algo que no cabe en GFP o supera la CPU de la Teensy | Exportar y volver a importar como filtro obligatorio; medir CPU on-device antes de adoptarlo |
| **Abundancia falsa** | Miles de renders de una misma receta parecen un dataset grande; ya pasó con 1.713 MIDI, de los que 1.265 eran distintos | Separar entrenamiento y prueba por preset, sesión o proyecto, nunca por clip |
| **Aprender el simulador** | Un modelo funciona con renders limpios y falla con tomas reales | Probar con tomas reales reservadas; añadir ruido y variaciones de nivel y EQ (Tobin et al.) |
| **Estrechar el gusto** | Optimizar hacia favoritos deja de descubrir | Reservar opciones de exploración y evaluarlas aparte (`motor-asistencia.md` §11) |
| **Fatiga de escucha** | Más opciones generan otro bloqueo | Mantener ≤3 opciones, incluida conservar lo existente |
| **Permisos** | Ajustar patches contra un track ajeno o entrenar con él | Las referencias sirven para análisis y se mantienen separadas del entrenamiento (`ia-local` §6) |

## 8. Ruta propuesta

Se hace una etapa a la vez, y cada una tiene una salida audible.

| Etapa | Entrega | Cómo decidir si continuar |
|---|---|---|
| 1. Render mínimo | CLI sin interfaz: preset + MIDI → WAV + manifiesto | Mismo hash en dos corridas; afinación y duración correctas |
| 2. Audición renderizada | `make_audicion.py` produce tres opciones para **una** decisión real | Menos tiempo activo hasta `resolved` que grabando; la decisión se sostiene en el Brain |
| 3. Reimpresión | Una toma tocada se reimprime con otro preset desde MIDI + snapshot | Juan usa alguna reimpresión en una pieza |
| 4. Vigilante y fichas | Corpus fijo por commit; `key_range` y medidas propuestas para que Juan las confirme | Detecta un cambio real, y las fichas ahorran trabajo a mano |
| 5. Grabadora y calibración | `@GF PLAY`, grabadora y medición por engine, en el orden que Juan fije | Diferencia medida y documentada; decide si L hace falta |
| 6. Un experimento de ML | Solo uno, elegido por el cuello de botella observado | Supera su baseline con el mismo presupuesto de escucha (`motor-asistencia.md` §11) |

Las etapas 1–4 no requieren entrenar ningún modelo, pero la 1 ya deja listo el camino para generar datos. Corresponden a las etapas 2–3 de `ia-local-refinamiento.md` §8.

## 9. Ruta al gemelo completo con arquitectura IoT

**Línea futura, sin fecha (Juan, 2026-11-03).** Primero va la IA local. Esta sección guarda una idea que puede ser
innovadora: mostrar cómo crear gemelos digitales de instrumentos y qué funciones pueden tener en la música, a partir de
su experiencia en IoT. Nada de esta sección se implementa por ahora.

### 9.1 El Brain ya tiene parte del patrón

| Concepto IoT | En el Brain | Estado hoy |
|---|---|---|
| Dispositivo | Teensy con firmware Studio | Existe |
| Estado reportado | `PARAM_CHANGED`, `ENGINE_CHANGED`, `PRESET_LOADED`, `UI_STATE`, `LAYER_STATE` | **Existe, pero solo hacia la cabeza** por UART |
| Heartbeat | `GF_CMD_HEARTBEAT` a 1 Hz | Existe, solo por UART |
| Modelo del dispositivo | `gf_params.h`: `param_id` estable por engine, grupo e índice | Parcial; el plugin usa otros IDs y falta el mapa entre ambos |
| Telemetría | CPU y memoria de audio | Solo como texto en el log serie |
| Comandos | `@GF` por USB; `UI_INPUT` desde la cabeza | Gestión de presets y gestos; **no hay escritura directa de parámetros ni de estado** |
| Estado deseado y delta | Cambio pedido contra cambio aplicado | No existe |
| Gateway | Daemon en el Mac por USB, o la cabeza Pi por Ethernet | No existe |
| Identidad y versión | `@GF HELLO` | Falta el build del firmware |
| OTA | IDs `0x55–0x57` reservados | Sin flujo; hoy se flashea a mano |
| Hilo digital | Snapshots, hashes y registro de eventos | Parcial; se capturan por sesión, no como flujo |
| Flota | Una unidad | Se pensaría en el futuro |

El [Device Shadow de AWS](https://docs.aws.amazon.com/iot/latest/developerguide/device-shadow-document.html) separa
`desired`, `reported` y `delta`; [Eclipse Ditto](https://projects.eclipse.org/projects/iot.ditto) también maneja
valores reportados y deseados. Sirven como patrón de diseño. No hace falta usar esos servicios ni la nube.
[DTDL](https://azure.github.io/opendigitaltwins-dtdl/DTDL/v3/DTDL.v3.html) y la
[Thing Description de W3C](https://www.w3.org/TR/wot-thing-description10/) son opciones para describir el modelo.

### 9.2 Arquitectura propuesta

```text
 Brain (Teensy)             Gateway local                 Gemelos y clientes
┌──────────────┐  USB   ┌───────────────────┐  MQTT    ┌──────────────────────┐
│ Audio        │  serie │ Daemon            │  local   │ Plugin (gemelo)      │
│ (no depende  │◄──────►│ · dueño del puerto│◄────────►│ Registro de eventos  │
│  del enlace) │  MIDI  │ · reported/desired│          │ Dashboard de salud   │
│ Reporte      │───────►│ · delta y permisos│          │ IA local (propone)   │
│ no bloqueante│        └───────────────────┘          └──────────────────────┘
└──────────────┘

grooveforge/brain/<id>/reported/...    estado retenido
grooveforge/brain/<id>/desired/...     cambios pedidos
grooveforge/brain/<id>/telemetry/...   CPU, memoria, enlace
```

Reglas que no se negocian:

1. **El audio del Brain nunca depende del enlace.** Si el gemelo o el daemon fallan, el instrumento sigue tocando,
   igual que `ia-local` §4 exige para Live.
2. **El reporte no bloquea.** Si el puerto está lleno se descarta y se conserva el último valor. Hay que verificar que
   `Serial` de la Teensy no se bloquee con el computador lento, y medir que el total siga ≤60 % de CPU.
3. **El panel físico tiene prioridad.** Un cambio pedido se descarta si Juan está tocando ese control. Falta definir la
   regla con tres escritores posibles: panel, automatización de Live e IA (`ia-local` §4).
4. **Las notas nunca se agrupan ni se descartan.** Los parámetros sí: último valor gana. Las notas siguen por MIDI.
5. **Los cambios pedidos van a RAM.** El daemon no expone `FORMAT` ni escrituras al flash sin confirmación.
6. **Todo funciona sin internet.**

El daemon en el Mac es el primer gateway que propongo: corre junto al gemelo y resuelve que solo una sesión pueda hablar
por el puerto serie. La cabeza Pi queda como opción para el vivo.

### 9.3 Qué funciones puede tener en la música

| # | Función | Qué hace | Requiere |
|---|---|---|---|
| 1 | **Espejo en vivo** | Mueves un encoder en el Brain; la perilla del plugin se mueve y su sonido cambia | T1 |
| 2 | **Máquina del tiempo** | Reconstruir el estado exacto del instrumento en cualquier momento de una sesión o toma | T2 |
| 3 | **Control desde el gemelo** | Editar en pantalla grande y que el Brain físico responda | T4 |
| 4 | **Salud del instrumento** | Historial de CPU, memoria, enlace y errores; aviso antes de un fallo en vivo | T3 |
| 5 | **Probar antes de flashear** | Pasar un corpus de presets por el DSP nuevo y mostrar qué cambió de sonido | T0 + vigilante (D) |
| 6 | **Respaldo en vivo** | Si el Brain falla, una instancia con el mismo estado toma su lugar | T2 + ruteo en Live |
| 7 | **Instrumento más allá del hardware** | Tocas una parte en el Brain y otras instancias guardan el sonido exacto para las demás pistas | T2 |
| 8 | **IA con estado real** | La IA lee `reported` y escribe una propuesta; el delta muestra qué cambiaría y Juan lo acepta | T4 + §5 H |
| 9 | **Colaboración** | Enviar estado + MIDI para que otra persona oiga el sonido sin tener el Brain | T2; decisión de producto abierta |
| 10 | **Gemelo del rig** | Brain, Push, e-drum y mixer como dispositivos relacionados, con recuperación de la escena completa | Después; el mixer analógico no reporta sus ajustes |

La función 8 une las dos líneas. Una propuesta de IA es un estado deseado: cambio pequeño, visible y recuperable, como
pide `motor-asistencia.md`. La función 6 es la más incierta, porque cambiar de fuente en plena interpretación puede
oírse, y hay que probarla antes de prometerla.

### 9.4 Hitos, cada uno demostrable

| Hito | Entrega | Demo o puerta | ¿Toca firmware? |
|---|---|---|---|
| T0 | Contrato: modelo del dispositivo generado desde `gf_params.h` y mapa de IDs al plugin | Cada parámetro hace ida y vuelta sin perder valor | No |
| **T1** | Espejo parcial: el plugin sigue los CC que el panel ya emite por USB | **En video: mueves el Brain y el plugin lo sigue, sin flashear** | No |
| T2 | Sombra completa: `@GF STATE` al conectar más cambios en vivo, hash para detectar desincronización | Cambias engine y preset en el panel; el plugin coincide y una desconexión se recupera sola | Sí |
| T3 | Telemetría y dashboard | Se ve la CPU real del Teensy al tocar | Sí |
| T4 | Estado deseado: cambio desde el plugin, con ack, delta y prioridad del panel | **Gemelo completo: el plugin mueve al Brain y el Brain mueve al plugin**, sin lazos | Sí |
| T5 | Calibración: grabadora y medición por engine | Informe publicado de cuánto se parecen y dónde difieren | Sí (`@GF PLAY`) |

T1 tiene límites conocidos: solo cubre parámetros con CC, no el cambio de engine o preset, y el plugin aún no atiende
todos los CC en Prophet (auditoría 2026-11-01). Basta para validar el concepto en poco tiempo.

El render mínimo de §8 hace falta para la función 5 y para T5. Las dos líneas compiten por las mismas 10–18 h semanales,
así que hay que elegir cuál avanza primero.

### 9.5 Dónde el instrumento se aleja de un dispositivo IoT típico

- **Sincronizar el estado no sincroniza el sonido.** Dos instancias con los mismos parámetros tienen fases y ruido
  propios; ADR-006 ya descarta el null test. La coincidencia se mide con medidas de audio y escucha.
- **El ritmo de datos es otro.** Un encoder genera ráfagas y un acorde debe llegar a tiempo. La latencia del espejo
  afecta a lo que se ve y se oye: hay que fijar su objetivo y medirlo, también a través del aislador USB que usas.
- **El puerto serie es exclusivo.** Grabadora, bibliotecario, monitor y daemon no pueden abrirlo a la vez. Todos
  tendrían que pasar por el daemon.
- **El estado completo es más que los parámetros.** Incluye bancos, capa, foco, memorias de acorde y transporte. Lo que
  no quepa en el modelo debe declararse, no ignorarse.

### 9.6 El valor de innovación

La búsqueda ubicó el campo del **Internet of Musical Things** (Turchet et al.) y el de instrumentos inteligentes, pero
una búsqueda rápida no encontró un trabajo que aplique gemelo digital a un sintetizador físico. Eso **no demuestra que
sea algo inédito**: antes de afirmarlo en público hace falta una revisión en IEEE Xplore, ACM, NIME y AES.

Lo que sí se puede mostrar con evidencia propia: mismo instrumento, dos cuerpos sincronizados en cámara, sus diferencias
medidas y publicadas, y una arquitectura reutilizable por otros constructores. Encaja con el contenido de GrooveMakers.
Publicar la arquitectura o el plugin sigue siendo decisión tuya; ADR-006 deja abierta la parte de producto.

## 10. Contradicciones y decisiones para Juan

Estas contradicciones quedan señaladas sin resolver:

1. **«Una sola fuente de DSP» (ADR-006) contra rutas de voz propias en el plugin (auditoría 2026-11-01).** Mientras
   exista esa diferencia, el dataset puede dejar de representar al Brain, justo el riesgo por el que ADR-006 descartó
   la opción D.
2. **Dónde vive el renderizador.** El spec §4 pide `apps/gf-render` sobre `libs/gf-dsp` y `libs/gf-instrument`, que no
   existen. Lo rápido es un renderizador sobre el plugin; lo limpio, esperar las librerías.
3. **`motor-asistencia.md` §1 dice que v1 no genera audio.** Renderizar un preset no compone nada, pero conviene
   aclarar que está permitido. ADR-006 ya lo anticipa para `make_audicion.py`.
4. **Renders como datos de entrenamiento.** ADR-006 (contradicción 4) y el spec §12 dejan abierto si caben en `04`.
   Juan ya indicó que el VST es para entrenar modelos; falta registrar eso frente a `04`. Se etiquetan `source: render`.
5. **El spec §2 deja el control bidireccional fuera de v1,** y `00` no incluye el gemelo en ningún nivel del North Star.
   El objetivo de §9 necesita su propio spec o ampliar el actual, y un ADR que lo declare.
6. **`grabadora-automatica.md` §2 hace a la grabadora dueña exclusiva del puerto serie.** Con el daemon de §9.2 pasaría
   a ser un cliente más.

Decisiones:

| # | Decisión | Mi recomendación |
|---|---|---|
| 1 | ¿Qué línea avanza primero? | **Decidido por Juan:** IA local; el gemelo completo queda para después |
| 2 | ¿Renderizador sobre el plugin o esperar las librerías? | Sobre el plugin, con la procedencia indicando qué ruta de DSP usó |
| 3 | ¿Un preset nacido en el gemelo puede subir al Brain sin escucharlo allí? | No: debe pasar la exportación y una escucha en el Brain |
| 4 | ¿Los renders pueden entrenar modelos? | **Juan: sí, es el propósito del VST.** Falta acordar cómo se etiquetan frente a `04` (contradicción 4) y qué tomas reales se reservan para probar |
| 5 | Gemelo completo: ¿gateway en el Mac o en la cabeza Pi? | Para cuando se retome: Mac |
| 6 | Gemelo completo: ¿MQTT desde el primer día o un socket local? | Para cuando se retome: socket en T1, MQTT en T2 |
| 7 | ¿Cuál es la primera decisión musical de prueba? | Una decisión abierta de `est-indie` (rama `feat/studio-engines-10x`), que ya tiene audición de clips y registro |

## 11. Referencias

Las consulté por buscador el 2026-11-03, en registros y resúmenes, sin leer los textos completos. CMA-ES se cita de
memoria.

- Grieves, M. y Vickers, J. «Digital Twin: Mitigating Unpredictable, Undesirable Emergent Behavior in Complex Systems».
  En *Transdisciplinary Perspectives on Complex Systems*, Springer, 2017.
  [DOI](https://doi.org/10.1007/978-3-319-38756-7_4).
- Kritzinger, W. et al. «Digital Twin in manufacturing: A categorical literature review and classification».
  *IFAC-PapersOnLine* 51(11), 2018. [DOI](https://doi.org/10.1016/j.ifacol.2018.08.474).
- Yee-King, M., Fedden, L. y d'Inverno, M. «Automatic Programming of VST Sound Synthesizers Using Deep Networks and
  Other Techniques». *IEEE TETCI* 2(2), 2018.
- Barkan, O. et al. «InverSynth», 2019. Citado en
  [CCRMA, 2024](https://ccrma.stanford.edu/events/ai-based-digital-synthesizer-preset-programming-parameter-estimation-sound-matching).
- Hayes, B., Shier, J., Fazekas, G., McPherson, A. y Saitis, C. «A Review of Differentiable Digital Signal Processing
  for Music and Speech Synthesis». *Frontiers in Signal Processing* 3, 2024.
  [DOI](https://doi.org/10.3389/frsip.2023.1284100).
- Hansen, N. y Ostermeier, A. «Completely Derandomized Self-Adaptation in Evolution Strategies». *Evolutionary
  Computation* 9(2), 2001.
- Steinmetz, C. y Reiss, J. «auraloss: Audio-focused loss functions in PyTorch». DMRN+15, 2020.
  [Código](https://github.com/csteinmetz1/auraloss).
- González, J., Dai, Z., Damianou, A. y Lawrence, N. «Preferential Bayesian Optimization». ICML 2017.
  [arXiv](https://arxiv.org/abs/1704.03651).
- Turchet, L. et al. «Internet of Musical Things: Vision and Challenges». *IEEE Access*, 2018.
  [Registro](https://doaj.org/article/4e6f5eb17f864e1a8505b686ad609608).
- Turchet, L. «Smart Musical Instruments: Vision, Design Principles, and Future Directions». *IEEE Access*, 2019.
  [Registro](https://doaj.org/article/40191b82d1754b28b398f0e7f8780736).
- ISO 23247-1:2021. *Digital twin framework for manufacturing*, parte 1.
  [Registro](https://committee.iso.org/standard/75066.html?browse=ics).
- Tobin, J. et al. «Domain Randomization for Transferring Deep Neural Networks from Simulation to the Real World».
  IROS 2017. [arXiv](https://arxiv.org/abs/1703.06907).
- Wright, A., Damskägg, E.-P., Juvela, L. y Välimäki, V. «Real-Time Guitar Amplifier Emulation with Deep Learning».
  *Applied Sciences* 10(3), 2020. [DOI](https://doi.org/10.3390/app10030766).
