# 31 · Puesta en marcha de las seis placas

2026-11-13 · Procedimiento para la llegada de JLC: PCB-M ensamblada; PCB-A y PCB-K
izquierda/derecha con soldadura manual; PCB-F y PCB-P se completan según el lote recibido y su BOM.
Se usa firmware **ya cargado**; este procedimiento no autoriza un flash.
Registrar cada puerta en `mediciones/puesta-en-marcha-<fecha>-<unidad>.md` con la plantilla §8.
No avanzar con un FAIL o un pendiente eléctrico de la etapa siguiente.

## 1. Fuentes, instrumentos y decisiones que prevalecen

Leer [24 §8/10](24-placa-principal.md), [17](17-mapa-pines-teensy.md),
[18](18-teensy-unica-firmware.md), [27](27-leds-teclas.md), [28](28-pcb-k.md),
[29](29-banco-pruebas.md), [30](30-protoboard-validacion.md) y el
[banco del 12 de noviembre](mediciones/banco-2026-11-12.md). Contrastar con la revisión
**realmente fabricada**, no solo con estos esquemas todavía sin layout en esta base.
Usar el [README de PCB-M](../../hardware/kicad/consola/boards/pcb-m/README.md),
[ASSEMBLY](../../hardware/kicad/consola/boards/pcb-m/ASSEMBLY.md),
[tools/HARNESS.md](../../hardware/kicad/consola/tools/HARNESS.md) y los informes de
[revisión HW](../../hardware/kicad/consola/docs/revision-hw-2026-11-12.md),
[adversarial 1](../../hardware/kicad/consola/docs/revision-adversarial-2026-11-12.md) y
[adversarial 2](../../hardware/kicad/consola/docs/revision-adversarial-2-2026-11-12.md).
Los nombres funcionales de puentes/TP pueden tener sufijo `1` en la referencia anotada:
localizarlos por red y `docs/reference-map.csv` de PCB-M.

**Decisiones vigentes:** no cortar VUSB–VIN (D35, sustituye HW B-05 y texto histórico de 24 §3);
no poblar R1/R2 de PCB-A, o retirarlas si ya están montadas; K son dos placas (28 §7, sustituye la placa única histórica);
0x22/23 izquierda, 0x21 derecha. MIDI de PCB-M y e-drum quedan DNP.
Esta tarea, atendiendo el hallazgo M3, prescribe **100 kΩ tras RC** en cada pot; el esquema/README de esta base todavía
especifica **1 MΩ**. No dar por aplicado ese cambio de hardware: comprobar valor en la unidad,
registrar discrepancia y no aprobar detección de cable abierto con la hipótesis equivocada (18 §10).

Preparar lupa, multímetro con fusible operativo, puntas finas/aisladas, estación de soldadura,
soporte aislante, fuente de 5,0 V con límite **si Juan dispone de ella**, y preferiblemente
osciloscopio/medidor USB. No se presupone una fuente de laboratorio disponible.
Sin ella, medir corriente **en serie** con la rama LED, empezando en rango alto y borne con
fusible adecuado; comprobar su caída de tensión. El amperímetro **no limita corriente**:
F1 no es un limitador rápido. Cortos deben resolverse sin alimentar; pulsos iniciales breves,
interruptor accesible y cortar inmediatamente ante exceso. Nunca conectar el amperímetro
entre riel y masa. Para medir tensión, devolver las puntas al borne V y medir en paralelo.

Teoría: las etapas separan fuentes, cargas y señales; un ACK no acredita identidad ni tensión.
Una luz de riel tampoco mide tensión o ruido. Abrir alimentación puede dejar backfeed por
señales/clamps: aislar también cables, no solo un puente. Referencias de circuito y límites:
24 §10, hojas TI ADS1115, Microchip MCP6004/MCP4725 y NXP PCA9517A citadas allí;
las revisiones adversariales 1/2 documentan los riesgos de alimentación parcial.

## 2. Puerta 0: inspección y continuidad, todo desconectado

1. Desconectar **todos** los USB (Pi, Teensy, M4, LED), fuente, audio, CV y mazos. Descargar
   condensadores por resistencia; confirmar 0 V antes de medir Ω. Fotografiar ambas caras
   y anotar revisión/serie, piezas DNP y retoques de cada placa.
2. Buscar puentes de estaño, restos conductores, patas levantadas y soldaduras frías. En A/K
   terminar soldadura manual e inspeccionar antes de integrar. Probar cupón Choc/zócalo/chapa:
   no forzar el plástico en taladros provisionales. FSR: socket 2,54 mm, cola sin tensión ni
   pliegue forzado; no soldar la película. Verificar mecánica antes de presionar pads.
