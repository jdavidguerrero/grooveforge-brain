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
| **BLOQUE LARGO** | Tríada | Un acorde sostenido por compás | Pedido de Juan (2026-11-08): acordes largos, no solo punteados |
| **SÉPTIMAS LARGAS** | Cuatriada | Un acorde sostenido por compás | Ídem; con un sonido sostenido (JD SUS DARK*) |
| **DÉCIMAS LARGAS** | Fundamental + décima | Un acorde sostenido por compás | Ídem; el más abierto, deja aire a la melodía |

Cada 4 compases, el último golpe anticipa el acorde siguiente (`lab-acordes.md` §3), salvo en los largos: ahí la anticipación borraría el compás entero del acorde actual.

## 4. Lectura de la melodía

Para cada combinación se calcula cuánto de la melodía **choca**: una nota a un semitono de una nota del acorde sin ser parte de él, ponderada por duración. **Es información, no un filtro:** un choque puede ser justo el color que se busca, y por eso nada se descarta. Se muestran los más frecuentes, como «Re# sobre A», para decidir de oído.

Se lee la primera pista con notas entre `LEAD`, `MELODY` y `MOTIVO`, en ese orden, y todos sus clips del arreglo. Si no hay melodía, el dato no aparece.

## 5. En Live

Clip de 16 compases en `CHORDS`, sin instrumento: Juan carga ahí el VST. Rige la regla de no pisar lo editado.

## 6. Cómo se valida

**Tests:** notas dentro de la escala, registro, conducción de voces, ritmo, forma, choques y regla de no pisar. **Lo que decide es Juan.**

## Sonido (2026-11-08)

Juan no encuentra presets de acordes en el VST. Antes de generar nada nuevo se oyen los que ya existen y nunca se probaron en el VST (`lab-acordes.md` §9): **JD PLK WOOD2** (elegido para el groove el 2026-10-29), **WOOD3** (variación), **JD SUS DARK** y sus variaciones **DARK2 / DARK3**, que no se habían oído. `gf_presets.py hacia-vst` los escribe como `.gfl` en `GrooveForge_Library/presets/acordes/`, sin tocar el Brain. Pluck para los patrones cortos (tresillo, contratiempo, clave, huecos); sostenidos para bloque, décimas y séptimas largas.

**Capas Prophet + DX7 (2026-11-08, para el VST):** mismo esquema que Juno + DX7, el analógico pone el cuerpo y el DX7 solo la tine del golpe, pero con el Prophet (más cálido; WARM KEYS ya gustó como lead) y pensadas para los patrones largos. Generadas en `tools/paleta-lab/make_jd_keys.py` (`LAYERS_PROPHET`) desde WARM KEYS, sin tocar el Brain.

| Capa | Prophet (cuerpo) | DX7 | Busca |
|---|---|---|---|
| **JD SUS WARM** | Pulso + sierra una octava abajo, filtro 750 Hz, S 0,45 | Tine suave, −9 dB | Lo más cálido |
| **JD SUS WARM2** | Pulso + sierra en la misma octava, filtro 1,2 kHz | Tine más presente, −5 dB | Más brillo, más Electric |
| **JD SUS POLY** | Dos sierras, B desafinada 7 cents, ataque 20 ms, S 0,55 | Tine muy corta, −10 dB | Ancho, casi pad |

## Pendiente: carácter de las progresiones (Juan, 2026-11-08)

Etiquetar cada progresión por carácter (luminosa o alegre, melancólica, eufórica, sombría) para tener más contexto al elegir. Juan lo deja para el futuro; en `organic-id-GF` se quedó con A – B. Idea de partida: en la lidia dominante, solo mayores = luminosa (el II mayor es el levante lidio); peso en F#m y Em = melancólica; el aumentado G+ antes de volver a A = eufórica; el disminuido D#dim = sombría.
