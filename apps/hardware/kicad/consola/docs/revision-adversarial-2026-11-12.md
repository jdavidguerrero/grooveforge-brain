# Revisión adversarial de GrooveForge Console

**Fecha:** 2026-11-12. **Base revisada:** `claude/monorepo-groovepush`, commit `e6a61ad29873bcab02bc3dc0273a177098b968c8`, clon independiente `/private/tmp/grooveforge-review` creado con `--no-hardlinks`.

**Dictamen:** el PASS de los verificadores no alcanza para liberar este diseño al pedido. Encontré un enlace UART conectado TX–TX/RX–RX según el firmware efectivo, dos códigos de compra que identifican piezas distintas, estados eléctricos fuera de especificación al perder una alimentación y una salida CV reservada que queda activa a media escala. Las incertidumbres de montaje se identifican como tales; no las presento como fallos físicos ya medidos.

## Alcance y método

Leí los specs 17, 18, 24, 27, 28 y 30, la medición `banco-2026-11-12.md`, las descripciones, XML, esquemas, huellas, contratos de mazos y firmware de superficie/Teensy. Para resolver el significado real de RX/TX también seguí el firmware de NeoTrellis y el core Adafruit instalado, contrastándolo con la fuente oficial. Consulté fuentes primarias de fabricantes y el catálogo del distribuidor para identidad de compra.

No regeneré esquemas, exportaciones ni grafos; no ejecuté las CLI que escriben artefactos ni pytest. Ejecuté únicamente funciones de comparación/lectura en memoria con `PYTHONDONTWRITEBYTECODE=1`. Resultado contra los XML existentes:

| Diseño | Componentes | Pines | NC | Redes | Comparación |
|---|---:|---:|---:|---:|---|
| PCB-M | 210 | 587 | 24 | 140 | PASS |
| PCB-K izquierda | 78 | 241 | 8 | 58 | PASS |
| PCB-K derecha | 43 | 127 | 5 | 32 | PASS |
| PCB-F | 10 | 32 | 0 | 6 | PASS |
| PCB-P | 12 | 24 | 0 | 12 | PASS |

`check_cables()` leyó las seis placas, incluida PCB-A: **29 cables, cero errores**. Eso reproduce la correspondencia declarada; no constituye un ERC/DRC nuevo ni una validación física.

Las rutas `boards/`, `tools/`, `lib/` y `fab/` se resuelven bajo `apps/hardware/kicad/consola/`; las rutas que empiezan por `apps/` son relativas al clon. Las menciones abreviadas a main.cpp y Surface.cpp conservan la ruta completa indicada en su hallazgo. Cada hallazgo cita líneas de esta base o pines/redes comprobados en sus XML. No se modificó ningún archivo del repo ni se hizo push. Esta entrega escribe exclusivamente este informe.

## Hallazgos, por severidad

### 1. CRÍTICO — El contrato de NeoTrellis invierte las funciones del UART real y conecta dos salidas entre sí

**Evidencia.** `apps/hardware/kicad/consola/tools/schgen/pcb_m.py:393–402,524` conecta Teensy 29/TX7 → `M4_TX7` → `J3_M4.3`, y Teensy 28/RX7 ← `M4_RX7` ← `J3_M4.4`. `tools/harness.py:82–84` define el contacto remoto .3 como **SDA = RX** y .4 como **SCL = TX**. El contrato pasa precisamente porque esa interpretación se repite en ambos extremos.

Pero `apps/consola/processor/lib/neotrellis_m4/UartInterface/UartInterface.cpp:10–21,34–35` llama al **Serial1 predefinido** y describe SDA/pin21 como **TX**, SCL/pin22 como **RX**. La selección de placa es `adafruit_trellis_m4` (`apps/consola/processor/platformio.ini:33–49,132–134`). Los macros contrarios de `include/shared/Config.h:55–56` no se usan para construir otro UART.

