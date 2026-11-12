# 29 · Banco de puesta en marcha por placas

2026-11-12 · Spec y teoría antes del código. Alcance autorizado: diagnóstico CDC y checklist local, sin flash.
Contratos: [17](17-mapa-pines-teensy.md), [18 §2](18-teensy-unica-firmware.md),
[24 §7/9](24-placa-principal.md), [28 §4/7/8](28-pcb-k.md), `ConsolaMap.h` y
`apps/hardware/kicad/consola/panel/panel-coordinates-console-v1.csv`.
Prevalece la división aprobada de 28 §7: derecha 0x21; izquierda 0x22/23. Las descripciones antiguas
de 24 §2/3 quedan históricas por su §9. No se cambia el orden de LEDs (trabajo independiente).

## Teoría y límites del ensayo

Un ACK I2C prueba presencia de una dirección, no identidad, calibración ni continuidad de cada pin.
Una transición de entrada prueba el camino sensor→lectura; debe contrastarse con el control solicitado.
Para teclas se exige pulsar y soltar; encoders deben variar en ambos sentidos; analógicos deben superar
un recorrido mínimo respecto de su muestra inicial (ADS 512 cuentas, ADC 64). No certifica recorrido
completo, linealidad ni ausencia de cruces entre controles. Los valores son crudos, antes de filtros MIDI.

Wire, Encoder, IntervalTimer, USB y audio son PJRC oficiales; el scanner, captura de buzones, parser,
cola CDC y checklist son custom. Referencias técnicas: contratos locales anteriores; Microchip MCP23017
(GPIO activo bajo), TI ADS1115 (conversión single-shot) y core Teensyduino 1.160.0 instalado.
No se añade acceso I2C ni formateo en audio/ISR; la ISR sólo conserva además su última muestra ADC.

## Preparación y criterios por placa

Usar Teensy con firmware ya cargado. `consola_uno` mantiene el mínimo de mesa; `consola_uno_full`
habilita el panel completo, sólo con las entradas conectadas (18). Este banco no cambia perfiles ni flashea.
Desconectar alimentación para conectar placas. Cada ejecución permite placas adicionales: direcciones extra
se registran, no fallan. Falta de cualquier dirección obligatoria falla; entradas sin muestras no pasan.

| Selección | I2C obligatorio | Controles (nombres CSV / mapa) | Dependencias |
|---|---|---|---|
| pcb-m | 0x0A,48,49,60,61 | JOG ambos sentidos, JOG_CLICK, PEDAL_BTN, POT_MASTER/CUE/IN, SEL_PAD (tres posiciones), SHARP_L/R | clic/pedal requieren PCB-K izquierda; Sharp externos |
| pcb-k-izq | 0x22,23 | 17 teclas de edición, navegación, motor/sección, SHIFT/UNDO | conector desde PCB-M |
| pcb-k-der | 0x21 | 9 teclas transporte/escenas | conector desde izquierda |
| pcb-a | 0x20 | E1–E8 ambos sentidos y sus ocho clics | cable a PCB-M |
| pcb-f | 0x48 | F1–F4 | ADS en PCB-M, cable vía PCB-A |
| pcb-p | 0x49 | FSR_V1–4 pulsar/soltar, FSR_X1–2 presión sostenida | ADC Teensy + ADS en PCB-M |
| todo | unión | todos los anteriores, sin duplicar placas | panel cableado |

PCB-F/P no tienen direcciones propias: SCAN comprueba su convertidor en PCB-M, no la placa pasiva.
Para K se usa `@GF KEYLEDS 26` y confirmación s/n del operador del tramo correspondiente; no se codifican
índices ni orden de cadena. Izquierda aislada: comprobar su tramo; derecha necesita continuidad/bypass del
tramo anterior. Siempre `@GF KEYLEDS 0` al salir de esa prueba. No se certifican colores individuales con
una respuesta CDC. NeoTrellis M4 no pertenece a las placas seleccionables: INPUTS publica conexión/bits y
WATCH sus notas 48–79, pero este checklist no certifica sus 32 pads ni LEDs.

