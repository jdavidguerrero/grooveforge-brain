# ADR-003: Filtro analógico 2N3904 como add-on externo (Brain v1 con filtro digital)

**Status:** Accepted (2026-10-07)
**Date:** 2026-10-07
**Deciders:** Juan Guerrero (owner)
**Afecta specs:** `00-master-strategy.md` (posicionamiento/precio), `01-architecture.md` §2 (audio
path), `03-filter-design.md`, `CLAUDE.md` (audio path). Sincronizados el 2026-10-07 con autorización
del owner.

---

## Context

- El filtro analógico es el **riesgo #1 del proyecto (P0-2, P0-3)**: conversor exponencial y control
  de resonancia sin definir, alimentación sin resolver, sin prototipo.
- Alimentación: un Moog ladder apila ~5–6.5 V (fuente de corriente + par de entrada + 4 etapas +
  carga). Con 5V de una sola fuente queda al límite; necesita ~±5V (charge pump) o +9V (boost).
  El owner descarta ±12V en el enclosure final y no quiere sumar la complejidad de ±5V ahora.
- El ladder es **mono** y filtra la mezcla: en los engines polifónicos actúa como filtro global; el
  trabajo por voz ya lo hace el filtro digital (`MoogLadder4P`, D'Angelo & Välimäki) en los 6 engines.
- El lado digital del Brain está casi completo en protoboard. Lo que falta para terminar Studio v1
  es la UI de Studio, el PCB y el enclosure.
- Tiempo real disponible: ~1–2 h/semana de Bloque B. El filtro genera fricción y frena todo lo demás.
- GrooveMakers ya planea un Ep 4 que monta un Moog ladder a ±12V con 2N3904 emparejados.

## Decision (propuesta)

1. **Brain v1 (empezando por Studio) sale con filtro digital** (`MoogLadder4P`) en todos los engines.
2. **El ladder 2N3904 pasa a ser un add-on externo** (nombre de trabajo: *GrooveForge Ladder*), estilo
   módulo: caja propia, alimentación propia (puede ser ±12V, porque queda fuera del enclosure del
   Brain), entrada y salida de audio, entrada de CV y perilla de resonancia.
3. **El Brain incluye desde el primer PCB la interfaz del add-on**, para no rediseñar después:
   - 1 salida de **CV de cutoff** (PWM del Teensy filtrado + buffer, 0–5V, jack 3.5 mm). El Teensy
     calcula ahí todo: ENC L, envelope, LFO y modulación por ML.
   - Opcional: 2ª salida de CV (resonancia) si el costo es marginal.
   - Audio en línea en v1: salida del Brain → add-on → mezcla. El retorno al Brain queda para después.
4. **Ajuste de D1** (rol del filtro por engine): la política por engine aplica cuando el add-on está
   conectado (Model D y DX7 con el digital apagado; polys con digital por voz + add-on maestro). Sin
   add-on, el filtro digital queda siempre activo.

## Options Considered

| Opción | Complejidad | Riesgo | Tiempo a Studio v1 | Posicionamiento comercial |
|---|---|---|---|---|
| A. Integrado con ±5V (charge pump) | alta: conversor exp, resonancia, alimentación, calibración por unidad | alto (P0) | largo | intacto ($599 con filtro analógico) |
| **B. Add-on externo** | baja en el Brain (solo la interfaz CV); el ladder se resuelve aparte | bajo para el Brain | **corto** | cambia: el analógico se vende o se suma aparte |
| C. Solo digital, sin add-on | mínima | nulo | el más corto | se pierde el diferenciador analógico |

## Trade-off Analysis

B saca el riesgo existencial del camino crítico sin cerrar la puerta: la interfaz CV en el PCB cuesta
unos pocos componentes, y el ladder se puede desarrollar a su ritmo (incluido el Ep 4, a ±12V, sin
conflicto con el enclosure). Frente a C, conserva la historia analógica y abre el formato modular.
Frente a A, renuncia a "filtro analógico integrado", que es hoy el argumento central del precio en
`00`. Para Studio (uso personal) ese argumento no aplica; para el comercial queda **abierto**.

## Consequences

**Más fácil:**
- PCB y enclosure del Brain pueden arrancar ya: el ladder deja de bloquearlos.
- Se liberan el ADC del SGTL5000 y el path de retorno: el modo FX processor (entrada externa) queda
  más simple y la decisión D5 se reduce.
- Sale del Brain el matching y la calibración por unidad.
- Se cierran para el Brain P0-2, P0-3 y la contradicción X2 (TPDT).

**Más difícil / se pierde:**
- El audio USB del Brain sale **antes** del add-on.
- Las features que leían la señal post-filtro (envelope follower, Sprint 5B.3) y la modulación del
  filtro analógico por ML (`03` §6.1) pasan a hacerse por CV o quedan para el add-on.
- **El filtro digital pasa a ser el filtro del producto**, así que su calidad importa más: verificar el
  coeficiente `g` contra el paper (posible error de 2× en la calibración del cutoff) y evaluar
  oversampling para la auto-oscilación (auditoría P3).

**A revisitar:**
- Posicionamiento y precio del producto comercial en `00` (no se resuelve acá).
- Escala del CV (1V/oct estándar modular, rango de 5 octavas con perilla de frecuencia base en el
  módulo) y PWM filtrado contra un DAC externo: se define en un theory doc antes del PCB.
- Formato del add-on (caja de escritorio o Eurorack). Si es Eurorack, la salida de CV del Brain
  también puede manejar otros módulos.

## Action Items

1. [x] Owner confirma este ADR (Proposed → Accepted).
2. [ ] Theory doc de la interfaz del add-on (CV out: rango, escala, PWM vs DAC, jack) antes del PCB.
3. [ ] Calidad del filtro digital: coeficiente `g` y oversampling de `MoogLadder4P`.
4. [ ] Reordenar el backlog: matching y diseño del ladder salen del camino crítico (hito Add-on Ladder).
5. [x] Con autorización: actualizar `00`, `01` §2, `03` y `CLAUDE.md` en la sincronización de docs (R-D1).
6. [ ] El Ep 4 de GrooveMakers funciona como prototipo del add-on.
