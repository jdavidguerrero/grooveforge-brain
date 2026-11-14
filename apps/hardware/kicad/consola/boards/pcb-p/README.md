# PCB-P — pads de fuerza Console

Placa pasiva de 223 × 31 × 1,6 mm, ensamblada a mano. Seis discos FSR402 sobre la placa;
colas por ranuras NPTH a seis sockets HCTL C2897365 inferiores. Se conservan seis JST-PH
J1–J6 de 2 vías y los doce nets independientes del mazo aprobado.

Especificación y cálculo: `docs/THEORY-layout.md`. Resultado y límites: `../../tools/layout/REPORT-p.md`.
Recrear layout con Python de KiCad: `../../tools/layout/layout_p.py`.
La terminación inferior reemplaza la soldadura directa D43.

`placement.csv`: coordenadas de panel, seis datums de disco F. `docs/layout-targets.json`: datums pin 1 de FSR/J B.
Conversión: x_panel=x_KiCad+44; y_panel=371−y_KiCad.
Gerbers y taladros en `fab/`; montaje 1:1 y comprobaciones en `docs/`.

Los identificadores FSR1–FSR6 del esquema se conservan para los landings de sus sockets inferiores.
La huella local reemplaza la geometría D43; su nombre histórico no indica soldadura directa.
La compra de sockets está en `assembly-sockets.csv`, validada con el mismo catálogo y comprobador BOM.