3. Orientación: marca de pin 1 de MCP23017 **SSOP28**, MCP6004 SOIC14, MCP4725 y ADS/PCA contra
   huella/BOM; puentes de dirección K=22/23 y21, A=20, RESET alto. No intercambiar SOIC/SSOP.
   SK6812MINI-EA inverso: **1 GND, 2 DIN, 3 VDD, 4 DOUT**, luz hacia el recorte; seguir 27/28.
   LEDs de riel: ánodo al resistor/riel, cátodo a su retorno. Condensadores polarizados + al riel.
   SMAJ5.0A: **K al riel protegido después de F1**, A a GND; PESD pedal:1 K señal/2 A GND.
   BAT54S: pin 1 GND (AGND en frente), pin 2 riel superior (3V3A frente/5V_LED CV),
   pin 3 señal protegida; comprobar contra BOM/red, no inferir de su dibujo. AO3401A: G1 GND/S2 carga/D3
   fusible. La segunda revisión M1 advierte símbolos TVS bidireccionales engañosos: manda la pieza.
4. **No cortar VUSB–VIN.** No unir +5V_TEENSY y +5V_LED. Backup de 5 V sin poblar ni conectar.
   R1/R2 de PCB-A ausentes; pull-ups opcionales de K DNP. NT1 es la única unión AGND/GND.
   Sleeve HP exclusivamente HP_VGND, aislado de chasis/GND/AGND; comprobar primero el mazo HP
   separado del shield. Pads sin rótulo del shield sin cable; usar la foto del reverso de README.
5. Medir cada placa aislada y ambos sentidos de puntas; esperar a que carguen los condensadores.
   Repetir entre **cada par de rieles** presente, además de cada riel a GND/AGND. La tabla es
   orientación de diagnóstico, **no límites de resistencia de producción medidos**: semiconductores
   hacen la lectura dependiente del multímetro. Un corto persistente obliga a parar.

| Placa aislada | Pares / lectura aproximada esperable sin alimentar |
|---|---|
| M sin Teensy/shield, puentes abiertos | +5V_LED_RAW, +5V_LED, +5V_TEENSY, +3.3V, +3.3VA a GND: típicamente kΩ o lectura creciente/OL, no 0 Ω estable. Entre rieles independientes: alta/no continuidad óhmica directa; diodos/LDO pueden conducir en un sentido. +5V_RAW→+5V_LED incluye F1/MOS, no exigir OL. +3.3VA→+3.3VA_FRONT abierto y +5V_LED→+5V_CV aislado. AGND↔GND por NT1 ≈0–1 Ω (restar puntas) |
| A, R1/R2 fuera, sin F/K | +3V3↔GND y +5V↔GND: kΩ/creciente/OL según cargas; +5V↔+3V3 sin corto directo. Clics a masa: abiertos en reposo, ≈0–1 Ω pulsados. Fases A/B alternan al girar; su reposo depende de la posición |
| K izquierda | +3V3↔GND, +5V_LED↔GND: carga capacitiva, luego alta (habitualmente kΩ); +3V3↔+5V sin puente óhmico. No fijar un Ω exacto a LEDs/MCP apagados |
| K derecha | Mismos pares que izquierda; medir aparte, sin el cable de seis vías. Tecla abierta alta; pulsada ≈0–1 Ω |
| F, cuatro B50K | +3V3↔GND ≈50k/4 = **12,5 kΩ** nominal (tolerancia de pots). Cada B50K fuera de circuito tiene extremos ≈50 kΩ y cursor 0–50 kΩ; montado hay caminos paralelos, no exigir 50 kΩ desde el cursor. Verificar variación continua; no circula +5 V |
| P, sensores sin presión | +3V3↔GND alta, dependiente del FSR; señal de cada divisor a GND ≈**10 kΩ** sin sensor/presión donde está poblado el pulldown. En V1–4 el 10 kΩ está en M: medirlo allí, no exigirlo en P separada. No circula +5 V |

Para todas: ≈0–2 Ω estable entre riel y GND o rieles independientes = **FAIL**; <100 Ω
persistente es sospechoso y requiere localizar la carga antes de encender, no declararlo normal.
Una lectura alta no garantiza ausencia de corto bajo tensión. Pot de panel: extremos ≈10k;
ADC→GND sin cable ≈100 kΩ con el montaje pedido (≈1 MΩ en esta base);
cursor hasta ADC ≈1 kΩ. Medir componentes en circuito puede incluir caminos paralelos.
Con pots conectados sus tres extremos dan ≈3,33k entre 3V3/GND, además de otras cargas.

