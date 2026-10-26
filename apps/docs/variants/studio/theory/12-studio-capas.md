# Studio · Capas de dos engines (HYBRID) — diseño

> **Fecha:** 2026-10-26 · **Estado:** PROPUESTO (falta medir la CPU en el hardware y elegir la primera pareja probando
> los HY) · **Depende de:** `docs/theory/caracter-sonoro.md` §8–9 · paleta-v1 §6 (híbridos HY) · theory/09 (MORPH,
> CC 17) · theory/10 (velocity).

## 1. Qué es

Una nota dispara **dos engines a la vez**: A = ataque / definición, B = cuerpo / peso / espacio (guía de híbridos de
Juan). Hoy se imita grabando las dos mitades HY en Live; esto lo lleva al Brain.

## 2. Alcance de la primera versión (caracter-sonoro §9)

- **Una sola pareja** primero, la que mejor salga de probar los HY en Live (candidata: Prophet + DX7, cuerpo
  sostenido + ataque percusivo). Se generaliza a cualquier pareja solo después de medir.
- **Antes de escribir código:** medir la CPU con la pareja elegida a 6 voces + los 4 SYNTH FX encendidos + UNISON
  apagado. Si pasa de 60 % (`01` §5.2), la capa B se limita a 3 voces o se pide mono.

## 3. Cómo funciona

| Pieza | Diseño |
|---|---|
| **Audio** | `audio_graph` hoy enciende un solo canal de engine (`set_active_engine`). Con capas se encienden dos, con ganancias A / B que da MORPH (crossfade de igual potencia). Las dos cadenas de SYNTH FX y el PUMP quedan después de la suma: procesan la capa entera. |
| **Notas** | `play_on` / `play_off` (después de CHORD y ARP) mandan la nota a A y a B. B puede ir transpuesta (−12 / 0 / +12) y con el ataque retrasado 0–35 ms (cola como la del STRUM). |
| **Polifonía** | En **notas musicales**: la capa da tantas notas como el engine con menos voces (Prophet 5). Con UNISON, cada engine divide las suyas. |
| **MORPH** (CC 17, parada MACRO) | 0 % = solo A, 50 % = las dos, 100 % = solo B; hacia la derecha suma el retraso del ataque de B (guía de Juan). |
| **Edición** | Las páginas OSC / ENV / LFO / SYNTH FX editan la capa **enfocada** (A o B); un control la cambia (a definir con la cabeza, p. ej. SHIFT + B1). La cabeza muestra `A · PROPHET-5` / `B · DX7`. |
| **Preset de capa** | Un banco aparte de 16 capas: nombre, engine y preset de A, engine y preset de B (por **nombre**, para sobrevivir a reordenar), MORPH, transposición y retraso de B. Los patches siguen en el banco de su engine. El bibliotecario exporta también las capas. |

## 4. Cómo se prueba

| Qué | Cómo |
|---|---|
| Ruteo de notas a A y B (transposición, retraso, soltar), crossfade de MORPH, preset de capa ida y vuelta | test native |
| CPU de la pareja con 6 voces + FX; sin xruns | en el hardware (`[stats]`) |
| La pareja suena como la versión grabada en Live con los HY | en el hardware |

## 5. Qué no cubre (todavía)

Cualquier pareja (solo la primera), capas de tres engines, split de teclado (zona A / zona B), estéreo por capa.
