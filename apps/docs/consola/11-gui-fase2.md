# Consola: GUI fase 2

> 2026-11-09 · Demo desktop sin hardware · QML/PySide6 · ADR-007.
> Documento inicial escrito antes del código; resultado y revisión añadidos después.

## Alcance y cobertura

**35 entradas de revisión** a 1280×720, registro único en
`apps/consola/gui-console/registry.py`. Las primeras nueve conservan IDs y atajos.

| IDs | Pantallas / estados |
|---|---|
| 01–09 | Boot, ModeSelect, Toasts, Settings/Surface, LiveSession, Layer, Transition, Mixer, Hands |
| 10–20 | Search + teclado, LiveArrange, SequencerSteps, SequencerPianoRoll, Pads, BrainEngine, BrainHybrid, Kit, Library, Log, Routing |
| 21–30 | Session, Note/drum rack, Clip, Device, Mix, Browse, Scale, Repeat, Tempo, Motor |
| 31–35 | LiveSessionOffline, SessionNoLive, LiveSessionEmpty, LiveSessionLoading, NoteMelodic |

Extras del diseño: HYBRID (16), offline (05b/21b), empty/loading (notas MVP) y NOTE
melódica (notas Controller: reutiliza PADS con colores Live). Scale/Repeat/Tempo se
pueden abrir en ambos modos y restauran la pantalla de origen al cerrar.

**Diseños ausentes, omitidos sin inventarlos:** Settings Audio/Connections/System
solo aparecen como navegación en el export MVP; no hay diseño de su contenido.
Se conservan los resúmenes mínimos de fase 1, no se presentan como ajustes completos.
BRAIN solo tiene páginas ENGINE/HYBRID: OSC, FILTER, ENV·LFO, FX y PRESETS son
pestañas sin contenido entregado. No se inventaron páginas para ellas.

## Archivos, componentes y fuentes

- `Main.qml`, `main.py`, `registry.py`: navegación común, `--screen <name>` (también
  IDs legacy), `--list-screens`, Tab/Shift+Tab, F2 y selector de ambos modos.
- `theme/Theme.qml`, `Layout.qml`, `FontAsset.qml`: paleta, geometría y carga local
  condicional. Contrato 56/568/96; columnas x8+i160/w152; ventanas específicas.
- Nuevos componentes: Keyboard, BrowserList, ChoiceRow, PadGrid, PianoRoll,
  StepLane, ArrangementLane, BrainTabs, ParameterPage, ArcKnob, GraphPlot,
  Waveform, ActionButton, PressureBar, EnvelopeSketch y ReviewWindow.
  OptionCard admite también roll compacto y color Live para MOTOR.
- `fake_data.py`, `review_data.py`, `review_samples.json`: snapshots deterministas
  del HTML suministrado. Listas nuevas = QAbstractListModel con rol `entry`
  transportable (mapas/listas Qt); sin importar Python desde QML.
- `models.py`: timers de un solo disparo al recibir cambios; sin despertar listas
  inactivas cada 16 ms. `queue_update` sigue fusionando por fila, último valor gana.
  El engine posee el QObject de estado para evitar wrappers huérfanos al destruirlo.
- `fonts/`: Big Shoulders, Archivo y JetBrains variables, Plex Regular/Medium/
  SemiBold/Bold estáticas; licencias OFL incluidas. FontLoader carga todos los pesos
  Plex 400/500/600/700; textos variables fijan `wght` explícitamente, sin bold
  sintético. Archivo admite `wdth=125`. General Sans (ITF FFL) solo local, ignorada
  junto con `fonts/general-sans/`; jamás se añade a Git. README y fetcher actualizados.

## Qué funciona y qué es fake

**No hay integración real**, tampoco nueva integración MVP: fase 3 conectará solo
el MVP previsto. Ninguna pantalla conecta con motor/audio/Live/superficie/red.
No se generan archivos ALS, no se cargan sonidos ni se persisten ajustes.

Navegación, teclado (texto/borrar/espacio/cerrar), selección visual y soft keys son
locales. LAYER conserva cue/generate/accept/skip simulados; MOTOR modifica sus
opciones fake, sin escribir clips Live. Las listas mantienen las muestras del
export, no son una búsqueda real. Las páginas de dispositivo no controlan audio.

Piano roll: pads = ventana 8 pasos × 4 notas de escala; `pressPad(col,row)` simula
hardware. Encoders editan nota/posición/largo/velocity/microtiming; al editar una
nota del motor pasa a ser propia. Vary preserva las notas propias y no propone
notas encima de sus intervalos. Touch solo desplaza/zoom, nunca dibuja ni toca.
Shift+flechas desplaza la ventana; Steps permite velocity/probability/microtiming.
Mixer mantiene cuatro grupos fijos RHYTHM/BASS/MUSIC/FX y ocho encoders de canal.

## Revisión visual y diferencias pendientes

Se inspeccionaron todas las capturas contra geometría, contenido y colores de los
HTML, incluidas las nueve MVP con fuentes reales. La galería y el manifiesto con
la **lista completa de 35 PNG** están en
[screenshots/index.html](../../consola/gui-console/screenshots/index.html) y
[screenshots/manifest.json](../../consola/gui-console/screenshots/manifest.json).
[Registro por captura](../../consola/gui-console/screenshots/README.md).