6. Comprobar continuidad **contacto por contacto** de cada mazo suelto y ausencia de cruces con
   adyacentes según HARNESS. IDC 30 vías 1:1, raya pin 1; I2C A por J2 independiente, **no por IDC**.
   A.J4 antiguo sin cable. M–K diez vías:1 3V3/2 SDA/3 GND/4 SCL/5 GND/6 DIN/7 GND/8 5V/9 jog/10 pedal.
   K izq–der seis vías:1 3V3/2 GND/3 SDA/4 SCL/5 5V/6 datos. No identificar contactos por color solo.
   M4: confirmar físicamente llave/vista por continuidad (A1 de segunda revisión pendiente);
   GND común, **sin hilo 3V3**, TX29→SCL/RX22 M4 y RX28←SDA/TX21 M4. Sin evidencia no conectar.

## 3. Puerta 1: PCB-M sola, sin Teensy ni Audio Shield

1. Sin tensión, abrir **SJ_FRONT, SJ_CV, SJ_LED_K, SJ_SHARP, SJ_LED_DATA** y los pares
   **SJ_SDA_A/SJ_SCL_A, SJ_SDA_K/SJ_SCL_K**. Son `SolderJumper_2_Bridged`: de fábrica
   tienen cobre cerrado; retirar el puente de cobre de la huella, comprobar OL, luego podrán
   cerrarse con estaño. No cortar pistas fuera del puente. Dejar 2,2k seleccionadas por segmento,
   4,7k abiertas; EN GPIO cerrados. Retirar R_ISO_IN_L/R (0 Ω) para probar frente antes del códec.
   No hay cables remotos ni señal externa alguna.
2. Con fuente apagada, 5,0 V a J_5V_LED_IN.1 y GND a .2. **Nunca inyectar 5 V en 3V3 ni en VIN**.
   Si hay límite de corriente, iniciar **20 mA** para esta placa sin cargas; es un límite de
   prueba conservador, no consumo ya medido. Si entra en límite, apagar y diagnosticar antes de
   subirlo. Sin fuente limitada, usar alternativa en serie de §1, lectura inmediata y apagar.
3. Encender brevemente, medir TP de la tabla; si tensión/corriente son correctas mantener 30 s.
   No debe calentarse ningún IC/F1/MOS ni oler. Registrar corriente real y caída en protección.

| Punto (respecto a GND, salvo indicado) | Solo rama LED | Con Teensy/shield (§4) |
|---|---:|---:|
| TP_GND / TP_AGND | ≈0 V entre sí | ≈0 V entre sí |
| TP_5V_LED_RAW | 5,0 V nominal | 4,75–5,25 V |
| TP_5V_LED (tras F1/MOS) | próximo al RAW | 4,75–5,25 V bajo carga; comparar caída |
| TP_5V_TEENSY | ≈0 V | 4,75–5,25 V desde USB Teensy |
| TP_3V3 | ≈0 V | 3,20–3,40 V |
| TP_3V3A (a AGND) | ≈0 V | 3,20–3,40 V |
| TP_3V3A_FRONT (a AGND) | ≈0 V | ≈3,3 V al cerrar SJ_FRONT |
| TP_5V_CV | ≈0 V con SJ_CV abierto | próximo a LED menos caída 10Ω×I con SJ_CV cerrado |
| TP_VREF / TP_BUF_L/R (a AGND) | frente apagado: sin valor funcional | VREF 1,60–1,70 V; buffers en reposo próximos a VREF |
| TP_CV1/2_OUT y TP_CV1/2_PROTECTED (a AGND) | sin valor CV válido antes de DAC/firmware | cerca de 0 V a código 0; comparar antes/después del 1k |
| TP_SDA/SCL local, ambos lados de series A/K | sin 3V3: sin ACK/nivel operativo | reposo alto ≈3,3 V; VOL B puede ≈0,5 V |
| TP_LED_DIN_5V | separado por SJ_LED_DATA, no exigir trama | datos 0–5 V al cerrar; scope durante KEYLEDS |
| TP_MIDI_RX/TX | DNP, sin función | DNP, no probar Serial1 |

Los rangos son ventanas de cribado para banco; ≈0 V en rail apagado significa comprobar que
no se eleva persistentemente (si >0,3 V, detener y buscar backfeed), no una certificación de fugas.
El LED rojo de 5V debe encender; 3V3/3V3A apagados. Un LED encendido no sustituye el multímetro.

4. **Apagar antes de cada cambio.** Cerrar SJ_CV solo, encender, medir TP_5V_CV y caída de su
   RC de 10 Ω/47 µF; DAC aún apagados, no interpretar CV. Apagar; cerrar SJ_FRONT, medir de nuevo:
   sigue sin alimentación hasta USB Teensy. Luego SJ_LED_K, SJ_SHARP, SJ_LED_DATA, uno por ciclo,
   sin mazos: verificar que no aparece corto ni consumo anómalo. Reabrir SJ_SHARP y LED_DATA
   hasta sus pruebas. No aumentar corriente para vencer un corto; reabrir el último puente.
   Mantener SJ_CV cerrado al alimentar DAC posteriormente; abierto no protege entradas del MCP6004.

