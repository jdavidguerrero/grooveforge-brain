# Design

Archivos de diseño industrial del GrooveForge Brain: enclosure 3D, panel frontal,
referencias visuales Figma, renders y specs de fabricación.

## Specs relevantes

- `apps/docs/00-master-strategy.md` §8.2 — Identidad visual y materiales (SSoT)
- `apps/docs/01-architecture.md` §3.1 — BOM con componentes físicos (define cutouts del panel)
- `apps/docs/01-architecture.md` §3.3 — Pin mapping (qué conectores van en qué panel)

## Stack

- CAD 3D: Fusion 360 / FreeCAD / Onshape (cualquiera que exporte STEP)
- Figma para referencias UI, iconografía y renders 2D
- AI image gen para concept renders rápidos

## Estructura

```
figma-refs/      # Referencias Figma: panel layout, brand assets, renders 2D
3d-models/       # CAD 3D: STEP (ensamblaje + piezas), DXF (CNC), STL (impresión)
```

## Materiales (00-master-strategy.md §8.2)

| Pieza | Material | Fabricación | Costo BOM |
|---|---|---|---|
| Top panel | Aluminum 2mm 6061-T6, anodizado anthracite | CNC local Bogotá | $6.00 |
| Cuerpo | PETG dark color-stable | Impresión 3D FDM | $5.00 |
| Accent panel | Guadua colombiana, sellada | Local sourcing + acabado | $2.50 |

**No exceder $13.50 total en enclosure.**

## Identidad visual

- Colores: **dark anthracite** (cuerpo) + **green/teal** (accents) + **purple** (AI accents)
- Tipografía: Satoshi + General Sans
- Vibe: boutique craft + technical precision + Latin American identity
- La guadua es el elemento de identidad único — no opcional

## Agente recomendado

Invocar **Industrial Designer** para todo lo de este directorio.
Invocar **Firmware Engineer** solo si hay cambios de layout que afecten pin mapping.
