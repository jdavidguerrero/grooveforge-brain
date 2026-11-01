# Gemelo VST: HYBRID SYNTH

## Fundamento y contrato

HYBRID reproduce dos engines distintos a partir de las dos mitades completas de un `LayerPreset` GFL v1/v2. La mezcla utiliza `studio::layer_gains`: cada ganancia base en dB se convierte a lineal y MORPH recorre las ramas coseno/seno, limitadas a 1 y escaladas para conservar las ganancias base en el centro. B recibe la nota transpuesta, limitada a MIDI 0–127, y su ataque se retrasa en milisegundos; si se suelta antes del disparo, se cancela. El firmware aplica CHORD → ARP antes de distribuir las notas a A y B, y después mezcla ambos engines antes del bus SYNTH FX/PUMP compartido. CC 17 controla MORPH; CC 18 selecciona capa o sale de HYBRID.

El núcleo común `ui/layer_bank.cpp` es código custom, no Teensy Audio Library. Se reutiliza para GFL y capas de fábrica; la validación del VST añade comprobación de datos finitos y preserva el estado si falla la lectura. La implementación se entrega por cortes: codec; render de dos mitades, temporización y bus compartido; estado y edición; controles MIDI y banco. No se presenta HYBRID como completo hasta cubrir esos cortes. Las pruebas funcionales usan GFL de fábrica, eventos MIDI y bloques variables; la comparación A/B con el Brain se reserva al final de toda la implementación.

Referencias: ADR-006; `variants/studio/theory/12-studio-capas.md` §§3b–3e; `ui/layers.h`, `ui/layer_bank.cpp`; `49-studio-brain.cpp::sound_on`, `sound_off`, `layer_apply` y `groove_tick`; `05-fx-architecture.md`.

## Núcleo de ruteo temporal

El ruteador funciona por muestra y no reserva memoria en el procesamiento. Envía cada nota a A y agenda B según el retraso configurado; si la nota se suelta antes, cancela el disparo pendiente. Conserva la transposición usada al disparar para liberar la nota correcta aunque se edite después. Las notas distintas que se conviertan en la misma nota B por el límite MIDI comparten una cuenta de referencias, evitando liberar B mientras otra siga pulsada. El mezclador llama directamente a `studio::layer_gains`. Las pruebas comprueban extremos y centro de MORPH, transposición, retraso, cancelación y colisiones, sin A/B físico.

## Render portable de dos mitades

Cada mitad prepara el engine indicado y aplica su GFP con los adaptadores existentes. Un único bus portable aplica CHORD y ARP antes del ruteador y PUMP y SYNTH FX después de mezclar las señales estéreo. Se usa la mitad A para GROOVE y SYNTH FX; la mitad B se limita a una voz de unison como `layer_apply` del firmware. La memoria de delay y reverb se reserva al preparar, nunca al procesar una muestra. Se comprueban las ocho capas de fábrica, salida finita y comportamiento con notas; siguen pendientes la integración JUCE, edición y validación de fidelidad. El render usa los pilotos existentes y puede diferir por sus etapas de salida y temporización; no certifica paridad física.

## Integración inicial JUCE

El selector HYBRID activa el render portable en el VST. MORPH, DELAY B y TRANSPOSE B se publican como parámetros del host y los eventos MIDI llegan a las dos mitades a través del bus de GROOVE; bend, rueda, presión y sustain se enrutan en la muestra correspondiente. La importación de un GFL válido activa HYBRID y guarda el archivo íntegro junto con los parámetros en el estado del proyecto. Las capas de fábrica están preparadas como valor inicial; la publicación del GFL al hilo de audio usa un buzón atómico y las memorias de engines y efectos se reservan en `prepareToPlay`. Este corte todavía no ofrece editor completo de las dos mitades ni selección de banco mediante CC18. La prueba comprueba importación, audio, estado y offset MIDI sin A/B físico.

## Exportación y selección de capas de fábrica

