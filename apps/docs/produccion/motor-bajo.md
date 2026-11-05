# Motor: paso de bajo

> **Fecha:** 2026-11-05. **Estado:** aprobado por Juan el 2026-11-05; primera entrega en prueba. **Amplía:** `motor-asistencia-v2.md` §4, paso 5. **Base:** `bajo-groove.md`, `sesion-paleta-adn.md`, `apps/docs/variants/studio/theory/11-studio-preset-librarian.md`. **No modifica** `00–06`.

## 1. Objetivo

Elegir el bajo como el kick: varias opciones, Juan elige y queda sonando en Live. Hay dos decisiones separadas, **patrón** y **sonido**, para poder cambiar una sin perder la otra.

**Validación:** Juan confirma que llegó a un bajo que quiere conservar en menos tiempo que a mano, oyéndolo junto a su kick. El sistema no decide si suena bien.

## 2. Lo que ya existe

| Material | Cantidad | Estado |
|---|---|---|
| Patrones | `bass_adn_tambora`, `octavas`, `rodante`, `cumbia`; `bass_indie_rodante332`, `tresillo_aire`, `contratiempo` | MIDI escrito por reglas |
| Presets con «bass» o «sub» en nombre o ficha | Unos 24 | Solo `JD SUB MID` tiene rol `bass`; los demás no están etiquetados |
| Favorito | `JD SUB MID` con `bass_adn_octavas` | Pasó en los contextos A, B y C |
| Escribir en el Brain | `gf_presets.py import` y `layer-import` por USB | Existe; hay 25 de 32 capas ocupadas |
| Escribir en el VST | Importar un GFP con el botón | Manual; `GFMotor` aún no carga plugins |

## 3. Patrones

**Decisión de Juan:** el patrón no sigue una progresión. Lo que busca es una **sensación entre kick y bajo**, sobre la fundamental, y el rolling bass es una de ellas.

La sensación sale de **dónde cae el bajo respecto al kick**, sin cambiar de acorde (Butler, *Unlocking the Groove*, 2006, cap. 3; Toussaint, *The Geometry of Musical Rhythm*, 2013, para el tresillo):

| Familia | Dónde cae (en semicorcheas) | Sensación esperada |
|---|---|---|
| **ROLLING** | Todo lo que no es kick | Empuje continuo |
| **ROLLING AIRE** | La segunda mitad de cada tiempo | Rueda, pero respira |
| **CONTRATIEMPO** | Solo en la «y» | Bombeo de house |
| **TRESILLO** | 3-3-2 sin el golpe del kick | Balanceo tribal |
| **TAMBORA** | Síncopa con ghosts y octava | Conversación con el kick |
| **OCTAVAS** | «y» alternando octava | Rebote de disco |

**Son reglas, sin ML** (`ia-local-refinamiento.md` §11, paso 2). Cada familia es un compás; cada tanda cambia ghosts, acentos y el remate de la frase, cada 4 compases. Otra tanda da otras variantes de las mismas familias.

**Reglas que se comprueban con tests:**

1. **Ninguna nota empieza en un golpe del kick** a negras; si no, grave y kick se suman o se cancelan. Si editas tu kick con otros golpes, el patrón no lo sabe todavía.
2. **Fundamental en el registro de Mi1 a Do2** (41–65 Hz) o, si la nota no cabe, la más cercana. Solo se sale de ella con octava, y con la quinta o el semitono de abajo en el remate.
3. Las notas no se pisan entre sí, porque en un mono se cortaría el ataque.

**En Live:** un clip de 16 compases en la pista `BASS`, con la misma regla de no pisar lo editado. La salida MIDI hacia el Brain se configura una vez a mano, y conviene dejarla en el template.

## 4. Presets

**Selección:** los sonidos de bajo que ya están como capa: `JD SUB MID` primero, `ROLLER SUB` y `WOODBASS`. El motor lee el orden de capas del Brain por USB, sin escribir nada, y envía CC 18 al puerto MIDI `GrooveForge`. La fórmula es la inversa de `layer_from_cc` (`ui/layers.h`).

**Refinar más presets es el paso siguiente (Juan), no esta entrega:** variaciones acotadas de un preset elegido (`ia-local-gemelo-digital.md` §5, F), cada una explicable como un cambio pequeño en filtro, envolvente o mezcla de osciladores, nunca en algoritmos o parámetros discretos. Es la parte con más riesgo: una variación mal acotada suena rota, como pasó con la madera del kick. Todo preset generado se oye en el Brain antes de darlo por bueno (`ia-local-gemelo-digital.md` §10, decisión 3).

### 4.1 Primera tanda de variaciones: `TRESBASS`

**Escucha de Juan (2026-11-05):** en el Brain suena algo más brillante que en el VST, y prefiere el VST. Quiere más madera y un poco menos de sub.

`TRESBASS` es una capa: A es un DX7 (el golpe de madera) y B un Model D una octava abajo, con filtro cerrado a 122 Hz (el sub).

| Quiero | Parámetro | Por qué |
|---|---|---|
| **Menos sub** | MORPH hacia A y nivel del oscilador sub de B | MORPH reparte con ley de igual potencia (`layer_gains`); más bajo es más A |
| **Más madera** | Nivel y largo del modulador del DX7 | El modulador crea los parciales del golpe. Es FM (Chowning, 1973): más índice, más parciales, y si el modulador decae rápido quedan solo en el ataque, como en un golpe |
| **Menos brillo** | Cutoff de A | Es el control más directo de lo que sobra. No explica por qué el Brain difiere del VST: eso sigue sin medir |

| Capa | Cambios sobre `TRESBASS` | Pregunta que responde |
|---|---|---|
| `TRESB OSC` | Cutoff de A: 10,2 → 6,5 kHz | ¿Con menos brillo vuelve a parecerse al VST? |
| `TRESB MAD1` | MORPH 53 → 42, sub de B 75 → 50 | ¿Basta con mover el balance? |
| `TRESB MAD2` | Lo de MAD1, modulador 75 → 85, cutoff 7 kHz | ¿Más golpe de FM sin sumar brillo? |
| `TRESB MAD3` | Lo de MAD1, modulador en razón 3, nivel 60, cutoff 7 kHz | ¿Un timbre más hueco? Es la más arriesgada |

Solo cambian valores continuos o el nivel, salvo la razón de MAD3, que es la excepción explícita. `TRESBASS` no se toca. Quedan 2 ranuras libres de 32; descartar las que no sirvan es decisión de Juan.

## 5. Decisiones de Juan (2026-11-05)

Suena en el **Brain físico**. Se empieza con **presets existentes**, `JD SUB MID` primero. **Una sola pista** de bajo. **Sin progresión de acordes.**

## 6. Primera entrega

**Generar patrones → poner uno en `BASS` → oírlo con tu kick → cambiar de capa desde la web → registrar lo que elijas y edites.**

**Cambiar de sonido en el Brain:** el firmware ya elige capas de HYBRID con CC 18 (`ui/layers.h`), sin flashear; así lo hacen los clips `capa_<n>`. **No encontré un mensaje para elegir presets sueltos de un engine** en la búsqueda del firmware. Por eso la primera entrega presenta **capas**, incluidas `JD SUB MID`, `ROLLER SUB` y `WOODBASS`, y deja libres 7 de sus 32 ranuras para las que se generen. Los presets sueltos exigirían cambiar firmware o elegirlos a mano. El VST queda para después, junto con el comando de cargar plugins en `GFMotor`.

**No cubre:** acordes, niveles por rol, ni cargar el VST solo.
