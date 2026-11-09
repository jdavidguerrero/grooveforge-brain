# 🔌 Configuración UART - Raspberry Pi 5

## ¿Qué es UART?

UART (Universal Asynchronous Receiver-Transmitter) es el puerto de comunicación serial que PushClone usa para comunicarse con el Teensy y otros dispositivos externos.

---

## 🎯 Configuración Automática (Recomendada)

El script `setup_rpi5.sh` ya configura UART automáticamente. Solo ejecuta:

```bash
./setup_and_deploy_rpi5.sh
```

Esto configurará:
- ✅ UART habilitado en hardware
- ✅ Console serial deshabilitado
- ✅ Bluetooth deshabilitado (para liberar UART0)
- ✅ Permisos de usuario correctos

---

## ⚙️ Configuración Manual

Si prefieres hacerlo manualmente:

### 1. Habilitar UART en config.txt

```bash
sudo nano /boot/firmware/config.txt
```

Agregar al final:
```ini
# Habilitar UART para comunicación serial
enable_uart=1

# Deshabilitar Bluetooth para liberar UART0 (opcional)
dtoverlay=disable-bt
```

**Nota sobre Bluetooth:**
- Si deshabilitas BT, UART0 estará disponible en GPIO 14/15
- Si mantienes BT, usa UART5 en otros GPIO (más complejo)

### 2. Deshabilitar Console Serial

Por defecto, Raspberry Pi usa UART para login console. Necesitas deshabilitarlo:

```bash
sudo nano /boot/firmware/cmdline.txt
```

**Remover** estas partes (si existen):
```
console=serial0,115200
console=ttyAMA0,115200
```

**Antes:**
```
console=serial0,115200 console=tty1 root=PARTUUID=... rootfstype=ext4 ...
```

**Después:**
```
console=tty1 root=PARTUUID=... rootfstype=ext4 ...
```

### 3. Deshabilitar servicio getty

```bash
sudo systemctl disable serial-getty@ttyAMA0.service
sudo systemctl disable serial-getty@serial0.service
```

### 4. Permisos de usuario

```bash
sudo usermod -a -G dialout $USER
```

### 5. Reiniciar

```bash
sudo reboot
```

---

## 📍 Pines GPIO en Raspberry Pi 5

### UART0 (Principal - recomendado)

| Pin Físico | GPIO | Función | Conexión |
|------------|------|---------|----------|
| **8** | GPIO 14 | TXD (TX) | → RX del Teensy |
| **10** | GPIO 15 | RXD (RX) | ← TX del Teensy |
| **6** | GND | Ground | ⏚ GND común |

```
Raspberry Pi GPIO Header (vista superior):
   3V3  (1)  (2)  5V
 GPIO2  (3)  (4)  5V
 GPIO3  (5)  (6)  GND  ← GND
 GPIO4  (7)  (8)  GPIO14 (TXD) ← TX
   GND  (9) (10)  GPIO15 (RXD) ← RX
```

### Conexión con Teensy

```
Raspberry Pi 5          Teensy 4.1
━━━━━━━━━━━━━━━        ━━━━━━━━━━━━
Pin 8 (GPIO14 TX) ───→  RX1 (Pin 0)
Pin 10 (GPIO15 RX) ←───  TX1 (Pin 1)
Pin 6 (GND) ────────────  GND
```

**⚠️ IMPORTANTE:**
- **TX** de RPi va a **RX** de Teensy
- **RX** de RPi va a **TX** de Teensy
- **GND común** entre ambos dispositivos
- **NO conectes 5V** - UART solo necesita señales TX/RX/GND
- Los niveles de voltaje son compatibles (3.3V)

---

## ✅ Verificar que UART funciona

### 1. Verificar que el puerto existe:

```bash
ls -l /dev/ttyAMA0
# Debería mostrar: crw-rw---- 1 root dialout ... /dev/ttyAMA0
```

Si no existe, verifica:
```bash
ls -l /dev/tty*
# Busca: ttyAMA0, ttyS0, serial0
```

### 2. Verificar permisos de usuario:

```bash
groups
# Debe incluir: dialout
```

Si no está, ejecuta:
```bash
sudo usermod -a -G dialout $USER
# Luego logout y login de nuevo
```

### 3. Test de loopback (opcional):

Conecta **TX con RX** físicamente (pin 8 con pin 10) y ejecuta:

```bash
# Terminal 1: Escuchar
cat /dev/ttyAMA0

# Terminal 2: Enviar
echo "test" > /dev/ttyAMA0
```

Deberías ver "test" en la Terminal 1.

**⚠️ Desconecta el loopback después del test!**

### 4. Monitorear puerto serial:

```bash
# Instalar minicom
sudo apt-get install minicom

# Configurar y abrir
sudo minicom -b 115200 -o -D /dev/ttyAMA0

# Salir: Ctrl+A, luego X
```