El VST exporta GFL v2 preservando ambas mitades y escribiendo MORPH, retraso y transposición actuales. Se rechaza la exportación con LFO SYNC activo porque GFL no guarda ese ajuste. CC 18 usa `studio::layer_from_cc`: 0 vuelve al motor SYNTH que estaba seleccionado y 1–127 seleccionan las ocho capas de fábrica. Para evitar cargar y decodificar presets en el hilo de audio, CC 18 se comunica al hilo de mensajes y la capa nueva se publica al procesador mediante el buzón atómico; su cambio no es exacto a muestra. La prueba valida el mapeo, exportación, conservación del estado y selección de fábrica sin A/B físico.

## Mitades y ganancias de HYBRID

Las mitades A y B se pueden reemplazar por archivos GFP independientes, siempre que pertenezcan a engines distintos. Los cambios conservan MORPH, retraso, transposición y ganancias actuales, y se guardan en el proyecto y al exportar GFL. Las ganancias base se editan en dB, como en `LayerPreset`; MORPH sigue usando la curva compartida. Este corte selecciona presets completos para cada mitad; la edición directa de todos sus parámetros desde HYBRID queda pendiente. Se prueban reemplazo, rechazo de engines repetidos y persistencia sin A/B físico.

## Controles básicos por mitad

Cada mitad expone cutoff, resonancia normalizada y ataque/release base del GFP, independientes y automatizables. El render aplica GRIT, POCKET y MOVE a esos valores, igual que al cargar el preset. El GFL exportado conserva valores base y macros sin incorporarlas dos veces. Un cambio de control se aplica sin resetear las notas. Al reemplazar una mitad se cargan sus nuevos controles; los de la otra se conservan. Los proyectos anteriores migran desde sus mitades GFL. Quedan fuera de este corte los controles detallados de osciladores, envolventes secundarias, modulación y efectos por mitad.

## Navegación del banco de fábrica

Un selector de escritorio carga las ocho capas de fábrica mediante el mismo camino que CC18. La interfaz identifica la capa y los engines A/B; al elegir una capa se restablecen sus controles y presets. No modifica el banco físico del Brain.

## Foco y CC de filtro

El foco A/B determina la mitad principal para CC 74 y 71. CC 74 usa la curva logarítmica del engine enfocado y hace seguir el cutoff de la otra mitad proporcionalmente; CC 71 aplica a la otra mitad el mismo cambio de resonancia normalizada, con límites por engine. El audio actualiza ambos parámetros en el offset MIDI y un buzón atómico los publica al DAW; guardado y exportación conservan valores pendientes. Referencias: `layer_follow_filter` y `control_change` de `49-studio-brain.cpp`. La prueba cubre foco, offset y restauración, sin A/B físico.

## Prioridad de MORPH

Un CC 17 se aplica en el audio inmediatamente y se publica al DAW de forma asíncrona. Si el usuario o la automatización edita MORPH después del CC y antes de su publicación, la edición posterior tiene prioridad y el CC antiguo se descarta. El estado y la exportación respetan esa prioridad.

## Macros de interpretación

La capa A aporta DRIFT, GRIT, POCKET y MOVE y las profundidades de GRIT/POCKET. Las macros se editan y automatizan desde HYBRID; CC 14/15/16/19 actualizan DRIFT/GRIT/POCKET/MOVE en el offset MIDI. DRIFT se aplica a ambas mitades y MOVE a los FX compartidos encendidos. Las actualizaciones reconfiguran los adaptadores existentes sin reiniciar las voces; se guardan los valores base en A dentro del GFL. El foco para CC 19 del firmware aún no se implementa: este corte dirige las macros a A. La prueba cubre estado, prioridad de automatización y cambios audibles sin A/B físico.

## Ganancia de UNISON en capas

El firmware compensa con `1/√N` la mezcla completa cuando la mitad A usa UNISON. Los pilotos de cada engine ya compensan sus propias voces; por ello se aplica esa compensación adicional a B antes de la mezcla para aproximar el mismo balance. La ubicación anterior a los FX no es matemáticamente equivalente a la del firmware cuando hay procesamiento no lineal; su diferencia queda para la validación final.

## GROOVE y efectos de la capa

