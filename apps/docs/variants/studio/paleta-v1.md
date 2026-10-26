# Studio · Paleta de sonidos v1 (método de trabajo)

> **Fecha:** 2026-10-26 · **Estado:** guía de trabajo de Juan · **Depende de:** `docs/theory/caracter-sonoro.md` §9–10
> (manifiesto) · theory/10 (velocity) · theory/11 (bibliotecario).

Objetivo: tener, para cada rol de un track de 122–126 BPM, **3 o 4 sonidos propios probados juntos**, con nombre y
ficha. Esa paleta arma arreglos rápido y es el corpus que después lee la IA local.

## 1. Roles y por dónde empezar

| Rol (`role`) | Engines del Brain | Punto de partida (presets que ya existen) |
|---|---|---|
| `bass` | Model D, Juno, ARP | GF POCKET BASS, JD RUN FUNKBASS, FAT BASS, SUB BASS |
| `chords` | Juno, Prophet, DX7 | GF FM DISCO STAB, JD RAP STAB JUNO, BRASS STAB |
| `pad` | Juno, OB-6 | GF PUMP PAD, JD HORIZON NOTCH, PWM PAD, BP PAD |
| `lead` | OB-6, Prophet, ARP | GF MELODIC LEAD, JD RUN LEAD BODY, SYNC LEAD |
| `pluck` | DX7, Prophet | GF ORGANIC ARP, MARIMBA, E.PIANO, JD VELVET RHODES |
| `perc` | DX7, ARP | JD ZAP PERC WOOD, JD ZAP PERC ARP, PERC BLIP |
| `fx` | ARP, OB-6 | S&H BURBLE |

Kicks: en **Kick 3**. Percusión latina y colombiana: **Tribe Instruments**. El Brain los complementa (bajos que dejan
lugar a la tambora, percusión tonal de madera y metal, una marimba que dialoga con la de Tribe).

## 2. El loop de prueba (siempre el mismo)

8 compases a **124 BPM** en Live: un kick de Kick 3, un loop de percusión de Tribe Instruments y un hat. El Brain entra
por una pista de audio (monitor en In) y recibe clock (Sync) para que PUMP y ARP caigan en fase.

## 3. Una sesión = un rol

1. Cargar los puntos de partida del rol y tocarlos sobre el loop.
2. Ajustar a oído. Para bajos y stabs: **VEL → AMP / FILTER** (ENV con SHIFT) para que las ghost notes se sientan.
3. Guardar lo que sirve (PRESETS → SAVE).
4. Al final: `gf_presets.py export` y completar la ficha de cada JSON nuevo.

## 4. La ficha (en el JSON del bibliotecario)

| Campo | Qué poner | Ejemplo |
|---|---|---|
| `name` | Nombre corto (16 caracteres), prefijo propio | `JD BASS TAMBORA` |
| `role` | Uno de la tabla de §1 | `bass` |
| `tags` | Estilo y carácter | `["afro", "ghost-notes", "warm"]` |
| `key_range` | Registro donde suena bien | `"E1–A2"` |
| `pairs_with` | Con qué combina | `["kick redondo", "tambora", "JD VELVET JUNO"]` |
| `notes` | Lo que no cabe arriba | `"bajar DRIFT en el drop"` |

Al reexportar, el bibliotecario conserva la ficha y solo actualiza los parámetros.

## 5. Cuándo está lista la paleta v1

Cada rol de §1 con al menos 3 sonidos fichados y probados juntos en el loop, y un arreglo de prueba de 16 compases armado
solo con la paleta. Los bancos tienen 16 presets por engine: lo que no se use se saca con `gf_presets.py bank`.
