# ADR-006: Gemelo en software del Brain (núcleo DSP compartido, renderizador y plugin AU/VST3)

**Status:** Accepted para las fases 1–2 en una **rama dedicada**. Juan retomó el plugin con **JUCE** y aprobó el spec inicial para iniciar implementación (2026-10-29; ver §Revisión vigente). La decisión de producto (contradicciones 1–2) sigue abierta.
**Date:** 2026-10-29
**Deciders:** Juan Guerrero (owner)
**Relacionados:** ADR-003 (el Brain v1 usa el filtro digital `MoogLadder4P`), ADR-005 (DX7 de 4 operadores con objeto
propio), `produccion/motor-asistencia.md`, `produccion/auditoria-datasets-adn-2026-10-28.md` §2 y §8
**Afecta specs (sin modificarlos; ver §Contradicciones):** `00-master-strategy.md` §4 (Nivel 4) y §3 (precio y
posicionamiento), `01-architecture.md` (estructura del código de engines), `04-ai-architecture.md` (datos de audio),
`06-implementation-roadmap.md`

---

## Context

**El problema de producción.** El Brain toca **un sonido a la vez**: un engine o una capa de dos. Un track tiene casi
todos sus instrumentos en el Brain, así que:

- para oír varias partes juntas hay que grabar cada una como audio; el estudio de acordes del 2026-10-29 obligó a
  grabar acordes, bajo, pluck y lead uno por uno solo para poder decidir la siguiente parte;
- Live no puede congelar (Freeze) un instrumento de hardware, porque el freeze renderiza fuera de tiempo real;
- cambiar de idea después de grabar es volver a grabar.

**El problema del dataset.** La auditoría de datasets encontró **0 archivos de audio** en `GrooveForge_Dataset_v1` y
seis tomas sueltas a 44,1 kHz. El protocolo prevé una sesión de grabación final, en tiempo real, preset por preset.
Para un buscador por audio o para comparar timbres se necesitan muchos renders consistentes:
`preset × MIDI de prueba × contexto`. A mano no escala.

**Lo que ya existe y lo hace posible:**

| Hecho | Dónde |
|---|---|
| Los engines y las etapas de audio son **C++ propio** (PolyBLEP, `MoogLadder4P`, envolventes RC, chorus BBD, FM de 4 operadores) | `src/engines/`, `src/audio/` |
| Los núcleos del Prophet y del DX7 **ya compilan y se prueban en el Mac** (`pio test -e native`, 154 tests) | `prophet_osc_core`, `dx7_core` |
| No hay intrínsecos ARM (CMSIS / `arm_math`) en engines ni etapas | búsqueda en `src/engines`, `src/audio` |
| Los presets y capas tienen **formato binario versionado** (GFP v5, GFL v2) y un bibliotecario en Python | `gf_presets.py`, `preset_bank`, `layer_bank` |
| El filtro analógico 2N3904 es un **add-on externo** (ADR-003): el sonido del Brain v1 es enteramente digital | ADR-003 |

**Lo que lo hace difícil:**

- Los engines heredan de `AudioStream` (Teensy Audio Library): **6 de 9 headers** de engines, y el grafo vive en
  `audio_graph.cpp`. El audio se procesa en bloques fijos de 128 muestras.
- **14 archivos** usan la frecuencia de muestreo de la Teensy (`AUDIO_SAMPLE_RATE_EXACT` ≈ 44 117,6 Hz) como
  constante. Un plugin corre a 44,1 / 48 / 96 kHz y con bloques variables.
- Algunos objetos del grafo son de la librería oficial (mezcladores, envolventes, ruido), no código propio.
- Entre el código y el oído están el **codec SGTL5000** y la etapa de salida analógica. El mismo código no garantiza el
  mismo sonido en la salida física.

## Decision (propuesta)

Construir un **gemelo en software** del Brain **a partir del mismo código**, en cuatro fases con su puerta. Cada fase
sirve por sí sola, y la primera ni siquiera tiene interfaz.

