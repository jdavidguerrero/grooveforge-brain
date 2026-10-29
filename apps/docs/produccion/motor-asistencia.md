# Motor de asistencia para cerrar piezas — propósito, registro de eventos v2 y primer caso

> **Fecha:** 2026-10-29 · **Rev. 2** (misma fecha): se incorpora la revisión de Juan. Cambios: decisiones pendientes
> en vez de huecos; estados en tres dimensiones; migración con grado de certeza; presencia distinta de uso; contrato
> v1 más pequeño; criterio de resolución; métricas de progreso; límites a las reglas. **Estado:** aprobado como
> dirección; falta implementar (§9).
>
> No modifica los specs `00–06` ni el firmware. Los presets se suben al Brain con el bibliotecario existente.
>
> **Depende de:** `ia-local-refinamiento.md` §7–§8, `auditoria-datasets-adn-2026-10-28.md` §5–§7 y §11, y
> `protocolo-captura.md` §6 (snapshots).
>
> **Reemplaza, al implementarse:** la ficha de `protocolo-captura.md` §5 y `gf_session.py` v1. Las fichas viejas se
> conservan (§7).

## 1. Propósito

> El motor ayuda a resolver **decisiones pendientes** de una pieza mediante **cambios pequeños y recuperables**.
> Presenta **hasta tres** alternativas, incluida la posibilidad de **conservar lo existente**, y registra el contexto,
> la evidencia de escucha, la decisión y su evolución posterior. Su éxito se mide por **decisiones resueltas y piezas
> cerradas con menos fricción**.

La pregunta central deja de ser "¿me gusta este sonido?" y pasa a ser:

> **"En esta sección, ¿qué decisión impide avanzar y cuál es el cambio mínimo que la resuelve?"**

Terminar música muchas veces no consiste en agregar. Consiste en quitar, simplificar, reemplazar o decidir que algo
ya funciona:

| Decisión pendiente | Posible solución |
|---|---|
| El drop no tiene fuerza | Quitar capas antes de la entrada |
| El bajo no conversa con el kick | Cambiar duración o posición de notas |
| El break parece vacío | Conservar el espacio y trabajar una automatización |
| No sé si falta un lead | Comparar con y sin lead |
| Sigo probando presets | Elegir uno y cerrar la decisión |

El laboratorio de paleta no se pierde: pasa a ser **el catálogo** de donde salen las propuestas.

**Qué no hace en v1:**
- no compone piezas completas ni genera audio;
- no decide por ti;
- no modifica firmware;
- no automatiza Live (sin AbletonOSC ni Max for Live);
- no detecta decisiones pendientes por su cuenta.

## 2. Contrato v1

> El motor **prepara una sesión de audición sobre una copia guardada**, con un **fragmento de contexto
> seleccionado**. **Registra automáticamente lo verificable y deja como desconocido lo que no puede observar.**

El contexto identifica al menos:
- la versión del proyecto (copia del `.als` guardada con **Collect All and Save**, para que viajen los samples);
- la sección, con inicio y fin en compases;
- el backing;
- el estado relevante de instrumentos y FX. Para el Brain, esto es el snapshot de presets.

El hash del `.als` identifica el archivo. **No garantiza** recuperar plugins ni el estado del hardware: eso lo cubren
el snapshot de presets y lo que se declare. Las escenas en una copia del set **no reproducen** las automatizaciones del
Arrangement. Para el primer caso (un estudio hecho de clips) basta; para piezas armadas en el Arrangement es una
limitación conocida.

## 3. El ciclo

```
 pieza (copia     ──► decisión pendiente ──► hasta 3 opciones ──► audición ──► decisión ──► presencia ──► uso
 guardada)            + criterio de            (incluye             (fragmento    (tus         (lo ve la      (lo confirmas
                      resolución               "dejar así")         de contexto)  palabras)    herramienta)   tú)
     ▲                                                                                                          │
     └─────────────────────────────────── registro de eventos (§5) ◄─────────────────────────────────────────────┘
```

