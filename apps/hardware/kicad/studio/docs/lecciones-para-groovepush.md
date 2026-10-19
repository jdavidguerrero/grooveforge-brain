# Lecciones del hardware de Brain Studio para el layout de GroovePush

> 2026-10-19 · Resumen para pasarle a la sesión de `~/Documents/PROJECTS/groovepush`. Todo sale de lo que se diseñó,
> fabricó en archivos y probó en la protoboard del Brain Studio (`apps/hardware/kicad/studio/`,
> `apps/docs/variants/studio/theory/02-studio-pcb.md`).

## 1. Reglas de diseño (JLCPCB, 2 capas, 1.6 mm)

| Regla | Valor usado en el Brain |
|---|---|
| Pista mínima | 0.2 mm (señal), más ancho para +5V / GND |
| Separación cobre–borde | 0.25 mm |
| Separación de agujeros | 0.25 mm; agujero a agujero 0.5 mm |
| Vía mínima | 0.5 mm de diámetro, anillo 0.13 mm (se usaron 0.6 / 0.3 por comodidad) |
| Agujero THT mínimo | 0.3 mm |
| Texto de serigrafía | ≥ 0.8 mm de alto, 0.08 mm de trazo |
| Tamaño | **≤ 100 × 100 mm** cuando se pueda: es la tarifa base de JLC |

- DRC limpio contra estas reglas antes de exportar, y **paridad 0/0/0** (sin errores, sin advertencias relevantes,
  sin pistas sin conectar) tras cada re-ruteo.
- Plano de GND en las dos caras con vías de costura; no dejar islas.

## 2. Pedido en JLC (lo que funcionó para el costo)

- **Ensamble Economic solo de SMD**, en una sola cara. Todo lo THT (zócalos, IDC, encoders, jacks, headers) se suelda a
  mano: baja mucho el precio.
- Piezas con **número de LCSC** en el campo del símbolo (`set_lcsc.py` en el Brain) para que el BOM entre directo.
- **Revisar la vista previa del ensamble** en la web de JLC antes de pagar: rotaciones del CPL (en el Brain la
  auditoría marcó C1 y U2; estaban bien, pero es el último punto para atraparlo). Por eso conviene JLC y no PCBWay.
- **Varios diseños en el mismo carrito = un solo envío y una sola aduana.** Cada placa se cobra aparte.
- Placas que no llevan ensamble (como la PCB-A del Brain): solo gerbers.

## 3. Teensy 4.1 en zócalo

- Dos headers hembra 1×24 (izquierda y derecha). Numeración verificada con los pines de alimentación:
  - **Izquierda:** pad 1 GND · pads 2–14 = pines 0–12 · pad 15 = 3.3V · pads 16–24 = pines 24–32.
  - **Derecha:** pad 1 VIN · pad 2 GND · pad 3 3.3V · pads 4–14 = pines 23–13 · pad 15 GND · pads 16–24 = pines 41–33.
- Comprobar la orientación en el `.kicad_pcb` mirando que el pad 2 de la izquierda sea el pin 0 (UART RX) antes de
  mandar a fabricar.
- Alto del conjunto con zócalo: separador + placa 1.6 + zócalo ~8.5 + Teensy: contarlo en el enclosure.

## 4. Encoders EC11

- **Los EC11 del prototipo dan 2 pulsos por detent** (medido 2026-10-19). Si el firmware divide entre 4, solo responde
  uno de cada dos clics. Comprar el mismo modelo o medir el nuevo antes de fijar el divisor.
- El contacto **vibra ±1 pulso en reposo**: contar detents con **histéresis** (2 pulsos completos por detent,
  `firmware-teensy/src/ui/detent.h`), no con `raw >> 1`. Sin eso, el valor sube y baja un paso solo.
- Condensadores de **10 nF** de A y B a GND como antirrebote: opcionales en el Brain, pero recomendados.
- Pulsador del encoder a un pin con `INPUT_PULLUP` (o al MCP23017 en GroovePush) y debounce de ~5 ms.
- Un encoder con un canal suelto da una cuenta que solo oscila 0 ↔ 1: es cableado, no firmware.
- Mecánica (ALPS EC11 con push, L = 25): buje liso Ø6.8, push de 1.5 mm, pines de 3.5 mm. Agujero en aluminio
  Ø7.1 liso; agujero en una PCB de encima Ø8.0 (tolerancia entre placas).

## 5. Conectores entre placas

- **IDC 2×N con pinout 1:1** entre las dos placas: definir la tabla del contrato una vez y generar los dos lados
  desde ella (`set_idc_pinout.py`), así no se cruzan pines.
- Del lado de una Raspberry Pi, **carcasa Dupont** y no IDC: el cuerpo del IDC tapa los pines vecinos del header
  de 40 pines.
- Hilos a teclas o botones de panel: **soldados a la pata**, no Dupont colgando (el Dupont tocaba la placa de abajo).
- Header acodado 1×N cuando el cable sale hacia el costado.

## 6. LEDs WS2812

- Una sola cadena por un pin: más simple que varias (el Brain usa 18 en el pin 28: anillo de 12 + 6 teclas).
- Resistencia serie de ~330 Ω en DIN cerca del primer LED.
- Nivel 3.3 V → 5 V: buffer (74AHCT1G125 o SN74LVC245A, como ya tiene GroovePush) si los LEDs van a 5 V y la cadena
  es larga; condensador grande en +5V cerca de los LEDs.
- La animación de un grupo no debe pisar a otro: dibujar todo el frame y escribir una sola vez
  (`panel_leds` del Brain).

## 7. Audio y ruido

- El ruido de fondo de la protoboard del Brain es casi seguro un lazo de tierra / cableado largo, no firmware: el
  plano de GND de la PCB es parte del diseño. GroovePush no lleva audio, pero los LEDs y los faders comparten tierra
  con el Teensy: plano continuo y retornos cortos.

## 8. Flujo de trabajo que funcionó

1. Esquemático por hoja / placa con el conector como contrato.
2. Colocación por script (`place_*.py`) a partir de coordenadas del panel → el enclosure y la PCB salen de la misma
   tabla.
3. Ruteo (Freerouting `.dsn` → `.ses`) + limpieza (`clean_vias.py`, `add_gnd_pours.py`).
4. DRC con paridad 0/0/0 → `export_fab.py` (gerbers, BOM, CPL) → revisar en GerbView y en el visor 3D de KiCad.
5. Auditoría externa antes de fabricar: verificar cada punto contra los archivos (en el Brain, de 13 "bloqueantes",
   4 eran reales y 5 falsos).
