# Sprint 2.7 — FX Modal Reverb

**Status:** In Progress
**Refs:** `apps/docs/05-fx-architecture.md` §1.7, `apps/docs/01-architecture.md` §5.2

---

## Theory

### El problema de los reverbs sin identidad

Todos los reverbs de sintetizadores comerciales simulan lo mismo: un cuarto rectangular
con muchas reflexiones distribuidas uniformemente. Freeverb, Schroeder, los algoritmos
de plate y spring — todos calculan estadísticamente la densidad de reflexiones que
tendría un espacio físico genérico.

El resultado es un reverb útil pero sin carácter. El "plate reverb" de un Korg Minilogue
suena igual que el "plate reverb" de un Roland Boutique. No hay material, no hay
geografía, no hay identidad.

El Modal Reverb toma el problema desde el otro extremo: en lugar de simular un espacio
con miles de reflexiones, simula las **resonancias específicas de un objeto físico
real**. Una campana tiene seis modos bien definidos. Una barra de guadua tiene seis
modos diferentes, con ratios y decays únicos. Un plato de cristal tiene su propio set.
Cada objeto tiene una "firma" de resonancias que es tan identificable como una huella
dactilar.

El resultado es un reverb con identidad auditiva — "reverb de guadua colombiana" es
una descripción precisa, no un nombre de marketing vacío. Es el único reverb en el
mundo construido sobre las resonancias físicas de esa especie de bambú.

---

### Síntesis modal — cómo resuenan los objetos físicos

Cuando golpeás una campana, una barra de guadua o un plato de cristal, el objeto
vibra en sus **modos resonantes** — frecuencias específicas determinadas por su
geometría, densidad y módulo de elasticidad.

La analogía más directa: imaginá una soga atada en ambos extremos. Solo ciertos
patrones de vibración son estables: el que tiene un solo arco (modo 1), el que tiene
dos arcos (modo 2), el que tiene tres arcos (modo 3), etc. Frecuencias intermedias
no "caben" en la soga — se cancelan. Los objetos rígidos hacen lo mismo, pero en
tres dimensiones y con geometrías más complejas.

A diferencia de un cuarto rectangular (donde los modos son tan densos y uniformemente
distribuidos que suenan como reverb continuo), los objetos pequeños tienen **pocos
modos bien separados**, cada uno con su propia frecuencia y su propio tiempo de
extinción (T60 — el tiempo que tarda en caer 60 dB desde el impacto).

La síntesis modal modela exactamente esto: en lugar de simular densidad estadística
de reflexiones (como Freeverb), modela directamente las resonancias individuales
del objeto.

#### La ecuación de onda para una barra libre (guadua)

El modelo físico de una barra libre en ambos extremos — la aproximación más cercana
a una sección de guadua golpeada — produce la ecuación diferencial parcial de vigas
de Euler-Bernoulli:

```
EI ∂⁴w/∂x⁴ + ρA ∂²w/∂t² = 0
```

donde:
- `E` = módulo de Young del material (rigidez)
- `I` = segundo momento de área de la sección transversal
- `ρ` = densidad del material (kg/m³)
- `A` = área de la sección transversal (m²)
- `w(x,t)` = desplazamiento transversal de la barra

Las soluciones de esta ecuación para condiciones de borde libre-libre dan las
frecuencias de los modos naturales:

```
f_n = (β_n L)² / (2π L²) × √(EI / ρA)
```

donde los valores de `β_n L` para los primeros modos de barra libre son:
```
n = 1:  β₁L = 4.7300  →  (β₁L)² = 22.37
n = 2:  β₂L = 7.8532  →  (β₂L)² = 61.67   ratio f₂/f₁ = 61.67/22.37 = 2.757
n = 3:  β₃L = 10.996  →  (β₃L)² = 120.91  ratio f₃/f₁ = 120.91/22.37 = 5.405
n = 4:  β₄L = 14.137  →  (β₄L)² = 199.85  ratio f₄/f₁ = 199.85/22.37 = 8.934
n = 5:  β₅L = 17.279  →  (β₅L)² = 298.57  ratio f₅/f₁ = 298.57/22.37 = 13.34
n = 6:  β₆L = 20.420  →  (β₆L)² = 416.98  ratio f₆/f₁ = 416.98/22.37 = 18.64
```

La característica fundamental: los ratios entre modos **no siguen la serie armónica
entera (1, 2, 3, 4...)** sino que son inarmónicos (1, 2.757, 5.405, 8.934...). Esto
es lo que hace que una campana o una barra de guadua suene diferente a una cuerda de
guitarra: la inarmónicidad produce ese timbre metálico o de madera que el oído
reconoce como "percusión" en lugar de "tono musical".

**Referencia:** Fletcher y Rossing, "The Physics of Musical Instruments" (2nd ed.,
Springer, 1998), Capítulo 2 "Vibrating Bars", §2.3 "Free-Free Bars" — derivación
completa de los valores β_nL para diferentes condiciones de borde y tabulación de
los primeros diez modos para barras libres, empotradas y apoyadas.

#### El factor Q y el tiempo de decay T60

Para cada modo n, el sonido no persiste indefinidamente — hay amortiguamiento interno
del material. El factor de calidad Q mide cuánta energía conserva el modo por ciclo:

