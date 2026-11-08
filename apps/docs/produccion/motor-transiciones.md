# Motor: transiciones (roll, riser, fill y ear candy)

> **Fecha:** 2026-11-08. **Estado:** aprobado por Juan el 2026-11-08 («procede con esto y el tema transiciones, fills, roll, risers»); primera entrega en prueba. **Amplía:** `motor-asistencia-v2.md` §4, pasos 2 y 7. **Base:** decisiones del estudio indie (eventos del 2026-10-29), `motor-pads.md`. **No modifica** `00–06`.

## 1. Objetivo

Proponer lo que une las secciones, **en el lugar exacto del arreglo**: el motor lee los bloques de `ARRANGE` y escribe los clips antes de cada llegada. Reglas, sin ML; MIDI editable; nunca se pisa lo editado.

A diferencia de los demás pasos, aquí **no** se escribe un clip de 16 compases en el compás 1: cada elemento va **en todos los puntos** del arreglo donde aplica, en su propia pista.

## 2. Lectura del arreglo

Se leen los clips con nombre de `ARRANGE` (INTRO, BUILD, DROP, BREAK, OUTRO). Una sección termina donde empieza la siguiente con nombre; los clips sin nombre se ignoran. Sin `ARRANGE`, el paso pide crear la guía primero.

| Punto | Qué es |
|---|---|
| **Llegada al drop** | Inicio de cada DROP; la sección anterior es la que sube |
| **Cambio de sección** | Inicio de cualquier sección menos la primera |
| **Break** | Inicio de cada BREAK |

## 3. Elementos

**Roll** (pista `ROLL`, nota del snare del rack de drums, 48). Decisión de Juan en el estudio indie: empieza en negras y pasa a corcheas, semicorcheas y fusas; lo recortó a 4 compases.

| Patrón | Largo antes del drop | Velocidad |
|---|---|---|
| **ROLL 4** | 4 compases, uno por subdivisión | Sube de 50 a 120 |
| **ROLL 8** | 8 compases, dos por subdivisión | Ídem |
| **ROLL 2** | 2 compases, medio por subdivisión | Ídem |

**Riser** (pista `RISER`), durante la sección que sube, hasta 16 compases:

| Patrón | Qué toca | Sonido |
|---|---|---|
| **RISER NOTA** | La tónica sostenida: la subida la pone el preset | GF NOISE RISER, JD AIRE |
| **RISER ESCALERA** | Las notas del acorde de la tónica entran una a una y se sostienen hasta el drop | Un pad o un pluck largo |
| **RISER OCTAVAS** | La tónica salta de octava cada cuarto del tramo | Cualquier sonido sostenido |

**Fill** (pista `FILL`, notas del rack de drums), en el último compás antes de cada cambio de sección:

| Patrón | Qué toca |
|---|---|
| **FILL TRESILLO** | Clap en 3-3-2 que sube de fuerza |
| **FILL ÚLTIMO TIEMPO** | Snare en semicorcheas en el último tiempo y clap en la última |

**Ear candy** (pista `EAR CANDY`), golpes sueltos que se recuerdan:

| Patrón | Dónde | Qué toca |
|---|---|---|
| **IMPACTO** | Primer tiempo de cada drop, 2 compases | La tónica grave (sub drop) |
| **STAB ANTES DEL DROP** | La «y» del tercer tiempo del último compás antes del drop, y después silencio | Tríada de la tónica |
| **GOTA EN EL BREAK** | Inicio de cada break y su mitad | Tónica y quinta agudas, largas, para un eco |

Todo lo que tiene nota va a la tónica o al acorde de la tónica, por la regla de Juan.

### 3.1 Segunda ronda (Juan, 2026-11-08): rolls con carácter, Tribe y FX

**Más rolls** (los de arriba son PIRÁMIDE), todos de 4 compases:

| Patrón | Qué hace |
|---|---|
| **ROLL TRESILLO** | 3-3-2 en semicorcheas, luego tresillos de corchea, sextillos y fusas: el tresillo de la base, acelerando |
| **ROLL CON SILENCIO** | La pirámide, pero el último tiempo antes del drop queda vacío: el hueco hace que el drop pegue más |
| **ROLL FLAM** | Cada golpe de negras, corcheas y semicorcheas con una nota de adorno 1/64 antes y más suave |

**Sonido del roll con la drum machine:** snare y clap de `gf_perc` (madera, piel u 808, afinados a la fundamental) en un rack que se carga en `ROLL`. El nombre empieza por `DRUMS roll-…` para que `GFMotor` quite el rack anterior de esa pista sin tocar su Remote Script.

**Corrección de Juan (2026-11-08): los fills van de paso entre frases**, dentro de las secciones; los builds y breaks son de risers y rolls. Por eso:

- **Fills** (pista `FILL`, **con Tribe**): al final de **cada frase de 8 compases**, en todas las secciones menos los builds y menos la llegada a un drop (esa es del roll). Reemplazan a los fills de clap y snare de la drum machine, que quedaron fuera.
- **Roll y riser con Tribe** (pista `ROLL TRIBE`): antes de cada drop, junto al roll de la drum machine.

En la cumbia y el bullerengue, el llamador sostiene y el alegre «llama» los cambios; la tambora remata (notas de `kits/tribe_colombia.json`):

