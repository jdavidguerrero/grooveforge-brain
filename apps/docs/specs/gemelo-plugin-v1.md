# Spec · GrooveForge Brain Gemelo v1 (JUCE / VST3)

> Fecha: 2026-10-29 · Versión: 0.1 · Estado: aprobado por Juan para iniciar la implementación (chat).
> Rama: `feat/gemelo-dsp` · Owner: Juan Guerrero.
> Decisiones confirmadas: JUCE, SYNTH + HYBRID, GUI actual reorganizada para escritorio con referencia organizativa Serum.
> Theory previo: [gemelo-plugin-juce.md](../theory/gemelo-plugin-juce.md).
> El layout concreto requiere una revisión visual posterior, antes del acabado del editor.

## 1. Problema y resultado

El Brain físico interpreta una parte o una capa a la vez. Producir varias pistas exige grabaciones sucesivas; las
audiciones del motor de asistencia dependen de esa captura. Queremos un instrumento autónomo con el sonido y los
presets actuales del Brain, varias instancias por canción y un renderizador que use exactamente el mismo motor.

Éxito v1: abrir un set de Live con al menos cuatro instancias (bajo, acordes, pluck, lead), reproducirlas sin Brain,
automatizar macros, congelarlas, exportar y reabrir el set conservando sus patches. Una audición A/B/C puede producir
WAVs con procedencia sin sesión de grabación manual.

## 2. Alcance y límites

**P0:** seis engines actuales, SYNTH, HYBRID de dos engines distintos, GFP/GFL, modulación y respuesta musical del
Studio, SYNTH FX, GROOVE que interviene en esos modos, estado del DAW, MIDI y automatización de controles editables.
El DX7 conserva cuatro operadores/ocho algoritmos (ADR-005). La polifonía replica el firmware vigente por engine;
no adopta automáticamente los objetivos comerciales de voces que difieren del estado actual en `01`.

**Primer host/plataforma propuestos:** Ableton Live en macOS, VST3. La arquitectura del Mac real y versión de Live
se registran al preparar el primer build; no se presupone Apple Silicon ni se promete un Universal Binary sin validar.
AU es P1; Windows/Linux y distribución comercial son posteriores.

**Fuera de v1:** FX Processor con entrada externa, add-on analógico, control remoto bidireccional del Brain,
multitímbrico dentro de una instancia, motores nuevos, wavetables, matriz de modulación nueva, AI/cloud dentro del
callback y modificación automática de `.als`. La grabadora automática previa queda como herramienta separada; no
bloquea el desarrollo del plugin ni es requisito para usarlo.

## 3. Historias prioritarias

1. Como productor, cargo mi preset Brain en una pista y lo edito mientras las otras instancias siguen sonando.
2. Como productor, cargo una capa, veo las dos mitades y ajusto MORPH/retraso sin navegar combinaciones de botones.
3. Como productor, reabro una canción aunque haya reorganizado o borrado mis bancos externos.
4. Como diseñador de sonidos, cambio parámetros mediante ratón o automatización y la GUI muestra el valor efectivo.
5. Como usuario del motor de asistencia, comparo tres propuestas con el mismo MIDI/contexto y hashes verificables.

## 4. Arquitectura y build

| Ruta propuesta | Responsabilidad |
|---|---|
| `libs/gf-dsp/` | DSP custom y objetos oficiales portados/aislados, engines completos y efectos |
| `libs/gf-instrument/` | Patches, codecs de presets compartidos, Hybrid, macros, GROOVE, reloj y eventos |
| `apps/gf-render/` | CLI de MIDI a WAV con manifiesto |
| `apps/brain-plugin/` | Wrapper JUCE, parámetros del host, estado y editor |
| `libs/teensy-audio-shim/` si se necesita | Harness del grafo anterior para golden, no dependencia del VST |

C++17 en las librerías compartidas y firmware. CMake para desktop/JUCE; fijar tag/commit de JUCE y requerimientos
de su toolchain al incorporarlo. Si JUCE requiere un estándar mayor, se restringe al wrapper desktop sin elevar el
del firmware por accidente. PlatformIO sigue compilando Teensy. No hay segunda copia de algoritmos para el plugin.
La modalidad de licencia JUCE se registra antes de incorporar la dependencia; elegir framework no presupone un tier.