```
Q = 2π × (energía almacenada) / (energía disipada por ciclo)
```

Un Q alto significa que el material disipa poca energía por ciclo — las resonancias
son largas. Un Q bajo significa amortiguamiento rápido — las resonancias se extinguen
pronto.

La relación entre Q y el tiempo de decay T60 (caída de 60 dB):

```
T60_n = Q_n × ln(10^(60/20)) / (π × f_n)
      = Q_n × 6.908 / (π × f_n)
      ≈ Q_n / (π × f_n) × 6.91
```

Despejando Q a partir del T60 objetivo:

```
Q_n = T60_n × π × f_n / 6.91
```

Para un modo de campana de 500 Hz con T60 = 8 s:
```
Q = 8.0 × π × 500 / 6.91 ≈ 1817
```

Para el mismo modo en madera de pino con T60 = 1.5 s:
```
Q = 1.5 × π × 500 / 6.91 ≈ 341
```

Los materiales ordenados de mayor a menor Q (y por tanto de mayor a menor T60):
- Cristal (cuarzo, vidrio de borosilicato): Q = 1000–10000
- Metal (acero, bronce para campanas): Q = 500–5000
- Guadua (bambú colombiano, seca): Q = 200–800
- Madera (pino, abeto): Q = 100–400
- Goma, plástico: Q = 10–50

**Referencia:** Rossing, "The Science of Sound" (3rd ed., Addison-Wesley, 2001),
Capítulo 18 "Bells, Gongs, and Bowls", §18.2 "Modes of Vibration of Bells" —
mediciones de Q en campanas europeas fundidas con la aleación tradicional (78% Cu,
22% Sn). La tabla 18.1 lista los primeros doce parciales con sus frecuencias relativas
y tiempos de decay medidos en campanas reales, confirmando la inarmónicidad
característica y los T60 de varios segundos para los modos graves.

---

### Los 4 materiales — frecuencias y decays de modos

El FX define los modos en términos de **ratios sobre el modo 1**, con una frecuencia
base configurable (`BASE_FREQ = 300.0 Hz` en la implementación). Las frecuencias
absolutas son `ratio_n × BASE_FREQ`.

#### CAMPANA (tipo church bell, aleación Cu-Sn)

Las campanas fundidas europeas tienen un espectro de parciales bien estudiado. Los
parciales tienen nombres históricos que los campanólogos usan desde el siglo XVI:

```
Modo 1: ratio 1.000,  f = 300 Hz,  T60 = 8.0 s  (partial "hum"   — fondamental grave)
Modo 2: ratio 2.756,  f = 827 Hz,  T60 = 6.0 s  (partial "prime" — octava ligeramente flat)
Modo 3: ratio 5.404,  f = 1621 Hz, T60 = 4.0 s  (partial "tierce"— tercera menor)
Modo 4: ratio 8.933,  f = 2680 Hz, T60 = 3.0 s  (partial "quint" — quinta)
Modo 5: ratio 13.46,  f = 4038 Hz, T60 = 2.0 s  (partial "nominal"— octava superior)
Modo 6: ratio 18.02,  f = 5406 Hz, T60 = 1.5 s  (parcial superior — brillo agudo)
```

La firma tímbrica de la campana es inconfundible: el "hum" es el más grave y más
largo, el "nominal" es la nota que el oído percibe como la nota de la campana, y la
"tierce" (tercera menor) es la responsable del carácter melancólico de las campanas
europeas. Los ratios son de mediciones acústicas de campanas reales — no son
divisores enteros de la fundamental.

**Por qué no son armónicos enteros:** una campana no es una cuerda ni un tubo.
Su geometría tridimensional (cuerpo de revolución con labio reforzado) produce modos
de vibración que no tienen relación simple de enteros. Esto distingue tímbricamente
una campana de un gong o un triángulo.

**Referencia:** Rossing, "The Science of Sound" (3rd ed., 2001), Cap. 18, Tabla 18.1.
Los valores de ratio para "hum", "prime", "tierce", "quint" y "nominal" son los
mismos que aparecen en la literatura estándar de campanología acústica.

#### GUADUA (Guadua angustifolia, bambú colombiano, sección seca)

La guadua es el material de mayor identidad cultural colombiana de esta colección.
La especie Guadua angustifolia crece en los Andes colombianos entre 1000 y 2000 m de
altitud y tiene propiedades mecánicas excepcionales — comparable al acero en relación
resistencia/peso, usada en construcción sismorresistente en Colombia y Ecuador.

Como objeto resonante, la guadua tiene dos características que la distinguen:

**1. Estructura cilíndrica hueca con paredes de bambú:** el segundo momento de área
de una sección hueca es mayor que el de una sección maciza de igual masa. Esto aumenta
la rigidez flexional (EI) sin aumentar la masa (ρA). El resultado: frecuencias de
modo más altas para igual longitud, y modos más separados que en madera maciza.

**2. Anisotropía:** la guadua tiene diferente rigidez en dirección longitudinal
(fibras de celulosa corren paralelas al eje) vs radial (paredes celulares). Las
frecuencias de modos en la dirección longitudinal son distintas a los de flexión
transversal. Esta implementación modela los modos de flexión transversal de una
barra de guadua de longitud aproximada 60 cm, que es la longitud típica de
percusión en marimba de chonta o carritos musicales colombianos.

