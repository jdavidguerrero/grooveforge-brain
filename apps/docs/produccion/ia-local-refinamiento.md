# Producción e identidad con IA local — refinamiento

Fecha: 2026-10-27. Estado: propuesta para Juan; no cambia los specs 00–06 ni autoriza implementar nuevos modelos.

Base: documento adjunto «Arquitectura de Producción Generativa & Ecosistema Hardware», `caracter-sonoro.md`, firmware y documentación Studio disponibles. Juan aclaró que es productor principiante, ya dispone de GroovePilot y su prioridad es **encontrar su sonido y construir una marca**. Por eso el primer resultado buscado es una paleta reconocible y música terminada que la ponga a prueba. El volumen de publicaciones es una etapa posterior.

## 1. Dictamen y objetivo reformulado

La separación entre ordenador, asistente y hardware es razonable. El catálogo original reúne varias investigaciones independientes, con tiempos y resultados que todavía no se han demostrado. Construirlas todas antes de hacer música retrasaría la búsqueda de identidad.

**Objetivo propuesto:** ayudar a Juan a descubrir, registrar y reutilizar sus decisiones musicales, manteniendo la interpretación y la selección bajo su control. La IA busca, compara, explica y propone variaciones acotadas; el hardware aporta interpretación y material propio; Ableton conserva una sesión editable y recuperable.

Primera entrega: tres estudios musicales breves, una paleta inicial de sonidos preferidos y una ficha de decisiones que explique qué conecta los tres. No prometer un sonido definitivo ni un track publicable en dos horas.

No hace falta entrenar una red para empezar. Un catálogo pequeño, etiquetas, MIDI editable, presets y registro de preferencias permiten comprobar primero qué ayuda realmente.

## 2. Identidad: un núcleo y varias direcciones

Los géneros indicados por Juan son organic, melodic y progressive house, disco house, indie tech y house. El manifiesto original se inclina más hacia disco/funk/afro y deja menos desarrollados el arco melódico y la progresión a largo plazo.

Propuesta creativa para probar, **no descripción universal de esos géneros**:

| Dirección | Explorar | Mantener como hilo común |
|---|---|---|
| Organic | Percusión interpretada, madera/FM, pequeñas respuestas rítmicas, espacio | Ataque reconocible, grave legible, contraste entre gesto humano y pulso estable |
| Melodic / progressive | Motivo breve que evoluciona, tensión armónica, cambios de densidad y registro | Un motivo principal claro y automatización con intención |
| Disco / house | Bajo articulado, stabs, llamadas y respuestas, repetición eficaz | Groove legible y relación kick/bajo deliberada |
| Indie tech | Texturas extrañas, FM/ring mod, silencios, acentos y contraste tímbrico | Un elemento inesperado sin perder el centro rítmico |

La identidad puede residir en tres hábitos: cómo articulas el bajo, cómo responden melodía y percusión, y cómo transformas un timbre durante una frase. No necesita que todos los tracks lleven el mismo preset o efecto.

### Cambios al manifiesto

- **122–126 BPM:** conservar como zona inicial opcional, no filtro que descarte ideas de los demás estilos.
- **Microtiming de 5–15 ms:** sustituir el desplazamiento indiscriminado por patrones repetibles por instrumento y posición. Mantener una versión cuantizada como comparación. Un pulso estable puede convivir con percusión expresiva.
- **Pitch drift, cinta y vinilo:** recursos opcionales por sonido. La limpieza y el silencio también forman parte del carácter.
- **Referentes:** describir qué interesa de cada fragmento — articulación, espacio, tensión, densidad — sin convertir nombres de artistas en el criterio automático de calidad.
- **Exploración:** reservar una parte de las propuestas para alternativas fuera de la preferencia actual; de otro modo el asistente puede estrechar prematuramente el gusto.

## 3. Primer flujo: laboratorio de identidad

### Sesión A: elegir con el oído

Seleccionar un conjunto pequeño de fragmentos de referencia, repartido entre las direcciones anteriores. Anotar por fragmento un atributo que gusta y otro que no se quiere trasladar. No entrenar todavía con esas grabaciones.

GroovePilot organiza las observaciones y convierte palabras vagas en comparaciones: «más cálido» puede significar menos agudo, ataque más suave o saturación; presentar dos alternativas ayuda a descubrir cuál quería Juan.

