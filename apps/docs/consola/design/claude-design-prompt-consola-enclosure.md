# Prompt para Claude Design — Enclosure de la GrooveForge Console (v3, complemento de GroovePush v2)

> **Cómo usarlo:** en el mismo proyecto de Claude Design de GroovePush (donde están
> `claude-design-2026-10-19-v1/` y `-v2/`), pegar lo que está debajo de la línea. Adjuntar:
> `GroovePush Enclosure v2.dc.html`, `groovepush-geometry-v2.js`, `REVIEW.md` de la v2,
> `kicad/consola/panel/panel-coordinates.csv` y el doc `apps/docs/consola/25-panel-consola.md`.
> Ya no tiene campos pendientes: el apilado de la Pi va estimado hasta que llegue el HAT NVMe.
> Base: `24-placa-principal.md` (D1–D7) y `25-panel-consola.md` (2026-11-11).

---

## Contexto

GroovePush v2 (adjunta) pasa a ser la **GrooveForge Console**: un instrumento autónomo para producir y tocar sets de
música generada **sin computador**. Conserva la identidad, los materiales, las reglas de validación (las 13 de la v1)
y las piezas ya diseñadas, pero cambian cuatro cosas: **forma plana**, **Pi separada de la pantalla**, **controles
nuevos** y **panel trasero completo**. Rehaz el layout con eso; conserva todo lo demás.

## 1. Forma: plana, estilo MPC / Ableton Push

- Cara superior **horizontal** de aluminio 2 mm (sin la cuña de 14–16° de la v2). La pantalla queda **a ras** del
  panel, como en Push 3 o MPC Live.
- Alto total objetivo **≤ 60 mm** en la zona de controles; donde vaya la Pi el cuerpo puede crecer por debajo
  (justifícalo con el apilado).
- Patas de goma; evalúa un ángulo opcional con patas traseras plegables (no cambia la geometría del cuerpo).
- Huella: **la consola puede crecer** frente a la v2 (284 × 251): los botones nuevos, los 6 FSR y los 2 Sharp
  necesitan aire, y Juan prefiere comodidad a compacidad. Compárala con Push 3 (378 × 293 mm) y MPC Live II
  (410 × 245 mm) como referencia, no como límite. Prioriza: manos que no chocan, grupos claros, pads FSR cómodos,
  Sharp sin disparos falsos.

## 2. Pi separada de la pantalla (calor)

En la v2 la Pi iba atornillada detrás del módulo de pantalla y el apilado (~50 mm) obligaba a la cuña. Ahora:

- La Pi 5 va en **su propio compartimento** con ventilación de entrada y salida (regla 9), unida a la pantalla por un
  **cable DSI largo** (FFC 22 pines, 0,5 mm; largos comerciales 200 / 300 / 500 mm: elige y justifica con el radio
  de curvatura de la regla 8).
- Apilado de la Pi: **Pi 5 + Active Cooler + HAT NVMe (arriba de la Pi, trae su regleta apilable) + RaspiAudio
  Audio+ V2 encima**. El HAT todavía no llega: **usa un alto estimado de 55–60 mm** desde la base de la Pi hasta el
  tope del Audio+ y deja el compartimento con **≥ 10 mm de margen** y tornillería que admita ajustar el alto. Marca
  esta medida como «a verificar». Verifica que el Active Cooler sigue teniendo aire bajo el HAT y que el FFC del
  PCIe y el DSI no se cruzan.
- Puertos de la Pi que deben salir al panel trasero con extensiones de panel: **USB-C de alimentación** (la única
  entrada de energía de toda la consola), **1 USB-A** (teclado MIDI) y **Ethernet**. Los otros 3 USB-A quedan
  internos (Teensy, NeoTrellis M4 y alimentación de LEDs de la placa principal).
- La pantalla, sin la Pi detrás, queda en **14,8 mm** de módulo: úsalo para bajar el perfil.

## 3. Controles

Los existentes se mantienen con sus medidas (tabla de la v1 y coordenadas de la v2): pantalla 7", 8 encoders EC11
bajo la pantalla (regla 6), NeoTrellis M4 8×4, 4 faders RS6011, 20 teclas Choc V2 en sus grupos (con los rótulos
nuevos de `25` §3: **LIVE · SYNTH · DRUM · SEQ · MIX** en los modos, PAGE ◀ ▶ y OCT − + en página/octava).

**Nuevos (ubícalos tú; los grupos no se mezclan):**

| Control | Cant. | Medidas de referencia | Notas de uso |
|---|---|---|---|
| Teclas Choc V2 con LED, grupo **Navegación**: BACK · MENU · ◀ · ▶ · BROWSE | 5 | como las 20 | Junto al jog, a mano de la pantalla |
| Teclas Choc V2 con LED, grupo **Motor en vivo**: CUE · FILL · TAP | 3 | como las 20 | Cerca del transporte |
| **Rueda jog** (EC11 con clic, **perilla propia impresa en 3D**, Ø35–45, ver §3.1) | 1 | agujero Ø7,1 | Reemplaza ↑ ↓ OK; navega listas de la pantalla |
| **Pads FSR con velocity** (FSR 402, Ø18 activo, con actuador de silicona y disco rígido) | 4 | pad visible ~25 × 25 mm, alto ~4 mm sobre el panel | Para tocar kick/perc con dinámica: junto a la NeoTrellis, al alcance de la mano de los pads |
| **FSR de expresión** (presión sostenida) | 2 | mismos FSR 402, pad ~25 × 25 | Abrir filtro, empujar un riser: fáciles de sostener mientras la otra mano trabaja |
| **Sensores de mano Sharp GP2Y0A41** (miran hacia arriba) | 2 | módulo 29,5 × 13 × 13,5 mm, ventana IR con cristal o ranura | Uno a cada lado o en las esquinas superiores; la mano pasa por encima a 4–30 cm **sin tapar la pantalla** (regla 5) |
| **Perillas** MASTER · CUE · IN (potenciómetro 9 mm, perilla Ø18) | 3 | | MASTER y CUE juntas, como en un mixer DJ |
| **Selector de pad de entrada** 0 / −10 / −20 dB | 1 | conmutador deslizante de 3 posiciones | Cerca de IN |

