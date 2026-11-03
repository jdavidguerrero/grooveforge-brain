# Gemelo VST — correcciones de la revisión del 2026-11-03

> Theory doc antes del código (CLAUDE.md). Sale de la revisión dinámica del commit `8fd2e58`: build limpio, 12/12 tests
> y un arnés externo con los 101 presets de fábrica en los 7 modos (44,1 / 48 / 96 kHz, bloques aleatorios,
> guardado/restauración, asignaciones dentro de `processBlock`). Cubre tres correcciones; el resto de las brechas de
> `gemelo-auditoria-motores-2026-11-01.md` sigue abierto.

## 1. La PLATE deja un offset DC permanente

### Qué se midió

Con la reverb PLATE encendida, cuando termina la cola la salida queda fija en un valor **negativo constante**, sin cruces
por cero: −0,0018 (−55 dBFS) en GF NOISE RISER, −0,0006 (−64 dBFS) en GF SUNRISE PLUCK. Pasa también con GF DARK DRONE
y GF NOCTURNE PAD. Con la reverb MODAL, el delay, el chorus, el tape o el drive, la cola llega a silencio digital.

### Por qué pasa

`PlateReverbPreview` es el Freeverb en enteros de 16 bits de la Teensy Audio Library (`effect_freeverb.cpp`, PJRC).
Las multiplicaciones en punto fijo se reescalan con un desplazamiento aritmético (`value >> 15`), que **redondea hacia
−∞**: −1 >> 1 sigue siendo −1. En un lazo con realimentación (los peines y los all-pass de Schroeder) un error de
redondeo con signo fijo no se cancela, y el estado queda atrapado en unos pocos LSB negativos, un *limit cycle* de
frecuencia cero (Oppenheim & Schafer, *Discrete-Time Signal Processing*, 3.ª ed., §6.9, "zero-input limit cycles").
La salida multiplica por 30 (`sat(output * 30, 0)`), así que esos LSB salen a −55…−64 dBFS.

El firmware usa el mismo código de PJRC, así que el Brain físico tiene el mismo residuo. **El gemelo es fiel en el
dominio digital.** La diferencia está después: la salida de línea del Brain (SGTL5000 → LINE_OUT) es **acoplada en
alterna** (condensadores en serie), así que el DC no llega al cable. En el VST no hay condensador, y el DC:

- se suma entre instancias y come headroom;
- produce un click al mutear la pista o cambiar de preset (salto de DC);
- impide que la pista llegue a silencio (el freeze o el render nunca terminan "limpios").

### Corrección

Un **bloqueador de DC de primer orden** a la salida del plugin, después de todo (incluida la preescucha):

    y[n] = x[n] − x[n−1] + R · y[n−1],   R = exp(−2π · fc / fs),   fc = 5 Hz

Es un pasa-altos de un polo y un cero en DC (Smith, *Introduction to Digital Filters*, "DC Blocker"). Con fc = 5 Hz:

| Frecuencia | Atenuación |
|---|---|
| 0 Hz (DC) | total |
| 20 Hz | −0,26 dB |
| 40 Hz | −0,07 dB |
| 100 Hz | −0,01 dB |

El kick y el sub (40–100 Hz) no se tocan de forma audible. El residuo de −55 dBFS cae con una constante de tiempo de
1/(2π·5) ≈ 32 ms. Modela, en primera aproximación, el acople en alterna de la salida física; el valor exacto del
condensador del Audio Shield no se midió, así que **no se afirma** que sea el mismo corte.

**Alternativas descartadas:**
- *Corregir el redondeo dentro de la PLATE* (sumar media unidad antes del `>>`, o redondear hacia cero). Elimina el
  residuo en origen, pero cambia el DSP compartido con el firmware y rompe la paridad digital: habría que cambiarlo en
  los dos lados y volver a validar en el hardware. Queda como opción para el firmware, no para esta corrección.
- *Un bloqueador solo en la PLATE.* Menos general: el acople en alterna del hardware afecta a toda la salida.

## 2. CC 74 y 71 en el DX7

El firmware (`49-studio-brain.cpp`, `control_change`) trata los CC 74 y 71 igual en todos los engines:

- CC 74 → `cutoff[e] = gf_cutoff_from_cc_norm(e, v/127)`; para el DX7 es log entre 500 Hz y 20 kHz (`GF_CUTOFF_RANGE[4]`).
- CC 71 → `resonance[e] = gf_denorm(RESO_RANGE[e], v/127)`; para el DX7, 0,7 + 2,3 · v/127.
- Después, `engine_filter` aplica MOVE (cutoff × `move_cutoff_mul`) y GRIT (`macro_resonance`).