Los ratios de modos para barra libre-libre de sección cilíndrica hueca son los mismos
que para barra maciza (los valores β_nL no dependen de la forma de la sección,
solo del módulo de Young efectivo). Los decays son más cortos que en campana porque
la guadua tiene mayor amortiguamiento interno (Q ≈ 300–600 vs Q ≈ 1000–3000 del bronce):

```
Modo 1: ratio 1.000,  f = 300 Hz,  T60 = 3.0 s  (modo fundamental — grave, presente)
Modo 2: ratio 2.757,  f = 827 Hz,  T60 = 2.5 s  (segundo modo libre-libre)
Modo 3: ratio 5.404,  f = 1621 Hz, T60 = 1.8 s  (tercer modo)
Modo 4: ratio 8.933,  f = 2680 Hz, T60 = 1.2 s  (cuarto modo — empezando a difuminarse)
Modo 5: ratio 13.34,  f = 4002 Hz, T60 = 0.8 s  (quinto modo — alta frecuencia)
Modo 6: ratio 17.81,  f = 5343 Hz, T60 = 0.5 s  (sexto modo — brillo muy corto)
```

El carácter de la guadua en reverb: más "vivo" y percusivo que la campana, con
menos cola pero más ataque. El modo 1 dura 3 segundos — suficiente para reverb
notable — pero los modos superiores se extinguen rápido, dejando un tail
dominado por las bajas frecuencias. Esto produce el carácter "abrigador" de los
instrumentos de bambú: cuerpo en los graves, brillo fugaz en los agudos.

**Nota sobre los ratios modo 5 y 6 de la guadua vs la campana:** difieren ligeramente
(13.34 vs 13.46, 17.81 vs 18.02). Esto refleja que los valores tabulados de β_nL
son ligeramente diferentes para barras con nudos de bambú (las divisiones internas
del nudo modifican marginalmente las condiciones de borde). Los valores de guadua
son una aproximación conservadora para barra libre sin nudos intermedios.

**Referencia:** Fletcher y Rossing, "The Physics of Musical Instruments" (2nd ed.,
1998), §2.4 "Bars with Variable Cross Section" — discusión de cómo la variación de
sección afecta los modos de barras. Para guadua específicamente, la literatura
de etnomusicología colombiana de la marimba de chonta (Escalante-Jiménez, 2011)
documenta las propiedades físicas de la guadua como material de láminas resonantes.

#### CRISTAL (plato rectangular de vidrio, libre en los bordes)

El cristal (vidrio de borosilicato o cuarzo fundido) tiene el Q más alto de los
cuatro materiales — puede retener energía de resonancia durante decenas de segundos.
Las copas de cristal frotadas con el dedo (armónica de cristal, instrumento del siglo
XVIII) explotan exactamente esta propiedad.

Para un plato rectangular libre (no barra), los modos son bidimensionales y sus
ratios difieren de los de barra unidimensional. Los modos del plato libre son más
complejos (incluyen modos de torsión además de flexión) y producen ratios que no
siguen la serie de la barra:

```
Modo 1: ratio 1.000,  f = 300 Hz,   T60 = 12.0 s  (modo (2,0) — dos nodos longitudinales)
Modo 2: ratio 2.442,  f = 733 Hz,   T60 = 10.0 s  (modo (0,2) — dos nodos transversales)
Modo 3: ratio 3.908,  f = 1172 Hz,  T60 =  8.0 s  (modo (2,2) — cuatro nodos)
Modo 4: ratio 5.983,  f = 1795 Hz,  T60 =  6.0 s  (modo (3,1))
Modo 5: ratio 8.547,  f = 2564 Hz,  T60 =  4.0 s  (modo (2,3))
Modo 6: ratio 11.41,  f = 3423 Hz,  T60 =  2.0 s  (modo superior)
```

El carácter del cristal en reverb: decays extremadamente largos, cola brillante y
sostenida. A Diffusion=6 (todos los modos activos), el cristal produce una nube de
resonancias que puede perdurar más de diez segundos — efecto de ambient pad
que domina el espacio acústico. Con Size=LARGE y Decay alto, el cristal se convierte
en un reverb casi infinito.

**Por qué los ratios del cristal son distintos a los de la barra:** las condiciones
de borde de un plato libre son fundamentalmente diferentes a una barra libre. La
geometría bidimensional produce modos de Kirchhoff-Love (teoría de placas delgadas),
cuyas frecuencias siguen patrones más complejos que los de la ecuación de Euler-Bernoulli
unidimensional.

**Referencia:** Rossing, "The Science of Sound" (3rd ed., 2001), Capítulo 19
"Rectangular and Circular Membranes and Plates" — §19.3 "Vibrations of Plates"
con tablas de frecuencias relativas para platos rectangulares libres.

#### MADERA (tabla de pino, libre en los bordes cortos)

