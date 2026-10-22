# EP3 — "Diseñando la caja de mi synth" · guion para grabar

> Temporada 1 · 🛠️ Taller. Duración objetivo: 12 min. Biblia: [`BIBLIA_T1_LIVESET.md`](BIBLIA_T1_LIVESET.md).
> Todo se graba en pantalla (Claude Design, FreeCAD, KiCad) + tomas de las piezas reales del banco. No hace falta
> nada fabricado.
> Fuentes: `apps/docs/variants/studio/design/README.md`, `apps/docs/variants/studio/theory/02-studio-pcb.md`
> (§1-§3, §10E), commits `d212df0`, `21ece79`, `2bc6abe`.

---

## Antes de grabar

- [ ] Abrir en el navegador `apps/docs/variants/studio/design/claude-design-2026-10-09/GrooveForge Studio Enclosure.dc.html`
      y `Enclosure 3D.html` (también con `?cheeks=1`).
- [ ] Abrir `apps/design/3d-models/studio/studio-enclosure.FCStd` en FreeCAD; preparar las vistas: corte lateral
      con el apilado, el EC11 contra el aluminio, la portadora bajo la Pi.
- [ ] Tener a mano, para tomas físicas: un EC11, una tecla Choc V2 (si la tienes), la Pi con la pantalla, la Teensy
      con el Audio Shield, un calibre.
- [ ] Si alguna pieza ya está impresa o cortada, tenerla en la mesa. Si no, decirlo: *"todavía no está fabricada"*.

---

## 0:00-0:15 · Cold open

| Tiempo | Imagen | Audio |
|---|---|---|
| 0:00 | **[C]** FreeCAD, corte lateral: el cuerpo del encoder sale por encima del aluminio, en rojo | Clic de selección |
| 0:04 | **[A]** el encoder real junto al calibre | VO: *"En mi primer diseño, este encoder atravesaba la tapa de aluminio."* |
| 0:08 | **[F]** con la protoboard detrás | VO: *"Hoy te muestro cómo diseñé la caja de mi synth, y los tres errores que la habrían arruinado."* |

## 0:15-2:00 · Qué tiene que cumplir la caja

**[F]** + B-roll de la protoboard:
> "La protoboard no se lleva a un estudio, y menos a un show. La caja tiene que: aguantar las perillas cuando las
> giro fuerte, dejar ver la pantalla, sacar todos los conectores por atrás, y verse como un instrumento, no como un
> proyecto escolar."

**[C]** la vista general del `.dc.html` del enclosure.

## 2:00-4:00 · Del prompt al primer diseño

- **[C]** `apps/docs/variants/studio/design/claude-design-prompt-enclosure.md`: el prompt (resaltar 3-4 líneas
  clave: medidas reales de las piezas, materiales, cuña).
- **[C]** el resultado de Claude Design: vistas 1:1, corte, explotada, conectores.
- VO: *"Usé IA para la primera propuesta de forma, con las medidas reales de mis piezas. Pero un render bonito no
  significa que todo quepa. Eso lo comprobé pieza por pieza."*
- Materiales (README §Resumen): **aluminio 6061-T6 de 2 mm anodizado negro** arriba, **cuerpo de PETG 3 mm**
  impreso boca abajo con insertos M3, cuña de **12°**. Mostrar `?cheeks=1`: las mejillas opcionales de **guadua
  laminada de 12 mm** con imanes. *"Guadua, porque es de aquí."*

## 4:00-6:30 · Error 1: el encoder que atravesaba el aluminio

- **[A]** el EC11 y el calibre: cuerpo de 5 mm, buje liso Ø6.8 (`02-studio-pcb.md` §1).
- **[A]** la tecla Choc: su placa tiene que estar 3.4 mm bajo el aluminio (1.2 de clip + 2.2 de carcasa).
- **[C]** FreeCAD, corte: si teclas y encoders compartieran placa a −3.4 mm, el cuerpo del encoder llegaría a
  **+1.6 mm: por encima del aluminio**.
- La solución (§2): **dos niveles**. La placa de los encoders (PCB-A) baja a −12 mm sobre separadores de 10 mm, y el
  buje liso del encoder sirve de guía a través del aluminio, sin tuerca.
- VO: *"El render no lo mostraba. La resta sí."*

## 6:30-8:30 · Error 2: la placa que no cabía

- **[C]** la portadora (Teensy + Audio Shield sobre headers) bajo la Pi.
- El diseño original tenía la cuña de **28 / 63 mm**; la portadora con el shield desmontable no entraba con holgura
  frente al GPIO de la Pi. Se subió **+20 mm** pareja: **48 / 83 mm** (README rev. 2026-10-14 b, theory/02 §10E).