### Sesión B: crear la paleta propia

Elegir alrededor de diez sonidos de los presets y grabaciones disponibles: dos bajos, dos acordes/stabs, dos plucks, dos percusiones tonales, un pad y una textura. Es una cantidad de trabajo propuesta, no el mínimo de un dataset de ML.

Para cada uno guardar preset/versión, MIDI de prueba, audio seco y procesado, nivel de captura y una descripción propia. Comparar dentro del mismo loop, a niveles de escucha semejantes. Conservar originales sin normalizar para no perder información de dinámica.

No esperar a terminar el Mixer, E-Drum o los modelos locales: usar los instrumentos disponibles y sustituir temporalmente lo que falte dentro de Live.

### Sesión C: tres estudios y una elección

Hacer tres estudios de 60–90 segundos con parte de la misma paleta:

1. Organic/melodic: pluck o percusión tonal que establece un motivo y luego lo desarrolla.
2. Disco/house: bajo y stab que conversan, con un cambio perceptible de energía.
3. Indie/progressive: textura con movimiento, tensión y una resolución.

Reutilizar timbres permite comprobar si hay carácter común más allá del género. Completar al menos una transición y un cierre en cada estudio; no quedarse solo con tres loops.

Elegir uno para llevar a track completo. Registrar qué se conserva, qué se descarta y por qué. El asistente debe mostrar pocas alternativas por decisión — por ejemplo tres — para evitar una nueva forma de bloqueo por exceso de opciones.

### Papel de la IA en esta etapa

Buscar entre recursos disponibles, sugerir pequeñas variaciones MIDI, explicar decisiones, comparar versiones y mantener la ficha de identidad. Cada propuesta debe indicar qué cambió y permitir recuperar la anterior. Una función de «por qué podría encajar» ayuda a aprender; una puntuación automática de «suena a Juan» aún no tiene fundamento.

## 4. Arquitectura refinada

| Componente | Responsabilidad | Condición de funcionamiento |
|---|---|---|
| Ableton Live | Transporte maestro en estudio, clips, grabación, arreglo, automatización y renders | Sigue reproduciendo aunque se cierre el asistente o se pierda internet |
| Servicio local en Mac | Catálogo, búsqueda, análisis, cola de renders, propuestas MIDI y adaptador de control | Trabajos pesados pausables; modo grabación limita la carga; medir memoria real con la sesión abierta |
| GroovePilot | Intención, contexto, explicación y selección asistida | Reutilizar lo que ya existe. Nube opcional para funciones cognitivas; no gobierna eventos de audio muestra a muestra |
| Brain y controladores | Interpretación, síntesis, macros y ejecución de eventos | DSP predecible; nuevos modelos solo tras pruebas de coste y beneficio audible |
| Mixer / interfaz | Ruteo, procesamiento externo, escucha y captura | Documentar canales, conversión y compensación de latencia; conservar tomas secas cuando sea posible |

### Separar tres tiempos

- **Interpretación:** deadline de audio medido extremo a extremo, con jitter y carga máxima. No usar «latencia cero».
- **Interacción:** búsqueda, vista previa y variación corta; evaluar tiempo de respuesta típico y p95 en el equipo real.
- **Preparación:** entrenamiento, indexación y generación de audio por lotes; fuera de la grabación cuando compitan por recursos.

128 muestras / 44.100 Hz = **2,90 ms por bloque**: es un periodo de procesamiento, no un máximo de latencia de todo el camino MIDI/audio/USB/codec/Live. La memoria libre no puede fijarse en 20 GB solo por tener 32 GB instalados.

