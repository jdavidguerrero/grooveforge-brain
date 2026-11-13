# PCB-A — informe de entrega de la tira

Fecha: 2026-11-13. Base: `edc67a3`. Rama: `claude/monorepo-groovepush`.
Revisión actual: PCB-A, panel, herramientas de fit y BOM de herrajes; sin commit ni push.

**Resultado: DRC CLI PASS (0 errores, 0 advertencias, 0 sin conectar), integridad PASS y fit PASS.**
Gerber, taladros, SVG y ZIP generados de la placa final. El montaje físico sigue pendiente.

## Geometría final

- Dos capas, FR4 de 1,6 mm; contorno nominal **217 × 29,6 mm**, esquinas R1.
- KiCad: x=10…227; y=166,4…196. Panel: x=54…271; y=175…204,6.
- Conversión fija: `(x+44, 371−y)` mm. Error de los ocho ejes **0,000000 mm**;
  y panel=192; paso **26,000 mm** en los siete intervalos.
- J1 IDC 2×15 inferior y paralelo a X; origen (207,78;192,07), +90°. J2 SH4,
  J3 SH6, MCP23017 U1 y pasivos dentro de la tira. Todos los courtyards están
  dentro del contorno, comprobados con la API de KiCad.
- R1/R2/J4 **DNP**, con sus redes y cobre conservados.

| Agujero | KiCad mm | Panel mm | Taladro |
|---|---|---|---|
| H1 | 28;190 | 72;181 | NPTH Ø3,2 mm |
| H2 | 74;179 | 118;192 | NPTH Ø3,2 mm |
| H3 | 100;179 | 144;192 | NPTH Ø3,2 mm |
| H4 | 204;179 | 248;192 | NPTH Ø3,2 mm |

Las coordenadas están también en `Cmts.User` y [SVG de montaje](docs/pcb-a-mounting.svg).
El IDC se separó 0,8 mm del MP pasante de RE7; H4 se llevó al hueco E7/E8 para
liberar RE8, y R2 se apartó del MP de J2. La huella IDC local adapta únicamente la
marca de pin 1 en serigrafía; cobre, taladros, cuerpo y courtyard siguen iguales.
El cotejo de biblioteca se mantiene activo y no detecta diferencias con la instancia.

## Revisión de anclajes al panel (base 0ef531f)

H1 pasa a KiCad (28;190), panel (72;181), sin rerutear: se conservan las 764 pistas/vías.
Se rellenaron ambos planos y se reexportaron Gerber, PTH/NPTH, ZIP y 14 SVG.
No existe un STEP de PCB-A en esta base, por lo que no hay STEP de A que actualizar.
El SVG de montaje y las cuatro filas `PCB-A,agujero` del CSV concuerdan; el generador
usa Ø4,2 para BSO-M3. El DXF láser también contiene estos cuatro círculos;
STEP/FCStd históricos del panel requieren reconstruir desde el script (no son un STEP de PCB-A).
Separadores **18 mm útiles**, no 10: 83−18=65=TOP−20.

El [chequeo de montaje](../../tools/fit/REPORT-pcba-mounting.md) da puentes láser
≥4,2 / 7,35 / 7,35 / 7,35 mm; en CNC con rebajes, H1 conserva ≥3,9 mm.
Con cuerpo envuelto en Ø5,6, no hay choque con K combinado ni sus particiones
izquierda/derecha de doc28 §7. Los separadores sí atraviesan la altura de K (z80…81,6),
pero sus posiciones no atraviesan material de K; luz mínima H1=1,2 mm.
No existen layouts de K izq./der. en esta base: son los contornos especificados.

La paridad completa detectó 18 campos de suministro ausentes ya en la base:
`Supply` y `SupplyDescription` de J1 y RE1–RE8. Se copiaron del esquema como campos
ocultos; esquema, pads, redes y cobre de pistas permanecen idénticos. Esto elimina
las discrepancias sin exclusiones ni cambios de severidad. El CLI de KiCad abortaba
al crear los marcadores de estas discrepancias en macOS; tras corregirlas termina con código 0.

