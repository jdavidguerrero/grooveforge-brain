# Percusión orgánica por síntesis: teoría y diseño del primer generador

> **Fecha:** 2026-11-05. **Estado:** primera versión, en prueba de escucha.
> **Código:** `tools/perc-synth/`. Todo es Python propio (no usa Teensy Audio Library),
> fuera de tiempo real, y no toca el firmware ni el plugin.
>
> **No modifica** `00–06`. **Se relaciona con:** `percusion-groove.md` (lo añade como fuente nueva, junto a Tribe y los
> kits) y `motor-asistencia.md` §1, cuya v1 "no genera audio". Este generador es una herramienta aparte; falta decidir
> cómo se conecta con el workflow completo.

## 1. Objetivo

Generar percusión que suene **tocada y orgánica**: madera, piel y semillas, no "drum machine". El objetivo viene del ADN
de Juan (`caracter-sonoro.md`, `sesion-paleta-adn.md`): bajo groovy, percusión étnica muy nativa y melodías profundas.

**No busca reemplazar a Tribe.** Tribe son instrumentos reales grabados, y sus articulaciones y su toque son difíciles de
igualar. Esto busca sonidos **propios**, que se pueden variar sin límite, y mezclarlos con lo grabado.

## 2. Por qué suena a máquina la percusión sintética

La pregunta central no es "¿qué oscilador uso?", sino qué le falta a un golpe clásico de caja de ritmos.

| Rasgo de un instrumento tocado | Rasgo típico de una máquina | Solución en este generador |
|---|---|---|
| Parciales inarmónicos, fijados por la física del cuerpo | Uno o dos osciladores | Bancos modales con relaciones físicas (§3) |
| El material fija cómo decae cada parcial | Una sola envolvente | Decay que depende de la frecuencia (§4) |
| El lugar del golpe cambia el timbre | Siempre igual | Amplitud modal según la posición (§5) |
| La mano o el palo filtran el golpe | Ataque instantáneo | Pulso de contacto con duración física (§6) |
| Dos golpes nunca son idénticos | Efecto ametralladora | Golpe distinto en cada repetición (§8) |
| Suena en un espacio | Totalmente seco | Reflexiones tempranas muy cortas (§9) |

## 3. Síntesis modal

Un cuerpo golpeado vibra como una suma de **modos**: cada uno es una senoidal amortiguada con frecuencia, decay y
amplitud propios (Adrien, 1991; Cook, 2002):

```text
y(t) = Σ a_k · exp(−σ_k · t) · sin(2π · f_k · t)
```

Se elige este método porque cada parámetro tiene significado físico: tamaño, tensión, material o punto de golpe. Eso
permite pedir variaciones con sentido, a diferencia de mover parámetros opacos.

**Membrana (tambor).** Una membrana circular ideal tiene modos en los ceros de las funciones de Bessel (`j_mn`). Sus
relaciones son inarmónicas: 1,00 · 1,59 · 2,14 · 2,30 · 2,65 · 2,92… respecto al modo (0,1) (Fletcher y Rossing, 1998,
cap. 18). El aire y el cuerpo desplazan esas relaciones en un tambor real, así que se añade un parámetro que las comprime.
Se afina con el modo (1,1), porque en un golpe abierto de borde suele ser dominante.

**Barra (clave).** Una barra libre tiene relaciones 1 · 2,756 · 5,404 · 8,933 (Fletcher y Rossing, 1998, cap. 2). Esa
distancia entre parciales es parte de lo que la hace sonar a madera golpeada y no a tono.

**Detalle que da vida.** En una membrana real los modos con m > 0 se separan en dos frecuencias muy cercanas, porque la
tensión nunca es uniforme. Se oyen como un batido lento. Se sintetiza partiendo cada modo en dos, con una separación
distinta en cada golpe.

## 4. Material: cómo decae cada modo

La tasa de decay en función de la frecuencia es una de las señales más fuertes para percibir el material (Klatzky, Pai y
Krotkov, 2000; Aramaki et al., 2011). Aquí se modela así:

```text
σ(f) = σ0 + σ1 · f + σ2 · f²        T60 = 6,91 / σ
```

- **Madera:** amortiguación alta y agudos que mueren rápido.
- **Piel:** cuerpo más largo; σ2 pequeño y agudos de corta duración.
- **Modos con m = 0:** irradian sonido con mucha eficiencia y por eso pierden energía antes. Se les aplica un factor.

## 5. Dónde se golpea

La amplitud de cada modo es proporcional a su forma en el punto de golpe: `J_m(j_mn · r)`, con r entre 0 (centro) y 1
(borde). De esto salen las articulaciones de mano sin escribir un sonido distinto para cada una:

| Articulación | Punto | Contacto | Otros efectos | Resultado esperado |
|---|---|---|---|---|
| **bajo** | Centro (r ≈ 0,1) | Largo, palma | Solo se excitan modos m = 0; más resonancia de aire | Grave y redondo |
| **abierto** | Borde (r ≈ 0,75) | Medio, dedos | Modos m > 0 libres | Tono claro y afinado |
| **slap** | Borde (r ≈ 0,92) | Muy corto | Graves apagados, más ruido de piel | Seco y brillante |
| **apagado** | Borde (r ≈ 0,7) | Medio | La mano queda encima: mucha más amortiguación | Golpe sordo, sin cola |

