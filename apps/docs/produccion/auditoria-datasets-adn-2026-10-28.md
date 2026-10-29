# Auditoría del levantamiento del ADN musical y preparación para IA local

**Fecha:** 28 de octubre de 2026 · **Destinatario:** Juan · **Estado:** evaluación y propuestas para decidir; no modifica los specs ni implementa modelos.

## 1. Dictamen

**El laboratorio está bien encaminado para descubrir tu criterio musical. Todavía no constituye un dataset validado para entrenar un generador de sonido o un compositor que represente tu identidad.** Sí permite empezar a construir una biblioteca asistida y variaciones MIDI guiadas por tus elecciones.

Lo más valioso de hoy no son los archivos generados: son las diferencias que ya puedes expresar. Prefieres ciertas combinaciones de articulación, cuerpo, tensión, percusión y contexto; además, cambiaste de opinión cuando cambió el acompañamiento. Eso es información útil para personalizar una herramienta.

El problema principal es que esas decisiones están repartidas entre relatos, listas de favoritos, presets y sesiones. En varios casos no se puede reconstruir **qué combinación exacta escuchaste**. Si se entrenara directamente con lo disponible, el modelo podría aprender las reglas del generador y las interpretaciones del asistente con más fuerza que tu criterio.

Mi recomendación es continuar el trabajo musical, pero mejorar ahora tres cosas: **registrar cada escucha con su contexto, conservar versiones y probar las conclusiones en música completa**. El primer ML personalizado que ensayaría sería un selector de candidatos según tu gusto, sobre propuestas MIDI editables. La generación de audio sería un experimento posterior con su propia captura.

## 2. Qué revisé y qué puedo afirmar

Revisé el informe solicitado, ambos laboratorios, paleta, captura, percusión, documentación de IA local, las siete fichas de sesión, los JSON del dataset y los generadores relevantes. También inspeccioné los archivos MIDI, las cabeceras de audio y la estructura XML de los sets de Live.

El informe `revision-2026-10-28.md`, `lab-leads.md` y `lab-acordes.md` están en el worktree **dreamy-goodall-071cc6**, no en el checkout principal. Para los resultados de hoy usé esas versiones y las fichas reales, no únicamente los documentos anteriores del checkout principal. Las rutas verificables están en §13.

**Límite:** esta es una auditoría documental, de datos y del diseño de experimentos. No escuché las tomas, no operé el Brain, no ejecuté entrenamiento ni medí rendimiento del Mac. No atribuyo calidad audible a un preset por sus parámetros. Los contadores siguientes son una fotografía del material disponible al revisar; no equivalen a un inventario de toda tu producción histórica.

### Inventario comprobado

| Evidencia | Resultado | Qué significa |
|---|---:|---|
| MIDI en `GrooveForge_Dataset_v1` | 1.713 | Volumen de archivos, no de ideas independientes |
| MIDI distintos por SHA-256 del archivo | 1.265 | 448 archivos son copias adicionales idénticas, aproximadamente 26,2 % |
| Secuencias distintas de eventos de canal, ignorando metadatos | 1.103 | Comparación orientativa adicional; tampoco mide diversidad musical independiente |
| Resolución y tempo de los MIDI | 480 PPQ; aproximadamente 124 BPM en todos | Buena homogeneidad para el lab; no hay evidencia de generalización a otros tempos |
| Errores al leer MIDI / JSON | 0 / 0 | Integridad sintáctica; no valida etiquetas, notas o calidad musical |
| Presets del Brain | 118 JSON: 97 de engine y 21 capas | Biblioteca recuperable; no son 118 sonidos elegidos por ti |
| Presets con `role` / `tags` completados | 17 / 17, de 118 | La clasificación musical sigue incompleta |
| Presets con `key_range` / `pairs_with` completados | 0 / 1, de 118 | Falta información importante para recomendar en contexto |
| Lab de leads | 48 leads + 12 acompañamientos + 6 variantes de dosis | Muchas combinaciones proceden de una misma receta |
| Lab de acordes | 72 MIDI | La primera escucha documentada cubre 21 escenas; 16 gustaron y 5 quedaron sin marcar |
| Selección de percusión de ronda 2 | 37 archivos: 35 Tribe, 2 de 808 | El texto «34 de 37» no coincide con los archivos actuales ni con sus favoritos |
| Fichas de sesión | 7, todas fechadas el 28 de octubre | Siete roles no equivalen a siete días de evaluación independiente |
| Listas estándar `decisiones` / `rechazados` | 1 / 1 registros | Hay mucha más información en campos anidados y prosa; el formato común no la recoge |
| `origen_permisos` en las sesiones | Vacío en las 7 | Hay permisos en otros archivos, pero no una trazabilidad completa por escucha |
| Audio dentro del dataset | 0 archivos de audio | `originals`, `renders`, `studies`, `references` y `exports` están vacíos de archivos visibles |