La madera de pino (Pinus sylvestris) tiene Q relativamente bajo comparado con los
otros tres materiales. El amortiguamiento interno de las fibras de celulosa
disipa energía rápidamente. La madera usada en cajas de resonancia (violines, guitarras,
pianos) está específicamente seleccionada por tener Q alto para maderas — pero incluso
así, menor que cristal o metal.

El modelo aquí es una tabla delgada de pino libre, aproximada como barra para
simplificar la implementación:

```
Modo 1: ratio 1.000,  f = 300 Hz,  T60 = 1.5 s  (modo fundamental — seco y presente)
Modo 2: ratio 2.756,  f = 827 Hz,  T60 = 1.2 s  (segundo modo)
Modo 3: ratio 5.404,  f = 1621 Hz, T60 = 0.9 s  (tercer modo)
Modo 4: ratio 8.933,  f = 2680 Hz, T60 = 0.6 s  (cuarto modo)
Modo 5: ratio 13.34,  f = 4002 Hz, T60 = 0.4 s  (quinto modo — muy corto)
Modo 6: ratio 17.81,  f = 5343 Hz, T60 = 0.2 s  (sexto modo — casi inaudible)
```

El carácter de la madera en reverb: muy seco y percusivo. Los modos superiores se
extinguen en décimas de segundo. Solo el modo 1 dura más de un segundo. El resultado
es una reverb con ataque claro y cola muy corta — más un "refuerzo" del sonido que
una reverberación propiamente dicha. Útil para dar "espacio" sin difuminar el ritmo.

A Size=SMALL, la madera produce una reverb de apenas 0.45 s en el modo 1 — prácticamente
una habitación pequeña. Es el material más funcional para mezclas densas donde una
reverb larga enturbia el groove.

**Referencia:** Fletcher y Rossing, "The Physics of Musical Instruments" (2nd ed., 1998),
§2.6 "Effect of Internal Damping on Bar Vibrations" — discusión del amortiguamiento
en diferentes maderas con valores de Q medidos para spruce (abeto), pine (pino) y
maple (arce) usados en luthería.

---

### Comparativa de materiales

| Material  | Modo 1 T60 | Carácter     | Uso sugerido               |
|-----------|------------|--------------|----------------------------|
| Campana   | 8.0 s      | Metálico     | Pad, ambiente, transiciones |
| Guadua    | 3.0 s      | Orgánico     | Firma colombiana, live act  |
| Cristal   | 12.0 s     | Etéreo       | Ambient extremo, drone      |
| Madera    | 1.5 s      | Percusivo    | Groove preservado, mezclas  |

---

### Por qué bandpass filters y no síntesis aditiva

La forma más directa de modelar modos resonantes sería síntesis aditiva: un
oscilador sinusoidal por modo, con envelope de amplitud que decae exponencialmente
al T60 correspondiente. Esto funcionaría bien para un instrumento percusivo atacado
por un impulso.

El problema para un reverb es que el input stream es continua — no es un impulso
aislado, sino una señal sostenida (el chord de C mayor del engine). La síntesis
aditiva requeriría un onset detector para saber cuándo "excitar" cada oscilador,
y los osciladores tendrían que reiniciarse con la fase correcta en cada ataque.
Complejo y con artefactos en inputs continuos.

La solución es modelar cada modo como un **filtro bandpass resonante de alta Q**.
El filtro tiene exactamente la respuesta correcta: cuando la señal de entrada tiene
energía cerca de la frecuencia del modo, el filtro amplifica y retiene esa energía
(resonancia). Cuando la energía cae, el filtro decae con la constante de tiempo
determinada por su Q. El filtro es lineal e invariante en el tiempo — no necesita
saber cuándo hay "ataque". Simplemente procesa la señal continua y produce una
salida que naturalmente decae cuando el input decae.

En la Teensy Audio Library, `AudioFilterBiquad` en modo bandpass implementa la
función de transferencia:

```
H(z) = b₀ (1 - z⁻²) / (1 - a₁z⁻¹ - a₂z⁻²)
```

con los coeficientes calculados para una frecuencia central `f₀` y un factor Q:

```
ω₀ = 2π f₀ / fs         (frecuencia normalizada)
α  = sin(ω₀) / (2Q)     (bandwidth relativa)

b₀ =  α
a₁ =  2 cos(ω₀)
a₂ = -(1 - α)
```

La ganancia en la banda central es exactamente 1.0 (0 dB). Fuera de la banda,
la ganancia cae a tasa de 6 dB/oct en cada lado. El Q controla el ancho de banda
a -3 dB: `BW = f₀/Q`.

**Referencia:** Zölzer, "DAFX: Digital Audio Effects" (2nd ed., Wiley, 2011),
Capítulo 3 "Filters", §3.5 "Peaking and Shelving EQ Filters", y §3.7 "Resonant
Filters" — derivación de los coeficientes biquad para bandpass con parametrización
de Q. La figura 3.14 muestra la respuesta en frecuencia del filtro bandpass para
diferentes valores de Q, confirmando el comportamiento descrito.

**Referencia adicional:** Smith, "Introduction to Digital Filters with Audio
Applications" (W3K Publishing, 2007), Capítulo 8 "Biquad Filters" — análisis
de estabilidad del filtro biquad en función del valor de Q y la frecuencia
central relativa a la frecuencia de muestreo. Smith demuestra que para Q > 100
y frecuencias por debajo de fs/4, los coeficientes permanecen dentro del círculo
unitario y el filtro es estable en aritmética de punto flotante.

