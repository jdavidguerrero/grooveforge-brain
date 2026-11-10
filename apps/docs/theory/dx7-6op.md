# DX7 6-OP: modelo y contrato de integración

Base: ADR-008 §4 (enmienda ADR-005). Código **custom**, C++17 sin Arduino/JUCE. FM 4-OP (engine 4,
Studio, GFP 2–5) conserva su DSP, controles y serialización. DX7 6-OP usa el identificador **7** (6 se
reserva para HYBRID en el navegador del VST); no aumenta las matrices de seis engines del Studio.
La consola selecciona `dx7_6op`; el firmware ofrece selección explícita por su consola serie.

## Modelo audible

Seis senos modulan **fase**, no la frecuencia instantánea. Una salida moduladora en Q24 representa
ciclos de fase. Los 32 algoritmos son buses con suma/reemplazo y una ruta de feedback declarada por
algoritmo. El feedback usa el promedio de las dos salidas anteriores de su extremo de retorno,
con profundidad 0–7. No siempre está en OP6: algoritmos como 2/8/9/28/30 lo ubican en otro operador;
4/6 recorren varios operadores. Esto precisa la simplificación de ADR-008 («feedback en operador 6»).

Cada EG tiene R1–R4/L1–L4: avanza a L1, L2, sostiene L3 hasta soltar, luego alcanza L4. Los rates
son índices no lineales; las pendientes ascendentes aceleran desde un piso (ataque), las descendentes
son lineales en dominio logarítmico. La curva de output level también es logarítmica con tabla en el
tramo bajo. No equivale a un ADSR en milisegundos. Level scaling por grupos de tres semitonos desde
breakpoint+17, profundidad y curva independiente izquierda/derecha; rate scaling y velocity afectan
cada operador antes del EG. Ratio/fixed, coarse/fine/detune, transpose y pitch EG se calculan antes de
convertir log-frecuencia a incremento. El LFO global por instancia tiene seis ondas, S/H determinista,
delay, sync, PMD/PMS y AMD/AMS por operador. Operator on/off es una extensión de estado, no parte VCED.

## Procedencia y aritmética

Referencia local, **sin red**: `refs/msfa/app/src/main/jni` (Google, Apache-2.0). No se usa Dexed (GPL),
ni voces ROM Yamaha. Referencias conceptuales: Yamaha DX7 Owner's Manual, secciones FUNCTION/EDIT y
MIDI DATA FORMAT; Chowning, JAES 21(7), 1973. Fuentes verificables aquí: MSFA `dx7note.cc` (frecuencia,
velocity y scaling), `env.cc` (curvas EG), `pitchenv.cc`, `lfo.cc`, `fm_core.cc` (tabla de algoritmos),
`fm_op_kernel.cc` (interpolación y feedback), `sin.*`, `exp2.*`, `freqlut.*` y `patch.cc` (VMEM→VCED).
Se conserva atribución por archivo y copia de LICENSE; un manifiesto detalla las adaptaciones.

Se porta aritmética fija Q24 y LUT de MSFA para preservar curvas e índices y evitar seis `sin()` por
muestra. Se define el wrap de fase con unsigned (evitar overflow C++); salida float sólo en la frontera.
Los controles se calculan cada **64 muestras**, con ganancia interpolada. `edit_patch` conserva fase,
etapa de EG, notas y sustain; `set_patch` carga un preset y limpia notas. Código nuevo: API portable,
polifonía, parser SysEx estricto/checksum, empaquetado inverso, GFP, metadatos, adaptadores y pruebas.
Se añaden AMD/AMS y feedback multioperador (ausentes en MSFA). AMD/AMS es una aproximación nueva:
profundidades nominales 0/13,5/23,5/47 dB, con interpolación en dominio logarítmico; **falta calibrarla
contra hardware**. No se promete identidad con MSFA en esos casos ni con el DAC/quantización físico. Sin oversampling: aliasing propio de PM.

Ubicación: `firmware-teensy/src/engines/dx7_6op*`, igual que `dx7_core`; el CMake del preview compila la
misma fuente, sin duplicarla. `msfa/` es una adaptación privada del núcleo, no otro motor público.

## Relojes, CPU y límites

Teensy recibe `AUDIO_SAMPLE_RATE_EXACT` (44,1 kHz nominales), Pi/VST `prepare(48000)`. LUT de frecuencia,
unidad de pitch EG, LFO y pendiente de EG se parametrizan por **instancia**; ningún cambio de frecuencia
en una instancia altera otra. EG de amplitud de MSFA se calibra a 44,1 kHz y escala incrementos por
44100/fs. El tamaño del bloque del host no altera los ticks de 64 muestras.