Contrato: `prepare(fs, max_block)` reserva; `reset(seed)` fija estado inicial; `process` recibe audio/eventos/transporte
con memoria preparada. El núcleo no conoce `AudioProcessor`, `AudioStream`, LVGL, I2S o SGTL5000. La API pública usa
snake_case y Doxygen; no se añaden declaraciones de implementación en esta entrega documental.

## 5. Requisitos funcionales

| ID | Requisito y aceptación |
|---|---|
| F01 | SYNTH carga cada uno de los seis engines con sus controles y polifonía actuales; un corpus fija asignación/robo de voces por engine antes de migrarlo. |
| F02 | HYBRID restaura engine y patch A/B, ganancias base, MORPH, transposición y retraso. Mantiene la prohibición actual de usar el mismo engine en ambas mitades. |
| F03 | MORPH usa `layer_gains` actual. Se prueban 0/50/100 y ganancias no simétricas; no se renormaliza el volumen automáticamente. |
| F04 | SYNTH FX conservan orden, estéreo y comportamiento del sketch 49. En HYBRID son comunes y usan A; la interfaz lo identifica. |
| F05 | Se importan GFP v5 y GFL v2 y los formatos anteriores admitidos por los decodificadores actuales. Entrada inválida se rechaza conservando el sonido previo. |
| F06 | MIDI: note-on/off, velocity, sustain, bend, mod wheel y CC existentes que afecten el sonido. Tabla exacta se extrae del sketch 49 antes del primer build, sin inventar CC nuevos. |
| F07 | CC 17 controla MORPH; CC 18 conserva selección/salida de capa según `layer_from_cc`, incluido su mapeo dependiente del número y orden del banco. |
| F08 | Arp, chord/strum, macros y pump usan reloj de muestras/transporte. No se añade ML para reproducir la lógica GROOVE existente. |
| F09 | Cada instancia es independiente; abrir/cerrar su editor no afecta su procesamiento. Cuatro instancias forman parte del demo v1. |
| F10 | Reproducción, Freeze y exportación usan el mismo camino DSP y mantienen colas. El wrapper informa la cola y cualquier latencia añadida real al host. |
| F11 | Stop/panic/cambio de modo limpian notas y eventos pendientes según política documentada; no quedan notas colgadas. Seek/loop reinician la agenda musical sin resetear colas arbitrariamente. |
| F12 | Guardar/reabrir restaura estado sin archivos externos. Archivo/estado futuro desconocido muestra error o compatibilidad limitada explícita, sin aplicar datos parcialmente. |

La agenda de implementación incluye exportar GFP/GFL cuando el estado cabe en el formato del Brain. Semilla, tamaño
del editor y otros datos exclusivos del plugin se guardan en su contenedor de estado, no se fuerzan en GFP/GFL.

## 6. Contratos de audio, tiempo y automatización

- Referencia nominal Brain: **44.100 Hz**. Host soportado v1: 44,1/48/96 kHz, estéreo, procesamiento float32.
- El resultado del núcleo depende de eventos absolutos y ticks de control, no de la partición del callback.
- Ticks de control del piloto: 1 ms con acumulador fraccional. Documentar toda excepción al migrar cada engine.
- Eventos MIDI se aplican en su offset. Retrasos internos se expresan en muestras; note-off cancela la nota pendiente.
- El host gobierna BPM, play/stop, posición y compás si están disponibles; sin transporte válido se usa tempo interno
  guardado (default propuesto 120 BPM) y el reloj avanza por audio. El piloto compara ese fallback con el Studio antes
  de declararlo equivalente.
- Automatización: IDs permanentes por ruta (`a.*`, `b.*`, `global.*`) e IDs estables por engine donde aplique. Cambiar
  el foco A/B no cambia qué controla un ID. Enumeraciones mantienen significado entre versiones.
- Registrar la granularidad efectiva entregada por JUCE/host. Suavizado de cutoff, nivel, mezcla y macros continuas
  con tiempos por parámetro; los pasos discretos no se interpolan como si fueran valores continuos.