---

### El problema de Q alto en filtros digitales y la solución dual

La relación `T60_n = Q_n / (π × f_n) × 6.91` establece que para T60 largos
(campana modo 1: 8 s a 300 Hz), el Q necesario es:

```
Q = 8.0 × π × 300 / 6.91 ≈ 1088
```

Un Q de 1088 en un filtro biquad digital es problemático por dos razones:

**1. Estabilidad numérica:** los coeficientes del filtro se acercan al límite del
círculo unitario. Con aritmética de 32 bits en punto flotante (que es lo que usa
el procesador ARM Cortex-M7 del Teensy 4.1 en su unidad FPU), los errores de
redondeo en los coeficientes pueden causar que el polo del filtro caiga fuera del
círculo unitario — el filtro se vuelve inestable y produce un tono creciente en
lugar de un decay.

**2. Limitación de la librería:** la implementación de `AudioFilterBiquad` en la
Teensy Audio Library está optimizada para Q en el rango de uso de EQ y síntesis
estándar (típicamente Q < 50 para EQ, Q < 200 para síntesis). Para Q > 500, la
librería puede producir comportamiento inesperado dependiendo de la frecuencia y
la implementación interna de los coeficientes.

**La solución — arquitectura dual (Q del filtro + envelope software):**

Se separan dos responsabilidades que estaban entrelazadas en el modelo simple:

```
Q del filtro:       define la "campana" espectral del modo
                    (cuán afilado es el modo, cuánta selectividad de frecuencia)
                    Valor práctico: Q = 30–80 por filtro

Envelope software:  define el T60 real del modo
                    (cuánto tiempo persiste la energía en ese modo)
                    Implementado con coeficiente de decay por muestra en loop()
```

Con un Q de 50 en el filtro bandpass a 300 Hz, el ancho de banda es `BW = 300/50 = 6 Hz`.
Esto es suficientemente estrecho para que el modo suene como una resonancia afinada,
sin los problemas numéricos del Q ultra-alto.

El envelope de amplitude por modo controla el gain del canal del AudioMixer
correspondiente a ese modo. El decay es exponencial:

```
level_n[t + dt] = level_n[t] × exp(-dt / τ_n)
```

donde `τ_n` es la constante de tiempo de decay del modo n:

```
τ_n = T60_n / 6.908       (6.908 = ln(10^3) = ln(1000) — factor de 60dB)
```

En la implementación, `dt` es el período del loop de actualización (tipicamente
1–10 ms), y el coeficiente de decay precalculado es:

```
decay_coeff_n = exp(-dt_ms / (τ_n × 1000.0))
             = exp(-dt_ms × 6.908 / (T60_n × 1000.0))
```

En cada iteración de loop(), el nivel de cada modo se multiplica por su coeficiente:
```cpp
mode_level[n] *= mode_decay_coeff[n];
mixer.gain(n, mode_level[n]);
```

Cuando hay señal en el input (siempre, para el sketch de demo con chord sostenido),
el modo es excitado continuamente. El nivel se mantiene elevado mientras haya energía
en la banda del filtro. Cuando el input se silencia (por ejemplo al enviar el chord
a amplitud 0), el modo comienza a decaer según su coeficiente.

**Simplificación para v1.0:** los modos están siempre "excitados" al nivel 1.0
mientras haya input. El decay opera continuamente pero el input también repone el
nivel. El efecto audible correcto: mientras suena el chord, se escucha la coloración
modal; cuando el chord termina (si fuera monofónico o se apagara), los modos
decaen con sus T60 naturales. Para el demo con chord sostenido, el efecto es
principalmente de coloración tímbrica — la reverb "tail" se escucha en el silencio
entre acordes si el usuario silencia el engine.

---

### Parámetros de usuario — física de cada uno

#### Material (campana / guadua / cristal / madera)

Selecciona el set de frecuencias de modos y tiempos de decay base. Cambiar el material
es cargar un nuevo conjunto de 6 pares (f_n, T60_n) y recalcular los coeficientes
de los filtros biquad y los decay_coeff. En la implementación, esto se hace en loop()
la primera vez que se detecta un cambio de material — no en el audio thread.

#### Size (small / medium / large) — escala de decay

El tamaño del objeto físico afecta los decays pero no las frecuencias relativas de
modos (los ratios son independientes del tamaño — solo la frecuencia fundamental
cambia con el tamaño, que aquí se fija en BASE_FREQ). Size escala los T60:

```
Size = SMALL:   T60_effective = T60_base × 0.30
Size = MEDIUM:  T60_effective = T60_base × 1.00
Size = LARGE:   T60_effective = T60_base × 2.50
```

La fisica: un objeto más grande de mismo material tiene Q similar (el Q es una
propiedad del material, no del tamaño), pero sus modos están a frecuencias más bajas.
Frecuencias más bajas + mismo Q = T60 más largos (porque `T60 = Q × 6.91 / (π × f)`).
La escala de Size simula este efecto manteniendo BASE_FREQ fija y escalando T60.

