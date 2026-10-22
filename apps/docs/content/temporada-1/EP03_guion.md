# EP3 — "Diseñando la caja de mi synth" · guion para grabar

> Temporada 1 · 🛠️ Taller · 12 min. Biblia: [`BIBLIA_T1_LIVESET.md`](BIBLIA_T1_LIVESET.md).
> **Posiciones de cámara (P1-P7):** [`GUIA_CAMARA.md`](GUIA_CAMARA.md). Formato igual al EP1.
>
> Casi todo es **P6** (grabación de la pantalla del computador: Claude Design, FreeCAD) y **P5** (a cámara). Las
> piezas reales se graban en **P2** (cenital cerrado) sobre la mesa vacía, como en tu foto con el brazo.
> Fuentes: `apps/docs/variants/studio/design/README.md`, `apps/docs/variants/studio/theory/02-studio-pcb.md`
> (§1-§3, §10E), commits `d212df0`, `21ece79`, `2bc6abe`.

---

## Antes de grabar

- [ ] Abrir en el navegador `apps/docs/variants/studio/design/claude-design-2026-10-09/GrooveForge Studio Enclosure.dc.html`
      y `Enclosure 3D.html` (también con `?cheeks=1` al final de la dirección, para las mejillas).
- [ ] Abrir `apps/design/3d-models/studio/studio-enclosure.FCStd` en FreeCAD y **guardar vistas** listas: corte
      lateral con el apilado, el encoder contra el aluminio, la portadora bajo la Pi.
- [ ] Sobre la mesa, para P2: un encoder EC11, una tecla Choc V2 (si la tienes), la Pi con la pantalla, la Teensy con
      el Audio Shield, un calibre. Fondo despejado (la mesa negra de tu foto es perfecta).
- [ ] Si alguna pieza ya está impresa o cortada, tenerla a mano. Si no, decirlo: *"todavía no está fabricada"*.
- [ ] En OBS, la fuente de pantalla en **1080p o más** y el zoom del navegador / FreeCAD alto: todo tiene que leerse
      en un celular.

---

## 1 · Cold open (0:00-0:15)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T1 🔲V | **P6** | FreeCAD, corte lateral: el cuerpo del encoder sale por encima del aluminio (márcalo en rojo en la edición) | — |
| T2 🔲V | **P2** | El encoder real; mides su cuerpo con el calibre | "En mi primer diseño, este encoder atravesaba la tapa de aluminio." |
| T3 | **P5** | A cámara | "Hoy te muestro cómo diseñé la caja de mi synth, y los tres errores que la habrían arruinado." |

## 2 · Qué tiene que cumplir (0:15-2:00)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T4 | **P1** | La protoboard con todos sus cables (si está montada; si no, la foto del banco a pantalla completa) | "Una protoboard no se lleva a un estudio, y menos a un show." |
| T5 | **P5** | A cámara | "La caja tiene que cumplir cuatro cosas. Aguantar las perillas cuando las giro fuerte. Dejar ver la pantalla. Sacar todos los conectores por atrás. Y verse como un instrumento, no como un proyecto del colegio." |
| T6 | **P6** | Vista general del `.dc.html` del enclosure, recorriéndola despacio | (va bajo T5) |

## 3 · Del prompt al primer diseño (2:00-4:00)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T7 | **P6** | `apps/docs/variants/studio/design/claude-design-prompt-enclosure.md`, resaltando las líneas con medidas reales | "La primera propuesta de forma la hice con IA, dándole las medidas reales de mis piezas." |
| T8 | **P6** | El resultado: vistas 1:1, corte, explotada, conectores | "Esto es lo que salió. Se ve bien. Pero un render bonito no significa que todo quepa. Eso lo comprobé pieza por pieza." |
| T9 | **P6** | La vista con los materiales | "Arriba, aluminio de 2 milímetros anodizado negro. El cuerpo, plástico PETG impreso en 3D. Y forma de cuña, inclinada 12 grados hacia mí." |
| T10 🔲V | **P6** | `Enclosure 3D.html?cheeks=1` girando: aparecen las mejillas | "Y opcional: mejillas de guadua, con imanes. Guadua, porque es de aquí." |

## 4 · Error 1: el encoder que atravesaba el aluminio (4:00-6:30)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T11 | **P2** | El encoder y el calibre: mides el cuerpo | "El cuerpo de este encoder mide 5 milímetros de alto." |
| T12 | **P2** | La tecla Choc al lado | "Y esta tecla necesita que su placa esté 3.4 milímetros debajo del aluminio." |
| T13 | **P6** | FreeCAD, corte: teclas y encoders en la misma placa, a −3.4 mm | "Si las teclas y los encoders van en la misma placa, el encoder queda 1.6 milímetros por encima de la tapa. La atraviesa." |
| T14 | **P6** | FreeCAD: la solución con dos niveles; PCB-A más abajo sobre separadores | "La solución: dos placas a distinta altura. La de los encoders va más abajo, sobre separadores de 10 milímetros, y el cuello del encoder sirve de guía a través del aluminio." |
| T15 | **P5** | A cámara | "El render no lo mostraba. La resta sí." |

