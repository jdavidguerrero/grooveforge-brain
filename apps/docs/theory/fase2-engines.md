# Fase 2 de carácter sonoro: engines

> **Fecha:** 2026-10-27 · **Estado:** en curso, un engine a la vez · **Depende de:** `caracter-sonoro.md` §4 fase 2,
> `revision-fidelidad-2026-10-25.md`, `fidelidad-engines.md` §11–12.

Objetivo: lo que se oye en house melódico, orgánico y disco, con **criterios audibles** (ataque, graves estables,
modulación expresiva, sin artefactos), no un porcentaje de parecido. Orden: OB-6 → Model D → Juno → Prophet → DX7.

## 1. OB-6: sync en el sentido real y X-MOD con el VCO 2 de verdad

### Qué estaba mal

- **Sync invertido** (revisión 2026-10-25, hallazgo alto): el código hacía `VCO2.syncTo(VCO1)`. En el OB-6
  **VCO1 es el esclavo de VCO2** ([manual OB-6, Oscillators](https://www.sequential.com/wp-content/uploads/2019/03/OB-6-Operation-Manual-1.1.pdf)):
  VCO2 fija la altura y VCO1 se barre por encima para el timbre de sync.
- **X-MOD desde el VCO 2** usaba el LFO como sustituto, a tasa de control: no hay FM de audio, que es lo que da el
  gruñido metálico del X-MOD del Oberheim.

### Diseño

**Orden de los objetos de audio.** La Teensy Audio Library llama a los `update()` en orden de construcción. El
maestro tiene que correr antes que el esclavo para que las vueltas del bloque ya estén anotadas: los VCO 2 se
declaran **antes** que los VCO 1. Invertir solo el puntero leería las vueltas del bloque anterior (2.9 ms tarde).

**Sync con el panel del Brain.** El Brain no tiene perilla de frecuencia del VCO 1: el INTERVAL / DETUNE del VCO 2 es
el único intervalo. Con SYNC encendido se interpreta como **el barrido de VCO1 sobre VCO2**: VCO2 (maestro) suena en la
nota y VCO1 (esclavo) en nota + intervalo. Así:

- la altura percibida es la de la nota (la del maestro), como en el OB-6;
- el preset XMOD LEAD (intervalo +7, sync) conserva altura y carácter, pero ahora el timbre de sync sale por el VCO 1
  (nivel 80) y el VCO 2 queda limpio, que es la combinación del original;
- X-MOD → FREQ 1 barre el esclavo: el barrido de sync clásico del OB-6.

Sin SYNC no cambia nada: VCO1 en la nota, VCO2 en nota + intervalo.

**X-MOD a tasa de audio.** El VCO 2 guarda su último bloque normalizado (−1…1). El VCO 1, que corre después, lo lee
muestra a muestra:

- FREQ 1: `dt[i] = dt · 2^(2 · VCO2_amt · vco2[i])` (hasta ±2 octavas, la misma profundidad que tenía a tasa de
  control). FM exponencial, como un VCO analógico con CV en su entrada exponencial.
- PW 1: `pw[i] = pw + 0.40 · VCO2_amt · vco2[i]`.
- `2^x` con un polinomio minimax de grado 5 (error relativo < 10⁻⁵, `fast_exp2`); `exp2f` por muestra en 6 voces costaba ~4 % de
  CPU extra.

La parte de FILTER ENV del X-MOD sigue a tasa de control (es una envolvente, no necesita más).

**Qué no cubre:** VCO2 → FLT MODE a tasa de audio (el morph del SEM se recalcula cada 4 muestras; queda solo con
FILTER ENV); la forma triangular del VCO 2 (no está en el panel del Brain y cambiaría el significado de los presets).

### Cómo se prueba

| Qué | Cómo |
|---|---|
| FM de audio: con `mod = 1` y 1 octava, el oscilador da el doble de vueltas | test native (`polyblep_core`) |
| PW por muestra dentro de 0.05–0.95 y sin NaN con modulación extrema | test native |
| `fast_exp2` contra `exp2f` en −3…3 | test native |
| Sync: XMOD LEAD suena en la misma altura y el barrido de X-MOD → FREQ 1 se oye como sync | en el hardware |
| CPU del OB-6 con 6 voces y X-MOD | `[stats]` en el hardware |

## 2. Model D: las 6 formas, FM de audio del VCO 3 y DECAY

### Qué faltaba

- El Minimoog tiene **6 formas** por VCO: triángulo, *shark tooth* (tri-saw), diente de sierra, cuadrada, pulso ancho y
  pulso angosto; en el VCO 3 la segunda posición es **diente de sierra inverso** (útil como LFO de rampa hacia abajo).
  El Brain tenía 4 (SAW · TRI · SQR · PULSE con el PW global).
- La rueda de MODULATION seguía al VCO 3 a tasa de control: exacto como LFO, pero sin la **FM de audio** de
  VCO 3 → VCO 1-2 que da los bajos metálicos y los leads "growl" del Model D.
- Los contornos del original son **ADS con interruptor DECAY**: encendido, al soltar la tecla la envolvente cae con el
  tiempo de DECAY; apagado, se corta.

### Diseño

**Formas, sin romper los presets:** los índices 0–3 siguen siendo SAW · TRI · SQR · PULSE (PW global). Se agregan
**4 = SHARK** (en el VCO 3: saw inversa) y **5 = NARROW** (pulso fijo del 10 %, como el pulso angosto del original).

- *Shark tooth* = ½ (saw + triángulo): sube en dos pendientes y cae de golpe a la mitad del salto de un saw. El salto
  lleva PolyBLEP (½ del del saw); el quiebre del triángulo es solo de pendiente.
- *Saw inversa* = −saw, con el BLEP con el signo invertido.

**FM de audio del VCO 3:** el VCO 3 se construye antes que los VCO 1-2 y les entrega su último bloque, como el VCO 2
del OB-6 (§1). La parte VCO 3 de la mezcla de MODULATION va a la FM exponencial por muestra
(`fm_oct = rueda · (1 − NOISE MIX)`, ±1 octava a fondo, la misma escala que ya tenía); la parte de ruido sigue a tasa de
control (es ruido filtrado lento). El destino FILTER MOD sigue a tasa de control.

**DECAY:** la parada MODULATION tenía un lugar libre: **DECAY** (toggle). Encendido, el RELEASE de las dos envolventes
toma el valor de su DECAY (el comportamiento del original). Apagado, las envolventes siguen siendo ADSR como hasta
ahora: no se cambia el sonido de los presets guardados.

**Qué no cubre:** FILTER MOD a tasa de audio (el ladder recibe el cutoff por bloque).

### Cómo se prueba

| Qué | Cómo |
|---|---|
| SHARK y REV SAW acotadas, sin DC y sin NaN | test native (`polyblep_core`) |
| Las formas 0–3 de los presets guardados no cambian | los índices del panel quedan iguales (test de la plantilla) |
| FM de audio: OSC MOD con la rueda y el VCO 3 en audio da bandas laterales (growl), con LO sigue siendo vibrato | en el hardware |
| DECAY encendido: al soltar, la nota cae con el tiempo de DECAY | en el hardware |

## 3. Juno-106: tiempos de envolvente del 106 y hiss del BBD

### Qué faltaba

- Rangos de envolvente genéricos (0–5 s de ataque, 10 ms–5 s de decay y release). El 106 va de **1.5 ms a 3 s** de
  ataque y de **1.5 ms a 12 s** de decay y release (Roland Juno-106 Service Notes, sección ENV). Los pads largos y los
  *swells* del 106 piden esos 12 s; el ataque de 1.5 ms es el "click" de sus stabs.
- El chorus (MN3009) no tenía el **hiss** del BBD, parte del sonido del original con el chorus encendido.

### Diseño

- **Rangos** del Juno en `gf_params.h`: ATTACK 1.5–3000 ms, DECAY 1.5–12000 ms, RELEASE 1.5–12000 ms. Los presets
  guardan milisegundos: no cambian, solo el recorrido de la perilla. La curva sigue siendo la RC de `AnalogEnv`.
- **HISS** (0–100 %, por defecto 0 = apagado) en la parada LFO del Juno, que tenía lugares libres. Ruido blanco
  descorrelacionado L / R sumado a la señal retardada **antes** del pasabajos de 10 kHz del BBD (como en el circuito:
  el ruido sale del BBD y lo filtra el reconstructor). 100 % ≈ −50 dBFS: se oye en silencio, se pierde en la mezcla.
  Solo suena con el chorus encendido.

**Qué no cubre:** la envolvente única del 106 (VCF y VCA comparten una ADSR en el original); el Brain conserva dos
páginas (AMP y FILTER) para no cambiar los presets.

### Cómo se prueba

| Qué | Cómo |
|---|---|
| HISS 0 = salida idéntica a la de antes; HISS 100 con entrada en silencio da ruido acotado y estéreo | test native |
| Pad con RELEASE de 12 s y stab con ATTACK de 1.5 ms | en el hardware |

## 4. Prophet-5 y DX7: lo que no se cierra con código

- **Prophet-5.** El UNISON ya está (`0572ecb`). La "forma real de saw / pulse del CEM3340" de `fidelidad-engines.md`
  §12 no se puede ajustar sin una **grabación de referencia** de un Prophet real: la revisión de fidelidad lo marca
  como "bajo en código, alto en escucha". Cambiar la forma sin medirla es inventar. Pasa a la **fase 5** (A/B con
  nivel igualado, si se consigue la referencia).
- **DX7.** El banco ya cubre madera, campana y órgano (MARIMBA, BELLS, ORGAN, JD ZAP PERC WOOD). Afinarlos es de oído
  (fase 5). Los timbres nuevos del estilo (log drum afro, marimba de chonta, kalimba) se hacen como presets del
  **banco Signature** (fase 4).

## 5. Estado de la fase 2

| Engine | Hecho | Pasa a |
|---|---|---|
| OB-6 | Sync en el sentido real, X-MOD del VCO 2 a tasa de audio (§1) | — |
| Model D | 6 formas, FM de audio del VCO 3, DECAY (§2) | — |
| Juno-106 | Tiempos del 106, hiss del BBD (§3) | — |
| Prophet-5 | UNISON (antes) | Fase 5: A/B de la forma del CEM3340 |
| DX7 | Timbres de madera / campana / órgano (antes) | Fase 4: timbres afro / orgánicos · fase 5: afinarlos |

**Demo de la fase** (en el hardware, con el aislador): XMOD LEAD del OB-6 con X-MOD → FREQ 1; un bajo del Model D con
OSC MOD y el VCO 3 en audio; un pad del Juno con RELEASE largo y HISS al 40 %.
