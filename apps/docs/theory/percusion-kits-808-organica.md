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

## 11. Rev. 3: caracteres de kick

**Escucha de Juan:** las tandas sonaban a los mismos kicks. La medición le dio la razón: en 8 de 9 kicks, el grave variaba menos de 1 dB y la cola entre 268 y 306 ms. Había tres causas: la primera opción era siempre la misma, la selección premiaba diferencias sobre 2 kHz a −35 dB y todo salía de una sola receta.

**Decisión de Juan:** quiere varias opciones, porque elige el kick según el bajo; suele gustarle el corto y seco, así que ese carácter trae dos variantes; y pide algo más acústico.

Por eso cada tanda trae **caracteres** y no variaciones al azar:

| Carácter | Idea | Qué cambia |
|---|---|---|
| **GOLPE MADERA** | Corto y seco; deja espacio al bajo | Cola corta, ataque fuerte y parciales de madera |
| **GOLPE PIEL** | Corto, pero más redondo | Cola algo mayor y parciales de parche, con menos clic |
| **ACUSTICO** | Bombo tocado | Modelo propio (abajo) |
| **CUERPO** | Gordo | Mucha energía de 90 a 300 Hz, por la capa de cuerpo y la saturación |
| **SUB** | Largo y profundo | Cola larga y casi sin ataque |

**El bombo acústico** no parte de la 808. Una membrana golpeada tiene sus parciales cerca de la fundamental (Fletcher y Rossing, 1998), mientras que el kick 808 los sube varias octavas para limpiar el sub. Aquí quedan donde están, y se añade:

- **Tensión:** la nota sube muy poco y cae rápido; en una 808 ese barrido es mucho mayor.
- **Mazo de fieltro:** contacto de unos 2 ms, más un golpe cerca de 1,8 kHz. Un contacto más largo apagaba los parciales del parche y dejaba un golpe sordo.
- **Casco:** dos resonancias breves cerca de 200 Hz.
- **Sala:** más que en los demás, porque un bombo se graba en un espacio. No lleva saturación.

Sigue siendo una aproximación física, no una emulación de un bombo concreto, y se ajusta de oído.

**La selección ahora mide lo que se oye:** grave, cuerpo, medios, largo de la cola y pegada. Cada tanda busca lo más lejano a lo ya presentado de ese carácter, así que pedir otra tanda no repite lo oído. Los tests comprueban que los caracteres se separan en estos rasgos; si sirven o no, lo decide el oído.

## 12. Rev. 4: mismo golpe, distinta cola

**Escucha de Juan:** los caracteres de §11 generan variedad, pero la variación que le sirve está en otra parte: un inicio duro, gordo y corto, y después una cola de madera o de piel. Es lo que usa en la tanda; §11 queda en el código por si hace falta.

| Parte | Qué es | Qué cambia |
|---|---|---|
| **Golpe** | Kick gordo y corto, con el mismo código de §9 | Solo entre tandas: una semilla, un golpe |
| **Cola de madera** | Parciales de barra libre, una octava sobre la nota | Largo |
| **Cola de piel** | Modos de membrana sobre la nota, juntos y graves | Largo |
| **Mixta** | Las dos, más suaves | Largo medio |
| **Secas** | Madera, piel o mixta, sin sala | Se apagan justo después del golpe |

**Por qué la cola entra sola.** Empieza a los 18 ms y llega a su nivel a los 68 ms. En un tambor real el ataque enmascara esos modos, y además así el inicio es idéntico en toda la tanda.

**Tres fallos que salieron al medir, antes de oírlo:**

1. Si la cola alargaba también el sub del golpe, el sub tapaba el material y madera y piel medían casi igual. Por eso el golpe tiene largo fijo y la cola lo lleva el material.
2. Con la capa de material normal los parciales agudos morían antes de que la cola entrara, porque decaen muy rápido. Aquí tienen su propio banco, con una caída más lenta.
3. Las colas cortas no se distinguían del golpe solo. Se alargaron.

Las colas pueden batir con el sub del golpe, y eso da pequeñas subidas y bajadas en la envolvente. Es parecido al batido de un parche, pero hay que oírlo: si molesta, se corrige.

**Secas (Juan, para no cerrarse a nada):** el grave del golpe seguiría sonando aunque la cola de material fuera corta. Por eso se aplica una caída desde los 70 ms, cuando el golpe ya pasó, y no se usa sala. El inicio queda igual al resto de la tanda.

## 13. Rev. 5: base medida de la referencia de Juan

**Decisión de Juan:** una captura de Simpler como base, con cola de piel y madera encima. La onda se extrajo de los píxeles de la captura, así que estos valores son aproximados; además, el sample tenía `WARP` activo, y eso puede alterar su duración.

| Rasgo medido | Referencia | En el modelo |
|---|---|---|
| Forma | Seno casi limpio, sin saturación visible | Seno sin saturación |
| Ataque | Clic de unos 2 ms, más bajo que el primer ciclo, y máximo cerca de 17 ms | Clic breve y entrada de 16 ms |
| Barrido | Unos 98 Hz a los 15 ms, unos 49 Hz a los 50 ms, unos 35 Hz al final | 85 Hz sobre la nota, con caída τ = 30 ms |
| Caída | Casi recta, hasta unos 240 ms | `(1 − t/T)^0,9`, T = 240 ms |

**Diferencia que se conserva:** la referencia termina cerca de 35 Hz, mientras que en La menor el kick va a 55 Hz, la nota que fija la regla de tonalidad (`percusion-generos-tonalidad.md`). Por eso el barrido se aplica en Hz sobre la nota.

