# Consola: GUI fase 1

> 2026-11-09 · Demo desktop sin hardware · QML + PySide6 (ADR-007).

## 1. Estructura y propósito

`apps/consola/gui-console/`: `Main.qml` compone chrome y nueve pantallas;
`theme/` concentra tokens y geometría; `components/` contiene TopBar, SoftKeys,
GridCell, OptionCard, MixerStrip, Toast y Window. `models.py` define el contrato;
`fake_data.py` lo alimenta; `main.py` arranca; `tools/screenshot.py` captura.
La GUI GroovePush queda intacta: su resolución y sus modelos de Live son distintos.

## 2. Tema y composición

Theme es singleton QML: paleta Brain (22 colores), paleta GroovePilot (7), nueve
roles con colores completos/dim (los ocho canales + FX), escala D-HERO a M-11,
espaciado 4/8/12/16/24/48, radios 6/8/10/12 y tiempos 80/150/300/4000 ms.
Los nombres semánticos conservan el significado del Design System: rol identifica
capa, teal indica sonido, violet propuestas, blanco cue/cola, amber desconexión.
Se soportan las codificaciones fill y stripe; fill es el default del export.
Geometría fija 1280×720: TopBar 0/56, contenido 56/568, SoftKeys 624/96;
columna i en 8+i×160, ancho 152. Boot ocupa toda la pantalla sin chrome visible.
Las cuatro tarjetas de Layer abarcan dos columnas cada una. Window: 960×488,
x160/y96; scrim solo sobre contenido. Texto visible en inglés.

FontLoader carga archivos locales solo si existen (evita warnings de recursos
inexistentes); sin fuentes usa sans/mono del sistema. `fonts/README.md` documenta
licencias y URLs; el script de descarga es opt-in y no se ejecuta en esta fase.
Esto mantiene el demo reproducible sin red, aunque cambia la métrica tipográfica.

## 3. Contrato de modelos para fase 3

Se expone `console` (QObject), sin importar Python desde QML. Todas las listas son
QAbstractListModel; roles con nombres estables y valores JSON/Qt transportables.

| Propiedad/lista | Roles / valores |
|---|---|
| `cells` (32, orden fila×8+canal) | roleName, title, subtitle, cellState, steps (16 enteros), level (0..1), row, channel |
| `channels` (8) | roleName, level, meter, peak, eq (3 bipolares), filterValue (-1..1), echo, verb, cue, selected |
| `options` (4) | letter, title, subtitle, optionState, energy (1..5), notes ({s,n,l}), hint, encoder |
| `softKeys` (8) | label, value, roleName, lit, turning, enabled, position, action |
| `groups` (4) | title (RHYTHM/BASS/MUSIC/FX), level, meter, members |
| `connections` (4) | title, status (ok/warn/down/off) |
| `hands` (2) | side, target, distance, inRange, value, rangeMin, rangeMax |
| `toasts` (hasta 2) | icon, message, value, tone, opacity |
| `screen` | 1..9: Boot, ModeSelect, Toasts, Settings, LiveSession, Layer, Transition, Mixer, Hands |
| estado | liveState (normal/offline/empty/loading), roomTitle, nextTitle, keyName, bpm, bar, phrase, section, selectedEncoder, selectedMode, settingsSection, encoding |

Comandos públicos: `setScreen(int)`, `setLiveState(str)`, `selectEncoder(int)`,
`turnEncoder(int,int)`, `pressEncoder(int)`, `triggerAction(str)`.
El adaptador futuro convierte `superficie.py` en comandos y snapshots del motor en
roles; no se fija aquí el transporte IPC. No se modelan datos de audio crudos.
`queue_update(row, values)` acumula por fila y `flush()` emite dataChanged por rango
una vez cada 16 ms: el último valor gana, sin resetear la lista por evento.
Los cambios de navegación son inmediatos; los eventos de controles se agrupan.

## 4. Reglas de rendimiento y verificación

Sin motor, audio, inferencia ni I/O pesado en el proceso GUI. Sin MultiEffect ni
blur; halos con gradientes simples y glyph de arranque con OpacityAnimator.
Los medidores reciben snapshots agrupados; no hay polling de hardware. Las capturas
son deterministas: sin reloj musical ni entrada automática durante el recorrido.
La compuerta Pi 5 de ADR-007 queda pendiente antes de fase 3: estas pruebas desktop
no demuestran 60 fps ni latencia de hardware.

Tests con `python3 -m pytest apps/consola/gui-console/tests`: conteos, roles,
integridad musical, coalescencia, carga de cada QML sin warnings y geometría.
`python3 apps/consola/gui-console/tools/screenshot.py` genera nueve PNG 1280×720.
Atajos 1–9 navegan; izquierda/derecha seleccionan encoder; arriba/abajo giran;
Enter pulsa; O/E/L/N muestran offline/empty/loading/normal; B cambia fill/stripe;
Escape vuelve. Layer: teclas 1–4 hacen cue, giro cambia energía; 5–8 Generate/Accept/Skip/Back.
Touch en soft keys ejecuta el mismo comando que un clic de encoder.

