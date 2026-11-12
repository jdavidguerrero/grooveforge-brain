# Banco CDC — verificación de escritorio

2026-11-12 · Clon `/private/tmp/grooveforge-banco`, rama `claude/monorepo-groovepush`, base
`00d464abc42df0621b4d86e159ae6326eba722e1`. Spec previo: [29](../29-banco-pruebas.md).
Sin push, flash ni ensayo físico; no se modificaron `key_leds`, `KeyLedReturn` ni el doc27.

| Verificación real | Resultado |
|---|---|
| Desde apps/firmware-teensy: `~/.platformio/penv/bin/pio test -e native` | **201/201**, 17 suites, 22,615 s; incluye 5 casos nuevos de banco |
| Desde apps/firmware-teensy: `~/.platformio/penv/bin/pio run -e consola_uno` | **SUCCESS**, última pasada incremental 2,909 s |
| Adicional: `pio run -e consola_uno_full` | **SUCCESS**, 25,780 s |
| Desde tools/consola, Python3 del sistema (3.14.3): `python3 -m pytest -q` | **487 passed**, 2,14 s; incluye 37 pruebas nuevas de banco |
| `python3 banco.py --placa todo --simular` | **exit 0**, 61 controles, 6 SCAN, 6 snapshots, 6 comprobaciones de pérdidas y 2 confirmaciones LED simuladas: 81 filas PASA |
| `git diff --check` / `git diff --cached --check` | Sin errores |

El build mínimo final usa FLASH código **88880 B**, RAM1 variables **25184 B**, RAM2 **55040 B**.
Completo: FLASH código **89712 B**, RAM1 variables **25216 B**, RAM2 **55040 B**.
Warnings existentes al compilar C con `-fno-rtti`; no se añadieron warnings en el código del banco.
Los builds no miden latencia, carga bajo audio ni presencia eléctrica de placas.

Reproducción de PlatformIO en este entorno: prefijo
`PLATFORMIO_CORE_DIR=/private/tmp/grooveforge-banco/.banco-pio PLATFORMIO_SETTING_ENABLE_TELEMETRY=no`.
El core temporal dentro del clon referencia plataformas/paquetes instalados; se copió Unity **2.6.1**
desde caché local a `.pio/libdeps/{native,consola_uno,consola_uno_full}/Unity` para evitar una descarga
que quedaba esperando red. Frameworks USB/Wire privados se generan dentro de `.pio/build`, como en18.
No se modificó `~/.platformio`. Los directorios de trabajo `.banco-pio`/`.banco-logs` y `.pio` están
excluidos del parche. Logs locales: `.banco-logs/{native-final,build-final,pytest-final,simular-final}.log`.

Pruebas nuevas: gramática estricta, formato y límites, scan incremental/vacío/ACK completo con wrap,
WATCH con umbral, primera muestra, desconexión, cola llena y expiración; checklist por placa, puertos
ambiguos, I2C faltante/extra, movimientos incompletos, eventos de otro control, encoder con wrap,
renovación WATCH, ACK atrasado, pérdida de eventos, rechazo LED, interrupción y conservación de informe parcial.

El informe `banco-todo-20261112-*.md` de esta entrega contiene **datos sintéticos** y está marcado SIMULADO.
Quedan pendientes los ensayos con placas reales de29, la puerta de audio dúplex/latencia de18 y la
verificación eléctrica/ERC/DRC de24/28. Para un panel cableado completo usar una imagen
`consola_uno_full` ya instalada; este trabajo no la flashea.
