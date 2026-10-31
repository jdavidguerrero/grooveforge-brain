# Bajo groove — patrones por caso

> Teoría antes del código (CLAUDE.md). Los patrones del ADN están en `sesion-paleta-adn.md` §4 (`bass_adn_*`); acá van
> los que se generan para cada estudio del motor de asistencia (`motor-asistencia.md`).

## 1. Caso 2 (est-indie): bajo sobre la base 3-3-2 (2026-10-31)

### Lo que ya está decidido

- **Percusión:** sirven las dos bases con el 3-3-2 `cascara_332` y `bullerengue_tech` (audición
  `20261029T170601-a30b6d7`, "están limpias pero dan un toque interesante"). Ver `percusion-groove.md` Rev. 10b.
- **Timbre:** se fija **ROLLER SUB** (capa 7, Model D + Juno), el bajo que ganó en el caso 1. Lo que se decide es el
  **patrón**, no el sonido; el timbre de bajo sigue abierto como pendiente de paleta.
- **Armonía:** la progresión de la regla confirmada de leads (`m-3d232fac2d57`): **menor con V prestado**,
  Am – F – C – E. Es la que pasó en los cinco dialectos y la que tiene el "acorde de afuera" (el Mi mayor, con Sol#)
  del ADN armónico (`lab-acordes.md`). Los acordes indie siguen sin decidir; el bajo solo marca las fundamentales.

### Reglas que cumplen las tres opciones

1. **Ninguna nota en las semicorcheas del kick** (1, 5, 9, 13 — pasos 0, 4, 8, 12). En un sistema de club el kick y
   el sub comparten 40–100 Hz; si suenan juntos se suman en fase o se cancelan según la nota, y el kick pierde ataque.
   Es la regla de los bajos `tambora` y `rodante` del ADN (tiene test, `sesion-paleta-adn.md`) y del "bajo que no se
   come el kick" de `motor-asistencia.md` §4.
2. **Atracción por semitono al volver a La:** en el compás 4 (Mi mayor) la última semicorchea es **Sol#**, la sensible,
   que resuelve un semitono arriba en el La del compás 1. Es el mismo tirón del Sol# en el acorde de Mi
   (dominante prestada del menor armónico; Aldwell & Schachter, *Harmony and Voice Leading*, cap. sobre la sensible).
3. **Registro:** fundamentales entre Mi1 (41 Hz) y Do2 (65 Hz), la más cercana a La1: La1 · Fa1 · Do2 · Mi1. Grave
   limpio y mono, sin saltos de más de una sexta entre compases.

### Las tres opciones (cambia el lugar del 3-3-2)

| Patrón | Idea | Relación con el 3-3-2 de la percusión |
|---|---|---|
| `bass_indie_rodante332` | Semicorcheas rodantes en todos los pasos fuera del kick (el `rodante` del ADN, dialecto indie / melodic); los acentos caen en 3, 6, 11 y 14; octava en el 14 | El 3-3-2 dentro del rodar: los acentos son los golpes del tresillo que no coinciden con el kick |
| `bass_indie_tresillo_aire` | Cuatro notas largas por compás: 3, 6 (octava), 11 y 14 (quinta) | El 3-3-2 sin su primer golpe (que es del kick): deja aire, como pidió el caso 1 ("los que dejan espacio ganan") |
| `bass_indie_contratiempo` | Corcheas a contratiempo (2, 6, 10, 14) con ghosts en 7 y 15; octava en el 14 | **Sin** 3-3-2: el bajo del indie dance / house en contratiempo. Es la opción de contraste: si gana, el 3-3-2 vive solo en la percusión |

Por qué tres lugares y no tres sonidos: con la percusión ya llevando el 3-3-2 la pregunta abierta es **cuánto** del
tresillo debe cargar el bajo — todo el rodar (A), solo sus golpes (B) o nada (C). La respuesta dice si el "toque
tribal" que Juan quiere vive en la percusión o en el grave.

Referencias: el tresillo y la clave como esqueleto del bajo afrolatino en Toussaint, *The Geometry of Musical Rhythm*
(2013, cap. del tresillo) y Peñalosa, *The Clave Matrix* (2009, cap. sobre el bajo "anticipado"); el bajo en
contratiempo del house en Butler, *Unlocking the Groove* (2006, cap. 3).

### Qué no cubre

- No compara timbres (ROLLER SUB fijo) ni el nivel kick/bajo (nivel sin igualar: `level_matched: unknown`).
- No prueba las otras progresiones indie (frigia dominante, frigia); si el patrón gana, se pasa a ellas después.
- Los ghosts y velocities son programados, no tocados (auditoría §7).

### Archivos

`midi/adn/estudio_indie/bass_indie_<patrón>.mid` (`tools/paleta-lab/make_estudio_clips.py`, `indie_bajos()`).
La audición va con la capa por CC 18 y, como fondo, el kick 4x4 y las dos bases de percusión elegidas.
