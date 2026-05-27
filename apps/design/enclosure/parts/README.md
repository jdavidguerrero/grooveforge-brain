# GrooveForge Brain — Enclosure Parts Specs

> **Autor:** Industrial Design Agent
> **Version:** 0.1
> **Status:** ACTIVO
> **Fecha:** 2026-05-26
> **Derivado de:** `apps/design/enclosure/01-enclosure-design-spec.md` v0.3

Este directorio contiene los specs modulares de fabricacion del enclosure GrooveForge Brain.
Cada archivo es autocontenido: un modelador CAD puede leer solo su archivo y modelar esa pieza
sin necesitar el spec maestro para cada detalle.

El spec maestro (`01-enclosure-design-spec.md`) es la fuente de autoridad. Estos archivos
lo *derivan* y *citan*, no lo reemplazan. Si hay conflicto, gana el spec maestro.

---

## Indice de archivos

| Archivo | Pieza | Material | Status |
|---------|-------|----------|--------|
| `00-master-parameters.md` | User parameters Fusion 360 (SSoT) | — | PROPUESTO |
| `01-petg-body.md` | Cuerpo PETG impreso (shell + standoffs + rebajes) | PETG dark anthracite | PROPUESTO |
| `02-top-panel-aluminum.md` | Panel superior aluminio CNC con todos los cutouts | Al 6061-T6 anodizado | PROPUESTO |
| `03-cheek-left-guadua.md` | Cheek izquierdo guadua (audio IN) | Guadua angustifolia laminada | PROPUESTO |
| `04-cheek-right-guadua.md` | Cheek derecho guadua (audio OUT + USB) | Guadua angustifolia laminada | PROPUESTO |
| `05-display-bracket.md` | Bracket elevacion ESP32-S3 + display GC9A01 | PETG dark anthracite | PROPUESTO |
| `99-assembly.md` | Ensamblaje completo, fijaciones, orden de montaje | — | PROPUESTO |

---

## Grafico de dependencias entre piezas

Las piezas no son independientes: algunas deben estar definidas (o modeladas) antes que otras.

```
00-master-parameters
        |
        +--------+--------+--------+--------+
        |        |        |        |        |
   01-petg   02-top    03-cheek  04-cheek  05-bracket
    body     panel      left      right
        |        |        |        |
        +--------+--------+--------+
                          |
                    99-assembly
```

### Dependencias detalladas

| Pieza | Requiere definida primero | Motivo |
|-------|--------------------------|--------|
| `00-master-parameters` | Ninguna | Es el SSoT |
| `01-petg-body` | `00-master-parameters` | Usa BODY_W, BODY_D, BODY_H_F, BODY_H_R, WALL_T, etc. |
| `02-top-panel-aluminum` | `00-master-parameters`, `01-petg-body` | Los 4 puntos de montaje van sobre los heat inserts del PETG |
| `03-cheek-left-guadua` | `01-petg-body` | El perfil de altura sigue el loft del PETG; el rebaje de encastre define la anchura interna |
| `04-cheek-right-guadua` | `01-petg-body` | Idem izquierdo |
| `05-display-bracket` | `01-petg-body`, `02-top-panel-aluminum` | Se monta sobre el top panel; el bracket no puede interferir con la cara inferior del aluminio |
| `99-assembly` | Todas las anteriores | Ensamble completo |

---

## Status de modelado por pieza

| Pieza | Spec escrito | CAD en Fusion | Impreso/fabricado | Fit validado |
|-------|:---:|:---:|:---:|:---:|
| `00-master-parameters` | SI | PENDIENTE | — | — |
| `01-petg-body` | SI | PENDIENTE | PENDIENTE | PENDIENTE |
| `02-top-panel-aluminum` | SI | PENDIENTE | PENDIENTE | PENDIENTE |
| `03-cheek-left-guadua` | SI | PENDIENTE | PENDIENTE | PENDIENTE |
| `04-cheek-right-guadua` | SI | PENDIENTE | PENDIENTE | PENDIENTE |
| `05-display-bracket` | SI | PENDIENTE | PENDIENTE | PENDIENTE |
| `99-assembly` | SI | PENDIENTE | PENDIENTE | PENDIENTE |

