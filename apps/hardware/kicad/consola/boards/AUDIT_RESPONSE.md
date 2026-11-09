# Respuesta a la auditoría de preproducción (2026-10-22)

Responde a `hardware/audit-2026-10-22/AUDITORIA_DELTA.md`. Las cinco placas (A, F, K1, K2, S) se
regeneraron y pasan **DRC 0/0/0 con paridad de esquemático, ERC 0 y 0 vías sobre pads SMD**. Los archivos
nuevos para JLC están en `hardware/boards/fab/` (ver `fab/README.md`).

La auditoría resultó correcta en lo importante: D01 era un defecto real y lo causaba mi router. Dos hallazgos
no los cerré en el PCB (D02, D05) y dejo escrito por qué.

## Resumen por hallazgo

| ID | Estado | Qué se hizo |
|---|---|---|
| **D01** vías dentro de pads SMD | **Corregido** | `grid_route.py` ya no coloca vías en pads SMD de su propia red (centro ≥ 0.5 mm del borde). Nuevo `check_via_in_pad.py` en cada build. A: 15 → 0, K1: 27 → 0. Con el verificador de la propia auditoría: 28 centros y 39 invasiones de taladro → 0 y 0. |
| **D02** apoyos mecánicos K1/K2/A | **No es cambio de PCB; pendiente de validar** | Las tiras no tienen espacio entre teclas (paso 19.5 mm, tecla de 18 mm) ni margen, de unos 3 mm hasta el borde, para un agujero M2.5 con su arandela. Según el diseño, la carga la lleva la placa frontal que captura cada Choc y los encoders atraviesan el panel (agujero de 7.1 mm). Validar con el conjunto real: ajuste 1:1, par de tornillos, pulsación en los extremos. Si falla, la solución es en enclosure (más postes), no en estas placas. |
| **D03** ESD / mandos | **Corregido en PCB** | Los pads de montaje (MP) de los 8 EC11 estaban sin red: ahora van a GND (símbolo `RotaryEncoder_Switch_MP`). Todos los MP de encoders y faders usan unión **sólida** al plano (antes térmica, con un spoke). Falta: unión panel→plano en el enclosure y medirla. |
| **D04** MLCC cerca de bordes/tornillos | **Corregido** | K1.C3 (47 µF): 1.7 → 7.4 mm del borde. K1.C24: 1.1 → 5.8 mm (pasa al otro lado de su tecla). K2 C10–C14: 1.6 → 2.7 mm y giradas 90° (eje largo atravesando la placa). F.C5: 3.2 → 13.1 mm del centro de H3. K2 no admite más: la placa mide 18.7 mm y por ahí pasan unas 9 pistas. |
| **D05** solapes SDA/SCL entre caras | **Aceptado, no corregido** | Medido sobre el ruteo actual: K1 SDA (F.Cu) sobre SCL (B.Cu) **87 mm** (la versión auditada tenía 52); A ENC2_B sobre ENC4_A 78 mm. Estimación: con FR4 de 1.6 mm y pistas de 0.2 mm son ~1.5 pF de acoplamiento; con 2.2 kΩ y flancos de ~100 ns da un pico de ~0.1 V, frente a un umbral de ~1 V. Es una estimación, no una medida. Las líneas de encoder pasan por filtros de 10 nF. Pasar a 4 capas lo resolvería pero no está justificado sin una medida: se mide en la primera unidad (flancos de SDA/SCL con LEDs activos). |
| **D06** desacoplo lejos del VDD | **Mejorado** | Nuevo `place_decap`: A.U1–C1 6.6 → 2.3 mm; K1.U1–C1 6.6 → 2.3 mm; K1.U2–C2 12.5 → 5.4 mm. U2 no baja más: lo bloquea el LED D4 de UNDO. |
| **D07** margen de I²C | **Corregido** | R1/R2 de 4.7 kΩ → **2.2 kΩ** (C4190, basic, 6.1 M en stock). Límite de capacidad sube de ~75 pF a ~160 pF a 400 kHz; I_OL = 1.5 mA, dentro de lo que admiten los MCP23017. Firmware: arrancar a 100 kHz. |
| **D08** alimentación / hot-plug | **Documentado** | Contrato en `CONNECTORS.md`: conectar con el equipo apagado, cables polarizados, la única limitación de corriente es la de la portadora (polyfuse 3 A). No se añade eFuse ni bloqueo inverso a estas placas. |
| **D09** fiduciales / panel | **No aplica en Economic** | JLC Economic no exige fiduciales ni rieles. Se revisa solo si se pasa a otra línea. |

## Lo que la auditoría descarta y comparto

- **Orientación de los LED**: las hojas Rev A y Rev B del SK6812MINI-EA numeran distinto, pero la función por
  esquina es la misma (chaflán inferior derecho = GND). La huella respeta Rev B: 1 GND, 2 DIN, 3 VDD,
  4 DOUT. La huella de la librería de JLC (EasyEDA Official Library) coincide.
- **No** se separó AGND/DGND ni se cambiaron dieléctricos (sin ruta de audio en estas placas).

## Lo que queda abierto antes de pagar: rotación del CPL

Esto no lo puedo cerrar sin el visor de ensamblaje de JLC y es lo único que puede dañar placas ya montadas.

La herramienta comunitaria JLCKicadTools (que sigue el comportamiento de JLC) aplica a las piezas de la cara
inferior `rotación = 180 − rot`, y mi exportación usa `rot`. Las dos coinciden para piezas a 90°/270° y difieren
180° en las de 0°/180°. Por eso `fab/<placa>/` trae **dos CPL**: `*-cpl.csv` y `*-cpl-alt-bottom.csv`. Las piezas
polarizadas que cambian son:

| Placa | Piezas polarizadas que difieren |
|---|---|
| K1 | 15 LED (D1–D15), MCP U2 (SOIC), JST J1, J2 |
| K2 | 5 LED (D1–D5), JST J1 |
| A | JST J2, J3 |

Cómo decidir: subir una placa con el primer CPL y, en la vista 3D del paso de ensamble, comprobar para un LED
y para el MCP que el chaflán/punto de pin 1 cae sobre el pad 1 de la huella. Coordenadas del pad 1 de cada
pieza: `hardware/audit-2026-10-22/assembly-polarity.csv`. Si no coincide, usar el segundo CPL. Para los LED, pin 1
es GND y su pad es el que conecta con la vía de GND más cercana.

## Archivos tocados

- Placas: `pcb-a`, `pcb-f`, `pcb-k1`, `pcb-k2` (`.kicad_sch` solo A; `.kicad_pcb` las cuatro), `pcb-s` sin cambios.
- `tools/`: `grid_route.py` (vías en pads; contorno rectangular y arcos; ventanas Edge.Cuts), `boardlib.py`
  (`place_decap`, `place_free`), `check_via_in_pad.py` (nuevo), `add_gnd_pours.py` (MP sólidos),
  `finish_routing.py`/`build.sh` (orden de ruteo, pasadas de GND), `export_fab.py` (segundo CPL).
- Un fallo propio hallado en el camino: el router leía los contornos rectangulares (F, K2) como una sola
  diagonal. No afectaba a las placas entregadas, solo a reconstruirlas.