- Un único valor efectivo alimenta motor, editor y estado; gestures del ratón notifican al host. No usar un envío de
  CC de retorno que cree bucles de automatización.
- El callback no reserva/libera memoria dinámica, bloquea, accede a disco ni llama a la GUI. Eventos/colas tienen
  capacidad fija; sobrecarga preserva note-offs o ejecuta panic seguro y deja diagnóstico fuera del audio.

No se transfieren al plugin los targets físicos de latencia USB/I2S del Brain. El buffer del DAW y la interfaz
forman parte de la latencia total. Una arquitectura sin SRC ni buffering extra buscará no añadir latencia propia;
se informa y prueba el valor real, no se fija cero por anticipado.

## 7. Estado y bancos

El contenedor del plugin incluye versión, modo, engine(s), patches completos, parámetros modificados, macros,
GROOVE/SFX, valores de capa, tempo fallback, configuración de semilla y banco de capas usado por CC 18.
Guardar solo nombres/slots no cumple. En SYNTH se preserva también cualquier banco invocado por selección MIDI que
se habilite; si no se soporta ese selector en v1 se declara expresamente en su tabla MIDI.

Separar estado musical de editor (tamaño, pestaña, foco). El foco no cambia el sonido. Reabrir restaura configuración,
no buffers de delay ni fases a mitad de nota; el render reproducible comienza con reset/semilla y origen definidos.

## 8. Organización de GUI propuesta

Ventana inicial propuesta 1200×800 unidades lógicas, redimensionable; mínimo de diseño 1000×700. El prototipo visual
validará medidas antes de fijarlas. La inspiración Serum se limita a módulos visibles y edición directa. Se mantiene
la identidad GrooveForge del tema Studio, sin representar un sintetizador de wavetables.

```text
┌ Brain · SYNTH / HYBRID ─ Preset de instrumento ─ Guardar/Importar ─ Master ┐
│ A: engine / preset / nivel       MORPH       B: engine / preset / nivel   │
│ foco A/B · retraso/transposición B · enlace de filtros                   │
├ SYNTH ─ SYNTH FX ─ GROOVE ─ BROWSER ──────────────────────────────────────┤
│ Osciladores / operadores del foco    │ Filtro / mezcla / carácter        │
│ Controles específicos del engine    │ Controles con unidades            │
├ AMP ENV ─ FILTER ENV ─ LFO / MOD: gráficos y controles directos ──────────┤
│ DRIFT       GRIT       POCKET       MORPH       MOVE       teclado*      │
└─────────────────────────────────────────────────────────────────────────┘
* teclado opcional; no reduce los controles esenciales al tamaño mínimo.
```

| Área | Comportamiento |
|---|---|
| Cabecera | Modo y preset general, estado modificado, importar/guardar, nivel master con medidor |
| A/B persistente | HYBRID muestra ambas mitades y foco explícito. SYNTH muestra A y B deshabilitada, sin doble significado |
| SYNTH | Parámetros OSC/mixer/filtro del engine enfocado visibles por grupos; nombres correctos para FM o arquitectura sustractiva |
| Modulación | Envolventes y LFO actuales con destinos existentes; sin matriz nueva. Gráficos ilustrativos identificados si no representan medición real |
| SYNTH FX | Cadena en orden con on/off, mezcla y parámetros. Etiqueta «Compartidos · configuración de A» en HYBRID |
| GROOVE | CHORD/ARP/STRUM/PUMP y controles actuales; tempo y fuente de transporte visibles |
| BROWSER | Filtros por modo/engine, selección y carga diferenciadas; error de importación conserva patch anterior |
| Macros | Siempre disponibles. MORPH deshabilitada en SYNTH con explicación breve; MOVE conserva sus destinos actuales |

Se reutilizan nombres y diagramas de `screen_osc`, `screen_env`, `screen_lfo`, `screen_hybrid`, `screen_sfx` y tokens
de `theme.h`. Se reimplementan componentes en JUCE; no se introduce LVGL dentro del editor. Teal identifica SYNTH y
cyan HYBRID; A/B se distinguen además mediante texto/foco, sin depender solo del color.