## 6. Contacto: mano, baqueta o palo

El golpe no es un impulso. La fuerza dura unos milisegundos, y ese pulso **filtra** el sonido: cuanto más largo, menos
agudos (Hunt y Crossley, 1975). Se usa un pulso `sen²` y se convoluciona con los modos. Una mano dura unos 3–8 ms y un palo
de madera menos de 1 ms. La velocity acorta el contacto, así que **tocar más fuerte también suena más brillante**, sin
filtro añadido.

## 7. Cuerpo, tensión y semillas

- **Aire del cuerpo:** un modo grave (resonancia de Helmholtz) que da el peso del tambor. Lo excita sobre todo el golpe de
  centro.
- **Casco:** un modo breve que imita el golpe transmitido a la madera.
- **Ruido de piel:** ráfaga corta de ruido filtrado para el contacto de la mano.
- **Subida de tensión:** un golpe fuerte estira la membrana y sube su afinación un instante; luego cae. Se modela como una
  caída de afinación proporcional a la velocity (Bilbao, 2009, para el fenómeno; aquí es una aproximación simple).
- **Semillas (maracón, guache):** se usa PhISEM (Cook, 1997). Muchas partículas chocan al azar: la energía del gesto de agitar
  fija cuántos choques ocurren, cada choque excita ruido breve, y el resultado pasa por resonadores del recipiente.

## 8. Timbre fijo y golpe variable

Hay **dos semillas aleatorias**, y la separación importa:

- **`seed_timbre`:** define el instrumento, es decir, tamaño, afinación y material. Si te gusta una variante, se conserva.
- **`seed_golpe`:** define la ejecución, es decir, punto exacto, fuerza y separación de modos. Cambia en cada repetición.

Así se puede exportar un mismo instrumento con varios golpes para round-robin, y dos corridas con iguales semillas producen
exactamente el mismo audio.

## 9. Espacio

Pocas reflexiones de 3–25 ms y una cola muy breve evitan la sensación de sonido dentro de la máquina. Se usa la misma sala
para todos los golpes de una versión. El nivel es bajo a propósito y se puede poner en cero para procesar después.

## 10. Alcance de esta primera prueba

| Familia | Instrumentos | Técnica |
|---|---|---|
| Madera | `clave`, `guadua` | Barra modal; `guadua` añade resonancia de tubo |
| Piel | `llamador`, `alegre`, `bombo`, con 4 articulaciones | Membrana modal + cuerpo |
| Semillas | `maracon`, `guache` | PhISEM |

Sustituye a los hats propuestos antes: en este ADN los shakers cumplen mejor esa función, y los hats vienen después.

Los nombres indican **inspiración**. Los valores son puntos de partida elegidos con criterio físico, no mediciones de
instrumentos colombianos reales. No se usa audio de Tribe en ningún paso.

**No cubre todavía:** kick, snare, clap, hats, filtro con CLAP, ni el workflow completo.

## 11. Cómo se valida

**Tests automáticos** (`tools/perc-synth/tests/`), solo lo medible: sin NaN ni clipping, determinismo, golpes distintos entre
sí, frecuencias modales correctas, golpe de centro sin modos m > 0, madera con menos decay que piel, y coherencia de la
velocity con el brillo.

**Lo que decide es tu oído.** Los tests no dicen si suena orgánico. Criterio de la prueba:

1. Oír el loop de demostración y los golpes sueltos.
2. Compararlo a oído en el mismo loop con Tribe, sin procesar ni extraer audio de Tribe.
3. Por familia: ¿suena a madera, a piel o a semilla? ¿Hay al menos un sonido que usarías?
4. Lo que falle se describe con palabras y se ajusta la receta.

Con el resultado se decide si vale la pena ampliar el generador. Si nada convence, se registra y se prioriza procesar
muestras reales.

**Siguiente mejora posible:** medir modos y decays en samples propios con licencia clara para calibrar las recetas contra
instrumentos reales (análisis-síntesis).

## 12. Referencias

- Adrien, J.-M. (1991). «The missing link: Modal synthesis». En *Representations of Musical Signals*. MIT Press.
- Aramaki, M., Besson, M., Kronland-Martinet, R. y Ystad, S. (2011). «Controlling the perceived material in an impact sound
  synthesizer». *IEEE Transactions on Audio, Speech, and Language Processing*, 19(2).
- Bilbao, S. (2009). *Numerical Sound Synthesis*. Wiley.
- Cook, P. (1997). «Physically Informed Sonic Modeling (PhISM): Synthesis of Percussive Sounds». *Computer Music Journal*, 21(3).
- Cook, P. (2002). *Real Sound Synthesis for Interactive Applications*. A K Peters.
- Fletcher, N. y Rossing, T. (1998). *The Physics of Musical Instruments*, 2.ª ed. Springer.
- Hunt, K. y Crossley, F. (1975). «Coefficient of restitution interpreted as damping in vibroimpact». *Journal of Applied
  Mechanics*, 42(2).
- Klatzky, R., Pai, D. y Krotkov, E. (2000). «Perception of material from contact sounds». *Presence*, 9(4).
