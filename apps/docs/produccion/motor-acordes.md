# Motor: acordes

> **Fecha:** 2026-11-06. **Estado:** primera entrega, en prueba. **Base:** `lab-acordes.md`, `motor-drums.md` §7. **No modifica** `00–06`.

## 1. Objetivo

Explorar acordes sobre el groove y la melodía de Juan, con varias progresiones, voicings y ritmos. Reglas, sin ML. Es MIDI editable, y suena con el instrumento que Juan cargue en la pista.

**No cubre:** pads ni atmósfera, que van después, sobre los acordes elegidos.

## 2. Armonía desde la escala

Cada acorde se apila **por terceras dentro de la escala** (Kostka y Payne, *Tonal Harmony*): así todas sus notas pertenecen al modo, sin una tabla por escala, y siguen sirviendo si Juan cambia de modo. En La lidia dominante (La Si Do# Re# Mi Fa# Sol):

| Grado | Tríada | Con séptima |
|---|---|---|
| I | A | A7 |
| II | B | B7 |
| V | Em | Em(maj7) |
| VI | F#m | F#m7 |

| Progresión (grados) | En La lidia dominante | Por qué |
|---|---|---|
| **I – II – VI – II** | A – B – F#m – B | Ya gustó en `lab-acordes.md`, y su B trae el Re# |
| **I – II** | A – B (vamp) | Hipnótica: deja el protagonismo al groove |
| **I – II – V – I** | A – B – Em – A | El Em trae el Sol, la ♭7, y da algo de sombra |

Son progresiones de una vuelta por compás. El bajo no las sigue: sigue la decisión de Juan de quedarse en la fundamental, y el II sobre La es lo que suena lidio.

## 3. Voicings y ritmos

**Conducción de voces (`lab-acordes.md` §2):** cada acorde toma la inversión más cercana al anterior, en un registro de Do3 a Mi4 (MIDI 48–64), porque la melodía vive por encima y el bajo por debajo. No hay nota grave por debajo de Do3: choca con el bajo.

| Patrón | Notas | Ritmo | De dónde sale |
|---|---|---|---|
| **BLOQUE TRESILLO** | Tríada | 3-3-2 dos veces | Pasó en el lab |
| **DÉCIMAS TRESILLO** | Fundamental + décima | 3-3-2 dos veces | Pasó en el lab; pedido de Juan |
| **1-5-8 TRESILLO** | Fundamental, quinta, octava | 3-3-2 dos veces | Pasó en el lab |
| **SÉPTIMAS CONTRATIEMPO** | Cuatriada | En la «y» | Stab de house; pedido de Juan |
| **SÉPTIMAS HUECOS** | Cuatriada | Pasos libres de kick y bajo, **leídos de Live** | Conversa con el groove |
| **SÉPTIMAS CLAVE** | Cuatriada | Golpes de la clave, **leídos de Live** | Refuerza la clave |
| **ARCO** | 1 3 5 8 5 3 | Corcheas | Lo que gustó en lidia, para el break |

Cada 4 compases, el último golpe anticipa el acorde siguiente (`lab-acordes.md` §3).

## 4. Lectura de la melodía

Para cada combinación se calcula cuánto de la melodía **choca**: una nota a un semitono de una nota del acorde sin ser parte de él, ponderada por duración. **Es información, no un filtro:** un choque puede ser justo el color que se busca, y por eso nada se descarta. Se muestran los más frecuentes, como «Re# sobre A», para decidir de oído.

Se lee la pista `MELODY` o, si no existe, `MOTIVO`. Si no hay melodía, el dato no aparece.

## 5. En Live

Clip de 16 compases en `CHORDS`, sin instrumento: Juan carga ahí el VST. Rige la regla de no pisar lo editado.

## 6. Cómo se valida

**Tests:** notas dentro de la escala, registro, conducción de voces, ritmo, forma, choques y regla de no pisar. **Lo que decide es Juan.**
