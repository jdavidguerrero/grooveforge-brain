# Revisión 2 — teoría y alcance previo a implementación

Spec: doc24 §8 D38–D43 y doc28 §9, aprobados por Juan 2026-11-12; D40 actualizado por Juan 2026-11-13.

Un origen USB común elimina la secuencia estable DAC encendido/amplificador apagado al quitar
USB LED. Los clamps superiores de salida también pasan de +5V_LED a +5V_CV para que el
amplificador activo no alimente el riel LED apagado a través de ellos. RC sigue retrasando VDD y el jumper permite abrirla: no prueba límites absolutos en
rampas/fallo local. No abrir SJ_CV con DAC activo; servicio apagado y sin señales externas.
La resistencia UART limita corriente, no convierte un GPIO en Ioff. El cálculo RC a 1 Mbps
es condicional a C≤100pF; verificar mazo real y secuencias separadamente.

A1: el extremo M4 se identifica por los pads SDA/SCL/GND/3V; no por números de CONN1.
El contrato del mazo valida una biyección entre pines de J3_M4 y esos rótulos. Juan confirmará
los rótulos impresos antes de soldar; el software no acredita lectura física ni continuidad.

El pull-down 100k mejora fuga/tiempo pero carga más al pot. Doc24 contiene ecuaciones,
umbral y plazo: tests validan números y topología; no modelan ADC ni prueban desconexión real.
Las entradas en mínimo y con cable abierto son eléctricamente indistinguibles.

Para M1 usar K1/A2 con redes intactas y marca de cátodo en footprint. Para M2 validar dos
propiedades distintas: identidad de cada compra e integridad entre esquema, XML y BOM exportada.
Leer lib_id real del esquema y libsource del XML; la huella es editable y no acredita tipo.
Mantener DNP/excluidos BOM explícitos y comparar también metadatos. Pruebas de mutación deben
borrar/agregar filas XML/CSV y disfrazar un IC con huella TestPoint sin eludir el checker.

M4: a y84.5/y73.5 V1–V4 están bajo K derecha, X1/X2 fuera de K. 1.5mm libres no admiten un
socket hembra vertical. D43 elige terminales directos con envolvente superior ≤1mm, sin calor
sobre película; la validación física de crimpado/cola sigue siendo requisito, no PASS de software.
Pruebas de geometría validan solape y handoff; no certifican alturas de la pieza de Juan.

No modificar firmware, boards/pcb-a ni layouts F/P. ERC de A se ejecuta en copia dentro del
clon; los otros cinco esquemas se regeneran desde schgen. No hay push.
