# GrooveForge — Modelo de Rampa, Apertura y Secuencia

> **Qué es este documento:** la capa estratégica (el "sistema operativo") por encima del
> backlog táctico (`00-content-playbook.md`) y del currículo de temporadas (GrooveMakers).
> Responde tres cosas que las otras no: (1) cómo la meta crece por etapas con compuertas,
> (2) qué mostrás y qué protegés, (3) en qué orden, sin quemarte.
>
> **Escrito para una realidad concreta:** solo founder, tiempo parcial (10-18h/sem), con
> ansiedad por exceso de frentes, que necesita feedback corto para sostener constancia, y
> que ya tiene el Brain ~90% construido y un e-drum parcialmente implementado.
>
> Última actualización: 2026-06-08

---

## 1. La idea madre: contenido y producto son UNA actividad, no dos

Tu problema nunca fue "qué construir". Fue **foco + constancia**, y ya diagnosticamos que
la constancia se rompe por falta de feedback corto. Este modelo resuelve eso fusionando
dos cosas que venías tratando como separadas:

```
        ANTES (lo que te quemaba)              AHORA (el modelo)
   ┌─────────────────────────────┐      ┌─────────────────────────────┐
   │  Construir el Brain (solo,   │      │   Construir el Brain ES      │
   │  sin deadline, sin feedback) │      │   hacer el contenido.        │
   │            +                 │      │                              │
   │  Hacer contenido (trabajo    │  →   │   El calendario de publicar  │
   │  extra encima del anterior)  │      │   es el deadline que te      │
   │            =                 │      │   fuerza a terminar.         │
   │   dos frentes compitiendo    │      │   La audiencia esperando es  │
   │   = ansiedad                 │      │   el feedback que te falta.  │
   └─────────────────────────────┘      └─────────────────────────────┘
```

**Tres efectos que esto produce:**

1. **Contenido = forcing function.** "Publico cada 2 semanas" se vuelve el deadline que un
   solo founder nunca tiene. El Brain se termina porque el episodio lo exige.
2. **Contenido = feedback loop corto.** Rompe el silencio que genera tu ansiedad. Señal en
   días, no en meses.
3. **Contenido = el moat.** La audiencia que te vio construir es lo que ningún clonador copia.

> **Regla #1:** no "saco tiempo para contenido". El contenido es el subproducto natural de
> construir en público. Si construir y filmar se sienten como dos trabajos, lo estás
> sobre-produciendo (ver §5).

---

## 2. La rampa: la meta crece por etapas, y cada etapa se gana

La meta final (dedicarte solo a esto, ~$10k/mes) es el **destino**, no el punto de partida.
No cargás el peso del destino hoy. Cargás solo la etapa actual. Avanzás cuando los datos lo
justifican — no antes.

| Etapa | Horizonte* | Qué hacés | Ingreso realista | Compuerta para avanzar |
|---|---|---|---|---|
| **0 — Validación** | meses 0-3 | Terminar de mostrar el Brain que YA existe. Lista de email. | ~$0 (no es la meta) | ¿Publiqué consistente 8+ piezas? ¿Alguien respondió genuinamente? |
| **1 — Ingreso extra** | meses 3-12 | Terminar el Brain build-in-public. Vender 5-10 founder units. GroovePilot subs. | $300-2k/mes | ¿Vendí las primeras unidades? ¿La lista crece sola? ¿Sigo con energía? |
| **2 — Ingreso real** | meses 12-30 | Brain en tandas (30-60/año). El e-drum entra como Temporada 2. GroovePilot crece. | $2-5k/mes | ¿El ingreso es estable? ¿Quiero esto full-time de verdad? |
| **3 — Dedicación** | meses 30+ | Ecosistema, protocolo, módulos, kits, workshops. La plataforma. | $5-10k+/mes | — (es el destino) |

\* *Horizontes orientativos para tiempo parcial. La rampa puede ser más lenta — y está bien.*

**La regla de oro de la rampa:** **no pre-comprometés la Etapa 3.** El currículo GrooveMakers
(con su ecosistema y protocolo) describe las Etapas 2-3 — es un mapa excelente para *cuando
llegues ahí*, no una deuda que contraés hoy. Hoy solo existe la Etapa 0.