## Contrato CDC (una línea por mensaje)

Gramática estricta, sin argumentos sobrantes: `@GF SCAN`, `@GF INPUTS`, `@GF WATCH 0|1`.
Errores de estos verbos: `@GF ERR <verbo> ARG`. Se mantiene el parser de líneas y el resto de comandos.

- SCAN: una dirección 0x08–0x77 por iteración como máximo, separadas ≥1 ms; sustituye el polling
  ADS/MCP de esa iteración. Mantiene timeout Wire 250 µs y recuperación acotada de 18. No escribe datos
  a dispositivos. Respuesta diferida `@GF OK SCAN 0A,20,...` (hex mayúscula, `-` si vacío).
  Se serializan comandos durante el scan; no hay espera activa. Un bus averiado no implica ACK.
- INPUTS: `@GF OK INPUTS MCP=<4 hex> MCP_VALID=<máscara> ENC=<9 enteros> ENC_MASK=<máscara>
  ADS=<8 enteros> ADS_VALID=<máscara> FSR=<4 enteros> FSR_VALID=<máscara> POT=<3 enteros>
  POT_VALID=<máscara> SEL=<entero> SEL_VALID=<0|1> M4=<0|1> M4_BITS=<hex> DROPS=<contador>`.
  Todo va en una única línea real. Máscaras decimales salvo MCP y M4_BITS. ADS orden chip*4+canal;
  ENC índice8=jog; FSR/POT/SEL ADC12. MCP bits físicos activos bajos. Son últimas muestras, no lecturas
  simultáneas; válido significa muestreado desde arranque/última caída conocida. Cero con válido=0 no es muestra.
- WATCH: `@GF OK WATCH 1` / `@GF OK WATCH 0`. Activo sólo 120000 ms desde cada WATCH1, incluso sin host;
  expira con `@GF OK WATCH 0 EXPIRED`. Evento `@GF OK EVENT <KEY|ENC|ADS|FSR|POT|SEL> <índice> <valor>`.
  KEY usa nota musical y estado estable 0/1 (incluye M4/FSR); ENC cuenta absoluta con signo.
  Analógicos: ADS umbral128 cuentas, FSR/POT/SEL umbral32 desde último valor publicado.
  Histéresis de dirección: continuar requiere un umbral; invertir requiere dos umbrales.
  Cada canal tiene su reloj independiente: mínimo50ms entre eventos (máximo20/s), también desde WATCH1
  o una nueva referencia válida. Durante ese plazo se conserva sólo el valor actual; un transitorio
  que desaparece antes del próximo cupo no se publica. Primera muestra válida establece referencia,
  no evento; perder validez borra dirección y referencia. WATCH1 reinicia referencias y relojes.
  KEY/clics/ENC no pasan por este filtro ni por su límite de tasa.
  Una cola fija de 64 eventos conserva orden; si se llena descarta nuevos y aumenta DROPS en INPUTS.
  WATCH0 descarta eventos pendientes. El host falla el chequeo de integridad si DROPS aumenta.
  Las supresiones por histéresis/cadencia son intencionales y no aumentan DROPS; éste sigue contando
  únicamente desbordamientos reales de cola. El límite por canal no garantiza cero drops si todos
  los controles cambian a la vez o el host deja de leer.

CDC RX hasta16 bytes/loop y TX hasta32 con availableForWrite, sin esperar al host. Memoria estática;
sin heap, delay, scans síncronos ni cambios de prioridades. Se conservan todas las rutas MIDI y audio.
El objetivo loop_max_us<2000 y cero drops USB/audio sigue requiriendo medición física de 18.

## Herramienta y demo

