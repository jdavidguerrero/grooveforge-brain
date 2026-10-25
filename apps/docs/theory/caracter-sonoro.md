# Carácter sonoro GrooveForge: un instrumento enfocado en house melódico, orgánico y disco

> **Fecha:** 2026-10-25 (rev. 1 con la revisión crítica del mismo día) · **Estado:** PROPUESTO, espera aprobación de Juan · **Depende de:** `fidelidad-engines.md`
> §12 (estado de los engines), `05-fx-architecture.md` (FX existentes), theory/04–07 de Studio (panel, SYNTH FX,
> presets, MIDI).

## 1. Objetivo

No una réplica perfecta de cada synth (no hay A/B ni se busca), sino un instrumento cuyo **proceso** lleve rápido a
los sonidos de la música de Juan: organic / afro / melodic / progressive house, house, disco house e indie tech.
Las referencias (Sébastien Léger, Adam Ten, Purple Disco Machine, Roy Rosenfeld, ANOTR, Bob Sinclar) orientan las
**técnicas**, no se copian sonidos ni temas.

Criterio de éxito: con el Brain solo (sin plugins) Juan arma en pocos minutos un bajo, un stab, un pad, un pluck y
un arpegio que **se sientan en el mismo mundo** que sus referencias dentro de un mix, y desde ahí los lleva a su
propio sonido.

## 2. Qué tienen en común esos estilos (técnicas, no sonidos)

Rasgos típicos del género (orientación de producción, no afirmaciones sobre el equipo de cada artista):

| Técnica | Dónde aparece | Qué pide al instrumento |
|---|---|---|
| **Stabs de acordes** (un dedo, un acorde) | disco house, house, ANOTR / PDM | Modo de acordes (chord memory), envelope corta, chorus |
| **Bombeo con el kick** (sidechain) | todo el house | Ducking sincronizado al tempo, sin compresor externo |
| **Filtro tipo "French touch"** (barrido LP / HP sobre todo el sonido) | disco / French house | **Fuera del Brain:** lo hace el filtro del GrooveMixer (decisión de Juan, 2026-10-25) |
| **Arpegios y secuencias** sincronizados | melodic / progressive (Léger, Rosenfeld) | Arpegiador con divisiones, gate, octavas, swing; consciente de la tonalidad |
| **Plucks** (ataque seco, decay corto, filtro que se cierra) | melodic, organic | Envelope de filtro rápida, velocity al brillo |
| **Pads que evolucionan** anchos | progressive, melodic | Unison / detune, LFO lento al filtro, chorus estéreo, reverb grande |
| **Bajos de octava y "slap" sintético** | disco house, nu-disco | Mono con glide y legato, filtro con env punchy, drive |
| **Percusión tonal orgánica** (marimba, kalimba, maderas, campanas) | organic / afro | FM de 4 operadores, modal reverb con material GUADUA |
| **Órgano de house** y pianos de los 90 | house, Bob Sinclar, ANOTR | Patches FM de órgano / piano, saturación |
| **Delays sincronizados** (ping-pong, 1/8 con punto) | todos | Delay con tempo en la cadena del synth |
| **Phaser / chorus "disco"** | disco house, nu-disco | Phaser o chorus de ensemble en la cadena |
| **Calidez y grano** (cinta, saturación suave) | todos; más en indie tech | TAPE por defecto, etapa de salida ✓, BIT para lo sucio |

## 3. Qué ya existe y qué falta

| Pieza | Estado |
|---|---|
| Engines Juno / Prophet / OB-6 / Model D / DX7 / ARP | Existen; fidelidad 70–85 % (`fidelidad-engines.md` §12) |
| SYNTH FX: BIT · TAPE · CHORUS · MODAL | En la cadena del synth |
| Delay (GHOST ECHO), reverb SPRING + PLATE, BEAT SYNC | Existen en el **modo FX**, no en la cadena del synth |
| Arpegiador SMART ARP (Markov, acordes, sincronía) | Existe en `ml/`, solo en modo AI |
| Detección de tonalidad y acordes (Camelot) | Existe (Layer 1 AI) |
| Modo de acordes, unison, sidechain, phaser | **Faltan** (el filtro DJ lo pone el GrooveMixer) |
| Banco de presets por estilo | **Falta** (hay uno de fábrica por engine, theory/06 §8) |

## 3b. Identidad de cada engine (direcciones a validar de oído)

