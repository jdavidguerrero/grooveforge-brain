# Checklist antes de alimentar PCB-M

- [ ] **NO cortar VUSB–VIN de Teensy**: USB desde Pi es la única entrada de +5V_TEENSY; PCB-M no le inyecta tensión. Rev HW B-05 no aplica y su recomendación de corte es incorrecta aquí.
- [ ] **Desoldar R1/R2 de PCB-A fabricada, o no poblarlos**. Pull-ups están en PCB-M por segmento. No conectar PCB-A J4 a teclas antiguas.
- [ ] Confirmar BOM desde esquema y resolver todos los LCSC vacíos del REPORT antes de comprar. F1 en 1812; PCA9517ADP en MSOP8; no usar códigos históricos del README.
- [ ] UART continuidad: Teensy TX29 → J3_M4.4 → CONN1.4 SCL/RX22; RX28 ← J3_M4.3 ← CONN1.3 SDA/TX21; .1 GND y .2 NC. Nunca TX contra TX.
- [ ] Audio Shield: seguir fotos del reverso y tabla README. Dos pads sin rótulo sin cable. Rotular **VGND ≠ GND** en mazo/jack; medir aislamiento del sleeve HP respecto a GND, AGND y chasis.
- [ ] Verificar polaridad TVS (K a +5V_LED) y AO3401A (G=1 GND, S=2 carga, D=3 fusible), F1 junto a entrada y retorno TVS corto. NT1 única unión AGND/GND.
- [ ] EN_K pin0 y EN_A pin1 con pull-ups10k y puentes GPIO separados cerrados. No iniciar Serial1 Teensy. Solo cambiar EN con bus y puerto ociosos; rama retenida en bajo exige procedimiento de fallo, no conmutación arbitraria.
- [ ] Provisionar **ambos MCP4725 0x60/0x61** a EEPROM cero una vez; leer/verificar, hacer power-cycle y medir ambos jacks antes de conectar carga. Firmware pendiente: cero al inicio de setup, no EEPROM por cada arranque.
- [ ] Medir rails/entradas al quitar USB Teensy, USB LED y USB M4 por separado, fusible abierto y SJ_CV abierto; verificar ausencia de alimentación parásita relevante y límites de corriente/tensión. No conectar en caliente hasta superar secuencia.
- [ ] Cupón Choc+zócalo/chapa y LED inverso antes de pedido completo; comprobar cuerpo plástico y salida de luz. FSR real a socket2.54: acomodar cola sin tensión y medir altura, no forzar pliegues.
- [ ] Bringup prioritario: códec con 26LED conectados desde power-on; repetir power/reset, ambos sentidos UART, CV cero, pot/pedal sueltos y ruido de ADS con Sharp. Registrar osciloscopio/corriente/latencia; ERC no sustituye estas pruebas.
