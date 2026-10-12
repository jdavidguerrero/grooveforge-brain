# Prompt para Claude Design — Iteración: páginas OSC y LFO

> **Cómo usarlo:** pegar lo que está debajo de la línea en el mismo proyecto de Claude Design donde
> están *GrooveForge Studio UI v3* y *GrooveForge Studio FX Viz*. No hace falta adjuntar nada si el
> chat es el mismo; si es uno nuevo, adjunta `claude-design-2026-10-09/GrooveForge Studio UI v3.dc.html`.
>
> Última actualización: 2026-10-12

---

## Objetivo

Agregar a **Studio UI v3** las dos páginas de edición que faltan: **OSC** (B1) y **LFO / MOD** (B3).
Hoy B1 y B3 están en la barra inferior pero no abren nada. Todo lo demás de la v3 queda igual:
tokens, tipografía IBM Plex Mono, grilla de 8 px, TopBar, BottomBar3, reglas de render y estilo
de ilustración.

## Contexto que ya está definido (no cambiar)

- 800×480 a 1×, LVGL 8.4 por software en una Raspberry Pi 3B, solo rellenos sólidos, 30 fps.
- La interfaz ya corre en el hardware: PERFORM, AI, SPECTRUM, ENV, FX MAIN (con las vistas de FX
  Viz), ENGINE SELECT, PRESETS y los overlays. Úsalas como referencia de estilo.
- Mapeo de controles de la v3 para estas páginas (tabla de controles):

| Página | P1 | P2 | SHIFT + P1 / P2 |
|---|---|---|---|
| OSC | WAVE | DETUNE | OCTAVE / MIX |
| LFO | RATE | DEPTH | SHAPE / DESTINATION |

- ENC L / R siguen siendo CUTOFF / RES en todas las páginas de SYNTH. B5 MODE pasa a FX. B6 es SHIFT.
- **B1 abre OSC y B1 otra vez vuelve** a la última vista (PERFORM / AI / SPECTRUM). Lo mismo con B3 y
  LFO. Es la misma regla que B2 con ENV.

## Gramática de página: la misma de ENV (03a / 03b)

Las dos páginas siguen el patrón de ENV para que las tres páginas de edición se sientan una familia:

- Gráfica arriba: **16,48 · 768×208**, fondo SURFACE, marco LINE.
- Cuatro celdas de valor abajo: **16,264 · 4 × 186×112, gap 8**, con tag **P1 / P2 / SHIFT P1 / SHIFT P2**.
  El par activo va en TEAL_DIM con borde superior TEAL_PLUS; el par inactivo atenuado.
- Variante **SHIFT OFF** y **SHIFT ON** de cada página, como 03a / 03b.
- TopBar con el título `OSC · <ENGINE>` / `LFO · <ENGINE>`. BottomBar3 con B1 (OSC) o B3 (LFO / MOD)
  activo y los valores de P1 / P2 en la fila 2.

## Página OSC

Los engines no tienen ni la misma cantidad de osciladores ni los mismos parámetros. Esto es lo que
el firmware implementa hoy (no lo que tiene el instrumento original):

| Engine | Osciladores | Parámetros de oscilador que existen |
|---|---|---|
| MODEL D | VCO 1 · VCO 2 · VCO 3 + NOISE | WAVE por VCO · DETUNE VCO1↔VCO2 (cents) · INTERVAL del VCO 3 (semitonos) · nivel de cada VCO y del ruido |
| JUNO-106 | DCO + SUB + NOISE | WAVE del DCO · nivel del SUB (square fijo, −1 oct) · nivel del ruido |
| PROPHET-5 | OSC A · OSC B | WAVE de A y de B · INTERVAL de B (semitonos) · CROSSMOD · MIX A/B |
| OB-6 | VCO 1 · VCO 2 + SUB + NOISE | WAVE de 1 y de 2 · INTERVAL del VCO 2 · MIX de VCO1 / VCO2 / SUB / NOISE |
| DX7 | OP 1 · OP 2 · OP 3 · OP 4 (FM de 4 operadores, **firmware en desarrollo**, ver ADR-005) | por operador: RATIO (coarse + fine) · LEVEL · envolvente propia; globales: ALGORITHM (1–8) · FEEDBACK del OP 4 |
| ARP 2600 | VCO 1 · VCO 2 + RING MOD + NOISE | WAVE de 1 y de 2 · RATIO del VCO 2 · RING MIX · nivel del ruido |

Por eso el mapeo WAVE / DETUNE / OCTAVE / MIX de la tabla de controles es **genérico**: cada engine
pone sus propios nombres en P1 / P2 / SHIFT P1 / SHIFT P2 (por ejemplo, en PROPHET-5 P2 es INTERVAL y
SHIFT P1 es CROSSMOD). La página tiene que funcionar como **plantilla alimentada por datos**: hasta 4
parámetros por oscilador seleccionado, con nombre y unidad que vienen del engine, y celdas que se
ocultan o atenúan si el engine tiene menos.

