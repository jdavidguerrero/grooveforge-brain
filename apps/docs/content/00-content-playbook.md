# GrooveForge — Content Playbook (Build in Public)

> **Para qué existe este documento:** que nunca te sientes frente a la cámara sin saber
> qué grabar. No es un plan de marketing — es un *backlog de momentos documentables*
> sacados de lo que YA construiste.
>
> **Regla madre:** documentar, no producir. No estás haciendo "contenido de calidad
> de estudio". Estás mostrando lo que ya hacés. El trabajo ya existe — solo lo volvés visible.
>
> Última actualización: 2026-06-05

---

## 0. Por qué esto, antes de los topics

Tres problemas tuyos que este sistema ataca de frente:

1. **Feedback loop largo → ansiedad.** Cada pieza que publicás te trae señal en días,
   no en 18 meses. Rompe el silencio que genera la ansiedad.
2. **Conseguir beta testers es agotador.** El contenido los atrae solos. La persona que
   comenta "quiero probarlo" es tu tester (sirve para GroovePilot *y* para Brain).
3. **Falta de foco entre proyectos.** Contar una historia coherente te *obliga* a elegir.

**No mido el éxito en views.** Lo mido en: ¿publiqué algo esta semana? ¿una persona nueva
interactuó? Esos son los únicos KPIs que importan los primeros 6 meses.

---

## 1. Reglas del sistema (leer antes de cada sesión)

- **Costo bajo por pieza.** Si un video te toma >30 min, lo estás sobre-produciendo. Pará.
- **Crudo = auténtico.** En build-in-public la imperfección vende. Nadie quiere un comercial.
- **Un hero feature, no una lista.** No vendemos "7 features de AI". Vendemos UNA sensación.
  (Ver §3 — Hero features.)
- **Captura ahora, edita después.** Prendé la cámara/screen-record cuando pase algo, aunque
  no lo publiques ese día. El material crudo es oro; se edita en frío.
- **Nunca muestres lo que no existe.** No KiCad vacío, no "prototipo ensamblado" (aún no hay),
  no cloud AI. Solo lo real. La credibilidad es el activo #1.

---

## 2. Pilares de contenido (los 5 carriles)

Todo topic cae en uno de estos. Rotá entre ellos para no aburrir.

| # | Pilar | Qué es | Audiencia que atrae |
|---|---|---|---|
| **A** | **Hardware en vivo** | El filtro de transistores, matching, enclosure, guadua | Synth nerds, makers, DIY |
| **B** | **DSP explicado** | Cómo funciona un synth/efecto por dentro | Devs de audio, estudiantes, curiosos |
| **C** | **AI en hardware diminuto** | 3 modelos ML en 73KB sobre un Teensy | ML/embedded crowd, prensa tech |
| **D** | **Historia & filosofía** | Boutique, Colombia, solo founder, por qué $599 | Compradores potenciales, comunidad |
| **E** | **Música + Tech (crossover)** | Tu marca personal, GroovePilot, demos musicales | Productores, músicos |

---

## 3. Hero features (el mensaje que elegimos)

No marketees el conteo. Elegí 1-2 sensaciones y todo lo demás es "bonus que descubrís".

**Candidato hero #1 — Scale Lock:** *"Nunca toques una nota mal."*
El synth detecta la tonalidad de lo que tocás y ajusta las notas a la escala. Es visual,
inmediato, y cualquiera lo entiende en 10 segundos de video.

**Candidato hero #2 — Smart Arp:** *"Sostené un acorde, sale una melodía."*
Demo perfecto: una mano quieta, melodía saliendo sola. Muy "wow" en clip corto.

**El moat real (pilar D, no hero de feature):** el **filtro analógico de transistores discretos
a precio maker**. Es lo único no-copiable por software. Es la historia que te diferencia.

> Decisión pendiente para vos: ¿con cuál hero arrancás? No hace falta resolverlo hoy,
> pero el primer "video bandera" debería ser uno de los dos.

---

## 4. EL BACKLOG — momentos documentables (lo que ya existe)

Cada item: **[Pilar] Título · qué mostrar · formato · esfuerzo (🟢 bajo / 🟡 medio / 🔴 alto)**

