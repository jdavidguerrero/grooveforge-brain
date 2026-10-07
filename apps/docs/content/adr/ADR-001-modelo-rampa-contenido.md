# ADR-001: Modelo de rampa progresiva, secuencia Brain-first y política de apertura para contenido/negocio

**Status:** Accepted
**Date:** 2026-06-08
**Deciders:** Juan Guerrero (solo founder)
**Supersedes:** Currículo GrooveMakers v2 (build-led) como plan ejecutable inmediato — se conserva como mapa de Etapas 2-3, no como deuda actual.

---

## Context

GrooveForge es un proyecto de un solo founder, a tiempo parcial (10-18h/sem), con el Brain
~90% construido (6 engines, 9 FX, 3 modelos TinyML, bridge protocol — todo real y funcional)
y un e-drum parcialmente implementado.

Fuerzas en juego, declaradas por el founder en conversación:

- **Meta progresiva, no binaria:** empezar con ingreso adicional y escalar hasta poder
  dedicarse full-time (~$10k/mes como destino, no como punto de partida).
- **Restricción emocional crítica:** ansiedad por exceso de frentes simultáneos; el founder
  identifica explícitamente que su problema es **foco** y que su **constancia se rompe por
  falta de feedback corto** (intentos previos de contenido/comunidad murieron por silencio,
  no por falta de capacidad).
- **Tiempo limitado y prioridad #2** (GroovePilot es #1).
- **Activo desaprovechado:** el Brain, lo más fuerte y ya construido, estaba planeado para
  mostrarse recién en ~12 meses (Temporada 2 del currículo).
- **Preocupación nueva:** cuánto mostrar del Brain sin desvelar lo propietario (abierto vs
  cerrado), generando fatiga de decisión frente a la cámara.

El currículo GrooveMakers v2 existente es estratégicamente sólido (build-led, escalera de
confianza, fundamentos just-in-time) pero diseñado para una meta de $10k/mes desde el inicio,
liderando con un build nuevo (e-drum) y comprometiendo el ecosistema/protocolo desde el día 1.
Eso **contradice la restricción de foco y la realidad emocional** del founder.

---

## Decision

Adoptar un **modelo operativo de rampa** (documentado en `01-modelo-rampa-y-apertura.md`) con
cuatro componentes:

1. **Contenido = producto.** El contenido build-in-public es la misma actividad que construir,
   funcionando como (a) forcing-function/deadline para terminar el Brain, (b) feedback loop
   corto que cura la ansiedad, (c) el moat real (audiencia que confía).
2. **Rampa con compuertas.** 4 etapas (0 Validación → 1 Ingreso extra → 2 Ingreso real →
   3 Dedicación). Cada etapa se **gana con datos**; ninguna se pre-compromete. La Etapa 3
   (ecosistema/$10k) es destino condicional, no deuda presente.
3. **Secuencia Brain-first.** El Brain (ya construido) es el release principal de las Etapas
   0-1. El e-drum entra en Etapa 2. El ecosistema en Etapa 3.
4. **Política de apertura por subsistema, decidida una vez.** Niveles 🟢 abierto / 🟡
   superficie / 🔴 cerrado, en tabla maestra. Default = abierto. Elimina la fatiga de decisión.

---

## Options Considered

### Option A: GrooveMakers v2 tal cual (build-led, e-drum primero, ecosistema desde el diseño)

| Dimensión | Evaluación |
|---|---|
| Complejidad | Alta (producto nuevo + ecosistema + comunidad + SaaS en paralelo) |
| Costo (esfuerzo) | Alto — e-drum como release principal = I+D filmada desde casi cero |
| Escalabilidad | Excelente a largo plazo (network effects, razor-and-blades) |
| Ajuste a la restricción (foco/ansiedad) | **Malo** — agrega frentes; amplifica el problema raíz |
| Time-to-feedback | Lento — el activo construido (Brain) se posterga ~12 meses |

**Pros:** estrategia de plataforma de clase mundial; escalera de confianza bien diseñada;
e-drum más acotado pedagógicamente.
**Cons:** entierra el activo más fuerte; pre-compromete el ecosistema; meta $10k/mes desde el
inicio choca con el estado emocional; sin válvula de escape ni kill-switch; depende de la
constancia sin resolver la causa (feedback).

### Option B: Modelo de rampa, Brain-first, contenido como forcing-function, apertura por niveles (ELEGIDA)

| Dimensión | Evaluación |
|---|---|
| Complejidad | Baja al inicio, creciente por etapas |
| Costo (esfuerzo) | Bajo en Etapa 0 (documentar lo que ya existe) |
| Escalabilidad | Igual destino que Option A, alcanzado por compuertas |
| Ajuste a la restricción (foco/ansiedad) | **Alto** — un solo frente activo; feedback corto |
| Time-to-feedback | Rápido — cosecha el Brain ya construido desde la semana 1 |

