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

## 6. Híbridos para probar en Live (guía de motores híbridos de Juan)

Los 8 híbridos de la guía, como **pares A + B** en el banco de cada engine (prefijo HY, sin nombres de artistas,
sellos ni lugares). Hasta que existan las capas (fase 3 de caracter-sonoro), cada mitad se graba en su pista de Live
con la misma parte MIDI y se mezclan allá. Probarlos dice qué parejas valen la pena para la primera capa nativa.

| Híbrido (guía) | A · ataque / definición | B · cuerpo / peso | Mezcla sugerida A / B |
|---|---|---|---|
| 01 Wood bass | DX7 · HY WOODBASS A (golpe de 45 ms) | MODEL D · HY WOODBASS B | −6 dB / 0 dB |
| 02 Kalimba | DX7 · HY KALIMBA A (uña 1:4 de 30 ms) | OB-6 · HY KALIMBA B (BP con LFO lento) | 0 dB / −6 dB |
| 03 French stabs | JUNO · HY FRENCH STAB A | PROPHET · HY FRENCH STAB B (barrido de 15 ms) | 0 dB / −3 dB |
| 04 Acid arp | ARP · HY ACID ARP A (pulso estrecho, PWM) | MODEL D · HY ACID ARP B (triángulo −12, 140 Hz) | 0 dB / −4 dB |
| 05 Desert lead | OB-6 · HY DESERT LEAD A (notch lento) | PROPHET · HY DESERT LEAD B (sync) | 0 dB / −3 dB |
| 06 Shaker & zap | ARP · HY SHAKER ZAP A (ruido, filtro por velocity) | DX7 · HY SHAKER ZAP B (campana sorda) | 0 dB / −12 dB |
| 07 Roller sub | MODEL D · HY ROLLER SUB A (drive alto) | JUNO · HY ROLLER SUB B (casi solo el SUB) | 0 dB / −6 dB |
| 08 Retro brass | PROPHET · HY RETRO BRASS A | JUNO · HY RETRO BRASS B (PWM a 4', CHORUS II) | 0 dB / −10 dB |

Lo que la guía pide y todavía no existe: HYBRID MORPH (balance A/B y retraso del ataque de B, CC 17) y el selector
de pareja (por preset, theory/09 y caracter-sonoro §8). Llegan con las capas. El retraso del ataque de B se puede
imitar en Live corriendo la pista B unos 10–35 ms.

