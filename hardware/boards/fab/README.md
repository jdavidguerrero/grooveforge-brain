# Pedido a JLCPCB — placas del panel GroovePush

Generado por `tools/export_fab.py` (2026-10-22, tras la auditoría; ver `../AUDIT_RESPONSE.md`).

| Placa | Gerber | Ensamble | BOM | CPL |
|---|---|---|---|---|
| PCB-A | `pcb-a/pcb-a-gerbers.zip` | Economic, cara **inferior** | `pcb-a-bom.csv` (8 líneas) | `pcb-a-cpl.csv` / `-cpl-alt-bottom.csv` |
| PCB-F | `pcb-f/…` | Economic, inferior (opcional: solo placa) | 2 líneas | idem |
| PCB-K1 | `pcb-k1/…` | Economic, inferior | 7 líneas | idem |
| PCB-K2 | `pcb-k2/…` | Economic, inferior (opcional) | 4 líneas | idem |
| PCB-S | `pcb-s/…` | **Sin ensamble** (solo placa; todo a mano) | — | — |

Placas de 2 capas, 1.6 mm. THT a mano en todas: EC11, RS6011, Choc V2, IDC de PCB-A, headers y JST-PH de PCB-S.

## Piezas *extended* (cargo extra por pieza distinta y por diseño)

| Pieza | LCSC | Placas |
|---|---|---|
| SK6812MINI-EA | C5378731 | K1, K2 |
| MCP23017T-E/SS (SSOP-28) | C558584 | A, K1 |
| MCP23017-E/SO (SOIC-28W) | C47023 | K1 |
| JST SM04B-SRSS-TB / SM06B / SM10B | C160404 / C160405 / C160409 | A (4, 6, 10), F (6), K1 (10), K2 (10) |

Todo lo demás es *basic*: 100 nF C14663, 10 nF C57112, **2.2 kΩ C4190 (nuevo, antes 4.7 kΩ)**, 10 µF C15850,
47 µF C96123, 10 kΩ C25804. Stock verificado con el MCP de JLC durante esta sesión.

## Antes de pagar

1. **Rotación**: elegir entre `*-cpl.csv` y `*-cpl-alt-bottom.csv` con la vista 3D del ensamble (LED, MCP, JST).
   Criterio y lista de piezas en `../AUDIT_RESPONSE.md`.
2. Confirmar que todo el ensamble está en la cara **Bottom** y que no falta ninguna pieza del BOM.
3. Pedir las placas juntas con las de la portadora del Brain: un solo envío.
