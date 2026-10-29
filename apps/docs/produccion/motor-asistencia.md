# Motor de asistencia para terminar tracks — propósito, registro de eventos v2 y primer caso

> **Fecha:** 2026-10-29 · **Estado:** propuesta para que Juan la apruebe; no hay código todavía. No modifica los specs
> `00–06` ni el firmware.
>
> **Depende de:** `ia-local-refinamiento.md` §7 (de identidad a publicación) y §8 (etapas 2–4),
> `auditoria-datasets-adn-2026-10-28.md` §5–§7 y §11, y `protocolo-captura.md` §6 (snapshots).
>
> **Reemplaza, cuando se implemente:** el formato de ficha de `protocolo-captura.md` §5 y `gf_session.py` v1. Las
> fichas viejas no se borran (§6).

## 1. Propósito: el laboratorio está al servicio de terminar tracks

Hasta hoy el laboratorio respondía **"¿qué sonido o patrón me gusta?"**. Era el paso correcto, porque sin paleta no hay
nada que proponer. Pero el objetivo es otro: **que más ideas tuyas lleguen a track terminado**
(`ia-local-refinamiento.md` §7). A esa meta le sirve otra pregunta:

> **"En este track, en este punto, ¿qué falta y cuál de estas opciones lo resuelve?"**

La pregunta cambia la **unidad de datos**. Ya no es un clip aislado con una estrella: es una decisión **dentro de un
track en proceso**. Esa decisión guarda qué sección había, qué faltaba, qué se propuso, qué usaste y qué cambiaste.

| | Laboratorio (hasta hoy) | Motor de asistencia (lo que se construye) |
|---|---|---|
| Pregunta | ¿Me gusta este sonido / patrón? | ¿Qué resuelve este hueco de este track? |
| Contexto | Loop de prueba fijo (A/B/C…) | Tu track real: sección, lo que ya suena |
| Resultado | Favoritos, ganadores | Propuesta usada, editada o descartada, y por qué |
| Papel del lab | El fin | **El catálogo** de donde salen las propuestas |

**Qué es el motor:** una herramienta local en el Mac. Mira el estado del track y propone **tres opciones** para un
hueco concreto: una variación MIDI, un sonido del Brain, una transición o un build. Te deja escucharlas en Live y
**registra lo que pasó**. Con el tiempo, ese registro le enseña a ordenar mejor las propuestas.

**Qué no es (por ahora):**
- no compone tracks completos;
- no genera audio;
- no decide por ti;
- no toca el firmware ni el Teensy;
- no automatiza Live más allá de cargar clips en un set que es copia del tuyo.

El ML llega solo cuando haya registro suficiente para compararlo contra reglas simples (§9).

## 2. El ciclo

```
 track en proceso ──► hueco ──► 3 propuestas ──► escucha en contexto ──► decisión ──► uso real
   (versión .als)    "falta X     (catálogo +        (tu track, no el        elegir /     ¿quedó en el
                     en la         generadores)       loop de prueba)        editar /     track? ¿se
                     sección Y"                                             ninguna      cambió luego?
        ▲                                                                                     │
        └────────────────────────── registro de eventos (§4) ◄────────────────────────────────┘
```

1. **Track en proceso.** Un proyecto de Live tuyo con un ID estable. Cada vez que el motor trabaja sobre él, primero
   congela la versión: un snapshot del `.als` con su hash (`protocolo-captura.md` §6).
2. **Hueco.** Lo nombras tú en pocas palabras: "falta un lead en el drop", "el break está vacío", "el bajo no
   conversa con el kick en el verso". Se guarda con la sección y lo que ya suena. Más adelante el motor podrá
   sugerirlo, pero siempre lo confirmas tú.
3. **Tres propuestas.** Salen del catálogo (presets, MIDI favoritos, kits) y de los generadores (`make_*`), ajustadas
   al tempo, la tonalidad y la sección del track. **Una de cada tres veces**, una de las opciones viene **fuera de tus
   favoritos** (exploración), para que el catálogo no estreche tu gusto (auditoría §7).
4. **Escucha en contexto.** Las propuestas entran en una **copia** de tu set, en escenas, igual que hoy con
   `fill_lab_set.py`. Se escuchan sobre tu track, no sobre el loop de prueba.
5. **Decisión.** Respondes solo tres cosas: **cuál** (o empate, o las dos sirven, o ninguna), **para qué** y **qué le
   cambiarías**. Todo lo demás lo anota la herramienta.
6. **Uso real.** Más tarde (al guardar o cerrar el track) se anota si la propuesta **sigue en el track**, si la
   editaste o si la quitaste. Es la señal más valiosa: "me gustó al escucharla" y "la usé" son cosas distintas.

## 3. Piezas: qué existe y qué falta