## 4. Puerta 2: Teensy + Audio Shield y panel local

1. Apagar fuente/USB. Insertar módulos con pin 1 y desplazamiento comprobados, sin microSD/flash
   del shield ni pot opcional en 15. Montar mazos shield por función/foto y HP aislado. Con SJ_FRONT
   cerrado y R_ISO retiradas, el códec aún está separado del frente. Jacks CV sin cargas externas.
   Conectar pots/selector/jog locales uno por ciclo apagado; comprobar cursor 0–3,3 V.
2. Energizar LED/CV primero; conectar USB Teensy a Pi después. No mezclar fuente de laboratorio y
   USB-A LED: usar **una u otra**, nunca ambas. Al terminar se usará solo la fuente oficial de Pi.
   El firmware pone ambos DAC a cero antes del códec y DMA. Medir 5V_TEENSY/3V3/3V3A, cerrar o
   revisar SJ_FRONT sin tensión si faltaba; ahora VREF/buffers deben cumplir la tabla. Tres LEDs
   rojos de riel encendidos. SJ_CV debe estar cerrado, TP_5V_CV presente.
3. Abrir CDC «GF Consola», `@GF SCAN`: **0A,48,49,60,61**, sin MCP todavía. `@S STATS`:
   `codec=1 cv=1 cv2=1`, errores dejan de crecer en régimen; `m4=0` esperado. Mínimo no sondea
   ADS49 ni todos los pots, aunque SCAN sí ve sus direcciones. Para aprobar todos los controles
   se necesita imagen **consola_uno_full** ya cargada (`pot_mask=7`, ADS mask3, selector activo).
   Si solo está mínimo, registrar controles como pendientes; no sustituir por PASS ni flashear aquí.
4. Enviar **una vez por unidad** `@GF CV EEPROM0`; esperar PENDING y respuesta final
   `VERIFIED=60,61 WRITTEN=3`. Fallo: guardar WRITTEN/VERIFIED y reparar sin reintento automático.
   ALREADY evita reescritura en ese arranque. No mandar el comando en cada encendido. Medir ambos
   TP OUT/PROTECTED y jacks a código 0: objetivo de banco **0–50 mV**, registrar offset real.
   Hacer POR de ambos DAC con todas las fuentes apagadas; encender LED primero y luego Teensy,
   medir también ventana anterior a setup con scope si disponible. EEPROM verificada no garantiza
   cero analógico ni mute durante reset. No conectar un sintetizador hasta comprobarlo.
5. Con silencio y sin fuente externa, medir VREF y seguidores. Apagar, reinstalar R_ISO_IN_L/R
   solo si pasan. Reproducir tono USB bajo, `@GF CUE 1`, HP bajo al comenzar; comprobar cue L/R.
   `@GF RUTA LINEA`: tono de 1 kHz por LINE IN L/R, captura USB1/2; pad0/−10/−20 debe dar relativo
   0/≈−9/≈−20 dB (30). Entrada en corto, grabar 30 s y comparar ruido con shield directo.
   Medir LINE OUT separado de salida de sala Audio+ del Pi. HP_VGND nunca a masa de scope.
6. CV1 en full: códigos0/2048/4095 y cinco puntos intermedios, sin carga y luego carga conocida
   registrada: ≈0/2,47/4,9 V, error<1% y rizado≤5 mV como criterio30. Medir OUT y PROTECTED;
   el 1k cae con carga. CV2 permanece cero. RC reduce ruido, no regula: doc24 advierte margen
   insuficiente posible cerca del máximo. Si falla 4,9 V, registrar FAIL y aislar; no cambiar ganancia
   ni poblar un LDO para esconderlo. Volver CV1 a 0 antes de abrir SJ_CV o perder USB LED.

## 5. Puertas 3–7: integrar una placa a la vez

Cada fila: **apagar todas las fuentes y esperar 0 V**, conectar solo la placa nueva, inspeccionar
pin 1/continuidad, encender LED→Teensy, medir sus rieles y corriente incremental, SCAN/INPUTS y banco.
Abrir el par I2C de esa rama antes de cablearla; cerrar **ambos** SJ SDA/SCL sin tensión al probar.
No retirar lo ya validado salvo diagnóstico. Ausencias esperadas por etapa no son fallo.

