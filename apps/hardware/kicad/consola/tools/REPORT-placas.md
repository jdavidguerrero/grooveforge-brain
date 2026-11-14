# Console — PCB-K izquierda/derecha, PCB-F y PCB-P

2026-11-12. Base `670457f`, rama `claude/monorepo-groovepush`, clon
`/private/tmp/grooveforge-placas`. Captura de esquemas y preparacion mecanica, sin layout.
No se modifica `schgen/pcb_m.py`, `boards/pcb-m/` ni `apps/docs/consola/24-placa-principal.md`.
No se cambia firmware ni los specs. Teoria previa: [THEORY-placas.md](THEORY-placas.md).

## Resultados reales

KiCad CLI **10.0.6**; simbolos instalados de la misma version. Verificacion final sin
regeneracion, sobre los esquemas entregados, con exportacion real XML, ERC y PDF:

| Placa | Conectividad | Componentes | Pines | NC | Redes | ERC errores | ERC avisos | PDF paginas |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| pcb-k-left | PASS | 78 | 239 | 8 | 58 | 0 | 0 | 11 |
| pcb-k-right | PASS | 43 | 127 | 5 | 32 | 0 | 0 | 7 |
| pcb-f | PASS | 10 | 32 | 0 | 6 | 0 | 0 | 2 |
| pcb-p | PASS | 12 | 24 | 0 | 12 | 0 | 0 | 2 |

Los PWR_FLAG virtuales no cuentan como componentes de PCB. Se exportaron y renderizaron
las **22 paginas** de PDF; revision visual de todas con montajes y una hoja de escenas
a resolucion completa: sin recortes de contenido ni superposiciones observadas.
KiCad tambien cargo/exporto a SVG los **3 footprints locales**, sin errores de lectura.
Los SVG/PNG de revision son temporales del clon, no archivos de fabricacion.

Pruebas finales:

```text
python3 -m pytest -q apps/hardware/kicad/consola/tools/tests --basetemp=.schgen-placas-release
38 passed in 35.59s
```

Incluyen regresiones existentes, mapa GPIO independiente de la descripcion, straps de
direccion, RESET/desacoplo, pads libres, conectores, cadena completa, cada condensador LED,
DNP, aislamiento FSR, posiciones/caras, agujeros mecanicos, geometria y marcado de huellas,
carga real de huellas con KiCad, esquemas deterministas y exportacion/ERC/PDF real por placa.
Intercambiar UNDO y BACK en la expectativa hace fallar el comparador contra el XML real.

Tambien se ejecuto:

```sh
python3 apps/hardware/kicad/consola/tools/check_netlist.py --all --directory .verification-placas/all --regenerate
```

**Exit 0 en las cinco placas**. PCB-M se genero solo en esa carpeta temporal, sin tocar
su directorio original: PASS, 113 componentes / 398 pines / 21 NC / 100 redes, ERC 0 errores
**7 avisos preexistentes** de etiquetas con un solo pin: I2S_TX, I2S_RX, I2S_MCLK,
I2S_BCLK, I2S_LRCLK, LINE_IN_L_R y LINE_IN_R_R. Su exportador tambien advierte de
anotacion preexistente. No se corrigen por estar fuera de alcance.

Los reportes finales de las nuevas placas no contienen avisos ni exclusiones particulares.
KiCad lista como ignorados sus checks por defecto: etiqueta global unica, cuatro uniones,
modelo SPICE y filtros de huella. No se agregaron supresiones para lograr ERC limpio.
La primera pasada de PCB-K tenia referencias sin sufijo numerico; se corrigieron antes
de la entrega. Una configuracion temporal inicialmente sin tablas produjo avisos de
libreria; se corrigio cargando las tablas reales, no silenciando el check.

## Decisiones de captura

- **PCB-K partida:** 17 teclas izquierda, 9 derecha, MCP SOIC-28W: U22=0x22, U23=0x23,
  U21=0x21. GPIO fisico segun 28 §4; JOG_CLICK=U23.27 (GPA6), PEDAL_BTN=U23.1 (GPB0).
  GPA7/GPB7 y NC del integrado marcados NC. Entradas disponibles e INTA/INTB a pads.
