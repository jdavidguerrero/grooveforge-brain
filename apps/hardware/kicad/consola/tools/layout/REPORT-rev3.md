# Correcciones de revisión adversarial — rev3

2026-11-13. Informe de entrada: `/private/tmp/grooveforge-review-pcb.md`.
No se modifican las PCB de K izquierda/derecha, sus generadores build/route/finish, PCB-M ni el CSV de contornos.

## Cambios y comprobaciones

| Hallazgo | Corrección y evidencia |
|---|---|
| I05 · F | Añadidos 6 campos LCSC (C1–C5/J1) y 8 campos Supply/SupplyDescription (RV1–RV4), ocultos. Comparación con Python de KiCad: 50 campos de 10 componentes, **0 diferencias**; 40 pads coinciden con XML recién exportado; conectividad API: **0 sin conectar**. |
| M01 · P | 14 textos con altura ≥1,0 mm y trazo ≥0,15 mm; 150 contornos de serigrafía con ancho ≥0,15 mm. El generador conserva estos mínimos. DRC CLI completo: **0 violaciones, 0 sin conectar, 0 paridad**. |
| M02/I01 · K y mecánica | README de ambas K: layout terminado, DRC de entrega, fabricación local, transformación `(x+44,371−y)` y Choc V2 soldadas sin zócalos, D-K1b. README/doc28: colas JST PH THT ≤0,8 mm, incluido metal/soldadura, con medición individual de altura; luz nominal 1,4 mm. Herrajes: cuatro anclajes M4 del CSV y estado actual de K. Informe de A: hashes vigentes y anclaje H1 actualizado. |
| I07 · ESD | Propuesta pendiente para Juan en BOM de herrajes y doc24: un único terminal de ojo bajo PEM en aluminio con contacto eléctrico real, cable corto a pad GND de PCB-M y continuidad <1 Ω. No se registra como decisión ni como medición efectuada. |
| M03 · A | C10–C25: C57112 → C1589, 10 nF 0603 X7R. BOM, esquema, campos de PCB y XML sincronizados; catálogo y pedido identifican la sustitución de compra v1 (50 unidades; lote físico a registrar al recibir). Manifiesto e integridad de A actualizados. DRC CLI nuevo: **0 violaciones, 0 sin conectar, 0 paridad**. |
| M04 · Pedido | Dimensiones A 217×29,6; K izq. 102,2×216; K der. 144×152; P 223×31; F 81×83; M 186×80 mm. Precios identificados como históricos hasta recotizar. |

La comparación estructural de las PCB de F/P/A frente al inicio de esta tarea conserva todos los elementos
excepto los campos indicados y los tamaños/trazos de serigrafía de P: **sin cambios de cobre, rellenos,
redes, posiciones, taladros ni contornos de placa**. Los archivos excluidos se cotejaron por SHA-256.

## Fabricación

Exportación P con las opciones originales de `layout_p.py`:

```sh
kicad-cli pcb export gerbers -l F.Cu,B.Cu,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts -o <temporal>/ pcb-p.kicad_pcb
kicad-cli pcb export drill --excellon-separate-th --generate-report --report-path <temporal>/pcb-p-drill-report.txt -o <temporal>/ pcb-p.kicad_pcb
```

Sin sustracción de máscara ni marcas de taladro en Gerber, como en la entrega previa.
Comparación con los originales retirando únicamente fechas/comentarios de generación y checksum:
solo difieren funcionalmente F/B_Silkscreen de P. Se conservan los demás archivos idénticos; ZIP reconstruido
con 7 Gerbers + 2 Excellon, 9 miembros planos, CRC correcto e iguales a los archivos sueltos.
Actualizados SVG de montaje superior/inferior, DRC y los 30 hashes del manifiesto de P, incluido el catálogo.
STEP y dibujos de geometría se conservan al no cambiar la mecánica.

F: reexportados a carpeta temporal 9 Gerbers y los dos Excellon con las opciones de `layout_f.py`;
coinciden con los existentes tras retirar fechas de generación/checksum. El job también coincide salvo fecha.
**No se sustituyen los archivos fab de F**. A conserva sus archivos fab: solo cambia el hash de PCB
registrado en el manifiesto por el campo LCSC oculto, sin cambios de fabricación.

## Verificación y límites

```sh
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb drc --all-track-errors --schematic-parity --severity-all --format json --exit-code-violations -o <temporal>/drc.json <placa>.kicad_pcb
python3 apps/hardware/kicad/consola/tools/check_bom.py --board <placa>
pytest -q apps/hardware/kicad/consola/tools
```

Suite completa: **148 tests pasados, 1 omitido**, sin fallos.

`check_bom` se ejecutó para pcb-a, pcb-f, pcb-p, pcb-k-left y pcb-k-right: **0 errores de identidad y
0 compras sin verificar** en cada una, incluidos los 6 sockets de montaje adicionales de P.

**Limitación I05:** el CLI DRC de F aborta en este entorno con SIGABRT/salida 134 y no produce un JSON nuevo,
también después de sincronizar los campos. No se atribuye una causa ni se presenta el reporte antiguo como
una nueva certificación. La alternativa se ejecutó con
`/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3`,
leyendo la PCB con `pcbnew.LoadBoard`, sus campos y pads, el esquema y un XML nuevo, además de
`BuildConnectivity()` / `GetUnconnectedCount(False)`. Resultado: paridad de campos 0 y sin conectar 0;
**el DRC físico completo de F queda sin recertificar**.

No se repite el DRC de K ni se valida PCB-M. La comprobación de serigrafía de P cubre los mínimos solicitados
de altura/trazo; `min_silk_clearance` sigue en 0 en el proyecto, por lo que el DRC no certifica un margen global
tinta–pad de 0,15 mm. No se ensayan piezas físicas, continuidad de chasis, CAM ni cotización.
Los restantes hallazgos del informe original conservan sus condiciones de validación.