- **[C]** la comprobación de choques con `freecadcmd` (§10E): *"le pregunté a FreeCAD dónde se tocan las piezas,
  en vez de confiar en mi ojo."*
- Ajuste extra: la pantalla se corrió **2.5 mm** hacia atrás para que su vidrio no se montara sobre la placa de las
  teclas.

## 8:30-10:30 · Error 3: lo que encontró la auditoría

Commit `2bc6abe` ("correcciones mecánicas de la auditoría antes de fabricar"):
- Los agujeros de los espárragos PEM estaban a **Ø4.22** (la medida de un separador, no la del espárrago): pasaron a
  **Ø3.0** en `panel-aluminio.dxf`.
- Los jacks de 1/4" quedaban a **1.5 mm** de los pads de la portadora: subieron de z 22 a z 25 y quedaron a
  **4.5 mm**.
- VO: *"Ninguno de estos errores se ve en un render. Todos te cuestan una caja entera."*

## 10:30-11:30 · Lo que sale de aquí

- **[C]** carpeta `apps/design/3d-models/studio/`: `cuerpo-petg.stl`, `panel-aluminio.dxf`, las barras de la pantalla,
  los soportes de las teclas, el difusor del anillo de LEDs (`difusor-nav.dxf`).
- VO: *"De aquí salen el archivo para la impresora 3D y el plano para cortar el aluminio. Y de las mismas coordenadas
  del panel salen las placas: eso es el episodio 7."*
- Estado real: *[elige] "ya mandé a imprimir…" / "todavía no está fabricada"*.

## 11:30-12:00 · Cierre

**[F]**: *"El próximo episodio vuelvo al estudio: pruebo los seis motores, y no todos van a pasar."* CTA único.

---

## Packaging

**Título principal:** *Diseñando la caja de mi synth (y los 3 errores que casi la arruinan)*
**A/B:**
1. *El encoder que atravesaba mi caja de aluminio*
2. *De un prompt a FreeCAD: la caja de mi synth*
3. *Aluminio, PETG y guadua: así diseñé mi synth*
4. *Un render bonito no significa que quepa*

**Miniatura:** corte de FreeCAD con el encoder atravesando el aluminio, marcado en rojo; el encoder real en la mano
del otro lado. Texto: **"NO CABÍA"**.

---

## Shorts del EP3

### Short 1 · Mind-blow — "Por qué mi synth tiene dos placas para el panel" (40 s)
- **0-3 s:** el encoder saliendo por encima del aluminio en el corte. *"Este encoder atravesaba mi caja."*
- **3-35 s:** 3.4 mm de la tecla vs 5 mm del encoder → dos niveles, buje liso como guía.
- **Archivos:** `02-studio-pcb.md` §1-§2.

### Short 2 · Build/ASMR — "La caja, pieza por pieza" (30 s, sin voz)
- Vista explotada del `.dc.html` girando, corte a FreeCAD, corte a las piezas reales sobre la mesa, cierre con las
  mejillas de guadua (`?cheeks=1`).

### Short 3 · Polémica — "Un render bonito no significa que quepa" (40 s)
- **0-3 s:** el render de Claude Design. *"La IA me propuso esta caja. Mi placa no cabía."* (Decir lo justo: la
  propuesta servía como forma; faltaban 20 mm para la portadora y un segundo nivel para los encoders.)
- **3-35 s:** los tres errores, 8 s cada uno.
- **35-40 s:** *"Mide siempre. ¿Cuál fue tu peor error de medidas?"*

---

## Plan de sesión (≈ 1 h 30)

| # | Setup | Toma | Para | Min |
|---|---|---|---|---|
| 1 | C | Navegador: prompt, `.dc.html`, `Enclosure 3D.html` con y sin mejillas | 2:00-4:00, Shorts 2/3 | 15 |
| 2 | C | FreeCAD: corte del encoder, portadora bajo la Pi, pantalla, choques | 4:00-8:30, Short 1 | 25 |
| 3 | C | Commit `2bc6abe`, `panel-aluminio.dxf`, carpeta de STL | 8:30-11:30 | 10 |
| 4 | A+V | Piezas reales: EC11 + calibre, Choc, Pi + pantalla, Teensy + shield | B-roll, Shorts | 15 |
| 5 | F+V | Cold open, qué tiene que cumplir, cierre, Short 3 | todos | 20 |