- J1 izquierdo PH8: GND / +3V3 / SDA / SCL / LED_DIN_5V / +5V_LED / JOG_CLICK / PEDAL_BTN.
  J2 izquierdo y J1 derecho PH6: +3V3 / GND / SDA / SCL / +5V_LED / LED_DATA. Cable 1:1.
  Footprints PH verticales para K, a montar por debajo en layout.
- **Orden nuevo autorizado**, SW1/D1 indice 0 hasta SW26/D26 indice 25:
  BACK, MENU, NAV_L, NAV_R, BROWSE, TAP, FILL, CUE_ENG, BUILD, DROP, BREAK, SHIFT, UNDO,
  DELETE, DUPLICATE, QUANTIZE, NEW → cable → PLAY, STOP, REC, LOOP, METRO,
  SCENE4_PAGE_R, SCENE3_PAGE_L, SCENE2_OCT-, SCENE1_OCT+.
  D17.DOUT → J2.6 → cable → J1.6 → R1 derecho → D18.DIN. D26.DOUT NC.
  33 ohm 0603 en cada entrada, 100 nF por LED y 22 uF 1210 por placa.
- R2/R3 de cada K son 2.2 kohm **DNP**, sin pull-ups activos adicionales. Los RESET
  tienen 10 kohm y cada MCP 100 nF + 1 uF. Los PWR_FLAG afirman las alimentaciones y
  fuentes de SCL/DIN externas para ERC; no representan reguladores ni buffers locales.
- **PCB-F:** conserva J1 hacia PCB-A J3, JST-SH horizontal, pinout +3V3 / FADER1–4 / GND.
  Se reutiliza sin cambiar `groovepush:ALPS_RS6011P_60mm`, B50K, MP a GND, 100 nF por
  cursor y 100 nF en alimentacion como la captura GroovePush. Ejes a x 264.5/284.5/304.5/324.5,
  y 129.5, rotacion 90 grados. Contorno 81 x 83 y cuatro agujeros copiados a mechanical.csv.
- **PCB-P:** seis resistencias variables de dos terminales, cada una a un JST-PH S2B-PH-K
  dedicado. V1–V4 a J_FSR1–4; X1/X2 a J7_EXP1/J8_EXP2. Pin 1 +3V3, pin 2 senal.
  No hay union entre los seis circuitos en PCB-P, ni divisores, ni filtrado, ni activos.
  `+3V3_V1` etc. identifican la alimentacion del cable respectivo; se unen en PCB-M.
  Posiciones de sensores, contorno y diez agujeros vienen del CSV v5.
- `placement.csv` usa el sistema del panel (+y hacia atras). La geometria local de
  footprints KiCad invierte Y. La huella SW integra tecla superior y zocalo inferior;
  sus dos filas de colocacion comparten referencia deliberadamente, no se duplican al
  colocar. El LED va en B, centro a y_tecla+4.93. Su propia huella aporta Edge.Cuts;
  SW solo muestra la ventana en Dwgs.User, evitando recortes duplicados.
- No se cambia `generator.py`: solo registro de placas, CLI y soporte de artefactos.
  `--board pcb-m` sigue siendo el defecto; `--description`, `--output`, `--directory` y
  `--libraries` siguen disponibles. `--all` devuelve fallo si falla alguna placa y
  continua comprobando las demas. Temporales/config de KiCad dentro del directorio de
  salida, con tablas generadas de librerias instaladas; no se usa configuracion personal.
- Se conservan el proyecto y layout **antiguos de GroovePush** presentes en `boards/pcb-f/`;
  ese `.kicad_pcb` no corresponde al nuevo esquema Console. README lo identifica claramente.
  No se genero ni actualizo ningun `.kicad_pcb`, contorno de PCB ni ruta de cobre.

## Fuentes y discrepancias conservadas

