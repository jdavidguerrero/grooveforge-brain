# Segunda revisión adversarial — GrooveForge Console

**Fecha:** 2026-11-12. **Base:** `/private/tmp/grooveforge-review2`, rama `claude/monorepo-groovepush`, commit completo `095370c19b340b084151e3a9079798f92aa0c23d`. Revisión de los cambios `751d949` y `095370c`, y de sus dependencias eléctricas y de fabricación.

**Dictamen:** hay correcciones reales: desaparecen los dos códigos LCSC incorrectos de la primera revisión, el LDO cambia efectivamente de fuente, se separan EN, el pedal incorpora protección y el shield tiene ahora un mazo por pads identificables. **El conjunto no está cerrado para liberar el pedido.** Persisten cruces por señales al perder alimentación, CV2 no tiene implementado el nuevo estado inicial y falta cerrar la identificación física del conector M4. Encontré además errores nuevos de representación de los TVS y límites reproducibles del nuevo verificador de compras. No encontré una pieza distinta escondida detrás de ninguno de los 62 códigos del catálogo.

No identifico un **CRÍTICO nuevo demostrado**. Distingo a continuación fallos verificables, correcciones incompletas e incertidumbres de montaje: una duda sobre numeración o mecánica no equivale a un corto/interferencia ya demostrado.

## Alcance, fuentes y comprobaciones reproducidas

Leí las dos revisiones originales, REPORT-fixrev/REPORT-lcsc, specs Console 17, 18, 24 —incluidos D24–D37 y diagnóstico—, 27, 28 y 30; generadores, esquemas, XML, huellas locales, mazo, cuatro BOM y pedido manual. Inspeccioné visualmente las tres fotos entregadas. Consulté hojas de fabricante, el core oficial Adafruit y **las 62 identidades LCSC**, con contraste adicional de C7469972/C41376163/C225519/F1 mediante el buscador de componentes JLC/LCSC.

Todas las rutas abreviadas `tools/`, `boards/`, `lib/`, `fab/` y `docs/fotos/` parten de `apps/hardware/kicad/consola/` dentro del checkout indicado. Las líneas corresponden a **095370c**, no a los commits de las revisiones anteriores.

Usé `PYTHONDONTWRITEBYTECODE=1`, comparadores en memoria y los XML ya entregados. **No ejecuté las CLI que regeneran XML/PDF/ERC/mazos, ni pytest, ni DRC, ni firmware, ni grafos.** No hice ensayos físicos. Solo se escribe este informe fuera del repositorio; no hay push.

| Placa | Componentes | Pines | NC | Redes | Resultado contra XML existente |
|---|---:|---:|---:|---:|---|
| PCB-M | 226 | 620 | 23 | 148 | PASS, incluidos metadatos |
| PCB-K izquierda | 78 | 241 | 8 | 58 | PASS |
| PCB-K derecha | 43 | 127 | 5 | 32 | PASS |
| PCB-F | 10 | 32 | 0 | 6 | PASS |
| PCB-P | 12 | 24 | 0 | 12 | PASS |
| PCB-A | 37 | 166 | — | 48 | PASS contra contrato congelado |

`check_cables(netlists, CABLES, ALIASES, PANEL, EXCLUDED)`: **32 cables, cero errores**. `check_components`: cero errores y pendientes en M/K izquierda/K derecha/A. Esto reproduce el cierre declarado de **cuatro placas**, no de seis. Los ERC almacenados dicen cero errores en las seis; M tiene siete avisos. **Son resultados leídos, no ERC nuevos.**

## Hallazgos por severidad

### A1 · ALTO — UART corregida por función; numeración física de CONN1 todavía no suficientemente acreditada

**Estado:** corrección parcial de Adversarial 1; no afirmo una nueva inversión funcional TX/RX.

**Evidencia del diseño.** `tools/schgen/pcb_m.py:390–399,525` y `boards/pcb-m/docs/pcb-m-netlist.xml:9354–9361`: T28/RX7 está unido a J3_M4.3, y T29/TX7 a J3_M4.4. `tools/harness.py:82–84` atribuye al módulo **CONN1.1 GND / .2 3V3 / .3 SDA / .4 SCL**, con cable 1:1. El test `tools/tests/test_fixrev.py:17–24` solo comprueba esa misma atribución textual.

