# Auditoría de motores del VST gemelo — 2026-11-01

## Alcance y método

Revisión estática del contrato de ADR-006, firmware Studio (especialmente `src/sketches/49-studio-brain.cpp`), adaptadores, parámetros JUCE, códecs GFP/GFL, interfaces y pruebas de `apps/brain-plugin/`, en la rama `feat/gemelo-dsp`, commit base `6181a07`. Se ejecutó `ctest --test-dir /private/tmp/gf-brain-vst-build --output-on-failure -E comparison`: 10/10 aprobadas. Se excluyeron explícitamente las comparaciones de referencia y no se conectó hardware, como pidió Juan. No se ejecutó validación visual, MIDI ni rendimiento en Live.

**Alcance de confianza:** esta auditoría identifica recorridos y diferencias comprobables en el código; no es todavía una matriz exhaustiva de cada slot GFP, ni certifica fidelidad sonora, cuantización, timing físico o comportamiento en todos los DAW. Los sprints históricos describen incrementos en distintos momentos: sus frases de pendientes no necesariamente son actuales.

## Hallazgos transversales

| Prioridad | Hallazgo confirmado | Evidencia / consecuencia |
|---|---|---|
| Alta | No hay una sola implementación completa compartida entre firmware y plugin. | `apps/brain-plugin/CMakeLists.txt` enlaza núcleos como `prophet_osc_core`, `dx7_core`, `dsp/lfo` y funciones UI, pero los seis `*_preview.cpp` implementan rutas de voz y procesamiento propias. ADR-006 pide evitar divergencia; compartir algunos núcleos no demuestra paridad del grafo completo. |
| Alta | CC 74 y 71 no tienen rama de procesamiento en SYNTH Prophet, Juno ni DX7. | `plugin_processor.cpp`, `processBlock` (ramas de CC 74/71 para HYBRID, ARP, Model D y OB-6); `49-studio-brain.cpp`, `control_change`, los aplica al engine seleccionado. No se debe confundir con automatización de parámetros del host. |
| Alta | Prophet SYNTH no tiene editor completo de parámetros ni exportación GFP. | `BrainEditor` crea solo cutoff, resonance, attack y release para el modo por defecto; el selector de páginas no incluye Prophet y `export_.setVisible` lo excluye. `PresetImport` no declara exportación Prophet. La importación aplica un patch y parte del bus, pero la edición de OSC, ENV, LFO, macros, GROOVE y FX no tiene paridad con los otros motores. |
| Alta | CC de macros 14, 15, 16 y 19 no tienen rama para Prophet SYNTH. | Las ramas de `processBlock` cubren HYBRID, ARP, Model D, OB-6, Juno y DX7; el `control_change` Studio llama `studio::apply_cc` para el engine activo. La macro del GFP cargado puede aplicarse al importar, pero no equivale a control MIDI en vivo. |
| Media | SOURCE de PUMP se conserva pero no decide el reloj del plugin. | `gemelo-juno-referencia.md` §SOURCE y `plugin_processor.cpp` (tooltip SRC GFP); `apply_groove_preview` configura PUMP sin pasar SOURCE. El DAW gobierna transporte; MIDI/INT del Brain no están replicados. |
| Media | LFO SYNC sigue BPM, pero seek/loop y fase no se realinean. | `plugin_processor.cpp` pasa BPM a `set_lfo_sync`/`set_lfo_control`; `dsp/lfo` expone un acumulador libre y no recibe posición PPQ. El estado sync no cabe en GFP/GFL y la exportación lo rechaza. La semántica deseada en seek debe decidirse frente al comportamiento Brain, no asumirse. |
| Media | GFP exportado no siempre conserva la identidad editable de las macros. | Los exportadores SYNTH incorporan valores efectivos de GRIT/POCKET/MOVE y ponen esas macros a cero. Se intenta conservar el sonido, pero el GFP resultante no conserva sus posiciones independientes; el estado del proyecto sí. Revisar por engine y saturaciones. |
| Media | Chords aprendidos y estado del panel no viajan en GFP. | `apps/brain-plugin/README.md` indica que se usa la memoria inicial. Una carga GFP aislada no basta para reproducir memorias aprendidas en el Brain. |
| Media | Cambio de preset / restauración necesita medición de tiempo real. | `apply_pending_preset` ejecuta preparación de Prophet al recibir estado vacío; `ProphetPreview::prepare` recalcula tablas de chorus y reinicializa estado. No se constató asignación dinámica en ese recorrido, pero no existe medición de peor caso/xruns en Live. |
| Media | Preescucha duplica procesamiento mientras se usa. | El procesador auxiliar y el principal avanzan juntos; hay fundido de 10 ms al entrar/salir, pero el cambio entre dos presets auxiliares y bloques mayores al buffer reservado pueden conmutar sin fundido. Falta medir CPU y escuchar en Live. |