Desde la raíz: `python3 tools/consola/banco.py --placa <selección>` (o `cd tools/consola &&
python3 banco.py --placa <selección>`). `--puerto /dev/...` desambigua el puerto. Guardar el informe
físico de mediciones; `--simular` solo sirve para aprender el flujo, no aprueba hardware.

| Puerta / nueva placa | Conectar y probar | SCAN acumulado esperado |
|---|---|---|
| 3 · PCB-A | R1/R2 ausentes; IDC1:1 y J2 I2C separados, J4 antiguo libre. Cerrar SJ_SDA_A/SJ_SCL_A. `--placa pcb-a`: ocho encoders en ambos sentidos y ocho clics/pulsar-soltar | 0A,20,48,49,60,61 |
| 4 · K izquierda | M→K de 10 vías; cerrar SJ_LED_K y SJ_LED_DATA tras verificar 5 V. Cerrar SJ_SDA_K/SJ_SCL_K. `--placa pcb-k-izq`: 17 teclas y LEDs0–16 | 0A,20,22,23,48,49,60,61 |
| 5 · K derecha | Izq.J2→der.J1 seis vías, **nunca** al conector de 10 vías. `--placa pcb-k-der`: 9 teclas y LEDs17–25; probar ahora jog click88 y pedal122 del MCP23 | 0A,20,21,22,23,48,49,60,61 |
| 6 · PCB-F | Por PCB-A J3, cursores a ADS48 AIN0–3. `--placa pcb-f`: F1–4, recorrido completo y orden, no solo mínimo de 512 cuentas del banco | Igual; placa pasiva sin dirección propia |
| 7 · PCB-P | V1–4 a M FSR14–17; X1–2 a expresión/ADS49 AIN0/1 según HARNESS. `--placa pcb-p`: soltar para armar, golpe/velocity/release y presión sostenida; verificar pulldowns 10k | Igual; placa pasiva sin dirección propia |

LEDs: `@GF KEYLEDS N` enciende **los primeros N**, no el índice N; ciclo de color de 1 s, caduca a los 60 s.
Izquierda: N=1 BACK0; N=5 navegación hasta BROWSE4; N=11 hasta BREAK10; N=17 hasta NEW16.
Derecha: N=18 añade PLAY17, N=22 llega a METRO21, N=26 termina SCENE1_OCT+25.
Para comprobar **cada índice 0–25**, enviar N en este orden y verificar que solo se añade la
tecla indicada; los LED anteriores continúan en el ciclo. Si la GUI tiene brillo 0, ajustarlo
antes: el banco conserva ese brillo (27 §5). Contrastar colores GRB durante los cinco colores.

| N de `@GF KEYLEDS N` | Nuevo LED / tecla | N | Nuevo LED / tecla |
|---:|---|---:|---|
| 1 | 0 BACK | 14 | 13 DELETE |
| 2 | 1 MENU | 15 | 14 DUPLICATE |
| 3 | 2 NAV_L | 16 | 15 QUANTIZE |
| 4 | 3 NAV_R | 17 | 16 NEW |
| 5 | 4 BROWSE | 18 | 17 PLAY |
| 6 | 5 TAP | 19 | 18 STOP |
| 7 | 6 FILL | 20 | 19 REC |
| 8 | 7 CUE_ENG | 21 | 20 LOOP |
| 9 | 8 BUILD | 22 | 21 METRO |
| 10 | 9 DROP | 23 | 22 SCENE4_PAGE_R |
| 11 | 10 BREAK | 24 | 23 SCENE3_PAGE_L |
| 12 | 11 SHIFT | 25 | 24 SCENE2_OCT− |
| 13 | 12 UNDO | 26 | 25 SCENE1_OCT+ |

Tras cada prueba `@GF KEYLEDS 0` (vuelve al estado enviado por Pi; detener animación GUI si se
necesita oscuridad). Renovar N si se exceden los 60 s del banco.
Si falla a partir de un LED, revisar DOUT del anterior/DIN del primero oscuro, 5V y soldaduras,
no puentear piezas con tensión. Izquierda puede funcionar sin derecha; derecha necesita cadena anterior.

Añadir Sharp uno por ciclo apagado, cerrar SJ_SHARP, medir salida≤3,3 V hacia ADS49 AIN2/3,
ruido con LED/audio, capacidad local≥10µF. M4 se añade aparte tras verificar su mazo físico y
secuencia (§6); no tiene selección propia en banco. Probar sus 32 pads/notas48–79/LEDs y UART
ambos sentidos; `m4=1` es respuesta válida, no prueba del consumo. Después ejecutar `--placa pcb-m`
(con K izquierda para clic/pedal, y Sharp) y `--placa todo`. Cualquier canal sin muestra válida falla.

## 6. Puerta de sistema, secuencia y ensayos de fallo