**Fuente primaria.** El [core Adafruit variant.h](https://raw.githubusercontent.com/adafruit/ArduinoCore-samd/master/variants/trellis_m4/variant.h) define Serial1 RX22/TX21 y SDA21/SCL22; la [instancia de Serial1](https://raw.githubusercontent.com/adafruit/ArduinoCore-samd/master/variants/trellis_m4/variant.cpp) usa esos macros. Por tanto **Teensy TX → SCL y Teensy RX ← SDA sí es correcto**. El firmware M4 usa ese Serial1 (`apps/consola/processor/lib/neotrellis_m4/UartInterface/UartInterface.cpp:10–21`), aunque Config.h:55–56 sigue comentando el sentido contrario.

El problema restante es físico: [Adafruit, especificación STEMMA](https://learn.adafruit.com/introducing-adafruit-stemma-qt/technical-specs) enumera para **PH** el orden SCL/verde, SDA/blanco, V+/rojo, GND/negro; su lista para **SH/QT** empieza por GND. La [guía M4](https://learn.adafruit.com/adafruit-neotrellis-m4/board-tour) identifica el puerto como PH/STEMMA/Grove. Esa diferencia exige fijar **vista y numeración del fabricante**, no copiar el orden de QT. La enumeración de una página tampoco basta por sí sola para demostrar los números Eagle de CONN1.

**Escenario de falla.** Armar un 1:1 usando «CONN1.1» como si fuese universal puede invertir el orden completo de cuatro contactos, cruzando señales con alimentación/masa; corregir solo los nombres RX/TX no evita ese error de montaje. No declaro que ese corto esté probado en el módulo de Juan.

**Corrección propuesta.** Cerrar una tabla con foto/dibujo del **lado de acoplamiento**, llave y contacto 1 del PH real, trazada contra el Eagle de Adafruit o continuidad. Mantener como invariantes GND↔GND, 3V3 M4 sin hilo, RX28←SDA/TX21, TX29→SCL/RX22. Si la numeración física es la inversa de la tabla actual, corregir el mapa del cable, sin necesidad de cambiar esas funciones de PCB-M. **No doy la numeración CONN1 por verificada.**

**Límite de esta revisión:** intenté obtener el Eagle oficial y el PDF completo; la descarga no estuvo disponible mediante web, el conector GitHub pidió reautenticación y el host de navegador estaba indisponible. No sustituyo esa evidencia faltante por una afirmación de pinout.

### A2 · ALTO — Los 2,2 kΩ del CV limitan corriente, pero no completan la protección con el MCP6004 apagado

**Estado:** persiste parte de Adversarial 4; la nueva resistencia no equivale a aislamiento.

**Evidencia.** `tools/schgen/pcb_m.py:439–458`: DAC alimentado por +3.3V → R_DAC_LIMIT1/2 de 2,2 kΩ → U_OA_CV1.3/.5; alimentación del operacional en .4 = +5V_CV. XML:8813/8821 confirma que en cada nodo LIMITED solo están resistencia y entrada. No hay clamp superior externo allí. Abrir SJ_CV o quitar USB LED conserva el DAC activo. Doc24:154–164 reconoce la tensión inválida, pero el circuito sigue permitiéndola.

La [hoja Microchip MCP6004, §1 y §4.1.2/figura 4-2](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP6001-1R-1U-2-4-1-MHz-Low-Power-Op-Amp-DS20001733L.pdf) impone límites tanto de corriente como de tensión y muestra diodos externos para limitar la excursión positiva. No permite suponer un clamp interno convencional a VDD+0,3 V.

**Falla.** Sin +5V_CV, la resistencia sola no asegura VIN≤VDD+1 V: una entrada de alta impedancia puede quedar cerca del valor del DAC. `3,3/2,2k=1,50 mA` es una cota de corriente, no una demostración de tensión segura. No hay daño medido.

**Corrección propuesta.** Alimentación común o aislamiento apto sin alimentación; alternativamente, protección de tensión como la recomendada por fabricante, resolviendo también dónde descargar corriente sin levantar el riel apagado. Verificar VIN, VDD y corrientes en todas las secuencias. No aprobarlo solo con el test aritmético `test_fixrev.py:36`.

### A3 · ALTO — Hay otro cruce por señal: M4 encendida conduce directamente a una Teensy apagada

**Estado:** incertidumbre de la revisión 1 ahora contrastada con una prohibición expresa del fabricante; no es un corto de los rieles de 5 V.

**Evidencia.** XML M:9354–9361 solo une cada hilo UART al socket Teensy y J3_M4; no hay resistencia serie, buffer con Ioff ni desconexión. `tools/harness.py:82–84` mantiene USB propio en M4. `apps/docs/consola/24-placa-principal.md:165` deja el ensayo pendiente.

La [hoja NXP i.MX RT1060 Rev.4, §4.2.1.3](https://www.nxp.com/docs/en/nxp/data-sheets/IMXRT1060CEC.pdf) indica que los I/O no deben conducirse externamente cuando su alimentación NVCC está apagada, por riesgo de corrientes inversas y mal funcionamiento. No he extrapolado un límite de inyección permitido que esa sección no da.

**Escenario.** Quitar solo USB Teensy, o una caída de su alimentación, dejando M4 activa y UART en reposo alto. El hilo de recepción sigue siendo conducido; dejar el contacto 3V3 del mazo NC no aísla esa señal. El buffer LED Nexperia sí tiene especificaciones diferentes y no debe usarse como analogía para este GPIO.

**Corrección propuesta.** Aislar UART con componentes que especifiquen operación parcial apagada, o imponer una secuencia eléctrica efectiva que elimine conducción antes de perder NVCC. Una recomendación de no conectar en caliente no cubre por sí sola una desconexión USB posterior o un brownout. Evaluar también el sentido inverso sobre SAMD51 con su hoja exacta; aquí solo doy por contrastado el caso receptor Teensy apagado.

### A4 · ALTO — CV2 de fábrica sigue activa: se aprobó un contrato, no se implementó su cumplimiento

**Estado:** Adversarial 5 incompleto, explícitamente delegado a firmware/provisionado.

**Evidencia.** Doc24 D27:140 y `boards/pcb-m/ASSEMBLY.md:10` exigen cero temprano a 0x60/0x61 y EEPROM cero. Ambos canales siguen poblados (`pcb_m.py:438–453`). En la base revisada `apps/firmware-teensy/src/consola_uno/main.cpp:76–89` solo escribe 0x60; setup:153 en adelante inicia audio/superficie sin poner ambos DAC a cero. `apps/docs/consola/18-teensy-unica-firmware.md:21` todavía llama reservado a 0x61.

La [hoja MCP4725, §5.4/tabla 5-3](https://ww1.microchip.com/downloads/en/devicedoc/22039d.pdf) mantiene el valor inicial de fábrica a media escala y la restauración desde EEPROM al POR.

**Falla.** Con piezas vírgenes, ambos rieles y sin provisionar, CV2 puede permanecer nominalmente en `3,3×0,5×1,5=2,475 V`. Renombrarlo como «función poblada» no cambia ese comportamiento. Resetear MCU tampoco reinicia necesariamente el DAC.

**Corrección propuesta.** Cumplir el provisionado y la inicialización de ambos canales antes de usar las salidas, con lectura de verificación y prueba de POR/reset. Si se exige cero garantizado durante reset o fallo de software, el contrato actual sin mute no lo ofrece y requerirá otra decisión. No modifiqué firmware ni propongo escribir EEPROM en cada arranque.

### A5 · ALTO — Hot-swap permanece sin validar; Supply=owner no certifica el encaje

**Estado:** persiste Adversarial 8 / HW B-01; no es un defecto nuevo introducido por 095370c.

**Evidencia.** `lib/gf_consola.pretty/Kailh_Choc_V2_PG1353_Hotswap.kicad_mod:87–106` conserva Ø1,3 PTH y pads inferiores de 2,6×2,6. `apps/docs/consola/28-pcb-k.md:68–72` reconoce que sustituyen aperturas NPTH de Ø3 y que debe verificarse el plástico. El nuevo catálogo:879–913 declara los conjuntos tecla+zócalo como suministro de Juan; no cambia geometría.

El [fabricante Kailh](https://www.kailhswitch.com/mechanical-keyboard-switches/box-switches/choc-type-hot-swap-socket.html) describe CPG135001S30 para Choc V1. Consulté también [su dibujo/especificación actual](https://www.kailhswitch.com/uploads/15927/files/CPG135001S30.pdf?rnd=101), que registra revisiones del cuerpo/terminal. No obtuve una lectura visual suficientemente fiable de todas las cotas para convertir esta incertidumbre en una interferencia medida ni para certificar compatibilidad V2.

**Escenario.** Los 26 zócalos pueden ser los comprados correctos y no asentarse en la huella dual. DRC, continuidad o el PASS de compras no detectan el volumen plástico. Soldar teclas directamente sería una alternativa al montaje elegido, no su validación.

**Corrección propuesta.** Cupón con el lote real, espesor y chapa previstos; comprobar inserción, apoyo, altura y retención. Si falla, separar las alternativas de huella. No resolverlo únicamente declarando la pieza como externa.

### M1 · MEDIO — Regresión de representación: ambos TVS unidireccionales están dibujados como bidireccionales

**Evidencia.** `tools/schgen/pcb_m.py:550–551,564–565` usa `Device:D_TVS` para PESD3V3U1UL y SMAJ5.0A. Los símbolos **embebidos** en `boards/pcb-m/sheets/09-connectors-rear.kicad_sch:526–645` y `10-power.kicad_sch:357–476` se describen como bidireccionales y denominan sus terminales A1/A2. El XML:9385 incluso llama A1 al terminal que en la pieza del pedal es K.

**La conexión numérica no está invertida:** la [hoja PESD3V3U1UL, tabla 2](https://assets.nexperia.com/documents/data-sheet/PESD3V3U1UL.pdf) asigna 1=K y 2=A; el diseño lleva 1 a PEDAL_BTN y 2 a GND. La [identidad SMAJ5.0A/C83329](https://www.lcsc.com/product-detail/C83329.html) también es unidireccional, y la huella D_SMA usa el pad 1 del cátodo. Es una representación engañosa, no prueba de un TVS ya montado al revés.

**Escenario.** Montaje/inspección manual desde el PDF puede interpretar que no importa la orientación. En el pedal, invertir la pieza real la polariza en directa al reposo; en la alimentación, invertir SMAJ produce una carga directa fuerte sobre el riel y actuación de las protecciones.

**Corrección propuesta.** Símbolo unidireccional con K/A y numeración 1/2 correctas, conservando las redes; indicación de marca de cátodo en los dibujos de montaje. Verificar el primer montaje contra datasheet y huella, no contra A1/A2.

### M2 · MEDIO — El cierre de compras omite PCB-F/PCB-P y no demuestra integridad de una BOM

**Evidencia.** `tools/check_bom.py:13,89–100` solo recorre M/K izquierda/K derecha/A. Aplicando su función a los XML existentes:

- PCB-F: **5 pendientes** — J1 y RV1–RV4.
- PCB-P: **12 pendientes** — FSR1–FSR6 y J1–J6.

Parte de ellos está resuelta narrativamente: `fab/pedido-v1.md:93–94` dice que Juan tiene faders/sensores y deja los sockets FSR para otra tarea. No afirmo que falten todos esos componentes en el inventario. El hallazgo es que **no están cubiertos por el cierre automático** de las seis placas.

Además, `check_components` valida las filas recibidas, pero no su conjunto de referencias. Reproduje en memoria:

```text
filas XML M actuales                         -> ([], [])
mismas filas, eliminando U_ADS1              -> ([], [])
U_ADS1 como TestPoint, sin LCSC/metadatos     -> ([], [])
```

La tercera mutación explota la exención por prefijo de huella en líneas 38–42. Es un límite de este checker aislado: el comparador de netlist detecta cambios de metadatos y no queda invalidado por esa prueba. La primera omisión, aplicada al CSV, sí muestra por qué pasar identidad no demuestra que se compren todas las piezas. `tools/tests/test_lcsc.py:37–45` comprueba XML y CSV por separado; solo M tiene comparación con un export real en líneas 48–57.

**Escenario.** Una BOM manual truncada, una pieza comprable confundida con cobre o una nueva pieza de F/P puede quedar fuera del pedido conservando un PASS de identidad. **No encontré una fila poblada omitida en las cuatro BOM actuales.**

**Corrección propuesta.** Cubrir las seis placas, declarar suministros de F/P y cotejar conjuntos de referencias/atributos entre esquema, XML, BOM y futura CPL, con filtrado explícito de DNP/cobre/THT/manual. Vincular las exenciones al tipo real de símbolo, no solo al texto de Footprint. Mantener separado «identidad» de «integridad del pedido».

### M3 · MEDIO — La nueva polarización de pots no demuestra un cero fiable con cable abierto

**Estado:** límite nuevo del dimensionado D31; no encontré resistencia equivocada ni red de pot cruzada.

**Evidencia.** `pcb_m.py:479–483` y XML:9390–9406 confirman 1 MΩ después de cada serie de 1 kΩ, en paralelo con 100 nF. C26083 sí es **1 MΩ**, no 100 kΩ. El cálculo de carga para pot 10 kΩ del nuevo test (`test_fixrev.py:50–55`) es correcto: error máximo ≈0,217 % FS. El test no incluye fuga del pad ni muestreo ADC.

La [hoja i.MX RT1060, tabla 22 y figura 35](https://www.nxp.com/docs/en/nxp/data-sheets/IMXRT1060CEC.pdf) da IIN sin pull-up/down hasta ±1 µA bajo sus condiciones de ensayo, y representa la fuga del pad en la entrada ADC. **No uso esa cota como fuga típica ni como modelo exacto del ADC en cualquier tensión.** Basta para mostrar que se necesita un presupuesto de fugas: 1 µA sobre 1 MΩ corresponde a una escala de error de 1 V; incluso corrientes menores importan al comparar con cero.

**Escenario.** Con el cursor desconectado, el condensador se descarga con τ nominal de 100 ms, pero el valor final depende también de fugas/contaminación/muestreo. «Tiene pull-down» deja de significar flotante puro; no demuestra lectura cero ni ausencia de CC. Después de una sola τ queda aproximadamente el 37 % de la carga inicial, tampoco cero.

**Corrección propuesta.** Definir umbral y plazo de cable abierto y validarlos con el ADC configurado y temperatura prevista; dimensionar resistencia según ese presupuesto o añadir detección/deshabilitación. Si se baja la resistencia, recalcular/calibrar su carga. El pull-down **LED de 100 kΩ** no tiene el mismo problema de margen: es una entrada digital con VIL especificado; no lo agrupo como fallo demostrado.

### M4 · MEDIO — El nuevo socket FSR tiene taladros coherentes, pero no una envolvente/compra que cierre el montaje

**Estado:** D32 resuelve la clase de terminación; la mecánica sigue incompleta.

**Evidencia.** Foto `docs/fotos/fsr402-juan.png`: dos terminales macho prensados. `passive_boards.py:7,33–37` usa ahora `FSR402_Socket_1x02_P2.54mm`. La huella:52–61 tiene taladros Ø1,0 y pads separados 2,54 mm; `boards/pcb-p/placement.csv:2–7` conserva los seis centros de disco. El conector queda 11 mm delante de cada centro, no donde termina una cola recta de longitud no medida. El catálogo no declara estos sockets, y `fab/pedido-v1.md:94` lo deja explícitamente pendiente.

`apps/docs/consola/28-pcb-k.md:29–31` da solo **1,5 mm** entre PCB-K y PCB-P en la zona superpuesta. No hay modelo/cota de altura del zócalo seleccionado ni una envolvente de cola plegada que permita compararlos.

**Escenario.** Un socket de paso correcto puede sobresalir hacia PCB-K o exigir un pliegue incompatible con el sensor. La foto sin escala no certifica el espesor del pin, altura de carcasa o radio de curvatura. No declaro una colisión ya medida.

**Corrección propuesta.** Elegir el zócalo real, incluirlo como compra o suministro externo y comprobar altura/pin/cola en el apilado. Si se adopta soldadura, definir proceso sobre terminales metálicos, sin calor sobre película. Conservar el datum de disco, pero no tratar el courtyard provisional como el volumen del conjunto.

### B1 · BAJO — Las tres identidades nuevas carecen de URL reproducible en el catálogo

**Evidencia.** `tools/lcsc_catalog.json:822–855` guarda «buscador LCSC» para C41376163, C225519 y C7469972. `check_bom.py:21–24` acepta cualquier source no vacío; no verifica que sea una fuente consultable. En esta revisión sí pude contrastar las tres identidades con las páginas enlazadas en el anexo y el buscador JLC/LCSC.

**Falla.** La próxima revisión no puede reproducir el cierre desde el catálogo sin volver a descubrir la fuente; una cadena arbitraria recibiría el mismo tratamiento. No es una compra incorrecta actual.

**Corrección propuesta.** Guardar URL exacta y, para geometría, dibujo/revisión aplicable. Conservar la declaración de Juan como fuente de inventario externo, separada de una certificación dimensional.

## Corrección de la revisión 1 → verificada / no verificada / incompleta

«Verificada» significa comprobada en el diseño/fuente indicada; no implica ensayo físico. «Incompleta» reconoce cambios reales cuyo objetivo completo aún no se cumple. «No verificada» evita convertir un pendiente de banco o una lectura mecánica insuficiente en PASS.

| Origen / corrección | Estado | Evidencia y alcance |
|---|---|---|
| Adversarial 1 / D24: UART TX–RX | **Incompleta** | Función correcta en XML y core; falta cerrar vista/numeración PH física. A1. |
| Adversarial 2: C7466 → C12495 | **Verificada** | Nexperia 74AHCT1G125GW,125, SOT-353, en símbolo/XML/BOM; no es inversor. |
| Adversarial 3: C15578 → C404027 | **Verificada** | TI TLV75533PDBVR, SOT-23-5; IN1/GND2/EN3/NC4/OUT5 coinciden. |
| Adversarial 4 / D26: +3.3VA desde Teensy | **Incompleta** | IN/EN/C_IN sí cambian a +5V_TEENSY; cruces por CV/UART permanecen. A2/A3. |
| Adversarial 5 / D27: CV2 cero | **Incompleta** | Requisito y checklist correctos; firmware/provisionado no acreditados. A4. |
| Adversarial 6 / D28: pads reales del shield | **Verificada** | Foto reverso, README:188–212, harness:102–110 y XML coinciden; requiere continuidad del montaje. |
| Adversarial 7 / D29: pedal protegido | **Verificada** | 1k serie, 10k pull-up, 10nF, TVS pin1 K→señal/pin2 A→GND; inmunidad física no ensayada. Símbolo engañoso: M1. |
| Adversarial 8: zócalo Choc | **No verificada** | No se cambió la huella ni se aportó resultado de cupón. A5. |
| Adversarial 9 / D30: EN por rama | **Incompleta** | GPIO0→EN_K y GPIO1→EN_A, cada uno con 10k y jumper; firmware aún usa EN común pin0. |
| Adversarial 10 / D31: LED sin flotar | **Verificada** | R_LED_BIAS1 100k a LED_DATA; U_LED_BUF1.2/T13 en la misma red. |
| Adversarial 10 / D31: pots sin flotar | **Incompleta** | 1M después de RC y cálculo de carga correctos; cero/plazo de cable abierto no demostrados. M3. |
| Adversarial 11 / D32: FSR terminado | **Incompleta** | Foto y landing THT corresponden al tipo; altura/cola/socket concreto pendientes. M4. |
| Adversarial 12 / D36: dato LED J1.5→.6 | **Verificada** | Doc27:8 y cadena K; .5 sigue GND. |
| HW M-01: fusible/huella/código | **Verificada** | C208467, MF-MSMF075/24-2; 1812 sin override 1206. Bourns especifica el formato 1812. |
| HW M-02: retirar pull-ups PCB-A | **Verificada** | ASSEMBLY:4, README:168; R1/R2 DNP en XML/BOM A. No certifica retirada en una placa física. |
| HW M-03 / D33: TVS e inversión LED | **Verificada** | D entrada/S carga/G masa y SMAJ detrás de F1; análisis detallado abajo. M1 afecta representación. |
| HW M-04 / D34: PCA9517A huella | **Verificada** | NXP SOT505-1, cuerpo 3×3/paso0,65, KiCad MSOP-8 con pads adecuados nominalmente. |
| HW M-05: arranque códec con LEDs | **No verificada** | No hay nuevo ensayo de PCB-M con 26 LEDs desde power-on. |
| HW B-01: cupón zócalo | **No verificada** | A5. |
| HW B-02: cupón LED inverso | **No verificada** | Identidad/pinout nominal conservados; no prueba luz/cara/giro del layout final. |
| HW B-03: supuesto error BAT54S | **Verificada: no había ese error** | 1=A1/2=K2/3=unión; conservar BAT54S es correcto. No sustituir por BAT54C. |
| HW B-04: ADS1115 VSSOP/TSSOP | **Verificada** | DGS 3×3/paso0,5 y huella KiCad TSSOP-10_3x3 coinciden nominalmente; no confundir alias con paquete distinto. |
| HW B-05 / D35: corte VUSB–VIN | **Verificada: recomendación rechazada** | NO cortar es coherente: el USB Teensy origina +5V_TEENSY; no hay segunda fuente directa allí. |
| HW B-06: umbral FSR con 10k | **No verificada** | Resistencias presentes; umbral/ensayo musical no se validan con XML. |
| HW B-07: PARTS con huellas vacías | **Verificada en exportación** | Componentes comprables M tienen huella final; PARTS no es la BOM. No se ha completado fabricación/CPL. |
| D37: referencias sin «?» y metadatos | **Verificada** | 226 referencias M únicas; mapa Function→Reference; mutación de valor ahora rechazada. |
| 095370c: sockets Teensy y header 2×4 | **Verificada en identidad y geometría nominal** | C41376163 y C225519 sí son esos conectores; dibujo dimensional completo/montaje apilado no certificado. |
| 095370c: 47µF 5×5,4 | **Verificada en identidad/polaridad nominal** | C7469972 47µF/16V/5×5,4; pad1 positivo. Ver límite mecánico abajo. |
| 095370c: suministros externos | **Verificada como declaración** | M1/K17/K9/A9, clases local/owner correctas; no prueba ajuste ni recepción física. M2/A5. |

## Comprobaciones focales que sí cierran parte del diseño

### Alimentación y dominios

| Dominio | Cargas/conexiones comprobadas | Resultado |
|---|---|---|
| +5V_TEENSY | Socket VIN; U_LDO1 IN1/EN3; C_LDO_IN1 | Cambio real desde LED, no solo etiqueta. No cortar VUSB–VIN. |
| +3.3V Teensy | FSR/pots/expresión, PCB-A/F y K, DAC, pull-ups I2C, PCA/EN/pedal | Independiente del riel analógico regulado aunque comparten origen USB. |
| +3.3VA | U_ADS1/2 VDD8, ADDR ADS2, desacoplos, SJ_FRONT | No quedó alimentada desde +5V_LED. |
| +3.3VA_FRONT | MCP6004 de entrada, VREF, clamps de audio, C_OA_IN | Tras SJ_FRONT; retorno AGND. |
| +5V_LED | Buffer, ramas LED, Sharp y CV por sus puentes; clamps de salida CV | Conserva cruces de señales; no hay corto directo con +5V_TEENSY. |
| +5V_CV | Tras SJ_CV y 10Ω; U_OA_CV1.4, 47µF y desacoplos | Señales DAC no se desconectan con SJ_CV. A2. |
| HP_VGND | Solo retorno del módulo HP y J_HP | Sin unión lógica a GND/AGND. Aislamiento del jack/chasis pendiente de montaje. |

**Otros cruces, sin exagerar su protección.** Sharp tiene 10k serie real (`pcb_m.py:529–534`); la [hoja ADS1115 §9.1.3](https://www.ti.com/lit/ds/symlink/ads1115.pdf) describe diodos a rieles y limitación de corriente. Eso respalda la reducción de inyección, no la ausencia de riel fantasma. Faders/expresión siguen excitados desde +3.3V y los ADS desde otro LDO: un origen USB común elimina el caso estable «USB LED quitado deja ADS apagado», pero no prueba rampas idénticas ni comportamiento ante fallo local de +3.3VA. En el extremo del fader sigue sin haber resistencia dedicada a limitar corriente hacia AIN. No declaro una violación de corriente medida.

Abrir solo SJ_LED_K deja DIN conectado a LEDs apagados. Los clamps BAT54S de CV todavía pueden alimentar +5V_LED desde una fuente exterior; el PMOS nuevo tampoco debe suponerse un bloqueador de corriente inversa con el riel de carga ya energizado. Son límites anteriores conservados, no errores nuevos descubiertos de polaridad. El procedimiento de servicio debe desconectar señales/jacks además de cortar alimentación cuando corresponda.

### Audio Shield y retornos reales

En la foto **reverso**, fila de pads inferior de izquierda a derecha: **OUT L / G / OUT R / IN R / IN L**. El mazo actual usa IN L→J_SHIELD_IN1.1, IN R→.2 y G superior sobre IN L→.3. Para OUT: L→J_SHIELD_OUT1.1, R→.2 y G inferior entre L/R→.3. Los dos pads superiores sin rótulo permanecen sin cable; no les atribuyo una función. Abajo a la derecha: L/R/VGND→J_SHIELD_HP1.1/.2/.3.

El circuito y `harness.py:102–110` coinciden con esa lectura. El G del módulo va a GND, no crea una unión extra con AGND; NT1 conserva esa unión en el diseño. VGND permanece separado. **Esta corrección es material y verificable en las fotos**; no equivale a validar continuidad ni aislamiento del jack metálico ya montado.

### Pedal, buffer LED y EN

- Pedal: J_PEDAL1.1→PEDAL_RAW→R_PEDAL_SER1 1k→PEDAL_BTN; pull-up10k, C10nF y TVS al mismo nodo antes de K. El pin1 del PESD es K y está bien conectado. Con contacto cerrado ideal, el divisor externo deja ≈0,30V; hay que sumar la pull-up interna del MCP al comprobar el margen real. No encontré un TVS en directa en el netlist vigente.
- Buffer: 74AHCT1G125 correcto, OE1=GND, A2=LED_DATA, GND3, Y4=LED_BUF_OUT, VCC5=+5V_LED; C_LED_BUF1 100nF presente. La [hoja Nexperia](https://assets.nexperia.com/documents/data-sheet/74AHC_AHCT1G125.pdf) fija VIL=0,8V para AHCT y fuga de entrada hasta 2µA en su rango extendido. Una estimación de 2µA×100k=0,2V deja margen nominal de reposo; no sustituye medir el pin13 durante bootloader/DMA. La hoja también especifica fuga con VCC=0: no invento aquí un diodo a VCC en A.
- EN: XML:8901/8906 y 9055/9059 confirma dos redes y dos jumpers independientes, cada pull-up10k a +3.3V. Hay cuatro desacoplos de los PCA y dos de los USBLC6; no encontré un IC nuevo sin desacoplo. La [hoja NXP §6.1](https://www.nxp.com/docs/en/data-sheet/PCA9517A.pdf) exige estado ocioso en ambos segmentos para cambiar EN. **Firmware vigente sigue declarando EN común=pin0** (`ConsolaMap.h:16–17`, `Surface.cpp:346–358`); EN_A no recibe esa recuperación. Es seguimiento pendiente, no una prestación ya recuperada por el esquema.

### Entrada LED: AO3401A, TVS y caída

`pcb_m.py:560–579`, XML:8666–8683 y ASSEMBLY:8 son consistentes: **J entrada→F1→D3 MOS; S2→carga; G1→GND**. El [AO3401A de AOS](https://www.aosmd.com/sites/default/files/res/datasheets/AO3401A.pdf) es P-channel, con VGS absoluto ±12V. A +5V, VGS≈−5V está dentro del límite. El diodo de cuerpo permite cargar inicialmente S con entrada positiva y se opone a una entrada negativa con la carga descargada; el canal reduce luego la caída.

A 25°C y usando el máximo de 60mΩ especificado a −4,5V: **0,5A×60mΩ=30mV y 15mW**. La cifra del informe anterior, basada conservadoramente en 85mΩ a −2,5V, es 42,5mV/21,25mW; no encontré un error de factor o una orientación S/D invertida. Estas cifras son del MOS, **no de fusible+cable+conectores**, y no son una cota universal a cualquier temperatura.

F1 sí es [Bourns MF-MSMF075/24, serie 1812](https://www.bourns.com/docs/product-datasheets/mf-msmf.pdf), C208467, hold0,75A/trip1,5A. El [SMAJ5.0A seleccionado](https://www.lcsc.com/product-detail/C83329.html) tiene standoff5V y clamp9,2V al pulso especificado: su polaridad numérica es correcta y está después de F1. **No mantiene los IC por debajo de 5,5V ante todo transitorio ni reemplaza protección de sobretensión sostenida.** El propio D33 lo advierte; no presento ese límite como una nueva promesa incumplida. Falta validar la coordinación pulso/fusible/MOS con layout y mazo reales.

### Encapsulados modificados y suministros externos

- **PCA9517ADP:** el dibujo NXP SOT505-1 corresponde al MSOP/TSSOP8 comercial de cuerpo3×3 y paso0,65. La huella instalada `MSOP-8_3x3mm_P0.65mm` tiene centros x±2,1125 y pads1,625×0,4mm. Es una selección coherente; no solo renombrado de valor. ADS DGS usa paso0,5 y pads1,45×0,3mm en x±2,15: no debe confundirse con ese PCA.
- **47µF:** `C_CV_FILTER1`, `pcb_m.py:466–468`, XML y CSV coinciden en C7469972/FVH016ADA470M0554/47µF16V y `CP_Elec_5x5.4`. La [ficha LCSC](https://www.lcsc.com/pl/product-detail/C7469972.html) confirma diámetro5 y altura5,4. En la biblioteca instalada, pads1/2 están en x−2,2/+2,2, tamaño3×1,6mm; el símbolo lleva **positivo1 a +5V_CV y negativo2 a GND**. No quedó 5×5,8 en esa instancia. La obtención del dibujo completo FOSAN falló: verifico identidad, dimensiones nominales y polaridad de la huella, **no certifico todas las cotas de terminales ni la rotación CPL futura**.
- **C41376163:** LCSC/JLC confirma hembra1×24, paso2,54, aislamiento8,5mm y cola3mm. **C225519:** confirma macho2×4, paso y separación de filas2,54, cola3mm y pin de acoplamiento6mm. Las huellas elegidas son coherentes nominalmente; montaje/altura final de Teensy+shield sigue pendiente.
- **Supply:** los IDC son `local` en M y A; los SW son `owner` —incluyendo explícitamente tecla+zócalo— y los ocho EC11 de A también `owner`. No encontré un SMD de M convertido erróneamente a suministro externo. El CSV manual no duplica el 47µF SMD ni el SH4 de M, y sí incluye sus THT. No se acredita ajuste dimensional de un EC11 solo por llamarlo «EC11»; el reporte conserva esa limitación mecánica.

## Fabricación: qué sigue impidiendo certificar el paquete de seis placas

Esto no es una auditoría de Gerbers finales. `boards/pcb-m/pcb-m.kicad_pcb` sigue vacío: cero footprints, pistas y contorno de placa; `boards/pcb-m/README.md:223–228` declara layout pendiente. K izquierda/derecha tienen esquema/placement, sin layout final correspondiente. Los artefactos antiguos de `boards/fab/pcb-k1`, `pcb-k2`, etc. son GroovePush; no deben sustituir las placas Console corregidas. PCB-F advierte expresamente que su layout antiguo no corresponde al esquema Console.

Por ello no pude verificar separación/retornos físicos, agujeros/altura de sockets, recorte LED, disipación, desacoplos colocados junto a pines, pasta, rotaciones ni coincidencia BOM–CPL–cobre. La ausencia de layout es **un pendiente conocido, no una regresión causada por 095370c**. Para este pedido deben cerrarse contra el paquete que efectivamente se vaya a fabricar, especialmente el capacitor nuevo y el pin1 de MOS/TVS/headers.

Los rings LINE IN siguen sin terminar (`pcb_m.py:541,556`); el firmware y documentación deben describir la entrada como realmente construida. No invento recepción balanceada a partir de un jack TRS. HP requiere jack/sleeve aislado del chasis; el BOM de pared aún no fija un modelo que lo garantice. Son pendientes externos al PASS eléctrico.

## Anexo: auditoría de las 62 identidades LCSC

**V** = fabricante/MPN y encapsulado nominal contrastados en web con la ficha del distribuidor; en pasivos también el valor. Comparé las huellas declaradas y las asignadas a los componentes. No significa stock reservado, garantía de montaje JLC ni certificación de todas las cotas de cada drawing. Las equivalencias dimensionales y los límites mecánicos más delicados están descritos arriba. No encontré un código que identificase otro componente.

| Código / evidencia consultada | Fabricante · MPN confirmado | Encapsulado del catálogo | Estado |
|---|---|---|---|
| [C25744](https://www.lcsc.com/product-detail/C25744.html) | UNI-ROYAL · 0402WGF1002TCE | 0402 | V |
| [C25879](https://www.lcsc.com/product-detail/C25879.html) | UNI-ROYAL · 0402WGF2201TCE | 0402 | V |
| [C26083](https://www.lcsc.com/product-detail/C26083.html) | UNI-ROYAL · 0402WGF1004TCE | 0402 | V |
| [C11702](https://www.lcsc.com/product-detail/C11702.html) | UNI-ROYAL · 0402WGF1001TCE | 0402 | V |
| [C25765](https://www.lcsc.com/product-detail/C25765.html) | UNI-ROYAL · 0402WGF2002TCE | 0402 | V |
| [C25741](https://www.lcsc.com/product-detail/C25741.html) | UNI-ROYAL · 0402WGF1003TCE | 0402 | V |
| [C25804](https://www.lcsc.com/product-detail/C25804.html) | UNI-ROYAL · 0603WAF1002T5E | 0603 | V |
| [C21190](https://www.lcsc.com/product-detail/C21190.html) | UNI-ROYAL · 0603WAF1001T5E | 0603 | V |
| [C14675](https://www.lcsc.com/product-detail/C14675.html) | YAGEO · RC0603FR-07100KL | 0603 | V |
| [C22790](https://www.lcsc.com/product-detail/C22790.html) | UNI-ROYAL · 0603WAF1202T5E | 0603 | V |
| [C4216](https://www.lcsc.com/product-detail/C4216.html) | UNI-ROYAL · 0603WAF3302T5E | 0603 | V |
| [C25819](https://www.lcsc.com/product-detail/C25819.html) | UNI-ROYAL · 0603WAF4702T5E | 0603 | V |
| [C23186](https://www.lcsc.com/product-detail/C23186.html) | UNI-ROYAL · 0603WAF5101T5E | 0603 | V |
| [C22843](https://www.lcsc.com/product-detail/C22843.html) | UNI-ROYAL · 0603WAF1501T5E | 0603 | V |
| [C22978](https://www.lcsc.com/product-detail/C22978.html) | UNI-ROYAL · 0603WAF3301T5E | 0603 | V |
| [C22859](https://www.lcsc.com/product-detail/C22859.html) | UNI-ROYAL · 0603WAF100JT5E | 0603 | V |
| [C23162](https://www.lcsc.com/product-detail/C23162.html) | UNI-ROYAL · 0603WAF4701T5E | 0603 | V |
| [C4190](https://www.lcsc.com/product-detail/C4190.html) | UNI-ROYAL · 0603WAF2201T5E | 0603 | V |
| [C17414](https://www.lcsc.com/product-detail/C17414.html) | UNI-ROYAL · 0805W8F1002T5E | 0805 | V |
| [C17714](https://www.lcsc.com/product-detail/C17714.html) | UNI-ROYAL · 0805W8F470JT5E | 0805 | V |
| [C17477](https://www.lcsc.com/product-detail/C17477.html) | UNI-ROYAL · 0805W8F0000T5E | 0805 | V |
| [C17520](https://www.lcsc.com/product-detail/C17520.html) | UNI-ROYAL · 0805W8F2201T5E | 0805 | V |
| [C17634](https://www.lcsc.com/product-detail/C17634.html) | UNI-ROYAL · 0805W8F330JT5E | 0805 | V |
| [C17713](https://www.lcsc.com/product-detail/C17713.html) | UNI-ROYAL · 0805W8F4702T5E | 0805 | V |
| [C17673](https://www.lcsc.com/product-detail/C17673.html) | UNI-ROYAL · 0805W8F4701T5E | 0805 | V |
| [C1525](https://www.lcsc.com/product-detail/C1525.html) | Samsung Electro-Mechanics · CL05B104KO5NNNC | 0402 | V |
| [C52923](https://www.lcsc.com/product-detail/C52923.html) | Samsung Electro-Mechanics · CL05A105KA5NQNC | 0402 | V |
| [C15850](https://www.lcsc.com/product-detail/C15850.html) | Samsung Electro-Mechanics · CL21A106KAYNNNE | 0805 | V |
| [C28323](https://www.lcsc.com/product-detail/C28323.html) | Samsung Electro-Mechanics · CL21B105KBFNNNE | 0805 | V |
| [C1779](https://www.lcsc.com/product-detail/C1779.html) | Samsung Electro-Mechanics · CL21A475KAQNNNE | 0805 | V |
| [C1710](https://www.lcsc.com/product-detail/C1710.html) | Samsung Electro-Mechanics · CL21B103KBANNNC | 0805 | V |
| [C14663](https://www.lcsc.com/product-detail/C14663.html) | YAGEO · CC0603KRX7R9BB104 | 0603 | V |
| [C49678](https://www.lcsc.com/product-detail/C49678.html) | YAGEO · CC0805KRX7R9BB104 | 0805 | V |
| [C5137478](https://www.lcsc.com/product-detail/C5137478.html) | FOJAN · FCC0805B105K500FT | 0805 | V |
| [C12891](https://www.lcsc.com/product-detail/C12891.html) | Samsung Electro-Mechanics · CL31A226KAHNNNE | 1206 | V |
| [C57112](https://www.lcsc.com/product-detail/C57112.html) | FH · 0603B103K500NT | 0603 | V |
| [C12495](https://www.lcsc.com/product-detail/C12495.html) | Nexperia · 74AHCT1G125GW,125 | SOT-353 | V |
| [C404027](https://www.lcsc.com/product-detail/C404027.html) | TI · TLV75533PDBVR | SOT-23-5 | V |
| [C80799](https://www.lcsc.com/product-detail/C80799.html) | NXP · PCA9517ADP,118 | MSOP-8 | V |
| [C37593](https://www.lcsc.com/product-detail/C37593.html) | TI · ADS1115IDGSR | VSSOP-10 | V |
| [C144198](https://www.lcsc.com/product-detail/C144198.html) | MICROCHIP · MCP4725A0T-E/CH | SOT-23-6 | V |
| [C1346056](https://www.lcsc.com/product-detail/C1346056.html) | MICROCHIP · MCP6004-I/SL | SOIC-14 | V |
| [C15127](https://www.lcsc.com/product-detail/C15127.html) | AOS · AO3401A | SOT-23 | V |
| [C83329](https://www.lcsc.com/product-detail/C83329.html) | Littelfuse · SMAJ5.0A | DO-214AC(SMA) | V |
| [C552530](https://www.lcsc.com/product-detail/C552530.html) | Nexperia · PESD3V3U1UL,315 | DFN1006-2(SOD-882) | V |
| [C208467](https://www.lcsc.com/product-detail/C208467.html) | BOURNS · MF-MSMF075/24-2 | 1812 | V |
| [C7519](https://www.lcsc.com/product-detail/C7519.html) | ST · USBLC6-2SC6 | SOT-23-6L | V |
| [C47546](https://www.lcsc.com/product-detail/C47546.html) | Nexperia · BAT54S,215 | SOT-23 | V |
| [C2286](https://www.lcsc.com/product-detail/C2286.html) | Hubei KENTO Elec · KT-0603R | 0603 | V |
| [C506653](https://www.lcsc.com/product-detail/C506653.html) | MICROCHIP · MCP23017-E/SS | SSOP-28(5.3x10.2x0.65) | V |
| [C5378731](https://www.lcsc.com/product-detail/C5378731.html) | OPSCO Optoelectronics · SK6812MINI-EA | SMD-4P,3.2x2.8mm | V |
| [C131337](https://www.lcsc.com/product-detail/C131337.html) | JST · B2B-PH-K-S(LF)(SN) | Through Hole,P=2mm | V |
| [C131339](https://www.lcsc.com/product-detail/C131339.html) | JST · B3B-PH-K-S(LF)(SN) | Through Hole,P=2mm | V |
| [C131334](https://www.lcsc.com/product-detail/C131334.html) | JST · B4B-PH-K-S(LF)(SN) | Through Hole,P=2mm | V |
| [C131342](https://www.lcsc.com/product-detail/C131342.html) | JST · B6B-PH-K-S(LF)(SN) | Through Hole,P=2mm | V |
| [C158038](https://www.lcsc.com/product-detail/C158038.html) | JST · B10B-PH-K-S(LF)(SN) | Through Hole,P=2mm | V |
| [C160404](https://www.lcsc.com/product-detail/C160404.html) | JST · SM04B-SRSS-TB(LF)(SN) | SMD,P=1mm,Surface Mount,Right Angle | V |
| [C173752](https://www.lcsc.com/product-detail/C173752.html) | JST · S2B-PH-K-S(LF)(SN) | Through Hole,Right Angle,P=2mm | V |
| [C160405](https://www.lcsc.com/product-detail/C160405.html) | JST · SM06B-SRSS-TB(LF)(SN) | SMD,P=1mm,Surface Mount,Right Angle | V |
| [C41376163](https://www.lcsc.com/product-detail/C41376163.html) | Hong Cheng · HC-PM254-8.5H-1x24PZ | THT 1x24 P2.54mm H8.5mm | V |
| [C225519](https://www.lcsc.com/product-detail/C225519.html) | CJT · A2541WV-2x4P | THT 2x4 P2.54mm vertical | V |
| [C7469972](https://www.lcsc.com/pl/product-detail/C7469972.html) | FOSAN · FVH016ADA470M0554 | SMD 5x5.4mm 47uF 16V +/-20% | V |

Los alias comerciales de paquete no se equiparan solo por nombre: PCA MSOP8/SOT505-1 y ADS VSSOP10/DGS se contrastaron con dibujos y geometría KiCad. Los tres códigos finales se corroboraron también con la búsqueda oficial JLC/LCSC, que devuelve los mismos fabricantes/MPN y dimensiones nominales; no usé sus cantidades de stock como aceptación de compra. Las páginas inglesas de C7469972 fallaron en la consulta inicial; la ficha polaca del mismo distribuidor y la búsqueda JLC sí resolvieron su identidad.

## Lo que intenté romper y no pude

1. **Renombrado que esconda TX contra TX por función.** El XML enlaza RX28 con J3.3 y TX29 con J3.4; el core oficial confirma SDA=TX y SCL=RX. No encontré el viejo cruce funcional. La numeración física externa queda separadamente abierta en A1.
2. **Reintroducir inversor/LDO equivocado mediante LCSC.** Los 62 códigos consultados concuerdan con fabricante/MPN/paquete; C12495 es AHCT125 y C404027 TLV755 SOT23-5. No encontré C7466 ni C15578 aplicados como compras vigentes.
3. **Borrar la nueva polarización con un cambio de nombre.** En XML, LED_DATA incluye U_LED_BUF1.2, T13 y R_LED_BIAS1; cada POT incluye ADC, resistor1M y C100nF después del serie. C25741=100k y C26083=1M en LCSC. El cálculo ideal de carga del pot también coincide; no forcé un falso error de valor para obtener un hallazgo.
4. **Ocultar un puente entre rieles, HP o segmentos I2C mediante alias.** No encontré unión directa +5V_TEENSY/+5V_LED, ni HP_VGND a GND/AGND, ni unión B_A/B_K en las redes exportadas. El mazo pasa 32 cables con los dominios explícitos. Esto no elimina los cruces por señales descritos en A2/A3.
5. **Hacer pasar valor incorrecto solo porque las redes coinciden.** Muté U_LED_BUF1.value en el modelo en memoria y `compare_netlist()` respondió `Component metadata mismatch: U_LED_BUF1`. La ceguera de metadatos de la primera revisión sí fue corregida.
6. **Encontrar referencias M duplicadas o «?» y compras externas mal clasificadas.** 226 referencias únicas, sufijos numéricos y mapa Function conservado. Los conjuntos de referencias de las cuatro BOM coinciden con los XML incluyendo las filas DNP que KiCad exporta; las diferencias al filtrar poblados son precisamente DNP identificados. No hallé un IC poblado ausente de los CSV actuales.
7. **Invertir EN en la nueva separación.** R_EN_K1 y su puente llegan a GPIO0; R_EN_A1 y el suyo a GPIO1. Son redes distintas con pull-ups propios, no el EN común rebautizado. No los confundí con la recuperación incompleta del firmware.
8. **Encontrar el pedal en directa o un PMOS invertido.** PESD pin1 K→señal/pin2 A→GND es correcto. AO3401A G1 masa/S2 carga/D3 fusible es la topología adecuada para inversión con carga inicialmente descargada. Encontré el símbolo TVS engañoso, no una inversión de los números de pad.
9. **Conectores de shield inventados como header directo.** Ahora están explícitamente declarados como tres mazos soldados; orden y retornos coinciden con las fotos. VGND sigue separado y los pads sin identificación se dejan sin cable.
10. **Perder el desacoplo con componentes nuevos.** LED buffer, LDO, ambos ADS/DAC y ambas ramas PCA/USBLC6 conservan capacitores en sus rieles. No encontré un IC nuevo poblado sin desacoplo en esquema; su colocación física no existe todavía para auditarla.
11. **Romper cadena/GPIO al anotar referencias.** Se conserva el recorrido D1…D17→cable→D18…D26, final DOUT NC, dato por J1.6 y mapa MCP sin GPA7/GPB7 como entradas. Las referencias renombradas no rompieron la conectividad de los XML comparados.
12. **Regresión de polaridad o valor por 5×5,8→5×5,4.** La instancia y BOM usan ya C7469972/47µF16V, positivo1 al riel y negativo2 a masa. No encontré el capacitor antiguo oculto en esa instancia; queda pendiente la geometría completa del fabricante y el montaje final, no una polaridad invertida demostrada.