> **Por qué esto baja tu ansiedad:** no estás construyendo una plataforma. Estás terminando
> un synth y mostrándolo. Eso es todo lo que existe en tu cabeza ahora mismo.

---

## 3. Política de apertura: qué muestro, qué protejo (decidido UNA vez)

### 3.1 El principio

Tu moat **no es el secreto** — es la marca, la confianza, el gusto, la ejecución física del
filtro analógico, el soporte y la comunidad. El secretismo no te defiende de un clonador
(que clona vea lo que vea) y sí mata tu motor de crecimiento (la audiencia que confía).

> **La distinción clave:** mostrar **que algo funciona y cómo suena y por qué lo decidiste**
> es marketing (mostralo fuerte). Publicar **el plano listo-para-fabricar / los pesos /
> el backend** es regalar la fábrica (protegé solo eso, y solo lo que de verdad es replicable
> mecánicamente).

### 3.2 Los tres niveles

- **🟢 ABIERTO — mostrar fuerte (tu motor de crecimiento):** el sonido, la visión, las
  decisiones y tradeoffs, los fracasos, el gusto de diseño, las capacidades, la educación
  conceptual. Esto *vende* y *construye confianza*. Nunca lo escondas.
- **🟡 SUPERFICIE — mostrar el qué, reservar la profundidad:** mostrás que existe, cómo
  funciona en concepto, cómo suena, el drama de hacerlo. NO publicás el archivo de producción
  final / el código fuente completo / la receta exacta.
- **🔴 CERRADO — núcleo propietario (lo poco que sí proteger):** lo que es replicable
  mecánicamente y te tomó meses, o es tu negocio recurrente.

### 3.3 La tabla maestra (esto es lo que consultás antes de grabar)

| Subsistema | Nivel | Mostrás | Reservás |
|---|---|---|---|
| **Filtro analógico 2N3904** | 🟡 | topología (es de libro), el matching, la calibración, **cómo suena**, los fracasos | valores finales afinados, gerbers de PCB de producción |
| **Synth engines (6)** | 🟡 | que existen, cómo suenan, el concepto DSP, snippets educativos | el código fuente completo de cada engine |
| **FX (9)** | 🟢 | todo — el sonido seco/mojado, el concepto, son el mejor contenido | (nada crítico; el valor es el conjunto) |
| **Modelos TinyML (3)** | 🟡→🔴 | el concepto, los números (94%, 73KB), por qué on-device | **pesos entrenados, dataset, arquitectura exacta, receta de quantización** |
| **GroovePilot (SaaS)** | 🔴 | el producto en uso, los resultados | algoritmos, backend, infra |
| **Bridge Protocol** | 🟢→ futuro abierto | el concepto, el frame format | (en Etapa 3 se abre a propósito = open-core) |
| **Enclosure / diseño** | 🟢 | todo — guadua, PETG, Fusion, ensamblaje. El gusto ES feature | (nada — mostralo, es diferenciador) |
| **Datos de negocio** | 🔴 (o 🟢 a propósito) | tu decisión: la transparencia radical es una jugada de marca válida | BOM con proveedores, costos, datos de clientes |

### 3.4 El modelo open-core (para cuando llegues a Etapa 3)

Cuando el ecosistema sea real, la estructura coherente es: **abrís la interfaz (el protocolo),
vendés la implementación (tus módulos + el Brain + GroovePilot).** El protocolo abierto
enciende la comunidad; tu hardware y tu SaaS capturan el valor. Esto resuelve el dilema:
ser abierto donde crea red, cerrado donde está tu ingreso.

> **Regla #2 (mata-ansiedad):** esta tabla se decide UNA vez. Frente a la cámara nunca más
> te preguntás "¿esto lo puedo mostrar?". Consultás la fila y listo. Si dudás de algo nuevo,
> el default es 🟢 ABIERTO — porque tu mayor riesgo no es que te copien, es que nadie te conozca.

---

## 4. Secuencia: el Brain primero, y por qué

Tenés tu activo más fuerte ~90% construido y lo estabas por enterrar un año. Lo invertimos:

1. **El Brain es el release principal de las Etapas 0-1.** Razón doble: (a) es tu mejor
   historia ("synth boutique con filtro analógico, hecho en Bogotá, que YA construí"), y
   (b) el contenido se vuelve el deadline que te fuerza a cerrar el último 10%.
