# schgen — Python 3 + KiCad 10

Desde la raíz del repositorio, regenerar PCB-M y actualizar/validar XML, ERC y PDF:

```sh
python3 apps/hardware/kicad/consola/tools/check_netlist.py --regenerate
```

Solo generar los esquemas (no necesita `kicad-cli`, sí las librerías de símbolos):

```sh
PYTHONPATH=apps/hardware/kicad/consola/tools python3 -m schgen --board pcb-m
```

Pruebas (pytest es dependencia **solo de desarrollo**):

```sh
python3 -m pytest -q apps/hardware/kicad/consola/tools/tests --basetemp=.schgen-test-tmp
```

El módulo y el verificador usan únicamente la biblioteca estándar. Por defecto buscan símbolos
en la instalación macOS de KiCad y `kicad-cli` en PATH o en el bundle de macOS. `--libraries DIR`
y `--kicad-cli PATH` permiten otras instalaciones. La prueba de integración se omite si no está
la CLI; las pruebas que necesitan librerías se omiten si no están instaladas.

`schgen/pcb_m.py` es la descripción eléctrica revisable. `PARTS` conserva referencias, valores y
footprints de la captura cuando son válidos; `FRONT` conserva el frente analógico, con el divisor de selector adaptado a D13 (3 hilos). Las
posiciones se asignan en la factoría con una grilla explícita; los bucles muestran cómo repetir
FSR, canales ADC/DAC y conectores. `None` significa NC, nunca pin olvidado. Cada unidad tiene su
propia posición/pines pero conserva la misma referencia, valor y footprint del encapsulado.
La hoja 04 permanece vacía en la raíz, con nota del traslado a PCB-K.

Una descripción nueva es un archivo Python que exporta `board()`:

```python
from schgen import Board, Sheet, Symbol


def board():
    """A small board, independently reusable with the same generator/checker."""
    resistors = [Symbol('Device:R', f'R{i+1}', '10k',
                        'Resistor_SMD:R_0402_1005Metric',
                        (50.8 + i * 50.8, 50.8), {'1': '+3.3V', '2': f'KEY{i}'})
                 for i in range(4)]
    return Board('example', [Sheet('keys', resistors)])
```

Ejecutar con `python3 -m schgen --description path/to/description.py --output path/to/board`
(el mismo `PYTHONPATH`), o verificar con `check_netlist.py --description ... --directory ...`.
Las placas Console se describen al final de este documento.

Los pines se toman de las librerías estándar, incluidos símbolos derivados (`extends`) y
unidades de alimentación. Colisiones de cuerpos/pines/cables y envolventes de textos abortan
antes de escribir archivos. Los UUID dependen del nombre de placa/hoja/referencia/unidad/pin;
no dependen del orden o la fecha. La salida `.kicad_sch` es idéntica al regenerar con la misma
versión de librerías. Los nombres de hoja/referencia son identidades persistentes: renombrarlos
cambia sus UUID. Actualizar KiCad puede cambiar las definiciones de librería incrustadas.

Las redes compartidas entre hojas y los rieles son globales; las demás son locales. Si dos hojas
usan el mismo nombre, se considera una conexión intencional. Usar nombres diferentes para
señales privadas repetidas. Las etiquetas llevan un cable de 2,54 mm desde la punta real del pin.
Las instancias se colocan en grilla de 1,27 mm; los pines conservan su geometría nativa de librería.

`check_netlist.py` **no regenera** por defecto: así detecta cambios manuales incorrectos en el
esquema. Siempre exporta XML, ejecuta ERC y exporta PDF; devuelve error ante cualquier diferencia
de pines, NC sin marca, partición/nombre de redes, componentes inesperados o errores ERC. Los
avisos ERC se imprimen y conservan. Los `PWR_FLAG` (referencias virtuales `#`) no aparecen en el
XML de PCB; sí los valida ERC. No se modifica el layout; los proyectos nuevos de Console se crean solo al regenerar.

La fecha y ruta del XML y la fecha de ERC/PDF las escribe KiCad y pueden cambiar entre
exportaciones. Esto no afecta la estabilidad de los esquemas generados. Ver [THEORY.md](THEORY.md)
y [REPORT.md](REPORT.md) para límites, conflictos documentales y resultados reales.


PCB-M incorpora las decisiones D8–D15 aprobadas por Juan (2026-11-12), registradas en doc24 §8.
J1 sigue PCB-A fabricada; los destinos Teensy se verifican contra el firmware. La entrada 5V por
JST-PH/F1, net-tie de masa, protecciones CV/pots, MIDI 33Ω/10Ω y mazos de jacks se describen en
`schgen/pcb_m.py`. Las pruebas `tests/test_pcb_m_contract.py` verifican ese contrato sin cambiar
la API de generación compartida.

## Console: PCB-K izquierda/derecha, PCB-F y PCB-P

Teoria y limites antes de la implementacion: [THEORY-placas.md](THEORY-placas.md).
Resultados reales y comprobaciones fisicas: [REPORT-placas.md](REPORT-placas.md).

```sh
# Una placa, con proyecto/placement y exportacion XML/ERC/PDF:
python3 apps/hardware/kicad/consola/tools/check_netlist.py --board pcb-k-left --regenerate
# Las otras: pcb-k-right, pcb-f, pcb-p. Sin --board sigue siendo pcb-m.
# Todas las registradas (incluye PCB-M); sin --regenerate detecta cambios manuales:
python3 apps/hardware/kicad/consola/tools/check_netlist.py --all
# Regenerar/verificar en una copia de revision, dejando intactas las placas originales:
python3 apps/hardware/kicad/consola/tools/check_netlist.py --all --regenerate --directory tmp/review-boards
# Solo esquema + archivos de proyecto/colocacion para una placa nueva:
PYTHONPATH=apps/hardware/kicad/consola/tools python3 -m schgen --board pcb-k-right
# Regenerar la libreria local (requiere footprints estandar de KiCad):
PYTHONPATH=apps/hardware/kicad/consola/tools python3 -m schgen.footprints
```

`pcb_k.py` fija GPIO, orden serie y circuitos de soporte; `passive_boards.py` fija F/P.
`console_artifacts.py` copia datos de panel a `placement.csv` y mecanica F/P a
`mechanical.csv`. Las filas key/socket del CSV son una sola referencia SW compartida:
no son dos footprints superpuestos. Ver README de cada placa para ejes/caras.
Los proyectos existentes se conservan; los nuevos se crean con configuracion minima.
La libreria `consola/lib/gf_consola.pretty` se resuelve con `fp-lib-table` por placa;
la huella RS6011 sigue en `boards/lib/groovepush.pretty`.

El checker usa configuracion/cache/temporales de KiCad debajo del directorio de salida
y tablas de libreria generadas desde las librerias instaladas; no depende de las tablas
personales del usuario. No cambia el nucleo `generator.py` ni la descripcion PCB-M.
`--all` continua con las otras placas si una falla y devuelve exit 1 al final.