Interacción: click/arrastre, ajuste fino con modificador, valor numérico editable, reset a default, tooltip/unidades,
tabulación y foco de teclado. El cambio de página no altera valores; la automatización actualiza el control sin
realimentarla al host. En el tamaño mínimo no hay solapamientos ni controles esenciales inaccesibles.

Demo visual previa al editor definitivo: cargar SYNTH, pasar a HYBRID, editar B, ajustar MORPH y editar FX comunes
sin ambigüedad de foco. Juan valida organización y legibilidad antes de pulir toda la interfaz.

## 9. Renderizador y asistencia

CLI propuesta (no existe todavía):

```sh
gf-render --preset patch.gfp --midi clip.mid --sample-rate 48000 --seed 42 --output take.wav
gf-render --layer layer.gfl --midi clip.mid --sample-rate 48000 --seed 42 --output hybrid.wav
```

Tempo del archivo MIDI si existe, override explícito registrado y fallback definido si falta. Render stereo PCM24,
sin normalización implícita. Política de cola: duración explícita o detector de silencio con mínimo/máximo acotados;
resonancia sostenida no puede dejar un render infinito.

Manifiesto versionado: hashes de GFP/GFL/MIDI/WAV, versión/commit del DSP, build/arquitectura, sample rate/formato,
tempo/mapa de tempo, semilla, política de cola, opciones y `source: render`. Piloto sin FX lleva `fx: none` y no se
presenta como gemelo completo del preset.

`make_audicion.py` conserva su registro de opciones/contexto/linaje; añade renders de ese snapshot. La copia ajustada
de nivel para A/B tiene hash propio y vínculo al original. Nunca se asigna preferencia ni «escuchado» automáticamente.

## 10. Secuencia y puertas

| Hito | Entrega vertical | Puerta antes de avanzar |
|---|---|---|
| M0 | Documentos + snapshot de referencia | Revisar este spec; fijar commit, toolchain, bancos, corpus y tolerancias |
| M1 | Prophet compartido + adaptador Teensy + harness golden | Misma interpretación; tests; CPU y escucha on-device |
| M2 | `gf-render` Prophet/GFP/MIDI/WAV/manifiesto | Hash repetible, afinación/tiempos, WAV 48k/24 y procedencia |
| M3 | Seis engines + SYNTH FX, migración uno por uno | Cada engine tiene demo/test y no regresa firmware; SFX estéreo validado |
| M4 | HYBRID + GROOVE y GFL | Casos A/B, CC, scheduler, macros, colas y presets propios |
| M5 | VST3 JUCE con editor mínimo | Audio MIDI, estado, cuatro instancias, Freeze/export en Live |
| M6 | GUI de escritorio | Prototipo visual validado y pruebas de edición/automatización/foco |
| M7 | Audiciones y fidelidad | A/B/C con hashes, informe hardware y sesión musical completa |

No hay dos features simultáneas. M1/M2 conservan la puerta Prophet del ADR; el editor no antecede a la equivalencia
del piloto. Se puede preparar un wireframe documental sin anticipar la implementación del motor.

Presupuesto inicial orientativo acumulado: M1–M2 40–70 h; M3–M4 130–230 h; conjunto 200–350 h. A 10–18 h/semana,
aproximadamente 3–8 meses. Reestimar después de M2 con horas reales; no convertir estas cifras en fechas de entrega.

## 11. Criterios de aceptación y pruebas

Umbrales iniciales propuestos; se revisan en M0, antes de generar referencias, y no se relajan tras un fallo sin
explicar su causa. El theory anterior tiene umbrales más estrictos ligados a una simplificación float no demostrada;
no se toma aquella suposición como prueba de viabilidad.

