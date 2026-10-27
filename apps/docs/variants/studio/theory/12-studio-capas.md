# Studio · Capas de dos engines (HYBRID) — diseño

> **Fecha:** 2026-10-26 · **Estado:** v1 APROBADA por Juan ("implementa todas las capas híbridas para empezar a probar",
> aceptando el riesgo de CPU; se mide en el hardware) · **Depende de:** `docs/theory/caracter-sonoro.md` §8–9 · paleta-v1 §6 (híbridos HY) · theory/09 (MORPH,
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

## 3b. Versión 1 implementada (2026-10-26)

Juan pidió las **8 capas de su guía** para probar, sin esperar la medición. Diferencias con §2–3:

| Pieza | v1 |
|---|---|
| Parejas | Las 8 de la guía (paleta-v1 §6), cada una apunta a sus mitades HY **por nombre** (si borraste una del banco, se usa la de fábrica) |
| Elegir capa | **SHIFT + B2** recorre OFF → 1 … 8 → OFF; **CC 18** la elige desde Live (0 = OFF, después 8 tramos iguales). Cambiar de engine o cargar un preset normal sale de la capa |
| Mezcla | Ganancia base A / B de la tabla de paleta-v1 §6; **MORPH** (CC 17 o ENC R en la parada MACRO) hace un crossfade de igual potencia: 0 % solo A, 50 % las dos a su ganancia base, 100 % solo B. Al cargar la capa MORPH queda en 50 % |
| B | Transposición y retraso del ataque fijos por capa (0–20 ms); su patch es el de su preset HY (no se edita en v1: las páginas editan A) |
| Filtro | ENC L / R mueven A y B juntos (el cutoff de B sigue a A en proporción, la resonancia con el mismo paso) |
| LFO | Cada capa con su propio LFO (el de su patch) |
| GROOVE | ARP / CHORD / UNISON / PUMP del patch de A; el ARP y el CHORD disparan las dos capas |
| Cabeza | Chip **LAYER** en PERFORM (bit 4 de `flags` en UI_STATE) |
| CPU | Riesgo aceptado: se mide en el hardware. Si el pico pasa de ~85 % aparecen clicks; se baja la polifonía de B |

## 3c. Modo HYBRID: entrar, elegir la capa y editar las dos (rev. 3, 2026-10-27)

Juan pidió una forma de **entrar** al modo híbrido con un flujo propio donde se editen **las dos capas**.

**Entrar y salir:** **SHIFT + B2** entra al modo HYBRID (carga la última capa, la primera vez la 1) y abre su página
de inicio; SHIFT + B2 otra vez sale (queda A sola). CC 18 desde Live también carga una capa y entra al modo, sin
cambiar de página.

**Página HYBRID** (página 13, la "home" del modo; NAV largo vuelve acá en lugar de PERFORM):

```
[ WOODBASS · KALIMBA · FRENCH STAB · ACID ARP · DESERT LEAD · SHAKER ZAP · ROLLER SUB · RETRO BRASS ]  ← NAV elige, push carga
┌─ A · DX7 ─────────────┐   MORPH ▮▮▮▮▯ 50 %    ┌─ B · OB-6 ────────────┐
│ HY KALIMBA A   (foco) │   B +0 ST · 0 MS      │ HY KALIMBA B          │
└───────────────────────┘                       └───────────────────────┘
```

| Control | En la página HYBRID |
|---|---|
| NAV / NAV push | Elegir capa / cargarla |
| **P1 push / P2 push** | **Foco en A / foco en B** |
| P1 | MORPH (balance A / B, CC 17) |
| P2 · SHIFT + P2 | Retraso del ataque de B (0–35 ms) · transposición de B (−24 … +24, de a octava) |
| ENC L / R | Cutoff / resonancia de las dos capas (la otra sigue a la del foco) |
| B1 … B4 | OSC · ENV · LFO · SYNTH FX **de la capa en foco** (las páginas de siempre) |
| SHIFT + B4 | PRESETS **de la capa en foco**: cargar un preset cambia esa mitad sin salir de la capa; SAVE guarda esa mitad en el banco de su engine |
| SHIFT + NAV push | ENGINE SELECT: cambia el engine **de la capa en foco** (capa CUSTOM); no puede ser el mismo que el de la otra capa |

- **SYNTH FX y GROOVE son de la capa entera** (los toma de A): abrir esas páginas pasa el foco a A.
- La transposición, el retraso y el engine elegido valen para la sesión; al cargar una capa de la lista vuelven a los de
  la tabla. Guardar capas propias llega con el banco de capas.
- **Protocolo:** `0x46 LAYER_STATE` (Teensy → cabeza, rango de UI de theory/04 §5): `[capa i8 (−1 = sin capa),
  cursor, engine A, engine B, foco, MORPH %, retraso ms, transposición i8, nombre de la capa (12), preset A (16),
  preset B (16)]`. Se manda al cambiar algo y al conectar.
- **Luces:** el anillo muestra la capa del cursor (8 posiciones) y B2 queda encendida en el modo.

## 4. Cómo se prueba

| Qué | Cómo |
|---|---|
| Ruteo de notas a A y B (transposición, retraso, soltar), crossfade de MORPH, preset de capa ida y vuelta | test native |
| Modo HYBRID: entrar / salir, NAV y push, foco con P1 / P2 push, NAV largo vuelve a HYBRID, LAYER_STATE ida y vuelta | test native |
| CPU de la pareja con 6 voces + FX; sin xruns | en el hardware (`[stats]`) |
| La pareja suena como la versión grabada en Live con los HY | en el hardware |

## 5. Qué no cubre (todavía)

Cualquier pareja (solo la primera), capas de tres engines, split de teclado (zona A / zona B), estéreo por capa.