1. **Núcleo DSP compartido (`libs/gf-dsp/`, C++17, sin Arduino).**
   - Se extrae de los engines la parte que calcula muestras.
   - La frecuencia de muestreo pasa a ser un **parámetro** (`prepare(sample_rate, max_block)`), no una constante.
   - Los objetos de la librería oficial que use el sonido se reemplazan por equivalentes propios, o se aíslan detrás de
     una interfaz.
   - En el firmware, cada engine queda como un **adaptador delgado** `AudioStream` → núcleo. **Una sola fuente de
     DSP**: el firmware y el gemelo no pueden divergir.
2. **Renderizador sin interfaz (`gf-render`, CLI en el Mac).** Entra un preset o capa (GFP/GFL) + MIDI; sale un WAV a
   **48 kHz / 24 bit** + una ficha con hashes.
   - Sirve para el dataset desde el primer día: renders por lote con su procedencia.
   - Cierra el hueco de audio de la auditoría **sin** grabar en tiempo real.
3. **Plugin AU / VST3, uso personal.**
   - Carga los mismos presets y capas, con macros (DRIFT, GRIT, POCKET, MORPH, MOVE) y CC 18 para la capa.
   - Una instancia por pista, con Freeze, automatización y mezcla normales.
   - El Brain físico queda para tocar, el vivo y lo que no se emula (panel, add-on analógico).
4. **Validación de fidelidad contra el hardware.** Con las tomas de la sesión de grabación final, el mismo preset y el
   mismo MIDI, grabado del Brain y renderizado, se comparan:
   - LUFS;
   - envolvente (ataque / decay medidos);
   - espectro promedio por bandas;
   - una prueba a ciegas de Juan (A/B/X).

   **No** se usa un null test: el codec, la salida analógica y el ruido/drift lo impiden por diseño. Umbrales a definir
   en el theory doc de la fase 4.

**La decisión comercial queda fuera de este ADR.** Si el gemelo se vende, se regala con el Brain, se usa solo
internamente o compite con el hardware lo decide `00` (§Contradicciones). Este ADR propone el gemelo como
**herramienta propia de producción y de datos**, en línea con las prioridades de 2026-10: comunidad, crecer como
productor, herramienta local personal.

## Options Considered

| Opción | Veredicto | Por qué |
|---|---|---|
| A. Grabar todo del hardware (flujo actual, mejorado: sonido en el clip con CC 18 + una pasada por pista al final) | ✅ **ya, mientras tanto** | Cero desarrollo. No escala para el dataset ni para cambiar de idea después |
| B. Modo multitímbrico en el firmware (un canal MIDI por engine) | 🟡 otro ADR | Sirve para el vivo. El audio sale mezclado en un estéreo, así que para mezclar igual hay que grabar. Cuesta CPU (≤60 %) |
| **C. Gemelo desde el mismo código (este ADR)** | ✅ propuesta | Resuelve producción **y** dataset. Una sola fuente de DSP |
| D. Reescribir los engines en el plugin, aparte del firmware | ❌ | Dos implementaciones que divergen; el dataset dejaría de representar al Brain |
| E. Plugin "envoltorio" que manda MIDI al Brain y devuelve audio (como el External Instrument) | ❌ | Es lo que ya hace Live; no resuelve monotimbría ni freeze |

**Framework del plugin (a decidir en la fase 3, no ahora):**

| | JUCE | iPlug2 | CLAP + clap-wrapper |
|---|---|---|---|
| Formatos | AU, VST3, AAX, CLAP (según versión) | AU, VST3, CLAP, web | CLAP nativo, envoltorios a VST3 / AU |
| Licencia | Revisar la vigente antes de elegir: ha cambiado entre versiones y depende del uso comercial | Permisiva (zlib-like) | Permisiva (MIT) |
| Comunidad / documentación | La más grande | Menor | Creciendo |

## Trade-off Analysis