Los tests de fit esperaban la antigua L (680 mm² de solape con F) y altura −12.
Se actualizan intencionalmente a la tira aprobada (0 mm², 4 mm a F) y doc25 §7
(−20, luz A/F 8,9). Las rectas J1/J2 quedan 19,722/10,698 mm bajo el módulo
de pantalla con los conectores de la tira; se comprueban esas distancias en lugar
del umbral histórico de 20 mm de la L. Las rutas de servicio siguen sin colisión.
Se añade regresión del H1 original y del cuerpo de separador
que colisiona con K aunque su centro esté fuera. No se modifica el criterio de 1,5 mm.

## Cobre e integridad eléctrica

| Comprobación | Resultado real |
|---|---|
| Esquema frente a edc67a3 | Idéntico byte a byte |
| Referencia/pad/red de todas las huellas | Idéntico a edc67a3 |
| Huellas | 37 |
| Capas / espesor | 2 / 1.6 mm |
| Pistas + vías | 764 |
| Vías | 172, todas Ø0,6 / taladro Ø0,3 mm |
| Señales | 0,25 mm |
| +3V3, +5V y pistas GND | 0,4 mm |
| Separación mínima exigida | 0,2 mm |
| Planos GND | 2, rellenados en F.Cu y B.Cu; separación de zona 0,3 mm |
| MP de RE1–RE8 | /GND, unión sólida, sin térmicos |

Las reglas por red en `pcb-a.kicad_dru` hacen exigibles los anchos de señal y potencia;
el mínimo global se mantiene en 0,2 mm. Se corrigió una separación al NPTH H4 y se
eliminaron vías/tramos sin uso antes del DRC definitivo. [Integridad JSON](docs/integrity.json).

SHA-256 del esquema: `887ac90cb5f758ef9eaf306f3dbd0887a63d128141b0a392b08fde96fc055ed6`.

SHA-256 de la placa final: `20f494ae63f43ea48bd4f3817b3db3a16eaddf4cb196b9bc0d8529c2a5801bd1`.

## DRC definitivo

KiCad **10.0.6**, ejecución `2026-11-13T11:53:10`. Se rellenaron y guardaron las
zonas antes de ejecutar el CLI sobre el proyecto real:

```sh
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb drc \
  --all-track-errors --schematic-parity --format json --exit-code-violations \
  -o apps/hardware/kicad/consola/boards/pcb-a/docs/drc-final.json \
  apps/hardware/kicad/consola/boards/pcb-a/pcb-a.kicad_pcb
```

Código de salida **0**. El reporte incluye errores y advertencias: **0 violaciones**,
**0 sin conectar**, **0 discrepancias de paridad**. [Resultado original de KiCad](docs/drc-final.json).
No se añadieron exclusiones ni se rebajaron severidades. Se conservan los cinco
chequeos ya ignorados en la base: courtyard ausente, extremo de pista no centrado en
vía, geometrías de tuning, filtros de huella del símbolo y tipo de componente/huella.

El diagnóstico SWIG intermedio omite el cotejo de biblioteca por una limitación del
proceso Python independiente y produce una variable de hoja no resuelta al desacoplar
el proyecto. No es el criterio de aceptación. El CLI final carga el proyecto y sus
bibliotecas completos, incluyendo la huella IDC local. Ver [método](THEORY-strip.md).

Freerouting 2.1 tuvo excepciones internas y dejó tres conexiones pendientes según su
contador. La sesión se importó, se cerraron las ramas que KiCad detectó mediante grilla
y se volvió a verificar todo el cobre. Su contador no se usó como PASS. Se conservan
[DSN](docs/routing-final/pcb-a.dsn), [SES](docs/routing-final/pcb-a.ses) y
[log del ruteador](docs/routing-final/freerouting-final.txt) para trazabilidad; la PCB
final incluye las correcciones posteriores a ese SES. `BoundedFreerouting.java` usa
el límite efectivo de la sesión para evitar iteraciones indefinidas.