| Pieza | Hoy | Falta |
|---|---|---|
| Catálogo | `GrooveForge_Dataset_v1` (MIDI, presets, kits) + snapshots con hash | Rol, registro útil y `pairs_with` en los **finalistas** (no en los 118) |
| Generadores | `make_adn_clips`, `make_lead_lab`, `make_chord_lab`, `make_perc_patterns` | Recibir tempo / tonalidad / sección como entrada (hoy: 124 BPM y La fijos) |
| Sonido | Brain + `gf_presets.py` | Nada nuevo para el primer caso |
| Escucha | `fill_lab_set.py` carga escenas en un `.als` | Cargar en una copia **de tu track**, no solo en los sets del lab |
| Registro | `gf_session.py` v1 (A/B, rechazados, prosa en campos sueltos) | **Eventos v2** (§4) |
| Ranking | — | Después de §9; primero, reglas simples |

## 4. Registro de eventos v2

### 4.1 Principios (de la auditoría, §5 P0–P1)

1. **Una escucha se identifica por lo que sonó, no por nombres.** Cada estímulo (MIDI, preset, backing) va con su
   **SHA-256** del snapshot. Si algo histórico no se sabe, va como `unknown`: no se rellena con lo probable.
2. **Estados explícitos.** Los estados posibles son:
   - `prefer` (con el elegido);
   - `tie` (empate);
   - `both_useful` (las dos sirven, para papeles distintos);
   - `neither` (ninguna);
   - `uncertain` (no estoy seguro);
   - `not_auditioned` (no se escuchó).

   "Sin marcar" **no** es rechazo, y `both_useful` **no** es una preferencia.
3. **El denominador se guarda.** Se registra qué se presentó y qué se escuchó de verdad. Sin eso no se puede medir
   cuántas propuestas sirven.
4. **Tus palabras, separadas de las mías.** `reason` es tu frase literal. `inference` es mi interpretación, siempre con
   estado `hypothesis` hasta que una evidencia la confirme o la descarte.
5. **Nada se sobrescribe.** El registro es **solo para agregar** (`.jsonl`, una línea por evento). Una opinión nueva no
   borra la vieja: la matiza con un evento `revision` que la referencia.
6. **El linaje se conserva.** Cada MIDI o preset derivado lleva `parent` y `family`, para que sus variantes no queden
   repartidas entre entrenamiento y prueba (auditoría §10).
7. **El origen va aparte de la etiqueta.** Las autorías posibles son `authored_by_juan`, `generator_proposal`,
   `juan_edited`, `reference` y `augmentation`. El dialecto que trae el nombre de un MIDI es una **expectativa del
   generador**, no lo que se observó.

### 4.2 Dónde vive

```
GrooveForge_Dataset_v1/
  metadata/
    eventos/<AAAA-MM-DD>.jsonl      ← un evento por línea, solo se agrega
    tracks/<track_id>.json          ← el track en proceso: secciones, huecos abiertos, versiones
    sesiones/*.json                 ← fichas v1, se conservan tal cual (§6)
  snapshots/<fecha>/manifest.json   ← los hashes que referencian los eventos
```

### 4.3 Tipos de evento

| Tipo | Cuándo | Lo que respondes tú |
|---|---|---|
| `hueco` | Abres una necesidad en un track | Qué falta y dónde |
| `audicion` | Escuchas las propuestas para un hueco (o un A/B del lab) | Cuál, para qué, qué cambiarías |
| `edicion` | Cambias una propuesta (notas, preset, mezcla) | Nada: se guardan el antes y el después |
| `uso` | Al guardar o cerrar el track: ¿la propuesta sigue ahí? | Sí / editada / quitada (y por qué, si quieres) |
| `revision` | Reescuchas algo otro día, con el orden cambiado | Lo mismo que en `audicion` |
| `regla` | Una conclusión ("mixtas > largas en leads") | Nada: la escribe el asistente con su evidencia y estado `hypothesis`, `confirmed` o `superseded` |

### 4.4 Ejemplo (ilustrativo, no es una decisión real)

```json
{"schema_version": 2, "event_id": "2026-11-02T19:41:07-a3", "type": "audicion", "session_id": "2026-11-02-a",
 "track": {"id": "trk-001", "als_sha256": "…", "section": "drop", "bpm": 124, "key": "Am"},
 "hueco_id": "2026-11-02T19:30:12-h1",
 "candidates": [
   {"id": "c1", "midi_sha256": "…", "patch_sha256": "…", "patch_name": "JD LEAD BODY",
    "origin": "generator_proposal", "generator": "make_lead_lab@5e48ddc", "family": "lead_mixta_b",
    "exploration": false, "auditioned": true},
   {"id": "c2", "…": "…", "auditioned": true},
   {"id": "c3", "…": "…", "exploration": true, "auditioned": false}
 ],
 "presentation": {"order": ["c2", "c1", "c3"], "level_matched": "by_ear"},
 "decision": {"state": "prefer", "chosen": "c1", "for": "lead del drop",
              "reason": "corta y larga mezcladas, no tapa la tambora", "change": "menos cola en la última"},
 "inference": {"claim": "en el drop prefiere frases que dejan huecos a la percusión", "status": "hypothesis"},
 "firmware_loaded": "unknown"}
```

## 5. Lo que haces tú y lo que hace la herramienta

