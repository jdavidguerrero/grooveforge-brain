# Consola: pendientes

> Lista viva de trabajo decidido o propuesto que **no** entra ahora. Cada entrada dice de dónde sale, la propuesta y
> la condición para retomarla. Las decisiones abiertas de arquitectura están en la auditoría
> (`auditoria/auditoria-fable-2026-11.md` §8).

## ~~DX7 de 6 operadores como modo adicional~~ → modo implementado; validación física pendiente (ADR-008 §4)

Juan decidió implementarlo ya; queda como registro de la discusión.
Implementación: [modelo y APIs](../theory/dx7-6op.md), [validación reproducible](../theory/dx7-6op-validation.md).
FM 4-OP sigue disponible. Falta la medición Pi/Teensy y la calibración AMD/AMS contra un DX7 físico.

- **Origen:** el diseño de Claude Design mostraba «FM 6-OP»; ADR-005 fijó 4 operadores. Juan preguntó si agregar
  los dos que faltan para acercarlo al DX7.
- **Por qué ADR-005 lo descartó:** no por CPU (estimación 4–5 % de la Teensy con 4 operadores; del orden de 7–8 %
  con 6), sino por la edición: 6 operadores con 5 encoders del Brain Studio. En la consola hay 8 encoders y pantalla
  con diagrama de algoritmo, y si el gemelo corre en el Pi la CPU deja de ser un límite.
- **Propuesta:** mantener **FM 4-OP** (edición simple, también en Studio) y agregar un modo **DX7 6-OP** para
  importar y tocar patches SysEx originales, editables desde la consola y el VST.
- **Lo que de verdad da la fidelidad del DX7**, y es la parte más trabajosa: envolventes de 4 etapas (rate/level)
  por operador, escalado por teclado (level y rate), sensibilidad a la velocity, 32 algoritmos y feedback en el
  operador 6. Sin eso, un SysEx cargado no suena como el original.
- **Afecta:** objeto de audio del DX7 en el firmware, formato GFP, VST, gemelo, páginas de edición (el diagrama de
  algoritmo del brief de Claude Design ya se pidió preparado para crecer de 4 a 6).
- **Referencia de implementación a evaluar:** MSFA (motor FM en el que se basa Dexed). Licencia a verificar (se cree
  Apache 2.0); Dexed es GPL y no conviene mezclarlo.
- **Condición para retomar:** gemelo funcionando en el Pi (benchmark de `12-gemelo-pi.md` aprobado) y E5 cerrado.
  Requiere enmienda a ADR-005 y theory doc antes del código. No es del MVP.

## Chips de la superficie soldados en la PCB por JLCPCB (Juan, 2026-11-10)

- **Decisión:** en la placa principal van los chips soldados por JLC, no módulos. Los módulos (CJMCU-2317, ADS1115 de
  8 pines) son solo para el prototipo de protoboard. No se compran más módulos.
- **Piezas (LCSC, precios unitarios del 2026-11-10):** ADS1115IDGSR **C37593** (VSSOP-10, USD 1,40) ×2 · MCP23017T-E/SS
  **C558584** (SSOP-28, USD 1,72) ×3 + huella del 4.º sin poblar · MCP4725A0T-E/CH **C144198** (SOT-23-6, USD 0,96) ×2
  (0x60/0x61 con su pata A0). Las tres son «extended»: cargo de ~USD 3 por tipo y pedido, a confirmar al cotizar.
- **Direcciones por pista** (ADS 0x48/0x49 con ADDR a GND/3,3 V; MCP 0x20–0x23 con A0–A2) y **un solo par de pull-ups**
  en el bus I2C de la placa.
- **Prototipo actual:** 2 MCP (módulos, puerto A) y 1 ADS1115 (dirección fija 0x48, sin pata ADDR): basta para validar
  el bus compartido con el SGTL5000. El Sharp se prueba en A2 del mismo ADS.
