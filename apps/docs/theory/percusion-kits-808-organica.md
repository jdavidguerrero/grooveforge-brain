# Kits tipo 808 con material: madera y piel

> **Fecha:** 2026-11-05. **Estado:** primera versión, en prueba de escucha.
> **Base:** `percusion-organica-dsp.md`, cuyos modos, contacto y sala se reutilizan. **Código:** `tools/perc-synth/gf_perc/kit.py`.
> **No modifica** `00–06`.

## 1. Decisión de Juan

Los instrumentos reales ya los cubre Tribe. Lo que el generador debe producir son **kits de batería completos**: kicks,
snares, claps, hats, claves, bongos y congas, partiendo de la lógica de una 808 pero con sonidos más orgánicos, de madera y piel.

## 2. La idea: conservar la función, cambiar el material

Una 808 es reconocible por la **función** de cada voz: un kick con grave largo, una clave corta y aguda, un clap con varias
ráfagas. Su sonido electrónico viene de otra cosa: un **resonador casi puro** para cada voz. Por eso cada voz conserva su
función, y lo que cambia es qué vibra:

| Parte | Se conserva en todos los kits | Cambia con el material |
|---|---|---|
| Afinación y duración base | Sí | Pequeñas diferencias |
| Grave del kick | Sí: es lo que hace bailar | No: siempre hay fundamental limpia |
| Parciales | No | Ninguno (`clasico`), barra (`madera`), membrana (`piel`) |
| Ataque | No | Clic, golpe de madera o contacto de mano |
| Espacio | No | Nada (`clasico`), sala muy corta en los demás |

La regla para kicks: **el material va encima del grave, no lo reemplaza**. Sus parciales se colocan unas octavas arriba para no
ensuciar el sub.

## 3. Referencia: cómo funciona la 808

Se usa como arquitectura, con estos puntos de partida (Roland, 1981; Werner et al., 2014):

| Voz | Circuito de la 808 | Valores de partida |
|---|---|---|
| Kick | Resonador con caída de afinación y clic | Unos 52 Hz |
| Snare | Dos resonadores + ruido agudo | 180 y 330 Hz |
| Rim | Dos resonadores muy cortos | 455 y 1667 Hz |
| Clave | Un resonador muy corto | Unos 2,5 kHz |
| Congas | Resonador con pequeña caída de afinación | Bajo, medio y alto |
| Clap | Ruido filtrado en tres ráfagas, más cola | Banda cerca de 1 kHz |
| Hats | Seis osciladores cuadrados, filtrados | 205,3 · 304,4 · 369,6 · 522,7 · 540 · 800 Hz |
| Maraca | Ruido agudo muy corto | |

Los hats usan ondas cuadradas sintetizadas por suma de armónicos para evitar aliasing. Es una reconstrucción por modelo, sin samples
de una 808; la voz `clasico` es una **referencia de comparación**, no una emulación exacta.

## 4. Voces por material

| Voz | `madera` | `piel` |
|---|---|---|
| Kick | Grave + golpe de madera, tipo cajón | Grave + parche y mazo, tipo bombo |
| Snare | Cuerpo de madera, poco ruido de bordón | Parche + bordón |
| Clap | Choques de palos; ráfagas con resonancia de madera | Palmas: ráfagas irregulares y más graves |
| Rim y clave | Barra modal | Golpe de aro con ruido de piel |
| Congas y bongos | Tono de cajón o caja de madera | Membrana modal, con batido entre modos |
| Hats | Semillas (PhISEM) y un tic de madera | Roce sobre piel, con un poco de metal de 808 |
| Maraca | Semillas en recipiente de madera | Semillas en recipiente más blando |

Los hats no existen como instrumento de madera o piel, así que son **los más experimentales**.

## 5. Los cuatro kits

- **`808_referencia`:** arquitectura clásica, para comparar.
- **`808_madera`** y **`808_piel`:** todo el kit en un material.
- **`808_mixto`:** una combinación por voz. Kick y congas de piel, snare y clave de madera, por ejemplo.

Todos tocan el mismo patrón de prueba, y así la comparación solo depende del sonido.

## 6. Notas en Live

Se reutiliza el **GF Perc Map** (`percusion-groove.md` §3) y la convención de `GF 808 Perc`, para que los patrones existentes
suenen sin cambios. Lo que el mapa no contempla va desde la nota 51, como ya hace `make_gf_racks.py`.

| Nota | Voz | Nota | Voz |
|---|---|---|---|
| 38 | Conga baja | 47 | Clave |
| 39 | Conga media | 48 | Snare |
| 40 | Conga alta | 49 | Clap |
| 41 | Bongo alto | 51 | Kick |
| 42 | Rim | 52 | Hat cerrado |
| 43 | Maraca | 53 | Hat abierto |
| | | 54 | Bongo bajo |

Tu mapa y este archivo dejan el kick fuera del mapa común; aquí se añade como nota 51 solo para tener el kit completo.

## 7. Cómo se valida

**Tests:** kick con la mayor parte de su energía en graves en todos los materiales, hat abierto más largo que el cerrado, el clap
con varias ráfagas, y cada material distinto de los demás. **La decisión es de escucha:** comparar los cuatro kits con el mismo patrón,
elegir voces por material y describir lo que falla.

## 8. Referencias

- Roland Corporation (1981). *TR-808 Service Notes*.
- Werner, K., Abel, J. y Smith, J. (2014). «A Physically-Informed, Circuit-Bendable, Digital Model of the Roland TR-808 Bass Drum
  Circuit». *Proceedings of DAFx-14*.
- Ver también las referencias de `percusion-organica-dsp.md`.

## 9. Rev. 2 (2026-11-05): kick más gordo

**Escucha de Juan:** le gustan los tres materiales, pero el kick se queda corto. Lo quiere más gordo, y considera que ese rasgo es
común al house en general.

"Gordo" no es solo más sub: un altavoz pequeño casi no reproduce 50 Hz, y un kick puede tener mucho sub y sonar delgado. Lo que se
percibe como cuerpo depende sobre todo de cuatro cosas:

| Cambio | Efecto |
|---|---|
| **Capa de cuerpo** a unos 95–130 Hz | Peso en el rango donde el kick se oye y empuja |
| **Caída de afinación más lenta** | El golpe tarda más en asentarse y se siente más grande |
| **Saturación suave y algo asimétrica** | Crea armónicos pares e impares del sub; sube el nivel medio sin subir el pico |
| **Grave un poco más largo** | Más sostén, con el riesgo de chocar con el bajo |

El último punto importa: un grave más largo deja menos espacio para el bajo. Por eso `gordura` es un parámetro continuo, con
`0` equivalente al kick anterior, para elegir según la línea de bajo.

La medida de éxito sigue siendo el oído; los tests solo comprueban que sube la energía de 60–250 Hz y el nivel medio.

## 10. Rev. 2: variaciones de one-shots

Cada variante conserva voz y material y varía **parámetros con sentido**: afinación, decay, cantidad de material, ataque, brillo y,
en el kick, gordura.

Oír muchas variantes casi iguales es tiempo perdido, así que se generan más candidatas de las necesarias y se conservan las **más
distintas** con selección *farthest-first*: se empieza por la versión base y se añade cada vez la candidata más lejana a las ya
elegidas. La distancia usa energía en bandas logarítmicas y duración. Es más simple que CLAP, no necesita descargar modelos y sirve
para esta etapa; CLAP entraría cuando haga falta buscar por descripción.

Cada voz trae además un archivo de audición con todas sus variantes en fila, en el orden de sus números, para elegir sin arrastrar
archivo por archivo.