### 4.1 Filtro analógico & hardware (Pilar A) — tu material más fuerte

- **[A]** *"¿Por qué un synth de $599 lleva 8 transistores a mano?"* · explicás el ladder
  2N3904 vs un chip · talking head + foto del esquemático · 🟢
- **[A]** *Matching de transistores en vivo* · el jig midiendo Vbe, emparejando pares ·
  screen + manos, time-lapse · 🟡 *(cuando construyas el jig)*
- **[A]** *"El filtro Moog explicado en 4 escalones"* · animación simple del ladder de 4 polos,
  24 dB/oct · pizarra/dibujo + audio del sweep · 🟡
- **[A]** *Guadua + PETG + aluminio: anatomía de un enclosure boutique* · las 3 piezas,
  por qué cada material, el costo real ($5 + $6 + $2.50) · fotos del 3D + render · 🟢
- **[A]** *"Por qué guadua y no madera importada"* (la historia colombiana) · pilar D también ·
  talking head + material físico · 🟢
- **[A]** *Render → impresión: el body en PETG* · el .f3d girando, slicing, print · 🟡

### 4.2 Synth engines (Pilar B) — 6 piezas listas, una por engine

Cada engine es un video. Patrón: *qué synth legendario emula · cómo suena · una decisión
técnica interesante · demo audible.* Esfuerzo 🟡 cada uno (el audio ya existe en sketches).

- **[B]** *Minimoog Model D* — 3 VCO, mono, el filtro ladder que define el sonido
- **[B]** *Juno-106* — 6 voces poly, el chorus BBD que lo hace famoso, voice stealing FIFO
- **[B]** *Prophet-5* — 5 voces, 2 VCO con detune, el "ancho" del unison
- **[B]** *Oberheim OB-6* — dual saw, envelope de filtro dedicado, modo unison
- **[B]** *ARP 2600* — semi-modular, routing arbitrario, memoria de patch
- **[B]** *Yamaha DX7* — síntesis FM, 4 operadores, por qué FM suena "digital/metálico"
- **[B]** *"6 synths legendarios en una caja: ¿hace trampa el software?"* · el debate
  analog-vs-digital, dónde entra el filtro real · talking head · 🟢

### 4.3 Efectos (Pilar B) — 9 piezas, las más "satisfactorias" en audio

Los FX dan los mejores clips de "antes/después". Patrón: *seco → con efecto.*

- **[B]** *Granular Cloud* — estirar el tiempo con 8 granos, demo dramático · 🟡
- **[B]** *Cymatic Resonator* — síntesis modal, 8 modos resonando · 🟡
- **[B]** *Tape Saturate* — tanh + wow/flutter, el "calorcito" de la cinta · 🟡
- **[B]** *Modal Reverb* — Schroeder + Freeverb, cómo se construye un reverb · 🟡
- **[B]** *Bit Sculpt* — bitcrusher, lo-fi a propósito · 🟢
- **[B]** *Spring Plate, Phase Chorus, Ghost Echo, Sub Genesis* — uno por clip corto · 🟢
- **[B]** *"Cómo funciona un reverb por dentro"* (educativo profundo) · 🔴 pero alto valor

### 4.4 TinyML / AI (Pilar C) — gancho fuerte para prensa tech

- **[C]** *"3 redes neuronales en 73 KB sobre un microcontrolador"* · el número impacta ·
  talking head + gráfico de tamaños (Key 19KB, Chord 23KB, Beat 30KB) · 🟢
- **[C]** *Key Detector: 94% de acierto detectando tonalidad* · cómo, el histograma de 12 bins,
  int8 quantization · screen + demo · 🟡
- **[C]** *Beat Follower: el synth siente tu tempo* · 99.7% dentro de ±1 BPM · demo en vivo · 🟡
- **[C]** *"Por qué NO uso la nube para esto"* (privacidad, latencia <20ms, offline) · pilar D · 🟢
- **[C]** *Quantización float32 → int8 explicada simple* · educativo · 🟡

### 4.5 Historia, filosofía, negocio (Pilar D) — construye la comunidad que compra

