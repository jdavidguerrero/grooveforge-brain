# Prompt para Claude Design — GrooveForge Brain · Studio UI

> **Cómo usarlo:** copiar todo lo que está debajo de la línea en Claude Design.
> Adjuntar como referencia: los 3 mocks de `apps/docs/ui-mocks/*.html` (UI redonda actual)
> y `apps/firmware-esp32/src/display/ui_theme.h` (design tokens).
>
> Última actualización: 2026-10-07

---

## Rol y objetivo

Sos diseñador/a de interfaces para instrumentos musicales de hardware. Vas a diseñar la UI de
**GrooveForge Brain · Studio**: una variante personal (no comercial) de un sintetizador
boutique híbrido, usada por su creador para producir música en su estudio. La UI corre en una
pantalla táctil de 5" a **800×480 px** y tiene que sentirse como un instrumento, no como una app.

## Contexto del producto

- GrooveForge Brain es un synth híbrido hecho en Bogotá: un Teensy 4.1 genera el audio
  (6 engines inspirados en synths clásicos, 12 efectos, ML on-device que detecta tonalidad,
  acorde y BPM) y la señal pasa por un **filtro analógico discreto de transistores** (ladder
  de 4 polos tipo Moog). Ese filtro es el corazón del instrumento.
- El producto comercial usa una pantalla redonda de 240×240. **Studio** es la variante de
  estudio con pantalla grande: la misma lógica y la misma identidad, con más espacio para
  mostrar información a la vez.
- Filosofía: minimalista boutique, más cerca del OP-1 que de un Elektron. Estudio oscuro,
  el contenido es protagonista.

## Restricciones de hardware y rendering (críticas, condicionan todo el diseño)

- **Pantalla:** 5" IPS, 800×480 px, ~187 PPI, touch capacitivo de 5 puntos.
- **Procesador:** Raspberry Pi 2B (4 núcleos ARM a 900 MHz). La UI se implementa en **LVGL 8.3
  con render por software** (sin GPU). Por eso:
  - Rellenos **sólidos**. Nada de blur, glassmorphism ni backdrop-filter.
  - Sombras grandes y difusas: no. Gradientes: solo en áreas chicas.
  - Sin overlays translúcidos de pantalla completa, salvo un scrim oscuro al 55% para avisos.
  - Las animaciones van solo en **regiones chicas** (espectro, medidores, indicador de fase).
    Objetivo: 30 fps. Las transiciones entre pantallas son slide o fade simples de ≤200 ms.
  - Las imágenes se renderizan **al tamaño final en píxeles** (sin escalar en runtime). Mejor
    con fondo opaco que con alpha.
  - Las tipografías son bitmaps pregenerados: **máximo 7 tamaños** y 2 pesos.
- **Grilla:** 8 px, todo alineado al píxel.

## Controles físicos (la UI se maneja PRIMERO con estos)

Todas las pantallas tienen que funcionar completas **solo con controles físicos**. El touch es
un atajo secundario: donde se ofrezca tap, el target mínimo es **64 px**.

| Control | Modo SYNTH | Modo FX |
|---|---|---|
| ENC L (girar) | Cutoff del filtro analógico | Dry/Wet del FX activo |
| ENC L (push) | Bypass del filtro ON/OFF | — |
| ENC R (girar) | Resonance | Parámetro principal del FX |
| ENC R (push) | Reset de resonance a 0 | — |
| ENC NAV (girar) | Navegar páginas / listas | Recorrer los FX |
| ENC NAV (push) | Confirmar / acción AI · **doble push = cambiar SYNTH↔FX** | Activar/desactivar FX |
| B1–B4 | OSC · ENV · LFO/MOD · PRESET/ENGINE | INSERT · SEND · MASTER · FX favorito |
| VOL | Volumen de salida (siempre) | igual |