## Encaje

```sh
python3 -B apps/hardware/kicad/consola/tools/fit/check_pcba_fit.py
```

Código de salida **0**, ejecutado nuevamente sobre la placa final.
[Informe completo](../../tools/fit/REPORT-pcba-fit.md),
[JSON](../../tools/fit/pcba-fit.json), [vista](../../tools/fit/pcba-fit.svg).

| Reserva | Solape mm² | Separación mm |
|---|---|---|
| PCB-F | 0 | 4,00 |
| Ventana M4 | 0 | 6,00 |
| M4 nominal CSV | 0 | 7,00 |
| Reserva M4 doc28 | 0 | 1,00 |
| Módulo de pantalla | 0 | 3,28 |

No hay pines THT bajo PCB-F. Los tres corredores de mazo pasan las pruebas de
polilíneas contra los prismas A/F/M4/pantalla. Los extremos remotos y radios de cable
son hipótesis explícitas del informe de fit, no medidas de un montaje real.
El CSV histórico de PCB-K v5 arroja 494,4 mm² de solape XY y no representa el layout
nuevo de K; permanece visible como diagnóstico y no forma parte de los criterios
F/M4/pantalla de esta tarea. Ver la explicación de alturas y separadores en el fit y el chequeo de montaje enlazado arriba.

## Archivos exportados y revisión

- [ZIP de fabricación](fab/pcb-a-gerbers.zip): **9 Gerber + PTH + NPTH + job metadata**.
  Se verificaron sus 12 miembros y el CRC interno del ZIP.
- [Manifest SHA-256](fab/manifest.json): hashes de todos los archivos de fabricación,
  vinculados al hash de la placa final.
- Taladros verificados: **172 vías de 0,3 mm**, **70 pines PTH de 1 mm**,
  **16 slots MP con herramienta de 1,5 mm**, **4 NPTH de 3,2 mm**.
- **14 SVG**: cobre, máscara, pasta y serigrafía de ambas caras; contorno, montaje
  y cuatro vistas compuestas. [Superior](docs/pcb-a-top.svg),
  [inferior](docs/pcb-a-bottom.svg), [ensamble superior](docs/pcb-a-assembly-top.svg),
  [ensamble inferior](docs/pcb-a-assembly-bottom.svg).
- Se rasterizaron y revisaron visualmente las dos vistas compuestas: tira continua,
  ocho encoders, cuatro taladros, fan-out IDC, planos y costura GND.
  La cara inferior se muestra desde arriba, sin espejo adicional; los Gerber usan
  las coordenadas de fabricación originales.

El bounding box del `.gbrjob` es 217,05 × 29,65 mm porque incluye el trazo Edge.Cuts
de 0,05 mm; el perfil nominal se define por su línea central de 217 × 29,6 mm.

Falta validar físicamente la unión de chapa/ejes a GND, alturas y tolerancias reales,
acceso de separadores y volumen/radio de los conectores y cables enchufados. No se ha
fabricado, ensamblado ni medido eléctricamente una unidad durante esta tarea.

## Validación de esta revisión

- `python3 -m pytest -q apps/hardware/kicad/consola/tools/tests`: 127 pruebas aprobadas.
- `python3 -m pytest -q apps/hardware/kicad/consola/tools/fit`: 19 aprobadas;
  una omitida porque el Python estándar no incluye `pcbnew`.
- Los dos chequeos ejecutables de fit devuelven 0; DRC CLI completo devuelve 0.
- Manifest, 12 miembros y CRC del ZIP, cuatro NPTH y XML de los SVG verificados.
- DXF láser: 120 contornos cerrados y handles únicos; cuatro círculos nuevos Ø4,2.
- El STEP/FCStd del panel no se regeneró: FreeCAD local aborta durante la inicialización
  de Qt en este entorno. El DXF láser sí incorpora los cuatro cortes; no se libera
  la copia 3D antigua como evidencia de encaje de los nuevos anclajes.
