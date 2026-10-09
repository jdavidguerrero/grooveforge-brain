# ADR-002: Variante Brain · Studio (cabeza Pi 2B + pantalla DSI de 5")

**Status:** Accepted
**Date:** 2026-10-07
**Deciders:** Juan Guerrero (owner)
**Relacionados:** ADR-001 (`apps/docs/content/adr/`), ADR-003 (filtro analógico como add-on)

---

## Context

- El objetivo inmediato es **terminar un instrumento para uso propio** (producir música) que a la
  vez sirva al canal GrooveMakers. El producto comercial sigue su spec (`00`–`06`).
- La cabeza comercial es ESP32-S3 + GC9A01 240×240 (`01` §2). En cámara casi no se ve.
- Hay disponible una pantalla **Freenove 5" IPS 800×480 táctil, DSI** (Freenove lista
  compatibilidad con Pi 5 / 4B / 3B+ / 3B / 3A+ / 2B / 1B+ / 1A+). La Pi 5 queda reservada para
  GroovePush. La Pi Zero no tiene DSI.
- Se consiguió una **Raspberry Pi 2B** (~70.000 COP).
- Se generó el diseño de UI completo con Claude Design: `variants/studio/design/`.

## Decision

1. **Brain · Studio** es una variante personal, **no comercial**: mismo cuerpo, otra cabeza.
   - Idénticos a la base: firmware del Teensy (sin `#ifdef` de variante), engines, FX, ML,
     controles físicos (3 encoders, 4 botones, volumen) y bridge protocol `02`.
   - Cabeza: **Pi 2B + Freenove DSI 800×480 táctil**, como bridge slave por UART 921600.
2. App de la cabeza: `apps/studio-head/`, **C++17 + CMake + LVGL 8.3** (misma versión mayor que el
   ESP32), DRM/fbdev + evdev. Arquitectura en `variants/studio/theory/01-studio-head-architecture.md`.
3. **Reglas de la variante:**
   - Ninguna feature musical vive solo en Studio: todo lo musical entra primero a la base.
   - Cumple `02` tal cual: sin frames privados.
   - Ante un conflicto, la base gana (`01` está por encima del spec delta).
   - Deltas permitidos: hardware de la cabeza, alimentación (fuente de pared 5V), enclosure,
     toolchain de la cabeza y, si las mediciones lo exigen, la latencia del bridge.
4. **Navegación:** cambio SYNTH↔FX con **doble push de NAV** (como `01` §3.4; el firmware actual usa
   B2 y se corrige). En Studio, el giro de NAV recorre PERFORM → AI → SPECTRUM y B1–B4 abren las
   páginas de edición.
5. **Touch, fase 1:** atajo secundario. Toda función se opera con los controles físicos.
6. **Diseño de referencia:** `variants/studio/design/` (ver su `README.md`). Vigentes desde el 2026-10-09:
   UI v3, FX Viz y Enclosure en `claude-design-2026-10-09/`.

## Options Considered

| Opción | Resultado | Motivo |
|---|---|---|
| Pi Zero (original) + Freenove | ❌ | sin conector DSI; además, ARM11 de un núcleo |
| Pi 5 + Freenove | ❌ | reservada para GroovePush |
| ESP32-S3 + pantalla RGB 5" nueva | considerada | máxima reutilización de código y PlatformIO, pero obliga a comprar otra pantalla y deja la Freenove sin uso |
| **Pi 2B + Freenove** | ✅ | reutiliza la pantalla, compatible oficialmente, barata; LVGL rinde en 4 núcleos A7 |
| Qt Quick en lugar de LVGL | ❌ para Studio | arranque lento y pesado en la 2B; sin reutilización con la base. Queda como opción para GroovePush |

## Trade-off Analysis

La opción ESP32-S3 con pantalla RGB era técnicamente más cercana al producto comercial. Se eligió la
Pi 2B porque reutiliza hardware que ya está en mano y porque la regla "ninguna feature solo en
Studio" mantiene a la variante empujando a la base en lugar de competir con ella. El costo es una
segunda toolchain (Linux/CMake) y el riesgo de jitter del bridge en Linux, que se mide antes de
aceptarlo.

## Consequences

**Más fácil:** UI legible en cámara; desarrollar vistas en el simulador SDL; dogfooding real del
instrumento en producciones.

**Más difícil:** mantener dos sets de vistas (240×240 redonda y 800×480); una toolchain fuera de
PlatformIO; boot de Linux (mitigado con systemd y rootfs de solo lectura).

**Desviaciones registradas:**
- Excepción a la regla PlatformIO para `apps/studio-head/`.
- Latencia del bridge: `02` §7.3 pide <2 ms p95. Se mide en la 2B; si no cumple, se decide en un
  ADR nuevo si se relaja solo para Studio.

**A revisitar:**
- Frames de espectro: requieren enmendar `02` (rango libre 0x90–0xEF). Necesita autorización.
- Nombres de los engines (marcas registradas) antes de mostrarlos en contenido.
- La robustez del bridge del lado Teensy (R-C1) es prerrequisito de la reconexión automática.

## Action Items

Ver hito **Brain · 4 Studio head** en Notion (Micro-tareas). Resumen:
1. [ ] Esqueleto `apps/studio-head` con simulador SDL.
2. [ ] Transporte de replay, hilo de I/O con parser resync y `StudioState`.
3. [ ] Pantalla de enlace perdido y reconexión (depende de R-C1).
4. [ ] Toolchain armhf, systemd con watchdog, rootfs de solo lectura.
5. [ ] Medir los criterios de aceptación del theory doc en la 2B.
