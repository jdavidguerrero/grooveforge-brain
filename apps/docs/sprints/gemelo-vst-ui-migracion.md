# Gemelo VST: migración del editor a la referencia de diseño

> Theory doc antes del código (CLAUDE.md). Referencia: `GrooveForge VST.dc.html` (Claude Design, 2026-10-31), pantallas
> 1a–1k (SYNTH) y 2a–2h (HYBRID). Estado de partida y brechas: revisión del 2026-11-03 (`gemelo-vst-ui-studio.md`,
> sección "Texto y tipografía", y la comparación pantalla por pantalla de esa fecha).

## 1. Por qué por cortes

El editor actual es un layout lógico de 720 × 456 escalado ×1,67, con cada página posicionada a mano y pestañas
distintas por engine. La referencia cambia la estructura entera: cabecera, pestañas comunes, tira del panel Studio,
y páginas nuevas. Reescribirlo de una vez rompería todo a la vez y no se podría demostrar nada hasta el final.

Cada corte deja el plugin usable, con tests, y se puede ver en una captura (puerta de fase: "visible").

| Corte | Pantallas | Qué queda demostrable |
|---|---|---|
| **1. Estructura** | cabecera, pestañas comunes, tira del panel, 1j ENGINE SELECT, 1k FX · LATER | Se navega como en la referencia; las páginas viejas viven dentro del área de contenido |
| 2. PERFORM | 1a | Onda por el filtro, curva, perillas héroe, chips de GROOVE, P1/P2 de PERFORM |
| 3. OSC | 1b–1d (+ Juno, ARP, Prophet) | Sistema de paneles por engine |
| 4. ENV y LFO · MOD | 1e, 1f | ADSR editables en el formato de la referencia; arrastrar el LFO a un destino |
| 5. GROOVE y FX | 1g, 1h | Cinco columnas + TAP; unidades con interruptores y selectores en vez de sliders 0/1 |
| 6. Navegador | 1i | Rol, dialecto, rating, DESCARTADO, A/B, ● BRAIN / ○ VST ONLY |
| 7. HYBRID | 2a–2h | Selector A \| B, HOME con lista de capas y MORPH, presets y ENGINE de la mitad, + CUSTOM, SAVE LAYER, estados |

## 2. Corte 1: estructura

### Geometría (de la referencia, 1200 × 760 a escala 100 %)

| Zona | Rectángulo | Contenido |
|---|---|---|
| Cabecera | 0–64 | línea de modo de 2 px arriba; GROOVEFORGE / BRAIN · VST (x 16); selector de modo (x 152: SYNTH 72, HYBRID 80, FX · LATER 96, alto 32); engine con miniatura (x 424, 176 × 40); barra de preset ◀ nombre ▶ ≡ (x 616, 320 × 40); bancos A–D (x 952, 24 × 32 c/u); BPM y CPU (x 1064); medidor L/R (x 1160, 8 × 40 c/u) |
| Pestañas | 64–112 | seis pestañas de 96 × 47 desde x 16 (barra inferior de 2 px en la activa); a la derecha, SAVE / IMPORT GFP / EXPORT GFP (alto 32, y 72) |
| Contenido | 112–600 | 1200 × 488 |
| Tira del panel | 600–760 | "PANEL STUDIO · página" y KEYS ▴; fila 1 (y 24): ENC L 112, B1–B4 96 c/u, ENC R 112; fila 2 (y 96): P1 152, B5 72, NAV 168, B6 72, P2 152; macros (x 680, 504 de ancho): DRIFT · GRIT · POCKET · MORPH · MOVE con su CC |

En HYBRID: la pestaña PERFORM se llama HOME; la cabecera muestra las dos miniaturas (A y B) y "LAYER nn / total"; el
selector A | B va en la fila de pestañas (x 600); SAVE LAYER se suma a las acciones.

### Qué hace cada elemento en este corte