---

## Orden recomendado de modelado en Fusion 360

El orden va de menor a mayor riesgo de retrabajo:

### Prioridad 1 — PETG body (01)
El PETG body es la pieza madre del ensamblaje. Todo lo demas se referencia a ella.
Modelarla primero y validar su perfil de loft (42mm frontal, 30mm trasero) con una
impresion rapida en PLA (ver §13 del master spec, Fase 1-2).

### Prioridad 2 — Top panel aluminio (02)
El panel no puede mandarse al CNC de Bogota sin estar el PETG body impreso: los 4 puntos
de montaje M3 deben verificarse fisicamente antes de pagar el CNC. Imprimir el panel en PLA
primero (Fase 1 del plan de impresion en §13 del master spec).

### Prioridad 3 — Cheeks guadua izquierdo y derecho (03, 04)
El perfil de altura de los cheeks depende del loft del PETG. Modelar juntos. Los cutouts
de jacks y USB se hacen post-ensamblaje (guiados por el PETG), por lo que el riesgo
de error en el modelo 3D es bajo — el riesgo real es en la manufactura fisica.

### Prioridad 4 — Display bracket (05)
La altura del bracket (BRACKET_H = 15mm) se verifica solo despues de tener el PETG impreso
y el top panel. Cualquier ajuste de altura del bracket no afecta las otras piezas.

### Prioridad 5 — Assembly (99)
El assembly en Fusion sirve para detectar interferencias antes de fabricar. Construirlo
despues de tener los cuerpos sólidos individuales.

---

## Como usar este directorio con Fusion MCP

### 1. Cargar parametros
Ejecutar el script Python de `00-master-parameters.md` §Script Python via `fusion_mcp_execute`.
Verificar que todos los parametros aparezcan en `Modify > Change Parameters`.

### 2. Modelar por pieza
Cada archivo `01` a `05` tiene una seccion "§8 Workflow Fusion 360" con los pasos
ejecutables en orden. Usar `fusion_mcp_execute` para cada operacion.

### 3. Verificar en assembly
Ensamblar en `99-assembly.md`. Usar `fusion_mcp_read` para leer el estado del modelo
y detectar interferencias con `Inspect > Interference`.

### 4. Exportar
Cada pieza tiene una seccion "§7 Export targets" con el path exacto en `apps/design/3d-models/`
y los settings de export. Usar `fusion_mcp_execute` para automatizar el export.

---

## Convencion de naming de archivos exportados

```
apps/design/3d-models/
├── 01-petg-body.f3d          # Fusion archive — historial editable
├── 01-petg-body.step         # Para slicers y verificacion
├── 01-petg-body.stl          # Para impresion directa
├── 02-top-panel.f3d
├── 02-top-panel.step         # Para CNC (Bogota)
├── 02-top-panel.dxf          # Para CNC 2D si el proveedor lo requiere
├── 03-cheek-left.f3d
├── 03-cheek-left.step
├── 04-cheek-right.f3d
├── 04-cheek-right.step
├── 05-display-bracket.f3d
├── 05-display-bracket.step
├── 05-display-bracket.stl
└── 99-assembly.f3d           # Ensamblaje completo con joints
```

Formato de version en nombre de archivo cuando hay revisiones: `01-petg-body_v02.step`.
El archivo sin sufijo de version es siempre la version mas reciente aprobada.

---

## Contacto y proveedores (Bogota)

| Servicio | Proveedor candidato | Tiempo estimado | Costo referencia |
|----------|-------------------|-----------------|------------------|
| CNC aluminio top panel | CNC local Bogota (por confirmar) | 5-7 dias habiles | $6.00/ud @ qty 100 |
| Impresion PETG | Impresora propia o servicio local | 18-22h por unidad | $5.00/ud |
| Guadua laminada | Proveedor local guadua angustifolia | Por lote | $2.50/ud (2 cheeks) |