**Sí existe audio fuera del dataset:** en `GFExploration Project/Samples/Recorded` encontré seis WAV, aproximadamente **48,78 segundos en total**, estéreo, 24 bit, **44,1 kHz**. Los sets de leads, dialectos y acordes contienen referencias a algunas de esas tomas; son referencias repetidas, no diez grabaciones distintas por set. No identifiqué por escucha qué preset contiene cada WAV.

El protocolo establece **48 kHz**, por lo que hay una diferencia entre lo prescrito y lo capturado. No invalida las tomas: registrar la frecuencia real y enlazarlas con su función. Si un modelo exige otra frecuencia, generar una copia derivada; cambiar la cabecera no es remuestrear.

## 3. Lo que conservaría

1. **Separar sonido, patrón y contexto.** El paso de Tribe en todos los bloques a kits por dialecto con una dosis de Tribe produjo una observación concreta sobre identidad y función.
2. **Probar el sonido con MIDI repetible.** Los clips neutros permiten comparar instrumentos; los musicales permiten decidir si los usarías. Conviene mantener ambos y etiquetarlos como tales.
3. **Trabajar con pocas opciones por decisión.** Original + dos variantes es una unidad manejable, aunque el set tenga muchas escenas preparadas.
4. **Guardar presets y MIDI editables.** Es mucho más útil para producción que conservar únicamente un WAV o una descripción.
5. **Conservar tus palabras y tus cambios de opinión.** «Le falta brillo» o «no sé si es el sonido del lead» tienen más información que una etiqueta genérica de aprobado.
6. **La separación top/perc y el mapa de roles.** Facilita probar arreglos y cambiar instrumentos. Debe mantenerse la versión del mapa y la correspondencia con la articulación real.
7. **Grabar por lotes al final de la selección.** Es una decisión razonable para no frenar la escucha. Su condición es conservar el estado exacto de lo escuchado antes de sobrescribirlo.

El trabajo actual ya tiene utilidad musical. La mejora necesaria es que esa utilidad también quede representada en datos consistentes.

## 4. Qué conclusiones sobre tu ADN sostienen los datos

La confianza indicada aquí es cualitativa y se refiere a esta muestra. Una preferencia artística tuya puede adoptarse de inmediato; afirmar que el experimento demostró su causa requiere más control.

| Hallazgo | Evaluación | Cómo lo guardaría |
|---|---|---|
| Tribe como firma, dosificado según el contexto | Bien respaldado dentro del lab: selección reiterada y aprobación explícita de las dosis | Preferencia creativa actual; no obligación para todos los tracks |
| Mezcla de notas cortas y largas | Respaldado: 11 de 16 favoritos iniciales son mixtos; también hay 3 cortos y 2 largos | Tendencia, con excepciones aceptadas |
| Menor con V mayor, mixta B | Señal fuerte dentro de la ronda: pasa en 5 contextos | Preferencia por ese clip y sus acompañamientos; no por cualquier melodía en menor armónica |
| Lidia como color luminoso | Declaración explícita y paso en afro, organic y melodic | Hipótesis prioritaria para nuevas frases y arreglos |
| Abiertos en tresillo 3-3-2 | 9 de 9 combinaciones ensayadas gustaron con Electric y contexto afro | Preferencia contextual; falta comparar los mismos voicings con otros ritmos |
| JD PLK WOOD y dirección JD SUS DARK | Hay elección y comentario de refinamiento | Favorito y candidato a ajustar, respectivamente; no dos sonidos terminados |
| «El color va en el acorde, no en la melodía» | Generalización excesiva; la lidia gustó y los ensayos de dosis no mantuvieron resultados estables | Interpretación del asistente, pendiente de contraste |
| «La segunda aumentada repetida es la causa» | No demostrado: también cambiaron armonía, alturas, registro o preset | Hipótesis causal, no etiqueta negativa de entrenamiento |
| «No me gusta el DX7» | Se rechazaron presets concretos; luego gustó una capa Juno + DX7 | Rechazo de un estímulo en contexto, no de todo el engine |
| «Un solo acorde de afuera» define el ADN | No aislado experimentalmente; depende de cuál sea la escala de referencia | Propuesta compositiva, no restricción del generador |

Dos correcciones de lectura importan para el ML:

- `lab-leads.md` cuenta **4 mixta A + 8 mixta B**, pero la ficha y la tabla contienen **4 + 7**. El resultado correcto es **11/16 mixtos**. No cambia la dirección del hallazgo, sí su trazabilidad.
- En el lab de escalas también cambia la **progresión**; además, el generador ajusta notas a los acordes. Por tanto, «solo cambia el color» no describe un experimento aislado de escala. Lo aprobado es una combinación de frase, armonía y sonido. La ronda con presets alternados refuerza esta limitación.