El Mac mini M6 aparece en el [anuncio oficial de Apple](https://www.apple.com/newsroom/2026/08/apple-unveils-a-more-powerful-mac-mini-featuring-the-all-new-m6-and-m5-pro/); eso no valida el rendimiento de los modelos propuestos. [PyTorch MPS](https://docs.pytorch.org/docs/2.14/notes/mps.html) usa GPU; no equivale a ejecutar automáticamente en Neural Engine. Elegir backend por modelo y benchmark, no por la presencia del acelerador en la ficha del equipo.

### Control editable y recuperable

Propuesta JSON → validación de parámetros y capacidades → vista previa → aplicar sobre copia/versión → registro y deshacer. Usar IDs estables de pistas y assets, schema versionado y detección de cambios del proyecto. No escribir simultáneamente desde varios controladores sobre el mismo parámetro sin una regla de prioridad.

En el repositorio actual existen CC, protocolo bridge y bibliotecario de presets. La búsqueda en firmware no mostró un receptor SysEx de patches implementado. Definir ese contrato o reutilizar el bibliotecario existente antes de prometer «CLAP-to-SysEx». La existencia de USB MIDI no implica transferencia arbitraria de presets.

## 5. Auditoría de las herramientas propuestas

| Propuesta original | Evaluación | Refinamiento |
|---|---|---|
| Buscador CLAP + base local | Útil cuando buscar ya consume tiempo | Empezar con etiquetas y audiciones; añadir embeddings al crecer el catálogo. Combinar similitud semántica con rol, duración, tonalidad/BPM cuando sean fiables y procedencia |
| CLAP + MLP → parámetros exactos en <50 ms | No demostrado; timbres similares admiten múltiples configuraciones | Recuperar presets renderizados, presentar candidatos y ajustar después. Un inversor necesitaría pares audio–patch, representación por engine y evaluación fuera del conjunto de entrenamiento |
| Hyper-macro XY con 50 presets | Exploración plausible, sin garantía de espacio continuo útil | Probar primero interpolaciones acotadas y las macros actuales. No interpolar índices de algoritmos, formas o toggles como si fueran controles continuos; respetar escalas musicales y límites |
| GRU como filtro/saturación en Teensy | Investigación independiente | Capturas entrada/salida alineadas, controles de cutoff/resonancia si varían, entrenamiento, prueba de estabilidad y benchmark con voces/FX. Una GRU de audio no se integra por declarar «RTNeural + CMSIS-NN» |
| CNN del E-Drum con 2–3 ms | Hipótesis a comprobar con el sensor real | Comparar primero umbral/peak/slope y sensores disponibles. Separar detección de golpe de clasificación tímbrica; considerar crosstalk y ejecución de varias personas. Esa ventana ya consume parte del presupuesto de respuesta |
| Stable Audio + LoRA | Experimento posterior | Comprobar modelo, versión, receta de adaptación y backend. No asumir generación a BPM exacto, loop sin costura, ni ajuste barato en Mac. Evaluar propuestas antes de entrenar |
| RAVE / DDSP / nn~ | Puede aportar un instrumento experimental | Son tecnologías distintas; escoger un modelo y medir su comportamiento. Conversión en tiempo real no garantiza conservación de notas/transitorios ni latencia imperceptible |
| Transformer MIDI propio | Prematuro si no hay corpus personal suficiente | Primero variaciones por reglas sobre MIDI tocado por Juan; registrar elecciones. Después decidir si un modelo añade valor frente a ese baseline |
| allin1 → arreglo | Buena ayuda para segmentación | Usar estimación de beats/secciones como entrada a un editor de estructura. No atribuirle detección directa de todas las entradas instrumentales, automatizaciones o tensión |
| AbletonOSC → track completo | Integración posible, cobertura a verificar | Probar creación/duplicación de clips, locators y automatizaciones por separado en la versión de Live. Usar Live Object Model/Max for Live donde corresponda; no asumir que todas las operaciones están expuestas en OSC |

[CLAP](https://github.com/LAION-AI/CLAP) proporciona embeddings de audio/texto; la conversión inversa al sintetizador sería desarrollo propio. [allin1](https://github.com/mir-aidj/all-in-one) devuelve tempo, beats, downbeats y segmentos etiquetados; las decisiones de producción requieren análisis adicional o intervención humana.

El [Live Object Model](https://docs.cycling74.com/apiref/lom/track/) documenta duplicación de clips hacia Arrangement; el adaptador [AbletonOSC](https://github.com/ideoforms/AbletonOSC) debe verificarse operación por operación. No confundir soporte de Live con soporte del wrapper.

[MidiTok REMI](https://miditok.readthedocs.io/en/latest/tokenizations.html) discretiza posiciones según `beat_res`. Para aprender microtiming deben conservarse los tiempos originales y una resolución adecuada o residuos de timing explícitos; el tokenizer no es por sí mismo un modelo de infilling ni una implementación MLX.

[RAVE](https://github.com/acids-ircam/RAVE) documenta variantes causales/no causales y distintas necesidades de memoria. [nn~](https://github.com/acids-ircam/nn_tilde) permite inferencia en Max/Pd; no convierte cualquier DDSP en un efecto listo para producción.

[RTNeural](https://github.com/jatinchowdhury18/RTNeural) y [CMSIS-NN](https://github.com/ARM-software/CMSIS-NN) son bibliotecas con operadores y backends que hay que comprobar. Para un MLP 2→16→32→40 con bias hay 1.912 parámetros: **7.648 bytes en float32 solo en pesos/bias**. Activaciones, estado, código y runtime se presupuestan aparte; tener 50 presets no garantiza aprender una superficie de 40 dimensiones musicalmente útil.

[PJRC](https://www.pjrc.com/store/teensy41.html) distingue RAM, memoria externa y ubicación del código: FASTRUN corresponde a funciones/ITCM, no a una clase genérica de RAM para pesos. La PSRAM es expansión opcional, no memoria rápida garantizada instalada. Cargar modelos desde SD se hace fuera del callback de audio. La inferencia de control va fuera del callback; solo un modelo que procese audio y cumpla su deadline justificaría ejecutarse dentro.

Para audio generativo, congelar el modelo y su licencia concreta: [Stable Audio Open 1.0](https://huggingface.co/stabilityai/stable-audio-open-1.0) es una opción existente; hay también una [familia posterior de Stable Audio](https://stability.ai/news-updates/meet-stable-audio-3-the-model-family-built-for-artistic-experimentation-with-open-weight-models). La selección no queda decidida aquí ni se presume compatibilidad MPS/MLX del entrenamiento. [stable-audio-tools](https://github.com/Stability-AI/stable-audio-tools) sirve para evaluar la ruta soportada.

## 6. Dataset: memoria de decisiones antes que volumen de archivos

```text
GrooveForge_Dataset_v1/
  originals/          # tomas propias inalteradas
  patches/           # preset + engine/firmware/schema
  midi/              # interpretación original y variaciones
  renders/           # seco/procesado, protocolo de captura
  metadata/          # manifest de assets y preferencias
  studies/           # estudios musicales y sesiones recuperables
  references/        # análisis/observaciones, separado de entrenamiento
  exports/           # versiones elegidas para evaluación/publicación
```

Campos por asset: ID/hash, creador/origen y permiso de uso, rol, engine y versión, preset, MIDI asociado, BPM, tonalidad estimada y confianza, sample rate, afinación, notas/velocities capturadas, FX, ganancia, fecha, etiquetas propias y valoración de Juan. Separar uso para referencia, entrenamiento y publicación; «percusión favorita» o sample procesado no describe sus permisos. La [guía de Stability](https://kb.stability.ai/knowledge-base/stable-audio-commerical-and-usage-licensing) remite a la licencia del modelo concreto para uso local.

Guardar decisiones pareadas: A/B, preferida, contexto y motivo — «B deja espacio al kick», «A tiene demasiado ataque». No solo estrellas. Retener ejemplos rechazados para evitar repetir errores.

Si más adelante hay entrenamiento: separar evaluación por sesión/patch/proyecto, no repartir cortes casi idénticos entre train y test. Comparar siempre contra búsqueda o reglas simples.

El corpus de un principiante empieza pequeño. No presupone proyectos históricos suficientes ni requiere fabricar miles de ejemplos antes de tocar y elegir.

## 7. De identidad a publicación

La oportunidad es aumentar la proporción de ideas que terminan en música elegida. Generar más archivos sin mejorar la selección no demuestra progreso.

Flujo propuesto: estudio breve → elección → arreglo → interpretación/automatización → mezcla → revisión en otra sesión → exportación y publicación elegida por Juan.

Agregar un asistente de terminación con:

- Lista corta de decisiones pendientes por track y siguiente acción concreta.
- Comparación de versiones a nivel semejante, escucha mono y comprobación de graves/transitorios/colas.
- Estado de renders, stems, proyecto consolidado, créditos/procedencia y copias recuperables.
- Captura de MIDI/CC, presets y audio seco/procesado de hardware cuando la ruta lo permita. Una toma analógica no permite reconstruir ajustes si el mixer no los registra.
- Motivo distintivo y evolución del track: una plantilla de seis minutos no crea por sí sola desarrollo musical.

La marca puede empezar con una serie pequeña y coherente de piezas y procesos: explicar qué se tocó, qué aportó el hardware y qué decisiones se repiten. La cantidad de lanzamientos se decide tras comprobar que se mantienen el criterio y la calidad, sin automatizar la publicación.

**Corrección de duración:** en 4/4 a 124 BPM, 128 compases duran aproximadamente **4:07,7**. Seis minutos son 186 compases; 192 compases dan aproximadamente **6:11,6**. Elegir duración según el arreglo y su uso, no un número fijo.

«Track en dos horas» puede convertirse en un reto para obtener un **borrador arreglado** con una plantilla y paleta preparadas. No incluye automáticamente captura de dataset, entrenamiento, búsqueda de identidad, mezcla final ni preparación de lanzamiento.

## 8. Orden de trabajo y pruebas de valor

| Etapa | Entrega | Cómo decidir si continuar |
|---|---|---|
| 1. Identidad piloto | Paleta inicial, tres estudios y ficha de preferencias | Juan identifica qué conservar y elige uno para desarrollar |
| 2. Biblioteca asistida | Búsqueda por rol/descripción y audición de candidatos propios | Encuentra material útil con menos fricción que las carpetas actuales |
| 3. Variaciones y estructura | Tres variaciones editables; dos propuestas de arreglo sobre una copia | Se aprovecha alguna propuesta y Juan entiende/modifica sus decisiones |
| 4. Terminación | Un track completo con proyecto y renders recuperables | Puede revisarse y cerrarse sin perder versiones ni decisiones |
| 5. ML personalizado | Un experimento elegido por el cuello de botella observado | Supera el baseline simple en utilidad y coste; no solo en novedad |

El entrenamiento local, la saturación neuronal y la CNN del E-Drum no son dependencias de las cuatro primeras etapas.

Durante las dos semanas previas al ordenador, priorizar tocar/grabar, elegir y etiquetar la paleta, producir los estudios y documentar el flujo actual. No prometer que todos los modelos estarán entrenando al conectar el Mac.

Medir: tiempo hasta una idea que Juan quiere guardar; propuestas que realmente usa; estudios que llegan a arreglo; capacidad de recuperar una sesión; preferencia en escucha repetida. Las latencias, RAM y CPU se miden cuando exista el prototipo correspondiente.

## 9. Pendientes para una implementación futura

Confirmar versión/edición de Live y Max for Live, capacidades actuales de GroovePilot, equipo realmente disponible, interfaz y canales de entrada/salida, protocolo de presets, material propio utilizable y tiempo de sesiones musicales. Son datos para dimensionar la integración; no bloquean comenzar la paleta y los estudios.

El repo conserva formulaciones históricas diferentes sobre la ubicación de la IA entre `01` y `04`. Este refinamiento propone una ruta de estudio local con nube opcional; no resuelve unilateralmente esos contratos ni convierte nuevas funciones TinyML en requisitos del Brain.

---

## Anexo (2026-10-27): cómo encaja con lo que ya existe

| Este documento pide | Ya existe en el repo |
|---|---|
| Sesión B: paleta de ~10 sonidos con ficha | `variants/studio/paleta-v1.md` (roles, loop de prueba, sesión por rol) |
| `patches/` con preset + engine + versión | Bibliotecario (`tools/preset-librarian`, theory/11 de Studio): un JSON por sonido con formato, engine, parámetros y ficha |
| Transferir presets sin prometer SysEx | El bibliotecario lo hace para el Brain por el USB serie (`@GF EXPORT / PUT`); no hay receptor SysEx |
| Material tímbrico para los estudios | Presets JD, pilotos GF, mitades HY y capas híbridas (theory/12 de Studio) |
| Protocolo de captura | `protocolo-captura.md` (este directorio) |

Decisión de carpetas: la librería del bibliotecario vive dentro del dataset (`GrooveForge_Dataset_v1/patches/brain/<engine>/`),
para no tener dos lugares con los mismos sonidos.
