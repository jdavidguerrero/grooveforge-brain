# Studio · Cadena de FX del synth (B4) y presets con SHIFT + B4

> **Fecha:** 2026-10-16 · **Estado:** APROBADO por Juan el 2026-10-16 ("FX que se puedan aplicar al synth solo
> algunos, que no rompan la CPU · implementa eso")
> **Depende de:** `05-fx-architecture.md` (capas, CPU medido), ADR-004 (panel), theory/04 (navegación en el
> Teensy), theory/lfo-modulacion.md (P1/P2 por página, `gf_params.h`).
> **Enmiendas que esto pide y que no se aplican solas** (specs inmutables sin instrucción): ver §7.

## 1. El problema

En SYNTH no hay forma de darle carácter al sonido con efectos: los FX viven en el modo FX (B5), que es para
procesar el master o la entrada en vivo. Los synths modernos (Serum, Vital) traen una **cadena de FX dentro del
patch**: saturación, chorus, reverb, encendibles y editables, guardados con el sonido. Además B4 abre PRESETS,
una página que se usa poco durante la edición.

## 2. Decisión

- **B4 abre la página SYNTH FX**: una cadena fija de 4 efectos sobre la salida del engine, guardada en el patch
  de cada engine. B4 otra vez vuelve a la última vista (misma regla que B1–B3).
- **SHIFT + NAV push abre ENGINE SELECT** desde cualquier página de SYNTH (acceso directo, pedido de Juan el
  2026-10-16: llegar al engine por PRESETS → B4 eran 4 pasos).
- **PRESETS pasa a SHIFT + B4** (y al toque sobre el nombre del preset en la barra superior, fase 2 con
  UI_INPUT). ENGINE SELECT sigue dentro de PRESETS (B4 en PRESETS).
- **Solo entran los FX livianos ya medidos en el Teensy 4.1** (`05` §4, roadmap con CPU medido por sprint):

| Orden | FX | CPU medido | Por qué en el synth |
|---|---|---|---|
| 1 | BIT SCULPT | 0.6 % | carácter lo-fi antes de todo (como un pedal de distorsión al principio) |
| 2 | TAPE SATURATE | 3 % | calor y compresión; la saturación va antes de la modulación |
| 3 | PHASE CHORUS | 1.1 % | ensancha pads y leads (el chorus del Juno es parte del sonido) |
| 4 | MODAL REVERB | 1.2 % | espacio al final de la cadena |
| | **Las 4 encendidas** | **~6 %** | vs. ~12 % de un solo Granular en el presupuesto de `05` §2 |

**Fuera de la cadena**, y por qué:

| FX | Motivo |
|---|---|
| GHOST ECHO | `AudioEffectDelay` toma bloques de AudioMemory: 375 ms ≈ 130 bloques y el pool es de 96 (`audio_graph.cpp`). Entra cuando se mueva a un buffer propio (PSRAM o DMAMEM) |
| GRANULAR, CYMATIC, SPECTRAL, PITCH MOSAIC | 8–18 % estimado cada uno: siguen solo en el modo FX |
| SUB GENESIS, GLITCH, SPRING | pensados para el master / performance (`05` §1, capa MASTER) |

**Orden fijo**, no reordenable: distorsión → saturación → modulación → espacio es el orden de un pedalboard y
evita recablear el grafo en vivo (clicks). Reordenar queda para una fase 2 si hace falta.

## 3. Grafo de audio (sketch49)

```
g_engine_bus ─> BIT ─> TAPE ─> CHORUS ─> MODAL ─> g_master_mix ch1
               (cada FX: dry/wet propio; apagado = setBypass(true), pasa seco)
g_master_mix ch0 (engine directo) = 0
```

Los cuatro FX se instancian una vez (estáticos) y quedan siempre en el grafo: apagar uno es su `setBypass()`,
que deja pasar la señal seca. Cuesta la CPU de los cuatro siempre (~6 %), a cambio de no crear ni destruir
objetos en vivo, que en la Teensy Audio Library implica `AudioConnection` dinámicas y riesgo de clicks.
Todo el código de los FX es custom (`src/fx/`), sobre objetos de la Teensy Audio Library.

## 4. Controles en la página SYNTH FX

| Control | Qué hace |
|---|---|
| NAV | elige el FX (cursor 0–3) |
| NAV push | enciende / apaga el FX elegido |
| ENC L / ENC R | MIX y el parámetro principal, **igual que en el modo FX** (`05` §1.13) |
| P1 / P2 | los dos parámetros de usuario de `05` §1.13 |

(Corregido el 2026-10-18 tras la auditoría: la primera versión ponía MIX en P1 y quedaba distinto del modo FX.)
| B4 | vuelve a la última vista · SHIFT + B4: PRESETS |

| FX | ENC L | ENC R | P1 | P2 |
|---|---|---|---|---|
| BIT SCULPT | MIX | BITS 16–1 | SAMPLE RATE 48 k–1 kHz | SCULPT |
| TAPE SATURATE | MIX | DRIVE 1–10 | WOW | FLUTTER |
| PHASE CHORUS | MIX | DEPTH | RATE 0.1–10 Hz | DRIFT |
| MODAL REVERB | MIX | DECAY ×0.2–4 | MATERIAL (campana · guadua · cristal · madera) | SIZE (S · M · L) |

## 5. Parámetros en el bridge (`gf_params.h`)

Con el mismo `param_id` del synth (`((0x10 + engine) << 8) | (grupo << 4) | índice`), porque la cadena es parte
del patch de cada engine:

| Grupo | Índice | Parámetro |
|---|---|---|
| **4 SFX** | 0–3 | encendido de cada FX (0 / 1) |
| 4 SFX | 4–7 | MIX de cada FX |
| **5 SFX_PARAM** | `fx · 3 + k` (0–11) | k = 0 principal, 1 SHIFT P1, 2 SHIFT P2 (0–1; los discretos con `gf_index_norm`) |

El cursor de la página viaja en `UI_STATE.cursor` (como en ENGINE y PRESETS). La página es la **10 SYNTH FX**
del enum de páginas (la 7 sigue siendo el error de la cabeza). No cambia el tamaño de UI_STATE.

## 6. Cómo se prueba

| Qué | Cómo | Dónde |
|---|---|---|
| B4 / SHIFT + B4, cursor y encendido | tests Unity de StudioUi | `pio test -e native` |
| P1/P2 en SYNTH FX, ids y rangos | tests Unity de page_params | `pio test -e native` |
| Se oye | encender TAPE y subir DRIVE; CHORUS en un pad del Juno; REVERB con MATERIAL | protoboard |
| CPU | el sketch49 imprime `AudioProcessorUsageMax()` cada 5 s, con las 4 encendidas | protoboard |

## 7. Enmiendas pendientes (no aplicadas)

- **`05` §2 (presupuesto de CPU):** suma una capa "SYNTH FX" de ~6 % con los 4 livianos. No se toca sin
  instrucción explícita.
- **ADR-004 (panel):** B4 pasa de PRESET a SYNTH FX, y PRESETS a SHIFT + B4. Hay que anotarlo como revisión
  del ADR cuando Juan lo confirme en el hardware.
- **UI v3:** la etiqueta de B4 en la barra inferior pasa de PRESET a SYNTH FX (implementado en la cabeza).

## 8. Qué no cubre

Reordenar la cadena, GHOST ECHO en el synth, más de una instancia del mismo FX, el modo FX (B5) con audio real
en el sketch49, y guardar la cadena en presets (no hay presets persistentes todavía).