## 5 · Error 2: la placa que no cabía (6:30-8:30)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T16 | **P2** | La Teensy con el Audio Shield encima, y la Pi al lado | "Esta es la placa principal: la Teensy con la tarjeta de sonido encima. Va debajo de la Raspberry Pi." |
| T17 | **P6** | FreeCAD: la portadora bajo la Pi, sin espacio | "Con la caja original no había espacio: chocaba con los pines de la Pi." |
| T18 | **P6** | Las dos alturas: 28 / 63 mm → 48 / 83 mm | "Así que subí la caja 20 milímetros parejo: de 28 a 48 adelante, de 63 a 83 atrás." |
| T19 | **P6** | La comprobación de choques con `freecadcmd` (theory/02 §10E) | "Y en vez de confiar en mi ojo, le pedí a FreeCAD que me dijera dónde se tocan las piezas." |
| T20 | **P6** | La pantalla corrida 2.5 mm | "De paso, corrí la pantalla 2.5 milímetros hacia atrás: su vidrio se montaba sobre la placa de las teclas." |

## 6 · Error 3: lo que encontró la revisión (8:30-10:30)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T21 | **P6** | El commit `2bc6abe` en GitHub o en la terminal | "Antes de fabricar, hice revisar todo el diseño. Encontró dos cosas más." |
| T22 | **P6** | `panel-aluminio.dxf`: los agujeros de los espárragos | "Los agujeros de los tornillos de la tapa estaban a 4.22 milímetros: esa es la medida de un separador, no la del tornillo. Los pasé a 3." |
| T23 | **P6** | FreeCAD: los jacks de 1/4" contra la placa | "Y los jacks quedaban a 1.5 milímetros de los contactos de la placa. Los subí y ahora quedan a 4.5." |
| T24 | **P5** | A cámara | "Ninguno de estos errores se ve en un render. Y cualquiera te cuesta una caja entera." |

## 7 · Lo que sale de aquí (10:30-11:30)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T25 | **P6** | La carpeta `apps/design/3d-models/studio/`: `cuerpo-petg.stl`, `panel-aluminio.dxf`, barras de la pantalla, soportes de las teclas, `difusor-nav.dxf` | "De aquí salen el archivo para la impresora 3D y el plano para cortar el aluminio." |
| T26 | **P5** | A cámara | "Y de las mismas medidas del panel salen las placas electrónicas. Eso es el episodio 7. [Estado real: 'ya mandé a imprimir…' / 'todavía no está fabricada'.]" |

## 8 · Cierre (11:30-12:00)

| T | Posición | Qué haces / qué se ve | Qué dices |
|---|---|---|---|
| T27 | **P5** | A cámara | "El próximo episodio vuelvo al estudio: pruebo los seis motores, y no todos van a pasar. Si quieres seguir el proceso, déjame tu correo." |

---

## Packaging

**Título principal:** *Diseñando la caja de mi synth (y los 3 errores que casi la arruinan)*
**A/B:** *El encoder que atravesaba mi caja de aluminio* · *De un prompt a FreeCAD: la caja de mi synth* ·
*Aluminio, PETG y guadua: así diseñé mi synth*

**Miniatura:** mitad izquierda, el corte de FreeCAD con el encoder atravesando el aluminio marcado en rojo; mitad
derecha, foto **P2** del encoder real en tu mano. Texto: **"NO CABÍA"**.

---

## Shorts (salen de las mismas tomas)

| Short | Tomas | Cómo se arma | Qué dices (nuevo) |
|---|---|---|---|
| **1 · "Este encoder atravesaba mi caja"** (40 s) | T1, T2, T11-T14 | Corte de FreeCAD ↔ pieza real | Arranque: "Este encoder atravesaba mi caja." Cierre: "Dos placas a distinta altura, y el cuello del encoder de guía." |
| **2 · "La caja, pieza por pieza"** (30 s, sin voz) | T8, T10, T16, T25 | Vista explotada girando → FreeCAD → piezas reales → mejillas de guadua | — |
| **3 · "Un render bonito no significa que quepa"** (40 s) | T8, T13, T17, T22 | Tú a cámara + los tres errores, 8 s cada uno | Arranque: "La IA me propuso esta caja. Mi placa no cabía." Cierre: "Mide siempre. ¿Cuál fue tu peor error de medidas?" |

---

## Plan de sesión, ordenado por posición (≈ 1 h 30)

| Orden | Posición | Tomas | Min |
|---|---|---|---|
| 1 | **P6** navegador | T6-T10, T21 | 15 |
| 2 | **P6** FreeCAD | T1, T13, T14, T17-T20, T22, T23, T25 | 25 |
| 3 | **P2** piezas sobre la mesa | T2, T11, T12, T16 + B-roll de cada pieza | 15 |
| 4 | **P1** | T4 | 5 |
| 5 | **P5** (a cámara) | T3, T5, T15, T24, T26, T27 + frases de los Shorts | 20 |
