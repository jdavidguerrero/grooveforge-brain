# GrooveForge — Track de Contenido: DSP & AI para Música en el Edge

> **Tipo de documento:** brief estructurado, diseñado para alimentar un pipeline externo de
> creación de contenido. Cada tópico trae campos consistentes (hook, demo, brand tie-in,
> esfuerzo, nivel de apertura) para que el pipeline downstream pueda expandirlo a guion.
>
> **Relación con otros docs:** ejecuta la estrategia de `01-modelo-rampa-y-apertura.md` y
> `ADR-001`; comparte backlog con `00-content-playbook.md`. Esto NO reemplaza esos — los
> especializa para el track DSP/ML/AI sobre plataformas edge.
>
> Última actualización: 2026-06-09

---

## 1. Cómo usar este documento (para el pipeline)

- La **§2-§5** son contexto fijo: posicionamiento, marca, audiencia, plataforma. El pipeline
  las usa como *system context* para mantener tono y consistencia.
- La **§6** es el **backlog de tópicos**. Cada entrada es una unidad consumible. Schema por
  tópico en §6.0.
- La **§7-§9** son restricciones: tono, política de apertura (qué mostrar/proteger), y
  cadencia anti-burnout. El pipeline debe respetarlas siempre.
- Default ante ambigüedad: ver §8 (regla = abierto, salvo el núcleo propietario listado).

---

## 2. Posicionamiento (el norte de todo el track)

**One-liner:** *DSP, Machine Learning y AI para música, corriendo en hardware real de bajo
costo — del microcontrolador al edge.*

**Qué NO es esto (límites explícitos):**
- ❌ No es electrónica básica (resistencias, qué es un capacitor, teoría de filtros 101). Esa
  cancha ya está tomada (Moritz Klein) y esconde el diferenciador.
- ❌ No es reseñas de gear ni noticias.
- ❌ No es tutoriales genéricos de Python/ML desconectados de audio.

**Qué SÍ es (el nicho defendible):**
- ✅ DSP **aplicado**: escribir el algoritmo, correrlo en hardware, oírlo, medirlo.
- ✅ ML/AI para música **en el dispositivo** (on-device / edge), no en la nube de otro.
- ✅ La intersección rara: **DSP + ML + música + embedded**. Casi nadie la cubre con
  profundidad real Y resultados audibles.

**Por qué es defendible:** requiere cuatro competencias a la vez (DSP, ML, música, embedded).
La mayoría de creadores tiene una o dos. Esa combinación ES el moat — y es exactamente lo que
hace creíbles a tus productos.

---

## 3. Conexión con la marca (cada pieza vende sin vender)

El track es la **demostración viva** de la identidad de tus productos. No se "menciona" el
producto — se *prueba* la competencia que lo respalda.

| Producto | Qué demuestra el contenido | Cómo aparece |
|---|---|---|
| **GroovePilot — Copilot AI de mezcla** | que sabés de AI aplicada a audio/mezcla de verdad | piezas de ML para análisis de audio, "cómo piensa un copilot de mezcla", demos de GroovePilot en uso |
| **GrooveForge Instruments — powered by AI** | que "powered by AI" no es marketing: hay ML real on-device | TinyML en el Brain, detección key/chord/beat, AI features audibles |
| **El ecosistema (futuro)** | visión técnica seria | arquitectura híbrida, DSP portable Teensy↔Pi |

**Regla de marca:** el producto es la *prueba*, no el *pitch*. Mostrás la competencia; la
venta es consecuencia. Cada pieza debe poder responder: *"¿qué competencia de mi producto
demuestra esto?"*

---

## 4. Audiencia

- **Primaria:** productores/músicos técnicos, makers de audio, devs de DSP/embedded curiosos
  por audio, gente de ML interesada en aplicaciones reales fuera del datacenter.
- **Secundaria (SEO/tope de funnel):** estudiantes de DSP/ML, hobbyistas de Raspberry Pi/Teensy.
- **Lo que NO buscamos:** alcance masivo de "synth memes". Profundidad > alcance (vende productos).

