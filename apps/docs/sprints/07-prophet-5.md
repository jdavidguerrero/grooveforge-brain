# Sprint 2.2 — Engine Prophet-5 con Polyphony de 5 Voces

**Status:** In Progress
**Refs:** `apps/docs/01-architecture.md` §4.1, `apps/docs/05-fx-architecture.md` §2

---

## Theory

### Prophet-5 — Arquitectura

El Prophet-5 (Sequential Circuits, 1978) fue el primer sintetizador polifónico
programable comercialmente exitoso. Su innovación central: 5 voces × 2 VCOs = 10
osciladores simultáneos, con memoria de patches que permitía reproducir sonidos exactos
entre sesiones. Cada voz es un sintetizador monofónico completo:

```
VCO-A ──┐
        ├──→ VoiceMixer ──→ VCF ──→ VCA ──→ MasterMixer ──→ Output
VCO-B ──┘
```

El resultado: acordes ricos con el carácter analógico de dos osciladores por nota,
sin el drift de estabilidad del Moog ni la austeridad del Juno (1 DCO por voz).

### Curtis CEM3340 — El Chip VCO

El CEM3340 era un VCO completo en un chip integrado: generaba simultáneamente sawtooth,
triangle y pulse con excelente estabilidad de V/oct. Su característica definitoria era
una caída de temperatura muy específica que producía un drift sutilmente diferente al
de los VCOs discretos del Moog — no inestabilidad, sino un "respiro" controlado que
los músicos identifican como el "sonido Sequential".

En nuestra implementación digital, `AudioSynthWaveform` emula la estabilidad del
CEM3340 sin el drift térmico. Si en futuras versiones se quiere emular el drift:
un LFO de baja frecuencia y amplitud muy pequeña (±1-2 cents) en cada VCO captura
el carácter sin desafinar.

### Cross-Modulation (Poly-Mod)

El Prophet-5 Rev 3.3 incluía una sección "Poly-Mod" que enrutaba:
- Salida de VCO-B → modulación de frecuencia de VCO-A (FM lineal)
- Filter Envelope → cutoff del filtro (modulación adicional de envelope)

Cuando el crossmod es alto, crea sonidos FM-like: metálicos, de campana, inarmónicos.
La diferencia con FM puro (Yamaha DX7) es que el Prophet-5 usa FM lineal entre
osciladores analógicos, no moduladores de fase digitales.

**Nuestra aproximación:** En `update()`, calculamos la fase del oscilador-B y la
usamos para modular la frecuencia de VCO-A via `sin()`. Esto es una aproximación
al FM analógico — no FM exacto (el FM exacto requeriría acceso a la fase interna del
oscilador, no disponible en `AudioSynthWaveform`). Es suficiente para capturar el
carácter tímbrico del Poly-Mod. Documentado en código como aproximación.

### Voice Allocation y Voice Stealing

Con 5 voces, el engine necesita un "planificador" que asigne voces físicas a notas
MIDI. El criterio de robo (stealing) es político: distintos synths clásicos usaban
estrategias diferentes.

**Estrategia implementada: FIFO (oldest-first stealing)**
- noteOn: buscar voz libre → si no hay, robar la voz con menor `age`
- noteOff: liberar exactamente la voz que tiene esa nota
- Re-trigger: si la nota ya suena, retrigger en la misma voz (evita duplicados)

```
Estado de las voces (ejemplo):
noteOn(C4):  [C4 age=1] [—] [—] [—] [—]
noteOn(E4):  [C4 age=1] [E4 age=2] [—] [—] [—]
noteOn(G4):  [C4 age=1] [E4 age=2] [G4 age=3] [—] [—]
noteOff(E4): [C4 age=1] [— free] [G4 age=3] [—] [—]
noteOn(B4):  [C4 age=1] [B4 age=4] [G4 age=3] [—] [—]
(6ta nota, todas ocupadas → robar age=1, C4):
noteOn(D5):  [D5 age=5] [B4 age=4] [G4 age=3] [—] [—]
```

El campo `midi_note = 255` sirve como centinela de "voz libre" (255 es valor
inválido en MIDI, cuyo rango es 0–127).

### Por Qué el Prophet-5 es "El Sonido de los 80s"