## Matriz por motor

| Motor | Confirmado en el VST | Brecha o trabajo prioritario | Evidencia y límites |
|---|---|---|---|
| Prophet-5 | Cinco voces; GFP v2–v5; núcleos OSC compartidos, filtros/envolventes, LFO, GROOVE y bus FX aplicado al cargar. | Implementar edición completa, macros CC 14/15/16/19, CC 74/71 y exportación GFP; revisar cada slot OSC/MOD y la ruta de FX. | `prophet_preset.cpp`; `prophet_preview.h` (5 voces); `plugin_processor.cpp`. La bandera histórica `limitations.synth_fx` marca reverbs no PLATE aunque hay una implementación MODAL aproximada: revisar semántica antes de afirmar ausencia. |
| Juno-106 | Seis voces; OSC/sub/noise, envelopes, chorus, LFO, GROOVE, FX, macros CC 14/15/16/19; GFP exportable. | Implementar CC 74/71; completar matriz slot → importación → DSP → automatización → exportación. Verificar diferencias de cuantización, ruido, control por bloque y chorus. | `juno_preset.cpp`, `juno_preview.cpp`, `gemelo-juno-referencia.md`. Existe comparación diagnóstica mediante shim, sin umbral de fidelidad ni cobertura total de presets/FX. |
| OB-6 | Seis voces, dos VCO, filtro SEM, X-MOD, sync, glide, UNISON, GROOVE/FX y CC 14/15/16/19/71/74. | El exportador exige haber cargado previamente un GFP OB-6; habilitar o decidir exportación desde INIT. Auditar todas las conversiones de macros/slots y discrepancias por control/PCM. | `ob6_preset.cpp`, `plugin_processor.cpp::export_ob6_preset`. Existe fixture diagnóstico con shim; no certifica equivalencia física ni todos los GFP. |
| Model D | Monofónico, tres VCO, noise, modulación, glide, filtros, GROOVE/FX y CC 14/15/16/19/71/74. | Exportación desde INIT exige GFP previo. Contrastar mapeos GFP, prioridad/legato, oscilador 3 y cuantización con firmware; no declarar completitud a partir de la etiqueta parcial. | `model_d_preset.cpp`, `model_d_preview.cpp`, `export_model_d_preset`; pruebas nativas cubren escenarios de control y audio, no corpus exhaustivo. |
| ARP 2600 | Cuatro voces, modo mono, dos VCO, ring, VCO3 de modulación, ruido, sync, glide, GROOVE/FX y CC 14/15/16/19/71/74. | Exportación desde INIT exige GFP previo. Comparar todos los slots y modos mono/poli/robos con firmware; no inferir soporte de características no modeladas solo por el nombre ARP 2600. | `arp2600_preset.cpp`, `arp2600_preview.cpp`, `export_arp2600_preset`; pruebas nativas incluyen ruido, sync, VCO3, glide y salida finita. |
| DX7 | Seis voces, cuatro operadores, algoritmos, feedback, controles de operadores, LFO, GROOVE/FX y CC 14/15/16/19; importación y exportación GFP. | Implementar CC 74/71, contrastar todos los slots/algoritmos y cuantización/cola con firmware; revisar semántica de macros exportadas. | `dx7_preset.cpp` mapea operadores y `dx7_core.cpp` se enlaza directamente. `prophet_preview_test.cpp` incluye smoke DX7; no hay target CTest separado para DX7. |
| HYBRID | Dos mitades de engines distintos, GFL, MORPH, delay, transpose, ganancias, controles por mitad, GROOVE/FX de A, CC 17/18 y filtros/expresión. | Auditar las 30 combinaciones ordenadas y compatibilidad de slots; CC18 se entrega en hilo de mensajes, no con precisión de muestra. Validar compensación de unísono aplicada a B antes del bus no lineal y diferencias de salida. | `hybrid_preview.cpp`, `hybrid_router.cpp` y `gemelo-hybrid.md`. Tests ejercitan las ocho capas de fábrica y ambos lados a varias frecuencias, no todas las combinaciones ni la fidelidad física. |