| Elemento | Comportamiento | Fuente |
|---|---|---|
| Selector de modo | SYNTH: sale de la capa (deja sonando el engine que estaba). HYBRID: entra a la capa. FX · LATER: muestra 1k y no cambia el audio | referencia, cambio 05 |
| Engine de la cabecera | Clic abre 1j ENGINE SELECT: seis tarjetas; elegir carga el preset A·00 de fábrica de ese engine | 1j |
| ◀ / ▶ | Preset de fábrica anterior / siguiente del engine (o capa anterior / siguiente en HYBRID) | barra de preset |
| ≡ | Abre el navegador existente sobre pestañas + contenido (1200 × 536) | 1i |
| Bancos A–D | A activo; B–D se dibujan deshabilitados: el plugin todavía no guarda bancos de usuario | — |
| BPM · CPU | BPM del host; CPU = tiempo de `processBlock` / duración del bloque, promedio móvil | — |
| Pestañas | PERFORM · OSC · ENV · LFO · MOD · GROOVE · FX para todos los engines | cambio 03 |
| Tira: ENC L / ENC R | Cutoff y resonancia del engine activo (en HYBRID, de la mitad en foco); se editan arrastrando | panel ADR-004 |
| Tira: B1–B4 | OSC · ENV · LFO · MOD · PRESET: cambian de pestaña; B4 abre el navegador | ADR-004 |
| Tira: B5 / B6 / NAV / P1 / P2 | B5 MODE alterna SYNTH ⇄ HYBRID; B6 SHIFT y NAV solo informan. P1 / P2 dicen NO FUNCTION hasta el corte de cada página | — |
| Tira: macros | DRIFT, GRIT, POCKET, MOVE ligados a los parámetros del engine activo; MORPH solo en HYBRID (en SYNTH: OFF, como en el firmware, theory/09 §6) | cambio 02 |
| KEYS ▴ | El teclado existente; amplía la ventana hacia abajo | — |

### Páginas existentes dentro del contenido

Hasta su corte, cada pestaña muestra la página vieja equivalente, escalada en el área de contenido. Cuando una
pestaña junta dos páginas viejas, aparece un sub-selector arriba a la izquierda del contenido:

| Pestaña | Model D / OB-6 / ARP | Juno | DX7 | HYBRID (según A \| B) | Prophet |
|---|---|---|---|---|---|
| PERFORM / HOME | VOICE | VOICE | VOICE | HOME · FILTER | sus 4 perillas |
| OSC | OSC | OSC | OPERATORS | A OSC / B OSC | — (deshabilitada) |
| ENV | ENV / LFO | ENV | VOICE | A VOICE / B VOICE | — |
| LFO · MOD | LFO | LFO | LFO | A LFO / B LFO | — |
| GROOVE | GROOVE | GROOVE · UNISON | GROOVE | GROOVE | — |
| FX | FX | FX | FX | FX | — |

La página MACROS de cada engine desaparece: sus macros están en la tira (cambio 02 de la referencia). Las casillas
"JUNO PREVIEW", "DX7 PREVIEW"… dejan de verse; el engine se elige con ENGINE SELECT y el modo con el selector.

### Decisiones abiertas de la referencia que este corte toma