| Prueba | Criterio |
|---|---|
| Golden Prophet G1–G4/G6 | Eventos alineados, fs/semilla/control iguales: error RMS relativo ≤−80 dB y pico absoluto ≤−60 dBFS; valores cercanos al silencio usan error absoluto, sin dividir por cero |
| Auto-oscilación G5 | Envolvente por ventanas 10 ms dentro de ±0,5 dB y frecuencia dominante dentro de ±1 cent; sin exigir fase idéntica |
| Afinación aislada | A4 440 Hz ±1 cent con drift/modulación apagados y ventana/resolución suficiente |
| Tiempo entre fs | Ataque/release medidos con el mismo método, diferencia ≤2% o una muestra (el mayor); registrar umbrales de cruce y valor real medido, no asumir que el knob equivale al 10–90% |
| Filtro entre fs | Misma configuración y respuesta medida frente a 44,1 kHz; frecuencia característica dentro de ±2%, sin asumir que el cutoff nominal del ladder de cuatro polos sea su punto −3 dB |
| Partición de bloques | Eventos absolutos idénticos: error máximo ≤1e−6 FS en el mismo build, sin desplazamiento de eventos |
| Repetibilidad CLI | Dos resets iguales en mismo build/entorno producen WAV/hash iguales |
| Aislamiento | Crear/procesar/resetear otra instancia no cambia el resultado de la primera |
| Persistencia | Reabrir sin bancos externos conserva parámetros y asignación CC 18; mismo render desde origen/reset |
| Offline | Con seed/reset/origen/tempo iguales, plugin y CLI producen audio dentro de tolerancia digital y colas sin cortar |
| Firmware | Suite native vigente verde; build `sketch49` y smoke del target CI `sketch`; CPU normal ≤60% y delta piloto ≤2 puntos porcentuales con idéntico escenario |
| Rendimiento desktop | Registrar máquina, host, buffer, cuatro patches y CPU; sesión de 10 minutos a 48 kHz/128 sin dropouts. No inferirlo de los budgets Teensy |
| GUI | Dos instancias con modos distintos, editor cerrado, resize mínimo, edición B y automatización no alteran otra instancia/foco |

Pruebas digitales no validan codec/salida física. CPU y escucha on-device necesitan Teensy y evidencia real; se marcan
pendientes si no hay hardware. No se declara alcanzado un hito porque compila. La comparación hardware final fija sus
umbrales perceptuales en un theory específico antes de medir (ADR-006 fase 4).

## 12. Decisiones, conflictos y pendientes

- **Confirmado por Juan:** JUCE y dirección de GUI organizada como instrumento de escritorio; retomar el plugin.
- **Verificado por código instalado:** 44.100 Hz nominales en Teensy 4.1 sin override.
- **Dirección aprobada:** librería de instrumento separada, procesamiento al fs del host, tick 1 ms y secuencia de
  hitos. El layout y tamaño final se validan con un prototipo visual. Se fijan los umbrales antes de la captura golden.
- **Se conserva abierto:** producto/precio/North Star (`00`), como declara ADR-006. El gemelo se trata como herramienta
  interna; no se cambia posicionamiento ni se decide distribución.
- **Contrato `01`:** estructura compartida requiere actualización tras pasar el piloto y dentro de la autorización
  del ADR. Las divergencias de polifonía comercial se documentan, no se resuelven aumentando voces.
- **Contrato `04`:** etiquetar `render` en nuevos manifiestos no da por aprobada una modificación del dataset de
  entrenamiento canónico ni sustituye su procedencia física.
- **Antes del primer build JUCE:** versión fijada, toolchain/arquitectura real del Mac, modalidad de licencia y IDs
  de plugin/automatización. **Antes de M3:** inventario exacto PJRC, licencias y revisión de delay/reverb a fs variable.
- **Antes de M6:** prototipo visual y revisión de Juan; Serum es referencia verbal de organización, no se ha realizado
  una auditoría de su interfaz ni se incorporan sus assets.

## 13. Referencias

- [ADR-006](../adr/ADR-006-plugin-gemelo.md), [theory](../theory/gemelo-plugin-juce.md),
  [capas Studio](../variants/studio/theory/12-studio-capas.md), [motor de asistencia](../produccion/motor-asistencia.md).
- [JUCE AudioProcessor](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html),
  [CMake API](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md),
  [licencia JUCE 8](https://juce.com/legal/juce-8-licence/), consultados 2026-10-29.
