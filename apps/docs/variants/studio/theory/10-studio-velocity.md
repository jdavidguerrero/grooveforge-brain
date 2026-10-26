# Studio · Velocity por preset (VEL → AMP · VEL → FILTER)

> **Fecha:** 2026-10-26 · **Estado:** APROBADO por Juan ("implementa todo lo necesario para arrancar con la generación
> de presets") · **Depende de:** `docs/theory/caracter-sonoro.md` §9–10 (manifiesto: bajos con bounce y ghost notes) ·
> theory/06 (presets) · theory/09 §6 (POCKET).

## 1. Problema

El manifiesto pide bajos con **ghost notes**: una nota a velocity 45–53 tiene que sonar como un murmullo y una a 74–81
tiene que morder. Hoy:

- Los cinco engines analógicos usan la **misma curva fija**: volumen `0.35 + 0.65·v` y brillo de la envolvente del
  filtro `0.5 + 0.5·v` (`v` = velocity 0–1).
- El sketch fuerza `v = 1` en el **Model D** y el **Juno**, como los originales (sin velocity); solo POCKET la devolvía.

Una curva fija no sirve para todo: un pad quiere poca respuesta, un bajo funk mucha, y un Juno de época, ninguna.

## 2. Diseño

Dos profundidades por preset, 0–100 %, en la **capa SHIFT de la página ENV** (estaba vacía):

| Control | Parámetro | Efecto por voz |
|---|---|---|
| SHIFT + P1 | **VEL → AMP** | ganancia = `1 − amp · (1 − v)` |
| SHIFT + P2 | **VEL → FILTER** | cantidad de la envolvente del filtro = `1 − cut · (1 − v)` |

- Con **amp = 65 %** y **cut = 50 %** la fórmula es exactamente la curva actual: los patches de hoy suenan igual.
- **100 %** = respuesta completa (una nota a v = 0.4 suena al 40 % y abre el filtro al 40 % del recorrido de la
  envolvente). **0 %** = sin velocity, como un Model D o un Juno originales.
- VEL → FILTER escala el **barrido de la envolvente**, como la velocity al filtro de los polys de época: con cutoff
  bajo y FLT ENV AMT alto (un bajo), una ghost note casi no abre y una nota fuerte muerde ("slap").
- **DX7:** la velocity ya mueve el índice de modulación y la ganancia de las portadoras dentro de cada operador. VEL →
  AMP le pasa `v' = 1 − amp · (1 − v)` (100 % = como hoy); VEL → FILTER no aplica.

**Valores por defecto por engine:** Model D y Juno 0 % / 0 % (como los originales), Prophet, OB-6 y ARP 65 % / 50 %
(la curva actual), DX7 100 %.

**POCKET** (theory/09 §6) ya no reemplaza la velocity: **suma** profundidad, `amp + POCKET · POCKET VEL · 65 %` y
`cut + POCKET · POCKET VEL · 50 %` (tope 100 %). En el Model D y el Juno sigue "devolviendo" la velocity; en los demás
la acentúa.

## 3. Protocolo y presets

- `PARAM_CHANGED` en el grupo ENV: **idx 8 = VEL → AMP**, **idx 9 = VEL → FILTER**, normalizados 0–1 (= 0–100 %).
- `PRESET_FORMAT` **4**: los dos valores van al final. Los presets de formato 2 y 3 se migran con los valores por
  defecto del engine (el sonido no cambia).
- Presets de fábrica con velocity alta: JD RUN FUNKBASS (VEL → FILTER 80 %, el pedido original), GF POCKET BASS y
  JD RAP STAB JUNO.

## 4. Cómo se prueba

| Qué | Cómo |
|---|---|
| Valores por defecto = curva actual; 0 % y 100 %; migración 3 → 4; edición en ENV SHIFT | test native |
| POCKET suma profundidad (tope 100 %) | test native |
| Ghost notes del MIDI de bajo de "Run" (45–53 murmullo, 74–81 mordida) | en el hardware |

## 5. Qué no cubre

Curvas no lineales (exponencial / log), velocity a la resonancia o al ataque, y la velocity de cada operador del DX7
(su sensibilidad por operador queda como está).