**Pros:** fusiona construir+contenido (mata el problema de dos frentes); usa el activo
existente; feedback corto desde el día 1; gates con permiso de parar; política de apertura
elimina fatiga de decisión; conserva GrooveMakers como mapa futuro.
**Cons:** progresión más lenta hacia $10k; requiere disciplina de no saltar etapas; el
e-drum y el ecosistema se difieren.

### Option C: Producto boutique sin contenido (o currículo v1 theory-first)

| Dimensión | Evaluación |
|---|---|
| Complejidad | Baja |
| Costo (esfuerzo) | Bajo |
| Escalabilidad | **Pobre** — sin audiencia, sin moat, sin canal de venta |
| Ajuste a la restricción | Neutro pero no resuelve la falta de feedback |
| Time-to-feedback | Malo — vuelve al silencio que causó la ansiedad |

**Pros:** mínimo esfuerzo; sin carga de comunidad.
**Cons:** no construye el moat; vender boutique sin audiencia es muy difícil; perpetúa el
ciclo de silencio→ansiedad; v1 theory-first compite de frente con Moritz Klein y esconde el
diferenciador.

---

## Trade-off Analysis

El eje decisivo **no es la calidad estratégica** (Option A gana ahí) sino el **ajuste a la
restricción real del founder**: foco limitado, tiempo parcial y constancia dependiente de
feedback corto. Option A optimiza el destino sacrificando la viabilidad del trayecto para
*este* operador en *este* estado. Option B llega al mismo destino preservando la viabilidad,
a costa de velocidad.

La clave que inclina la balanza: **el riesgo dominante del proyecto no es competitivo (que lo
copien) ni estratégico (mal plan) — es de ejecución/burnout** (que el founder se queme o
abandone antes de validar). Option B es la única que ataca ese riesgo directamente, al
(a) reducir frentes a uno, (b) acortar el feedback, y (c) diseñar válvulas de escape.

Sobre apertura: el secretismo no defiende a un solo founder de un clonador (topologías como
el ladder 2N3904 y el modelo D'Angelo son públicas), y sí mata el motor de crecimiento
(build-in-public). Por eso el default es abierto, protegiendo solo el núcleo mecánicamente
replicable (pesos ML, backend GroovePilot, archivos de producción, datos de negocio).

---

## Consequences

**Se vuelve más fácil:**
- Empezar ya, con fricción casi nula, usando el Brain existente.
- Sostener constancia (feedback corto + forcing-function + unidad mínima sostenible).
- Decidir qué mostrar (tabla de apertura resuelta una vez; default abierto).
- Parar o desacelerar sin culpa (gates explícitos por etapa).

**Se vuelve más difícil / se sacrifica:**
- Velocidad hacia $10k/mes (la rampa es deliberadamente gradual).
- Gratificación del e-drum y el ecosistema (diferidos a Etapas 2-3).
- Requiere disciplina para no re-expandir el scope antes de tiempo.

**A revisitar:**
- Compuerta Etapa 0→1 (~día 60): ¿hábito sostenido + señal real? Decide si se despliega el
  build-in-public completo del Brain.
- Compuerta Etapa 1→2: ¿primeras ventas + lista creciente + energía? Decide si entra el e-drum.
- Política de apertura: revisar la tabla cuando aparezca un subsistema nuevo (default abierto).
- Reconciliar con GrooveMakers v2 al llegar a Etapa 2 (pasa de "mapa" a "plan ejecutable").

---

## Action Items

1. [ ] Abrir lista de email (única infra de Etapa 0; sin Discord aún).
2. [ ] Publicar pieza de presentación ([D]) — Brain boutique, Bogotá, solo founder.
3. [ ] Publicar 4-6 piezas 🟢 de bajo esfuerzo del Brain existente (FX, filtro, ML) en 8 semanas.
4. [ ] Usar la tabla de apertura (`01` §3.3) como checklist pre-grabación; default abierto.
5. [ ] Definir el milestone del Brain a cerrar primero, para que sea el primer "🎬 momento de contenido".
6. [ ] Revisar compuerta Etapa 0→1 (~día 60) con datos reales: ¿continuar al build-in-public completo?

---

## Referencias

- `apps/docs/content/01-modelo-rampa-y-apertura.md` — estrategia (sistema operativo)
- `apps/docs/content/00-content-playbook.md` — backlog táctico de topics
- Currículo GrooveMakers v2 (build-led) — mapa de Etapas 2-3
- `apps/docs/00-master-strategy.md` — North Star, pricing, identidad