También importa la oferta: el lab prepara **24 clips mixtos, 12 cortos y 12 largos**. Los mixtos tenían el doble de oportunidades de aparecer entre favoritos. Tu declaración explícita respalda la preferencia; el 11/16 por sí solo no mide una ventaja imparcial. Para calcular tasas por familia habría que confirmar cuáles se escucharon realmente.

El nombre musical tampoco debe convertirse automáticamente en verdad de entrenamiento. Por ejemplo, Am–B♭–Am–Gm contiene acordes mayores y menores y fue rechazado en esos ensayos: «mezclar mayor y menor» no explica por sí solo los aprobados. B mayor es parte de La lidia, no un acorde externo a ese modo; E mayor cumple una función dominante habitual en La menor. Conviene almacenar las notas y los acordes reales, además del nombre usado en el lab.

Mantendría el cierre creativo de la línea de dosis si ya no te interesa. Cambiaría su interpretación documental a **«no priorizada tras esta escucha; causa incierta»**. No hace falta reabrir todas las exploraciones para evitar enseñarle al modelo una prohibición que no se demostró.

## 5. Riesgos del levantamiento actual y mejoras prioritarias

### P0 · La unidad de aprendizaje debe ser una escucha completa

Ahora hay listas de clips aprobados, pero no siempre una unión inequívoca entre clip, preset, acorde, kit, mezcla y resultado. En la ronda de dialectos se alternaron JD LEAD BODY y WARM KEYS sin asignar cada preset a cada aprobación. Ese dato no puede reconstruirse con certeza a posteriori.

**Mejora:** identificar el estímulo completo: MIDI exacto + preset exacto + acompañamiento + niveles/FX relevantes + resultado. Si falta una parte histórica, marcarla `unknown`; no completar con lo que «probablemente» ocurrió. Un aprobado de la mezcla no debe propagarse automáticamente como aprobado individual de todos sus componentes.

### P0 · Distinguir rechazo, empate, desconocido y no escuchado

`sin_marcar` no significa rechazo. Un favorito no establece que todos los demás se escucharon y perdieron. El registro de bajo incluso contiene `elegido: "ambos (papeles distintos)"`, mientras `gf_session.py` permite A/B y su presentación interpreta cualquier valor distinto de `a` como B. Un consumidor ingenuo convertiría una coexistencia útil en una preferencia falsa.

**Mejora:** estados explícitos `prefer_a`, `prefer_b`, `tie`, `both_useful`, `neither`, `uncertain`, `not_auditioned`; conservar el motivo y la tarea. No convertir los 16 favoritos en miles de pares artificialmente independientes. Registrar también qué opciones fueron presentadas y escuchadas: sin ese denominador no se puede medir tasa de aceptación.

### P0 · Preservar versiones, no únicamente nombres

Un archivo exportado con el mismo nombre puede recibir parámetros nuevos. La ficha conserva un nombre, pero eso no congela lo que sonó. El commit de Git del ordenador tampoco demuestra por sí solo qué firmware estaba cargado ni si había cambios locales.

**Mejora:** snapshot inmutable del patch y su hash; MIDI con hash; versión del mapa; preset del plugin; archivo de Live y escena; identificación de firmware cargado cuando pueda verificarse. Si hay azar, guardar semilla cuando exista y reconocer que el estado del sintetizador o plugin puede impedir reproducción idéntica muestra a muestra. El audio de referencia resuelve parte de esa incertidumbre.

### P1 · Evitar aprender únicamente las recetas del generador

Los 48 leads comparten cuatro esqueletos rítmicos; los 72 acordes proceden de tres progresiones × seis órdenes × cuatro ritmos. Las variaciones de percusión derivan de familias con mutaciones y semillas. Esto es excelente para explorar; no equivale a cientos de composiciones personales independientes.

**Mejora:** diferenciar `authored_by_juan`, `generator_proposal`, `juan_edited`, `reference` y `augmentation`. Guardar el antes y después de tus correcciones. Mover una nota, cortar una cola o quitar un golpe para hacer funcionar una frase enseña la transformación que deseas, no solo tu elección entre propuestas ajenas.

No eliminar las copias de favoritos: sirven para organizar. Evitar contarlas repetidamente al construir un entrenamiento o una evaluación.

### P1 · Los filtros del laboratorio también crean sesgos

Todos los MIDI examinados llevan aproximadamente 124 BPM; los labs melódicos se diseñan alrededor de La. El requisito de pasar siempre por B favorece material que tolera ese contexto oscuro. La mayor presencia de `peak`, la falta de hats en el mapa y los kits escogidos influyen en lo que parece funcionar por dialecto.

**Mejora:** mantener 124/La como control; después probar pocos finalistas a 122 y 126 BPM, en otro registro o centro tonal, y en una sección menos densa. Una transposición es una comprobación de robustez o una derivación, no una nueva preferencia observada hasta que la escuches. Registrar la regla de B como criterio del lab, no como definición universal de tu identidad.