---

## 🔧 Troubleshooting

### Problema: /dev/ttyAMA0 no existe

**Solución:**
1. Verifica config.txt:
   ```bash
   cat /boot/firmware/config.txt | grep enable_uart
   # Debe mostrar: enable_uart=1
   ```

2. Si no está, agrégalo:
   ```bash
   echo "enable_uart=1" | sudo tee -a /boot/firmware/config.txt
   sudo reboot
   ```

### Problema: "Permission denied" al abrir puerto

**Solución:**
```bash
# Verificar que estás en el grupo dialout
groups

# Si no aparece, agregarte:
sudo usermod -a -G dialout $USER

# IMPORTANTE: Logout y login de nuevo
exit
# Vuelve a conectarte por SSH
```

### Problema: Bluetooth interfiere con UART

**Solución:**
Deshabilita Bluetooth permanentemente:
```bash
sudo nano /boot/firmware/config.txt
```

Agregar:
```ini
dtoverlay=disable-bt
```

Reiniciar:
```bash
sudo reboot
```

### Problema: Console serial interfiere

**Síntomas:**
- Ves mensajes de kernel en el puerto serial
- Login prompts aparecen

**Solución:**
```bash
# Verificar cmdline.txt
cat /boot/firmware/cmdline.txt

# No debe contener: console=serial0 o console=ttyAMA0

# Si está, editar:
sudo nano /boot/firmware/cmdline.txt
# Remover console=serial0,115200 y similares

# Deshabilitar getty
sudo systemctl disable serial-getty@ttyAMA0.service
sudo systemctl disable serial-getty@serial0.service

sudo reboot
```

---

## 📊 Configuración en SerialController

En tu código C++ (SerialController.cpp), la configuración típica es:

```cpp
QSerialPort *serialPort = new QSerialPort();
serialPort->setPortName("/dev/ttyAMA0");
serialPort->setBaudRate(QSerialPort::Baud115200);
serialPort->setDataBits(QSerialPort::Data8);
serialPort->setParity(QSerialPort::NoParity);
serialPort->setStopBits(QSerialPort::OneStop);
serialPort->setFlowControl(QSerialPort::NoFlowControl);

if (serialPort->open(QIODevice::ReadWrite)) {
    qDebug() << "Puerto serial abierto correctamente";
} else {
    qDebug() << "Error:" << serialPort->errorString();
}
```

---

## 🎯 Velocidades comunes (Baud Rate)

| Baud Rate | Uso típico |
|-----------|------------|
| 9600 | Debug, comunicación simple |
| 19200 | Comunicación moderada |
| 38400 | Comunicación rápida |
| **115200** | **Recomendado para PushClone** ✅ |
| 230400 | Muy rápido (puede tener errores) |
| 460800 | Ultra rápido (no siempre estable) |

**Recomendación:** Usa **115200** para PushClone. Es el balance perfecto entre velocidad y estabilidad.

---

## 🔍 Debug y Monitoreo

### Ver mensajes del puerto en tiempo real:

```bash
# Opción 1: cat (simple)
cat /dev/ttyAMA0

# Opción 2: stty + cat (configurable)
stty -F /dev/ttyAMA0 115200
cat /dev/ttyAMA0

# Opción 3: minicom (interactivo)
sudo minicom -b 115200 -D /dev/ttyAMA0
```

### Enviar comandos de prueba:

```bash
# Configurar velocidad
stty -F /dev/ttyAMA0 115200

# Enviar texto
echo "HELLO" > /dev/ttyAMA0

# Enviar bytes hex (con xxd)
echo -ne '\x01\x02\x03' > /dev/ttyAMA0
```

---

## 📝 Configuración del Teensy

En el lado del Teensy 4.1, configura Serial1:

```cpp
void setup() {
    Serial1.begin(115200);  // Mismo baud rate que RPi
}

void loop() {
    if (Serial1.available()) {
        char c = Serial1.read();
        // Procesar datos de RPi
    }

    // Enviar a RPi
    Serial1.println("DATA");
}
```

**Pines en Teensy 4.1:**
- **RX1**: Pin 0 (← conecta a TX de RPi)
- **TX1**: Pin 1 (→ conecta a RX de RPi)
- **GND**: Cualquier pin GND

---

## ✨ Resumen

| Configuración | Valor |
|---------------|-------|
| **Puerto** | `/dev/ttyAMA0` |
| **Baud Rate** | `115200` |
| **Data Bits** | `8` |
| **Parity** | `None` |
| **Stop Bits** | `1` |
| **Flow Control** | `None` |
| **GPIO TX** | Pin 8 (GPIO 14) |
| **GPIO RX** | Pin 10 (GPIO 15) |
| **GND** | Pin 6 |

---

¡Tu UART está listo para comunicación con el Teensy! 🎉