- **[D]** *"Construyo un synth boutique en Bogotá, solo, en mis ratos libres"* — el video de
  presentación del canal · talking head honesto · 🟢
- **[D]** *Por qué $599 y no $1500* — desglose honesto de costo vs valor · 🟢
- **[D]** *La ansiedad de emprender solo* (vulnerable, real) — conecta profundo, muy compartible · 🟢
- **[D]** *Build-in-public: te muestro TODO, incluso lo que falla* — el contrato con tu audiencia · 🟢
- **[D]** *Música + tecnología: por qué uno mis dos mundos* — tu "por qué" personal · 🟢

### 4.6 Crossover / GroovePilot / música (Pilar E)

- **[E]** *Demo musical: un track hecho 100% con el Brain* (cuando haya audio estable) · 🔴
- **[E]** *GroovePilot + Brain: la visión completa de mi ecosistema* · 🟡
- **[E]** *Reaccionando a synths boutique que admiro* (Bastl, Folktek, OP-1) · 🟢
- **[E]** *"Buscando beta testers"* — call directo a la comunidad (resuelve tu problema real) · 🟢

---

## 5. Calendario sugerido (sostenible, no heroico)

**No prometas frecuencia alta.** Empezá con lo que aguantás *sin culpa*.

| Fase | Cadencia | Meta |
|---|---|---|
| Mes 1-2 (arranque) | 1 pieza/semana | construir el hábito, no la audiencia |
| Mes 3-4 | 1-2/semana | encontrar qué pilar resuena |
| Mes 5+ | según señal | doblar en lo que funciona |

**Mezcla por semana:** alterná esfuerzo. Una semana 🟢 (talking head fácil), la siguiente
🟡 (demo de engine). Nunca dos 🔴 seguidas.

**Mínimo viable de una pieza:**
1. Un clip o screenshot de algo real
2. Un texto honesto: *"Día X. Hoy logré / fallé en ___. Así suena."*
3. Publicar. Sin pulir de más.

---

## 6. Plataformas (dónde, en orden de prioridad)

| Plataforma | Para qué | Esfuerzo |
|---|---|---|
| **Instagram Reels / TikTok** | clips cortos de audio (FX, demos), alcance rápido | 🟢 |
| **YouTube** | piezas largas (engines, DSP educativo), búsqueda evergreen | 🟡-🔴 |
| **X / Reddit (r/synthesizers, r/synthdiy)** | build-in-public, conseguir testers | 🟢 |
| **lines.community / Muff Wiggler** | comunidad boutique seria, primeros compradores | 🟢 |
| **Newsletter (lista de emails)** | tu activo propio — captura interesados para el lanzamiento | 🟢 |

> El más importante a largo plazo es la **newsletter/lista**: es la audiencia que *poseés*
> y la que compra cuando lances. Poné un "déjame tu email" desde el primer video.

---

## 7. Cómo usamos esto juntos (Claude + vos)

A medida que construyamos cosas en el repo, yo te marco **"🎬 momento de contenido"**
cuando pase algo que valga mostrar (un FX nuevo suena, el filtro funciona, un modelo mejora).
Vos solo tenés que prender la cámara/grabar pantalla en ese momento.

Cuando completemos un sprint, podemos cerrar con: *"¿qué de esto es documentable?"* y lo
agregamos a §4.

**Backlog vivo:** este doc se actualiza. Cuando publiques algo, marcalo. Cuando se te ocurra
un topic, agregalo abajo.

---

## 8. Ideas sueltas / parking (agregar libremente)

- _(vacío — anotá acá cualquier idea que se te ocurra para no perderla)_

---

## 9. Lo que NO mostrar todavía (para no perder credibilidad)

- ❌ Esquemáticos KiCad (carpeta vacía)
- ❌ "Prototipo ensamblado" (no existe aún físicamente)
- ❌ Filtro en protoboard (diseñado, no construido)
- ❌ Features de cloud AI (planeadas, sin código)
- ❌ Plugin VST3 (no empezado)

Cuando alguno de estos pase a real → se mueve al backlog §4 y se convierte en *gran* contenido
("por fin armé el filtro que llevo meses diseñando").
