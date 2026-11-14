# 📚 Documentación PushClone - Raspberry Pi 5

## 🚀 Inicio Rápido

Ejecuta un solo comando para configurar todo:

```bash
./setup_and_deploy_rpi5.sh
```

Esto configura **automáticamente**:
- ✅ Display Freenove 5" DSI (800x480)
- ✅ Qt6 o Qt5 según disponibilidad
- ✅ Aceleración gráfica OpenGL
- ✅ UART para comunicación serial
- ✅ Touchscreen capacitivo
- ✅ Optimizaciones ARM Cortex-A76

---

## 📖 Guías Disponibles

### Guías Principales

| Guía | Descripción |
|------|-------------|
| **[GUIA_RPi5.md](GUIA_RPi5.md)** | Guía completa de configuración RPi 5 + PushClone |
| **[FREENOVE_DISPLAY.md](FREENOVE_DISPLAY.md)** | Display Freenove FNK0078: conexión, config, troubleshooting |
| **[UART_SETUP.md](UART_SETUP.md)** | Configuración UART para comunicación serial con Teensy |

### Scripts de Configuración

| Script | Uso |
|--------|-----|
| **[setup_and_deploy_rpi5.sh](setup_and_deploy_rpi5.sh)** | **Todo-en-uno**: Setup + Deploy (recomendado) |
| **[setup_rpi5.sh](setup_rpi5.sh)** | Solo configuración (se ejecuta en la RPi) |
| **[deploy_rpi5.sh](deploy_rpi5.sh)** | Solo deploy/compilación |
| **[check_rpi5_qt.sh](check_rpi5_qt.sh)** | Verificar capacidades Qt del sistema |
| **[deploy_rpi.sh](deploy_rpi.sh)** | Deploy genérico (Qt5 forzado) |

---

## 🎯 Hardware Soportado

### Raspberry Pi 5
- **Modelo**: Raspberry Pi 5
- **RAM**: 8GB (optimizado para esto)
- **OS**: Raspberry Pi OS Bookworm (recomendado)
- **Qt**: Qt6 (automático) o Qt5 (fallback)

### Display
- **Modelo**: Freenove FNK0078
- **Tamaño**: 5 pulgadas
- **Resolución**: 800x480 IPS
- **Conexión**: MIPI DSI (Puerto CAM/DISP)
- **Touch**: Capacitivo 5 puntos
- **Driver**: Driver-free (plug and play)

### Comunicación Serial
- **Puerto**: UART0 (/dev/ttyAMA0)
- **GPIO**: Pin 8 (TX), Pin 10 (RX), Pin 6 (GND)
- **Baud Rate**: 115200 (recomendado)
- **Conexión**: Teensy 4.1 (RX1/TX1)

---

## 📋 Configuración Aplicada

### 1. Display (DSI)
```ini
# /boot/firmware/config.txt
dtoverlay=vc4-kms-v3d
dtparam=dsi1
gpu_mem=256
```

### 2. UART
```ini
# /boot/firmware/config.txt
enable_uart=1
dtoverlay=disable-bt
```

```bash
# /boot/firmware/cmdline.txt
# Removido: console=serial0,115200
```

### 3. Touchscreen
```bash
# /usr/share/X11/xorg.conf.d/40-libinput.conf
Section "InputClass"
    Identifier "libinput touchscreen catchall"
    MatchIsTouchscreen "on"
    Driver "libinput"
EndSection
```

### 4. Optimizaciones Qt
```bash
# ~/.config/qt_env.sh
export QSG_RENDER_LOOP=basic
export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_FORCEVSYNC=1
```

---

## 🔧 Comandos Útiles

### Verificar configuración

```bash
# Ver modelo de RPi
cat /proc/device-tree/model

# Ver Qt instalado
qmake --version  # Qt5
qmake6 --version # Qt6

# Ver puerto serial
ls -l /dev/ttyAMA0

# Ver permisos de usuario
groups

# Ver config.txt
cat /boot/firmware/config.txt | grep -E "uart|dsi|gpu_mem"
```

### Compilar y ejecutar

```bash
# Deploy completo desde Mac
./deploy_rpi5.sh

# Compilar manualmente en RPi
cd ~/PushClone/build
cmake -DUSE_QT6=OFF -DCMAKE_BUILD_TYPE=Release ..
make -j8
./appPushClone

# Ejecutar desde Mac (remoto)
ssh pi@raspberrypi.local 'cd PushClone/build && ./appPushClone'
```

