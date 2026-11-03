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