| Engine | Rol en este estilo |
|---|---|
| Model D | Cuerpo y bajos: octava, slap sintético, leads gordos |
| Prophet-5 | Plucks y brass articulados, cuerdas, POLY-MOD para campanas |
| Juno-106 | Acordes y amplitud: stabs, pads, cuerdas con chorus |
| OB-6 | Movimiento: pads y brass que respiran, X-MOD, filtro con morph |
| DX7 (FM) | Madera, metal y percusión tonal: marimba, kalimba, campanas, órgano, pianos |
| ARP 2600 | Color: ácido, S&H, ring mod, texturas de indie tech |

Los presets Signature (fase 4) se diseñan dentro de estos roles; Juan los valida escuchando.

## 4. Plan por fases (una a la vez, cada una con demo audible)

Rev. 1: **las herramientas de proceso van primero** y el trabajo de engines se mide con criterios audibles, no
con un porcentaje de fidelidad.

### Fase 1 — Herramientas de proceso (performance) — primero

0. **Reloj común con fase:** un solo módulo de tiempo (BPM + posición dentro del compás) que comparten PUMP, ARP,
   DELAY y el SYNC del LFO. Fuentes: clock MIDI con START / SONG POSITION, BPM interno o tap tempo, y el beat
   detectado por la IA (`BEAT_DETECTED`). Conocer el BPM no basta: PUMP tiene que bombear **en fase** con el kick,
   así que necesita la posición del beat, no solo la velocidad.
1. **PUMP:** ducking del VCA con forma y profundidad, anclado al reloj común. Es el "groove" del house sin
   compresor externo.
2. **CHORD:** memoria de acordes (tocar uno, grabar las notas, después cada tecla dispara el acorde transpuesto);
   opción "en tonalidad" con la tonalidad detectada.
3. **ARP:** sacar SMART ARP del modo AI a la página de synth: UP / DOWN / UP-DOWN / RANDOM / SMART, divisiones con
   tresillos y puntillo, gate, octavas 1–3, **swing**, latch.
4. **UNISON:** voces apiladas con detune y spread estéreo. **Cuesta polifonía** (6 voces en Juno / OB-6 / DX7):

   | Unison | Notas a la vez | Uso |
   |---|---|---|
   | ×1 | 6 | Pads y acordes |
   | ×2 | 3 | Leads, stabs de 3 notas |
   | ×3 | 2 | Leads gruesos |
   | ×6 | 1 | Bajo / lead mono |

   Un acorde de 4 notas con unison ×2 necesitaría 8 voces: para **pads**, el ancho sale de las voice cards, el
   chorus estéreo y el detune por voz (sin apilar). Subir a 8 voces es posible solo si la CPU medida lo permite.

**Demo:** un loop con stab de acordes bombeando, arpegio con swing y pad en unison, solo desde el Brain.

### Fase 2 — Engines: lo que se oye en estos estilos

Puntos de `fidelidad-engines.md` §12 que pesan en esta música, con **criterios audibles** en vez de un porcentaje:
ataque definido, graves estables al tocar en el registro bajo, modulación expresiva (filtro, LFO, X-MOD) y ausencia
de artefactos molestos (aliasing, clicks, zipper):

| Engine | Trabajo | Para qué sonidos |
|---|---|---|
| Model D | 6 formas del Minimoog; FM del VCO 3 a audio; contornos ADS con DECAY | Bajos de house / disco, leads |
| OB-6 | X-MOD con el VCO 2 real | Brass, leads, pads con movimiento |
| Juno-106 | Tiempos de envelope del 106; hiss del BBD (opcional) | Stabs, pads, cuerdas |
| Prophet-5 | Forma de saw / pulse; unison | Cuerdas, brass, plucks con POLY-MOD |
| DX7 | Ajuste de timbres FM de madera / campana / órgano | Marimba, kalimba, órgano, pianos |

ARP queda como color (indie tech, ácido); su matriz de patch espera. **Demo:** cada engine en su rol (§3b) dentro
de un loop de house, sin los artefactos de la lista.

### Fase 3 — Efectos del estilo en la cadena del synth

1. **DELAY** sincronizado: 1/8, 1/8 con punto, 1/4, ping-pong, filtro en la realimentación. GHOST ECHO hoy es
   **mono por dentro** (`getOutputL()` y `getOutputR()` devuelven el mismo mezclador): el ping-pong exige dos
   líneas de delay cruzadas, memoria de delay doble y medir la CPU.
2. **PHASER** de disco (nuevo, barato) o ensemble como modo del CHORUS.
3. **REVERB grande** junto a MODAL. SPRING + PLATE usa `AudioEffectFreeverb`, que es **mono**: una reverb estéreo
   de verdad pasa a `AudioEffectFreeverbStereo` (Teensy Audio Library) o a una propia; más CPU que la actual.