- **Costo:** la fase 1 es la más pesada y la menos visible, porque es refactor de todos los engines y etapas. Es
  también la que más protege: sin ella, la fase 2 no representa al Brain. Estimación honesta: **semanas**, no días,
  dentro de las 10–18 h/semana (`CLAUDE.md`).
- **Riesgo para el firmware:** tocar los engines puede cambiar el sonido o el CPU del Brain. Mitigación:
  - **pruebas golden**: render del núcleo nativo antes y después del refactor, con tolerancia numérica;
  - medición de CPU on-device en cada engine migrado;
  - los 154 tests nativos siguen pasando.
- **Frecuencia de muestreo:** el Brain corre a ≈ 44 117,6 Hz y el gemelo a 48 kHz. Los coeficientes que dependen de la
  frecuencia (filtros, envolventes en ms, LFO, chorus) deben calcularse con el parámetro. Si se derivó algo en
  muestras fijas, el gemelo sonará distinto hasta corregirlo.
- **Aleatoriedad:** drift, ruido y el hiss del chorus llevan semilla en el renderizador, para que un render sea
  reproducible y su hash signifique algo (auditoría §5 P0).
- **Fidelidad percibida:** el SGTL5000 y la salida analógica colorean poco, pero algo. El gemelo es el **Brain digital
  ideal**. La fase 4 mide cuánto se parecen y la etiqueta del dataset lo dice (`source: render` vs `source: hardware`).
- **Producto:** un gemelo vendible puede **canibalizar** el hardware de $599, o puede venderlo (probar antes de
  comprar, uso en el estudio con el hardware para el vivo). No se resuelve aquí.

## Contradicciones con los specs (para que Juan decida; este ADR no las resuelve)

1. **`00` §4 (North Star, Nivel 4 "+ DAW") dice "GroovePilot VST3: AI co-pilot".** Un plugin con los **sonidos** del
   Brain no está en ningún nivel. Según el filtro de §4.2, o se agrega a un nivel (por ejemplo, ampliar el Nivel 4 a
   "DAW: co-pilot + gemelo de sonido") o se declara **herramienta interna**, fuera del producto. Además, desde 2026-10
   GroovePilot dejó de ser prioridad (memoria de prioridades), y eso ya tensiona el Nivel 4 tal como está escrito.
2. **`00` §3 / pitch:** el argumento es "no pagues $2000–3000 por los reissues; seis arquitecturas por $599" en
   hardware. Un plugin con las mismas seis arquitecturas cambia ese argumento, para bien o para mal.
3. **`01`:** los engines viven en `apps/firmware-teensy/src/engines/`. Un `libs/gf-dsp/` compartido cambia la
   estructura de código del contrato técnico. Requiere actualizar `01` con autorización.
4. **`04` §1.2:** el dataset y el entrenamiento están pensados alrededor del Brain y de TinyML. Los renders del gemelo
   son una fuente nueva de audio que `04` no contempla.

## Consequences

**Si se acepta:**
- Se escribe un theory doc por fase (`apps/docs/theory/`), primero el del núcleo DSP: qué depende de la frecuencia de
  muestreo, qué objetos oficiales hay que reemplazar y cómo se prueba la equivalencia.
- `make_audicion.py` podrá renderizar las opciones en vez de pedir a Juan que grabe cada parte. El motor de asistencia
  gana su "preparar una audición" completo.
- El protocolo de captura gana una fuente `render` con hash. La sesión de grabación final pasa a servir para validar
  fidelidad (fase 4), no para producir todo el audio.

**Si no se acepta:** se sigue con la opción A (sonido en el clip + una pasada por pista al final), y el audio del
dataset depende de sesiones de grabación.

## Decisión de Juan (2026-10-29)

- **Fases 1–2: adelante, en una rama dedicada** que sale de `feat/studio-engines-10x` y se compila desde ahí. En esa
  rama se pueden aplicar los cambios (incluido actualizar `01` cuando el piloto pase su puerta). Se integra a la rama
  principal solo cuando Juan lo apruebe.