### 3.1 Perillas, keycaps y tapas de los FSR: diseño propio

Juan no compra perillas ni keycaps: **los diseña e imprime en 3D**, y piensa hacer **moldes para fundirlos en
silicona**. Diseña la familia completa, con la misma identidad:

| Pieza | Cant. | Requisitos |
|---|---|---|
| Perilla de encoder (8) y de MASTER / CUE / IN (3) | 11 | Ø18, eje D de 6 mm (EC11 y pot 9 mm: **verificar** el eje), indicador de posición visible desde arriba y desde el frente |
| Perilla del jog | 1 | Ø35–45, con hoyuelo para el dedo y borde estriado; peso o inercia opcional (inserto metálico) |
| Keycap Choc V2 | 30 | Vástago Choc V2 (cruz de 2 patas, **verificar** con la tecla), ~18 × 17 mm, ventana o material translúcido para el LED de abajo, rótulo grabado o en relieve |
| Tapa / actuador de FSR | 6 | Silicona sobre disco rígido Ø ≥ 14 mm centrado en el área activa del FSR 402 (Ø 12,7); que no toque el borde del sensor |

Para cada pieza: **versión para imprimir** (FDM o resina, tolerancias del encaje al eje o vástago, orientación de
impresión) y **versión para molde de silicona** (ángulos de desmoldeo, paredes, línea de partición, canal de
colado, molde en dos partes imprimible). Indica qué partes conviene en silicona (keycaps translúcidos, tapas de FSR,
grip del jog) y cuáles rígidas (núcleo con el encaje al eje).

Reglas nuevas (además de las 13):

14. Los 4 pads FSR y las 2 de expresión: ≥ 6 mm entre bordes y entre ellos y los pads de la M4; base rígida debajo.
15. Ningún Sharp mira hacia donde la mano toca encoders o faders (no debe dispararse al tocar otro control).
16. Las 10 teclas nuevas se ubican sin mirar, por grupo, como la regla 13.
17. El jog se alcanza con la mano derecha sin cruzar los faders.

## 4. Electrónica interna

| Pieza | Medidas | Notas |
|---|---|---|
| **PCB-M** (placa principal, reemplaza la portadora y la PCB-S) | **[a definir por ti]**, 2 capas | Teensy 4.1 en zócalo + módulo Audio Shield Rev D en headers (~20 mm de alto sobre la placa). Lleva los jacks del panel trasero en su borde. Propón tamaño y ubicación |
| **PCB-N** (navegación) | si hace falta | Si las 8 teclas nuevas, el jog, las 3 perillas y el selector no caen sobre PCB-M, van en esta placa pequeña |
| **PCB-P** (base de los 6 FSR) | plana, rígida | Sin componentes activos; los FSR se pegan encima |
| PCB-A, PCB-F, PCB-K1, PCB-K2 | ya diseñadas | **No cambian de forma** salvo que el layout plano lo exija; si las mueves, dilo |
| NeoTrellis M4 | 120 × 60 mm | Conectores hacia el frente como en la v2 |
| Pi 5 con su apilado | §2 | compartimento propio |

## 5. Panel trasero y frontal

Trasero: USB-C de alimentación · salida de sala L/R (2 × 6,35 mm) · LINE IN L/R (2 × 6,35 mm) · CV 1–2
(2 × 3,5 mm) · MIDI IN/OUT (2 × TRS 3,5 mm) · pedal (6,35 mm) · e-drum (DB15 o RJ45, tapa ciega) · USB-A ·
Ethernet. Frente: audífonos cue (6,35 mm). Regla 10 para todos, con cables enchufados.

## 6. Fabricación

Igual que la v2: cuerpo de PETG en piezas ≤ 210 × 210 mm (Ender 3 V3) con uniones ocultas y tornillos M3 a
insertos; aluminio CNC de 2 mm.

## Entregables

1. Vista superior 1:1 con el centro de cada control (existentes y nuevos).
2. Tabla de alineación encoder ↔ soft-key (regla 6).
3. Corte lateral con el apilado y el compartimento de la Pi, el recorrido del DSI y el aire.
4. Panel trasero y frontal con posiciones.
5. Vista explotada y despiece para impresión.
6. Reporte de las 17 reglas (pasa / no pasa y valor).
7. **Tabla de coordenadas final** (mismo formato que `panel-coordinates.csv`: ref, kind, group, board, x, y, w, h,
   notas) con la placa de cada control: de ahí salen PCB-M, PCB-N y PCB-P.
8. **Recomendación: ¿PCB-N existe o todo cae en PCB-M?**, con el motivo.
9. Dos renders de producto terminado (3/4 frente y trasera).
10. Lista de medidas a verificar con calibre (incluido el apilado de la Pi cuando llegue el HAT).
11. **Familia de perillas, keycaps y tapas de FSR** (§3.1): geometría de cada pieza, versión impresa y versión para
    molde de silicona, con el molde en dos partes.
