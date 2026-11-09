# Motor: pads y atmósfera

> **Fecha:** 2026-11-08. **Estado:** primera entrega en prueba (Juan pidió la pestaña para escuchar; los patrones aún no los ha oído). **Amplía:** `motor-asistencia-v2.md`, último paso del flujo. **Base:** `motor-acordes.md`, `sesion-paleta-adn.md` (§ pads y texturas), `template-envios-valhalla.md`. **No modifica** `00–06`.

## 1. Objetivo

Sumar fondo al set de Juan sin competir con lo que ya está. Son dos partes con trabajos distintos:

| Parte | Trabajo | Dónde vive |
|---|---|---|
| **Pad** | Sostiene la armonía con notas largas que casi no se mueven | Pista `PAD` |
| **Atmósfera** | Textura sin ritmo: espacio, aire, movimiento lento | Pista `ATMOS` |

En `sesion-paleta-adn.md` ya se decidió que pads y texturas son **material de intro y break, no de pista entera**. El motor propone; Juan decide dónde va cada clip en el arreglo.

Igual que en los demás pasos: reglas, sin ML; MIDI editable; clip de 16 compases; nunca se pisa lo editado; registro de lo elegido.

**Validación:** Juan llega a un pad y una atmósfera que quiere conservar en menos tiempo que a mano. El sistema no decide si suena bien.

## 2. Lo que se lee de Live

- **Progresión:** la que dejó en `CHORDS` (hoy A – B, `I-II`). El pad la sigue; si no hay acordes, usa la tónica.
- **Registro ocupado:** `CHORDS` y `CHORDS 2` en MIDI 48–64, la melodía arriba, el bajo abajo. El pad evita duplicar las mismas notas en el mismo registro: si no, se suma volumen en lugar de color.

## 3. Pad: tres maneras de sostener la armonía

