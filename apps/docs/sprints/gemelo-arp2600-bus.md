# ARP 2600: bus GROOVE y SYNTH FX

## Teoría y alcance

Las cuatro voces del ARP se mezclan en mono y alimentan el mismo bus portable que usan los demás pilotos del VST: DRIVE, TAPE, chorus/phaser, delay y reverb, con PUMP, ARP y CHORD. El GFP proporciona el estado y los parámetros de cada etapa. La entrada MIDI pasa primero por el generador de acordes y el arpegiador; las notas resultantes alimentan el ARP 2600. PUMP se sincroniza al transporte del DAW. La ganancia de UNISON permanece en el motor ARP para que no se aplique dos veces.

El wow/flutter de TAPE afecta la frecuencia de los osciladores en la muestra siguiente, siguiendo el acoplamiento de los demás pilotos. La referencia de reloj externa PUMP se conserva en el GFP pero este piloto sigue el transporte del DAW. Esta entrega cubre reproducción desde preset; la edición individual y exportación se abordan por separado. La prueba verifica señal procesada, activación de ARP y restauración del estado; no compara con hardware. Referencias: ADR-006, `05-fx-architecture.md`, `prophet_preset.cpp` y `49-studio-brain.cpp::groove_tick`.

## Exportación GFP de los controles disponibles

El VST exporta desde el GFP ARP importado, preservando los campos que aún no se editan. El cutoff, resonancia, ataque, release y RING actuales reemplazan los valores correspondientes. Como los controles del VST contienen valores con macros aplicadas, GRIT, POCKET y MOVE se incorporan al patch y se ponen a cero; DRIFT y los ajustes GROOVE y FX se conservan. El GFP no guarda el estado LFO SYNC, por lo que se rechaza la exportación mientras está activo. Una prueba vuelve a importar el archivo exportado y verifica los valores y el engine.

## Edición de osciladores y PATCH

Una página OSC/PATCH expone formas, niveles y ratio de los dos VCO, pulse width, ruido, MONO, SYNC y los destinos del VCO 3 como parámetros independientes automatizables. Los valores proceden de los mismos rangos GFP del firmware; al abrir proyectos anteriores se inicializan desde su GFP y los nuevos conservan sus ediciones en el estado del DAW. Cambiar MONO limpia las notas activas para evitar voces atascadas. La exportación escribe estas ediciones en el GFP antes de aplicar las macros. La prueba valida interfaz, automatización, persistencia y exportación; no compara con hardware.

## Edición ENV/LFO y voz

La página ENV/LFO expone decay y sustain de amplitud; la envolvente de filtro, drive y velocity; forma, velocidad, división y destinos del LFO; glide, UNISON y DRIFT. Los valores base se guardan por separado de las macros del GFP, que se aplican al preparar el sonido; attack y release generales permanecen en la página VOICE. El estado del DAW y la exportación GFP preservan las ediciones. Los proyectos anteriores con GFP inicializan los parámetros desde el preset. Cambiar MONO limpia las voces; cambiar UNISON afecta notas nuevas. La prueba cubre enlace visual, automatización y persistencia; no es una prueba A/B física.

## Edición GROOVE y FX

PUMP, ARP y CHORD y las cinco etapas de SYNTH FX tienen parámetros propios de ARP y páginas de edición compartidas visualmente con los demás motores. Cada motor mantiene sus valores independientes. Los valores base se guardan en el DAW y en el GFP; MOVE se aplica a los efectos activos al renderizar y se incorpora al exportar. La migración inicializa proyectos anteriores a partir de su GFP. Se comprueban enlace de interfaz, aislamiento, persistencia y salida, sin A/B físico.
