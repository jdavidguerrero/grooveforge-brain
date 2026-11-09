# Consola: GUI

> **Fecha:** 2026-11-09. **Estado:** brief aprobado por Juan para Claude Design (diseño completo); el MVP implementa un subconjunto. **Base:** GroovePush ADR-0003 (Qt 6 / QML, 1280×720, identidad del Brain), `push-controller-gui-design.md` (glanceable, barra de 8 teclas, «nada rebota»), `00-concepto.md` §4–§5.1, `03-arquitectura-capas.md` §4. **No modifica** `00–06`.
> **Prompt para Claude Design:** `design/claude-design-prompt-consola.md`.

## 1. Decisiones de Juan (2026-11-09)

- **Dos modos** como familia Push: **CONTROLADOR** (clon de Push 3 conectado a Live) y **STANDALONE** (estilo Push 3 standalone o MPC Live 3).
- **Rejilla tipo Push / Akai como vista principal**, y **dos vistas en LIVE**, como en Ableton: SESIÓN (rejilla) y ARREGLO (los «ríos de capas»).
- **Diseño arriesgado, profesional y no genérico**: más color, botones y textos con carácter. La marca de GroovePilot se usa en menús, ajustes y arranque.
- **Touch auxiliar**: menús, cambiar algo puntual, buscar y escribir. **No se toca música en la pantalla**: eso es de pads, encoders y faders.
- **Referencias iniciales:** Ableton Push 3 y Akai MPC Live 3.
- **Diseño completo de todas las vistas** en Claude Design; el **MVP** implementa las 9 de la tabla §2.

## 2. Inventario de pantallas

| # | Pantalla | Modo | MVP |
|---|---|---|---|
| 1 | Arranque | Común | ✅ |
| 2 | Selector de modo | Común | ✅ |
| 3 | Avisos (toasts) | Común | ✅ |
| 4 | Ajustes (Audio · Conexiones · Superficie · Sistema) | Común | Mínimo |
| 5 | Buscar / Browser + teclado en pantalla | Común | |
| 6 | SESSION | Controlador | |
| 7 | NOTE | Controlador | |
| 8 | CLIP | Controlador | |
| 9 | DEVICE | Controlador | |
| 10 | MIX | Controlador | |
| 11 | BROWSE (de Live) | Controlador | |
| 12 | Escala · Repetir · Tempo (ventanas) | Controlador y standalone | |
| 13 | MOTOR (modo estudio: el motor del Mac) | Controlador | |
| 14 | LIVE · vista SESIÓN | Standalone | ✅ |
| 15 | LIVE · vista ARREGLO («ríos de capas») | Standalone | |
| 16 | CAPA | Standalone | ✅ |
| 17 | TRANSICIÓN (ventana) | Standalone | ✅ |
| 18 | MIXER | Standalone | ✅ |
| 19 | MANOS (sensores Sharp) | Standalone | ✅ |
| 20 | SECUENCIADOR (vistas PASOS y PIANO ROLL) | Standalone | |
| 21 | PADS | Standalone | |
| 22 | BRAIN (páginas del synth) | Standalone | |
| 23 | KIT (drum machine) | Standalone | |
| 24 | BIBLIOTECA / SETS | Standalone | |
| 25 | REGISTRO | Standalone | |
| 26 | RUTEO (modo híbrido) | Standalone | |

## 3. Sensores de mano (agregados al MVP, 2026-11-09)

Dos **Sharp GP2Y0A41** (IR, 4–30 cm, salida analógica) en los canales libres del **ADS1115 #2** de la Teensy S. Alimentados a 5 V con condensador local; su salida (≤ 3,1 V) entra al ADS1115 sin riesgo. Usos: abrir filtro o la macro MOVE, envío a echo o reverb, riser, y **modo theremin** (notas ajustadas a la escala). En pantalla: halo en el borde que crece al acercar la mano; pantalla MANOS para asignarlos.

## 4. Piano roll en standalone (Juan, 2026-11-09)

El SECUENCIADOR tiene dos vistas: **PASOS** (batería) y **PIANO ROLL** (bajo, lead, acordes). En el piano roll, los pads son una ventana de 8 pasos × 4 notas, los encoders editan la nota seleccionada (posición, largo, velocity, microtiming) y **el touch solo navega** (zoom, desplazarse, rango), sin dibujar notas. Las notas del motor se distinguen de las de Juan y el motor no pisa sus ediciones.