| Patrón | Qué toca | Por qué |
|---|---|---|
| **DRON** | La + Mi (fundamental y quinta) sostenidos 16 compases, en La2–Mi3 | Ya existe como `pad_adn_dron`. No sigue los acordes: es el piso que no cambia, como la tambora sobre la que se mueve todo |
| **LÍNEA GUÍA** | Solo la 3.ª y la 7.ª de cada acorde (A7: Do# y Sol; B7: Re# y La), una nota larga por compás, por encima de los acordes | Las notas guía definen el acorde con dos voces (Levine, *The Jazz Theory Book*, cap. 2). Aquí el Do# → Re# sube un semitono: ese paso **es** el color lidio, y la línea lo canta despacio |
| **ABIERTO ALTO** | Fundamental, quinta y décima de cada acorde, arriba de los acordes (fundamental desde Mi4, MIDI 64–88), cambio cada compás con ataque lento | Voicing abierto: ocupa espacio sin espesar el centro, y deja el registro medio a `CHORDS 2` |

La duración llena el compás (o los 16 compases en el dron), con un pequeño espacio para que el ataque lento del preset vuelva a entrar.

## 4. Atmósfera: tres texturas sin ritmo

| Patrón | Qué toca | Para |
|---|---|---|
| **NUBE** | Notas sueltas y largas de la escala, en registro alto, que se solapan, sin caer en el pulso; La, Mi y el Re# como color. Con semilla: la misma tanda da lo mismo | Intro y break: aire que se mueve |
| **SWELL** | Las notas del acorde entran una a una cada compás durante 4 compases y se sostienen | Subir la tensión antes del drop (build) |
| **RAÍZ GRAVE** | Un La grave sostenido 8 compases, dos veces | Base para un preset con movimiento propio (LFO, filtro) |

El movimiento de la atmósfera lo pone el **preset**, no el MIDI: por eso las notas son pocas y largas. Automatizar MOVE (CC 19) del Brain queda fuera de esta entrega, porque `GFMotor` todavía no escribe CC en clips.

## 5. Sonido

Para el VST, como en Acordes (`gf_presets.py hacia-vst` → `GrooveForge_Library/presets/pads/`). Candidatos que ya existen en la librería:

| Para | Presets | Lo que se sabe |
|---|---|---|
| Pad | GF NOCTURNE PAD (OB-6), GF BAMBOO PAD (Juno), PWM PAD (Juno), JD SUS POLY | En la sesión de paleta, el OB-6 sonó algo oscuro y el Juno muy brillante: se busca un punto medio |
| Atmósfera | GF DARK DRONE (OB-6), GF STRING SWELL (Juno), GF NOISE RISER (ARP 2600) | El riser ya va en el build (estudio indie) |

Si ninguno queda en el punto medio, la siguiente ronda es una capa OB-6 + Juno, con el mismo esquema de las capas de acordes. Es una decisión de Juan.

### 5.1 Atmósfera orgánica: agua y aire (propuesta, 2026-11-08)

Juan quiere un ambient más orgánico: cascadas, ríos, aire. Ya están en `presets/pads/` los siete presets anteriores para oírlos en el VST.

**De dónde sale el sonido del agua.** El agua que corre suena por miles de burbujas que resuenan al formarse; cada una da un tono corto que sube (Minnaert, 1933). Pocas burbujas son gotas con altura; muchas se funden en ruido con un pico de energía que se mueve al azar. El viento es ruido filtrado cuyo filtro se mueve despacio (Farnell, *Designing Sound*, 2010, caps. de agua, lluvia y viento). En el Brain eso se hace con lo que ya tiene: **ruido, filtro con resonancia y un LFO** que puede ser lento (seno) o al azar (S&H, *sample and hold*).

| Preset nuevo | Motor | Receta | Suena a |
|---|---|---|---|
| **JD AIRE** | ARP 2600, solo ruido | Filtro 900 Hz con algo de resonancia; LFO seno muy lento (0,1 Hz) que abre y cierra el filtro | Viento que sube y baja |
| **JD RIO** | Capa ARP 2600 + OB-6, ruido en las dos | A: ruido a 1,5 kHz con S&H rápido y poco profundo (el gorgoteo). B: ruido más grave que crece y baja muy lento | Corriente con burbujas |
| **JD CASCADA** | Capa ARP 2600 + OB-6, ruido en las dos (el Juno no puede apagar su oscilador) | A: ruido ancho a 3 kHz, constante. B: ruido grave a 300 Hz (el golpe del agua al caer) | Caída de agua, densa |
| **JD GOTAS** | ARP 2600, ruido con mucha resonancia | S&H mueve el filtro a saltos; la resonancia convierte cada salto en una nota corta | Gotas, agua que suena |

**Sin afinar:** el ruido no tiene nota y en JD GOTAS el S&H elige la altura al azar, así que la regla de la fundamental no aplica aquí, como en los loops de Tribe. Generados en `tools/paleta-lab/make_jd_agua.py`. Son sonidos de síntesis: se acercan al agua, no la imitan.

**Alternativa:** grabaciones de campo reales en una pista de audio, sin afinar, como Tribe. Es la vía más fiel. La síntesis tiene a su favor que se edita como MIDI y se mueve con el set.

**Mezcla:** envío **C · Hall** a −16 dB (`template-envios-valhalla.md`).

## 6. En la web

Una pestaña nueva, **Pads**, con dos bloques: Pad y Atmósfera. Cada bloque tiene **Leer acordes**, sus opciones y **Poner**. La regla de no pisar lo editado aplica igual: si Juan editó el clip, la opción siguiente va a `PAD propuesta` o `ATMOS propuesta`.

## 7. Cómo se valida

**Tests:** notas en la escala, registro, que el pad no duplique las notas de `CHORDS` en el mismo registro, que la línea guía use solo 3.ª y 7.ª, que la nube no caiga en el pulso, que se mantenga la semilla y la regla de no pisar. **Lo que decide es Juan.**

## 8. No cubre

- Colocar los clips en las secciones del arreglo: van al compás 1, y Juan los mueve.
- Automatización de CC (MOVE, filtro).
- Carácter de las progresiones (alegre, melancólica, eufórica): pedido de Juan para más adelante, anotado en `motor-acordes.md`.
