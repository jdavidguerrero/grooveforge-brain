# Revisión de las correcciones de preproducción — 23 de octubre de 2026

Alcance: cinco PCB del panel (A, F, K1, K2, S), archivos `fab`, herramientas modificadas y respuesta a los hallazgos D01–D09. Revisión de las correcciones del commit `125762ad28ad1d17150447d73a9a7aac78681ff9`, sobre HEAD `1cad391`. La portadora no se reaudita.

**Dictamen:** el defecto de fabricación D01 está resuelto en los archivos actuales y los ZIP están actualizados. La revisión mejora sustancialmente la fabricabilidad. No corresponde cerrar toda la auditoría: siguen pendientes la orientación final de ensamblaje, la validación mecánica/ESD y los márgenes eléctricos. Además, el nuevo control de vías no está funcionando como bloqueo del proceso de construcción.

No se ejecutó `build.sh`, ni se cambiaron PCB, esquemáticos, firmware o archivos `fab`. Los resultados nuevos están en esta carpeta; la auditoría anterior permanece intacta.

## Hallazgos de esta revisión

### R01 — Media: el build ignora el fallo del nuevo verificador

Ubicación: `hardware/boards/tools/build.sh`, líneas 40–43.

La expresión `check_via_in_pad.py ... | head -12 || true` permite seguir aun cuando el verificador termina con código 1. El DRC siguiente tampoco utiliza `--exit-code-violations` y también termina con `|| true`. Finalmente, el `grep` de incidencias, con `pipefail` activo, devuelve 1 cuando el reporte está limpio y 0 cuando contiene incidencias. Por tanto, el estado de salida del build no representa correctamente la validez del diseño.

Lo reproduje con las mismas expresiones en un shell aislado, sin reconstruir placas: un verificador que falla continúa; el resumen limpio devuelve 1 y el resumen con una incidencia devuelve 0. Evidencia: `build-gate-check.json`.

**Acción:** propagar el código de salida de `check_via_in_pad.py` sin `|| true` ni truncado que pueda ocultarlo; ejecutar DRC con `--exit-code-violations`; hacer que el resumen sea informativo y no determine el resultado. Si se permite exportar por separado, añadir validación previa en `export_fab.py`. El diseño actual pasa ambos verificadores de vías; este hallazgo afecta la prevención de futuras regresiones.

### R02 — Media: el desacoplo de K1.U2 sigue siendo una mejora parcial

Ubicaciones: `pcb-k1.kicad_pcb`, vías en `(31,75;206,75)` y `(29,85;206,75)`; `AUDIT_RESPONSE.md`, línea 19.

La distancia de 5,42 mm indicada para U2.9→C2.1 es en línea recta. El recorrido de cobre reconstruido entre esos pads mide aproximadamente **9,62 mm y cambia de cara dos veces**. El cálculo asigna longitud cero a los barriles: no representa la inductancia ni incluye el retorno de GND. No hay razón para tratar los 5,4 mm como longitud de conexión efectiva.

En contraste, A.U1→C1 y K1.U1→C1 sí quedan conectados mediante recorridos cortos de aproximadamente **2,45 y 2,42 mm**, respectivamente.

**Acción:** revisar manualmente el escape VDD–C2 de U2, priorizando la conexión local de bypass antes que otras ramas de 3,3 V. Si el espacio realmente impide mejorarlo, mantener D06 como mitigado y verificar VDD/VSS en banco durante actividad y transitorios. No afirmo que 9,62 mm provoquen un fallo inevitable del MCP23017. Evidencia y camino: `decoupling-paths.json`.

### R03 — Pendiente antes del ensamblaje: dos CPL candidatos no son una orientación aprobada

Ubicación: `AUDIT_RESPONSE.md`, líneas 31–49; `fab/README.md`, selección de CPL.

Confirmé que ambos archivos tienen posiciones y caras equivalentes, pero difieren 180° para los componentes indicados. Entre los componentes con orientación eléctrica o mecánica relevante están:

- K1: D1…D15, U2, J1 y J2.
- K2: D1…D5 y J1.
- A: J2 y J3.