**La cola tiene más nivel que en §12.** La base ocupa toda la energía hasta 240 ms, así que la cola solo se oye bien después; las secas no llegan a ese punto y llevan más cantidad de material, durante menos tiempo.

**Sustituye:** la cabeza saturada de §12 ya no se usa en las colas. El kick de §9 sigue disponible para los kits.

## 14. Rev. 6: cuerpo

**Escucha de Juan:** de la primera tanda con la base de §13 eligió `PIEL SECA`, `PIEL CORTA` y `MADERA SECA`, todas cortas, pero les falta cuerpo. Dio un segundo ejemplo, sin warp. Medido desde los píxeles, así que es aproximado:

| Rasgo | Segunda referencia | Base de §13 |
|---|---|---|
| Nivel a la mitad de su duración | Cerca del 70 % | Cerca del 55 % |
| Final | Corta a unos 190 ms, a casi la mitad del nivel | Se apaga hasta cero |
| Barrido | De unos 55 a 40 Hz, suave | Mucho mayor |
| Nota final | Unos 40 Hz | 55 Hz en La menor |

El cuerpo no viene de saturar: viene de **sostener el nivel** y de una nota más grave. Por eso la base nueva casi no cae, tiene poco barrido y corta con un fundido de 8 ms. Entre tandas varían el barrido (14–42 Hz), su velocidad y el largo (165–215 ms), para que pedir otra cambie de verdad el golpe.

**Nota grave.** Las dos referencias terminan entre 35 y 40 Hz. La regla de `percusion-generos-tonalidad.md` usa la tónica entre 41,2 y 65,4 Hz, y en La menor da 55 Hz. Aquí, por defecto, se busca entre C#1 y G#1 (34,6–51,9 Hz), primero la tónica y si no la quinta: en La menor sale E1, 41,2 Hz. Sigue en tonalidad, pero **el kick no siempre toca la tónica**. Se puede volver a la nota anterior desde la interfaz. El riesgo: más grave también deja menos sitio al sub, y en altavoces pequeños se oye menos; se juzga junto al bajo.

**La cola entra más tarde** (50–120 ms en vez de 18–68 ms), porque ensuciaba justo los ciclos que dan el cuerpo.

## 15. Rev. 7: cuerpo como opción

**Escucha de Juan:** el cuerpo de §14 le gusta, pero quiere además opciones con menos, porque elige según el bajo. Se añaden cuatro `LIGERA`, sobre las colas cortas y secas, que es donde más ha elegido. El nivel cae casi hasta el final y el golpe dura cerca de un 20 % menos. Las `LIGERA` usan `cuerpo = 0,4`, un punto de partida para corregir de oído.

## 16. Rev. 8: dos familias

**Escucha de Juan:** la base de §14 dejó raros algunos sonidos, sobre todo con madera. Le gustaron `1H`, `2C` y `2L`, y los quiere como base de la familia con cuerpo, junto con versiones hechas como antes.

| Familia | Opciones | Base | Nota en La menor |
|---|---|---|---|
| **CUERPO** | `MIX SECA` (1H), `PIEL CORTA` (2C), `PIEL LIGERA` (2L) | §14 | E1 solo en la primera; las otras dos en A1, como las eligió |
| **INICIAL** | Madera corta, piel corta, mixta, madera seca y piel seca | §13 | A1 |

- **Recetas exactas:** la tanda 1 de CUERPO reproduce esos tres sonidos. En las siguientes cambia el golpe y se conserva la receta.
- **Hipótesis sobre lo raro:** los parciales de barra (×2,756 y ×5,404) son inarmónicos. Sobre una base sostenida siguen sonando contra ella y baten, mientras que una base que cae los enmascara menos tiempo. No está medido; por eso, por ahora, la madera solo va con la base inicial.
- **Nota:** dos de los tres favoritos se eligieron en 55 Hz. Por eso el modo grave deja de ser el predeterminado y se quita el selector.

**Pendiente tras la escucha de §16:** Juan sigue oyendo rara la familia INICIAL y la madera, y prefiere seguir refinando con sus elecciones. No está resuelto.

## 17. Rev. 9: todo a la fundamental

**Regla de Juan (2026-11-06):** en lo posible, todo lo que se genera va a la nota fundamental. Reemplaza la elección entre tónica y quinta de §14 para el kick y la de `percusion-generos-tonalidad.md` para clave y snare en el motor.

| Voz | Cómo se afina |
|---|---|
| Kick | Tónica en C#1–C2 (34,6–65,4 Hz). En La menor, A1. Ya no sale en la quinta |
| Clave y snare | Tónica más cercana a su registro natural |
| Clap y hats | No tienen una nota clara, así que su resonancia principal se alinea con la tónica o con su tercer armónico. Se oirá como color más que como nota, y abierto y cerrado comparten afinación |

**Consecuencia:** `CUERPO MIX SECA` nace de 1H, que Juan eligió en E1. Ahora va en A1, así que ya no es idéntico a aquel. El archivo original sigue en `00 Elegidos`.

**Por qué no solo octavas en clap y hats.** Con una octava entre opciones, una resonancia tendría que moverse hasta un 41 %, y eso cambia el carácter del instrumento. Es la única excepción a «solo fundamental»: el tercer armónico pertenece a la serie de la tónica y deja el movimiento por debajo del 25 %. Por lo mismo, la clave sí se mueve mucho (de unos 2,5 kHz a 1,76 kHz en La), porque ahí la nota se oye; hay que comprobar de oído que siga sonando a clave.
