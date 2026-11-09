# Motor: loops y top loops

> **Fecha:** 2026-11-06. **Estado:** primera entrega, en prueba. **Decisiones de Juan:** MIDI editable, con Tribe, y sin afinar los loops porque son grabaciones. **Base:** `percusion-groove.md`, `library-grooveforge.md`. **No modifica** `00–06`.

## 1. Alcance

Poner en Live, como clips editables, patrones de percusión que ya existen, sobre el groove actual. **No genera patrones nuevos:** el orden aprobado (`ia-local-refinamiento.md` §11) empieza por las bases y favoritos de Juan, para saber después qué falta. Tampoco exporta audio todavía.

## 2. Dos papeles, dos pistas

| Papel | Qué hace | Pista |
|---|---|---|
| **PERC** | Conversa con kick, bajo y clave | `PERC` |
| **TOP** | Movimiento continuo | `TOP` |

Van aparte de `DRUMS` porque usan otro teclado: estos MIDI ya están convertidos a Tribe, y en `DRUMS` tocarían otros pads.

**Tribe se carga a mano una vez en cada pista**: `GFMotor` todavía no carga plugins. El motor no toca el instrumento ni la afinación de Tribe. Conviene revisar antes la licencia si alguna vez se extrae o se entrena con su audio; tocarlo por MIDI no exige eso.

## 3. Opciones

Salen de `GrooveForge_Library/_catalogo/catalogo.jsonl`: género, papel y `rack: tribe`.

**Orden:** primero lo que Juan eligió (`seleccion`, luego `favorito`) y después el resto. Dentro de eso, `peak`, `main`, `main_q` e `intro`.

**Diversidad:** hasta 6 opciones y un máximo de 2 por patrón, para no presentar variantes casi iguales del mismo. Lo mismo copiado en dos carpetas cuenta una vez.

**Encaje con el groove:** cada opción muestra qué parte de sus golpes principales cae con kick, con bajo y con clave, leídos de Live en ese momento. **Es información, no una nota:** coincidir no es malo, y el motor no descarta por eso. El microtiming se redondea a la semicorchea solo para esta cuenta.

## 4. En Live

El patrón se repite hasta 16 compases, **conservando su microtiming y velocities**, porque de ahí viene el toque humano (`percusion-groove.md` §7). El tiempo se toma en tiempos musicales, así que un archivo a 124 BPM encaja a 122.

Rige la regla de no pisar lo editado: un clip intacto se reemplaza; si cambió, o la pista tiene otro clip, la propuesta va a `PERC propuesta` o `TOP propuesta`, sin instrumento.

## 5. Géneros

`organic` usa `organic_house`; `house`, `disco_house` (no hay género `house` en los MIDI); `nu_disco`, `disco_house`, y `indie_tech`, el suyo. Son equivalencias provisionales, a confirmar cuando se trabaje esos géneros.

## 6. Cómo se valida

**Tests:** orden, diversidad, repetición, microtiming, regla de no pisar y encaje. **Lo que decide es Juan:** si estos patrones sirven sobre su groove o hay que crear otros.