- **Cian de HYBRID:** se usa la propuesta (HY #2FA8C8, HY_PLUS #7AD4EA, HY_DIM #0F4352), que el editor ya usaba.
  Juan puede cambiarla.
- **MON_TEXT #8E8EA2** para texto secundario (pasa AA); MON #5A5A6E solo para trazos y deshabilitados.
- **Miniaturas de engine:** la referencia usa las ilustraciones de la UI v3 (SVG). En este corte van glifos vectoriales
  simples por engine; portar las ilustraciones es parte del corte 2.

### Tests

- La ventana abre a 1200 × 760; las seis pestañas comunes existen en los siete modos y llevan a la página vieja de la
  tabla; las que no aplican (Prophet) están deshabilitadas.
- El selector de modo cambia `hybrid_preview`; FX · LATER no cambia el audio.
- ENGINE SELECT carga el preset A·00 del engine elegido.
- Las macros de la tira mueven los parámetros del engine activo.
- Siguen pasando todos los tests de páginas existentes.

**No cubre:** el contenido nuevo de cada página (cortes 2–7) ni la escala tipográfica fina de la referencia.

### Resultado del corte 1 (2026-11-03)

Hecho: cabecera, pestañas comunes, tira del panel con macros y ENC editables, ENGINE SELECT con las ilustraciones de la
UI v3 portadas a JUCE (`studio_shell.h::draw_engine`, mismo dibujo que `illo()` de `studio-data.js`), FX · LATER, BPM y
CPU del callback en la cabecera. 12/12 tests; el arnés de audio sigue sin regresiones.

Decisiones que salieron al implementar:
- **Las ilustraciones sí entraron en este corte:** en la referencia son procedurales (rectángulos, círculos, líneas),
  así que se portaron tal cual en vez de usar glifos provisionales.
- **MACRO va dentro de GROOVE** como sub-página (MACRO es una parada de GROOVE en el firmware, theory/09 §7): ahí
  quedan las profundidades GRIT RES, GRIT MOD y POCKET VEL, que no caben en la tira.
- **Foco de HYBRID y página:** gana el que cambió último. Si cambia el foco (A | B, CC), la página pasa a esa mitad;
  si otro control abre la página de la otra mitad, el foco la sigue (como "la misma tecla otra vez" del panel).
- **Las pestañas viejas siguen existiendo pero ocultas:** las usan los tests de cada página. Los tests que exigían
  verlas ahora comprueban la pestaña o sub-pestaña nueva equivalente.
- **Bancos B–D** se dibujan deshabilitados: el plugin todavía no tiene bancos de usuario (tema del corte 6).

Siguiente: corte 2, PERFORM (1a).

## 3. Corte 2: PERFORM (1a)

### Geometría (contenido 112–600)

| Elemento | Rectángulo (relativo al contenido) | Qué muestra |
|---|---|---|
| Onda | 16, 16, 752 × 248 | "OSC → FILTER · forma · A2 110 HZ"; a la derecha, "LFO forma rate → CUTOFF" en púrpura si el LFO mueve el cutoff |
| Curva del filtro | 16, 272, 752 × 200 | respuesta en dB, relleno dim y trazo hi, línea blanca en el cutoff con su valor; eje 100 · 1K · 10K |
| Chips | 784, 16 | ■ PUMP · 1/4 · □ ARP · OFF · □ CHORD · OFF, y HOST + BPM a la derecha |
| Perillas héroe | 784 y 992, 56; 192 de ancho | CUTOFF (ENC L) y RESONANCE (ENC R): perilla de 160 px, valor en 40 px y unidad en 16 |
| Fila P1 / P2 | 784, 352 | "PAGE · P1 · P2 · ⇧ P1 · ⇧ P2" y cuatro perillas: FLT ENV AMT, LFO → CUTOFF, ⇧ GLIDE, ⇧ DRIVE |

### De dónde sale cada dibujo

La referencia usa una aproximación (`filterDb` de `studio-data.js`). La cabeza de Studio ya tiene la versión fiel
(theory/08 de Studio, `apps/studio-head/src/ui/screen_perform.cpp` y `dsp_math.h`), y se porta tal cual:

- **Onda:** la forma principal de cada engine a 110 Hz (Model D: SAW · TRI · SQR · PULSE · SHARK · NARROW; Juno: forma
  + SUB una octava abajo; OB-6: SHAPE continua saw → pulse; DX7: OP2 → OP1 con el índice desde el LEVEL; ARP: SAW · TRI
  · SQR · PULSE · SINE; Prophet: SAW, porque su página OSC no existe en el plugin) pasada por un **ladder TPT de 4 polos
  con realimentación** (Zavalishin, *The Art of VA Filter Design*, cap. 5) al cutoff y la resonancia actuales. Arranca 4
  ciclos antes para que el filtro se asiente. Es ilustrativa: no es el audio medido.
- **Curva:** 24 dB/oct con el pico de resonancia; en el OB-6, el SEM SVF de 2 polos con su morph LP ↔ notch ↔ HP o BP.
- **LFO:** si el LFO va al cutoff, la onda y la curva se mueven con él (±4 octavas a profundidad máxima, la misma escala
  que el firmware). Solo entonces se redibuja a 30 Hz; si nada cambia, no se redibuja.
- **Resonancia normalizada:** el dibujo trabaja en 0–1. Se obtiene del parámetro con el rango de cada engine
  (`RESO_RANGE` del firmware: 0,1–5; OB-6 0,7–5; DX7 0,7–3).

### Interacción

- Arrastrar en la curva: horizontal = CUTOFF (eje logarítmico), vertical = RESONANCE (referencia, nota de 1a).
- Perillas héroe: arrastre vertical, Shift = fino, doble clic = valor por defecto. El arco púrpura MOD muestra el rango
  que recorre el LFO.
- Chip: clic = encender / apagar esa parada de GROOVE; doble clic = ir a GROOVE.
- La tira muestra P1 = FLT ENV AMT (⇧ GLIDE) y P2 = LFO → CUTOFF (⇧ DRIVE) en esta página.

### Qué no aplica a cada engine

- DX7: no tiene FLT ENV AMT ni LFO → CUTOFF en el plugin; esas perillas aparecen OFF.
- Prophet: sin parámetros de envolvente de filtro, LFO, GLIDE, DRIVE ni GROOVE en el plugin; solo cutoff y resonancia.
  Su pestaña ENV muestra las cuatro perillas viejas (ATTACK y RELEASE) hasta el corte 4.
- HYBRID: su pestaña HOME es la 2a (corte 7); este corte cubre SYNTH.

### Tests

- PERFORM muestra la página nueva en los seis engines y la vieja ya no se ve.
- La onda es finita y cambia al mover el cutoff; con resonancia alta aparece el pico en la curva.
- Arrastrar en la curva mueve el cutoff; las perillas P1 / P2 están ligadas al parámetro del engine.
- Un chip enciende PUMP.

### Resultado del corte 2 (2026-11-03)

PERFORM nuevo en los seis engines de SYNTH (`perform_view.h`): onda filtrada y curva portadas de la cabeza de Studio,
perillas héroe con arco MOD del LFO, chips de GROOVE (clic = on / off, doble clic = GROOVE), fila P1 / P2 ligada al
engine y la tira mostrando P1 / P2. 12/12 tests (bloque nuevo del corte 2: página por engine, onda finita que sigue al
cutoff, P1 / P2, chip de PUMP, arrastre en la curva); el arnés de audio sigue sin regresiones.

- Al abrir el editor arranca en PERFORM (en el DX7 la página vieja por defecto era OPERATORS y abría en OSC).
- El Prophet usa PERFORM para cutoff y resonancia y su pestaña ENV muestra las perillas viejas (ATTACK, RELEASE).
- La animación por LFO usa una fase propia del dibujo a 30 Hz, no la del audio (como en la cabeza de Studio).

Siguiente: corte 3, OSC (1b–1d).

## 4. Corte 3: OSC (1b–1d, más Juno, ARP 2600 y OB-6)

### El sistema de paneles (nota SYSTEM de 1b)

Una fila de paneles en el área de contenido (16, 16, 1168 × 456, separación 8):

- **Panel completo** (flex 1) para cada oscilador u operador; **panel angosto** (176) para una fuente que es solo
  nivel o una cantidad del engine entero.
- Cada panel: cabecera de 24 (nombre 16 semibold, subtítulo, etiqueta a la derecha), onda de 112, grilla de celdas
  (2 columnas en un panel completo angosto, 4 si el panel mide ≥ 560; 1 en el angosto) y al pie el mapeo del panel físico.
- Celdas (componente `VstParam` de la referencia), 88 de alto: **perilla** de 48 px (pista 4 px dim, arco 4 px hi desde el
  inicio o desde el centro si es bipolar, puntero blanco 3 px), **segmentos** (opción elegida rellena en hi, texto BG; el
  resto con borde dim) e **interruptor** (segmentos OFF | ON, para que el estado se lea como texto).
- **Foco:** en el hardware NAV mueve el panel seleccionado (borde de 2 px hi y "● NAV"). En el VST todos los paneles se
  editan directo; un clic en un panel solo mueve el foco que siguen P1 / P2 en la tira.

### Qué va en cada engine

Los nombres, rangos y unidades salen de la tabla de slots del firmware (`GF_OSC_SLOT` / `GF_MOD_SLOT` en
`gf_params.h`) y de los nombres de la cabeza (`osc_catalog.cpp`), como pide la nota SLOTS de la referencia. Donde la
referencia dibuja otra cosa, gana el firmware (01 §contratos técnicos) y queda anotado:

| Engine | Paneles | Diferencias con la referencia |
|---|---|---|
| Model D (1b) | VCO 1 (WAVE · LEVEL · PW), VCO 2 (WAVE · DETUNE · LEVEL), VCO 3 (WAVE · INTERVAL · LEVEL · FINE), NOISE angosto (LEVEL · WHITE / PINK) | WAVE tiene las **6 formas** del firmware (SAW · TRI · SQR · PULSE · SHARK · NARROW), no 3. PW es del engine: va en VCO 1 |
| OB-6 (1c) | VCO 1 (SHAPE · SUB · LEVEL · PW), VCO 2 (SHAPE · INTERVAL · LEVEL · PW · DETUNE), X-MOD angosto (VCO 2 → 1 · SYNC), NOISE angosto, SEM angosto (MORPH · BP MODE) | SHAPE es **continua** saw → pulse (fidelidad §11.3), no un selector de 3. El resto de X-MOD (FILT ENV, FREQ 1, FILTER, PW 1) sigue en LFO · MOD, donde lo pone el firmware |
| DX7 (1d) | OP 1–4 (RATIO · LEVEL · WAVE W1–W8 · DETUNE), ALGORITHM angosto (diagrama · ALGORITHM 1–8 · FEEDBACK 0–7) | Se suman WAVE y DETUNE por operador (están en el plugin; en el panel son la parada MOD). Sin RATIO fino, como el hardware (ADR-005) |
| Juno-106 | DCO (WAVE · PW · RANGE · HPF · VCA · CHORUS), SUB angosto, NOISE angosto (LEVEL · BBD HISS) | Las funciones de SHIFT del DCO van dentro del panel DCO |
| ARP 2600 | VCO 1 (WAVE · PW · LEVEL), VCO 2 (WAVE · RATIO · LEVEL · SYNC), RING angosto, NOISE angosto (LEVEL · COLOR) | La referencia no lo dibuja; se arma como dice su nota (SYNC como interruptor en VCO 2) |
| Prophet-5 | — | La pestaña OSC sigue deshabilitada: el plugin no tiene su página de osciladores |

**Envolventes de los operadores del DX7:** no caben en el panel y son de ENV (1e, corte 4). Hasta entonces la pestaña
ENV del DX7 suma una sub-página "OP EG" con la página vieja de operadores, así ninguna función se pierde.

### Ondas de los paneles

Ilustrativas, sin filtro (el filtro ya se ve en PERFORM): 2 ciclos a ancho completo.

- VCO: la forma elegida con `perform_math::osc_sample` (la misma de PERFORM). OB-6 mezcla saw → pulse según SHAPE.
  SUB: cuadrada una octava abajo. NOISE: ruido de un generador fijo (dibujo estable). RING: producto de VCO 1 × VCO 2.
  X-MOD: VCO 1 con su fase modulada por VCO 2 según la cantidad.
- DX7: cada operador con `dx7_wave` del firmware (W1–W8) a su RATIO. Nota OPERATORS de 1d: los **moduladores
  muestran su propia onda** y las **portadoras su salida FM**: y_op(φ) = wave(ratio·φ + Σ I_m · y_m(φ)), con el índice
  I desde el LEVEL del modulador (la misma escala que PERFORM). La red de modulación es la tabla `MODS` de
  `dx7_core.cpp` copiada en la vista; un test la compara con `dx7_is_carrier` para que no se desincronice.
- El subtítulo del operador dice su papel en el algoritmo: CARRIER o "MOD → OP n".

### Diagrama de algoritmo

Cajas de operador de 28 × 22; portadoras abajo con borde mc, la seleccionada (operador en foco) con borde de 2 px hi.
Cada operador va en el nivel de su camino más largo hasta una portadora; en x, las portadoras se reparten y cada
modulador se centra sobre sus destinos (si dos caen en el mismo lugar se separan). Las portadoras se unen por una barra
abajo; OP 4 lleva el lazo de feedback. Clic en el diagrama abre el selector de 8 algoritmos (como 11c2 de la UI v3);
clic en uno lo elige.

### Tira del panel

En OSC: P1 / P2 muestran los dos parámetros del panel en foco (los mismos del pie de cada panel) y NAV la lista de
paradas ("VCO 1 · 2 · 3"); B1 queda encendido.

### Tests

- OSC muestra la página nueva en Model D, Juno, OB-6, DX7 y ARP; Prophet sigue con la pestaña deshabilitada.
- Cada celda está ligada a un parámetro del engine y editarla cambia ese parámetro (perilla, segmento, interruptor).
- Las ondas son finitas; la de VCO 1 cambia al cambiar WAVE.
- DX7: los papeles CARRIER / MOD coinciden con `dx7_is_carrier` en los 8 algoritmos; el selector elige el algoritmo.
- Clic en un panel mueve el foco y la tira muestra su P1 / P2.
- DX7: ENV tiene la sub-página OP EG.

**No cubre:** HYBRID (corte 7, 2f usa estos mismos paneles), la página MOD de cada engine (corte 4).

### Resultado del corte 3 (2026-11-03)

OSC nuevo en Model D, Juno, OB-6, DX7 y ARP 2600 (`osc_view.h`): fila de paneles con su onda, celdas ligadas a los
parámetros del engine (perillas, segmentos, interruptores y valores con pasos), foco por clic que siguen P1 / P2 y NAV en
la tira, y en el DX7 el diagrama de algoritmo con el selector de los 8. 12/12 tests (bloque nuevo del corte 3: tabla de
algoritmos contra `dx7_is_carrier`, celdas ligadas en los cinco engines, ondas finitas, WAVE que edita y redibuja, foco y
tira, selector de algoritmo, salida FM de la portadora, interruptor SYNC, OP EG en ENV del DX7).

Decisiones que salieron al implementar:
- **Filas de alto variable:** una fila de segmentos mide lo que ocupan sus botones (≈ 56) y una de perillas 88; con
  filas fijas de 88, VCO 3 del Model D (WAVE + 3 perillas) se montaba sobre el pie. Los paneles de 280–559 de ancho usan 3
  columnas; los segmentos ocupan la fila entera salvo los de 2–4 opciones cortas.
- **WAVE del DX7 es un valor con pasos** ("W1 OF 8"; clic = siguiente, arrastre o rueda), no 8 segmentos: no caben con
  RATIO, LEVEL y DETUNE en el ancho de un operador.
- **El pie de cada panel sigue la tabla del firmware** (P1 = slot 0, P2 = slot 1, ENC L / R = slots 2–3, ⇧ = 4–7). Lo
  que en el panel físico vive en LFO · MOD (SYNC y COLOR del ARP, el resto de X-MOD del OB-6) lo dice el pie.
- **Arnés de audio:** sin cambios de audio (el corte solo toca el editor). El chequeo de guardado marca `LFO Sync` en
  HYBRID: el arnés le escribe 0,43 a un parámetro booleano y al restaurar vuelve como 0. El audio restaurado es idéntico
  (dif 0); es un artefacto del arnés, no del plugin.

Siguiente: corte 4, ENV y LFO · MOD (1e, 1f).

## 5. Corte 4: ENV (1e) y LFO · MOD (1f)

### ENV

Dos paneles de 576 × 456 (x 16 y 608): AMP ENV y FILTER ENV. Cada uno: gráfica de 544 × 232 con la envolvente
(ataque recto, decay y release curvos, sostén fijo de 0,5 s para que se vea), cuatro puntos de 8 × 8 y una fila de
perillas debajo.

- **Puntos (nota HANDLES de 1e):** el pico = ATTACK (x); la esquina del decay = DECAY (x) y SUSTAIN (y); el fin del
  sostén = SUSTAIN (y); el fin = RELEASE (x). Shift = fino (×0,2). El punto elegido se pinta blanco con su valor.
- **Eje común (nota SCALE):** los dos paneles comparten el eje, ajustado a la envolvente más larga en pasos de 0,6 s.
  Mientras se arrastra el eje queda quieto; si no, el punto se escaparía del cursor.
- **Perillas:** ATTACK · DECAY · SUSTAIN · RELEASE con la curva de cada parámetro del plugin (la referencia pide log
  1 ms–10 s; los parámetros ya tienen su propia curva y se respeta), más **VEL → AMP / VEL → FILTER**, que vivían en la
  página vieja y son de la envolvente (theory/10 de Studio).
- **Por engine:**
  - Model D, Juno, OB-6 y ARP: ATTACK y RELEASE del amplificador son los parámetros compartidos `attack` / `release`
    (los que usa cada engine); el resto es del engine.
  - DX7: AMP (dx7.amp_*, VEL = dx7.velocity) y, en el segundo panel, **OP EG**, con un selector 1–4 que cambia de
    operador. La sub-página "OP EG" del corte 3 deja de hacer falta.
  - Prophet: el plugin solo tiene ATTACK y RELEASE; DECAY y SUSTAIN aparecen OFF y FILTER ENV vacío.

### LFO · MOD

- **LFO (izquierda):** SHAPE (SINE · TRI · SAW · SQR · S&H), la onda de 544 × 176 con el punto de fase, RATE, SYNC,
  DIVISION (1/4 · 1/8 · 1/16 · 1/8T) y, en el Juno, DELAY. Con SYNC encendido RATE se atenúa y DIVISION se activa (nota
  PHASE DOT). La nota de arriba dice "FREE · x S PER CYCLE" o "SYNC · división · HOST bpm".
- **Destinos (derecha):** CUTOFF, PITCH, AMP y PWM, una profundidad cada uno (0–100 %, como el firmware: la referencia
  dibuja ±, el parámetro es unipolar). El firmware tiene tres destinos por engine: CUTOFF, PITCH y PWM, salvo el DX7 que
  tiene AMP en lugar de PWM y no tiene CUTOFF en el plugin. Los que no existen se ven "NOT ON THIS ENGINE".
  CUTOFF dice cuánto barre: "1240 HZ · SWINGS 950–1530 HZ" (±4 octavas a profundidad máxima, la escala del firmware).
- **Arrastrar el LFO (notas DRAG SOURCE y DROP):** el chip "⋮⋮ LFO" se arrastra sobre un destino; el destino válido se
  marca con borde púrpura punteado y una línea punteada sigue al cursor. Al entrar la profundidad pasa a +25 % y seguir
  arrastrando en vertical la ajusta; soltar la fija; salir sin soltar devuelve el valor anterior.
- **MOD:** sub-página con las paradas MOD del engine (GF_MOD_SLOT), con los mismos paneles de OSC: Model D MODULATION
  (8 controles), OB-6 X-MOD (6), DX7 KBD / EG (KBD LEVEL · KBD RATE · D2 RATE; WAVE y DETUNE ya están en OSC), ARP PATCH
  (6). El Juno no tiene parada MOD propia (DELAY va en el LFO, HISS en OSC).

**Arreglo del corte 3:** la página vieja de OSC también tenía estos controles MOD; desde el corte 3 quedaban tapados por
los paneles nuevos. Ahora están en LFO · MOD, donde los pone el firmware.

### Sub-páginas

| Pestaña | Sub-páginas |
|---|---|
| ENV | ENV; más VOICE (la página vieja) en OB-6, ARP y DX7, porque su unísono todavía no tiene lugar (GROOVE, corte 5) |
| LFO · MOD | LFO; más MOD en Model D, OB-6, DX7 y ARP |

En Model D y Juno todo lo de la página vieja de ENV ya tiene lugar (envolventes y VEL en ENV, FLT ENV AMT · DRIVE ·
GLIDE en PERFORM, LFO en LFO, DRIFT en la tira), así que no hay VOICE.

### Tests

- ENV liga los parámetros del engine; arrastrar el pico cambia ATTACK y el eje no se mueve; arrastrar la esquina del
  decay cambia SUSTAIN; en el DX7 el selector de OP EG cambia de operador; OB-6 conserva VOICE y Model D no la necesita.
- LFO liga sus parámetros; soltar el chip sobre un destino deja +25 %; el DX7 va a AMP y no a CUTOFF; el resto tiene PWM.
- MOD muestra las paradas del engine con todas sus celdas ligadas; el Juno no tiene MOD.

### Resultado del corte 4 (2026-11-03)

Hecho todo lo de arriba (`env_lfo_view.h`; MOD reutiliza `OscPage` con `build(kind, state, 1)`). 12/12 tests; arnés de
audio sin cambios (el corte solo toca el editor).

- **Prophet:** su pestaña ENV nunca había abierto: el editor devolvía siempre a PERFORM a los engines sin páginas viejas
  por pestaña. Corregido; B2 también queda activa en el Prophet.
- **Textos:** algunos "·" se veían como "Â·" por literales UTF-8 asignados sin `utf8()`; corregidos.
- Pendiente para cuando haya HYBRID (corte 7): la envolvente fantasma de la otra mitad (2c) y el arco MOD púrpura en
  todas las perillas del destino.

Siguiente: corte 5, GROOVE y FX (1g, 1h).

## 6. Corte 5: GROOVE (1g) y FX (1h)

### GROOVE

Cinco columnas de 224 de ancho (x 16 + i · 232): PUMP · ARP · CHORD · UNISON · MACRO. Cada una lleva su nombre, el
interruptor de la parada arriba a la derecha (MACRO no tiene: dice DEPTHS) y sus celdas en 2 columnas. Rangos y
nombres de `GF_GROOVE_SLOT` (gf_params.h) y `GROOVE_STOP` de la cabeza (osc_catalog.cpp):

| Parada | Celdas (P1 · P2 = las dos primeras, como en el panel) |
|---|---|
| PUMP | DEPTH % · SHAPE (SMOOTH · EXP · KICK) · RATE (1/4 · 1/8 · 1/2) · OFFSET ±50 MS · RELEASE 30–400 MS · CLOCK (AUTO · MIDI · INT) |
| ARP | MODE (UP · DOWN · UP-DN · RANDOM · SMART) · DIV (1/4 · 1/8 · 1/16 · 1/8T · 1/16T · 1/8.) · GATE 5–100 % · OCT 1–3 · SWING 50–75 % · LATCH |
| CHORD | TYPE (MEMORY · KEY) · INV (ROOT · 1ST · 2ND · 3RD) · STRUM 0–100 MS · KEY (24) |
| UNISON | VOICES (×1 · ×2 · ×3 · ×6) · DETUNE 0–50 CT |
| MACRO | GRIT RES · GRIT MOD · POCKET VEL (DRIFT · GRIT · POCKET · MORPH · MOVE están en la tira) |

- El **DX7** no tiene CLOCK en PUMP en el plugin: esa celda no aparece.
- El **Model D** es mono: la columna UNISON dice "MONO ENGINE" y no tiene celdas.
- **TAP:** en el VST el reloj es el del host; la caja TAP de MACRO muestra "HOST bpm" y no toma golpes (en el panel físico
  TAP sí existe porque el reloj puede ser interno).
- Con UNISON aquí, la sub-página VOICE de ENV (corte 4) ya no hace falta: todo lo de las páginas viejas de VOICE tiene
  lugar nuevo.

### FX

La cadena real del synth: **cinco** unidades en orden DRIVE → TAPE → CHORUS → DELAY → REVERB (theory/fase3-fx.md §2),
columnas de (1168 − 4 · 8) / 5 con una "›" entre ellas. La referencia dibuja seis, con un PHASER aparte: en el firmware
PHASER es un TYPE de CHORUS, así que va como selector ahí (punto abierto ya anotado en la revisión del diseño).

Cada unidad: número, nombre, interruptor ON / OFF, una línea con el tipo elegido, un dibujo de 72 de alto y sus celdas.
Los parámetros del plugin van normalizados 0–1; los valores se muestran como en la cabeza (`sfx_format` de
screen_sfx.cpp) y los discretos son segmentos (`GF_SFX_STEPS`):

| Unidad | Celdas |
|---|---|
| DRIVE | DRY/WET · AMOUNT % · TONE % · TYPE (OVERDRV · DIST · CRUSH) |
| TAPE | DRY/WET · DRIVE ×1,0–10,0 · WOW % · FLUTTER % |
| CHORUS | DRY/WET · DEPTH % · RATE 0,10–10 HZ · TYPE (CHORUS · PHASER) |
| DELAY | DRY/WET · TIME (1/16 · 1/8 · 1/8. · 1/4 · 1/4.) · FEEDBACK 0–90 % · TONE (DARK · FLAT · THIN) |
| REVERB | DRY/WET · DECAY ×0,2–4,0 · TYPE (BELL · GUADUA · GLASS · WOOD · PLATE) · SIZE (S · M · L) |

Dibujos (ilustrativos, siguen a los parámetros): curva tanh del DRIVE según AMOUNT; seno con wow y flutter; dos senos
desfasados según DEPTH (o los notches del phaser); los ecos del DELAY cayendo con FEEDBACK; ruido con caída
exponencial según DECAY.

### Cómo se arma

Las dos páginas usan los mismos paneles de OSC (`OscPage::build(kind, state, page)` con page 2 = GROOVE y 3 = FX),
con dos agregados al panel: el interruptor en la cabecera y la altura del dibujo configurable (FX: 72). Las celdas
aprenden a mostrar un parámetro normalizado con pasos (`steps`) y con un formato propio (`format`). Clic en una
columna = foco: P1 / P2 de la tira muestran sus dos primeras celdas y NAV "STOP n / 5" o "UNIT n / 5".

### Tests

- GROOVE muestra las cinco paradas en Model D, Juno, OB-6, DX7 y ARP, con las celdas ligadas a los parámetros del engine;
  el interruptor de PUMP cambia `*.groove.pump.on`; DX7 sin CLOCK; Model D sin unísono.
- FX muestra las cinco unidades ligadas a `*.fx.*`; TYPE de REVERB en PLATE deja el parámetro en 1,0 (índice 4 de 5); el
  formato de TAPE DRIVE es ×1,0–10,0; el interruptor de una unidad la enciende.
- ENV ya no tiene sub-página VOICE.

**No cubre:** HYBRID (corte 7).

### Resultado del corte 5 (2026-11-03)

GROOVE y FX nuevos en Model D, Juno, OB-6, DX7 y ARP 2600 con los mismos paneles de OSC (`OscPage` páginas 2 y 3):
interruptor en la cabecera de cada parada o unidad, celdas con los rangos del firmware, segmentos sobre parámetros
normalizados con pasos (`CellSpec::steps`) y valores con el formato de la cabeza (`CellSpec::format`). 12/12 tests (bloque
nuevo: celdas ligadas en los cinco engines, interruptor de PUMP, DX7 sin CLOCK, Model D sin unísono, TYPE de REVERB en
PLATE = 1,0, formato ×10,0 de TAPE DRIVE, subtítulo con el tipo y tira por unidad). Arnés de audio sin cambios.

- **Las páginas viejas ya no se ven en SYNTH:** con GROOVE y FX, ninguna pestaña de los seis engines muestra una
  página vieja (la sub-página VOICE de ENV desapareció: el unísono vive en UNISON). Las páginas viejas siguen existiendo
  ocultas para HYBRID (corte 7) y para los tests de cada página.
- **KEY de CHORD** es un valor con pasos (clic = siguiente, arrastre o rueda): 24 tonalidades no caben como segmentos.
- **Tira en GROOVE:** P1 / P2 son los slots 0 y 1 de cada parada en el firmware (PUMP: DEPTH · SHAPE; ARP: MODE · DIV;
  CHORD: TYPE · INV), no siempre las primeras celdas de la columna; la referencia mostraba RATE en P2 de PUMP.

Siguiente: corte 6, navegador (1i).
