# Template de envíos con Valhalla

> **Fecha:** 2026-11-05. **Estado:** primera versión para probar de oído. **Sustituye** los ajustes de reverb que Juan compartió ese mismo día.
> **Verificado contra:** ValhallaVintageVerb 4.0.0 y ValhallaDelay 2.5.1 instalados en el MacBook, con `tools/live-template/make_valhalla_presets.py`.

## 1. Qué se comprobó

Se ajustó cada valor en el plugin instalado y se leyó lo que devolvió: **todos los parámetros y modos existen**, y los valores de este
documento son los que el plugin muestra. Algunos quedan apenas distintos de un número redondo, como 2,01 s o 7990 Hz, porque el control avanza en pasos.

**Lo que esto no comprueba** es cómo suenan: los valores salen de los ajustes de Juan y de criterio técnico, y se corrigen de oído.

### Qué no correspondía en los ajustes anteriores

| Anterior | Real o corrección |
|---|---|
| Sin `Mix` | **Mix al 100 %** en cada retorno. Es lo más importante: si no, la señal seca se suma otra vez |
| Rangos como 0,4–0,7 s | Un preset guarda un solo valor; aquí se eligió uno |
| Rumble sin `Color` | `seventies`, que es lo que decía su título |
| Rumble con `LowCut` 250 Hz | Quitaba el grave que el retorno busca; ver §3 |
| Reverb nativa con `Algorithm`, `Wander` y `Size` en % | No se usa aquí. Hasta donde sé, esos controles no corresponden a la Reverb de Live, y no se verificó |

## 2. Reverbs: ValhallaVintageVerb

Los nombres son los de la pantalla del plugin, comprobados con una captura de Juan. `Ambience` está en el menú `Mode`, entre `Chorus Space` y `Sanctuary`.

| Control | A · Office | B · Plate | C · Hall | D · Rumble |
|---|---|---|---|---|
| **Mode** | Ambience | Plate | Concert Hall | Dirty Hall |
| **Color** | 1970s | 1980s | NOW | 1970s |
| **Mix** | 100 % | 100 % | 100 % | 100 % |
| **PreDelay** | 7,95 ms | 18,02 ms | 24,95 ms | 28,00 ms |
| **Decay** | 0,55 s | 2,01 s | 3,10 s | 4,20 s |
| **Size** | 25 % | 38 % | 50 % | 55 % |
| **Attack** | 0 % | 3 % | 5 % | 3 % |
| **BassMult** | 0,80 X | 1,00 X | 1,00 X | 1,20 X |
| **BassFreq** | 200 Hz | 200 Hz | 250 Hz | 250 Hz |
| **HighShelf** | 0 dB | −1,01 dB | −1,99 dB | −3,00 dB |
| **HighFreq** | 10010 Hz | 7990 Hz | 6510 Hz | 6000 Hz |
| **Diff Early** | 100 % | 100 % | 95 % | 90 % |
| **Diff Late** | 100 % | 100 % | 95 % | 90 % |
| **Mod Rate** | 0,40 Hz | 0,50 Hz | 0,60 Hz | 0,70 Hz |
| **Mod Depth** | 6 % | 12 % | 18 % | 22 % |
| **HighCut** | 10010 Hz | 7990 Hz | 6790 Hz | 2990 Hz |
| **LowCut** | 200 Hz | 180 Hz | 250 Hz | 40 Hz |

## 3. Rumble

Este retorno busca una cola grave que empuja detrás del kick. Por eso `LowCut` baja a 40 Hz y `HighCut` a unos 3 kHz.

Después de la reverb, en el mismo retorno, en este orden:

1. **EQ Eight:** paso bajo cerca de 300 Hz, para dejar solo el cuerpo.
2. **Compressor:** sidechain desde la pista `SC GHOST` (§3.1), para que la cola se abra entre golpes.
3. **Utility:** `Bass Mono` activado, cerca de 120 Hz, para que el grave quede centrado.

### 3.1 Pista fantasma de sidechain

**Técnica de Juan:** el sidechain no se dispara con el kick, sino con una pista MIDI que no suena, con notas muy cortas en los mismos golpes, usando un rimshot. Así no aplana tanto.

**Por qué funciona:** un kick largo mantiene el detector por encima del umbral durante todo su grave, y el compresor sigue bajando. Un golpe de pocos milisegundos solo dispara la bajada, y la recuperación queda en manos del `Release`. El sidechain tampoco cambia al cambiar de kick.

