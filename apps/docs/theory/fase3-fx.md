# Fase 3 de carácter sonoro: FX estéreo en la cadena del synth

> **Fecha:** 2026-10-27 · **Estado:** en curso · **Depende de:** `caracter-sonoro.md` §4 fase 3,
> `variants/studio/theory/05-studio-synth-fx.md` (cadena actual), `13-studio-bancos.md` (almacenamiento).

## 1. Qué falta

La cadena del synth es **BIT → TAPE → CHORUS → MODAL** en dos copias mono (L y R) sin nada que cruce los canales:

- No hay **delay sincronizado**. El de los plucks y leads de house melódico es el 1/8 con punto en **ping-pong**: exige
  dos líneas que se cruzan (L → R → L), y la cadena actual no puede cruzar canales.
- No hay **phaser**, el movimiento de los pianos y las cuerdas de disco.
- La reverb MODAL es un resonador por canal; para los pads falta una **plate estéreo** de verdad.

## 2. La cadena nueva

**DRIVE → TAPE → CHORUS / PHASER → DELAY → REVERB (MODAL / PLATE) → PUMP.** Cinco lugares; cada uno con on / off, MIX
y tres parámetros (P1 · SHIFT P1 · SHIFT P2, como hasta ahora).

| Lugar | P1 | SHIFT P1 | SHIFT P2 | MIX |
|---|---|---|---|---|
| **DRIVE** (antes BIT) | AMOUNT | TONE | **TYPE: OVERDRIVE · DIST · CRUSH** | dry / wet |
| TAPE | DRIVE | WOW | FLUTTER | dry / wet |
| CHORUS | DEPTH | RATE | **TYPE: CHORUS · PHASER** (antes DRIFT, que queda fijo) | dry / wet |
| **DELAY** (nuevo) | **TIME: 1/16 · 1/8 · 1/8. · 1/4 · 1/4.** (al tempo del reloj común) | **FEEDBACK** 0–90 % | **TONE**: oscuro ← plano → fino | nivel del eco |
| REVERB | DECAY | **TYPE: CAMPANA · GUADUA · CRISTAL · MADERA · PLATE** | SIZE | dry / wet |

### DRIVE (reemplaza al BIT)

Juan probó el BIT y mete demasiado ruido; para su música sirve más la saturación (2026-10-27). TAPE ya cubre la cinta,
así que DRIVE cubre lo que falta:

- **OVERDRIVE**: tanh suave con un poco de asimetría (armónicos pares, carácter de tubo). Bajos funk y leads melódicos.
- **DIST**: más ganancia y recorte asimétrico más duro. Indie tech, ácido, bajos con mordida.
- **CRUSH**: el bitcrusher **domado** (como mucho 8 bits y 11 kHz): textura lo-fi ocasional sin el ruido del BIT.

TONE = pasabajos de un polo después del recorte (2 → 18 kHz). El nivel se compensa con AMOUNT. El BIT sigue existiendo
como FX del modo FX (BIT SCULPT); solo sale de la cadena del synth.

### DELAY ping-pong

Un objeto estéreo compartido por las dos cadenas (2 entradas, 2 salidas). La suma L + R entra a la línea izquierda; la
salida izquierda alimenta la línea derecha y la salida derecha vuelve a la izquierda con el FEEDBACK. Las dos líneas
tienen el mismo tiempo, así que los ecos se alternan L, R, L…

- **TONE** a la entrada de la línea (señal + realimentación), como la cabeza de una cinta: el primer eco pasa una vez
  y cada repetición otra más. Pasabajos de un polo de 18 kHz → 1.5 kHz por debajo del centro, pasaaltos de 20 Hz →
  700 Hz por encima; en el centro, sin filtro.
- **Tiempo** = beats × 60 / BPM del reloj común (MIDI o interno). Al cambiar el tempo o la división, el tiempo de
  lectura se desliza (≤ 1 muestra por muestra): un glissando corto, como una cinta, en lugar de un click.