**Tú:** nombrar el hueco, escuchar y decir **cuál, para qué y qué cambiarías**. Nada más. Si un día no quieres
anotar, la herramienta guarda igual la exposición y queda `decision.state = uncertain`. **No eres anotador.**

**La herramienta, sola:**
- los hashes y el snapshot;
- el orden de presentación;
- el BPM y la sección (del set);
- el commit del generador;
- el estado de edición (comparando el `.als` antes y después);
- el evento `uso` al detectar si el clip sigue en el set.

## 6. Migración de las 7 fichas del 28-oct

No se reescriben. Un script las **convierte** en eventos con `"source": "ficha_v1"` y estas reglas:
- Lo que la ficha dice explícitamente (elegido, rechazado, tu frase) pasa como evento.
- Lo que no se puede saber (qué preset sonó en cada clip de la ronda de dialectos, qué se escuchó y qué no) queda
  `unknown` o `not_auditioned`.
- `"ambos (papeles distintos)"` (bajo) pasa como `both_useful`, no como B.
- Las interpretaciones de los docs del lab ("el color va en el acorde", "la segunda aumentada es la causa") pasan como
  `regla` con estado `hypothesis` (auditoría §4).
- Los MIDI y presets se enlazan por hash con el snapshot `2026-10-29`.

## 7. Primer caso de punta a punta: acordes → estudio de 60–90 s

Es el ciclo de la auditoría §11, ahora como **primer track terminado con asistencia**. Corresponde a la "Sesión C" de
`ia-local-refinamiento.md` §3.

| Paso | Trabajo | Entrega demostrable |
|---|---|---|
| 1. Congelar | Snapshot (hecho: `2026-10-29`). Subir WOOD2/3 y DARK2/3 al Brain y congelar de nuevo | Cada candidato se recupera por hash |
| 2. Timbre | WOOD / WOOD2 / WOOD3 con el mismo patrón, nivel parejo y backing fijo → `audicion` | Una elección o un empate bien registrado |
| 3. Ritmo | Con el timbre elegido: el mismo voicing en corcheas / tresillo / contratiempo → `audicion` | Saber qué parte del gusto viene del ritmo |
| 4. Track | Abrir `trk-001`: bajo JD SUB MID + perc + acordes. Huecos: lead del drop, transición, break → 3 propuestas cada uno | Un estudio de 60–90 s con transición y cierre |
| 5. Revisar | Otro día: reescuchar 4–6 decisiones con el orden cambiado → `revision` | Gusto estable vs. primera impresión |
| 6. Capturar | Grabación final: aislado + stem + mix de referencia (`protocolo-captura.md` §3), a 48 kHz | Un conjunto pequeño, trazable y audible |

**Puerta de salida:** se pueden recuperar todos los estímulos, cada elección tiene su motivo, hay al menos una
corrección tuya guardada (antes/después) y existe una pieza de 60–90 s que los usa. Entonces se repite con leads.

Meta de captura: **20–30 decisiones completas en varios días**. Es manejable, no un mínimo científico.

## 8. Implementación (después de aprobar este doc)

1. `gf_log.py` (nuevo, reemplaza a `gf_session.py`). Comandos:
   - `track new`, `hueco`, `audicion` (presenta y pregunta), `uso`, `revision`, `show`;
   - lee los hashes del último snapshot.

   `gf_session.py` queda solo para leer las fichas v1.
2. `migrate_v1.py`: las 7 fichas → eventos (§6).
3. `fill_lab_set.py --track <als>`: carga las propuestas de un hueco en una **copia** del set de tu track y guarda el
   orden de presentación.
4. Tests (`pytest`):
   - esquema válido;
   - solo-agregar (no se pierde ningún evento);
   - `both_useful` nunca se cuenta como preferencia;
   - la migración no inventa campos (lo ausente queda `unknown`);
   - el linaje no cruza entrenamiento y prueba.

**Fuera de alcance de esta implementación:**
- detectar huecos automáticamente;
- ranking;
- generación de audio;
- AbletonOSC / Max for Live;
- cualquier cambio de firmware.

## 9. Cómo saber si el motor sirve (antes de cualquier ML)

| Medida | Cómo se calcula con los eventos |
|---|---|
| Propuestas útiles | `uso = sí o editada` / propuestas **escuchadas** |
| Rapidez | Tiempo desde `hueco` hasta la decisión |
| Terminación | Estudios / tracks con transición y cierre |
| Estabilidad | Coincidencias en `revision` vs. la `audicion` original |
| Exploración | Cuántas propuestas fuera de favoritos terminaron usadas |

El primer experimento de ML (un ranking pequeño) solo se justifica si, con estos datos, **ordena mejor que "favoritos
del mismo rol y dialecto primero"**. Si no gana a esa regla, no entra.

## 10. Decisiones abiertas (para Juan)

1. **¿Cuál es el primer track?** Puede ser el estudio de acordes del §7 o un track tuyo que ya esté a medias. Uno real
   a medias da datos más valiosos, pero mezcla más variables.
2. **¿El registro se lee en Notion?** Los eventos viven en el dataset (JSONL). Se podría publicar un resumen por track
   en Notion; no es necesario para empezar.