| Ajuste | Valor |
|---|---|
| **Pista** | MIDI, `SC GHOST`, con un Simpler y un rim corto |
| **Salida** | `Audio To`: `Sends Only`, con todos sus envíos al mínimo. No se usa Mute |
| **Compressor** | `Audio From`: `SC GHOST`; detección `Peak`, porque `RMS` puede reaccionar menos a un golpe tan corto |

**Primera propuesta, por comprobar de oído:** la bajada corta puede no despejar toda la cola del Rumble; se compensa con `Release`.

Empieza enviando solo el kick. El bajo se añade si hace falta, porque los dos juntos pueden embarrar. **Si buscas una cola sin graves**, vuelve a `LowCut` 250 Hz, `HighCut` 6000 Hz y quita el EQ.

## 4. Delays: ValhallaDelay

Los nombres de `E · Delay` se cotejaron con una captura de Juan en modo HiFi. La pantalla cambia según el modo y el estilo: en esa vista no aparecen `Repeat/Swell`, `Wow`, `Flutter` ni `Ducking`. Los de `F · Echo` aún no se han cotejado.

| Control | E · Delay | F · Echo |
|---|---|---|
| **Mode** | HiFi | Tape |
| **Era** | Present | Past |
| **Style** | PingPong | Single |
| **Repeat/Swell** | Repeat | Repeat |
| **Delay L, sync** | Dotted | Note |
| **Delay L, nota** | 1/8. | 1/4 |
| **Delay R** | Dotted, 1/8. | No aplica |
| **Spread** | No aplica | 5 ms |
| **Mix** | 100 % | 100 % |
| **Feedback** | 30 % | 42 % |
| **Width** | 100 % | 60 % |
| **Color Drive** | 0 dB | 3 dB |
| **Color Age** | 0 % | 35 % |
| **Diff Amount** | OFF | OFF |
| **EQ Low** | 200 Hz | 250 Hz |
| **EQ High** | 6000 Hz | 3490 Hz |
| **Mod Rate** | 0,50 Hz | 0,50 Hz |
| **Mod Depth** | 5 % | 0 % |
| **Wow** | 0 % | 25 % |
| **Flutter** | 0 % | 20 % |
| **Ducking** | 0 % | 0 % |

**`Repeat/Swell`:** si aparece en pantalla con otro estilo o modo, ponlo en Repeat.

**E · Delay** es el limpio, para dar movimiento y ancho. **F · Echo** es el oscuro y gastado, para golpes puntuales.

## 5. Envíos generales de partida

Son valores de arranque, que se corrigen de oído. `—` significa sin envío.

| Rol | A · Office | B · Plate | C · Hall | D · Rumble | E · Delay | F · Echo |
|---|---|---|---|---|---|---|
| Kick | — | — | — | −18 dB | — | — |
| Bajo | — | — | — | — | — | — |
| Clap, snare | −20 dB | −24 dB | — | — | — | — |
| Hats, shakers | −18 dB | — | — | — | — | — |
| Congas, bongos, percusión | −16 dB | — | −28 dB | — | −28 dB | — |
| Claves, rim | −20 dB | — | — | — | −24 dB | −30 dB |
| Plucks, stabs | — | −20 dB | — | — | −20 dB | — |
| Leads, marimba | — | −18 dB | — | — | −24 dB | −28 dB |
| Pads, atmósferas | — | — | −16 dB | — | — | — |
| Arpegios | — | — | −20 dB | — | −24 dB | — |

## 6. Regla de uso

- **Lo general va en los retornos.** Los retornos no se cambian para una sola pista.
- **Lo específico va en la pista,** con su propia reverb o delay como insert, por ejemplo una reverb invertida o un delay de un solo acento.
- **Si un retorno cambia en un track,** ese ajuste se guarda como variante, y el general se mantiene.

## 7. Cómo montarlo

1. En un set vacío de Live, crea seis retornos con los nombres de §2 y §4.
2. Carga el plugin en cada uno, pon sus valores y guárdalo. Crea `KICK` y `SC GHOST` (§3.1), y añade la cadena de §3 al Rumble.
3. Guarda el set en `Templates` como template y prueba con un loop.

El motor usará este template creando pistas y aplicando la tabla de §5; nunca modificará el archivo.

**Pendiente:** cargar estos valores automáticamente mediante el Remote Script, si Live expone los parámetros de los plugins. Sin esto, se escriben a mano una vez.
