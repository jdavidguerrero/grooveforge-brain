# Tests de Hardware - Teensy 4.1

Este directorio contiene tests individuales para cada periférico antes de integrarlos al proyecto principal.

## 🎯 Filosofía de Tests

**Probar cada componente de forma aislada antes de integrarlo** al sistema completo. Esto facilita:
- Verificar que el hardware funciona correctamente
- Detectar problemas de cableado
- Entender el comportamiento de cada sensor/actuador
- Tener código de referencia limpio y simple

---

## 📋 Tests Disponibles

### 1. **test_faders.cpp** - Faders ALPS B50K
Prueba 4 faders analógicos en pines A0-A3

**Hardware:**
- 4x Faders ALPS B50K (50kΩ lineales)
- Conexión: A0, A1, A2, A3

**Compilar y ejecutar:**
```bash
pio run -e test_faders_teensy -t upload && pio device monitor
```

**Qué verás:**
- Valores ADC (0-4095)
- Porcentaje (0-100%)
- Valor MIDI (0-127)
- Barra visual ASCII
- Resumen cada 2 segundos

---

### 2. **test_encoders.cpp** - Encoders Rotatorios
Prueba 4 encoders incrementales

**Hardware:**
- 4x Encoders rotatorios con salida cuadratura
- Encoder 1: pines 2-3
- Encoder 2: pines 4-5
- Encoder 3: pines 6-7
- Encoder 4: pines 10-11

**Compilar y ejecutar:**
```bash
pio run -e test_encoders_teensy -t upload && pio device monitor
```

**Qué verás:**
- Posición absoluta de cada encoder
- Dirección de giro (CW/CCW)
- Incremento/decremento
- Contador acumulado

---

### 3. **test_mcp_encoder_buttons.cpp** - Botones de Encoders
Prueba MCP23017 #1 con 4 botones de encoders

**Hardware:**
- 1x MCP23017 @ dirección I2C 0x20
- 4 botones conectados a GPA0-GPA3
- Pull-ups internos habilitados

**Compilar y ejecutar:**
```bash
pio run -e test_mcp_encoder_buttons_teensy -t upload && pio device monitor
```

**Qué verás:**
- Estado de cada botón (PRESSED/RELEASED)
- Tiempo de pulsación
- Debouncing automático

---

### 4. **test_mcp_extra_buttons.cpp** - Botones Extra
Prueba MCP23017 #2 con 8 botones adicionales

**Hardware:**
- 1x MCP23017 @ dirección I2C 0x21
- 8 botones (Play, Stop, Record, etc.) en GPA0-GPA7
- Pull-ups internos habilitados

**Compilar y ejecutar:**
```bash
pio run -e test_mcp_extra_buttons_teensy -t upload && pio device monitor
```

**Qué verás:**
- Estado de los 8 botones con nombres
- Eventos de press/release
- Duración de pulsación
- Debouncing automático (50ms)

**Conexión MCP23017 #2:**
```
Pin 21 (GPA0) → Botón 1 (Play)
Pin 22 (GPA1) → Botón 2 (Stop)
Pin 23 (GPA2) → Botón 3 (Record)
Pin 24 (GPA3) → Botón 4 (Loop)
Pin 25 (GPA4) → Botón 5
Pin 26 (GPA5) → Botón 6
Pin 27 (GPA6) → Botón 7
Pin 28 (GPA7) → Botón 8

Pin 15 (A0)   → 3.3V  (dirección = 0x21)
Pin 16 (A1)   → GND
Pin 17 (A2)   → GND
Pin 18 (RESET)→ 3.3V (+ pull-up 10kΩ)
Pin 9  (VDD)  → 3.3V
Pin 10 (VSS)  → GND
Pin 12 (SCL)  → Pin 19 (Teensy)
Pin 13 (SDA)  → Pin 18 (Teensy)
```

---

## 🔧 Conexiones Teensy 4.1

### Pines Analógicos (Faders)
```
A0 (Pin 14) → Fader 1
A1 (Pin 15) → Fader 2
A2 (Pin 16) → Fader 3
A3 (Pin 17) → Fader 4
```

### Pines Digitales (Encoders)
```
Pin 2-3   → Encoder 1 (A, B)
Pin 4-5   → Encoder 2 (A, B)
Pin 6-7   → Encoder 3 (A, B)
Pin 10-11 → Encoder 4 (A, B)
```

### I2C (MCP23017)
```
SDA (Pin 18) → MCP #1 & #2
SCL (Pin 19) → MCP #1 & #2

MCP #1: 0x20 (botones encoders)
MCP #2: 0x21 (botones extra)
```

---

## 📊 Workflow Recomendado

1. **Conectar hardware** según el diagrama
2. **Probar cada componente** individualmente
3. **Anotar resultados:**
   - ¿Funcionan todos los faders?
   - ¿Algún encoder está invertido?
   - ¿Responden todos los botones?
4. **Documentar issues** antes de integrar
5. **Integrar al proyecto principal** una vez verificados

---

## 🐛 Troubleshooting Común

### Faders
- **Valores erráticos:** Revisar conexiones/soldaduras
- **Invertidos:** Intercambiar GND y 3.3V
- **Siempre 0 o 4095:** Fader dañado

### Encoders
- **No detecta giros:** Revisar pines A/B
- **Dirección invertida:** Intercambiar A y B
- **Saltos erráticos:** Agregar capacitores 0.1µF

### MCP23017
- **No detectado:** Verificar dirección I2C con `i2cdetect`
- **Botones siempre HIGH:** Revisar pull-ups
- **I2C colgado:** Revisar resistencias pull-up en SDA/SCL (4.7kΩ)

---

## 💡 Tips

- Usa `pio device list` para ver puertos disponibles
- Monitor serial siempre a **115200 baud**
- Los tests NO interfieren con el proyecto principal
- Puedes tener múltiples archivos `.cpp` en `src/test/`

---

## ✅ Checklist de Verificación

Antes de integrar al proyecto principal:

- [ ] Los 4 faders responden correctamente
- [ ] Los 4 encoders giran en ambas direcciones
- [ ] Los 4 botones de encoders responden
- [ ] Los 8 botones extra responden
- [ ] No hay conflictos de pines
- [ ] Todos los valores son estables (sin ruido)

---

**Siguiente paso:** Una vez todos los tests pasen, proceder con la integración al proyecto principal.