### Debug

```bash
# Ver errores de Qt
QT_DEBUG_PLUGINS=1 ./appPushClone

# Monitorear puerto serial
cat /dev/ttyAMA0

# Test touchscreen
sudo evtest
```

---

## 📊 Performance Esperado

| Métrica | Valor |
|---------|-------|
| **FPS** | 60 constantes |
| **Latencia touch** | <10ms |
| **Uso RAM** | ~500-700MB |
| **Tiempo compilación** | 1-2 minutos |
| **Tiempo carga** | <1 segundo |
| **Baud rate serial** | 115200 bps |

---

## 🗺️ Estructura del Proyecto

```
PushClone/
├── Main.qml                  # Ventana principal
├── SplashScreen.qml          # Pantalla de carga
├── PushCloneTheme.qml        # Sistema de colores/estilos
├── components/
│   ├── ClipPad.qml          # Pad individual de clip
│   ├── NavigationBar.qml    # Barra de navegación
│   ├── TransportBar.qml     # Barra de transporte
│   └── MixChannelStrip.qml  # Canal de mezcla
├── views/
│   ├── SessionView.qml      # Vista de sesión (grid 8x4)
│   └── MixView.qml          # Vista de mezcla
├── SerialController.cpp/h    # Control de comunicación serial
├── ClipGridModel.cpp/h       # Modelo de clips
├── TrackListModel.cpp/h      # Modelo de tracks
├── SceneListModel.cpp/h      # Modelo de escenas
├── main.cpp                  # Punto de entrada
├── CMakeLists.txt            # Configuración CMake
├── resources.qrc             # Recursos Qt (Qt5)
└── qmldir                    # Módulo QML
```

---

## 🎯 Próximos Pasos

### Después de ejecutar setup_and_deploy_rpi5.sh:

1. **La RPi se reiniciará**
2. **Espera 1-2 minutos** después del reinicio
3. **Ejecuta el deploy:**
   ```bash
   ./deploy_rpi5.sh
   ```
4. **La aplicación compilará** (~1-2 min)
5. **Ejecuta PushClone:**
   ```bash
   ssh pi@raspberrypi.local 'cd PushClone/build && ./appPushClone'
   ```

---

## 🆘 Problemas Comunes

| Problema | Solución Rápida | Guía Detallada |
|----------|-----------------|----------------|
| Display en blanco | Verificar cable DSI en CAM/DISP | [FREENOVE_DISPLAY.md](FREENOVE_DISPLAY.md) |
| Touch no responde | `sudo apt-get install xserver-xorg-input-evdev` | [FREENOVE_DISPLAY.md](FREENOVE_DISPLAY.md#troubleshooting) |
| Puerto serial no funciona | `sudo usermod -a -G dialout $USER` | [UART_SETUP.md](UART_SETUP.md#troubleshooting) |
| Qt no encontrado | Ejecutar `setup_rpi5.sh` | [GUIA_RPi5.md](GUIA_RPi5.md) |
| Error de compilación | Verificar que Qt5/Qt6 esté instalado | [GUIA_RPi5.md](GUIA_RPi5.md#problemas-comunes) |

---

## 📞 Recursos

- **Freenove**: https://github.com/Freenove/Freenove_Touchscreen_Monitor_for_Raspberry_Pi
- **Raspberry Pi Docs**: https://www.raspberrypi.com/documentation/
- **Qt5 Docs**: https://doc.qt.io/qt-5/
- **Qt6 Docs**: https://doc.qt.io/qt-6/

---

## ✨ Features Implementadas

- ✅ **Display 800x480** pantalla completa
- ✅ **Touchscreen** capacitivo 5 puntos
- ✅ **UART/Serial** para comunicación con Teensy
- ✅ **SessionView** grid 8x4 de clips
- ✅ **MixView** mezclador de audio
- ✅ **Transport** controles play/stop/record
- ✅ **Antialiasing** suavizado de bordes
- ✅ **Animaciones** fluidas 60 FPS
- ✅ **Aceleración OpenGL** para renderizado
- ✅ **Optimizaciones ARM** Cortex-A76

---

**¡Tu PushClone está listo para funcionar! 🎉**

Para empezar, ejecuta:
```bash
./setup_and_deploy_rpi5.sh
```
