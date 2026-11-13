# PCB-A · anclajes al panel

**PASS**. Reproducir: `python3 apps/hardware/kicad/consola/tools/fit/check_pcba_mounting.py`.

Corte de aluminio Ø4,2; puente mínimo exigido 1,5 mm. Distancias conservadoras a rectángulos exteriores de recortes redondeados; círculos exactos. Se ejecuta `panel_cuts` con recorders geométricos, tanto láser como CNC con rebajes.

| H | Panel mm | Puente láser ≥ mm | Puente CNC ≥ mm | K combinado / izq. / der. (mm) | CSV / corte |
|---|---|---|---|---|---|
| H1 | (72.000, 181.000) | 4.200 | 3.900 | 1.200 / 35.147 / 1.200 | 1 / True |
| H2 | (118.000, 192.000) | 7.350 | 7.350 | 18.671 / 21.898 / 16.200 | 1 / True |
| H3 | (144.000, 192.000) | 7.350 | 7.350 | 37.906 / 39.081 / 16.200 | 1 / True |
| H4 | (248.000, 192.000) | 7.350 | 7.350 | 138.483 / 9.200 / 9.200 | 1 / True |

Separadores BSO-M3 de 18: z=65…83; K z=80…81,6. Se cruzan en altura, pero ninguna envolvente Ø5,6 de separador intersecta material de K en XY. H1 es el limitante: 1,2 mm al contorno izquierdo de K. El cuerpo hexagonal nominal de 4,8 entre caras cabe en esta envolvente.

**Límite:** CSV v5 combinado y particiones de doc28 §7; no existen layouts K left/right en esta base. El resultado de izq./der. es sobre sus contornos especificados, no certifica un layout inexistente. Revalidar si cambian contornos, tolerancias o diámetro del proveedor.

H1 anterior (62;183) invade el paso del separador por K y solapa 0,8 mm con TAP en el generador láser. Se movió a (72;181) sin rerutear.

La prueba valida geometría nominal; el ancho de puente no certifica la resistencia de clinchado. Ver [BOM de herrajes](../../../../../design/3d-models/consola/BOM-herrajes.md) para taladro, dureza y distancia a borde exigidos por el proveedor.