- Memoria: hasta 1.0 s por canal en int16 (176 KB, en RAM2). 1/4. a 90 BPM = 1.0 s.
- MIX = nivel del eco sobre la señal seca (que no se atenúa: el delay es un efecto de envío).

### PHASER

6 etapas pasa-todo de primer orden barridas por un LFO triangular (200 Hz – 3 kHz, exponencial) con realimentación de
0.5: las muescas del phaser de disco (MXR Phase 90 / Small Stone, de 4 a 6 etapas). Cada canal tiene su propia copia,
con el LFO de R desfasado 90° para que se abra en estéreo. RATE y DEPTH son los del CHORUS. Costo: 6 pasa-todo por
muestra y por canal; el coeficiente del barrido se recalcula cada 16 muestras.

### PLATE estéreo

`AudioEffectFreeverbStereo` (Teensy Audio Library, oficial): entrada mono (L + R) / 2, salida L y R decorrelacionadas.
DECAY → *roomsize*; SIZE → amortiguación (SMALL más oscura, LARGE más brillante). Junto a ella sigue la MODAL; un
mezclador por canal elige cuál sale. Cuando la PLATE no está elegida, el objeto se **desactiva** (no corre su update):
no gasta CPU.

## 3. Presets: formato 5 sin perder nada

- **Formato 5 = el 4 + el lugar DELAY al final** (on, MIX y 3 parámetros: 17 bytes). Los cuatro lugares viejos quedan
  en su posición del archivo; el índice del DELAY en el patch es el 4 (la UI lo dibuja en el orden de la cadena:
  0 · 1 · 2 · 4 · 3).
- **Migración al leer un preset de formato 2–4:** el lugar BIT pasa a DRIVE en TYPE CRUSH con AMOUNT = la reducción de
  bits que tenía (TONE plano); DELAY apagado con valores neutros; el 3.er parámetro del CHORUS
  (DRIFT) pasa a TYPE = CHORUS; el TYPE de la REVERB se vuelve a normalizar de 4 a 5 pasos (el material no cambia).
- **Archivos de los bancos (`/bank<e>.gfb`) y de las capas (`/layers.gfl`):** el lugar crece, así que el archivo
  también. Al cargar un archivo del tamaño viejo, los lugares se reubican en RAM al nuevo paso; los presets adentro
  siguen en formato 4 y se leen migrados. Al guardar, se escribe el formato nuevo. Las capas pasan a `LAYER_FORMAT` 2
  (mitades en formato 5) y siguen leyendo las de formato 1.
- El bibliotecario lee y escribe el formato 5 y las capas de formato 2.

## 4. CPU

Presupuesto ≤ 60 % (`01` §5.2). Se mide en el hardware con una capa (dos engines) y los cinco FX encendidos, en las dos
reverbs. Si la PLATE no entra con una capa, se queda solo para un engine (se documenta).

## 5. Cómo se prueba

| Qué | Cómo |
|---|---|
| Ping-pong: un impulso sale en L a T, en R a 2T, en L a 3T, cada vez × FEEDBACK | test native |
| TONE oscuro baja la energía de alta frecuencia de cada eco; plano no la cambia | test native |
| DRIVE: los tres tipos acotados y sin NaN; CRUSH no baja de 8 bits | test native |
| Phaser: con DEPTH 0 las muescas quedan quietas (la salida no cambia con el tiempo); con DEPTH > 0, acotada y sin NaN | test native |
| Formato 4 → 5: un preset viejo se lee con DELAY apagado, CHORUS en TYPE CHORUS y la misma REVERB | test native |
| Banco y capas del tamaño viejo se leen enteros y se guardan en el nuevo | test native |
| Pluck con delay 1/8. en ping-pong; pad con PLATE; piano con PHASER | en el hardware |
| CPU con capa + 5 FX | `[stats]` en el hardware |