| Capturas | Diferencias restantes respecto del export |
|---|---|
| 01–02 | Glyph y glows horneados, sombras simplificadas; autoentrada de 8 s desactivada para revisión determinista. Status de selector conserva muestra «Live not found» aunque Controller es accesible fake. |
| 03 | LiveSession completo detrás de toasts; export deja más vacío el panel informativo. |
| 04 | Surface con thumbs, presión y banda IR; no calibración, sliders operativos ni contenido completo en otras secciones. |
| 05–09 | LAYER usa cue/energía combinados y cuatro soft actions; Transition simplifica mini barras/footer; Mixer/Hands conservan las diferencias de fase 1 (sin reloj/medidores interpolados ni takeover). |
| 10 | Coincidencias resaltadas, filas y teclado según export; preview fijo y conteo de 38 con cuatro resultados de muestra; teclado 123 simula un dígito. |
| 11–14 | Arreglo sin reloj; gráfico de energía por segmentos; piano roll con navegación real y muestras fijas. PADS y FSR no reaccionan a hardware. |
| 15–17 | Ilustraciones de engines son sketches, no arte final; HYBRID simplifica teclado de split; arcos estáticos, envolventes triangulares. KIT mantiene alternativas y cue dashed. |
| 18–20 | Se compactaron columnas de Library para evitar solapamiento en 1280 px; Log usa etiquetas de rol sin cápsula rellena; Routing es visual, sin mutar ruteo real. |
| 21–26 | Colores y 6 escenas originales; acentos de selección/recording simplificados. Device usa chips uniformes, sin bancos reales. Browse mantiene preview fijo al seleccionar otro ítem. |
| 27–30 | Windows sin sombra CSS viva; Repeat alinea tiles al contrato de columnas. MOTOR usa soft actions y sus hints adaptados; progreso fake fijo. |
| 31–35 | Offline conserva resumen ROOM/NEXT de fase 1, en lugar del banner amber completo. Empty/loading y NOTE melódica materializan los estados descritos por notas, sin un canvas propio adicional. |

Sin blur vivo ni MultiEffect; Canvas solo para gráficos/arcos/trazos, halos PNG y
OpacityAnimator donde aplica. Las capturas usan renderer software; no son prueba
de equivalencia pixel a pixel con Chromium ni de rendimiento GPU en Pi.

## Contradicciones señaladas (no se cambian specs)

- Notas LAYER llaman hardware a ACCEPT/GENERATE/SKIP y asignan pares cue/energy;
  07-gui §5 los exige como soft keys. LAYER/MOTOR reservan cuatro acciones, como MVP.
- CSS de ocho fracciones en 1264 px da 151 px; notas QML exigen 152 y pitch160:
  se sigue el contrato solicitado; última columna termina en x1280, sin margen8.
- Scrim 70% en Design System frente a 72% en notas MVP; se conserva 72% en Transition.
- Scale/Tempo 960×504 frente a Transition 960×488: se conservan tamaños específicos.
- Diseño BRAIN FM 6-OP frente a ADR-005 de cuatro operadores: contenido visual
  conservado; no se redefine el synth. TopBar enum de secciones tampoco incluye
  SCENE 4 aunque Controller lo pasa: la GUI muestra el texto del export.
- Piano roll declara YOU14/ENGINE6, pero sus datos contienen 16/6 (22 notas).
  Se conserva la muestra y leyenda entregadas. Su marco de altura32% ocupa ocho
  filas cromáticas de25; §4 pide cuatro notas de escala. Edición de pads sigue §4,
  marco visual sigue el export. HANDS sigue mostrando C4/17cm con ladder C4/14cm.
- Library especifica detalle336 y columnas de tabla cuyos mínimos exceden el
  centro disponible. Se conserva toda la información compactando columnas.

## Verificación y entrega

`python3 -m pytest apps/consola/gui-console/tests -q` → **110 passed in 18.23s**,
PySide6 6.11.1 / Python 3.14.3; sin warnings QML.
[Salida completa](../../consola/gui-console/screenshots/test-results.txt).
Carga de todo el registro sin warnings QML; chrome y columnas de cada diseño;
modelos, coalescencia, edición protegida, navegación/touch, fuentes y fallback.
General Sans omite su prueba si falta; no requiere red para CI.
`tools/screenshot.py`: 35 PNG 1280×720, galería y manifiesto, falla si hay warnings.
La compuerta Pi5 de ADR-007 sigue pendiente antes de fase3: sin hardware no se
certifican FPS, p99, CPU ni latencia de superficie.

Commit documental intentado primero: `Unable to create .git/index.lock:
Operation not permitted`. Se deja todo **sin commit**, en `codex/gui-fase2`, sin
push; General Sans permanece ignorada. Orden previsto: doc → fuentes/registro →
pantallas/modelos → capturas/tests. Todos los mensajes previstos llevan
`Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` en párrafo final.
