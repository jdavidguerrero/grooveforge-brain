# Auditoría previa a fabricación — Studio

Fecha: 2026-10-22. Alcance: los tres paquetes de `apps/hardware/kicad/studio/fab`, sus esquemas, PCB, BOM/CPL, conexiones entre placas y montaje documentado. KiCad 10.0.6.

> **Actualización posterior a los cambios F1–F4:** ver [auditoría delta de EMC, DFM y fiabilidad](auditoria-delta-2026-10-22.md). F1 está corregido en diseño/firmware; F2 y F4 están mejor especificados. F3 conserva una discrepancia de presupuesto de corriente. La ampliación identifica vías en pads C2/U1, discontinuidades de referencia I2S, desacoplo C7, proximidad mecánica de C4 y protecciones de puertos pendientes. **El dictamen de liberación vigente es el de la auditoría delta.** El texto inferior se conserva como registro de la primera revisión y no describe todos los archivos posteriores a ella.

## Dictamen

**NO liberar todavía el pedido conjunto de portadora + PCB-A.** Hay un problema funcional confirmado en la elección del pin del encoder derecho y quedan por cerrar la alimentación USB-C y las piezas de montaje. No encontré un cortocircuito, una pista sin conectar ni un desajuste entre los Gerbers y el diseño actual.

El trabajo existente es aprovechable. Los resultados no justifican empezar las placas desde cero; sí justifican resolver los puntos siguientes antes de pagar el envío.

| Placa | Resultado de esta revisión | Decisión de pedido |
|---|---|---|
| Portadora, 97 × 100 mm | Ruteo/paridad correctos; comparte el problema de ENC R y las condiciones de alimentación pendientes | Retener hasta cerrar F1–F3 |
| PCB-A, 168 × 88 mm | Ruteo/paridad y posiciones del panel correctos; pots aún sin modelo comprado | Retener hasta cerrar F1 y F4 |
| PCB-K, 156 × 62 mm | Ruteo/paridad y CAM correctos; riesgos de montaje específicos no cerrados | **Excluir del primer pedido**, conforme a ADR-004 rev. 3 |

Las dimensiones son las del contorno, sin sumar el grosor de la línea gráfica de Edge.Cuts. PCB-A y PCB-K superan 100 mm en un eje; no asumir la misma tarifa que la portadora.

El usuario confirmó durante esta revisión: tiene una fuente USB-C de 5 V, posiblemente 3–4 A, cuya etiqueta aún no se ha verificado; no tiene el interruptor PAD ni los dos pots. Quiere una única fuente para todo el instrumento y admite cambiarla si hace falta. **No considero confirmados los 4 A ni el perfil exacto de esa fuente.**

## Método y límites

Se revisó `01-architecture.md`, ADR-002/003/004 y los theory docs Studio 02/03. Se exportaron netlists nuevos, se ejecutó ERC y DRC con paridad y relleno nuevo de zonas, se regeneró CAM fuera de `fab`, se compararon BOM/CPL, se consultaron los 19 códigos LCSC de la portadora y se repitió la simulación existente. También se inspeccionaron renders nuevos de las tres PCB y se cruzaron centros de controles y fijaciones de PCB-A con `panel-aluminio.dxf`.

**Por qué hacen falta varias verificaciones:** DRC encuentra problemas geométricos entre redes, pero no sabe que un pin de un módulo tiene un LED, qué interruptor comprarás ni cuánto cae la alimentación en el cable. ERC tampoco conoce el circuito interno de la Teensy: está representada por conectores genéricos.

No se modificaron esquemas, PCB, firmware, specs ni archivos de pedido. No se realizó una medición física, validación térmica, ensayo EMC/ESD, prueba de sonido ni una nueva comprobación integral de colisiones en FreeCAD. No se abrió ni se aprobó un pedido en JLCPCB. Por eso este informe no promete funcionamiento al 100 % ni valida la orientación final que aplique el ensamblador.

Evidencias de esta ejecución: [directorio de resultados](auditoria-2026-10-22/). Los hashes identifican exactamente los archivos auditados; cualquier corrección exige repetir las comprobaciones afectadas y regenerar `fab`.

## Hallazgos que deben cerrarse

### F1 — Alta: ENC R-A usa el pin 13 con una carga que impide confiar en el pull-up interno

**Confirmado en el diseño y en la documentación del fabricante.**