**Convención de soft-keys:** asumí que los 4 botones están **debajo de la pantalla**, con el
mismo espaciado, y que ENC L queda a la izquierda, ENC NAV al centro y ENC R a la derecha.
Mostrá una barra de soft-keys abajo con 4 segmentos alineados a B1–B4, y los valores de ENC L y
ENC R en los costados correspondientes. (El enclosure de Studio todavía no está diseñado, así
que tu layout puede definir dónde van físicamente los controles.)

## Design system (respetar los hex existentes; podés extender la escala tipográfica y el spacing)

**Paleta**

| Token | Hex | Uso |
|---|---|---|
| BG | #0A0A0A | fondo antracita de todas las pantallas |
| TEAL | #1D9E75 | primario, identidad, chrome del modo SYNTH |
| TEAL_PLUS | #5DCAA5 | elementos activos, highlights |
| TEAL_DIM | #0E5040 | pistas de fondo, divisores, estados idle |
| PURPLE | #534AB7 | chrome del modo FX |
| PURPLE_BRIGHT | #7A6FE0 | FX activo, arcs FX |
| GRAY | #9FE1CB | labels secundarios (gris con tinte menta) |
| HERO_WHITE | #F2F2F2 | valores hero grandes |
| MONITOR_GRAY | #5A5A6E | info de monitoreo permanente que no compite |
| RED | #E84A4A | error, clip |
| AMBER | #E8B84A | warnings |
| BYPASS_ORANGE | #FF6A35 | estado BYPASS |

**Paleta Camelot (tonalidad detectada):** 12 colores indexados por el círculo de quintas:
C #1FB6BB · G #1D86C4 · D #3358C0 · A #5540B8 · E #8A2DB5 · B #B61F32 · F# #DD4D28 ·
C# #DD902C · G# #DDB42A · D# #B0CC30 · A# #74C443 · F #2EBB87. Las tonalidades se muestran como
código Camelot ("8B", "11A") + nombre ("C major"). Major y minor comparten color.

**Tipografía:** IBM Plex Mono en toda la UI. Proponé una escala para 800×480 (sugerencia de
partida: 12 / 16 / 20 / 28 / 40 / 64 / 96 px). El copy de la UI va **en inglés**, en mayúsculas
para labels técnicos (OSC, ENV, CUTOFF), igual que la UI redonda.

**Modo:** el modo SYNTH usa chrome teal y el modo FX chrome púrpura. El cambio de modo tiene que
leerse al instante.

## Datos disponibles para la UI (no inventar otros)

- Engine activo, preset, parámetros (cutoff, resonance, OSC/ENV/LFO por engine), bypass del filtro.
- **AI on-device:** tonalidad + confianza, acorde + confianza, BPM + fase del beat, género +
  confianza, estado de Scale Lock (cuando hace snap de una nota: nota original → nota corregida),
  cromagrama de 12 bins (actividad por clase de nota, a 4 Hz) y fase del compás.
- **Espectro en vivo (propuesto):** 32 bandas logarítmicas de 20 Hz a 20 kHz, en dB, a 30 Hz.
- FX activo, dry/wet, parámetro principal, ruteo (INSERT/SEND/MASTER).
- Estado de conexión con el motor de audio (si se pierde: pantalla de error).

## Engines (6) — para el selector con imágenes

| Engine | Arquitectura | Etiqueta corta |
|---|---|---|
| Model D | mono, 3 VCO, filtro ladder | MONO · 3 VCO |
| Juno-106 | poly 6 voces, DCO, chorus BBD | POLY 6 · CHORUS |
| Prophet-5 | poly 5 voces, 2 VCO por voz | POLY 5 · 2 VCO |
| OB-6 | poly 6 voces, doble saw | POLY 6 · DUAL SAW |
| ARP 2600 | semi-modular, patch memory | SEMI-MODULAR |
| DX7 | FM, 4 operadores por voz, poly 16 | FM · 4-OP |