---

## 5. Stack de plataformas (lo confirmado disponible)

| Capa | Hardware/Tool | Estado | Rol en el contenido |
|---|---|---|---|
| **Microcontrolador** | Teensy 4.1 (Cortex-M7) | ✅ en uso (el Brain) | DSP/ML determinístico, sub-1ms, "tiny" |
| **Edge SBC** | Raspberry Pi 5 8GB | ✅ disponible | DSP/ML más pesado, Linux, el "cerebro grande" |
| **Audio I/O** | RaspiAudio v2 (codec I2S) | ✅ disponible | in/out para experimentos; upgrade a HiFiBerry DAC+ADC Pro solo si hay muro |
| **Acelerador AI** | Hailo-8/8L (AI HAT+) | ⏳ opcional, no comprado | inferencia AI acelerada; comprar solo si la exploración valida (ver `01` gates) |
| **Lenguaje DSP** | Faust | a adoptar | escribir DSP una vez → correr en Pi, Teensy y como plugin. Cross-platform = continuidad |
| **Runtime audio Linux** | PREEMPT_RT + PipeWire/JACK | a configurar | baja latencia en Pi5 (~pocos ms round-trip) |
| **ML pipeline** | TensorFlow/TFLite (ya en `apps/training/`) | ✅ en uso | entrenar/quantizar modelos para edge |

**Nota Pi5:** Elk Audio OS (sub-1ms VST host) soporta oficialmente Pi3/4, Pi5 no confirmado →
en Pi5 el camino es Raspberry Pi OS + PREEMPT_RT + PipeWire. RaspiAudio v2 es consumer-grade:
sirve para empezar y para contenido; no es el techo de calidad.

---

## 6. Backlog de tópicos

### 6.0 Schema por tópico (para el pipeline)

```
ID:          identificador estable (DSP-01, ML-03, ...)
Título:      título tentativo (el pipeline puede reescribir)
Pilar:       DSP-aplicado | ML-edge | Producto-showcase | Plataforma-evergreen
Plataforma:  Teensy | Pi5 | ambas | tooling
Hook:        la pregunta/promesa de apertura
Demo:        qué se ve/oye (debe ser audible o visual; nada abstracto)
Brand:       qué competencia de producto demuestra
Esfuerzo:    🟢 bajo | 🟡 medio | 🔴 alto
Apertura:    🟢 abierto | 🟡 superficie | 🔴 reservar (ver §8)
Prereq:      tópicos o setup que deben existir antes
```

### 6.1 Pilar: DSP aplicado

- **DSP-01 — "¿Qué latencia logra una Raspberry Pi como procesador de audio?"**
  · Pi5 · Hook: ¿puede una Pi reemplazar hardware de audio caro? · Demo: medición real de
  round-trip in→out · Brand: rigor técnico (GrooveForge) · 🟢 · 🟢 · Prereq: setup Pi5+RaspiAudio
- **DSP-02 — "Un EQ paramétrico desde la ecuación hasta correrlo en una Pi"**
  · ambas · Hook: DSP no es magia, es una fórmula · Demo: escribir en Faust, compilar, oír el
  barrido · Brand: competencia DSP · 🟡 · 🟢 · Prereq: DSP-01
- **DSP-03 — "Construyendo un compresor: la matemática y el sonido"**
  · Pi5 · Demo: antes/después, ver la curva de ganancia · Brand: DSP de mezcla (GroovePilot) ·
  🟡 · 🟢
- **DSP-04 — "Un reverb desde cero (Schroeder/Freeverb) explicado y deployado"**
  · ambas · Demo: construir las primitivas, oír el espacio aparecer · Brand: FX del Brain ·
  🔴 · 🟡 (reservar valores finales de los FX de producción)
- **DSP-05 — "El mismo DSP en un micro de $30 vs una Pi: ¿cuál gana?"**
  · ambas · Hook: ¿cuándo necesitás un Linux y cuándo un microcontrolador? · Demo: el mismo
  efecto Faust en Teensy y Pi, comparar latencia/calidad · Brand: arquitectura híbrida · 🟡 · 🟢