Lo confirmé en el core instalado: `~/.platformio/packages/framework-arduino-samd-adafruit/variants/trellis_m4/variant.h:127–128,155–156`: RX=22, TX=21, SDA=21, SCL=22; `variant.cpp:113` instancia Serial1 con esos macros. La [fuente oficial Adafruit](https://raw.githubusercontent.com/adafruit/ArduinoCore-samd/master/variants/trellis_m4/variant.h) coincide, al igual que su [instancia de Serial1](https://raw.githubusercontent.com/adafruit/ArduinoCore-samd/master/variants/trellis_m4/variant.cpp).

**Falla.** Con el cable funcional descrito: Teensy TX29 → SDA/TX21 de M4, y Teensy RX28 → SCL/RX22. No hay enlace UART; cuando uno transmite bajo y el otro permanece alto hay contención entre salidas sin resistencia serie. La magnitud de corriente y daño no se midió, pero el cruce funcional está demostrado. Un cable de banco cruzado de otra manera puede funcionar y no validar este mazo.

**Corrección propuesta.** Llevar TX29 a **SCL/RX22**, RX28 a **SDA/TX21**; fijar el mapa por función y por contacto físico de CONN1, y corregir contrato/spec/macros discordantes. Verificar continuidad y ambas direcciones con el firmware real antes de conectar dos TX. No basta intercambiar las etiquetas del checker.

### 2. ALTO — C7466 no es el buffer de LEDs: es un inversor que cabe en el mismo tipo de encapsulado

**Evidencia.** `boards/pcb-m/README.md:184` prescribe `U_LED_BUF = 74AHCT1G125GW / C7466 / SC-70-5`. La descripción usa `74AHCT1G125`, SOT-353, OE pin1 a GND, entrada pin2 desde LED_DATA y salida pin4 (`tools/schgen/pcb_m.py:312–315,563–564`). Sin embargo, [LCSC C7466](https://www.lcsc.com/product-detail/C7466.html) identifica **TI SN74AHC1G04DCKR**, un inversor SOT-353. La [hoja TI](https://www.ti.com/lit/ds/symlink/sn74ahc1g04.pdf) confirma la inversión y los umbrales AHC.

**Falla.** Si se cierra la BOM desde esa tabla, una selección por código compra el inversor. Puede no saltar una alarma mecánica porque el encapsulado es compatible en apariencia: se invierte la trama y el reposo del LED; además AHC no ofrece los umbrales TTL de AHCT para convertir 3,3→5 V. La cadena no funciona correctamente. El esquema puede seguir pasando con el valor correcto mientras el material comprado es incorrecto.

**Corrección propuesta.** Sustituir el código por una identidad verificada del **74AHCT1G125GW**, y llevar MPN/LCSC al componente exportable, no solo a una tabla. [C12495](https://www.lcsc.com/product-detail/C12495.html) es un candidato de catálogo para Nexperia 74AHCT1G125GW,125; hay que confirmar disponibilidad de ensamble JLC al cerrar el pedido. No he reservado stock.

### 3. ALTO — C15578 identifica un regulador SOT-223, incompatible con la huella del LDO analógico

**Evidencia.** `boards/pcb-m/README.md:185` asocia TLV75533PDBV a **C15578**. `tools/schgen/pcb_m.py:292–295,553` exige SOT-23-5, con IN=1, GND=2, EN=3, NC=4, OUT=5. [LCSC C15578](https://www.lcsc.com/product-detail/C15578.html) identifica **TLV1117LV33DCYR, SOT-223**, confirmado por la [hoja TI](https://www.ti.com/lit/ds/symlink/tlv1117lv.pdf).

**Falla.** La BOM de referencia lleva a una pieza que no puede montarse sobre los cinco pads previstos. JLC tendría que detener/rechazar la selección o solicitar cambio; si se omite la pieza, quedan sin alimentación los ADS y el frente analógico. Es una discrepancia real de identidad, no una cuestión de stock.

**Corrección propuesta.** Cerrar la identidad del TLV75533 en SOT-23-5; [TLV75533PDBVR/C404027](https://www.lcsc.com/product-detail/Voltage-Regulators-Linear-Low-Drop-Out-LDO-Regulators_Texas-Instruments-TLV75533PDBVR_C404027.html) es un candidato coherente. Verificar pinout, variante y disponibilidad de ensamble, y exportar el código desde el esquema.

### 4. ALTO — Quitar el USB de LEDs deja entradas analógicas activas sobre chips sin alimentación

**Evidencia.** `tools/schgen/pcb_m.py:553–562` obtiene `+3.3VA` del USB de LEDs, mientras `+3.3V` viene de Teensy (`393–402`). Los ADS reciben `+3.3VA` en pin8 (`429–433`), pero los faders se alimentan desde `+3.3V` por `J1_PCBA.30` y PCB-A/F (`516–520`; `tools/schgen/passive_boards.py:14–21`). Los FSR de expresión también salen de `+3.3V` hacia `J7_EXP1/J8_EXP2`, con señal directa a ADS2.4/.5 (`427–431,529–531`). El XML de PCB-M confirma, por ejemplo, `FADER1_SIG = J1_PCBA.16 ↔ U_ADS1.4` y `EXP1_ADC = J7_EXP1.2 ↔ R_EXP1.1 ↔ U_ADS2.4`.

Además, los DAC siguen a `+3.3V` y sus salidas van directamente a U_OA_CV.3/.5; el operacional depende del otro USB mediante SJ_CV y el RC (`440–465,587–591`). Abrir SJ_CV conserva esas conexiones de entrada.

**Falla.** Teensy encendida y USB de LEDs ausente, fusible abierto o alimentación retrasada: VDD de los ADS cae mientras un fader/FSR entrega tensión positiva. La [hoja ADS1115, §5.1](https://www.ti.com/lit/ds/symlink/ads1115.pdf) limita AIN a **VDD+0,3 V**. Puede haber inyección y alimentación parásita; su magnitud depende de la posición/resistencia del sensor. No afirmo daño medido. Tampoco atribuyo este problema a SDA/SCL: la misma hoja admite hasta 5,5 V en entradas digitales.

En CV, un DAC de fábrica produce 1,65 V antes de recibir comandos (hallazgo 5), sobre un MCP6004 con VDD=0. La [hoja MCP6004 DS20001733L, §1](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP6001-1R-1U-2-4-1-MHz-Low-Power-Op-Amp-DS20001733L.pdf) da un límite de entrada analógica **VDD+1,0 V** y ±2 mA; no hay resistencia entre DAC y entrada. El estado no está especificado como operación válida. No supongo un diodo positivo convencional: esta familia tiene una estructura de protección particular (§4.1.2).

**Corrección propuesta.** Diseñar explícitamente la pérdida de cualquiera de los dos USB: alimentar excitación y receptor del mismo dominio cuando sea viable, o añadir aislamiento/secuenciación/protección cuya operación sin alimentación esté garantizada. Un pull-down o el timeout de firmware no soluciona una entrada forzada por otro dominio. Validar también apertura de SJ_CV y caída lenta de rieles; los PCA de las ramas remotas no aíslan estas conexiones locales.

### 5. ALTO — CV2 queda a aproximadamente 2,475 V de fábrica; CV1 tampoco nace necesariamente en cero

**Evidencia.** `tools/schgen/pcb_m.py:439–453,543` puebla ambos MCP4725, sus amplificadores de ganancia 1,5 y los jacks. `apps/firmware-teensy/src/consola_uno/main.cpp:13–17,76–89` tiene un solo estado DAC y solo escribe/sondea **0x60**; no hay inicialización de 0x61 en ese firmware ni en surface. `apps/docs/consola/18-teensy-unica-firmware.md:21` declara 0x61 reservado, pero eso no desconecta su salida física.

La [hoja MCP4725 DS22039D, §5.4 y tabla 5-3](https://ww1.microchip.com/downloads/en/devicedoc/22039d.pdf) especifica EEPROM de fábrica a **D11=1, restantes datos=0, modo normal**, es decir 0,5×VDD. POR restaura EEPROM. El firmware deliberadamente solo hace escrituras volátiles.

**Falla.** Con chips vírgenes y ambos rieles presentes, CV2 entrega nominalmente `3,3×0,5×1,5 = 2,475 V` antes del resistor/carga, y permanece ahí. CV1 lo hace hasta su primera escritura, que ocurre después de la inicialización del audio. Un reset solo de Teensy conserva el último valor del DAC alimentado. «Reservado» o «variable cv=0» no garantiza un jack a cero.

**Corrección propuesta.** Definir estado seguro para ambos canales: inicializarlos/estacionarlos incluso si uno está reservado, y usar desconexión/mute de salida o DNP del canal reservado si se exige cero antes de firmware o durante reset. Provisionar EEPROM a cero una sola vez puede complementar esa solución, sin escribirla en cada arranque. Ensayar power-on, reset MCU e interrupción de cada USB por separado.

### 6. ALTO — La interfaz de Audio Shield está representada como conectores funcionales, sin contrato físico suficiente para montar el módulo

**Evidencia.** `tools/schgen/pcb_m.py:17–24,403–409` crea `J_SHIELD_AUDIO` **1×4** con IN_L/IN_R/OUT_L/OUT_R consecutivos y `J_SHIELD_HP` **1×3** L/R/VGND. `tools/harness.py:92–95` excluye ambos, al igual que los sockets Teensy, de la validación del mazo. La decisión de módulo en headers está en `apps/docs/consola/24-placa-principal.md:114`.

[PJRC describe el header de línea como compatible con AC97](https://www.pjrc.com/store/teensy3_audio.html), no como esta fila funcional de cuatro señales. El repo no aporta un mapa de esos cuatro contactos al header real ni un adaptador que haga esa conversión. Tampoco hay layout que demuestre orientación de los sockets.

**Falla.** Interpretar el 1×4 como huella directa del header del shield, o cablear por «pin1–4» suponiendo identidad, no está justificado. Puede cruzar canales, retornos o contactos sin señal. Un adaptador cableado seleccionado por función sí podría ser válido; **no he verificado la numeración física completa del header/HP de la revisión exacta que tiene Juan**, por lo que no afirmo un corto concreto en ellos.

**Corrección propuesta.** Usar símbolo/huella del módulo real o documentar expresamente el adaptador, con contacto real ↔ contacto genérico y orientación superior/inferior, incluidos retornos y HP_VGND. Verificar sobre el módulo disponible. Mantener la advertencia de que el retorno de audífonos no es masa. La revisión del orden de las dos filas Teensy no encontró un desplazamiento lógico; su montaje físico sigue pendiente.

### 7. ALTO — La protección del pedal que PCB-K presupone no existe en PCB-M

**Evidencia.** `apps/docs/consola/28-pcb-k.md:139` dice que la protección del jack externo está en PCB-M antes del cable. En cambio, `tools/schgen/pcb_m.py:544,525–526` conecta el contacto exterior directamente a `PEDAL_BTN` y J_PCB_K.10. El XML `boards/pcb-m/docs/pcb-m-netlist.xml:6954–6957` contiene **solo J_PEDAL.1 y J_PCB_K.10** en esa red. En K, `tools/schgen/pcb_k.py:21–25,68–86` la lleva a **U23.1/GPB0**, sin protección. La pull-up es interna y la activa `apps/consola/surface/src/Surface.cpp:235–243`.

**Falla.** Una descarga o transitorio del cable exterior entra directamente en el GPIO del MCP. Las protecciones USBLC6 solo cubren SDA/SCL (`pcb_m.py:659–664`), no este hilo. Puede causar resets, falsas pulsaciones o daño del MCP y afectar al bus compartido. No se ha ensayado inmunidad; no asigno una tensión de descarga ficticia.

**Corrección propuesta.** Añadir acondicionamiento para contacto externo cerca de J_PEDAL: limitación serie, protección ESD adecuada al nivel de 3,3 V y, si procede, filtrado/pull-up externo. Dimensionar el circuito contra la hoja del MCP y el ensayo esperado. Verificar que las señales del jog no se confundan con esta entrada exterior protegida.

### 8. ALTO — La alternativa hot-swap sigue sin demostrar que el cuerpo del zócalo cabe en la huella dual

**Evidencia.** `lib/gf_consola.pretty/Kailh_Choc_V2_PG1353_Hotswap.kicad_mod:86–106` tiene contactos PTH Ø1,3 en (0;5,9)/(5;3,8), además de pads SMD inferiores en (−3,275;5,9)/(8,275;3,8). `apps/docs/consola/28-pcb-k.md:68–72` reconoce que sustituyen aperturas NPTH de 3 mm y que aún debe verificarse el plástico del zócalo. `tools/tests/test_manual_assembly.py:19–44` comprueba separación de cobre y anillo, pero no volumen de carcasa ni inserción. La decisión elegida es zócalos (`28:44–46`).

**Falla.** Puede pasar DRC y no entrar el zócalo, o quedar separado de la cara B, sin que la buena separación cobre–cobre ayude. La soldadura directa evita depender del zócalo, pero no cumple automáticamente el ensamble hot-swap elegido. Es una **incertidumbre mecánica bloqueante del montaje elegido**, ya reconocida en el repo; no una interferencia física que yo haya medido.

**Corrección propuesta.** Verificar el conjunto PG1353+CPG135001S30 en cupón con espesor real y chapa. Separar las alternativas de huella si los alojamientos son incompatibles. Consulté la [hoja Kailh KH-PS-1702-35 Rev B](https://www.kailhswitch.com/Content/upload/pdf/202115927/CPG135001S30-data-sheet.pdf), pero no pude confirmar de forma legible las cotas/vistas del alojamiento; no certifico Ø1,3 ni las coordenadas SMD por ese dibujo.

### 9. MEDIO — EN está habilitado por defecto y no ofrece recuperación automática del códec frente a una rama averiada

**Evidencia.** `tools/schgen/pcb_m.py:638–640,665–677` une ambos EN, pull-up 10 kΩ a 3,3 V y puente cerrado a pin0. `apps/docs/consola/24-placa-principal.md:255–257` pide dejar pin0 como entrada; no se inicia Serial1 en el firmware de Teensy revisado. `main.cpp:153–174` inicializa códec antes de surface; `main.cpp:43–74` reintenta el códec sin gestionar EN. `Surface.cpp:471–501` arranca los LEDs y el bus sin manejar ese GPIO.

**Falla.** No encontré la supuesta deshabilitación permanente por «falta de código EN»: la pull-up habilita nominalmente los buffers con el GPIO en alta impedancia. El problema inverso sí existe: una rama SDA/SCL retenida en bajo se propaga al bus local y puede impedir inicializar/reconfigurar SGTL5000. Los reintentos no aíslan la rama. Ambos EN comunes impiden seleccionar una sola por software.

El banco distingue el plazo I2C corregido (`mediciones/banco-2026-11-12.md:9–11`) de una causa eléctrica no identificada con LEDs (`41–52`). Mover init antes del DMA corrigió el banco, pero si el primer intento falla, los reintentos ocurren con DMA ya activo. No demuestro que esa causa reaparezca en PCB-M con buffer; tampoco la doy por eliminada.

**Corrección propuesta.** Si se requiere audio disponible ante fallo de superficie, definir aislamiento/recuperación explícito y probar arranque con cada rama retenida en bajo. Resolver el estado de EN durante reset con hardware/firmware coordinados. No bajar/subir EN arbitrariamente en plena transacción: [NXP PCA9517A, §6](https://www.nxp.com/docs/en/data-sheet/PCA9517A.pdf) exige cambios con bus ocioso. Medir el arranque real de pin0/EN; no afirmo su forma de onda durante ROM/bootloader sin osciloscopio.

### 10. MEDIO — Hay estados flotantes sin definir al resetear la Teensy o retirar cables de perillas

**Evidencia.** El buffer LED tiene OE permanentemente bajo y entrada A directamente desde pin13 (`tools/schgen/pcb_m.py:563–564`), sin pull-down externo. El primer LOW explícito llega en `key_led_dma::begin()` (`apps/consola/surface/src/KeyLedDma.cpp:47–55`), después de inicializar el códec (`main.cpp:159–170`). La rama LED puede permanecer alimentada por otro USB.

Los pots tienen 1 kΩ serie y 100 nF a GND, pero ninguna resistencia DC del ADC a un nivel conocido (`pcb_m.py:479–482`). `ConsolaMap.h:37` habilita los tres en FULL. El propio spec `apps/docs/consola/18-teensy-unica-firmware.md:112` exige pull-down externo si el cursor puede desconectarse.

**Falla.** Durante reset/boot, el nivel del buffer LED depende del estado del pad, del LED integrado y de la rampa de alimentación; el circuito no asegura un reposo externo definido. No afirmo que necesariamente emita una trama válida. Con una perilla desconectada el condensador solo conserva carga transitoriamente; después el ADC puede derivar y el firmware FULL convertirlo en cambios de ganancia de entrada o cue. EMA/deadband no identifica cable roto.

**Corrección propuesta.** Definir el reposo de A/LED_DIN con polarización externa o control seguro de OE, y probar reset con LEDs alimentados. Añadir polarización DC apropiada a los ADC de pots, recalibrando la carga introducida, o declarar/deshabilitar de forma explícita entradas ausentes. No considero necesario inventar una vulnerabilidad de back-powering por A del **Nexperia correcto**: su [hoja](https://assets.nexperia.com/documents/data-sheet/74AHC_AHCT1G125.pdf) especifica entradas tolerantes a sobretensión y fuga con VCC=0.

### 11. MEDIO — PCB-P todavía depende de una terminación FSR no identificada

**Evidencia.** `tools/schgen/passive_boards.py:27–40` asigna los seis sensores a `FSR402_SolderTail_Provisional`. La huella `lib/gf_consola.pretty/FSR402_SolderTail_Provisional.kicad_mod:44–51` usa dos pads SMD de 2×4 mm, paso 2,54 mm, sin pasta ni taladros. Su descripción declara que no es una terminación certificada. `fab/pedido-v1.md:74` identifica los sensores disponibles solo como FSR 402.

**Falla.** Si las piezas disponibles tienen otro terminal o requieren conector, el landing no permite el ensamble previsto o la manipulación de la cola puede dañarlas. [Interlink confirma cuatro opciones de conexión para FSR 402](https://www.interlinkelectronics.com/fsr-402); el nombre de familia no determina la terminación. No pude verificar el SKU/cola real de Juan ni su proceso de soldadura, y no afirmo que todas las variantes admitan o prohíban soldar.

**Corrección propuesta.** Identificar la variante física y medir terminales/recorrido/pliegue antes del layout; escoger una terminación autorizada o un adaptador con alivio mecánico. La ausencia de F.Paste aquí **no es por sí sola un defecto de JLC**, dado el ensamble manual previsto.

### 12. BAJO — El spec LED27 conserva un pin de datos que ahora es masa

**Evidencia.** `apps/docs/consola/27-leds-teclas.md:8` indica entrada en **J1.5** de K izquierda. El contrato actual de diez vías pone J1.5 a **GND** y LED_DIN en **J1.6** (`tools/schgen/pcb_k.py:27`, `apps/docs/consola/28-pcb-k.md:129–135`). Ambos XML y el mazo M–K coinciden en .6.

**Falla.** Quien siga el documento de LEDs para una conexión manual puede tomar masa como entrada de datos o conectar allí la salida del buffer. El checker sigue pasando porque sigue el contrato actualizado y no lee esa frase.

**Corrección propuesta.** Corregir esa referencia al cerrar la documentación de ensamble. Mantener un único pinout vigente y distinguir documentos históricos. El esquema/mazo actual usa el pin correcto; la severidad es documental.

## Fabricación/JLC: límites concretos de esta entrega

- **PCB-M no tiene todavía un diseño fabricable que inspeccionar por pasta/caras/orientación:** `boards/pcb-m/pcb-m.kicad_pcb:1–95` es una placa vacía: leí cero footprints, cero pistas y cero líneas de contorno. `boards/pcb-m/README.md:219–224` declara layout pendiente. PCB-F conserva un layout antiguo que su `README.md:11–14` excluye expresamente del diseño Console. No atribuyo a JLC un rechazo ya ocurrido; no hay paquete final aquí que pueda aprobarse.
- En los 210 componentes del XML de PCB-M, solo **cinco** tienen campo LCSC no vacío: J_PCB_K, dos PCA9517 y dos USBLC6. El constructor usa `fields={}` por defecto (`tools/schgen/pcb_m.py:370–378`) y solo añade esas identidades en `527,642,662`. Los códigos erróneos 2/3 están en la BOM **de referencia**, no aplicados hoy como campos LCSC de esos símbolos. El riesgo aparece al completar el pedido desde esa referencia; no afirmo que un CPL final ya seleccione las piezas erróneas.
- El pedido es explícitamente borrador (`fab/pedido-v1.md:7–9`), con conectores/IDC por buscar (`43,51`) y socket sin stock según la consulta histórica (`71`). No verifiqué inventario completo ni disponibilidad JLC de cada pieza. No convierto una nota histórica en afirmación de stock actual.
- No encontré `?` literales en las BOM CSV entregadas de A/K. **No certifiqué una BOM exportada nueva de M**: la exportación escribe artefactos y quedó fuera del mandato. El test `tools/tests/test_pcb_m_dft.py:139–143` elimina `?` antes de comparar referencias; por ello ese test tampoco prueba que la salida de ensamble preserve referencias exactas. Cerrar anotación y comparar referencias BOM–CPL–PCB sin normalizarlas sigue pendiente.

## Lo que intenté romper y no pude

1. **Orden de las filas Teensy 4.1.** Contrasté `pcb_m.py:393–402` con la [tarjeta PJRC](https://www.pjrc.com/teensy/card11a_rev4_web.pdf) y el mapa independiente de `tools/tests/test_pcb_m_contract.py:36–46`. No encontré desplazamiento de una posición: GND/0…12/3V3/24…32 y VIN/GND/3V3/23…13/GND/41…33 coinciden. Esto no acredita distancia, cara ni giro de sockets: la PCB está vacía.
2. **Cruces escondidos con alias entre I2C local/B_A/B_K, 5V de Teensy/LED y HP_VGND/GND.** Probé alias maliciosos en memoria: los tres se rechazaron. `check_harness.py:90–123` comprueba dominios antes de aceptar alias. Las excepciones IDC son exactas por pin (`harness.py:27–31`), no un comodín NC. No encontré un cruce real de esos dominios en el mazo vigente. Los alias de jumpers no prueban continuidad si se abren; el informe existente ya advierte esa condición.
3. **Hacer fallar el comparador por identidad/pieza incorrecta.** Este intento sí mostró su ceguera: cambié solo en memoria U_LED_BUF a un inversor, una huella SOIC-8 y DNP, dejando las redes iguales, y `compare_netlist()` siguió dando PASS contra el XML original. No alteré el repo. `check_netlist.py:33–98` compara referencias/pines/redes; no valida esos atributos. Es una reproducción del límite que hace relevantes los hallazgos de compra, no un cambio real del diseño.
4. **Cadena y protocolo de LEDs.** Seguí D1→D17→J2.6→J1.6→R1→D18→D26, con D26.DOUT NC (`pcb_k.py:100–109,123–129`). El orden coincide con la tabla de firmware `apps/consola/surface/include/consola_s/KeyLedMap.h:9–35` y el spec 27. La [hoja EA B/2](https://datasheet.lcsc.com/datasheet/pdf/7a14970dbcd694dcd129b083b19b0911.pdf?productCode=C5378731) confirma 1 GND/2 DIN/3 VDD/4 DOUT y GRB de 24 bits. Los tiempos nominales del driver (0,30/0,75 µs, período 1,25 µs, margen de latch) son compatibles con sus intervalos publicados. El modelo de corriente de 20 mA/canal es conservador frente a la versión EA consultada. No demuestro integridad de señal ni corriente real con esto.
5. **Espejo evidente en las huellas propias.** La Choc queda en F con pads de socket explícitos en B; no debe voltearse toda esa huella para colocar el zócalo. El LED está definido en F para luego colocarse en B (`SK6812MINI-EA_ReverseMount.kicad_mod:306–329`; `boards/pcb-k-left/placement.csv:2–4`). No encontré una doble inversión ya aplicada, porque no existe layout nuevo de K. Sus pads tienen pasta y máscara; el recorte pertenece a Dn. No pude certificar el giro/correspondencia geométrica final respecto al encapsulado y ventana real sin inspección física o dibujo plenamente legible; eso permanece abierto, además del socket del hallazgo 8.
6. **DNP MIDI o conexiones accidentales a pin0/1.** En las instancias de esquema, toda la hoja MIDI y sus TP/ajustes están DNP y `in_bom=no`; Teensy0 solo llega a EN y Teensy1 es NC. K R2/R3 y A R1/R2/J4 están marcados DNP. No encontré la unión del viejo MIDI_RX a EN. DNP no elimina cobre, pero la ruta eléctrica actual tampoco los une.
7. **HP_VGND y clamps BAT54S.** HP_VGND permanece separado de GND/AGND en el XML; NT1 une solo GND/AGND. El [pinout BAT54S de Nexperia](https://assets.nexperia.com/documents/data-sheet/BAT54S.pdf) es coherente con el nodo central pin3 y extremos 1/2 del circuito. No encontré el error de polaridad que busqué. Sigue siendo posible realimentar +5V_LED desde un CV exterior: el propio spec 24 lo reconoce en línea 204; no lo presento como descubrimiento nuevo.

**Otros estados sin certificar:** abrir SJ_LED_K corta el 5 V de K pero conserva datos si SJ_LED_DATA sigue cerrado (`pcb_m.py:587–596`); no pude verificar una garantía de entrada DIN tolerante con VDD=0 en la hoja EA. Tampoco certifiqué el comportamiento sin alimentación de los pines UART del SAMD51/Teensy con solo uno de sus USB conectado. Dejar el contacto 3V3 del mazo M4 NC separa fuentes, pero no demuestra aislamiento por señal. Son pruebas pendientes de secuencia y back-powering, no daños observados ni límites eléctricos inventados.

**Límite final:** no hubo hardware, ensayos de arranque, continuidad del módulo real, osciloscopio, DRC del layout final ni validación de BOM/CPL de producción. Las correcciones anteriores son propuestas de revisión; ninguna se aplicó al diseño.