No se ha verificado cuál representa correctamente cada modelo de montaje de JLC. La coincidencia de un LED y un MCP no valida automáticamente todos los conectores. Comprobar pad 1, esquina funcional del LED y apertura del conector para cada familia/orientación. Tras esa revisión debe quedar **un CPL aprobado por placa**, con revisión y evidencia de la vista del ensamblador. Los CSV alternativos pueden conservarse como investigación, claramente separados del paquete aprobado.

No se subieron archivos a JLC ni se generó un pedido durante esta revisión. Las coordenadas del CSV de polaridad de la auditoría anterior siguen coincidiendo con las piezas que contiene; ese CSV incluye LEDs e IC, pero no los conectores JST, que requieren comprobación propia.

### R04 — Media: no dar por cerrada la protección ESD ni la limitación por el simple cambio de net

Ubicaciones: `AUDIT_RESPONSE.md`, líneas 16/21/33; `CONNECTORS.md`, líneas 114–121.

Sí confirmé **16 pads MP de los ocho encoders y 12 pads MP de los cuatro faders a GND con unión sólida**. Es una corrección física verificable. Sin embargo, la frase de que el eje/carcasa “por tanto” descarga por esa unión presupone continuidad en la pieza real; tampoco queda definido y demostrado el recorrido hasta chasis. D03 debe permanecer **parcial**, con continuidad de la pieza y ensayo del conjunto pendientes.

