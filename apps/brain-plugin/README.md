# GrooveForge Brain Prototype · VST3 experimental

Instrumento experimental para probar el flujo DAW usando DSP existente del Brain. **No es aún un gemelo completo**:
implementa cinco voces Prophet, importación GFP y parte de GROOVE y SYNTH FX. Un selector experimental incorpora
el E.PIANO inicial de DX7 con seis voces, carga GFP, GROOVE, SYNTH FX y edición/automatización de operadores,
voz, LFO, UNISON, DRIFT, PUMP, ARP, CHORD, GRIT, POCKET, MOVE y SYNTH FX. Las macros de un GFP importado se
incorporan a los controles efectivos y sus posiciones editables comienzan en cero. La exportación GFP incorpora las macros a los valores
efectivos; con LFO SYNC activo no se exporta porque el formato GFP no almacena ese estado.
DX7 también acepta CC 14 (DRIFT), 15 (GRIT), 16 (POCKET) y 19 (MOVE); los cambios se aplican en el offset MIDI y
se reflejan en los parámetros del DAW desde el hilo de mensajes.
Un piloto inicial de Juno ofrece seis voces, DCO, sub, filtro y chorus estéreo, con MIDI sustain y pitch bend.
Carga GFP Juno y aplica OSC, ruido blanco basado en PJRC, envelopes, filtro y chorus. Las páginas OSC, ENV, LFO, UNISON, GROOVE, FX y MACROS permiten editar y automatizar los controles disponibles del piloto Juno. Las posiciones de GRIT, POCKET y MOVE se incorporan a los valores efectivos al exportar GFP. Aplica glide, UNISON y DRIFT del preset. Incluye LFO/PWM sincronizable, GROOVE y SYNTH FX del GFP, además de exportación GFP con LFO Sync apagado; el chorus admite frecuencias del DAW hasta 384 kHz en las pruebas nativas. Un piloto OB-6 ofrece seis voces, carga parcial de GFP, dos VCO, ruido y morph/BP del filtro SEM; incluye SYNC, X-MOD, glide, UNISON, DRIFT, LFO y SYNTH FX del GFP, incluida la modulación de afinación del tape. Incluye ARP, CHORD y PUMP del GFP mediante el bus compartido, con páginas de edición y automatización para OSC/X-MOD, ENV/LFO, GROOVE y FX, y exportación GFP con LFO SYNC apagado. Las macros GRIT, POCKET y MOVE también se pueden editar y automatizar; MOVE afecta los efectos activados. La presión de canal MIDI modula cutoff y vibrato en el siguiente tick de control. CC 14, 15, 16 y 19 controlan DRIFT, GRIT, POCKET y MOVE; CC 74 y 71 controlan cutoff y resonancia antes de las macros. El piloto MODEL D aporta tres VCO, seis formas de onda, ladder, envolventes, glide y prioridad monofónica con sustain. Importa y exporta GFP de forma parcial; reproduce ruido blanco/rosa, VCO3 KBD/LO, seguimiento de filtro, overdrive, modulación y LFO, DRIFT, GROOVE y SYNTH FX. Las páginas OSC/MOD, ENV/LFO, GROOVE, FX y MACROS permiten editar y automatizar sus controles; admite CC 14, 15, 16, 19, 71 y 74. La fidelidad frente al hardware queda pendiente de las pruebas A/B finales. ARP 2600 está disponible como piloto de cuatro voces con dos VCO con hard sync, ring modulation, ruido blanco–rosa, filtro, envolventes, modo mono, glide, unison, drift y sustain; admite importación y exportación GFP parcial, así como GROOVE y FX del preset; la página OSC/PATCH permite editar y automatizar osciladores, ruido y modulación; la edición detallada de envolventes, GROOVE y FX sigue pendiente. HYBRID sigue pendiente.
Su sonido no está validado contra hardware. Ver
`apps/docs/sprints/gemelo-vst-prototipo-prophet.md`.

El piloto admite sustain CC 64 y pitch bend ±2 semitonos; incluye envolvente de filtro y voice spread del Prophet.
El botón **IMPORT GFP** acepta presets Prophet v2–v5 y guarda el archivo completo en el proyecto del DAW. La carga es
una preescucha: incluye la etapa de salida, DRIVE, TAPE con wow/flutter, PHASER estéreo, PUMP, ARP, CHORD/STRUM y
UNISON, chorus, delay ping-pong y reverbs PLATE y MODAL. MODAL usa biquads flotantes: falta comparar su nivel y
cuantización con el Brain. La macro DRIFT también está implementada. CHORD usa la memoria inicial del firmware: los acordes aprendidos en el
hardware no están en el GFP. El LFO puede seguir el BPM del DAW; la fase en seek/loop queda pendiente.
La interfaz inicial muestra cuatro controles y la identidad oscura de Studio; la organización A/B completa está
pendiente. JUCE es una dependencia externa: no se descarga ni se modifica automáticamente. La configuración de desarrollo probada
es JUCE **8.0.3**, commit `5179f4e720d8406ebd1b5401c86aea8da6cc83c9`, reutilizado localmente desde GroovePilot.
Fijar una versión mantenida y confirmar la licencia aplicable antes de distribuir. JUCE es de doble licencia; elegir
el framework no decide si este proyecto será abierto o comercial.

```sh
cmake -S apps/brain-plugin -B /tmp/gf-brain-vst-build \
  -DGF_JUCE_DIR=/ruta/a/juce-src -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/gf-brain-vst-build -j 4
ctest --test-dir /tmp/gf-brain-vst-build --output-on-failure
```

El `.vst3` queda en `GrooveForgeBrainPrototype_artefacts/Release/VST3/` dentro del directorio de build. Instalarlo en
la carpeta de plugins del sistema es una acción separada: el build no copia archivos ni modifica Live. En macOS, si
el bundle aparece con firma inválida después de generar `moduleinfo.json`, firmar localmente al terminar y verificar:

```sh
codesign --force --sign - "/tmp/gf-brain-vst-build/GrooveForgeBrainPrototype_artefacts/Release/VST3/GrooveForge Brain Prototype.vst3"
codesign --verify --deep --strict "/tmp/gf-brain-vst-build/GrooveForgeBrainPrototype_artefacts/Release/VST3/GrooveForge Brain Prototype.vst3"
```

Esta es una firma local ad-hoc, no una notarización para distribución.

Para probar solo el DSP sin JUCE: configurar con `-DGF_BUILD_VST3=OFF`, compilar y ejecutar `ctest`.

El control **SRC GFP** conserva la fuente de reloj del preset para el Brain. El plugin usa el transporte del DAW y no cambia de fuente al editarlo; sin transporte válido usa el fallback del piloto.

Juno admite CC 14 (DRIFT), 15 (GRIT), 16 (POCKET) y 19 (MOVE); se aplican en el offset MIDI y se reflejan en los parámetros del DAW desde el hilo de mensajes.