- **DSP-06 — "Convertí una Raspberry Pi en un channel strip externo"** (el proyecto Apollo-ish)
  · Pi5 · Hook: de interfaz "tonta" a procesador con DSP propio · Demo: tracking monitoreando
  a través del EQ+comp+saturación · Brand: GrooveForge/GroovePilot DSP · 🔴 · 🟡 · Prereq: DSP-02,03

### 6.2 Pilar: ML en el edge (el insignia)

- **ML-01 — "3 redes neuronales en 73 KB sobre un microcontrolador"**
  · Teensy · Hook: el número impacta · Demo: gráfico de tamaños + las features funcionando ·
  Brand: GrooveForge "powered by AI" es real · 🟢 · 🟡 (reservar pesos/dataset) · core brand piece
- **ML-02 — "Detección de pitch en tiempo real con una red neuronal (CREPE) en una Pi"**
  · Pi5 · Demo: cantar/tocar → pitch en vivo · Brand: AI de música on-device · 🟡 · 🟢
- **ML-03 — "Cómo un instrumento GrooveForge 'entiende' la música" (key/chord/beat)**
  · Teensy · Demo: tocar acordes → el instrumento muestra tonalidad/acorde/BPM · Brand: el
  diferenciador central de GrooveForge · 🟡 · 🟡 (concepto sí, pesos no)
- **ML-04 — "Separación de stems en tiempo real en una Pi: ¿se puede?"**
  · Pi5 · Hook: separar voz de una canción en una caja barata · Demo: stem separation en vivo ·
  Brand: AI de audio (GroovePilot) · 🔴 · 🟢
- **ML-05 — "Quantización explicada: encoger un modelo para que quepa en un synth"**
  · Teensy · Hook: cómo va de float32 a int8 sin morir · Demo: accuracy antes/después, tamaño ·
  Brand: ingeniería ML edge · 🟡 · 🟡
- **ML-06 — "Compré un acelerador de AI de $70 (Hailo) para ver si hace música"**
  · Pi5+Hailo · Hook: experimento honesto, fracasos incluidos · Demo: qué corrió, qué no ·
  Brand: exploración seria de AI edge · 🔴 · 🟢 · Prereq: comprar Hailo (gate de `01`)
- **ML-07 — "Efectos de audio neuronales: un reverb/amp aprendido (TCN/RAVE)"**
  · Pi5 · Hook: ¿y si el efecto lo aprende una red? · Demo: timbre transfer / efecto neuronal ·
  Brand: AI generativa de audio · 🔴 · 🟢
- **ML-08 — "AI on-device vs nube para música: privacidad, latencia y por qué elegimos local"**
  · ambas · Hook: por qué tu instrumento no debería depender de internet · Demo: latencia
  comparada · Brand: filosofía de producto · 🟢 · 🟢

### 6.3 Pilar: Producto showcase (donde DSP+ML+productos se encuentran)

- **PR-01 — "Cómo piensa la AI de GroovePilot sobre una mezcla"**
  · tooling · Demo: GroovePilot analizando un track real · Brand: GroovePilot directo · 🟡 · 🔴
  (concepto/uso sí, algoritmos/backend no)
- **PR-02 — "Construyendo un copilot de AI para mezcla: la arquitectura"**
  · tooling · Demo: diagrama + decisiones · Brand: GroovePilot · 🟡 · 🔴
- **PR-03 — "Qué significa de verdad 'powered by AI' en un instrumento"**
  · Teensy · Demo: features AI audibles del Brain · Brand: GrooveForge · 🟢 · 🟡
- **PR-04 — "De la idea al instrumento: arquitectura híbrida (cerebro Linux + motor MCU)"**
  · ambas · Hook: cómo se construye un instrumento AI moderno · Demo: el diagrama del sistema ·
  Brand: visión técnica del ecosistema · 🟢 · 🟢

### 6.4 Pilar: Plataforma evergreen (SEO, entradas accesibles)