- **NAV elige qué oscilador editan P1 / P2** (VCO 1 → 2 → 3, da la vuelta; con un solo oscilador, NAV
  no hace nada). A diferencia de ENV, donde NAV solo resalta, aquí el resaltado define el destino.
  Muestra claramente cuál está seleccionado (en la gráfica y en una fila de selección, por ejemplo
  `VCO 1 · VCO 2 · VCO 3 · NOISE`).
- La gráfica muestra la **forma de onda de cada oscilador** (2 ciclos), el seleccionado en TEAL_PLUS
  y los otros en TEAL atenuado. El detune o el intervalo se ve como el corrimiento entre trazas; el
  mix como el alto relativo o barras de nivel por oscilador.
- WAVE es discreto (SAW, TRI, SQR, PULSE… según el engine): muéstralo como nombre + ícono chico,
  no solo como número.

Pantallas:
- **11a** OSC · MODEL D · SHIFT OFF · VCO 2 seleccionado
- **11b** OSC · MODEL D · SHIFT ON
- **11c** OSC · DX7: FM de 4 operadores con 8 algoritmos, como el Yamaha DX100 / TX81Z. NAV elige el
  operador (OP 1–4); propuesta: P1 / P2 = RATIO / LEVEL del operador seleccionado y SHIFT P1 / P2 =
  ALGORITHM / FEEDBACK. La gráfica tiene que mostrar el **algoritmo** (qué operador modula a cuál, cuáles
  son portadoras y el lazo de feedback del OP 4) y el nivel de cada operador. Diseña también la vista
  de los 8 algoritmos para elegir uno.
- **11d** OSC · JUNO-106: 1 DCO + SUB + NOISE (qué pasa con NAV cuando hay un solo oscilador).
- **11e** OSC · PROPHET-5: 2 osciladores con CROSSMOD, para validar que la plantilla aguanta nombres
  distintos en cada celda.

## Página LFO / MOD

> **Estado del firmware:** los engines todavía no tienen LFO; la única modulación hoy es el envelope
> del filtro. Esta página es la **especificación visual** del LFO que se va a agregar al firmware:
> diseña lo que debería existir, no lo que hay. Anota en las notas (sección **d**) los campos que el
> motor tendrá que exponer.

- La gráfica muestra la **forma del LFO (2 ciclos)** con un **punto de fase vivo** que se mueve a
  30 Hz (la fase llega del motor) y una indicación de a dónde va: el destino y cuánto se mueve
  (DEPTH).
- RATE en Hz, con opción de **sincronía al BPM** (1/4, 1/8, 1/16, 1/8T…): el BPM ya lo detecta la AI y
  está en la TopBar. Muestra cómo se ve cada caso.
- SHAPE: SINE · TRI · SAW · SQR · S&H (S&H como escalones).
- DESTINATION: CUTOFF · PITCH · AMP · PWM. Ojo: en PERFORM, P2 ya es "LFO → CUTOFF depth". Define
  si la página LFO edita ese mismo valor cuando el destino es CUTOFF (recomendado: sí, un solo valor
  de profundidad por destino) y anótalo.
- El soft-key se llama **LFO / MOD**: deja espacio para una segunda capa de modulación a futuro, pero
  **no** diseñes la matriz de modulación ahora. Basta con anotar dónde entraría.

Pantallas:
- **12a** LFO · SHIFT OFF · RATE libre en Hz · destino CUTOFF
- **12b** LFO · SHIFT ON · SHAPE / DESTINATION · RATE sincronizado a 1/8

## Entregables

1. Las pantallas 11a–11e y 12a–12b a 800×480, con el mismo formato de la v3 (título, subtítulo y
   panel de notas al costado).
2. **Notas por pantalla**, con las mismas cuatro secciones de la v3:
   - **c**: controles (qué hace cada control en esa página).
   - **a**: animación y regiones que se repintan, con su tamaño y tasa.
   - **d**: datos que necesita del motor (nombres de campos, como `osc[n].wave`, `lfo.phase`).
   - **s**: specs de geometría (x, y, w, h).
3. Actualizar la tabla de controles de la v3 y el component sheet si aparece algún componente nuevo
   (fila de selección de oscilador, ícono de forma de onda, punto de fase).
4. Para el render: todo con rects sólidos, sin antialiasing. Lo único que se anima a 30 Hz es el
   punto de fase del LFO; las formas de onda solo se redibujan cuando cambia un parámetro.

No cambies las pantallas existentes ni los tokens. Si algo de la v3 choca con estas páginas,
anótalo en lugar de resolverlo.