Los dialectos incluidos en los nombres de los MIDI son **expectativas del generador**, no etiquetas observadas. Guardarlos en un campo distinto del contexto en que efectivamente gustaron; al comparar, usar ocasionalmente nombres neutros A/B para reducir el efecto de esas expectativas.

### P1 · El registro necesita tiempo y revisiones

GF HOUSE KEYS aparece como ganador temprano y luego entre los presets rechazados frente a Electric. Es una evolución razonable por cambio de referencia y tarea; sobrescribir una etiqueta pierde esa información.

**Mejora:** decisiones con hora, ronda, objetivo e `identity_version`; una decisión nueva puede matizar una antigua sin borrarla. Separar claramente cita literal, observación y explicación del asistente. La base puede decir «prefiere X en esta tarea» aunque en otra use Y.

### P1 · Procedencia y permisos tienen que acompañar cada representación

Los kits de Brasil y djembe dicen `revisar`; Tribe indica revisar entrenamiento/sampleo; el 808 tiene `sí` sin evidencia de licencia en esa ficha. Los 118 patches tienen `train/publish: true`, y el bibliotecario asigna esos valores por defecto para presets del Brain. `gf_session.py permiso` también activa permisos salvo negación. Esos campos describen una declaración o un default, no una comprobación independiente.

**Mejora de datos:** separar permiso para la composición MIDI, el audio renderizado, entrenamiento, distribución y publicación; enlazar fuente y versión de la licencia. Material pendiente puede seguir en el catálogo con su estado, sin entrar automáticamente al manifiesto de entrenamiento. La adaptación de un MIDI de librería debe conservar su procedencia, aunque musicalmente lo hayas hecho tuyo. Esta auditoría no determina la licencia concreta de Tribe ni de tus samples.

## 6. Un ADN musical, varios conjuntos de datos

No intentaría alimentar todos los modelos con una sola carpeta. Mantendría los originales y crearía vistas según tarea, enlazadas por IDs.

| Tarea | Unidad de datos | Qué añadir | Primera solución útil |
|---|---|---|---|
| Encontrar un sonido | Patch + audición + descripción + rol | Render consistente y etiquetas tuyas | Catálogo y filtros; embeddings cuando aporten valor |
| Elegir lo que encaja contigo | Contexto + candidatos expuestos + decisión | A/B, empate, ambos, motivo, revisión posterior | Reglas de preferencia; después ranking pequeño |
| Variar patrones | MIDI original → MIDI corregido, armonía y rol | Tus ediciones, articulación, silencios y fraseo | Mutaciones acotadas y recuperables |
| Generar sonidos en audio | Audio utilizable + descripción + procedencia | Cobertura tímbrica y captura específica del modelo | Evaluar un modelo preentrenado antes de adaptarlo |
| Proponer presets | Audio objetivo + candidatos de patches y renders | Pares audio–patch y registro de ajustes | Recuperación de candidatos; después optimización acotada |
| Ayudar a terminar tracks | Proyecto, secciones, stems y decisiones | Transiciones, automatizaciones, versiones y export final | Recuperar estructuras propias y sugerir siguiente paso |

Un embedding es una representación para comparar; no genera audio ni explica automáticamente qué sonido te gusta. Un modelo de ranking ordena candidatos; no compone. Un generador de MIDI conserva notas editables; uno de audio requiere otra representación y otra evaluación.

**La oportunidad más cercana:** producir tres variantes MIDI, escucharlas con tu paleta y que la herramienta aprenda a mostrar primero las que suelen servirte. Ya dispones de generadores como baseline. No necesitas empezar entrenando un Transformer desde cero.

## 7. Qué datos agregaría sin convertirte en anotador de tiempo completo

### Registro mínimo por decisión

Los datos técnicos deben capturarse automáticamente en una futura herramienta. Tú solo deberías responder: **qué prefieres, para qué y qué cambiarías**.

| Grupo | Campos propuestos | Razón |
|---|---|---|
| Identidad | `schema_version`, `event_id`, fecha/hora, `session_id`, ronda | Poder interpretar y ordenar cambios |
| Estímulo | IDs/hash de MIDI, patch, acompañamiento y escena | Saber qué escuchaste realmente |
| Contexto | Rol, dialecto ensayado, sección, BPM, centro tonal, acordes | Evitar preferencias sin función musical |
| Presentación | Opciones expuestas, orden, escuchada sí/no, ajuste de nivel | Separar preferencia de disponibilidad y volumen |
| Resultado | Estado explícito, motivo literal, confianza declarada opcional | Conservar incertidumbre y empates |
| Edición | `parent_id`, qué cambiaste, versión resultante | Aprender transformaciones útiles |
| Origen | Autoría, generador/versión/semilla, fuente y estado de permisos | Distinguir propuesta, interpretación y derivación |
| Revisión | Evidencia que respalda una regla, estado provisional/confirmado/superado | Evitar que una conclusión del asistente se vuelva un hecho |