La declaración de una polyfuse de 3 A en la portadora es un requisito externo, no una protección verificada en este alcance. Una PPTC es una protección dependiente de corriente, tiempo y temperatura; no debe interpretarse como un techo instantáneo de corriente de 3 A. Hace falta la curva del componente concreto y su coordinación con el ramal del panel. [Littelfuse: comportamiento de PPTC](https://www.littelfuse.com/products/fuses-overcurrent-protection/polyswitch-resettable-pptc-devices).

**Acción:** describir D03 como conexión MP corregida, inmunidad pendiente; D08 como contrato de uso sin hot-plug, protección aguas arriba pendiente de evidencia. Retirar la afirmación de que la rotación del CPL es “lo único que puede dañar placas ya montadas”: mecánica, ESD y fallos de alimentación continúan siendo riesgos reconocidos en el mismo documento.

## Estado independiente de D01–D09

| ID | Resultado de la revisión | Cierre |
|---|---|---|
| D01 | **39 invasiones de taladro → 0; 28 centros dentro de pads → 0**, usando el comprobador de la auditoría. El nuevo comprobador, más conservador en margen, también pasa las cinco placas. | **Cerrado para los archivos actuales.** R01 pendiente en automatización. |
| D02 | Los anclajes no cambiaron. Se propone transferir cargas al panel, pero no se presentó evidencia física de esa sujeción. Un agujero de paso del eje por sí solo no demuestra retención axial del encoder. | **Abierto.** Puede resolverse en el enclosure si soporta realmente PCB/controles; no se exige añadir agujeros sin espacio. |
| D03 | MP conectados y sólidos, verificado; enlace de chasis y continuidad/ensayo pendientes. | **Parcial.** |
| D04 | K1.C3: 1,70→**7,40 mm** al borde; C24: 1,10→**5,77 mm**; K2.C10…C14: 1,60→**2,70 mm**, giro confirmado; F.C5: 3,16→**13,09 mm** al centro de H3. | **Correcciones de layout verificadas.** K2 sigue dependiendo de soporte y despanelizado. F.C5 queda ahora a 2,60 mm del borde: considerar ese borde al panelizar. |
| D05 | El solape/vecindad entre caras sigue presente. Con segmentos paralelos, separación entre centros ≤0,20 mm y unión de intervalos, obtuve ≈**84,73 mm SDA/SCL en K1** y ≈**77,08 mm ENC2_B/ENC4_A en A**. Es compatible con las estimaciones redondeadas de la respuesta, pero no es el mismo criterio que medir un único tramo sin plano. | **Riesgo aceptado para validación de prototipo, no cerrado por medida.** |
| D06 | Posición mejorada en los tres MCP. Recorrido real corto en A.U1 y K1.U1; U2 mantiene 9,62 mm y dos vías. | **Mejorado/parcial**, ver R02. |
| D07 | R1/R2 son 2,2 kΩ en PCB/esquema/BOM. El valor ≈161 pF para 300 ns es correcto bajo el modelo RC y esa resistencia equivalente. | **Cambio implementado; margen del bus pendiente de medir.** No se verificó comportamiento real a 100/400 kHz. |
| D08 | Uso con equipo apagado y cables polarizados documentado; no se añadieron dispositivos locales. | **Documentado, no probado.** Protección de la fuente pertenece al otro proyecto. |
| D09 | Se mantiene la decisión de usar Economic. No se observaron nuevos fiduciales/rieles; su aceptación depende del servicio contratado. | **Sin bloqueo adicional bajo ese proceso**, pendiente del DFM final del ensamblador. |

La estimación de acoplamiento de 1,5 pF y 0,1 V no viene acompañada de un modelo extraído ni caracteriza flancos rápidos de bajada, estados de la línea víctima o susceptibilidad de retorno. Puede orientar un prototipo; no acredita inmunidad. Tampoco justifica exigir cuatro capas automáticamente. Medir con mazo final, LEDs activos y sondas adecuadas, incluyendo ringing y falsas transiciones.

Para D07: `tr = 0,8473·R·C`; con R=2,2 kΩ y tr=300 ns resulta C≈161 pF. Hay que incluir los otros pull-ups presentes en el sistema y capacitancia completa. [TI: dimensionado de pull-ups](https://www.ti.com/lit/an/slva689/slva689.pdf).

## Comprobaciones ejecutadas sobre el estado actual

| Comprobación | A | F | K1 | K2 | S |
|---|---:|---:|---:|---:|---:|
| DRC tras rellenar zonas: incidencias / sin conectar / paridad | 0/0/0 | 0/0/0 | 0/0/0 | 0/0/0 | 0/0/0 |
| ERC | 0 | 0 | 0 | 0 | 0 |
| Invasiones taladro–pad SMD, comprobador independiente | 0 | 0 | 0 | 0 | 0 |
| Diferencias ZIP frente a exportación actual, normalizando fechas | 0 | 0 | 0 | 0 | 0 |
| BOM/CPL: referencias, LCSC, posición y cara | OK | OK | OK | OK | Sin PCBA |

Además: **30/30 pines de las cuatro interconexiones internas coinciden** entre las nuevas netlists. La validación BOM/CPL de esta tabla no certifica la orientación en la biblioteca del ensamblador.

KiCad se abortó al intentar DRC dentro del sandbox; se repitió fuera de él sin `--save-board`, y completó los cinco controles. Los hashes de las fuentes y de `fab` se verificaron al final para comprobar que la revisión no los modificó.

## Dependencia de puesta en marcha que no debe confundirse con fallo de PCB

No es una regresión de estas correcciones ni una reauditoría de la portadora: el `ButtonManager` actual todavía declara `mcp[2]`, inicializa 0x20/0x21 y utiliza el mapeo provisional. La PCB tiene un tercer MCP en 0x22 y un reparto de teclas distinto. Ejemplo: GPA0 de 0x20 corresponde a ENC5, mientras que el firmware lo asigna a ENC1. El software de prueba deberá inicializar los tres dispositivos y usar el contrato actualizado; de lo contrario, una PCB correcta parecerá tener teclas ausentes/intercambiadas. No se modificó ni se compiló el firmware en esta revisión de hardware.

## Decisión antes de gastar

1. **El bloqueo anterior de vías en pads está levantado.** No hace falta rehacer esa corrección.
2. Antes de pagar PCBA, cerrar la orientación del CPL completo por placa y comprobar las piezas reales con el panel/huellas. Mantener claro que la validación mecánica sigue abierta.
3. Corregir el control de errores de `build.sh` para que futuras regeneraciones no puedan exportarse por una falsa sensación de validación.
4. Revisar el recorrido local de K1.U2/C2; si se conserva, aceptarlo expresamente como punto a medir en el prototipo.
5. El primer lote puede plantearse como **prototipo de validación**, con prueba funcional preparada. La liberación comercial continúa condicionada a mecánica, ESD, alimentación y medidas de señal. No equivaler «documentado» o «aceptado para prototipo» a «ensayado y cerrado».

Evidencias: `inspection.json`, `advanced-checks.json`, `decoupling-paths.json`, `parallel-routes.json`, `mp-bonding.json`, `build-gate-check.json`, `new-checker-results.json`, `interface-checks.json`, netlists y reportes DRC/ERC. Se conservó la auditoría del 22 de octubre como referencia histórica.
