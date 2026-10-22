# Evidencia de la auditoría delta

Revisión `0d256bf`, 2026-10-22. Solo lecturas y exportaciones; ninguna modificación de diseño.

- `measurements.json`: distancias, referencias de retorno, candidatos vía-en-pad y thermal reliefs.
- `*-board.json`: pads, pistas y zonas de la revisión auditada.
- `portadora.xml`: netlist exportada del esquema actual.
- `fab-comparison.json`: 11/11 coincidencias CAM por placa; BOM/CPL comparados por identidad, valores, coordenadas y cara, no por el modelo final de JLC.
- `sha256-inputs.json`: commit y hashes auditados.
- `portadora-front/back.svg/png`: cobre y contorno guardados.

Para repetir desde la raíz del repo, ejecutar `inspect_board.py` y `measure.py` con Python de KiCad, después `check_fab.py` con Python normal. Los scripts escriben evidencia junto a sí mismos y regeneran CAM en `regenerated/`, nunca en `fab/`. `detail.py` conserva la comprobación puntual original y usa rutas temporales: es evidencia de apoyo, no el comando recomendado de repetición.

Límites: el muestreo de GND usa únicamente polígonos de la cara opuesta; no calcula campos, impedancia ni distribución real de corriente. La búsqueda de vía-en-pad identifica centros contenidos en el pad: no garantiza detectar todo solape parcial. El llenado usado es el guardado y exportado; no se realizó un nuevo DRC en esta ampliación. Los scripts no sustituyen un verificador CAM/DFM del ensamblador.