Ejemplo **ilustrativo, no una decisión histórica de Juan**:

```json
{
  "schema_version": 2,
  "event_id": "ejemplo-no-importar",
  "task": "elegir_timbre_acordes",
  "context": {
    "dialect": "afro",
    "section": "groove",
    "bpm": 124,
    "midi_id": "snapshot_del_clip",
    "backing_id": "snapshot_del_acompanamiento"
  },
  "candidates": [
    {"id": "patch_A_inmutable", "auditioned": true},
    {"id": "patch_B_inmutable", "auditioned": true}
  ],
  "decision": "prefer_a",
  "reason": "deja oír el ataque de la percusión",
  "inference": {"claim": "menos cola ayuda aquí", "status": "hypothesis"}
}
```

**No cargaría a mano los 118 presets de una vez.** Completaría primero los finalistas y los controles que se vuelvan a escuchar: rol, registro útil, combinaciones, una descripción concreta y snapshot. Ausencia de estrellas no es un defecto: las decisiones contextuales son más valiosas.

### Nuevas observaciones de alto valor

- **Correcciones tuyas:** conservar original y versión editada, aunque solo cambien dos notas.
- **Casi aciertos:** candidato que gusta pero tapa al bajo, tiene demasiado ataque o funciona solo en el break. Es más informativo que un rechazo sin explicación.
- **Una alternativa inesperada:** de vez en cuando, incluir una opción fuera de los favoritos para comprobar si el catálogo está estrechando tu gusto.
- **Uso real:** registrar si una propuesta terminó en un estudio, fue modificada o se descartó después. «Me gusta aislada» y «la usaría en este track» son tareas distintas.
- **Interpretación humana:** si deseas aprender tu toque, guardar MIDI tocado sin cuantizar, versión cuantizada y edición final, con velocity, note-off, CC y pitch bend cuando se usen.

El swing, jitter de velocity y alternancia de articulaciones de los scripts son **humanización programada**. No prueban que se haya capturado tu ejecución o la de un percusionista. Por ejemplo, alternar según paridad de semicorchea no garantiza alternar manos entre golpes sucesivos si hay huecos. Etiquetaría esos datos como generados; para aprender una interpretación nativa hay que registrar una interpretación real y su contexto.

## 8. Captura de audio: conservar el plan y cerrar sus huecos

Respetaría la grabación final por lotes. Antes de seguir modificando los ganadores, guardaría sus snapshots. Si una actualización de firmware cambia su sonido, la captura previa que ya contempla el protocolo cobra especial importancia.

En la sesión final distinguiría:

| Captura | Contenido | Para qué sirve |
|---|---|---|
| Instrumento aislado | Patch, MIDI y ganancia; sin FX de Live | Comparar timbre y recuperar la interpretación |
| Stem en contexto | Solo la pista del instrumento, con su procesamiento contextual si existe | Mezcla y reutilización |
| Contexto completo | Backing + instrumento, con mezcla identificada | Recuperar lo que motivó la decisión |
| Procesado | Cadena de FX y automatización que forman parte del resultado | Documentar transformación y producción |

El actual «en contexto» graba **solo el Brain**. Si el MIDI y el procesamiento son iguales, ese archivo no contiene el acompañamiento que te hizo preferirlo; puede resultar equivalente al aislado. Guardar el backing exacto y sus ajustes, o un mix de referencia adicional, cierra ese hueco.

Además:

- Mantener la distinción **«sin FX de Live»** frente a **«sin FX internos del Brain»**. El protocolo llama seco a un patch que conserva sus FX internos; eso es válido si se etiqueta correctamente.
- Guardar toma original sin normalizar y ganancia de comparación aparte. No usar la normalización para ocultar clipping.
- Registrar sample rate real, canales, duración, offset de inicio, colas, fuente de clock y cualquier warp/compensación. El procesamiento para un modelo se hace sobre derivados versionados.
- Para la paleta, basta una cobertura compacta de registros y velocities relevantes. Para aprender respuesta de un instrumento o audio→patch se necesita un barrido más controlado; no convertir cada preset en una sesión enorme sin elegir primero la tarea.
- Conservar algunos casi aciertos si interesa aprender preferencias **tímbricas desde audio**. Grabar solo ganadores da ejemplos positivos, pero no permite escuchar por qué perdió el vecino. Es una ampliación propuesta para la captura final, no una obligación de grabar todas las sesiones.

Los seis WAV encontrados deben catalogarse antes de decidir si sirven como contexto, prueba técnica o entrenamiento. No asumir que contienen los ganadores por estar en una pista llamada BASS o BRAIN AUDIO.

## 9. Modelos locales: qué probaría y en qué orden

La elección debe seguir el problema que consume tiempo, no la lista de modelos disponibles. Las siguientes son rutas de experimento, no una selección aprobada de checkpoints.

