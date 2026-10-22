# ADR-002: Comunidad primero, un device a la vez, sin carga monetaria

**Status:** Accepted
**Date:** 2026-10-22
**Deciders:** Juan Guerrero
**Supersedes (parcial):** ADR-001, en la meta de ingresos (etapas 1-3 de la rampa) y en la secuencia de productos.
Se mantienen de ADR-001: el contenido como forcing-function, el sistema anti-burnout y la regla "nunca mostrar lo que
no existe".
**Afecta:** `01-modelo-rampa-y-apertura.md` §2 y §3, `CLAUDE.md` (Filosofía, punto 6). No modifica `00-06`.

---

## Context

- GrooveForge nació como venta de equipos (Brain a $599, preventas, `00-master-strategy.md` §3), y ADR-001 lo
  ordenó como una rampa hacia vivir de esto (~$10k/mes).
- Juan tiene un trabajo estable que le da tiempo y estabilidad económica. **No necesita que el proyecto genere
  ingresos ahora** y no quiere ponerle esa carga: lo hace porque le gusta.
- Hay frentes abiertos a la vez: Brain Studio (prototipo funcional en protoboard), GroovePush (desmontado, PCBs
  diseñadas), GroovePilot (SaaS de mezcla con IA) y una lista larga de experimentos de hardware. La dispersión ya
  estaba identificada como el riesgo principal en ADR-001.
- En la música hay un rechazo fuerte a la IA que reemplaza al músico. Juan quiere mostrar que la IA puede ser una
  herramienta que no mata el proceso creativo, pero hoy no tiene la credibilidad musical en el canal para sostener
  ese mensaje.
- GroovePilot todavía no tiene una forma clara de venderse.

## Decision

1. **Comunidad y contenido primero; no se monetiza todavía.** Ni preventas, ni talleres pagos, ni suscripciones de
   GroovePilot como objetivo de esta etapa. La opción de vender queda abierta, no comprometida.
2. **Un device a la vez, terminado de verdad:**
   1. **GrooveForge Brain · Studio**: terminado = en su **enclosure**, sin protoboard, **funcional para la música de
      Juan** (producir y tocar en vivo).
   2. **GroovePush**, después de cerrar el Brain.
   3. El resto (e-drum, experimentos) después, como formato "Laboratorio" o temporadas propias.
3. **GroovePilot queda en segundo plano.** No es prioridad del contenido ni del desarrollo; entra de a pocos cuando
   aporte al proceso musical que se está mostrando y cuando haya una forma clara de presentarlo.
4. **La narrativa de la Temporada 1: "construyo mi synth para encontrar mi sonido".** Primero en mis producciones
   (encontrar y usar mi sonido en tracks), después en el live set. El live set es el cierre, no la promesa inicial.
5. **IA: después de la credibilidad, y siempre como herramienta.** La Temporada 1 no se apoya en IA. Cuando la IA
   entre: el músico decide, la IA escucha o sugiere; se dice siempre qué es ML y qué no (como ya hace el código de
   la Fase 6); el audio no sale de la máquina. No se muestra música generada por IA como si fuera el producto.
6. **Una regla para todos los experimentos: terminan sonando en música real.**
7. **Métricas sin dinero:** publicar al ritmo elegido (orientativo: un episodio largo cada 2-3 semanas y 2 Shorts por
   semana); conversación real en los comentarios; personas que construyen algo a partir del contenido. La lista de
   correo se abre igual: no es monetizar, es la audiencia propia.

## Options Considered

### A: Mantener ADR-001 (rampa hacia la venta)
- ✅ Camino claro a ingresos.
- ❌ Pone una carga económica que Juan no necesita ni quiere; empuja a vender antes de tener comunidad; el contenido
  se lee como marketing.

### B: Comunidad primero, varios devices en paralelo
- ✅ Mucho material para contenido.
- ❌ Repite el problema de foco de ADR-001; ningún device se termina; el canal no tiene identidad.

### C: Comunidad primero, un device a la vez, sin monetizar (ELEGIDA)
- ✅ Foco, un objetivo terminable (Brain en caja y en uso), contenido auténtico, la opción de vender queda abierta.
- ❌ Sin ingreso ni compuertas económicas: hace falta disciplina propia (métricas del punto 7) para no abandonar en
  silencio.

## Consequences

- **Más fácil:** decidir qué hacer cada semana (lo que acerca al Brain terminado), contar la historia del canal,
  mostrar fracasos y experimentos sin cuidar una venta.
- **Más difícil:** no hay presión externa; la constancia depende del ritmo de publicación elegido.
- **A revisar:** la política de apertura de `01-modelo-rampa-y-apertura.md` §3.3 se diseñó para vender. Sin venta,
  abrir más código (por ejemplo, el del Studio) puede sumar comunidad; decidirlo por partes, porque abrir es casi
  irreversible. En cualquier caso, los nombres de los synths originales son marcas: "inspirado en el Juno-106", no
  "mi Juno-106" como nombre de producto.
- `CLAUDE.md` (Filosofía, punto 6) pasa a: Brain primero, GroovePush después, GroovePilot en segundo plano.

## Action Items

1. [x] Temporada 1 con la narrativa del punto 4: `temporada-1/BIBLIA_T1_LIVESET.md`, `temporada-1/EP01_guion.md`.
2. [x] `CLAUDE.md`, punto 6 de la Filosofía.
3. [ ] Revisar la política de apertura (§3.3) con este ADR en mente.
4. [ ] Abrir la lista de correo antes de publicar el EP1.
5. [ ] Definir "Brain terminado" como lista verificable (enclosure, PCBs, modo FX, presets del set) en
   `06-implementation-roadmap.md` §10B, con autorización.

## Referencias

- `apps/docs/content/adr/ADR-001-modelo-rampa-contenido.md`
- `apps/docs/content/01-modelo-rampa-y-apertura.md`
- `apps/docs/adr/ADR-002-variante-studio.md` (Studio como instrumento propio)
- `apps/hardware/kicad/studio/docs/auditoria-fabricacion-2026-10-22.md` (lo que falta para la caja)
