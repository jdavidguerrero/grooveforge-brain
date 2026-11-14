# Ensamble manual — PCB-K izquierda/derecha y PCB-A

2026-11-12 · opción B de Juan. Base `6747775`, rama `claude/monorepo-groovepush`.
Se continuó el trabajo que ya estaba en este clon tras la interrupción de internet.
Teoría y límites: [THEORY-mano.md](THEORY-mano.md). Spec actualizado: [28 §3 y §6](../../../../docs/consola/28-pcb-k.md).

## Cambios entregados

- Huella mixta `Kailh_Choc_V2_PG1353_Hotswap`: se conservan los dos pads SMD
  inferiores del CPG135001S30 y se añaden contactos PTH Ø1,30, pad Ø1,90, con
  los mismos números/redes 1 y 2. Coordenadas de panel, vista superior y +y atrás:
  (0; −5,90) y (5,00; −3,80). KiCad local invierte Y: (0; +5,90), (5,00; +3,80).
  Postes NPTH Ø5,00 en (0; 0) y Ø1,50 en (−5,00; +5,15) de panel conservados.
  Centro de recorte LED conservado en (0; +4,93) de panel; el recorte Edge.Cuts
  pertenece a Dn y no se duplica en SWn.
- Anillo radial: (1,90 − 1,30) / 2 = **0,30 mm**, cumple ≥0,15.
  Cada SMD mide 2,60 × 2,60; sus centros x son −3,275 y +8,275.
  Distancia mínima PTH–SMD: 3,275 − 1,30 − 0,95 = **1,025 mm**, cumple ≥0,20.
  El test también comprueba los pares cruzados con una distancia conservadora
  círculo–rectángulo y la separación al poste central. Los pads de un mismo número
  deben unirse con cobre en el layout; el número compartido solo establece la red.
- LED **SK6812MINI-EA C5378731** en ambos esquemas y en la huella local regenerable.
  Hoja OPSCO `SK6812MINI-EA-000`, revisión B/2, 2026-07-23, pp.4–5:
  cuerpo 3,2 × 2,8 × 1,78 mm; **1 GND, 2 DIN, 3 VDD, 4 DOUT**.
  Se conserva el símbolo KiCad MINI-E por tener ese pinout; Value y LCSC identifican EA.
  La huella inversa parte de KiCad 10.0.6, con pads 1/2 a x = −2,725, pads 3/4
  a x = +2,725, filas y = ±0,75 y pads 1,35 × 0,82 mm. El hueco tiene envolvente
  3,4 × 3,0 mm, frente al cuerpo 3,2 × 2,8. Colocar Dn en B; no reflejar además
  su numeración a mano. La librería y su generador contienen el mismo resultado.
- Pasivos de PCB-K en 0805, salvo C1 22 µF en 1206. `Symbol.fields` permite
  exportar el campo LCSC oculto sin cambiar el cableado ni los UUID del esquema.
- PCB-A conserva su diseño existente: R1/R2 y J4 **DNP** tanto en esquema como
  en PCB. U1 pasa de C558584 a **C506653**, Value `MCP23017-E/SS`, manteniendo
  su huella SSOP-28. Los demás pasivos ya tenían campos LCSC; se conservaron.
  Se registró como placa existente en `check_netlist.py`, sin fábrica generadora:
  incluso con `--regenerate` solo se exporta y verifica PCB-A.

| PCB-K: valor / pieza | LCSC | Encapsulado |
|---|---|---|
| 100 nF | C49678 | 0805 |
| 1 µF | C5137478 | 0805 |
| 22 µF | C12891 | 1206 |
| 10 kΩ | C17414 | 0805 |
| 33 Ω | C17634 | 0805 |
| MCP23017-E/SS | C506653 | SSOP-28, 5,3 × 10,2, paso 0,65 |
| SK6812MINI-EA | C5378731 | Montaje inverso |
| R2/R3 2,2 kΩ, DNP | C17520 | 0805 |

## BOM exportadas

CSV en `../fab/`, una fila por referencia y columnas exactas
`Reference,Value,Footprint,LCSC,DNP`; se incluyen las referencias DNP.

| Archivo | Referencias | DNP |
|---|---:|---|
| `bom-pcb-k-left.csv` | 78 | R2, R3 |
| `bom-pcb-k-right.csv` | 43 | R2, R3 |
| `bom-pcb-a.csv` | 37 | R1, R2, J4 |