1. **Decisión pendiente.** La dices en una frase y el asistente la estructura (§4). Se cierra una a la vez.
2. **Opciones.** Hasta tres, sin inventar relleno cuando solo hay dos útiles. "Conservar lo que hay" es siempre una
   opción válida.

   **Exploración:** la política inicial es que una de cada tres veces entra una opción de fuera de los favoritos. Es
   **ajustable** (menos al cerrar, más al explorar) y se registra cuándo se aplicó.
3. **Audición.** En la copia, sobre el fragmento de contexto, a nivel parejo. Opcionalmente a ciegas, con escenas
   **A / B / C** sin nombre; la correspondencia la guarda la herramienta.
4. **Decisión.** Una frase tuya.
5. **Presencia → uso** (§6).

## 4. Decisión pendiente con criterio de resolución

Cada decisión pendiente guarda:

| Campo | Ejemplo |
|---|---|
| Problema | El bajo tapa el ataque del kick |
| Conservar | Las notas y el carácter del bajo |
| Cambiar | Duración, posición o envolvente |
| Criterio de resolución | Distingo el ataque del kick y sigo prefiriendo el groove |
| Estado | `open` · `resolved` · `postponed` · `cancelled` |

Tú dices una frase ("el bajo se come el kick pero me gusta el groove") y el asistente la propone estructurada. La
corriges solo si quedó mal. Es el dato más útil para aprender: **qué cambio resolvió qué problema y bajo qué
restricciones.**

## 5. Registro de eventos v2

### 5.1 Regla de esfuerzo

**La precisión vive en el esquema, no en tu esfuerzo.** Tú dices una frase. La herramienta llena lo verificable y deja
el resto como `missing` o `unknown`, **sin hacer más preguntas**. Ninguna escucha se convierte en formulario.

### 5.2 Estados en tres dimensiones independientes

| Dimensión | Estados | Nota |
|---|---|---|
| Escucha | `confirmed` · `played_detected` · `not_auditioned` · `unknown` | `played_detected` = sonó mientras estabas, sin confirmar que lo escuchaste. Si no se sabe si se escuchó, es `unknown`, **nunca** `not_auditioned` |
| Respuesta | `provided` · `missing` | No responder **no** es estar indeciso: pudiste estar tocando o cerrar la sesión |
| Decisión (solo si hay respuesta) | `prefer` · `tie` · `multiple_useful` · `neither` · `uncertain` · `keep_current` | `multiple_useful` lleva la lista de IDs y **no** cuenta como preferencia. `keep_current` = la opción "dejar así" |

### 5.3 Principios

1. **Lo que sonó se identifica por hash del snapshot**, con el grado de certeza de esa correspondencia (§7).
2. **El denominador se guarda.** Qué se presentó, en qué orden, y su estado de escucha.
3. **Tus palabras van separadas de las mías.**
   - `reason` es tu frase literal.
   - `inference` es la interpretación del asistente y siempre lleva estado.
4. **Nada se sobrescribe.** El registro es solo para agregar (`.jsonl`). Una opinión nueva matiza la vieja con un
   evento `revision` que la referencia.
5. **El linaje se conserva.** Cada derivado lleva `derived_from` y `family`, también si después se renderiza a audio.
   Así las variantes no quedan repartidas entre entrenamiento y prueba.
6. **El origen va aparte de la etiqueta.**
   - Autorías posibles: `authored_by_juan`, `generator_proposal`, `juan_edited`, `reference`, `augmentation`.
   - El dialecto que trae el nombre de un MIDI es una **expectativa del generador**, no lo observado.

### 5.4 Dónde vive

```
GrooveForge_Dataset_v1/
  metadata/
    eventos/<AAAA-MM-DD>.jsonl      ← un evento por línea, solo se agrega
    piezas/<pieza_id>.json          ← secciones, decisiones pendientes y versiones de cada pieza
    sesiones/*.json                 ← fichas v1, intactas
  snapshots/<fecha>/manifest.json   ← los hashes que referencian los eventos
```

### 5.5 Tipos de evento