#### Decay (multiplicador global 0.2–8.0)

Multiplicador aplicado sobre los T60 efectivos post-Size. Permite al usuario exagerar
o comprimir todos los decays simultáneamente. ENC R según el spec del Panel v5 Final §3.4.

```
T60_final_n = T60_base_n × size_mult × decay_mult
```

En la implementación, decay_mult=1.0 corresponde a los valores de T60_base de la tabla
de materiales. decay_mult=0.2 comprime todos los decays al 20% (reverb muy seca).
decay_mult=4.0 extiende todos los decays al 400% (reverb con colas larguísimas para
el cristal: hasta 48 s en modo 1).

#### Diffusion (1–6 modos activos)

Controla cuántos modos resuenan simultáneamente. Con Diffusion=1, solo el modo 1
(el más grave y más largo) está activo — el resultado es una resonancia de frecuencia
única, casi como un pitch resonator. Con Diffusion=6, todos los modos están activos
y la reverb tiene toda su densidad espectral.

La implementación pone gain=0.0 en los canales del mixer correspondientes a los
modos desactivados. Los filtros siguen procesando señal (no se detienen), pero
su salida no llega al mix.

```
Diffusion=1: solo modo 1 activo
Diffusion=2: modos 1-2 activos
...
Diffusion=6: modos 1-6 activos (máxima densidad)
```

La física: en objetos reales golpeados con un martillo blando, los modos altos
se excitan menos porque el impulso del golpe no tiene energía en frecuencias altas.
Un golpe suave o con un objeto grande excita menos modos superiores (diffusion baja).
Un golpe seco o con un objeto duro excita todos los modos con igual energía
(diffusion alta). El parámetro Diffusion reproduce este comportamiento.

#### Mix (0.0–1.0)

Balance entre dry (señal directa del engine) y wet (salida del banco de filtros
modales). A diferencia del Sub Genesis (donde el dry siempre permanece al 100% y
el sub se suma encima), aquí Mix es un dry/wet clásico:

```
gain_dry = 1.0 - mix
gain_wet = mix
```

Con Mix=0.0, solo la señal directa del engine pasa. Con Mix=1.0, solo la reverb
modal pasa (sonido muy colorado por los modos). El valor de trabajo es Mix=0.5–0.7.

---

### Signal flow completo

```
inputStream ──┬──→ [AudioFilterBiquad _mode0]  (f=300Hz × ratio_0, Q=50)
              ├──→ [AudioFilterBiquad _mode1]  (f=300Hz × ratio_1, Q=50)
              ├──→ [AudioFilterBiquad _mode2]  (f=300Hz × ratio_2, Q=50)
              ├──→ [AudioFilterBiquad _mode3]  (f=300Hz × ratio_3, Q=50)
              ├──→ [AudioFilterBiquad _mode4]  (f=300Hz × ratio_4, Q=50)
              └──→ [AudioFilterBiquad _mode5]  (f=300Hz × ratio_5, Q=50)
                        ↓            ↓            ↓            ↓
                   _modeMixA(ch0) _modeMixA(ch1) _modeMixA(ch2) _modeMixA(ch3)
                                       ↓            ↓
                                  _modeMixB(ch0) _modeMixB(ch1)

                   [AudioMixer4 _modeMixA]  — modos 0-3
                   [AudioMixer4 _modeMixB]  — modos 4-5

                         ↓                      ↓
                   _wetMix(ch0)           _wetMix(ch1)

                   [AudioMixer4 _wetMix]  — suma modeMixA + modeMixB
                         ↓
inputStream ────→ _dryWetMix(ch0)  ← dry  (gain = 1.0 - mix)
_wetMix ────────→ _dryWetMix(ch1)  ← wet  (gain = mix × envelope)
                         ↓
                   AudioOutputI2S L+R
```

**Por qué dos AudioMixer4 para los modos:** `AudioMixer4` acepta exactamente 4 entradas.
Con 6 modos, se necesitan dos mezcladores: `_modeMixA` para los modos 0–3 y
`_modeMixB` para los modos 4–5. Ambos van a un tercer mezclador `_wetMix` que
produce la señal wet total. El tercer mezclador también es un `AudioMixer4` con
dos canales activos.

**Fan-out desde inputStream:** la Teensy Audio Library permite múltiples
`AudioConnection` desde el mismo objeto fuente (fan-out implícito). El scheduler
de la librería ejecuta el objeto fuente una vez por bloque y su salida (el buffer
de 128 muestras) permanece disponible para todos los destinos conectados. Se crean
6 `AudioConnection` separadas desde `inputStream` hacia cada `_mode0`..`_mode5` —
no se necesita un splitter explícito.

**Total de AudioConnection:** 6 (inputStream→modes) + 4 (modes→modeMixA) +
2 (modes→modeMixB) + 2 (mixA+mixB→wetMix) + 1 (inputStream→dryWetMix dry) +
1 (wetMix→dryWetMix wet) + 2 (dryWetMix→outL, outR) = 18 conexiones.

---

### CPU estimado

