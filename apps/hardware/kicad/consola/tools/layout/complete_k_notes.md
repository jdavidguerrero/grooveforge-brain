# Criterio previo a completar K (2026-11-13)

Se conserva la geometría eléctrica de teclas/zócalos, incluyendo los PTH Ø1,3.
La banda de navegación original termina en x_panel=73, pero sus pads inferiores
llegan a 73,575: se amplía el borde a x=74,5 para conservar cobre y separación.
Los courtyards superiores de las teclas contiguas representan 0,25 mm de margen
artificial sobre cuerpos de 15 mm a paso 15 mm: se dibuja la envolvente nominal
sin margen adicional en la dirección del paso, sin mover las teclas.

Los cuatro brazos del marco alcanzan (77,130), (77,158), (241,103), (244,172),
fuera de las filas de teclas. El borde trasero central de la M4 queda libre.
Se reservan círculos inferiores de radio 4,6 mm (taladro R1,6 + margen 3)
y todos los cobres se separan 0,5 mm de los bordes. Los seis pasos PCB-P
que caen en K derecha son Ø4,6 con reserva Ø7; los otros cuatro caen fuera.

DRC y pruebas del contrato son independientes del ruteador y condicionan
la liberación; no se excluyen errores eléctricos. Referencias: doc28,
CSV v5/v1, datasheets transcritos en doc28. REPORT-f.md/layout_f.py no están
en este clon; se continúa el generador y ruteador preexistentes.