Exportación real de KiCad, para cada nombre `pcb-k-left`, `pcb-k-right`, `pcb-a`:

```sh
kicad-cli sch export bom --fields Reference,Value,Footprint,LCSC,DNP \
  --labels Reference,Value,Footprint,LCSC,DNP \
  --output apps/hardware/kicad/consola/fab/bom-<nombre>.csv \
  apps/hardware/kicad/consola/boards/<nombre>/<nombre>.kicad_sch
```

Los tests reexportan las tres BOM y las comparan byte a byte con los CSV entregados.
PCB-A mantiene 100 nF 0603 C14663, 16 × 10 nF 0603 C1589, 10 µF **0805** C15850
y pull-ups 0603 C4190 DNP. La nota de compra que agrupa los pasivos de PCB-A
como 0603 no describe C2: para ese componente manda su huella existente 0805.
No se cambia ese layout a 0805/1206 de PCB-K.

LCSC vacío identifica piezas sin código de compra confirmado: PCB-A H1–H4
(agujeros, no componentes), J1 IDC y RE1–RE8 (encoders de Juan); PCB-K SWn
(teclas/zócalos comprados aparte), conectores y testpoints. No se inventaron códigos
ni se incluyeron como chips de LCSC. J2/J3/J4 de PCB-A conservan sus códigos existentes;
J4 queda excluido del montaje por DNP.

## Verificaciones ejecutadas

Python 3 y KiCad CLI **10.0.6**, ejecución retomada el 2026-11-12.

```sh
TMPDIR="$PWD/.work-mano/resumed" \
PYTEST_ADDOPTS="--basetemp=$PWD/.work-mano/resumed/pytest" \
python3 -m pytest -q apps/hardware/kicad/consola/tools/tests

TMPDIR="$PWD/.work-mano/resumed" \
python3 apps/hardware/kicad/consola/tools/check_netlist.py \
  --all --directory .work-mano/resumed/verification
```

**pytest: 55 passed in 43.74s**, sin skips ni fallos. **check_netlist --all: exit 0**.

| Placa | Componentes | Pines exportados | Redes | Conectividad | ERC errores | ERC avisos |
|---|---:|---:|---:|---|---:|---:|
| PCB-M | 122 | 415 (23 NC) | 103 con nombre | PASS | 0 | 7 |
| PCB-K izquierda | 78 | 239 (8 NC) | 58 con nombre | PASS | 0 | 0 |
| PCB-K derecha | 43 | 127 (5 NC) | 32 con nombre | PASS | 0 | 0 |
| PCB-F | 10 | 32 | 6 con nombre | PASS | 0 | 0 |
| PCB-P | 12 | 24 | 12 con nombre | PASS | 0 | 0 |
| PCB-A existente | 37 | 166 | 48 exportadas, incluidas NC | PASS, contrato congelado | 0 | 0 |

Para respetar los archivos reservados a la otra tarea, `--all` se ejecutó sobre
copias de las seis placas actuales en `.work-mano/resumed/verification`, sin
regenerarlas. En las copias K/F/P se resolvieron las URI `${KIPRJMOD}` de
`fp-lib-table` a las librerías reales del clon: trasladar un proyecto cambia la base
de esas rutas. El primer pase había producido 34/18/4/6 avisos de librería ausente
en K izq./der./F/P; al resolver las rutas, el pase final dio los valores de la tabla.
No se desactivó ningún check ERC. PCB-M conserva cinco avisos `isolated_pin_label`
para I2S y dos para `LINE_IN_L_R`/`LINE_IN_R_R`; KiCad también avisa de anotación
durante su exportación. Se reportan, no se corrigen dentro de esta tarea.

Los XML/PDF/ERC entregados están en `boards/pcb-k-left/docs`, `pcb-k-right/docs`
y `pcb-a/docs`. Los logs completos retomados quedan localmente en
`.work-mano/resumed/{pytest-result,netlist-final-result}.txt`; el scratch está excluido
solo en `.git/info/exclude` para no incorporarlo al patch con `git add -A`.