Las páginas GROOVE y FX editan los parámetros de la mitad A, que son los que alimentan el bus compartido de HYBRID. Sus parámetros son independientes de los del modo SYNTH y se guardan en A al exportar GFL. Las ediciones no reinician las notas salvo cuando el propio arpegiador o CHORD necesita limpiar su estado al activarse o desactivarse. Se prueban aislamiento, persistencia y salida. La selección de reloj externo PUMP aún se adapta al transporte del DAW, como en los otros pilotos.

## Envolventes, LFO y voz de cada mitad

Cada mitad ofrece controles automatizables de decay/sustain de amplitud, ADSR y cantidad del filtro, drive, velocity, LFO, glide y UNISON. Attack/release comunes permanecen en la página de filtro y envolvente. Las ediciones se aplican sin volver a disparar notas y se guardan como valores base en su mitad GFL. B conserva sus parámetros de UNISON en el archivo, pero en reproducción se mantiene en una voz por nota como el firmware. DRIFT sigue siendo la macro compartida de A. Las pruebas verifican estado, aislamiento, exportación y comportamiento con notas activas, sin A/B físico.

## OSC y MOD específicos por mitad

Cada mitad expone las paradas OSC y MOD del engine seleccionado. Los parámetros del DAW usan valores normalizados 0–1; el adaptador transforma cada valor con los rangos y enumeraciones de `GF_OSC_SLOT` / `GF_MOD_SLOT`, incluyendo slots globales compartidos entre paradas. Las etiquetas y controles visibles cambian con el engine y la parada. Los slots no usados del GFP se preservan. El render aplica el patch actualizado sin redisparar notas, salvo al cambiar MONO en el ARP, que limpia voces; los valores se guardan en la mitad correspondiente del GFL y al cambiar una mitad se conservan los de la otra. Las pruebas comprueban engines, rangos discretos y roundtrip sin pruebas A/B físicas.

## MOVE en la mitad enfocada

MOVE de B se conserva y puede automatizarse por separado. CC 19 controla MOVE de la mitad que tiene el foco; MOVE de A sigue modificando los FX compartidos, mientras MOVE de B afecta a su propio filtro y patch. Ambas posiciones se guardan en sus respectivas mitades GFL. Se prueba la selección de foco y la recuperación del valor MIDI pendiente sin A/B físico.

## Afinación TAPE para Juno y Prophet

La modulación wow/flutter del TAPE compartido multiplica la frecuencia de ambas mitades, también cuando son Juno o Prophet. La razón externa se aplica además de la modulación propia del Prophet; en ausencia de TAPE vale 1 y no altera la afinación. La misma conexión se usa en el bus Juno de SYNTH. Se comprueba compilación y salida finita; la cuantía y la fase frente al hardware quedan para la validación final.

## Salida del Prophet antes del bus compartido

El piloto Prophet expone una salida de voces anterior a su etapa de salida y efectos. HYBRID usa esa salida para que la mezcla A/B pase una sola vez por el bus SYNTH compartido, igual que los demás engines; conserva la compensación de UNISON de la voz. El procesamiento SYNTH normal mantiene su recorrido anterior. La prueba cubre señal y regresiones de los otros modos; la fidelidad física queda para A/B final.

## Cola de notas en tiempo real

La cola conserva el próximo vencimiento y solo recorre las teclas cuando hay un evento de B listo o se cancela el vencimiento más cercano. Mantiene memoria fija y no realiza asignaciones por muestra. Esto evita recorrer 128 entradas en cada frame cuando no hay ataques retrasados; la prueba de temporización y cancelación sigue siendo la misma.

## Banco de capas del proyecto

El banco contiene las ocho capas de fábrica y admite guardar hasta 32 capas en total. Las capas adicionales se serializan completas como GFL dentro del estado del proyecto del DAW; no se escribe en el banco del Brain. CC 18 distribuye 1–127 sobre el número actual de capas y 0 vuelve a SYNTH. El selector muestra la misma lista. La carga y validación ocurren en el hilo de mensajes y se publican al audio mediante el buzón existente. Se prueban guardado, restauración, selección y límite del banco sin A/B físico.
