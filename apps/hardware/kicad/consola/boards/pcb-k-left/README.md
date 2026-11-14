# PCB-K izquierda — GrooveForge Console

Layout terminado de dos capas y 1,6 mm; DRC entregado con **0 errores, 0 sin conectar y 0 diferencias de paridad**.
Los avisos de huella/serigrafía y las comprobaciones físicas pendientes se detallan en
[REPORT-k](../../tools/layout/REPORT-k.md). Este estado no equivale a aprobación CAM ni a validación del lote real.

La placa guardada es `pcb-k-left.kicad_pcb`; los Gerbers, taladros y el ZIP para PCB desnuda están en
[fab/](fab/). Usar esta entrega local, junto con su DRC en `docs/`, como fuente de fabricación.
`docs/` contiene además XML, PDF y ERC. Ver [doc 28](../../../../../docs/consola/28-pcb-k.md) para montaje.

`placement.csv` usa mm de panel, vista superior, +y hacia atrás. La transformación del layout es
**x_panel = x_KiCad + 44; y_panel = 371 − y_KiCad**. Rotaciones CCW KiCad; F = superior, B = inferior.
El LED queda a +4,93 mm del centro de tecla en y_panel. Las demás piezas van abajo.
Los soportes y los cuatro anclajes del marco M4 están definidos en el CSV de contornos y doc 28 §2.

## Montaje

**Choc V2 SOLDADAS directamente, sin zócalos**, según doc 28 **D-K1b** (Juan, 2026-11-13).
Las filas históricas key/socket de igual referencia representan una única huella mixta;
no comprar ni montar zócalos CPG135001S30. Los pads SMD asociados quedan sin uso.

Los JST PH THT **J1 y J2** van en la cara B: **recortar las colas a ≤0,8 mm sobre la cara de soldadura**
(cara superior, hacia la chapa). El máximo incluye todo metal y soldadura. Verificar cada pin con
calibre de altura o galga apoyada en el FR4 después de soldar y recortar; eliminar rebabas y comprobar
que ningún pico exceda 0,8 mm. Registrar la altura máxima antes de montar contra el aluminio.
La luz nominal a la chapa es solo **1,4 mm**, por lo que quedan **0,6 mm nominales**; comprobar también
la separación final con las tolerancias del apilado. No aceptar contacto con la chapa.

Para verificar el esquema desde la raíz: `python3 apps/hardware/kicad/consola/tools/check_netlist.py --board pcb-k-left`.