| Tipo | Cuándo | Tu parte |
|---|---|---|
| `pendiente` | Abres, resuelves, pospones o cancelas una decisión | Una frase |
| `audicion` | Escuchas las opciones de una decisión (o un A/B del lab) | Cuál, para qué, qué cambiarías |
| `edicion` | Cambias una opción | Nada: antes, después y `derived_from` |
| `presencia` | La herramienta ve la opción en una versión guardada | Nada |
| `uso` | La opción está incorporada a una sección o conservada en una versión revisada | Confirmarlo; si la quitas, por qué (opcional) |
| `revision` | Reescuchas otro día con estímulo **y contexto equivalentes** | Lo mismo que en `audicion` |
| `regla` | Una conclusión con su evidencia (§8) | Confirmarla o no |

### 5.6 Ejemplo (ilustrativo, no es una decisión real)

```json
{"schema_version": 2, "event_id": "2026-11-02T19:41:07-a3", "type": "audicion", "session_id": "2026-11-02-a",
 "pieza": {"id": "est-acordes", "als_sha256": "…", "section": {"name": "groove", "bars": [9, 16]}, "bpm": 124},
 "pendiente_id": "2026-11-02T19:30:12-p1",
 "options": [
   {"id": "keep", "kind": "keep_current", "listen": "confirmed"},
   {"id": "o1", "midi": {"sha256": "…", "match": "exact"}, "patch": {"name": "JD PLK WOOD2", "sha256": "…",
    "match": "exact"}, "origin": "generator_proposal", "generator": "make_chord_lab@ce61fb4",
    "family": "acordes_tresillo", "exploration": false, "listen": "confirmed"},
   {"id": "o2", "…": "…", "exploration": true, "listen": "unknown"}
 ],
 "presentation": {"order": ["o1", "keep", "o2"], "blind": true, "level_matched": "by_ear"},
 "response": "provided",
 "decision": {"state": "prefer", "chosen": ["o1"], "reason": "redonda, deja oír la tambora",
              "change": "un poco menos de cola"},
 "inference": {"claim": "en el groove prefiere acordes que dejan aire a la percusión", "status": "hypothesis"},
 "firmware_loaded": "unknown"}
```

## 6. Presencia no es uso

Una opción puede seguir en el proyecto y aun así no haberse usado. Puede estar:
- en una escena de pruebas;
- silenciada;
- fuera del arreglo;
- como alternativa descartada;
- convertida en audio;
- tan editada que apenas queda la original.

| Escalón | Quién lo aporta | Evidencia |
|---|---|---|
| Detectada en el proyecto | Herramienta | El clip o preset aparece en una versión guardada |
| Incorporada a una sección | Tú (o una relación inequívoca con la versión final) | Suena en la sección, no en una escena de pruebas |
| Conservada tras revisión | Tú, en otra sesión | Sigue en la versión revisada |

**Quitar no significa que fuera mala.** Puede que desapareciera la sección o que cambiara la dirección de la pieza. Si
dices el motivo, se guarda; si no, `missing`. El vínculo `derived_from` se conserva aunque la opción se renderice a
audio. En v1 no se resuelven automáticamente esos casos.

## 7. Migración de las 7 fichas del 28-oct

**El snapshot del 29 no prueba lo que sonó el 28.** Cada estímulo migrado guarda:
- `source`: archivo y campo exacto de la ficha (p. ej. `2026-10-28_lead.json#lab_leads.favoritos[3]`);
- `original_ref`: el nombre o la ruta tal como estaba en la ficha;
- `snapshot_asset`: el asset localizado en el snapshot, si existe;
- `match`, con uno de estos valores:
  - `exact`: el MIDI se regenera en el commit del generador vigente el 28 y su hash coincide con el del snapshot;
  - `name_match_only`: coincide solo el nombre. Es el caso de casi todos los presets, porque la librería no estaba
    versionada;
  - `unknown`: no se localiza.

Las demás reglas:
- Lo explícito (elegido, rechazado, tu frase) pasa con `response: provided`.
- Lo que la ficha no dice queda `response: missing` y escucha `unknown`. **Nunca** `not_auditioned` por defecto.
- `"ambos (papeles distintos)"` (bajo) pasa a `multiple_useful`.
- Las interpretaciones de los docs del lab pasan a `regla` con estado `hypothesis`.
- **Idempotente:** cada evento migrado tiene un ID derivado de su `source`. Correrla dos veces no duplica decisiones.

## 8. Reglas: qué convierte una hipótesis en confirmada