1. **Arranque final con los 26 LEDs conectados desde antes de energizar**, no agregados después.
   Repetir al menos 10 arranques fríos/reset MCU, registrar cada uno: codec=1 al primer intento,
   codec_errors=wire_timeouts=0 en arranque sano, CV1/2 cero y sin flash LED inesperado. El banco
   encontró bloqueo del códec con datos LED conectados antes del arreglo 89d68e5; causa eléctrica
   de fondo no demostrada. Usar imagen que inicializa códec antes de DMA, sin bisect GF_NO_KEYLED_DMA.
2. Con fuente oficial de 27 W, Pi+pantalla+**NVMe/SSD y Audio+ finales**, M4, Sharp×2 y26 LED: medir
   reposo/pico con CPU+SSD activos. `vcgencmd get_throttled` antes/después:0x0. Banco previo de 5,12/5,05 V
   con 20 LED y sin NVMe no certifica este montaje. Rama LED≤0,6A (teclas estimadas≤0,5A), Teensy≤0,25A,
   M4≤0,6A, total USB≤1,6A; F1=0,75A sigue instalado. `key_led_ua` es estimación, medir corriente real.
3. Audio dúplex **60 min**, LEDs animando/heartbeat GUI y mover todas las entradas repetidamente.
   Después de setup/recuperación `@S RESET`, guardar STATS y `@GF USB` inicial/final y contadores
   de xruns del host. Pasa: cero drops de colas/USB/audio/xruns nuevos, FSR habilitados≥2000SPS
   cada uno, fsr_hz≥2000, gap≤500µs, loop_max_us<2000 estable, CPU total≤60%, sin notas colgadas,
   sin incremento de errores I2C/códec. `audio_cpu_max_x100` solo mide audio, no CPU total del loop.
   RX/TX_LESS/MORE de ajuste asíncrono no son drops; registrar todos sin confundirlos con UND/OVR.
4. Pots con 100 kΩ comprobados y full: mover MASTER/CUE/IN por encima del 5%, desconectar **solo el cursor** con jig
   que no pueda tocar 5 V (preparado apagado); comprobar crudo <41; tras **más de 200 ms** de muestras bajas, `pot_open_mask`
   vale 1/2/4 respectivamente si los otros dos pots están válidos (bits 0/1/2). Verificar
   CC30/31/32 y HP/IN retenidos sin nuevo cero. Reconectar cursor ≥41 cuentas: el bit se limpia
   y EMA/cadencia retoman.
   Comprobar también un transitorio bajo <200 ms: retiene desde la primera muestra sin marcar bit.
   Cronometrar desde el cruce <41, no desde la apertura del cable; una descarga RC previa puede
   haber enviado valores decrecientes. No se promete retención desde la apertura física.
   Probar también cero real y registrar la ambigüedad de 18 §10.
   No usar esta prueba para autorizar hot-plug de los mazos de placas.
5. **Hot-plug y pérdida de cada USB son ensayos condicionados**, no instrucciones para dañar módulos.
   Revisiones A2/A3 de la segunda adversarial siguen abiertas en esta base: DAC activo→MCP6004
   sin 5V_CV y M4 activa→Teensy apagada no son seguros por tener resistencias/GND común.
   Si la revisión recibida no incorpora solución verificada de alimentación parcial, registrar
   **BLOQUEADO por revisión**, no retirar ese USB con señales activas ni declarar sistema aprobado.
   Primero ensayar con UART M4 desconectada, CV a 0 confirmado y jacks externos sin carga; antes de
   una pérdida no controlada se necesita aislamiento eléctrico documentado, no un comando previo.

| Ensayo controlado, tras cerrar riesgos eléctricos | Medir / comportamiento a registrar |
|---|---|
| Quitar/reponer USB LED | CV a 0 previo; Sharp/LED/CV pierden función, Teensy/3V3/3V3A siguen. Medir rail fantasma5V_CV e inputs del operacional; audio/controles no dependientes siguen, sin drops. Si CV no cero o aislamiento no resuelto, bloquear |
| Quitar/reponer USB Teensy | UART M4 aislada previamente; comprobar 3V3/3V3A caen sin backfeed desde Sharp/CV. Audio/MIDI/CDC se pierden de forma esperada; reenumeran y códec/CV recuperan tras reconectar. No exigir audio continuo sin su USB |
| Quitar/reponer USB M4 | Validar primero sentido Teensy→M4 apagada; si falta aislamiento bloquear. Solo M4 cae; no notas colgadas ni caída de audio/teclas. Reconectar, m4=1 y snapshot LED vuelve |
| Pérdida USB de MIDI externo en Pi | Sin notas colgadas; UI informa desconexión/reconexión. No afecta audio ni controles de las seis placas |
| Hot-plug de datos LED/K/I2C | No realizar de rutina. Solo tras calificar alimentación parcial, mazo/jig que asegure masa y no cruce pines, monitorizar scope/corriente y guardar STATS. Tres ciclos como banco anterior; sin caída de audio, notas colgadas ni errores crecientes. Fallo corta ensayo y vuelve a servicio apagado |