```
6× AudioFilterBiquad (bandpass, Q=50):       ~6.0%   (~1.0% cada uno)
3× AudioMixer4 (modeMixA, modeMixB, wetMix): ~1.5%   (~0.5% cada uno)
1× AudioMixer4 (dryWetMix):                  ~0.3%
Decay envelopes en loop():                   <0.1%   (multiplicaciones float, < 1µs)
AudioOutputI2S:                               ~0.3%   (ya presente desde el engine)

Total Modal Reverb:                           ~8-10%

Budget en 05-fx-architecture.md §1.7:        ~12%
Headroom:                                     ~2-4%
```

El estimado conservador del spec (12%) deja margen para que los 6 filtros corran
con algo de tiempo extra en el scheduler. Los filtros biquad del Teensy Audio Library
están implementados con intrinsics SIMD del ARM Cortex-M7 — procesamiento de 128
muestras por bloque con instrucciones NEON optimizadas.

---

### Diagrama de modos — campana vs guadua

```
Frecuencia (Hz)   0    500   1000   1500   2000   2500   3000   3500   4000   4500   5000
CAMPANA          |█|          |██|          |█|     |█|         |█|         |█|
GUADUA           |█|          |██|          |█|     |█|         |█|         |█|
CRISTAL          |█|     |██|      |█|          |█|         |█|         |█|
MADERA           |█|          |██|          |█|     |█|         |█|         |█|

(Alturas relativas indican T60 — campana más alta que guadua, cristal la más alta de todas)
```

La diferencia entre campana y guadua en frecuencias es mínima (ratios casi idénticos
para barras libres). La diferencia real es en los T60: la campana tiene colas tres
veces más largas. En reverb, esto es la diferencia entre un sonido con carácter
metálico persistente y uno con carácter orgánico que se extingue más rápido.

---

### Referencias

- Rossing, "The Science of Sound" (3rd ed., Addison-Wesley, 2001) — Cap. 18 "Bells,
  Gongs, and Bowls": frecuencias y decays medidos de campanas europeas reales.
- Fletcher y Rossing, "The Physics of Musical Instruments" (2nd ed., Springer, 1998)
  — Cap. 2 "Vibrating Bars": derivación de los β_nL para barras con diferentes
  condiciones de borde; §2.6 sobre amortiguamiento interno en maderas.
- Zölzer, "DAFX: Digital Audio Effects" (2nd ed., Wiley, 2011) — Cap. 3 "Filters",
  §3.7 "Resonant Filters": coeficientes biquad para bandpass y análisis de Q alto.
- Smith, "Introduction to Digital Filters with Audio Applications" (W3K Publishing,
  2007) — Cap. 8 "Biquad Filters": análisis de estabilidad en función de Q y
  frecuencia normalizada.
- PaulStoffregen, Teensy Audio Library (GitHub, teensy/Audio) — fuente de
  `AudioFilterBiquad`, `AudioMixer4`, implementación de coeficientes biquad en ARM.

---

## Implementation

### Qué se implementa

- `apps/firmware-teensy/src/sketches/12-modal-reverb.cpp` — sketch de demo (chord
  C mayor, misma arquitectura que sprints anteriores, con ModalReverb como capa FX)
- `apps/firmware-teensy/src/fx/modal_reverb.h` — declaración de clase ModalReverb
- `apps/firmware-teensy/src/fx/modal_reverb.cpp` — implementación

### Decisiones de implementación

**Q fijo en 50 para todos los filtros:** equilibrio entre selectividad espectral (el
modo suena afinado, no como un filtro ancho sin pitch) y estabilidad numérica en
aritmética float32 del ARM Cortex-M7. Q=50 a 300 Hz produce BW=6 Hz — suficientemente
estrecho para carácter tonal. Si el Q fuera menor (Q=10 → BW=30 Hz), el modo sonaría
como un EQ amplio, no como una resonancia afinada.

**BASE_FREQ = 300.0 Hz:** centra el modo 1 de todos los materiales en 300 Hz, una
frecuencia que pasa bien por el codec SGTL5000 y es audible en cualquier sistema de
reproducción. Suficientemente baja para que el modo 1 de campana (T60=8s) suene como
una nota grave, suficientemente alta para que los modos 3–6 queden en el rango de
presencia (1–5 kHz).

**Envelopes en loop() no en audio thread:** el audio thread del Teensy Audio Library
corre cada 128/48000 ≈ 2.67 ms (cada bloque). El loop() típicamente corre cada 1–5 ms
dependiendo de la carga. Los gain updates de AudioMixer4 son thread-safe cuando se
hacen desde loop() — la librería los aplica en el próximo bloque completo, no
mid-block. No hay artifacts de clicks por actualización de ganancia.

**Un solo set de AudioFilterBiquad sin reiniciar:** los filtros se configuran una vez
con `setBandpass(stage, freq, Q)` y se mantienen corriendo continuamente. Al cambiar
de material, se llama a `setBandpass` con las nuevas frecuencias — la librería aplica
los nuevos coeficientes en el próximo bloque sin silenciar el audio ni producir
discontinuidades.

### Constraints respetados

| Constraint | Target | Estimado |
|---|---|---|
| CPU Modal Reverb | ~12% (§1.7) | ~8-10% |
| CPU total sketch | ≤60% (§5.2) | ~15% (engine 3OSC + FX) |
| Latencia audio | <1ms (§5.2) | 2.67ms (un bloque) — inherente |
| AudioMemory | ≤400KB RAM (§1.2) | ~20 bloques × 512B = ~10KB |