- **Producto (contradicciones 1–2): sin decidir.** Mientras tanto el gemelo es herramienta interna. `00` no se toca.
- Juan cree que hay mucho reutilizable (núcleos nativos, tests, bibliotecario): la fase 1 empieza por inventariarlo.

## Revisión de Juan (2026-10-29, tarde): shim, sin plugin por ahora, y grabadora automática

**Qué cambió y por qué.**

- **Fase 1 con un *shim* nativo de la Teensy Audio Library, no con el refactor.** El shim es una Audio Library mínima
  para el Mac (`AudioStream`, `AudioConnection`, `AudioMixer4` con la aritmética de PJRC y un `millis()` virtual). Con
  él, el código del firmware compila **sin cambios**: una prueba del 2026-10-29 renderizó el Prophet-5 actual de forma
  reproducible (mismo sha256 en dos corridas). Para renderizar los 6 engines con FX son ~20–28 h, contra ~45–65 h del
  refactor, y **el firmware no se toca**: el sonido y el CPU del Brain no cambian.
- **El plugin AU/VST3 (fase 3) queda en pausa.** Juan: "un plugin perdería la esencia". Tocar es del Brain; el gemelo
  solo **imprime** (como el Freeze de Live) y renderiza por lote. El refactor a `libs/gf-dsp/` con
  `prepare(sample_rate, max_block)` se pospone hasta que se decida si hay plugin: es lo que un plugin necesitaría
  (frecuencia y bloque variables, varias instancias por proceso), y el shim no lo da.
- **Se agrega la opción A automatizada: grabadora automática del hardware.** Un script carga el preset en el Brain, le
  manda el clip MIDI con el clock y graba la salida analógica por la interfaz, sin que Juan esté. Conserva el 100 % de
  la esencia (codec, salida analógica). Va en tiempo real, pero sin atención.

**Cómo se reparten:**

| Herramienta | Para qué | `source` en el dataset |
|---|---|---|
| `gf-render` (shim) | Componer: oír opciones de una parte contra el resto en segundos; renders por lote para el dataset | `render` |
| Grabadora automática | Lo definitivo: las tomas finales de un track y la sesión de grabación de la paleta (`protocolo-captura.md` §3) | `hardware` |

Las tomas de la grabadora son también la referencia de la fase 4 (fidelidad render ↔ hardware), así que esa
validación sale casi gratis.

**Hallazgos que corrigen este ADR (para que Juan decida si se corrige el texto de arriba):**

1. El Brain corre a **44 100 Hz**, no a ≈ 44 117,6 Hz. `AUDIO_SAMPLE_RATE_EXACT` no está definido en `platformio.ini`,
   así que vale 44100.0f, y en la Teensy 4.x el PLL4 fraccional lo sigue (`output_i2s.cpp`, `config_i2s`). Los
   44 117,6 Hz son de la Teensy 3.x.
2. En `engines/` solo `Prophet5Osc` y `Dx7Voice` heredan de `AudioStream`. La constante aparece en código en 13
   archivos, no en 14.
3. `src/dsp/` ya tiene 20 núcleos puros con la frecuencia como argumento.
4. El sonido de un preset depende de ~600 líneas de pegamento del sketch 49 (preset → engine, macros) y de la cadena
   SFX. `gf-render` necesita extraerlas.

**Contradicción 3 (`01`, estructura del código):** sigue vigente, pero en otra forma. Ya no es `libs/gf-dsp/`: es el shim
(`libs/teensy-audio-shim/`) y un env nativo `render`. Se actualiza `01` cuando `gf-render` pase su puerta.

Detalle técnico: `theory/gemelo-fase1-nucleo-dsp.md` y `produccion/grabadora-automatica.md`.

## Revisión vigente de Juan (2026-10-29): retomar plugin con JUCE y GUI de escritorio

**Fuente:** instrucción posterior en el chat: «usemos JUCE», adaptar la organización de la GUI actual a algo más
manejable «tipo Serum», usar la rama creada y empezar con theory y spec.