En el plugin, `dx7.cutoff` (500–20 000) y `dx7.resonance` (0,7–3) tienen **el mismo rango** que el firmware, y
`apply_dx7_general` ya aplica MOVE y GRIT. Basta con llevar el CC al parámetro: el buzón de CC del DX7 (el que ya usan
DRIFT, GRIT, POCKET y MOVE) pasa de 4 a 6 entradas. El valor suena en el offset de la muestra donde llega el CC, y el
parámetro del host se actualiza después desde el hilo de mensajes, como las macros.

## 3. CC 74 y 71 en el Prophet

Mismo contrato del firmware, con `GF_CUTOFF_RANGE[2]` = 20–8000 Hz y `RESO_RANGE[2]` = 0,1–5. Diferencia con el DX7:
en el Prophet del plugin los parámetros `cutoff` y `resonance` **ya traen MOVE y GRIT aplicados** al importar el preset
(`import_preset`), y el motor no los vuelve a aplicar. Por eso el CC se convierte con las macros del preset cargado,
igual que hacen el Model D, el OB-6 y el ARP:

- cutoff = clamp(`gf_cutoff_from_cc_norm(2, v)` × `move_cutoff_mul(MOVE)`, 20, 8000)
- resonancia = 0,1 + 4,9 · `move_resonance(macro_resonance(v, patch), MOVE)`

Para eso el procesador guarda el preset Prophet aplicado (`prophet_base_`) en el hilo de audio, al aplicarlo.

**Limitación que no se corrige acá:** `ProphetPreview::set_parameters` limita la resonancia a ≥ 0,707, mientras el
firmware acepta desde 0,1. Por debajo de CC 71 ≈ 16 el VST suena igual que en 16. Es previo a esta corrección.

**Fuera de alcance:** las macros 14/15/16/19 en el Prophet. El Prophet del plugin no tiene parámetros de macros ni
editor completo; agregarlas es el corte vertical del punto 2 de la auditoría (parámetros, editor, exportación GFP), no
una corrección.

## 4. Cómo se prueba

| Qué | Prueba | Dónde |
|---|---|---|
| DC | GF NOISE RISER: tras 20 s, \|media\| de la salida < 1e-5 (antes −0,0018) | `plugin_smoke.cpp` |
| DC | La respuesta en 20–100 Hz sale de la fórmula de la tabla del §1 (filtro de un polo); no hay test de audio | — |
| DX7 CC 74/71 | El CC mueve `dx7.cutoff` / `dx7.resonance` al valor del firmware; 0 y 127 dan los extremos | `plugin_smoke.cpp` |
| Prophet CC 74/71 | Igual, con MOVE del preset aplicado | `plugin_smoke.cpp` |
| Regresión | Los 12 tests y el arnés de revisión (0 NaN, 0 asignaciones, bloque y estado idénticos) | build local |

**No cubre:** comparación con el hardware (el A/B físico queda para el final, por decisión de Juan).

## 5. Resultado (2026-11-03)

- 12/12 tests (incluye los tres bloques nuevos del smoke) y el arnés de revisión sin regresiones: 0 NaN, 0 asignaciones
  en `processBlock`, audio idéntico con bloques fijos y aleatorios y tras guardar/restaurar. CPU sin cambios.
- **DC:** la media de la salida es 0,0000 en los 101 presets de fábrica. GF NOISE RISER: de −0,0018 a 0.
- **Ubicación del bloqueador:** va sobre la salida principal **antes** de mezclar la preescucha. Aplicado después, su
  memoria (≈ 32 ms) arrastraba el audio de la preescucha al volver al sonido original; lo detectó el test
  "cancel preview fades back to original output". Se reinicia donde se reinician los engines (cambio de motor,
  `reset()`, All Sound Off), para que el cambio de motor siga dejando silencio exacto.
- **Lo que queda y no es error:**
  - La PLATE deja, en algunos presets, un tono de muy bajo nivel (≈ −78 dBFS) después de la cola: el mismo *limit
    cycle* del §1, ahora sin su componente continua. Viene del Freeverb en enteros de PJRC y está también en el
    hardware.
  - Con MOVE (CC 19) al máximo, la cola del delay se alarga mucho (GF DARK DRONE sigue en −65 dB a los 20 s):
    `move_sfx` sube el feedback hasta ~0,85. Es el diseño de la macro en el firmware.