2. **El e-drum es Temporada 2 (Etapa 2).** Ya lo tenés parcial — no es desde cero. Pero
   entra *después* de que el Brain valide, no antes. Es tu segundo acto, no el primero.
3. **El ecosistema/protocolo es Etapa 3, condicional.** Vive como sueño documentado en el
   currículo GrooveMakers. No es deuda hoy.

**Importante sobre el e-drum y el "secret module":** mantenelo **standalone** en el contenido
por ahora. La revelación "este e-drum se conecta al Brain" es una *opción futura*, no una
promesa en cámara — atarte al ecosistema desde el día 1 te roba flexibilidad y agrega scope.

---

## 5. Sistema anti-burnout (diseñado, no improvisado)

Tu plan anterior no tenía válvula de escape. Este sí, desde el principio.

- **Unidad mínima sostenible:** la pieza más chica que mantiene el canal vivo si solo tenés
  4h en la semana = **1 clip corto + 1 texto honesto.** "Día X. Hoy logré/fallé en ___.
  Así suena." Publicar. Sin pulir. Si una semana es dura, esto es suficiente. No es fracaso.
- **Compuertas de decisión (gates):** al final de cada etapa (§2) revisás datos + energía y
  decidís *explícitamente* si seguís. **Tenés permiso de parar sin que sea fracaso.** Parar
  con datos es una decisión, no una rendición.
- **Regla de esfuerzo:** nunca dos piezas 🔴 (alto esfuerzo) seguidas. Alterná fácil/difícil.
- **Protección de salud mental:** es recurso crítico, no lujo. Founder quemado no entrega nada.

> **Regla #3:** la meta de la Etapa 0 NO es crecer una audiencia. Es **probar que podés
> sostener el hábito y que a alguien le importa**, al menor costo posible. Si eso falla,
> te ahorraste un año. Si funciona, todo lo demás se construye encima.

---

## 6. Arranque concreto — los próximos 60 días (Etapa 0)

Sin esperar a nada. Con lo que YA existe.

1. **Semana 1:** abrir la lista de email (lo único de infraestructura que importa ahora — es
   el activo que poseés). Un "déjame tu correo" simple. **No Discord todavía.**
2. **Semanas 1-8:** publicar **una pieza por semana**, todas 🟢 de bajo esfuerzo, sacadas del
   backlog (`00-content-playbook.md` §4) usando el Brain que ya existe:
   - El video de presentación ([D]: "construyo un synth boutique en Bogotá, solo")
   - 2-3 clips de FX que ya suenan (granular, tape, reverb — los más satisfactorios)
   - 1 pieza del filtro analógico ([A]: "por qué 8 transistores a mano")
   - 1 pieza de ML ([C]: "3 redes neuronales en 73KB")
3. **La única métrica que miro:** ¿publiqué esta semana? ¿una persona nueva interactuó de
   verdad? Nada de obsesionarse con views.
4. **Compuerta a Etapa 1 (día ~60):** ¿sostuve el ritmo? ¿me energizó o me drenó? ¿hubo
   señal real? Con eso decidimos si desplegamos el build-in-public completo del Brain.

---

## 7. Cómo trabajamos juntos (Claude + vos)

- Mientras terminamos el Brain en el repo, te marco **"🎬 momento de contenido"** cuando algo
  valga mostrarse (un FX nuevo suena, el filtro funciona, un modelo mejora). Vos solo grabás.
- Cada milestone técnico del Brain = un episodio potencial. El roadmap de ingeniería y el
  calendario de contenido se vuelven **el mismo calendario**.
- Cuando dudes de open/closed sobre algo nuevo, consultamos §3.3 y lo agregamos a la tabla.
- Antes de cada etapa nueva, revisamos este documento contra los datos reales y lo ajustamos.

---

## Relación con los otros documentos

- **Este doc (01)** = estrategia: rampa, apertura, secuencia, anti-burnout. El "sistema operativo".
- **`00-content-playbook.md`** = backlog táctico: los ~30 momentos documentables concretos.
- **Currículo GrooveMakers v2** = el mapa de temporadas para Etapas 2-3. Norte, no deuda actual.

*Documento vivo. La Etapa 0 es lo único firme; el resto se afina con datos reales.*