Medir también con fusible abierto/SJ_CV abierto **solo sin señales que realimenten**. No usar fallos
forzados con cortos ni invertir alimentación. Para EN, tres fallos MCP por rama solicitan recuperación,
ambos flancos con bus ocioso≥10µs/pulso≥1ms, contadores i2c_reset_k/a separados. Si SDA/SCL quedan bajas,
no forzar EN: apagar/aislar según §7. Arrancar diagnóstico a 100 kHz requiere imagen de banco ya preparada;
el firmware normal usa 400 kHz. No afirmar 100 kHz sin comprobarlo; validar subida30–70%/VOL con mazo real
antes de aceptar 400 kHz, cambiando pull-ups de ambas líneas juntas y por segmento.

## 7. Si una puerta falla: apagar, aislar, medir, repetir esa puerta

| Síntoma / etapa | Aislamiento sin tensión y medición siguiente |
|---|---|
| Corto o exceso al primer 5 V | Desenchufar entrada; abrir CV/LED_K/SHARP/DATA, todos los mazos fuera. Medir RAW→F1→MOS→LED. RAW bien/LED ausente: F1 abierto/MOS invertido. Corto previo a puentes: TVS, MOS, buffer y capacitores locales; no puentear F1 |
| 3V3 ausente /5V_TEENSY ausente | USB/cable/socket Teensy; no cortar VUSB–VIN. Desenchufar ramas A/K/panel; si solo3V3A cae, comprobar TLV755 IN/EN/OUT y abrir FRONT. Si ADS local retiene riel/bus, retirar/aislar componente averiado |
| VREF/entrada ruidosa o saturada | Abrir SJ_FRONT **y retirar R_ISO_IN_L/R**, desconectar entrada externa ante backfeed. Medir3V3A/FRONT/VREF/BUF L/R, bias y clamps. Cue/salida pueden seguir sin LINE IN. JP_BYP solo tras retirar operacional defectuoso; nunca luchar contra una salida activa |
| CV incorrecto, calor o rail fantasma | Desconectar jacks CV, llevar DAC0 si es seguro; apagar y abrir SJ_CV, aislar también señales/alimentación de DAC si inyectan o retienen I2C. Medir5V_CV, caída10Ω, OUT/PROTECTED y1k. Continuar sin CV solo con aislamiento completo, no SJ_CV abierto con DAC no cero |
| SCAN pierde A o bloquea local | Abrir SJ_SDA_A y SJ_SCL_A, quitar IDC **y** J2 de A para aislar alimentación/señales. Medir SDA/SCL local y lado cable/47Ω/TVS; RESET/dirección20, R1/R2 ausentes. No buscar SDA/SCL en J1_PCBA |
| SCAN pierde K o códec al conectarla | Abrir SJ_SDA_K/SJ_SCL_K y desconectar mazo10 vías; abrir SJ_LED_K y SJ_LED_DATA. Recuperar local, luego K izquierda y finalmente cable6 vías/derecha; medir 3V3, pull-ups y VOL/subida. No unir lados B de los dos PCA |
| LEDs oscuros, permutados o resets | Abrir SJ_LED_K/SJ_LED_DATA y retirar mazo. Medir5V_LED/5V_K, OE bajo, dato13 antes/después de buffer/serie33Ω, DIN/DOUT y soldaduras. No sacar5V de3V3. Si códec falla al arranque, guardar comparación con 26 LED y aislamiento DATA, sin declarar resuelto por desconectarlos |
| Sharp perturba ADS/audio | Abrir SJ_SHARP, quitar señal y alimentación de sensores; medir 5V, desacoplo10µF, señal≤3V3 y serie10k. Abrir solo alimentación puede dejar inyección por otros caminos |
| Pot abierto o no recorre | Revisar100k tras RC,1k/100nF, cursor/GND/3V3 y pin22/24/26; mirar raw INPUTS antes de EMA y pot_open_mask. Mínimo pot_mask0 no prueba presencia. No resetear diagnóstico para esconder el fallo |
| F/P o tecla no responde | Aislar placa nueva; continuidad de cursor/divisor10k, FSR sin tensión mecánica; pulldowns y máscara válida. K: RESET/dirección/GPIO según28; no usar GPA7/GPB7 como entrada |
| USB/audio/M4 se cae | Registrar tensiones/corrientes, throttled, USB y STATS antes de apagar. Volver al último conjunto aprobado; revisar HP_VGND, UART cruzada por función, USB de datos y pérdidas parciales antes de reconectar |