- Se retoma la dirección de plugin VST3 para SYNTH y HYBRID, con JUCE confirmado como framework. Esta instrucción
  sustituye la pausa de la revisión anterior; no borra su historial ni autoriza por sí sola venta/distribución.
- Juan aprobó el [theory](../theory/gemelo-plugin-juce.md) y el
  [spec v0.1](../specs/gemelo-plugin-v1.md) para empezar. El layout concreto requiere revisión visual posterior.
  No se declaran cumplidos los hitos por aprobar la documentación.
- El destino propuesto vuelve a ser núcleo compartido por instancia, frecuencia parametrizable y wrapper JUCE.
  El shim sirve de harness de referencia cuando se incorpore/verifique; su prueba scratch anterior no constituye
  código integrado ni golden de CI. La grabadora automática conserva su documento como herramienta separada.
- **Corrección confirmada:** la configuración nominal del Brain con Teensy 4.1 es **44.100 Hz** (`teensy4/AudioStream.h`,
  sin override en PlatformIO). Los 44.117,6 Hz del contexto original corresponden a Teensy 3.x. Medir el reloj físico
  sigue siendo distinto de verificar la configuración.
- Serum se usa como referencia de organización: módulos visibles, edición directa y navegación de escritorio.
  La identidad, parámetros y sonido siguen siendo GrooveForge. No se añade una arquitectura wavetable ni matriz nueva.
- Las contradicciones de producto con `00` y la procedencia de datos de `04` siguen abiertas; `00–06` no se modifican
  en esta entrega. La autorización anterior de integrar a principal solo con aprobación de Juan sigue vigente.

### Próximas acciones vigentes

1. [x] Theory y spec iniciales en `feat/gemelo-dsp`; registrar frecuencia correcta y elección JUCE.
2. [ ] Revisar el detalle del spec y fijar snapshot, corpus, toolchain y criterios del piloto.
3. [ ] Prophet completo compartido + adaptador firmware + golden y medición CPU/escucha on-device.
4. [ ] `gf-render` piloto con procedencia; después engines/SYNTH FX e HYBRID/GROOVE, uno por uno.
5. [ ] VST3 JUCE mínimo, prototipo visual, GUI definitiva y conexión con audiciones según puertas del spec.
6. [ ] Validación de fidelidad contra hardware e integración a principal con aprobación de Juan.

## Action Items históricos de la revisión con shim (sustituidos por la lista vigente)

1. [ ] Decidir las contradicciones 1–4 (Juan).
2. [x] Theory doc de la fase 1 (`theory/gemelo-fase1-nucleo-dsp.md`): inventario de engines y etapas, dependencias de la frecuencia de muestreo y de objetos
       oficiales, plan de pruebas golden.
3. [x] ~~Fase 1 con un engine piloto refactorizado a núcleo compartido~~ → reemplazado por el shim (§Revisión).
4. [ ] Shim en el repo + tests + `gf-render` con el Prophet-5. Puerta: render reproducible, afinación y tiempos
       correctos a 48 kHz, y Juan lo oye igual que el Brain.
5. [ ] Grabadora automática (`produccion/grabadora-automatica.md`).
6. [ ] `gf-render` con los 6 engines y la cadena SFX.
7. [ ] Fase 4 (fidelidad) con las tomas de la grabadora. Plugin y refactor a `libs/gf-dsp/`: en pausa.

## Referencias

- PJRC, Teensy Audio Library: `AudioStream`, bloques de 128 muestras, `AUDIO_SAMPLE_RATE_EXACT`.
- Steinberg VST3 SDK y Apple Audio Unit v3: modelo de `prepare` / `process` con frecuencia y tamaño de bloque
  variables.
- JUCE, iPlug2, CLAP (free-audio/clap) y clap-wrapper: documentación oficial de cada framework (licencias a verificar
  al momento de elegir).
- Välimäki, V. y Huovilainen, A. "Antialiasing Oscillators in Subtractive Synthesis". *IEEE Signal Processing
  Magazine* 24(2), 2007 (PolyBLEP; el oscilador depende de la frecuencia de muestreo).