Una regla tiene:
- **la afirmación con su alcance**, por ejemplo: "en estos drops y con estos acompañamientos, Juan prefirió frases
  mixtas". Nunca "mixtas > largas" en general;
- **las evidencias originales** (IDs de eventos `audicion` / `uso`), no textos del asistente;
- **el contexto donde aplica**;
- **las excepciones y la evidencia contraria**;
- **quién la confirmó** y cuándo.

El asistente **no puede confirmar** una regla porque sus propios textos la repitan. Solo la confirmas tú, con
evidencia a la vista.

Hay dos clases distintas:
- **Preferencia declarada**: "quiero Tribe como firma". Es una decisión artística tuya, válida sin estadística.
- **Tendencia observada**: sale de los eventos y queda en `hypothesis` hasta que la confirmes.

## 9. Implementación (siguiente paso)

1. **`gf_log.py`** (reemplaza a `gf_session.py`, que queda solo para leer v1). Agrega eventos al `.jsonl` desde una
   frase. Llena hashes del último snapshot, sesión, orden y escucha `unknown` salvo confirmación.
2. **`migrate_v1.py`**: §7, idempotente, con regeneración de MIDI en el commit histórico para intentar `exact`.
3. **`fill_lab_set.py --pieza <als>`**: hasta tres opciones (más "dejar así") en una **copia** de la pieza. Escenas
   opcionalmente ciegas; guarda el orden y la correspondencia.
4. **Tests (`pytest`):**
   - esquema;
   - solo agregar;
   - `multiple_useful` y `keep_current` no cuentan como preferencia;
   - `missing` ≠ `uncertain`, y `unknown` ≠ `not_auditioned`;
   - la migración es idempotente y no inventa campos;
   - el linaje no cruza entrenamiento y prueba.

## 10. Primer caso: una sola decisión del estudio de acordes

Se cierra **una** decisión de punta a punta antes de abrir otras (lead, transición, break vienen después y de a una).

| Paso | Trabajo | Entrega |
|---|---|---|
| 1. Congelar | Subir WOOD2/3 y DARK2/3 al Brain → snapshot nuevo | Cada opción recuperable por hash |
| 2. Pieza | Estudio con bajo JD SUB MID + perc + acordes; copia con Collect All and Save | `est-acordes` con secciones |
| 3. Decisión | "¿Qué timbre de acordes en el groove?" con criterio de resolución; opciones: WOOD actual (dejar así) · WOOD2 · WOOD3 | Un `pendiente` |
| 4. Audición | A ciegas, nivel parejo, sobre el fragmento del groove | Una `audicion` |
| 5. Cerrar | Aplicar lo elegido → `resolved` (o `postponed`) | Decisión cerrada |
| 6. Revisar | Otro día, mismo estímulo y contexto → `revision` | Estable o no |

**Puerta de salida:**
- se pueden recuperar las opciones;
- la decisión tiene criterio y motivo;
- hay presencia detectada y uso confirmado;
- queda un **estudio cerrado con asistencia** de 60–90 s.

Después se repite con la siguiente decisión.

## 11. Métricas de progreso (antes de cualquier ML)

| Métrica | Cómo |
|---|---|
| Utilidad | Tres tasas separadas sobre opciones **escuchadas**: elegidas en audición, incorporadas, conservadas tras revisión |
| Rapidez | Tiempo **activo** hasta `resolved`; pausas y esperas aparte |
| Cierre | Estudio cerrado · arreglo completo · pieza lista para publicar (tres niveles distintos) |
| Estabilidad | `revision` vs `audicion` solo con estímulo y contexto equivalentes |
| Exploración | Utilidad de las opciones de fuera de favoritos **y** su costo de escucha |

**Ranking:** un modelo solo entra si, con **el mismo presupuesto de opciones y material reservado**, resuelve mejor
que la regla "favoritos del mismo rol primero". Si el motor muestra casi solo favoritos, acumula datos casi solo de
favoritos y puede parecer mejor sin descubrir nada; por eso la exploración se registra y se evalúa aparte.

## 12. Decisiones abiertas

1. ~~¿Cuál es el primer caso?~~ Una sola decisión del estudio de acordes (§10).
2. ¿Se publica un resumen por pieza en Notion? No es necesario para empezar.