Se leyo 28 completo. Su §4/§6 conserva indices/recorrido LED de una placa; la instruccion
actual del usuario fija el nuevo recorrido partido (§7), usado aqui. El mapa GPIO no cambia.
La frase «36 de 42 entradas» de §4 no coincide con su tabla: hay 26 teclas + 2 entradas
remotas = 28 en uso y 14 disponibles; se sigue la tabla, sin modificar el documento.
El cable F hacia PCB-A se conserva por instruccion explicita aunque Console use PCB-M.

La huella Choc soldada de `boards/pcb-k1` y `pcb-k2` usa
`boards/lib/groovepush.pretty/SW_Kailh_Choc_V2_1u`, con otra orientacion y sin socket.
No se reutilizo como si fuese hot-swap. La geometria PG1353 solicitada es la autoridad.
El LED de K1/K2 ya referencia la huella estandar inversa; se copia esa geometria instalada
a `lib/gf_consola.pretty/SK6812MINI-E_ReverseMount` para disponer de una libreria local.

Referencias de fabricante consultadas:
[Kailh CPG135001S30, KHA-PG1350-005EN / KH-PS-1702-35 Rev B](https://www.kailhswitch.com/Content/upload/pdf/202115927/CPG135001S30-data-sheet.pdf),
[Interlink FSR 402 y opciones de terminacion](https://www.interlinkelectronics.com/fsr-402).
La consulta web localizo el plano del socket; no habia una huella hot-swap fiable en el repo.
No se considera certificada la interpretacion de sus pads SMD: se entrega marcada como
provisional segun la alternativa autorizada por el usuario.

## Verificar a mano antes de layout/fabricacion

1. **CPG135001S30 real + PG1353:** imprimir cupon 1:1, comprobar cara/vista, insercion de
   contactos, holgura del poste 5.0, guia 1.5, agujeros 3.0 del socket y pads SMD. Los
   centros SMD x -3.275 / 8.275 y tamanos 2.6 x 2.6 son provisionales. La huella incluye
   literalmente **VERIFICAR CON ZÓCALO REAL** y «PADS 1/2 PROVISIONALES». No comprar
   placas finales con este patron sin verificarlo. Confirmar recorte y posicion del LED
   respecto a la tecla, aletas/corte de chapa y separacion vertical de 28 §2/§3.
2. **FSR 402 real:** identificar variante exacta y longitud de cola. El footprint es un
   datum de disco 18.3 con landing generico, pads 2 x 4 sin pasta, paso 2.54 y y local
   KiCad 11. No certifica la terminacion Interlink ni la soldabilidad de la cola polimerica.
   Medir, comprobar pliegue/recorrido, alivio mecanico y proceso de conexion. Puede requerir
   adaptar el patron o un adaptador de cola; se mantiene pendiente hasta tener la pieza.
3. **RS6011 real:** confirmar sufijo/modelo, paso de pines, MP/carcasa, taladros de guia,
   longitud/altura de palanca, sentido del cursor y recorrido 60 antes de usar la huella
   GroovePush en Console. Comprobar ranura de chapa y agujeros M3 del CSV.
4. **Layout futuro:** contornos de las dos K aun por materializar; la spec v5 tiene el
   contorno original unico. Respetar ventanas/zonas prohibidas, montaje inferior, altura
   de PH sobre bahia Pi y cruces con PCB-P/M4. No hay DRC/layout ni validacion 3D en esta
   entrega. Los CSV F/P describen mecanica, no garantizan separaciones ni montaje final.
5. **Banco:** I2C 0x21/22/23, pull-ups solo en PCB-M, las 26 teclas y entradas remotas,
   colores en orden nuevo y continuidad pin a pin de cables. No se actualizo
   el firmware: confirmar/ajustar sus indices para esta cadena en una tarea especifica. Comprobar 500 mA
   de limite, caida de 5V, desacoplo y calidad de datos con el arnes real.

Estas verificaciones fisicas no se pueden inferir de PASS de netlist/ERC.