PCB-A: se contrastó el esquema de la base `6747775` con el contrato congelado
`pcb-a-connectivity.json` y con su XML actual: mismos 37 componentes y mismos
166 pares pin/red. Una comparación estructural completa de ambos archivos KiCad
contra la base dio igualdad exacta al permitir únicamente DNP de R1/R2/J4 y
Value/LCSC de U1. Por tanto, no se movieron huellas ni cambiaron pistas, vías,
zonas, contorno, pines o conexiones. El test de mutación también rechaza una
alteración deliberada de un pin del XML.

## PCB-A: verificar contra el panel v1 antes de fabricar

Se enumera la revisión pendiente; no se trasladó ni rediseñó PCB-A.

1. Usar `panel/panel-coordinates-console-v1.csv`, no el antiguo `panel-coordinates.csv`.
   E1–E8 deben quedar en **x = 79, 105, 131, 157, 183, 209, 235, 261; y = 192 mm**,
   paso **26 mm**, vista superior de panel. La PCB actual conserva orígenes RE1–RE8
   x = 27,5…209,5, y = 176,5 en KiCad. Su eje está en el offset local (+7,5; +2,5):
   centros de eje x = 35…217, y = 179. El paso sí coincide. Como registro candidato
   para CAD, `x_panel = x_KiCad + 44`, `y_panel = 371 − y_KiCad` alinea esos ocho ejes;
   es una transformación para verificar el ensamble, no una edición de la placa.
2. Superponer contorno y taladros H1–H4 con cuerpo, soportes y las dos mitades de
   PCB-K. El contorno existente mide 217 × 69,6 mm y tiene un brazo hasta y = 240
   de KiCad. Revisar especialmente ese brazo, la M4, el soporte de pantalla y el
   espacio de sus conectores/cables. Confirmar que los apoyos no fuerzan la placa.
3. Medir el EC11 real: patrón de cinco pines y anclajes, posición del eje, longitud
   útil (huella actual nominal H20), tuerca/arandela y carrera del clic. Verificar
   agujeros de panel Ø7,1 y perillas Ø18 con el aluminio de 2 mm y la altura real
   de separadores. Un paso correcto no certifica la variante mecánica del encoder.
4. Confirmar acceso del cautín a 0603, U1 SSOP y conectores inferiores; holgura
   entre soldaduras/patas y otras placas, aislamiento de tornillos y radios de
   curvatura de IDC/JST. J4 no lleva conector ni cable a la antigua PCB-K1.
5. Confirmar orientación y pin 1 de J1/J2/J3, cables y continuidad contra PCB-M
   y PCB-F; no asumir que dos conectores del mismo tamaño tienen igual pinout.
   R1/R2 permanecen DNP: pull-ups activos solo en PCB-M (D14).
6. Tras validar el ensamble, ejecutar DRC y revisar Gerbers/taladros del diseño
   existente antes de pedir. Estos resultados de esquema no actualizan ni validan
   los paquetes de fabricación antiguos de `boards/fab/pcb-a`.

## Límites y fuentes

Actualización 2026-11-13: ambas K tienen layout terminado y fabricación en su `fab/` local;
el DRC de entrega tiene 0 errores. **D-K1b: Choc V2 soldadas directamente, sin zócalos.**
Los pads SMD históricos quedan sin poblar: los PTH Ø1,3 no validan el alojamiento del
CPG135001S30, que requiere pasos Ø3. Probar orientación del LED, recorte de chapa y
soldadura directa de la tecla en el cupón; los README vigentes incluyen el recorte de colas JST.
No se fabricó ni se hizo una prueba eléctrica física de ninguna placa aquí.

- [LED EA, hoja OPSCO B/2, pp.4–5](https://datasheet.lcsc.com/datasheet/pdf/7a14970dbcd694dcd129b083b19b0911.pdf?productCode=C5378731).
- [C506653: MCP23017-E/SS, SSOP-28](https://jlcpcb.com/partdetail/MicrochipTech-MCP23017_ESS/C506653).
- `../fab/pedido-v1.md`, doc 28 y CSV del panel v1; geometría PG1353 indicada por Juan.
- Huella LED de las librerías KiCad 10.0.6; [licencia](https://www.kicad.org/libraries/license/).

No se modifican `pcb_m.py`, `boards/pcb-m/`, doc 24 ni `tools/check_harness.py`.
Sin push. Entrega mediante patch binario respecto de `6747775`.