1. Primer poly programable: los músicos podían guardar patches y reproducirlos
   exactamente — en 1978 esto era revolucionario.
2. 2 VCOs por voz: más rico que el Juno-106 (1 DCO), más estable que el Moog (3 VCOs
   con drift visible).
3. El CEM3340 con su carácter tímbrico específico.
4. Usado en: "Jump" (Van Halen — el pad de apertura icónico), "Don't You Forget About
   Me" (Simple Minds), "The Look" (Roxette), prácticamente todo el pop de los 80s con
   pads polifónicos ricos.

### AudioMemory para 5 Voces

```
Cálculo de bloques de audio necesarios:

Por voz (5 voces):
  oscA → voiceMix (ch0):   1 conexión
  oscB → voiceMix (ch1):   1 conexión
  voiceMix → vcf:          1 conexión
  vcf → vcaEnv:            1 conexión
  Subtotal por voz: ~4 bloques activos

5 voces × 4 bloques = 20 bloques activos

Master path:
  vcaEnv[0..3] → masterMixA: 4 bloques
  vcaEnv[4]    → masterMixB: 1 bloque
  masterMixA   → finalMix:   1 bloque
  masterMixB   → finalMix:   1 bloque
  finalMix     → out (L+R):  2 bloques
  Subtotal master: ~9 bloques

Total bloques activos: ~29 bloques

AudioMemory(72) = 72 × 256B = 18.4KB
Factor de seguridad: 72 / 29 ≈ 2.5× — amplio margen
Budget total de RAM: 18.4KB / 400KB = 4.6% del budget de audio
```

### Estimación de CPU

Datos de referencia del proyecto:
- Moog Model D (1 voz, 3 VCO + env + filter): ~0.6% CPU medido en hardware

Prophet-5 por voz: 2 VCO + VoiceMixer + VCF + VCA env = similar al Moog sin noise ni
3er oscilador. Estimado: ~0.5% CPU por voz.

```
5 voces × 0.5% = 2.5% estimado
Overhead master mix + crossmod update: ~0.5%
Total estimado Prophet-5: ~3% CPU

Budget specs (01-architecture.md §4.1): engines ~30% @ 6 voces
Prophet-5 @ 5 voces: 3% << 30% — amplio headroom para FX
```

### Wiring

Sin cambios de hardware respecto a sprints anteriores. Mismo pin mapping
`01-architecture.md §3.3`. Los objetos `AudioOutputI2S` y `AudioControlSGTL5000`
se instancian dentro del engine — misma arquitectura que `MoogModelD` y `Juno106`.

---

## Implementation Notes

### Por qué 27 conexiones nombradas individualmente (no array)

`AudioConnection` en la Teensy Audio Library no tiene constructor de copia — su
constructor toma referencias a objetos fuente y destino y los registra en el grafo
global de audio en tiempo de construcción. Los arrays de C++ con brace-init llaman
al constructor de copia al inicializar, lo que no es válido para `AudioConnection`.

La solución: declarar cada conexión como miembro nombrado (`_c0` a `_c26`) e
inicializarlas en la lista de inicialización del constructor. Es verbose pero compila
limpio y es el patrón establecido en `MoogModelD` (`_c1`..`_c8`).

### Crossmod — Aproximación FM

El crossmod implementado es una aproximación al FM analógico del Prophet-5:
- Calculamos la fase del oscilador-B acumulando `oscBFreq × dt_ms × 0.001` por ciclo
- Aplicamos `sin(2π × phase)` como modulador de la frecuencia de VCO-A
- `crossmodAmt` escala la profundidad de modulación

Esta aproximación captura el carácter tímbrico (sonidos campana/metálicos a valores
altos) sin implementar FM exacto. El FM exacto requiere acceso a la fase interna del
`AudioSynthWaveform`, que no está expuesto por la API de la Teensy Audio Library.

---

## Demo

Acorde C mayor al arrancar el sketch (n60 + n64 + n67 simultáneos):

```
Serial → n60  (C4)
Serial → n64  (E4)
Serial → n67  (G4)
```

Resultado: acorde C mayor con 2 VCOs por nota = 6 osciladores simultáneos.
Aumentar crossmod con `m0.3` para efecto FM-like en cada nota del acorde.

---

*Sprint 2.2 — GrooveForge Brain · Juan Guerrero (GPROG)*