Estimación previa (no medición): 36 operadores × 44100 × 35–100 ciclos ≈ 9–27 % de 600 MHz, seis voces
sin FX; en Pi A76 2,4 GHz sería ≈ 2–7 % de un núcleo si el costo en ciclos fuese comparable (no lo es
garantizado). La medición Mac se adjuntará con metodología; no se transforma en medida Teensy/Pi.
Resultados y comandos reproducibles: [validación](dx7-6op-validation.md). Gate físico pendiente: `AudioProcessorUsageMax`, xruns y escucha. `AudioStream`, asignación de bloques,
I2S/USB/SGTL5000 son **Teensy Audio Library oficial**; el sintetizador y su adaptador son custom.

## Presets y controles

GFP para DX7 6-OP: firma `GFP`, versión **6**, byte engine **7**, seguido de VCED (155 bytes) y máscara
de operadores (bits 0–5, OP1 en bit 0), checksum propio. API separada rechaza GFP antiguo o ambiguo;
los lectores existentes de GFP 2–5 no cambian. VCED/VMEM enumeran OP6 primero; la máscara enumera OP1
primero. SysEx acepta VCED 155 (`F0 43 0n 00 01 1B`) y VMEM 4096 (`F0 43 0n 09 20 00`), checksum
Yamaha y F7, exactamente un mensaje; un índice 0–31 elige voz de banco sin reservar memoria dinámica.

Tabla estática para `14-synth-host-contrato.md`: identificador, nombre, unidad, rango, default, step,
kind, opciones separadas por `|`, página y banco. Son **151 controles** (`params`, `param_count` en
`dx7_6op_params.h`); `ENGINE_NAME = "dx7_6op"`, `ENGINE_ID = 7`. Bancos de ≤8: OPS por operador (R/L, frecuencia/nivel/sensibilidad),
ALGO/FB, PITCH EG, LFO, KEY SCALING por operador. El setter usa la misma tabla para validar y recortar.

## Validación prevista y contradicciones

Tests nativos: integridad de 32 rutas, EG vs MSFA, cuatro curvas de scaling, VCED↔VMEM y mensajes
válidos/rotos, estabilidad/finite, polifonía/robo de voz, rates independientes, GFP y metadatos.
Golden: herramienta de prueba compila fuentes originales MSFA aparte del producto; renders de patches
propios, niveles RMS y espectros a igual fs, con casos separados para extensiones. Bench Mac a 48 kHz,
128/256, seis voces; Pi lo ejecuta el usuario con `tools/gemelo-pi/run_on_pi.sh`.

`01` §4.1 y ADR-005 registran textos históricos incompatibles sobre 4/6 operadores y
`AudioSynthWaveformPWM`: un generador PWM no implementa operadores senoidales PM ni feedback de muestra.
Se señala la contradicción; **00–06 no se editan**. FM 4-OP conserva la edición simple del Brain Studio;
el motor 6-OP tiene entrada propia, sin reinterpretar presets anteriores. El DX7 original tiene 16 voces;
aquí el presupuesto/API mantienen seis. No se emula su DAC analógico ni ruido de conversión.

## Uso y APIs

- Preview: target `gf_dx7_6op`, clase `gf::Dx7SixPreview`, `prepare(fs)`, `set_patch`, `edit_patch`,
  `note_on/off`, `set_sustain`, `set_pitch_bend`, `process_sample`. Sin heap/locks al renderizar.
- GFP: `gf::decode_engine_preset` distingue engine 7 de GFP 2–5; la representación `studio::Preset`
  antigua no se usa para guardar seis operadores. El GFP 6 mide **162 bytes**, checksum Yamaha de los
  161 anteriores. VCED conserva los 10 bytes del nombre. No se exporta la máscara al SysEx original.
- VST: activar **DX7 6-OP**; selector de páginas/bancos de ocho controles, automatización
  `dx7_6op.<id>`, IMPORT GFP/SYX y EXPORT GFP. **SYX voice 1–32** elige la voz antes de importar un
  banco; el índice de API es 0–31. GFP y VCED ignoran este selector. Estado DAW conserva parámetros.
- Firmware (los tres entornos): `@GF ENGINE dx7_6op` activa id 7; `@GF ENGINE studio` vuelve al panel
  original. `@GF DX7_PARAM op1.coarse 3` edita; `@GF DX7_BANK 12` selecciona índice VMEM antes de
  enviar SysEx por USB MIDI o host MIDI. `@GF DX7_GFP` exporta hex; `@GF DX7_GFP <hex>` carga GFP 6.
  Los bancos/sesiones del Studio siguen en GFP 2–5: el modo 6-OP se guarda por su archivo propio.
- Valores de metadatos en unidades DX7 crudas: algoritmo 0–31 (opciones 1–32), detune 0–14 (7 = 0),
  transpose 0–48 (24 = 0); el editor traduce las etiquetas. Un host convierte `options` a lista JSON.

La cuantización del EG y el detune son los modelos de MSFA, no simulación de los chips del Yamaha.
Con OSC SYNC apagado se conserva la fase de la ranura; no se simula el oscilador corriendo mientras
la voz está inactiva. No hay 16 voces, DAC de 12 bits, ni medición física de AMD/AMS o lazos 4/6.