| Orden | Enfoque | Encaje actual | Condición para avanzar |
|---|---|---|---|
| 1 | Catálogo + reglas + variaciones MIDI existentes | Alto: aprovecha los datos actuales | Recuperar y editar ideas más rápido |
| 2 | Ranking pequeño de preferencias, con pocas características musicales | Prometedor tras mejorar los eventos | Predecir elecciones futuras mejor que favoritos por contexto |
| 3 | Recuperación de audio con embeddings preentrenados | Útil al disponer de audiciones | Encontrar sonidos útiles mejor que etiquetas y carpetas |
| 4 | Adaptación de generador simbólico preentrenado | Condicional: faltan más familias y ediciones personales | Mejorar el baseline sin memorizar recetas |
| 5 | Adaptación de generador de audio | Hoy no preparada | Audio/procedencia adecuados, objetivo acotado y benchmark local |
| 6 | Instrumento neural o inversión audio→patch | Investigación específica | Captura diseñada para esa tarea y beneficio musical comprobado |

**CLAP:** proporciona embeddings de audio y texto; su API documenta el preprocesamiento, incluido 48 kHz para la entrada de audio por array. Lo probaría como buscador, combinado con rol, registro y tus etiquetas. Que dos audios estén próximos en ese espacio no demuestra que uno sea más «tuyo». [Fuente: LAION CLAP](https://github.com/LAION-AI/CLAP).

**MIDI:** preservaría los ticks originales. REMI discretiza la posición según `beat_res`; PerTok contempla microtiming explícito. La elección de tokenizer debe verificar que una ida y vuelta conserva las desviaciones y articulaciones que te importan. Los 480 PPQ actuales dan aproximadamente 1 ms por tick a 124 BPM, pero no implican que exista información humana en esos tiempos. [Fuente: MidiTok](https://miditok.readthedocs.io/en/latest/tokenizations.html).

**Stable Audio:** `stable-audio-tools` documenta configuración de dataset/modelo y continuación desde checkpoints. Eso permite evaluar adaptación, pero no demuestra que una receta LoRA determinada funcione en tu Mac ni que un pequeño conjunto de ganadores sea suficiente. Empezaría por una tarea estrecha —por ejemplo, texturas de transición— y evaluaría primero el modelo sin adaptar. [Fuente: stable-audio-tools](https://github.com/Stability-AI/stable-audio-tools).

**MusicGen:** tiene generación de audio condicionada por texto y una variante guiada por melodía; no entrega por ello tus pistas MIDI editables. Su documentación permite estudiar un baseline, pero sus pesos publicados se declaran CC-BY-NC 4.0: no lo elegiría automáticamente como base de producción comercial. [Documentación](https://github.com/facebookresearch/audiocraft/blob/main/docs/MUSICGEN.md), [model card y licencia](https://github.com/facebookresearch/audiocraft/blob/main/model_cards/MUSICGEN_MODEL_CARD.md).

**RAVE:** tiene sentido si buscas explorar un espacio tímbrico o un instrumento neural. Requiere preparar un corpus de audio y escoger configuración; el repositorio distingue configuraciones causales y no causales. No reemplaza el dataset de decisiones armónicas ni garantiza por sí solo un efecto de baja latencia dentro de Live. [Fuente: RAVE](https://github.com/acids-ircam/RAVE).

### Qué significa «local en una Mac M6» para esta decisión

Tomo la Mac M6 como tu plataforma prevista; no supongo RAM, variante de chip ni tiempos de entrenamiento. MPS ejecuta operaciones de PyTorch en GPU. MLX es un framework para Apple silicon con CPU/GPU y memoria unificada; disponer de él no convierte automáticamente un repositorio de audio a MLX ni lo ejecuta en Neural Engine. [PyTorch MPS](https://docs.pytorch.org/docs/stable/notes/mps.html), [Apple MLX](https://github.com/ml-explore/mlx).

Antes de comprometerse con un modelo: fijar checkpoint y versiones; probar carga, inferencia y —si se pretende entrenar— backward y paso de optimizador; medir memoria pico, swap, tiempo por propuesta y convivencia con Live. Registrar fallbacks a CPU y errores de operadores. Realizar el benchmark con duración, batch, sample rate y precisión definidos. Un límite de VRAM publicado para CUDA no se traslada directamente a memoria unificada.

La biblioteca y el ranking pueden avanzar sin esperar ese benchmark. No trasladaría estos modelos generativos al Teensy: esta propuesta pertenece al flujo de producción en Mac/GroovePilot. Los contratos TinyML del Brain y las formulaciones históricas de `01`/`04` necesitan su propia resolución; esta auditoría no los modifica.

## 10. Cómo evitar una evaluación engañosa

**Separar antes de aumentar datos.** Un patrón, sus versiones v2–v4, transposiciones, renders en distintos kits y copias en favoritos deben compartir linaje. Para medir generalización a motivos nuevos, sus derivados no pueden quedar repartidos entre entrenamiento y prueba.

`GroupKFold` y `LeaveOneGroupOut` formalizan la separación por grupos; su aplicación a familias de motivos es una propuesta para este corpus. No basta agrupar por nombre de archivo. Tampoco resuelve todo usar la fecha: en un mismo día hay familias distintas y una misma familia puede reaparecer días después. [Fuente: validación por grupos de scikit-learn](https://scikit-learn.org/stable/modules/cross_validation.html).

Haría dos evaluaciones distintas:

1. **Estabilidad de preferencias:** repetir candidatos conocidos otro día, con contexto recuperado y orden cambiado. Mide cuánto se mantiene el gusto; no generalización a material nuevo.
2. **Utilidad sobre material nuevo:** reservar familias y proyectos no usados para ajustar reglas o modelos. Todas sus derivaciones quedan fuera del entrenamiento. También reservar una sesión posterior para detectar cambios de preferencia.

Con la muestra actual no publicaría una cifra de precisión de un «modelo de tu ADN». Reportaría casos, denominadores y desacuerdos. Una evaluación musical útil compara, con la misma cantidad de opciones:

| Comparación | Medida |
|---|---|
| Carpetas/favoritos frente al nuevo buscador | Tiempo hasta encontrar algo que decides usar |
| Variaciones actuales frente a modelo | Propuestas usadas / propuestas efectivamente escuchadas |
| Elección sin ranking frente a ranking | Si una de las tres primeras sirve, y cuánta edición necesita |
| Preferencia inicial frente a revisión | Repeticiones coincidentes, cambios y motivos |
| Loop frente a resultado de producción | Estudios terminados, transiciones resueltas y versiones recuperables |

Para audio: escucha a niveles comparables, artefactos, conservación de ataques, colas y uniones de loop. Para patrones: rango tocable, silencios, relación kick/bajo, variedad útil y parecido excesivo con el material de entrenamiento. La distancia de embeddings no sustituye tu decisión de usarlo.

## 11. Siguiente ciclo propuesto: cerrar un caso antes de ampliar

No dedicaría la próxima semana a fabricar miles de archivos. Haría un ciclo corto centrado en **acordes**, porque ya hay patrones favoritos, un timbre de referencia y candidatos concretos. Después repetiría el método con leads.

| Paso | Trabajo | Entrega demostrable |
|---|---|---|
| 1. Congelar | Identificar snapshot del set, clips y capas que se van a comparar; verificar qué variantes están realmente cargadas | Se puede recuperar cada candidato sin depender de su nombre |
| 2. Elegir timbre | WOOD / WOOD2 / WOOD3 con un mismo patrón, niveles comparables y backing fijo | Una elección contextual o un empate bien registrado |
| 3. Aislar ritmo | Con el timbre elegido, comparar el mismo voicing en corcheas, tresillo y contratiempo | Saber qué parte de la preferencia viene del ritmo |
| 4. Revisar | Reescuchar una selección breve otro día, cambiando el orden | Separar preferencia estable de primera impresión |
| 5. Usar | Construir un estudio de 60–90 s con bajo, percusión, acordes y transición; usar Electric si aún resuelve mejor el trabajo | Música que permite evaluar la paleta completa |
| 6. Capturar | Guardar MIDI/ediciones, stems, referencia de mezcla y candidatos necesarios | Un pequeño conjunto trazable y audible |

La documentación dice que WOOD2/3 y DARK2/3 fueron generadas pero no subidas por puerto ocupado. No las trataría como variantes escuchadas ni validadas en hardware.

Como presupuesto orientativo, 4–6 sesiones de 20–30 minutos y 1–2 horas para el estudio/captura, ajustable al tiempo real. Empezaría con **20–30 decisiones completas distribuidas en varios días**, incluyendo algunos controles repetidos y casi aciertos. Es una meta de captura manejable, **no un mínimo científico para entrenar**.

Puerta de salida del ciclo: poder recuperar los estímulos, explicar las elecciones, conservar una corrección tuya y escuchar una pieza que los use. Solo después, un experimento de ranking. Para ampliar a un generador se necesita una curva de aprendizaje sobre más familias y proyectos; contar archivos no determina cuándo hay datos suficientes.

## 12. Cambios documentales que propondría aprobar

No apliqué estas modificaciones a los documentos fuente; quedan enumeradas para que la auditoría sea revisable.

| Documento / herramienta | Mejora propuesta |
|---|---|
| `revision-2026-10-28.md` | Separar hechos, palabras de Juan e hipótesis; acotar las reglas de color y armonía |
| `lab-leads.md` | Corregir 4+8 a 4+7; indicar que cambian armonía/preset; retirar lectura causal de la prueba de dosis; aclarar referencias antiguas al kit de disco |
| `lab-acordes.md` | Mantener 9/9 como resultado contextual; distinguir aprobación de pareja lead/acordes; evitar presentar una receta de síntesis como explicación demostrada de lo que no gustó |
| `sesion-paleta-adn.md` | Presentar ADN como perfil evolutivo; separar criterio B de identidad general y datos tocados de humanización programada |
| `percusion-groove.md` / README del dataset | Conciliar 34/37 con 35/37 actuales; sincronizar el estado del 808 y kits por dialecto; preservar procedencia de material adaptado |
| `protocolo-captura.md` | Diferenciar aislado/stem/mix; documentar 44,1 frente a 48 kHz; enlazar la captura con snapshots |
| `gf_session.py` | Diseñar esquema de eventos con empates, ambos útiles, desconocido y exposición; evitar selección de sesión solo por modificación reciente |
| Bibliotecario | Conservar versiones históricas y separar procedencia declarada de permisos verificados; no inferir autoría solo por prefijo del nombre |
| Nueva ficha de dataset | Propósito, contenido, origen, cobertura, limitaciones, exclusiones, linajes y evaluación reservada |

Las mejoras de software requerirían su teoría/spec y validación correspondiente. El resultado de esta auditoría es una propuesta de recolección y uso, no una autorización implícita para cambiar firmware, generadores o archivos de Live.

## 13. Evidencia local y método de comprobación

Fuentes principales, con las versiones efectivamente consultadas:

- [Revisión del día](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/.claude/worktrees/dreamy-goodall-071cc6/apps/docs/produccion/revision-2026-10-28.md).
- [Lab de leads](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/.claude/worktrees/dreamy-goodall-071cc6/apps/docs/produccion/lab-leads.md), [lab de acordes](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/.claude/worktrees/dreamy-goodall-071cc6/apps/docs/produccion/lab-acordes.md) y [sesión ADN actualizada](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/.claude/worktrees/dreamy-goodall-071cc6/apps/docs/produccion/sesion-paleta-adn.md).
- [Ficha real de leads y acordes](/Users/juandavidguerrerocastro/GROOVEFORGE/GrooveForge_Dataset_v1/metadata/sesiones/2026-10-28_lead.json), [bajo](/Users/juandavidguerrerocastro/GROOVEFORGE/GrooveForge_Dataset_v1/metadata/sesiones/2026-10-28_bass.json) y [acordes del check inicial](/Users/juandavidguerrerocastro/GROOVEFORGE/GrooveForge_Dataset_v1/metadata/sesiones/2026-10-28_chords.json); también se revisaron las de percusión, pluck, pad y FX.
- [README del dataset](/Users/juandavidguerrerocastro/GROOVEFORGE/GrooveForge_Dataset_v1/README.md), [kit Tribe](/Users/juandavidguerrerocastro/GROOVEFORGE/GrooveForge_Dataset_v1/kits/tribe_colombia.json) y restantes fichas de kits y patrones.
- [Protocolo de captura](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/apps/docs/produccion/protocolo-captura.md), [percusión y groove](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/apps/docs/produccion/percusion-groove.md), [paleta v1](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/apps/docs/variants/studio/paleta-v1.md) e [IA local: refinamiento](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/apps/docs/produccion/ia-local-refinamiento.md).
- [Generador de leads](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/.claude/worktrees/dreamy-goodall-071cc6/tools/paleta-lab/make_lead_lab.py), [generador de percusión](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/.claude/worktrees/dreamy-goodall-071cc6/tools/paleta-lab/make_perc_patterns.py), [registro de sesiones](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/tools/paleta-lab/gf_session.py) y [bibliotecario](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/tools/preset-librarian/gf_presets.py).
- [Set de acordes](</Users/juandavidguerrerocastro/MUSIC_PRODUCTION/Grooveforge/GFExploration Project/GF_Acordes_Lab.als>) y [set de dialectos con kits](</Users/juandavidguerrerocastro/MUSIC_PRODUCTION/Grooveforge/GFExploration Project/GF_Lead_Dialectos_kits.als>); también se inspeccionaron los otros sets GF de la carpeta principal.
- [Estrategia](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/apps/docs/00-master-strategy.md), [arquitectura](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/apps/docs/01-architecture.md) y [arquitectura IA](/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain/apps/docs/04-ai-architecture.md), como límites de alcance.

Método del inventario: recorrido de archivos visibles del dataset; JSON leídos con el parser estándar; MIDI leídos con `mido`; SHA-256 sobre sus bytes. El contador orientativo de eventos compara mensajes no-meta a posición absoluta normalizada por PPQ, conservando sus valores de canal, nota, velocity y controles. No colapsa transposiciones, equivalencias entre kits ni todas las similitudes musicales: **1.103 tampoco es el número de ideas independientes**. Las cabeceras WAV se leyeron con `wave`; los `.als`, como XML descomprimido, sin abrir ni alterar Live.

Las recomendaciones de modelos se contrastaron con documentación primaria enlazada junto a cada afirmación. No se descargaron pesos, no se enviaron tus audios a servicios externos y no se modificó el dataset.