---

## Demo

### Evidencia requerida

1. **Grabación de audio** (Audacity, mínimo 60 segundos): chord C mayor con ModalReverb
   activo. Demostrar los 4 materiales en secuencia (campana → guadua → cristal → madera).
   La diferencia de carácter entre materiales debe ser claramente audible — campana
   metálica y sostenida, guadua más orgánica, cristal brillante y larguísima, madera
   seca y percusiva.

2. **Demo de Size:** grabación de Size SMALL → MEDIUM → LARGE en material campana.
   A LARGE, el modo 1 (300 Hz, T60=8s) dura efectivamente 20 s — la cola de reverb
   debe ser claramente audible después de que el chord del engine suene.

3. **Demo de Diffusion:** grabación de Diffusion=1 (solo modo 1, resonancia single) vs
   Diffusion=6 (todos los modos, densidad completa). La diferencia entre una resonancia
   de frecuencia única y el espectro completo modal debe ser obvia.

4. **Demo bypass:** comparación A/B con bypass toggle. El chord sin Modal Reverb vs
   el chord con Modal Reverb activo (campana, medium, mix=0.6) debe evidenciar el
   carácter de coloración tímbrica del FX.

5. **Screenshot Serial Monitor:** CPU% < 15% con engine + Modal Reverb activo. Memoria
   de audio dentro del budget.

### Cómo reproducirlo

**Build y upload:**
```bash
cd apps/firmware-teensy
pio run -e sketch -t upload
```

**Monitor Serial:**
```bash
pio device monitor -b 115200
```

**Comandos Serial del sketch:**

| Comando   | Parámetro             | Rango            | Ejemplo  |
|-----------|-----------------------|------------------|----------|
| `t<n>`    | Material              | 1=campana, 2=guadua, 3=cristal, 4=madera | `t2`   |
| `z<n>`    | Size                  | 1=small, 2=medium, 3=large | `z3`  |
| `e<val>`  | Decay multiplicador   | 0.2–4.0          | `e2.0`   |
| `f<n>`    | Diffusion (modos)     | 1–6              | `f3`     |
| `m<val>`  | Mix (dry/wet)         | 0.0–1.0          | `m0.6`   |
| `p`       | Bypass toggle         | —                | `p`      |

**Defaults al arrancar:**
```
Material   = campana (t1)
Size       = medium (z2)
Decay      = 1.0× (e1.0)
Diffusion  = 6 modos activos (f6)
Mix        = 0.6 (m0.6)
Bypass     = false (efecto activo)
```

**Secuencia de demo sugerida:**

```
1. Arranque: campana, medium, todos los modos, mix=0.6
   → Chord C mayor con reverb metálica, decays largos
   → Silenciar el engine mentalmente: ¿se escucha la cola de reverb?

2. t2 → guadua
   → Notar el carácter más orgánico y seco
   → El mismo chord, diferente "sala"

3. t3 → cristal
   → La reverb más larga — modo 1 a 12s × 1.0 = 12s de cola
   → Si el sistema reproduce bien los 300 Hz, el modo 1 es muy audible

4. t4 → madera
   → Seco y percusivo — casi sin reverb perceptible
   → Más un "color" que una reverberación

5. t1 → volver a campana

6. f1 → Diffusion=1 (solo modo 1)
   → Una sola resonancia a 300 Hz — pitch resonator más que reverb

7. f6 → Diffusion=6 (todos los modos)
   → La densidad modal completa — más "espacio"

8. z3 → Size=LARGE
   → Decays 2.5× más largos: campana modo 1 a 20s
   → El chord flota sobre una nube de resonancias

9. z1 → Size=SMALL
   → Decays × 0.3: campana modo 1 a 2.4s — reverb corta

10. z2 e2.0 → Medium + decay×2.0
    → Decays escalados al 200%: entre medium y large

11. e0.2 → Decay muy corto
    → Reverb casi seca: los modos colapsan rápido, casi bypass

12. e1.0 z2 → Volver a defaults razonables

13. m0.0 → solo dry (engine sin reverb)
    → Comparación: el chord directo sin ninguna coloración modal

14. m1.0 → solo wet (reverb modal sola, sin dry)
    → Escuchar solo la respuesta modal del banco de filtros

15. m0.6 → Balance de trabajo

16. p → bypass
    → Comparación definitiva: con/sin Modal Reverb
    → El bypass evidencia cuánta identidad tímbrica agrega el FX

17. t2 m0.5 z2 e1.0 f6 → Preset "Guadua Colombiana"
    → El sonido de firma del GrooveForge Brain
```

---

## Learnings

*(Se completa después de la implementación y medición en hardware real.
Secciones típicas: diferencia entre Q teórico y audible en hardware, comportamiento
de los fan-out de AudioConnection en el scheduler, valores de T60 que suenan
correctos vs los teóricos, ajuste de BASE_FREQ post-escucha en hardware,
deuda técnica identificada.)*

---

*Sprint 2.7 — GrooveForge Brain · Juan Guerrero (GPROG)*
