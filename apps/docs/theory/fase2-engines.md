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
