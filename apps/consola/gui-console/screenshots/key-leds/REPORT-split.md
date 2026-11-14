# PCB-K partida · orden LED27

2026-11-12. Clon `/private/tmp/grooveforge-ledsplit`, rama
`claude/monorepo-groovepush`, base `00d464a`. Sin push ni flash.

Specs 27 §1/§5 y 28 §4/§6 actualizadas antes del código. Cadena izquierda
0 BACK → 16 NEW; un hilo LED_DATA del cable → derecha 17 PLAY → 25 SCENE1_OCT+.
Una tabla fuente por lenguaje: `KeyLedMap.h::CHAIN` y `key_leds.py::KEY_CHAIN`.
El test compila/exporta la tabla C++ y compara orden, 26 referencias únicas y
centros con Python, CSV y ambas specs. TAP deriva su índice de la tabla (5);
Unity comprueba que STOP (18) conserva su estado normal. DMA recorre bytes en
orden y no requiere remapeo.

LED27 CC102–111, colores, brillos y limitador sin cambios. Los 93 cuadros nuevos
son idénticos a los de la base por referencia de tecla, incluida la corriente;
sólo cambia su orden en el buffer. PNG/SVG/HTML/JSON regenerados con el renderer
C++ de producción; cable NEW → PLAY naranja. PNG de los tres modos revisados.

| Verificación real | Resultado |
|---|---|
| `~/.platformio/penv/bin/pio test -e native -f test_key_leds` | 8/8 PASS, 3,69 s |
| `~/.platformio/penv/bin/pio run -e consola_uno` | SUCCESS, 61,23 s |
| `QT_QPA_PLATFORM=offscreen python3 -m pytest -q` | 529 passed, 13 skipped, 215,78 s |
| GUI `tests/test_key_leds.py` | 6/6 PASS |
| Preview: 26 teclas × 3 modos × 31 cuadros | canales ≤62; máximo estimado 157,373 mA |
| `git diff --check` | PASS |

PIO ejecutado desde `apps/firmware-teensy`, con core/paquetes copiados y Unity
2.6.1 de la caché local dentro de `.pio/`; instalación global sin modificar.
GUI desde `apps/consola/gui-console`, Python del sistema 3.14, sin penv en PATH.
Temporales/cachés y logs en `.pio/` del clon (`native-split.log`,
`build-split.log`, `gui-split.log`, `gui-key-split.log`, ignorados por Git).
Los tests con host real compilaron y pasaron; 12 casos de sockets Unix se omiten
por el sandbox y uno por General Sans local ausente. El build emite avisos de
`-fno-rtti` aplicado a C. Verificación eléctrica/timing en hardware pendiente.

Regenerar: `python3 apps/consola/gui-console/tools/preview_key_leds.py` desde raíz.