## 5. Alcance, conflictos y diferencias visuales

No incluye conexiones reales, audio, persistencia, calibración real, controller,
secuenciador, autoarranque de modo ni certificación de rendimiento Pi.
Ajustes muestra Audio/Connections/Surface/System como demo; EQ/envíos son visuales.

Contradicciones detectadas (no se cambian los documentos fuente):
- Layout notes Layer dice «ACCEPT / GENERATE / SKIP are hardware buttons»; §5 de
  07-gui y esta tarea los exigen como soft keys. El export ocupa las ocho teclas
  con pares cue/energy. El demo muestra las acciones pedidas en la barra; queda
  pendiente acordar el mapeo final que conserve los ocho controles de opciones.
- El Design System dice scrim 70%; las notas QML dicen contenido al 28% (72%).
- El CSS del export usa repeat(8,1fr) dentro de 1264 px (celdas de 151 px);
  las notas QML exigen x=8+i×160, ancho152. El contrato solicitado se verifica
  literalmente: la última columna termina en x1280, sin margen derecho de 8 px.
- No existen en este clone los docs 06/08 citados en la tarea y ADR-007.

Revisión visual de los nueve PNG contra las posiciones, estilos y contenido de los
exports leídos como texto (renderer software offscreen, escala 1):
- Las fuentes ausentes cambian anchos/pesos: Helvetica del sistema y Menlo en este
  Mac, en vez de Big Shoulders/General Sans/Plex/JetBrains. Los títulos grandes son
  menos condensados y algunos nombres largos se eliden antes (p. ej. Prophet Chord).
- TopBar compacta BPM/Link en una línea y omite el separador vertical; las etiquetas
  de estados son texto compacto, sin todas las cápsulas rellenas del export.
- Los halos y LEDs son PNG de gradientes horneados; sombras exteriores y relieves
  de tarjetas/keycaps están simplificados. No hay MultiEffect ni blur vivo.
- Toasts conserva un LiveSession completo detrás, mientras el export de esa vista
  deja vacío el panel de información; los avisos se mantienen fijos en el catálogo.
  El componente Toast admite entrada 150/hold4000/salida300 con OpacityAnimator.
- Settings usa barras ilustrativas sin thumbs ni calibración real; IR simplificado,
  no banda de rango interactiva. Audio/Connections/System son resúmenes mínimos.
- Layer usa cuatro teclas cue (giro = energía) más Generate/Accept/Skip/Back. Sus
  pistas de encoder indican ese mapeo temporal, distinto del par cue/energy original.
- Transition conserva chips y progreso, pero simplifica las mini barras de tipos y
  el footer (sin estado dinámico de takeover). El chip actual y cue llevan guiones.
- Mixer mantiene 8 canales, EQ, filtro, envíos, cue y 4 grupos fijos; medidores y
  faders reciben snapshots pero no interpolan 80 ms ni siguen un reloj de audio.
- Hands usa halos horneados y una regla estática; CM usa el tamaño hero completo.
  El export presenta C4 y 17 cm aunque su ladder rotula C4 con ~14 cm: se registra
  esa inconsistencia de datos de ejemplo sin redefinir la conversión distancia/nota.

Resultado: `python3 -m pytest apps/consola/gui-console/tests -q` → **32 passed**,
sin warnings, PySide6 6.11.1 / Python 3.14.3. Se comprueba también que las propiedades
visibles reciben datos distintos por columna/opción, no solo que QML compile.
Las nueve capturas están en `apps/consola/gui-console/screenshots/01-Boot.png` hasta
`09-Hands.png`, todas 1280×720. El script no usa red.

El documento se escribió antes del código. El intento de commit de documentación
falló por `Unable to create .git/index.lock: Operation not permitted`. La rama
`codex/gui-fase1` queda con los archivos sin commit; no se hace push.

## Referencias

- [ADR-007](../adr/ADR-007-gui-consola-pyside6.md): frontera backend y compuerta.
- [07-gui](07-gui.md): inventario, idioma, teclas y grupos fijos.
- [Design System](design/claude-design/Console%20Design%20System.dc.html),
  [MVP + contrato geométrico](design/claude-design/Console%20MVP%20Screens.dc.html)
  y los cinco exports de componentes de la misma carpeta: valores y composición.
- Qt: https://doc.qt.io/qt-6/qabstractlistmodel.html,
  https://doc.qt.io/qt-6/qtquick-performance.html,
  https://doc.qt.io/qt-6/qml-qtquick-animator.html (referencias de API, sin red).
