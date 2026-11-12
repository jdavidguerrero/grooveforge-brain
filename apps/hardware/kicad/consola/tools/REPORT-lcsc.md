# Cierre LCSC y suministro externo — 2026-11-12

Clon exclusivo `/private/tmp/grooveforge-lcsc`, rama `claude/monorepo-groovepush`,
base `751d94957367a3f5148f75356ecd25340ec8c8ba`. Sin commit ni push.
Teoría previa: [THEORY-lcsc.md](THEORY-lcsc.md). Compras y declaraciones aprobadas por Juan.

- Catálogo: C41376163 (J_T41L1/J_T41R1), C225519 (SW_PAD1), C7469972 (C_CV_FILTER1),
  fecha **2026-11-12**, fuente **buscador LCSC**, según verificación de Claude aportada por Juan.
- Condensador: **47 µF 16 V ±20 %, CP_Elec_5x5.4** existente en KiCad **10.0.6** instalado.
  Símbolo Device:C_Polarized: pin 1 positivo a +5V_CV, pin 2 a GND; huella: pad 1 lado «+»
  (x=−2,2 mm), pad 2 negativo (x=+2,2 mm). Circuito/capacidad conservados.
- `Supply=local`: los dos IDC, descripción «caja IDC 2×15 2,54 mm recta (box header) con muesca».
  `Supply=owner`: SW1–SW26 (PG1353 + CPG135001S30 de AliExpress) y RE1–RE8 EC11 de Juan.
  Cada componente conserva descripción en `SupplyDescription`, esquema, XML y BOM.
- El checker exige coincidencia de tipo/descripción/valor/huella con `external_supplies` y
  rechaza LCSC junto a Supply. Ausencia de ambos sigue pendiente; declaración inválida falla.
- Cuatro BOM reexportadas **directamente de KiCad**, con Supply y SupplyDescription:
  M **212 filas**, K izquierda **78**, K derecha **43**, A **37**. Sin posproceso.
- Pedido manual: C41376163 ×2 y C225519 ×1 (THT de M, doc24 §6). C7469972 solo en BOM JLC;
  sin compra suelta duplicada. Conectores/cantidades y pasivos de A conciliados con catálogo/BOM;
  el SH4 de M queda en JLC, el de A en compra manual. CSV: 20 códigos únicos del catálogo.

## Verificación real

```sh
TMPDIR="$PWD/.lcsc-tmp" TMP="$PWD/.lcsc-tmp" TEMP="$PWD/.lcsc-tmp" PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q apps/hardware/kicad/consola/tools/tests --basetemp=.lcsc-tests
python3 apps/hardware/kicad/consola/tools/check_netlist.py --all --directory .lcsc-validation
python3 apps/hardware/kicad/consola/tools/check_harness.py
python3 apps/hardware/kicad/consola/tools/check_bom.py
```

**pytest: 110 passed (41,51 s), sin avisos.** Incluye rechazo de declaraciones mutadas, cierre XML/BOM y exportación
real de las cuatro BOM. **Netlist: PASS en las seis placas, ERC 0 errores**; M conserva 7 avisos
conocidos (5 I2S y 2 rings LINE IN); resto 0 avisos. **Harness: 32 cables, 0 errores.**
**BOM: 0 errores de identidad, 0 pendientes en las cuatro placas**; externos M1/K17/K9/A9.

Para `--all` se copiaron esquemas con sus hojas, proyectos y tablas de librerías a
`.lcsc-validation` dentro del clon, resolviendo rutas locales contra las originales. M/K/A
se verificaron y exportaron además en sus rutas definitivas (XML/ERC/PDF entregados).
Sin exclusiones ERC nuevas. Temporales de validación/pruebas eliminados.

`git diff --check` limpio. Sin cambios en firmware, apps/consola, boards/pcb-f ni layouts
*.kicad_pcb (incluido PCB-P). Sin modificación de specs 00–06. Patch binario staged contra
751d949 en `/private/tmp/grooveforge-lcsc.patch`. PASS de compras no certifica layout/DRC,
stock reservado ni cupones mecánicos hot-swap; permanecen los límites físicos previos.
