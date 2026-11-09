# ADR-007: GUI de la consola en QML con backend en Python (PySide6), con compuerta de rendimiento

**Status:** Accepted (Juan, 2026-11-09), condicionado a la compuerta de rendimiento de §Decisión.
**Date:** 2026-11-09
**Deciders:** Juan Guerrero (owner)
**Relacionados:** GroovePush ADR-0003 (`apps/docs/consola/groovepush/specs/adr/0003-gui-qt-brain-identity-7in-display.md`), `apps/docs/consola/07-gui.md`, diseño en `apps/docs/consola/design/claude-design/`, `apps/docs/consola/08-superficie-pi.md`
**Contradice:** GroovePush ADR-0003, que mantiene los modelos de la GUI en C++ (`ClipGridModel`, `TrackListModel`…). Ese ADR se escribió para GroovePush como controlador de Live, antes de la consola con motor propio en el Pi.

---

## Context

La consola corre en el Pi 5 una GUI Qt 6 / QML a 1280×720. Los datos que la GUI muestra vienen de procesos en **Python**: el motor (`tools/motor`), el reproductor (`tools/consola/reproductor.py`) y el lector de la superficie (`tools/consola/superficie.py`). Juan había definido C++ por rendimiento.

**Dónde se va el tiempo en una GUI QML.** El dibujo lo hace el *scene graph* de Qt, en C++ y en la GPU del Pi 5, con un hilo de render propio. El lenguaje del backend solo decide quién actualiza las propiedades que QML muestra. Con PySide6, el costo de Python aparece en el hilo de la GUI, en los callbacks que actualizan modelos. Hay dos riesgos:

1. **Pausas del hilo de la GUI**: si un callback de Python tarda (trabajo pesado o recolección de basura), las animaciones que no son *Animators* se atrasan un cuadro.
2. **Volumen de eventos**: la superficie puede mandar cientos de eventos por segundo (encoders, faders, manos).

## Decision

**QML para toda la interfaz y PySide6 (Python) para el backend**, con estas reglas:

- **Nada pesado en el proceso de la GUI.** El motor, el audio y la generación siguen en sus procesos; la GUI solo recibe estado y manda comandos.
- **Eventos agrupados por cuadro:** la superficie y el motor actualizan modelos como máximo una vez por cuadro (16 ms), no por evento.
- **Animaciones con `Animator`** (corren en el hilo de render) donde sea posible.
- **Brillos y halos con imágenes pre-renderizadas**, no con desenfoque en tiempo real (`MultiEffect`).
- **El QML no depende del lenguaje del backend:** los modelos se exponen con la misma interfaz (`QAbstractListModel` y propiedades), así que pasar el backend a C++ no toca las pantallas.

**Compuerta de rendimiento (antes de la fase 3, conexión real):** en el Pi 5, con la pantalla LIVE · SESSION y la de MIXER animando, y un generador de 1000 eventos de superficie por segundo:

| Medida | Objetivo |
|---|---|
| Cuadros por segundo | 60 sostenidos |
| Tiempo de cuadro (p99, `QSG_RENDER_TIMING`) | < 16,7 ms |
| Evento de superficie → cambio en pantalla (p95) | < 1 cuadro + 16 ms |
| CPU del proceso GUI | < 25 % de un núcleo |

Si no pasa, el backend se reescribe en C++ con la misma interfaz y el QML se conserva.

## Alternativas

| Opción | Pros | Contras |
|---|---|---|
| **PySide6 (elegida)** | Mismo lenguaje que motor y superficie, sin protocolo intermedio; iteración rápida; se previsualiza en el Mac (`pyside6-qml`, PySide6 6.11) | Riesgo de pausas en el hilo de la GUI (mitigado arriba y medido en la compuerta) |
| C++ con modelos (ADR-0003) | Sin GIL ni recolección de basura | Hace falta un protocolo local (socket) entre el motor en Python y la GUI; más lento de iterar con 10–18 h por semana |
| Reutilizar la GUI de GroovePush | Ya tiene modelos de Live | Está hecha para 800×480 y para controlar Live; las pantallas nuevas son otras |

## Consecuencias

- La GUI vive en `apps/consola/gui-console/` (la de GroovePush en `apps/consola/gui/` queda para el modo CONTROLLER mientras se migra).
- Textos de la interfaz en **inglés** (Juan, 2026-11-09).
- Fuentes empaquetadas en la app (el Pi no depende de internet).