Para EN alto manual: apagar y abrir **SJ_EN_K_GPIO o SJ_EN_A_GPIO** correspondiente; su pull-up10k
queda activo sin pelear contra GPIO bajo. Esto no repara un bus retenido bajo ni sustituye abrir SDA/SCL.
En cada reparación repetir inspección/Ω, puerta afectada y finalmente 60 min; dejar función aislada anotada.

## 8. Criterios y hoja de registro

| Puerta | PASS | FAIL / pendiente que impide avanzar |
|---|---|---|
| 0 · inspección/Ω | Sin cortos; polaridad, pin 1, R1/R2 DNP, VUSB intacto y HP aislado; mazos medidos | Corto persistente, discrepancia de montaje/valor o UART sin identificar |
| 1 ·5V sin módulos | Tabla de TP, corriente estable sin calor; cada puente probado por separado | Límite activo, >0,3V persistente en rail apagado sin explicación, caída o calentamiento |
| 2 · módulos | codec1, SCAN local completo, VREF1,60–1,70, audio L/R, ambos CV0 verificados/EEPROM/POR | Error creciente, offset/rango fuera de criterio, backfeed o canales cruzados |
| 3–7 · remotas | SCAN acumulado correcto, todas las entradas y26 LED en orden; informes físicos PASS | Falta ACK/muestra/control o corriente fuera del presupuesto |
| Sistema |10 arranques sanos,60 min sin drops/xruns, SPS/gap/loop/CPU y consumo de §6 | Error nuevo, bloqueo, nota colgada, tensión/consumo fuera de rango |
| Fallos/USB | Aislamiento parcial demostrado y recuperación observada por cada caso | Riesgos A2/A3 abiertos: BLOQUEADO, no PASS por omisión |

Copiar el bloque siguiente a `mediciones/puesta-en-marcha-AAAA-MM-DD-unidad.md`.
No rellenar cifras previstas como medidas; usar **NO MEDIDO / BLOQUEADO** cuando corresponda.

```markdown
# Puesta en marcha · fecha · unidad
Operador: / revisión PCB y serie de las seis placas:
Firmware commit / env / pot_mask / pot_open_mask / instrumentos y rangos:
Fuente (limitada sí/no; límite A) / cables / temperatura ambiente:
Fotos / revisión de fabricación / discrepancias con docs24/28/M3:
VUSB–VIN intacto: / R1-R2 A DNP: / HP_VGND aislado:
Puentes iniciales / R_ISO_IN retiradas / pull-ups por segmento:

| Placa | Par medido (Ω, dos polaridades) | Inicial / estabilizado | Resultado |
|---|---|---|---|
| M / A / K izq / K der / F / P | | | |

| Etapa / hora | TP y retorno | Esperado | Medido V | Corriente A / pico | Resultado |
|---|---|---|---|---|---|
| LED solo / cada SJ / módulos / cada remota | | | | | |

| Etapa | Puentes cerrados / cargas | SCAN real | STATS / INPUTS / informe banco | PASS/FAIL |
|---|---|---|---|---|
| | | | | |

CV EEPROM0 respuesta inicial/final (una provisión por unidad):
CV1/2 código 0, OUT/PROTECTED/jack, antes de setup/POR/reset:
CV rango/carga/linealidad/rizado: / VREF / BUF L/R / audio pad y ruido:
LED0–25: nombre, orden, colores, corriente real / key_led_ua estimado:
Pots: valor pulldown, raw abierto, tiempo, máscara y CC/HP/IN retenidos/retorno:
FSR/Sharp/selector: extremos, reposo, velocidad y calibración pendiente:
Boots1–10 (con 26 LED), codec_errors / wire_timeouts:
60 min: hora inicio/fin, configuración audio/host, actividad de entradas/LED:
STATS y @GF USB completos inicial/final; xruns host / CPU total:
Pi/NVMe/Audio+/pantalla/M4/Sharp conectados, get_throttled antes/después:

| Fallo inyectado / USB | Aislamiento seguro previo | Rieles/señales/corriente | Recuperación | Resultado |
|---|---|---|---|---|
| LED / Teensy / M4 / MIDI / hot-plug | | | | |

Incidencia / puerta detenida / jumper o 0Ω aislado / reparación:
Repetición de puerta y60 min / función temporalmente fuera de servicio:
Pendientes bloqueantes (A2/A3, M3, mazo M4, otros):
Dictamen: PASS / FAIL / BLOQUEADO. Firma y fecha:
```