**Ilustraciones:** **originales y estilizadas** que evoquen la época y el carácter de cada
instrumento (formas, perillas, teclado, panel), dentro de la paleta de la marca. **Sin logos,
sin marcas, sin réplicas fotográficas ni de diseño de productos reales.** Tienen que funcionar
como set coherente (mismo estilo, misma luz, mismo encuadre). Tamaños fijos: card 280×180 px y
thumbnail 120×80 px.

## FX (12 slots)

Cymatic Resonator · Granular Cloud · Ghost Echo · Spectral Smear · Tape Saturate · Bit Sculpt ·
Modal Reverb · Phase Chorus · Pitch Mosaic · Spring + Plate · Glitch Stutter · Sub Genesis.
Hoy hay 9 disponibles: **Spectral Smear, Pitch Mosaic y Glitch Stutter** se muestran como
slots deshabilitados ("IN DEVELOPMENT"). El diseño tiene que contemplar ese estado.

## Pantallas a diseñar

1. **PERFORM (modo SYNTH, pantalla principal).** Card del engine activo (thumbnail + nombre +
   etiqueta), **cutoff y resonance como protagonistas** (es el filtro analógico: que se sienta
   físico), franja de espectro en vivo con la **línea de cutoff superpuesta** sobre el espectro,
   chips de AI (KEY en su color Camelot, CHORD, BPM con indicador de fase, GENRE), soft-keys B1–B4.
2. **ENGINE SELECT.** Las 6 cards con ilustración, arquitectura y etiqueta. ENC NAV recorre y push
   confirma; tap también selecciona. Que el engine seleccionado se distinga sin ambigüedad.
3. **ENV (página representativa de SYNTH).** Gráfico ADSR grande y legible: el segmento que se
   edita resaltado, los valores numéricos visibles y su mapping a encoders.
4. **FX MAIN (modo FX).** Lista de los 12 slots, FX activo con dry/wet hero y parámetro
   principal, ruteo INSERT/SEND/MASTER, estado BYPASS.
5. **AI / HARMONY.** Tonalidad hero con rueda Camelot, acorde, cromagrama de 12 bins animado,
   Scale Lock ON/OFF con el último snap (nota → nota), BPM y fase del compás.
6. **SPECTRUM (pantalla completa).** Analizador de 32 bandas con peak hold, escala de frecuencia
   y dB, línea de cutoff + indicador de resonancia superpuestos. Pensada para que se lea en video.
7. **PRESETS.** Browser de presets por engine: lista, nombre, guardar/cargar.
8. **Overlays y estados:** volumen, bypass del filtro, cambio de modo SYNTH↔FX, error de conexión
   con el motor de audio, splash de boot.

Las pantallas 1, 4 y 5 van en su variante de color de modo. Si una pantalla existe en ambos
modos, mostrá las dos.

## Qué NO hacer

- Nada que requiera GPU (blur, vidrio, sombras grandes, gradientes de pantalla completa).
- Nada de UI tipo app móvil genérica (cards con sombras, íconos de librerías genéricas, hamburguesas).
- No depender del touch para ninguna función.
- No inventar datos que no estén en "Datos disponibles".
- No usar logos ni réplicas de productos reales.
- No pasar de 7 tamaños tipográficos ni de 2 pesos.

## Entregables

1. Las 8 pantallas en hi-fi a **800×480 px exactos, escala 1×**, con sus variantes de modo.
2. **Hoja de componentes:** barra de soft-keys, indicadores de ENC L/R, chips de AI, card de
   engine (card y thumbnail), widget de espectro, gráfico ADSR, item de lista FX (activo /
   inactivo / deshabilitado), overlays.
3. **Tokens de Studio:** escala tipográfica y spacing en grilla de 8 px que extiendan los
   tokens existentes sin cambiar ningún hex.
4. **Dirección de ilustración** + las 6 ilustraciones de engines a 280×180 y 120×80.
5. **Anotaciones por pantalla:** qué hace cada control físico, qué regiones animan y a qué
   frecuencia, y de qué dato sale cada elemento.
6. **Specs para implementar:** posiciones y tamaños en px de cada elemento (se va a pasar a LVGL).
