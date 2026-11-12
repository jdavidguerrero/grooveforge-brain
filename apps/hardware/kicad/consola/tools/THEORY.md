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
