# Diagnóstico y comprobación de mazos — teoría previa

Spec aprobado: doc24 §10 (2026-11-12). Un puente separa nombres de red aunque su cobre cerrado
conduzca; el XML debe conservar cada lado para poder medir/abrirlo. ERC necesita PWR_FLAG en
rieles pasivos alimentados por jumper/RC, sin declararlos fuentes físicas nuevas. Abrir solo VCC
no evita backfeed por señales. El RC atenúa ruido rápido a cambio de caída y asentamiento; no
se promete swing ideal de CV. DNP paralelo solo permite bajar resistencias existentes.

El verificador de cables compara pin físico, no geometría ni nombres de referencia parecidos.
Lee XML de KiCad y libparts para contar incluso pines NC/ausentes. Un NC intencional debe estar
autorizado por extremo y pin (D8); no se ignoran desplazamientos. Alias se declaran por placa:
+5V de PCB-A es +5V_LED según D9, nunca un alias global para cualquier +5V. Nombres privados
con prefijo de hoja se resuelven por alias exacto o por su nombre base cuando no es ambiguo.
Los rieles prohibidos se clasifican ANTES de aplicar alias; ninguna equivalencia debe ocultar
5V_LED/5V_TEENSY, 3V3/3V3A ni HP_VGND/masa. Panel sin netlist se valida contra contrato de
contactos (tip/ring/sleeve, cursor, etc.); es expectativa documental, no medida de continuidad.
Un conector de jack/panel no documentado debe fallar. PCB-A que no exporte se reporta pendiente,
y los cables asociados no se cuentan como comprobados. Las piezas mecánicas siguen pendientes.

Tests previos al banco: ejemplos XML buenos, cruce de rieles, desplazamiento, pin faltante,
conteo real, mapa no biyectivo, alias peligroso y cobertura de panel. Regresión D8–D15 adaptada
solo donde el nuevo puente interrumpe deliberadamente una red. No valida crimpado, orientación
mecánica del housing, corriente/cable, ruido, layout ni dispositivos físicos. HARNESS.md incluye
colores sugeridos por función; la raya del IDC identifica pin1, no una equivalencia de colores.


Ampliación aprobada D16–D23 antes de captura: R_ISO_IN 0 Ω separa el frente del códec; MIDI
completamente DNP libera pin0 para EN (GPIO desconectable antes de forzar alto) y pin1 queda NC.
PCA9517A: A local compartido, B independiente A/K, nunca unir B–B; pull-ups independientes por
segmento, TP y 47 Ω/ESD en cables. USBLC6-2SC6 C7519, 3,5 pF máx. por línea (ST DS4260),
reemplaza el ejemplo PESD5V0S2BT de capacitancia excesiva. Esquema no demuestra inmunidad ESD.
El checker debe comprobar dominio de I2C ANTES de alias, con contrato por extremo físico;
SDA remota de K equivale a B_K y SDA remota de A equivale a B_A, nunca a local/A.
Las dos puntas M–K cambian a 10 vías. Tests añaden corto A/B oculto por alias, conector incorrecto,
DNP real en salida KiCad/BOM, aislamiento del códec y pinout de protección/buffer contra datasheets.

La raíz con 12 hojas necesita reservar el cajetín de KiCad: que los bloques quepan en el marco
no basta si invaden el área inferior. El selector de papel deja 50 mm inferiores para cajetín
y textos, y el test comprueba ese margen. Referencias descriptivas heredadas causan que el
exportador BOM agregue «?»; el test normaliza solo ese sufijo antes de comprobar exclusión
MIDI y presencia de componentes activos, sin renombrar referencias del proyecto.
