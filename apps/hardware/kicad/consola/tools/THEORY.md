# Esquemas declarativos: conectividad antes de dibujo

El contrato es una relación explícita `(referencia, número de pin) → red`, no la proximidad de
textos. Cada pin se declara con una red o `None` (NC); omitirlo es un error. Las unidades de un
mismo circuito integrado comparten referencia y tienen números de pin distintos. Las librerías
instaladas de KiCad son la autoridad para geometría, números y tipos eléctricos de los pines.
No se sustituyen por dibujos o conectores inventados.

KiCad usa coordenadas cartesianas en las librerías y coordenadas de pantalla en las hojas.
Para un pin `(px, py)` y un símbolo en `(x, y)` rotado θ:
`X = x + px cos θ − py sin θ`, `Y = y − px sin θ − py cos θ`.
La orientación del pin apunta hacia el cuerpo: el cable de etiqueta sale en la dirección
contraria. Se comprueban envolventes que incluyen cuerpos, pines, cables y espacio para textos.
Las posiciones de instancias se expresan en milímetros y se validan en grilla de 1,27 mm.

Los rieles y las señales compartidas entre hojas usan etiquetas globales; las otras redes usan
etiquetas locales. UUID v5 de nombres persistentes permiten regenerar sin ruido aleatorio.
Los símbolos derivados se resuelven contra su padre antes de incrustarlos en `lib_symbols`.

La prueba de aceptación exporta con KiCad 10 y compara las particiones de pines por red,
además de sus nombres: dos redes distintas fusionadas son siempre un fallo. NC se valida
separadamente porque el XML puede representarlo con una red `unconnected-(...)`. ERC comprueba
tipos eléctricos, pero no prueba la intención ni sustituye la comparación pin a pin.
Las pruebas unitarias cubren transformación, colisiones y estabilidad. La prueba end-to-end usa
un resistor real de librería y el exportador de KiCad; se omite solo si KiCad no está disponible.
Nada de esto valida consumo, estabilidad analógica, layout, cableado externo o funcionamiento
físico. Los conflictos documentales se conservan explícitamente en REPORT.md.

Referencias de formato y geometría: documentación instalada de KiCad y librerías
`/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols/*.kicad_sym`;
contrato eléctrico: PCB-M/README.md, docs/consola/24 y 17. La CLI instalada (`kicad-cli sch --help`)
es la referencia para los comandos ejecutados.


## PCB-M: decisiones aprobadas D8–D15 (2026-11-12), antes de implementar

La placa fabricada fija el orden de las señales de J1, mientras que ENCODER_PINS del firmware y
`17` fijan su destino Teensy. El test cruzará estos contratos independientes, incluidos faders al
ADS0x48 y los cuatro NC. No basta exportar el mismo error que esté en la descripción.

El net-tie mantiene dos nombres de red y permite una única unión física de cobre; la huella
estándar declara `net_tie_pad_groups "1, 2"`. NT1 se debe colocar junto al LDO de audio en layout.
No unir HP_VGND al net-tie. El ERC no certifica la colocación física ni los retornos externos.

CV: la realimentación se toma antes del resistor de 1 kΩ, y los diodos BAT54S (1=GND,
2=+5V_LED, 3=salida protegida) limitan el nodo del cable a los rieles del buffer. El resistor
limita la corriente de fallo; no garantiza sobrevivir a cualquier tensión externa. Pot: el cursor
alimenta 1 kΩ y 100 nF a GND en el lado ADC; f_c=1/(2πRC)≈1,59 kHz y τ=100 µs. Validar
asentamiento con la impedancia real del pot/ADC en banco.

Selector: divisor permanente 10k/10k entre 3,3 V y GND, punto medio PAD_SEL_ADC. Tres hilos:
común del polo C al ADC; contactos extremos a los rieles, contacto central sin cable. El centro
queda a 1,65 V sin una cuarta línea; extremos fuerzan 0/3,3 V. La carga extrema es 0,33 mA.
Los polos A/B del mismo selector siguen seleccionando los taps de audio a través del cable existente.

MIDI OUT: CA-033 figura 1 especifica RA=33 Ω ±5 %, 0,5 W (riel) y RC=10 Ω ±5 %, 0,25 W
(TX) a 3,3 V ±5 %. Usar footprints 1210 y 1206 respectivamente, seleccionar potencia nominal
al comprar. Eliminar la resistencia serie adicional. TRS tipo A: tip→DIN5, ring→DIN4,
sleeve→DIN2; en IN sleeve abierto para conservar aislamiento. Se preserva el opto existente.
Referencias primarias: [MIDI Association CA-033](https://midi.org/5-pin-din-electrical-specs),
[CA-033 publicado por AMEI, fig.1 p.2](https://amei.or.jp/midistandardcommittee/Recommended_Practice/ca33-j.pdf),
y [MIDI Association TRS RP-054](https://midi.org/specifications-old/item/trs-specification).
BAT54S: hoja de datos enlazada en la librería KiCad Diode:BAT54S
(https://www.diodes.com/assets/Datasheets/ds11005.pdf).

Los tests validarán el recorrido de protecciones, el aislamiento de las redes del RC, las dos
redes del net-tie, valores MIDI, ladder de 3 hilos y contactos de cada mazo JST-PH. La exportación
real XML + ERC confirma conectividad. El PDF permite revisar el dibujo. No cubren montaje de
panel, ruido, potencia/consumo, transitorios USB, MIDI/CV físicos ni DRC: falta layout.
