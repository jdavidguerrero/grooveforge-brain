# Studio · Sesión: volver a donde estabas (cambio de modo y reinicio)

> **Fecha:** 2026-10-27 · **Estado:** APROBADO por Juan ("cuando se reinicie [arranque] con la última opción o cuando se
> cambie modo … todo siempre vuelve al home y se pierde lo que tenía") · **Depende de:** theory/12 §3d (HYBRID SYNTH),
> theory/13 (almacenamiento), theory/06 (presets).

## 1. Problema

- **Cambio de modo.** Entrar a HYBRID SYNTH carga una capa: sus dos mitades **sobrescriben** los patches de esos
  engines y cambia el engine activo. Al volver a SYNTH el sonido que tenías ya no está. Al volver a HYBRID SYNTH la capa
  se carga otra vez del banco (se pierde lo editado). Y en los dos modos la página vuelve siempre a la home.
- **Reinicio.** El Brain arranca siempre en SYNTH, Model D, preset 1, volumen 0 dB.

## 2. Cada modo guarda su estado

| Modo | Qué se guarda al salir | Qué pasa al volver |
|---|---|---|
| **SYNTH** | Engine activo, los patches de los 6 engines (con lo editado), cutoff, resonancia, preset cargado de cada uno y la página | Todo vuelve tal cual, en la misma página |
| **HYBRID SYNTH** | La capa como suena (las dos mitades editadas, MORPH, retraso, transposición, foco), cuál era y la página | La misma capa, con lo editado, en la misma página |
| FX | (no toca el sonido del synth) | — |

- Es RAM: no escribe el flash. Elegir otro preset de capa (NAV push) sí reemplaza la capa guardada, como siempre.
- ENGINE SELECT no se restaura como página (se vuelve a la home del modo): es una página de paso.
- CC 18 desde Live hace lo mismo que MODE: con un valor entra a HYBRID SYNTH guardando SYNTH; con 0 vuelve a SYNTH
  restaurándolo.

## 3. Reinicio: la sesión en el flash

Un archivo `/session.gfs` (~4.4 KB, un bloque) con:

- modo (SYNTH o HYBRID SYNTH), engine activo, volumen de salida;
- los 6 patches de SYNTH **con lo editado sin guardar** (en formato de preset 5) y el preset cargado de cada engine;
- si hay capa: la capa como suena (formato de capa 2), cuál era y el foco.

**Cuándo se escribe:** 10 s después del último cambio (perilla, tecla, preset, CC), y solo si no hay notas sonando.
Así una sesión de edición escribe una vez por pausa, no por cada detent (cada escritura borra un bloque de 64 KB del
flash: cuidar su vida útil y no frenar el audio). Si se corta la luz dentro de esos 10 s, se pierde solo lo último.

**Al arrancar:** si el archivo es válido, se restaura todo (en HYBRID SYNTH, la capa con lo editado). Si no existe o
es de otra versión, el arranque de siempre. `@GF FORMAT YES` también lo borra.

## 4. Cómo se prueba

| Qué | Cómo |
|---|---|
| La sesión va y vuelve entera (patches editados, capa, modo, volumen) | test native |
| SYNTH → FX → HYBRID SYNTH → SYNTH vuelve a la página de SYNTH; HYBRID SYNTH vuelve a su página | test native |
| Editar en SYNTH, ir a HYBRID SYNTH, editar la capa, volver: los dos sonidos como estaban | en el hardware |
| Editar, esperar 10 s, desconectar y conectar: arranca igual | en el hardware |