`cd tools/consola && python3 banco.py --placa pcb-a` descubre un único puerto con identidad «GF Consola»
(product/description); `--puerto /dev/...` evita ambigüedad. `--timeout 15` fija plazo por control;
plazos CDC y renovación WATCH son independientes. Eventos ajenos no aprueban el control solicitado.
Cada control empieza con nueva referencia INPUTS y WATCH1 (vacía pendientes); la espera renueva WATCH
antes de los120s si el plazo elegido es mayor. Fallos siguen al siguiente control; desconexión/interrupción
guarda informe parcial y apaga WATCH en finally si el puerto sigue disponible. LED s/n requiere terminal.

`python3 banco.py --placa todo --simular`: puerto falso determinista, sin USB, respuestas y movimientos
simulados; confirmación LED automática marcada simulada. Informe fechado en
`apps/docs/consola/mediciones/banco-<placa>-<fecha>.md`, incluyendo modo, puerto, SCAN, muestras,
resultado por control, errores y límites. Una simulación nunca constituye aprobación física.

Unity verifica gramática, formato exacto, límites de buffers, scan incremental, umbrales, expiración con
wrap y presión de cola. pytest usa el puerto falso para checklist completo, faltantes, evento incorrecto,
timeout, rechazo LED, desconexión y limpieza. Demo de escritorio: simulación todo produce informe sin
hardware. No cubre ERC/DRC, alimentación/consumo, polaridad, tensiones CV, MIDI DIN, audio/THD/latencia,
60min dúplex, calibración FSR/Sharp, NeoTrellis ni ensayos on-device. No sustituye 24 §7 ni 28 §8.

## Corrección de banco del 12 de noviembre: teoría y prueba previa

Evidencia: [banco físico](mediciones/banco-2026-11-12.md), 220880 eventos FSR en60s con14/15 al aire.
El deadband original compara contra el último evento: ruido que cruza repetidamente ambos lados puede
publicarse a velocidad de loop. La histéresis de dirección exige mayor recorrido para invertir; el reloj
por entrada pone una cota incluso ante oscilación de escala completa. Filtrar sólo CDC conserva MIDI
y permite mantener todas las transiciones de teclas y posiciones de encoders observadas por loop.
Subir sólo el umbral perdería sensibilidad sin acotar un pin flotante.

FSR mínimo conserva máscara3 y su muestreo. El detector anterior empezaba Armed: ADC1500≥600 iniciaba
Peak y emitía NoteOn36/37 tras3ms incluso sin FSR. En consola_uno (también full/brain) se exige primero
reposo continuo≤400 durante3ms, luego cruce≥600. Ruido por encima de400 reinicia la cualificación.
Se conserva velocity600–3000, ventana3ms, release400 y guard12ms después de armar. Un FSR conectado
con R a GND reposa bajo y conserva el golpe normal; si se arranca presionado, soltar antes del primero.
No se infiere la presencia de una resistencia mediante ADC: una desconexión después de armar o ruido
que imite un reposo real puede seguir pareciendo un golpe. El pull-down físico de PCB-M sigue siendo
necesario. No se deshabilitan pads conectados ni se cambian los umbrales para ocultar el ruido.

Unity debe verificar parser estricto, valores límite e inversión de histéresis, cadencia independiente
con wrap, coalescencia, revalidación, WATCH0/renovación/expiración y KEY/ENC sin rate limit; simular60s
de ruido en las entradas analógicas y comprobar≤1200 eventos/canal. Para FSR: ruido1500±sin reposo,
reposos breves rechazados y golpe/velocity/liberación/retrigger iguales al detector anterior tras
cualificación. Para EN: tres fallos consecutivos por MCP, éxito completo borra racha, espera de bus
ocioso, pulso≥1ms, wrap, ausencia de repulsos y contador. Son pruebas de lógica custom, sin prometer
comportamiento eléctrico ni recuperación de una rama en corto. Ensayo físico pendiente en18.