Ruta real: `ENC2.A` en PCB-A → `J2.18` → cable IDC → portadora `J5.18` → `J2.14` → **Teensy pin 13**. En PCB-A, esa red solo tiene ENC2.A, C4 de 10 nF y J2.18: no existe pull-up externo. El firmware declara `Encoder _enc_r{13, 14}` y la biblioteca Encoder instalada configura ambos pines como `INPUT_PULLUP`.

La Teensy 4.1 tiene su LED D3 y una resistencia R1 de 470 Ω conectados al pin 13. PJRC advierte expresamente que no se use `INPUT_PULLUP` en ese pin. La carga puede mantener la fase A por debajo del nivel HIGH válido: cuentas perdidas, dirección incorrecta o encoder inmóvil. El condensador de antirrebote no elimina una carga de corriente continua. [PJRC: pin 13](https://www.pjrc.com/store/teensy41.html), [esquema oficial](https://www.pjrc.com/teensy/schematic41.png).

**Cierre recomendado:** escoger una solución y probar el encoder real antes del pedido. La opción más limpia es reasignar esa fase a un GPIO libre, verificando todas sus funciones y actualizando mapa, portadora y firmware. Otra opción conserva el pin y modifica su carga/pull-up, pero exige dimensionamiento y comprobación de niveles; añadir un pull-up genérico de 10 kΩ no queda validado por este informe.

El pin 13 está fijado en `01-architecture.md` §3.3. Cambiarlo es una decisión de contrato técnico: se señala la contradicción y **no se reasigna silenciosamente** en esta auditoría. La prueba de cierre debe revisar ambas fases A/B en reposo y al girar, no solo los botones push del sketch 46.

### F2 — Alta, condicionada al montaje: la entrada USB-C no está definida eléctricamente de punta a punta

La PCB no recibe un USB-C: recibe **J10.1 = VBUS_IN y J10.2 = GND**. No hay CC1/CC2 ni resistencias Rd en la portadora. `compras-locales.md` pide un prolongador USB-C de panel para 5 V, sin especificar cómo implementa la detección de consumidor.

Esto **no demuestra que tu fuente falle**, pero sí que no hay un conjunto definido que permita aprobarla. Con una fuente USB-C y un cable C–C, sacar únicamente VBUS/GND de un conector pasivo puede dejar la fuente sin habilitar su salida. Un receptáculo consumidor sencillo requiere terminaciones Rd independientes en CC1 y CC2; además hay que respetar la corriente que anuncie la fuente. [Guía de USB-C de Texas Instruments](https://www.ti.com/lit/eb/slyy228/slyy228.pdf).

**Cierre:** identificar fuente y cable y definir el módulo/conector de panel completo, con detección CC correcta y salida fija de 5 V a J10. Verificar encendido con ambas orientaciones del cable. No comprar un disparador PD de 9/12 V para esta placa. Más de 3 A por USB-C exige un conjunto fuente/controlador/cable apropiado; no se obtiene suponiendo que dos resistencias permiten cualquier corriente.

La TVS D1 `SMBJ5.0CA` es protección de transitorios; su código “5.0” no significa que limite el riel a 5.0 V. El componente de la BOM tiene tensión de clamp especificada de 9.2 V bajo su corriente de ensayo. No sustituye una alimentación regulada ni protege por sí solo contra alimentar sostenidamente la placa con 9 V. [Componente D1](https://www.lcsc.com/product-detail/C113975.html).

### F3 — Alta, pendiente de carga real: una fuente mayor no elimina los límites del cableado común

Hay dos contradicciones que cerrar:

- Theory/02 §10D pide contactos de 3 A para la Pi y contempla unos 2.5 A; `compras-locales.md` admite un IDC genérico de 1 A por contacto, justificándolo por consumo típico. Los dos contactos positivos de 1 A suman nominalmente 2 A, por debajo de ese escenario. No es una base suficiente para garantizar arranque y picos.
- J10 es JST-XH y todo el consumo pasa por él y F1. El XH original está especificado para **3 A con AWG22**; una pieza genérica sin ficha no hereda automáticamente esa capacidad. F1 `mSMD300` tiene hold de 3 A y trip de 5 A: no regula ni limita instantáneamente a 3 A. [JST XH](https://www.jst-mfg.com/product/index.php?lang=2&series=277), [F1](https://www.lcsc.com/product-detail/C69689.html).

El presupuesto del proyecto llega a unos 2.5 A para Pi/cabeza y añade Teensy, shield y LEDs. Si se reservan además 0.4 A para LEDs y 0.5 A para un controlador USB alimentado por HOST, el total ilustrativo supera 3 A. **Es un escenario de presupuesto, no un consumo medido ni una afirmación de que la Pi siempre consuma 2.5 A.** Raspberry Pi recomienda 5 V/2.5 A para la familia Pi 3, pero eso no certifica la alimentación del instrumento completo. [Raspberry Pi: alimentación](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html).

En la PCB, la alimentación de 5 V usa pistas de 0.8 mm. De F1.2 a J6.4 hay unos 36 mm; de J10.1 a F1.1, 10.4 mm. Para cobre de 35 µm, ese trayecto positivo tiene aproximadamente 29 mΩ de cobre: unos 73 mV a 2.5 A, **antes** del fusible, conectores, cable y retorno GND. Es una estimación resistiva, no una certificación térmica.

**Cierre:** presupuesto de corriente para la configuración real, límite de LEDs si se montan y capacidad del controlador MIDI. Elegir conectores/contactos documentados y cable corto de sección adecuada; resolver el tramo J6–Pi para el pico, no solo para el consumo típico. Si la carga exige más de lo que admite J10, hay que revisar la distribución y la protección además de cambiar la fuente. Es viable mantener **una sola fuente**, pero el camino completo debe soportarla.

Prueba antes de liberar: Pi + pantalla + Teensy + shield + controlador MIDI previsto, brillo/carga máximos esperados; comprobar arranques, tensión en la Pi y ausencia de eventos de undervoltage. No basta una medida en vacío en la fuente.

### F4 — Media, riesgo directo de retrabajo: falta comprar piezas que correspondan al diseño

**PAD:** el esquema usa un **DP3T real**: dos comunes, cada uno seleccionando una de tres tomas. La lista de compras admite “ON-ON-ON u ON-OFF-ON según cableado”, que no especifica esa función.

Un DPDT ON-OFF-ON ordinario abre el circuito en el centro; un DPDT ON-ON-ON de seis terminales tampoco equivale, por sí solo, a dos selectores independientes de tres tomas. Un modelo concreto puede usarse con otra red o cableado, pero eso se debe diseñar y verificar. No comprar por el número de posiciones de la palanca.

La función que debe cumplir el selector, con la PCB actual, es esta:

| Posición | Unir en SW1, canal L | Unir en SW1, canal R |
|---|---|---|
| 0 dB | 3 ↔ 1 | 7 ↔ 5 |
| −10 dB | 3 ↔ 2 | 7 ↔ 6 |
| −20 dB | 3 ↔ 4 | 7 ↔ 8 |

Conseguir un selector de dos polos y tres vías, comprobarlo con continuidad, y verificar su cuerpo/rosca frente a la caja. Puede ser rotativo o de otro tipo; la compatibilidad mecánica debe revisarse. Esto puede resolverse externamente sin alterar las pistas del pad.

**Pots:** “RV09 B10K” no define la huella. PCB-A usa `Potentiometer_Alpha_RD901F-40-00D_Single_Vertical`: tres terminales a 2.5 mm, fila a 7.5 mm del eje, y anclajes a ±4.8 mm del eje. Ambos son **lineales B10K**, pues son controles ADC, no atenuadores de audio. Comprobar orientación de terminales, anclajes, eje, buje y altura con el modelo adquirido antes de fabricar. Si solo se consigue otra geometría, conviene cambiar la huella ahora.

## Qué sí quedó comprobado

### Integridad eléctrica, CAM y ensamblaje

| Comprobación | Portadora | PCB-A | PCB-K |
|---|---:|---:|---:|
| DRC con planos recalculados | 0 violaciones | 0 | 0 |
| Conexiones sin rutear | 0 | 0 | 0 |
| Paridad PCB/esquemático | 0 discrepancias | 0 | 0 |
| ERC | 0 errores, 2 advertencias | 0 | 0 |
| CAM actual vs ZIP en fab | 11/11 coinciden | 11/11 | 11/11 |

Las dos advertencias ERC de la portadora son `HOST_DM` y `HOST_DP`, etiquetas con un solo pin. Corresponden a J3, que no conecta el host de la Teensy por las filas laterales. El USB-A debe cablearse al header host de la propia Teensy, tal como ya documenta theory/02 §10D. No se contaron como una rotura nueva de la PCB.

La comparación CAM cubre nueve Gerbers y dos taladros, ignorando únicamente comentarios de fecha/generador y finales de línea. Se comprobó la integridad de los ZIP. El `.gbrjob` es información auxiliar y no forma parte del 11/11.

Las reglas activas tienen separación entre redes de 0.25 mm y pista mínima de 0.2 mm; GND ocupa ambas caras. No hay exclusiones individuales DRC. Hay categorías configuradas como `ignore`, entre ellas tipo/filtro de huella y courtyard ausente; en la portadora también se ignora divergencia respecto a huella de biblioteca. Por ello **DRC no certifica dimensiones de piezas compradas**. La separación general al borde es 0.25 mm y PCB-K tiene una excepción local de 0.2 mm en las ventanas LED. [Capacidades del fabricante](https://jlcpcb.com/capabilities/pcb-capabilities/).

- Portadora: BOM de **19 líneas / 38 piezas**, CPL de **38 posiciones**, conjuntos de referencias idénticos, valores/códigos/encapsulados iguales a PCB; coordenadas y cara coinciden. Los 19 códigos consultados existen, tienen stock informado y admiten Economic en la consulta de esta fecha. No implica reserva de stock ni precio de pedido.
- PCB-A: correctamente entregada sin BOM/CPL para ensamble; el pedido previsto es placa desnuda.
- PCB-K: su BOM y CPL también coinciden, pero pertenece al pedido anterior.
- Las correcciones de rotación del exportador no se calificaron como errores: C1 180°, U2 270°, SOT-23 180° son convenciones previstas. La **orientación final del modelo de JLC debe revisarse** antes de pagar; esta auditoría no ha visto ese visor.

### Interfaces y señales

- Los **30 pines** de portadora J5 y PCB-A J2 coinciden número a número. Los **10 pines** eléctricos de PCB-A J1 y PCB-K J1 también coinciden; sus conectores físicos son distintos en la revisión económica, por lo que no se debe asumir que un cable JST–JST antiguo sirva.
- La UART está cruzada correctamente: Pi pin 8 TX → Teensy pin 0 RX; Teensy pin 1 TX → Pi pin 10 RX. Pi recibe 5 V por 2/4 y GND por 6/9; no se conecta el 3V3 de la Pi al de la Teensy.
- I2S/I2C coinciden con Rev D: 7 DIN, 8 DOUT, 20 LRCLK, 21 BCLK, 23 MCLK, 18 SDA y 19 SCL. Los pines del slot SD del shield no se conectan a los encoders. Las filas del shield tienen separación de 15.24 mm y orientación coherente con el montaje documentado. [PJRC Audio Adapter](https://www.pjrc.com/store/teensy3_audio.html).
- MCLK mide aproximadamente **27.2 mm sin vías**; BCLK 31.3 mm; LRCLK 22.6 mm con una vía; RX/TX 44.2/48.4 mm sin vías. No aparece un cableado de reloj desproporcionado, pero falta medir integridad de señal/audio en hardware. El shield oficial ya incorpora 100 Ω en MCLK.
- U2: alimentación en 4/11 y seguidores L/R/VREF/CV cableados según el pinout MCP6004. D2/D3: 1 GND, 2 +3V3, 3 nodo protegido. U1: OE a GND, entrada desde pin 28, alimentación a 5 V, salida al jumper y R3 de 330 Ω. [Microchip MCP6004](https://www.microchip.com/en-us/product/MCP6004).
- PHONES: `J13.S` llega exclusivamente a `J9.9 / HP_VGND`, no a GND. La compra del jack aislado de plástico sigue siendo obligatoria para preservar ese circuito en el montaje. LINE OUT e IN son conexiones **no balanceadas**; que el jack sea TRS no las convierte en balanceadas.

### Audio analógico y CV

Se ejecutó otra vez `sim/input_stage_sim.py` contra el esquema actual, con sus resultados escritos en la carpeta de auditoría. Pasa C1–C6 **dentro de ese modelo**: pad −10.36/−20.33 dB, VREF 1.65 V, CV con asentamiento de 5.2 ms y ripple de unos 348 µVpp a la frecuencia PWM supuesta.

Se cerró una incertidumbre documental: **Rev D sí tiene 2.2 µF de acople en cada LINE IN y LINE OUT**, según su esquema oficial. Con C8/C9 de 4.7 µF en serie, la capacidad equivalente de entrada es ~1.50 µF. El cálculo ampliado da unos **−0.17 dB a 20 Hz respecto a 1 kHz** con 29 kΩ, aún dentro de ±0.5 dB. Con la carga de estrés de 10 kΩ da aproximadamente −1.1 dB. El datasheet del SGTL5000 especifica 29 kΩ típicos: no es un mínimo garantizado. [Esquema Rev D](https://www.pjrc.com/store/schematic_audio4.png), [NXP SGTL5000](https://www.nxp.com/docs/en/data-sheet/SGTL5000.pdf).

Un barrido determinista de las cinco resistencias relevantes a ±1 %, con fuente de 100 Ω, da −10.482 a −10.238 dB y −20.493 a −20.176 dB. Pasa el criterio de ±0.5 dB con muy poco margen en el peor extremo; no incluye temperatura, capacitancias variables, mayor impedancia de la fuente ni tolerancia del opamp.

**No convertir el “THD 0.001 %” simulado en una promesa de calidad:** el MCP6004 está representado por un modelo propio por parámetros, no el macro oficial; el riel es ideal, y el ensayo VREF usa el segundo canal en reposo. No demuestra THD+N real, ruido digital de la Pi, transitorios al encender/apagar, inyección con la placa apagada, ni inmunidad ESD. La respuesta nominal es razonable; la calidad sonora requiere la prueba física posterior.

### Mecánica

Se verificó que los ejes de los cinco EC11 y de los dos pots coinciden con los agujeros del DXF. Los seis agujeros de montaje de PCB-A coinciden con los centros de espárragos en (20/70/110/160, 47) y (15/165, 84) mm. Los diez agujeros PEM del panel ya son de Ø3.0 mm, como indica la revisión del 18 de octubre.

Esto no valida las piezas aún no compradas ni reproduce las holguras tridimensionales del enclosure. Se mantiene la necesidad de comprobar los jacks, altura real de los zócalos de 8.5 mm, salida lateral del cable del shield y holgura del GPIO con las piezas reales. Las anteriores correcciones de jacks y PEM no se vuelven a presentar como fallos pendientes.

## PCB-K y alcance del primer prototipo

No enviarla en este pedido. ADR-004 rev. 3 y theory/02 §10F la excluyen; su ZIP sigue en `fab` por historial. Las luces cableadas posteriores no convierten automáticamente PCB-K en parte del pedido.

Si se retoma: revisar tolerancia de ventanas SK6812MINI-EA, radio de fresa y montaje invertido con JLC; el propio theory/02 reconoce una ventana más ajustada que la recomendada. Revisar el modelo colocado por JLC en la cara inferior y preparar el cable mixto JST-SH ↔ header de PCB-A. Su DRC limpio no cierra esas verificaciones.

## Orden de cierre para proteger el gasto

1. Resolver y probar F1, manteniendo consistente el contrato de pines. Si se cambia la PCB, repetir DRC/paridad y exportar de nuevo.
2. Identificar la fuente existente y cerrar F2/F3 con una única alimentación, cableado documentado y carga máxima definida. No comprar una fuente mayor como única corrección.
3. Comprar o identificar por ficha los pots y el selector PAD; comprobar huella/altura y la tabla de continuidad anterior. Ajustar PCB/caja solo si esas piezas lo requieren.
4. Revisar en el pedido real las 38 piezas de la portadora, polaridad de C1, pines de U1/U2/D2/D3, taladros y contornos; confirmar dos capas, espesor de 1.6 mm y cobre elegido. Pedir solo portadora ensamblada SMD y PCB-A desnuda.
5. Al recibir: inspección y continuidad sin módulos; alimentar con límite de corriente; verificar +5V/+3V3/VREF, aislar VUSB–VIN para alimentación externa según PJRC, insertar módulos en la orientación indicada y hacer primero audio básico. Después verificar entrada estéreo, pad, controles y carga completa. Medir antes de conectar PHONES al panel metálico.

**Criterio para cambiar el dictamen:** F1 resuelto con evidencia; fuente/entrada/cableado definidos para la carga real; pots y selector compatibles confirmados; CAM regenerado si hubo cambios; revisión final de ensamblaje del pedido. Las pruebas de calidad sonora y temperatura continuarán siendo gates del primer prototipo físico.