- **PL-01 — "Configurando una Raspberry Pi 5 para audio en tiempo real serio"**
  · Pi5 · Demo: PREEMPT_RT + PipeWire paso a paso, latencia final · Brand: credibilidad · 🟡 · 🟢
- **PL-02 — "Faust: el lenguaje de DSP que corre en todos lados (Pi, Teensy, plugin)"**
  · ambas · Demo: un efecto, tres targets · Brand: competencia DSP · 🟡 · 🟢
- **PL-03 — "El panorama de audio HATs: qué comprar para DSP de baja latencia"**
  · Pi5 · Demo: comparación RaspiAudio vs HiFiBerry vs Pisound · Brand: utilidad · 🟢 · 🟢
- **PL-04 — "RaspiAudio v2 review: ¿alcanza para DSP?"**
  · Pi5 · Demo: pruebas reales de latencia/calidad · Brand: honestidad técnica · 🟢 · 🟢

---

## 7. Tono y estilo

- **Honesto e investigativo, no gurú.** "Probemos y veamos" > "así se hace". Los fracasos son
  contenido (build-in-public ethos de `01`).
- **Siempre audible o visual.** Ningún concepto se queda en abstracto: se oye o se ve medido.
- **Profundidad accesible.** Riguroso pero con analogías. Lenguaje natural (ver glosario/agua
  del currículo GrooveMakers cuando aplique), sin perder el rigor.
- **Producto como prueba, no como pitch** (§3).
- **Crudo > sobre-producido.** Coherente con "documentar, no producir".

---

## 8. Política de apertura (referencia condensada — fuente: `01` §3)

**Principio:** el moat no es el secreto, es la competencia + marca + ejecución. Default = abierto.

| Nivel | Significado | Aplica a |
|---|---|---|
| 🟢 abierto | mostrar todo | conceptos DSP, setups, Faust, latencias, filosofía, capacidades |
| 🟡 superficie | mostrar el qué/cómo-suena, reservar el detalle final | engines, FX (valores de producción), arquitectura ML (no pesos) |
| 🔴 reservar | núcleo propietario | **pesos ML entrenados + datasets**, **backend/algoritmos de GroovePilot**, archivos de producción, datos de negocio |

**Regla operativa:** el pipeline nunca publica nivel 🔴. Ante un tópico nuevo sin clasificar,
default 🟢 y se marca para revisión.

---

## 9. Cadencia y anti-burnout (fuente: `01` §5)

- **Unidad mínima sostenible:** 1 clip + 1 texto honesto. Las semanas duras, eso basta. No es fracaso.
- **Mezcla de esfuerzo:** nunca dos 🔴 seguidas. Alternar 🟢/🟡.
- **Empezar por lo que ya existe:** priorizar tópicos sin prereq de compra ni de muro técnico
  (DSP-01, ML-01, ML-03, PR-03, PL-01, PL-02).
- **Gates de `01` aplican:** no comprar Hailo (ML-06) ni upgrade de audio HAT hasta que la
  exploración valide. La meta de la fase actual es sostener el hábito + señal real, no crecer.

---

## 10. Orden de arranque sugerido (primeras 6 piezas, cero compras)

1. **PR-03** — "qué significa powered by AI" (🟢, presenta la tesis del track, usa el Brain)
2. **ML-01** — "3 redes en 73KB" (pieza insignia de marca)
3. **PL-01** — "configurar la Pi5 para audio real" (abre el camino Pi)
4. **DSP-01** — "¿qué latencia logra una Pi?" (primer experimento medible)
5. **PL-02 / DSP-02** — "Faust en todos lados" + primer EQ (la herramienta central)
6. **ML-03** — "cómo el instrumento entiende la música" (diferenciador GrooveForge)

Estas 6 demuestran toda la tesis del track sin gastar un peso y usando lo que ya existe.

---

*Documento vivo. El backlog (§6) crece; las restricciones (§7-§9) son estables. Sincronizar
con `01` y `ADR-001` ante cambios de estrategia.*