## Prioridad de trabajo antes del A/B físico

1. Implementar y probar CC 74/71 en Juno, DX7 y Prophet, respetando el mapeo y el orden de macros del firmware; incluir offset MIDI, saturación, persistencia y prioridad de automatización.
2. Completar Prophet SYNTH por cortes verticales: parámetros y macros, GROOVE/FX, exportación y estado. Confirmar primero el contrato GFP, migración y comportamiento en saturaciones.
3. Construir una matriz automatizada por engine/slot GFP y GFL: importación, efecto observable, automatización, exportación/reimportación y entradas fuera de rango; distinguir campos de hardware que no forman parte del formato.
4. Resolver exportación desde INIT en OB-6, Model D y ARP y especificar la conservación de posiciones de macros y LFO SYNC sin cambiar silenciosamente el formato de firmware.
5. Definir el contrato de reloj SOURCE y seek/loop para DAW, probar bloques y posiciones variables; medir rendimiento de preset, polifonía, múltiples instancias y preescucha en Live.
6. Comparar las rutas de firmware y plugin, cuantización y buses con pruebas nativas apropiadas; las pruebas A/B físicas y umbrales subjetivos se hacen al final por instrucción de Juan.

## Estado de validación

El resultado 10/10 cubre las suites de comportamiento incluidas en esa configuración, no es una certificación de paridad. Los tests `juno_comparison_smoke` y `ob6_comparison_smoke` son diagnósticos de fixtures y no establecen un umbral de aprobación; se excluyeron de esta ejecución por el orden de trabajo acordado. No se midió la señal del codec SGTL5000 ni la salida física, ni se probó la compilación más reciente en Live.

## Seguimiento de corrección: Juno CC 74/71

Implementado en el VST: CC 74 y 71 llegan al filtro Juno en el offset MIDI y se aplican antes de GRIT/MOVE. El valor MIDI de resonancia se conserva incluso por debajo del mínimo del control del host; cambios del usuario/DAW recuperan la prioridad. Se cubren los valores 0 y 127, macros, entrega diferida, guardado/restauración antes y después de entregar, exportación, carga de otro GFP y aislamiento frente a otro motor. No se hizo A/B con hardware. La fila histórica de la matriz refleja el diagnóstico anterior a esta corrección; Prophet y DX7 siguen pendientes.

## Seguimiento de corrección: DX7 y Prophet CC 74/71, DC de la PLATE (2026-11-03)

Implementado en el VST (`gemelo-correcciones-2026-11-03.md`): CC 74 y 71 llegan al filtro del DX7 y del Prophet en el
offset MIDI, con los rangos de `GF_CUTOFF_RANGE` / `RESO_RANGE` del firmware, se guardan con el proyecto y se entregan
al host. En el Prophet se convierten con MOVE y GRIT del preset cargado, porque sus parámetros ya los traen aplicados.
Además, la salida pasa por un bloqueador de DC de 5 Hz: la PLATE dejaba hasta −55 dBFS de DC al terminar la cola. Siguen
pendientes las macros 14/15/16/19, el editor y la exportación del Prophet. No se hizo A/B con hardware.
