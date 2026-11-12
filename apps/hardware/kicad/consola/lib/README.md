# gf_consola.pretty

Fuentes regenerables: `../tools/schgen/footprints.py`.

- `Kailh_Choc_V2_PG1353_Hotswap`: geometria PG1353 de doc 28 / instruccion de Juan.
  Ejes: panel +y hacia atras se convierte a KiCad -y. Centro NPTH 5.0; guia NPTH 1.5;
  centros de contactos (panel): (0,-5.90), (5,-3.80). Los agujeros de contacto son
  3.0 para el zocalo, no taladros metalizados de 1.2 para soldar la tecla directamente.
  Los pads inferiores 2.6 x 2.6 en x -3.275 y 8.275 son PROVISIONALES. No se encontro
  una huella hot-swap local; la huella GroovePush es soldada y usa otra orientacion.
  **VERIFICAR CON ZÓCALO REAL** antes de fabricar. No son cotas de pad certificadas.
  La huella se coloca F; sus pads de zocalo ya son B. El recorte LED lo aporta Dn,
  centrado a (0,+4.93) en panel / (0,-4.93) local KiCad.
- `SK6812MINI-E_ReverseMount`: copia de la libreria estandar KiCad 10.0.6,
  `LED_SMD:LED_SK6812MINI-E_3.2x2.8mm_P1.5mm_ReverseMount`, renombrada sin cambios
  geometricos. Incluye el recorte real en Edge.Cuts. Colocar en B en PCB-K.
  Procedencia/licencia de librerias KiCad: https://www.kicad.org/libraries/license/ .
- `FSR402_SolderTail_Provisional`: disco de referencia 18.3 (CSV de panel), pads manuales
  sin pasta 2 x 4 a paso 2.54, centro en (+/-1.27,11) KiCad. Es un landing generico
  para cola acomodada/plegada, no un land pattern certificado de Interlink. Medir la
  variante de cola, espacio y metodo de terminacion antes de layout/fabricacion.

Las huellas de zocalo y FSR llevan su advertencia dentro del archivo y sobre Fab/SilkS.
Ver `../tools/REPORT-placas.md`. No se ha generado ningun layout con estas huellas.
