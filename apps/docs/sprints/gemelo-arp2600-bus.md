# ARP 2600: bus GROOVE y SYNTH FX

## Teoría y alcance

Las cuatro voces del ARP se mezclan en mono y alimentan el mismo bus portable que usan los demás pilotos del VST: DRIVE, TAPE, chorus/phaser, delay y reverb, con PUMP, ARP y CHORD. El GFP proporciona el estado y los parámetros de cada etapa. La entrada MIDI pasa primero por el generador de acordes y el arpegiador; las notas resultantes alimentan el ARP 2600. PUMP se sincroniza al transporte del DAW. La ganancia de UNISON permanece en el motor ARP para que no se aplique dos veces.

El wow/flutter de TAPE afecta la frecuencia de los osciladores en la muestra siguiente, siguiendo el acoplamiento de los demás pilotos. La referencia de reloj externa PUMP se conserva en el GFP pero este piloto sigue el transporte del DAW. Esta entrega cubre reproducción desde preset; la edición individual y exportación se abordan por separado. La prueba verifica señal procesada, activación de ARP y restauración del estado; no compara con hardware. Referencias: ADR-006, `05-fx-architecture.md`, `prophet_preset.cpp` y `49-studio-brain.cpp::groove_tick`.

## Exportación GFP de los controles disponibles

El VST exporta desde el GFP ARP importado, preservando los campos que aún no se editan. El cutoff, resonancia, ataque, release y RING actuales reemplazan los valores correspondientes. Como los controles del VST contienen valores con macros aplicadas, GRIT, POCKET y MOVE se incorporan al patch y se ponen a cero; DRIFT y los ajustes GROOVE y FX se conservan. El GFP no guarda el estado LFO SYNC, por lo que se rechaza la exportación mientras está activo. Una prueba vuelve a importar el archivo exportado y verifica los valores y el engine.