| Patrón | Pista · dónde | Qué toca |
|---|---|---|
| **ALEGRE QUE LLAMA** | `FILL` · último compás de cada frase | Frase de abierto, quemao y latigazo que crece |
| **TAMBORA** | `FILL` · 2 últimos compases de cada frase | Apagado en corcheas, luego en semicorcheas, y remate en abiertos |
| **CAJA** | `FILL` · último compás de cada frase | Caja vallenata: fondo en semicorcheas, quemao y latigazo |
| **LLAMADOR** | `ROLL TRIBE` · 4 compases antes de cada drop | Abierto y bajoneo alternados: negras, corcheas, tresillos, semicorcheas |
| **GUACHE RAIN** | `ROLL TRIBE` · 4 compases antes de cada drop | Dos lluvias y luego trémolo en semicorcheas que crece: un riser orgánico |

**Teclas de Tribe (resuelto, 2026-11-08):** Juan probó G#1 y A#1 en Tribe: suenan, son la cáscara de la tambora y son los **únicos sostenidos** del kit. El mapa `kits/tribe_colombia.json` está bien. La TAMBORA del fill vuelve a la cáscara, y un test exige que todo lo de Tribe caiga en teclas del kit, sin otros sostenidos.

**FX** (pista `FX`). El gesto lo pone el preset, así que no hace falta automatización:

| Patrón | Dónde | Preset |
|---|---|---|
| **CAÍDA** | Al entrar cada drop, 2 compases | **JD CAIDA**: ruido que se cierra en ~3 s |
| **SWELL INVERSO** | El compás antes de cada drop, cortando en el drop | **JD SWELL**: ruido que crece ~2 s y corta, como un platillo al revés |
| **STUTTER** | Último compás antes del drop | La tónica en corcheas, semicorcheas y fusas, cada vez más cortas, con el sonido del lead o un stab |
| **NATURALEZA** | Intro y breaks, hasta 8 compases | JD RIO, JD AIRE, JD CASCADA |

Presets de FX y ear candy para el VST en `GrooveForge_Library/presets/fx/`: JD CAIDA, JD SWELL, GF NOISE RISER, JD AIRE, JD RIO, JD GOTAS, GF LOG DRUM (impacto), GF KALIMBA TINE, GF CHONTA, BELLS, RING BELL (gota). Los stabs están en `presets/stabs-plucks/`.

**Más adelante** (requieren que `GFMotor` escriba automatización): barrido de filtro en el build, echo throw de la última nota del lead y el silencio de un tiempo en todo el set.

### 3.2 INTER: el interludio del build (Juan, 2026-11-08)

Juan agregó a `ARRANGE` una sección **INTER**: el interludio del segundo build, donde va el **subdrop** y caben más detalles. Cuando un DROP viene después de un INTER:

- La **subida** (riser, roll y roll de Tribe) llega al **inicio del INTER**: ahí se corta la tensión.
- **SUBDROP INTER**: al empezar cada INTER (§3.3). **SUBDROP EN DROP** sigue en el primer tiempo de cada drop.
- El INTER puede llevar **GOTA** y **NATURALEZA**. **SWELL INVERSO**, **STUTTER** y **STAB ANTES DEL DROP** siguen yendo al drop, al final del INTER.
- No lleva fills: no es una frase de 8 compases.

### 3.3 Subdrop propio (Juan, 2026-11-08)

Juan dejó el INTER en **un compás**: un pequeño silencio para el subdrop. Ningún preset del Brain servía como subdrop: el Brain no tiene envolvente de tono, y GF LOG DRUM es un golpe. El subdrop sale de la drum machine (`gf_perc/subdrop.py`): un seno que **cae de tono** hasta la tónica grave (La1, 55 Hz), con algo de saturación para que se oiga en parlantes pequeños.

| Sonido | Caída | Dura | Para |
|---|---|---|---|
| **CORTA** (por defecto) | Una octava, rápida | 1,8 s | Un INTER de un compás (≈ 2 s a 122) |
| **LARGA** | Una octava, lenta | 3,5 s | Un INTER largo o un break |
| **808** | Un cuarto de octava | 2,5 s | Casi recto: un 808 largo |

Va a su propia pista, `SUBDROP`, con un rack del motor (pad C1, nombre `DRUMS subdrop-…` para que se reemplace al elegir otro). **SUBDROP INTER** cae al empezar cada INTER; **SUBDROP EN DROP** (antes IMPACTO, en Ear candy) en el primer tiempo de cada drop. Si no se eligió sonido, al poner el primero se crea el CORTA.

## 4. Regla de no pisar

Cada elemento recuerda los clips que escribió. Si Juan editó o agregó algo en esa pista, la opción siguiente va a `<PISTA> propuesta`.

## 5. Sonido y mezcla

Roll y fill: una copia del rack de `DRUMS` o el snare de Tribe en la pista. Riser: `presets/pads/` (GF NOISE RISER, JD AIRE). Ear candy: stabs y plucks de `presets/stabs-plucks/`, con el envío **F · Echo**.

## 6. Cómo se valida

**Tests:** lectura de secciones, posición exacta antes de cada llegada, subdivisiones y crescendo del roll, notas en la tónica, regla de no pisar con varios clips. **Lo que decide es Juan.**

## 7. No cubre

Automatización (filtro, volumen, envíos) y efectos sobre audio, como el reverse: `GFMotor` aún no los escribe.
