# Kits por género y tonalidad

> **Fecha:** 2026-11-05. **Estado:** primera versión, en prueba de escucha.
> **Base:** `percusion-kits-808-organica.md`. **Código:** `tools/perc-synth/gf_perc/generos.py` y `tonalidad.py`.
> **No modifica** `00–06`.

## 1. Decisiones de Juan

- **Sonidos por género, sin converger.** Se retira la regla de los dos contextos (`sesion-paleta-adn.md` §1c). Un kit de house no
  tiene que funcionar en organic. Esto sustituye la propuesta de que el género solo inclinara un núcleo común.
- **Cuatro géneros iniciales:** organic, house, nu disco e indie tech.
- **Hats abiertos y cerrados** en todos los kits.

## 2. Afinar es sintetizar otra vez

Con pitch shift, cambiar la nota también estira o encoge el sonido. Aquí la nota es un parámetro de síntesis: el decay, el ataque y el
cuerpo conservan su duración, y solo se mueven las frecuencias. Por la misma razón, las notas se fijan en Hz exactos, sin variación
aleatoria de afinación.

| Voces | Regla |
|---|---|
| Kick | Tónica si cae entre 41,2 y 65,4 Hz; si no, la quinta, para no perder peso ni ensuciar el bajo |
| Congas y bongos | Grados de la escala cerca de su registro natural, siempre en orden ascendente |
| Clave, rim y cuerpo de la snare | Tónica o quinta |
| Hats, clap y maraca | Sin afinar: son ruido, y una nota no se percibiría |

La caída de afinación del kick empieza más arriba: la nota medida es **donde se asienta**. Se trabaja en Hz porque Live llama C1 a la
nota MIDI 36, que en notación científica es C2; así se evita la confusión de octavas.

**Riesgo:** una nota fija en cada golpe puede chocar con acordes que cambian. Si un kick en tónica choca, se elige la quinta o se
afina a la nota que más se repite en el bajo. Se decide de oído.

## 3. Los perfiles

Son **hipótesis**, para corregir de oído. Cambian materiales, decay, ataque, brillo, gordura, sala y patrón de prueba.

| | Organic | House | Nu disco | Indie tech |
|---|---|---|---|---|
| Materiales | Piel y madera | Kick y hats clásicos, clap de piel | Piel y hats clásicos | Madera, hats clásicos |
| Decay | Largo | Corto | Medio; hats abiertos largos | Muy corto |
| Ataque | Suave | Definido | Medio | Duro |
| Brillo | Cálido | Medio | Brillante | Oscuro |
| Gordura del kick | Alta | Muy alta | Alta | Media |
| Sala | Más | Poca | Media | Muy poca |
| Hats | Semillas, poco metal | Cerrado en semicorcheas, abierto en contratiempo | Abierto en contratiempo y protagonista | Secos, pocos abiertos |

**Nu disco** comparte base con house, como dijo Juan. Se diferencia por el cuerpo vivo: snare de parche, hats abiertos largos y
congas tocadas.

Los BPM y patrones son solo de prueba. El ritmo final vendrá del paso de percusión del workflow.

## 4. Hats como pareja

Un hat abierto y uno cerrado son el mismo instrumento tocado de dos formas. Por eso:

1. **Comparten timbre:** ambos usan la misma `seed_timbre`, y por tanto el mismo filtro y la misma afinación.
2. **Se eligen juntos:** la selección se hace sobre el cerrado y el abierto lo acompaña.
3. **Se cortan entre sí:** al entrar el cerrado se apaga el abierto, como en un hi-hat real. El loop lo aplica, y el Drum Rack lo hará con
   un grupo de choke cuando se genere.

## 5. Generación bajo pedido

No se pre-generan todas las combinaciones. Se pide, por ejemplo, `organic`, La menor y 122 BPM, y salen una base y pocas opciones
por voz, con su archivo de audición y un loop. Las más distintas se eligen igual que antes (`percusion-kits-808-organica.md` §10), con
variación menor, para no salir del perfil.

## 6. Cómo se valida

**Tests:** en las 12 tonalidades, la fundamental de kick, congas, bongos y clave cae a menos de 25 cents de su nota. Con todas las capas, en kick, congas y bongos se acepta hasta 80 cents medidos, porque la caída de afinación y los parciales desplazan el pico. En snare y rim el ruido impide medir la nota de forma fiable: solo se comprueba que reciben la frecuencia pedida. El kick queda en su rango, congas y
bongos ascienden y usan notas de la escala, la pareja de hats comparte timbre y el choke corta la cola. **Lo que decide es el oído:**
¿cada kit suena a su género? ¿Choca alguna nota con el bajo? Lo que falle se describe con palabras y se ajusta el perfil.

**Siguiente paso:** registrar lo que Juan elige en cada género, para que los perfiles salgan de sus decisiones.