Cadena propuesta (orden fijo, cada una con on / off y MIX): BIT → TAPE → CHORUS / PHASER → DELAY → REVERB (MODAL o
PLATE). El filtro DJ queda en el GrooveMixer. Presupuesto de CPU a medir: objetivo ≤ 60 % total con 6 voces (`01` §5.2).

**Demo:** pluck con delay 1/8 con punto en ping-pong y pad con plate estéreo.

### Fase 4 — Banco Signature por estilo y macros

- **Banco "Signature"** además de los clásicos de cada engine: 6–8 sonidos por estilo (organic / afro, melodic /
  progressive, house / disco, indie tech), cada uno con su engine, FX, PUMP / ARP / CHORD guardados en el preset.
  Nombres propios (no de artistas ni de temas).
- **Macro MOVE:** un control de performance que mueve varios parámetros a la vez según el preset (p. ej. cutoff +
  delay feedback + reverb mix), para los builds y breaks.
- Ampliar el formato de preset (`PRESET_FORMAT` 3) para guardar PUMP, CHORD, ARP, UNISON, FX nuevos y MOVE.

**Demo:** un set corto tocando solo presets Signature.

### Fase 5 — Ajuste con las referencias (sin copiar)

- **Perfil de balance tonal por estilo, solo como contexto:** analizar las referencias de Juan a nivel de género
  (curva espectral promedio, ancho estéreo por banda). **No es un objetivo para los presets:** el espectro de una
  mezcla incluye kick, bajo, voces y mastering, y llevarlo a un synth aislado da decisiones equivocadas. Sirve para
  ver en qué banda vive cada rol (bajo, stab, pad) dentro del mix. No se extraen melodías, sonidos ni samples.
- **Sesiones de escucha** de Juan: cada sonido Signature dentro de un loop junto a una referencia, y ajuste de oído
  (calidez, ancho, pegada, brillo). El criterio es "encaja en el mismo mix", no "es igual".

## 5. Lo que no entra

Vocoder, samples / ROMplers (pianos M1, etc.), secuenciador de patrones completo, DX7 de 6 operadores, matriz de
patch del ARP. Se re-evalúan después de la fase 5.

## 6. Decisiones abiertas para Juan

1. ¿Aprobar el orden (rev. 1): proceso → engines → FX → banco Signature → ajuste con referencias?
2. ~~Filtro DJ~~: resuelto, va en el GrooveMixer.
3. PUMP: ¿solo con clock externo o también con BPM interno / tap tempo?
4. Panel: dónde viven ARP, CHORD y PUMP (página nueva de PERFORM o la página LFO / MOD), sin romper el mapa de 4
   encoders (theory/04 §3.1).

## 7. Propuesta de reloj y panel para la fase 1 (2026-10-25, a confirmar)

**Reloj común (PUMP, ARP, DELAY, SYNC del LFO).** Prioridad automática de fuentes:
1. Clock MIDI con START / SONG POSITION (Ableton, GroovePush): fase exacta del beat 1.
2. Beat detectado por la IA (`BEAT_DETECTED`, trae fase): tocando sobre audio sin clock.
3. BPM interno con tap tempo: tocando solo.

PUMP tiene **OFFSET** (± ms) para calzar el bombeo con el kick y compensar la latencia del clock.

**Página GROOVE (SHIFT + B3).** Igual que LFO / MOD: NAV elige la parada, NAV push la enciende / apaga, 4 encoders
editan (+ capa SHIFT). Respeta el mapa de 4 encoders de theory/04 §3.1.

| Parada | P1 | P2 | ENC L | ENC R | SHIFT |
|---|---|---|---|---|---|
| PUMP | Profundidad | Forma (suave / exp / kick) | Ritmo (1/4, 1/8, 1/2 compás) | OFFSET (ms) | Release · fuente del reloj |
| ARP | Modo (UP, DOWN, UP-DOWN, RANDOM, SMART) | División (tresillos, puntillo) | Gate | Octavas 1–3 | Swing · latch |
| CHORD | Tipo (memoria / en tonalidad) | Inversión | Strum | Tonalidad y escala | — |
| UNISON | Voces (×1, ×2, ×3, ×6) | Detune | Spread estéreo | — | — |

- CHORD: con el acorde apretado, NAV push lo graba.
- TAP: B4 en la página GROOVE (como B4 → ENGINE en PRESETS) y tocando el BPM en la pantalla.
- PERFORM: chips de estado (PUMP / ARP / CHORD activos y BPM).
- Protocolo: GROOVE usa ids de página y grupos de parámetros nuevos en `gf_params.h`; los CC de theory/07 se amplían.
