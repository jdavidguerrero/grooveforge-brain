# 🚀 Guía de Configuración Raspberry Pi 5 - PushClone

## 📱 Hardware Detectado
- **Display**: Freenove FNK0078 - 5" DSI Touchscreen
- **Resolución**: 800x480 IPS
- **Conexión**: MIPI DSI (Puerto CAM/DISP en RPi 5)
- **Touch**: Capacitivo 5 puntos
- **RPi**: Raspberry Pi 5 (8GB RAM)

---

## Opción 1: Configuración Automática (Recomendada)

Desde tu **Mac**, ejecuta:

```bash
./setup_and_deploy_rpi5.sh
```

Esto hará:
1. ✅ Copiar script de configuración a tu RPi
2. ✅ Ejecutar configuración completa automática
3. ✅ Instalar todas las dependencias
4. ✅ Configurar aceleración gráfica
5. ✅ Optimizar rendimiento

**Importante:** Al final te pedirá reiniciar la RPi. Di que **SÍ**.

---

## Opción 2: Configuración Manual

### Paso 1: Copiar script a la RPi

Desde tu **Mac**:
```bash
scp setup_rpi5.sh pi@raspberrypi.local:~/
```

### Paso 2: Conectarte a la RPi

```bash
ssh pi@raspberrypi.local
```

### Paso 3: Ejecutar configuración

En la **Raspberry Pi**:
```bash
bash ~/setup_rpi5.sh
```

Sigue las instrucciones en pantalla:
- Te preguntará qué tipo de display tienes (800x480)
- Si quieres autoarranque
- Al final, **reinicia la RPi**

---

## Después del Reinicio

### Opción A: Deploy desde tu Mac (Recomendado)

```bash
./deploy_rpi5.sh
```

Este script:
- 📦 Empaqueta el proyecto
- 📤 Lo sube a la RPi
- 🔍 Detecta automáticamente Qt6 o Qt5
- 🔧 Compila con optimizaciones ARM
- ✅ Verifica que todo funcionó

### Opción B: Compilar manualmente en la RPi

Conectado a la RPi vía SSH:

```bash
cd ~/PushClone/build

# Con Qt6 (si está disponible)
cmake -DUSE_QT6=ON -DCMAKE_BUILD_TYPE=Release ..
make -j8

# O con Qt5
cmake -DUSE_QT6=OFF -DCMAKE_BUILD_TYPE=Release ..
make -j8
```

---

## Ejecutar la Aplicación

### Desde la RPi (local):

```bash
cd ~/PushClone/build
./appPushClone
```

### Desde tu Mac (remoto):

```bash
ssh pi@raspberrypi.local 'cd PushClone/build && ./appPushClone'
```

---

## ¿Qué Hace el Script de Configuración?

### 1. **Actualiza el sistema**
```bash
sudo apt-get update && sudo apt-get upgrade -y
```

### 2. **Instala drivers gráficos OpenGL**
- Mesa drivers
- OpenGL ES 2.0
- Aceleración por hardware

### 3. **Instala Qt6 o Qt5**
- **Bookworm (Debian 12)**: Qt6 automáticamente
- **Sistemas antiguos**: Qt5 como fallback

### 4. **Configura aceleración GPU**
Modifica `/boot/firmware/config.txt`:
```ini
dtoverlay=vc4-kms-v3d
gpu_mem=256
```

### 5. **Configura display 800x480**
Para displays HDMI Waveshare u otros compatibles

### 6. **Optimiza variables de entorno Qt**
Crea `~/.config/qt_env.sh`:
```bash
export QSG_RENDER_LOOP=basic
export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_FORCEVSYNC=1
```

### 7. **Configuración UART (Puerto Serial)**

**UART habilitado** en `/boot/firmware/config.txt`:
```bash
enable_uart=1
dtoverlay=disable-bt  # Libera UART0 (GPIO 14/15)
```

**Console serial deshabilitado** en `/boot/firmware/cmdline.txt`:
- Removido `console=serial0,115200`

**Servicios deshabilitados:**
```bash
sudo systemctl disable serial-getty@ttyAMA0.service
```

**Permisos de usuario:**
```bash
sudo usermod -a -G dialout pi
```

**Puerto disponible:** `/dev/ttyAMA0` (GPIO 14=TX, GPIO 15=RX)

📖 Ver guía completa: [UART_SETUP.md](UART_SETUP.md)

### 8. **Autoarranque (opcional)**
Crea archivo `.desktop` para arranque automático en `~/.config/autostart/`

---

## Verificar que Todo Funciona

### 1. Verificar OpenGL:
```bash
glxinfo | grep "OpenGL version"
```

Debería mostrar OpenGL ES 2.0 o superior

### 2. Verificar Qt:
```bash
# Qt6
qmake6 --version

# Qt5
qmake --version
```

### 3. Verificar puerto serial:
```bash
ls -l /dev/ttyUSB* /dev/ttyACM*
groups  # Deberías ver 'dialout' en la lista
```

---

## Problemas Comunes

### La aplicación no arranca
```bash
# Ver errores detallados:
cd ~/PushClone/build
QT_DEBUG_PLUGINS=1 ./appPushClone
```

### Display no se ve bien (resolución incorrecta)

**Para Freenove FNK0078 (DSI):**

1. **Verificar conexión física:**
   - Display conectado al puerto **DSI-1** (marcado como CAM/DISP en RPi 5)
   - Cable FPC bien conectado en ambos extremos
   - Latch del conector bien cerrado

2. **Verificar config.txt:**
```bash
cat /boot/firmware/config.txt | grep -E "dsi|dtoverlay=vc4"
```

Debería mostrar:
```
dtoverlay=vc4-kms-v3d
dtparam=dsi1
```

3. **Si no aparece la imagen:**
```bash
# Editar config.txt
sudo nano /boot/firmware/config.txt

# Agregar estas líneas si no existen:
dtoverlay=vc4-kms-v3d
dtparam=dsi1

# Guardar (Ctrl+O, Enter, Ctrl+X) y reiniciar
sudo reboot
```

4. **Para display HDMI en lugar del DSI** (temporal):
Si quieres usar un monitor HDMI en lugar del Freenove:
```bash
# Comentar las líneas DSI en config.txt
sudo nano /boot/firmware/config.txt
# Agregar # delante de: dtparam=dsi1
```

### Sin aceleración gráfica
Verificar `/boot/firmware/config.txt`:
```bash
cat /boot/firmware/config.txt | grep vc4-kms-v3d
```
Debería mostrar: `dtoverlay=vc4-kms-v3d`

### Puerto serial no funciona

**1. Verificar que el puerto existe:**
```bash
ls -l /dev/ttyAMA0
# Debe existir y tener grupo 'dialout'
```

**2. Verificar permisos:**
```bash
groups
# Si 'dialout' no aparece, ejecutar:
sudo usermod -a -G dialout $USER
# Luego logout y login de nuevo
```

**3. Verificar que UART está habilitado:**
```bash
cat /boot/firmware/config.txt | grep enable_uart
# Debe mostrar: enable_uart=1
```

**4. Test de loopback (conecta pin 8 con pin 10):**
```bash
# Terminal 1
cat /dev/ttyAMA0

# Terminal 2
echo "test" > /dev/ttyAMA0

# Deberías ver "test" en Terminal 1
```

📖 **Guía completa de troubleshooting:** [UART_SETUP.md](UART_SETUP.md)

### Touchscreen no responde

**Para Freenove FNK0078:**

1. **Verificar que el driver esté instalado:**
```bash
sudo apt-get install -y xserver-xorg-input-evdev
```

2. **Verificar que se detecta el touch:**
```bash
ls /dev/input/event*
# Deberías ver varios dispositivos event0, event1, etc.

# Ver eventos del touch en tiempo real
sudo evtest
# Selecciona el dispositivo touchscreen y toca la pantalla
```

3. **Si el touch está rotado:**
```bash
# Crear archivo de calibración
sudo nano /usr/share/X11/xorg.conf.d/40-libinput.conf
```

Agregar (para rotar touch 180°):
```
Section "InputClass"
    Identifier "libinput touchscreen catchall"
    MatchIsTouchscreen "on"
    MatchDevicePath "/dev/input/event*"
    Driver "libinput"
    Option "TransformationMatrix" "-1 0 1 0 -1 1 0 0 1"
EndSection
```

Transformaciones comunes:
- 90° CW: `"0 -1 1 1 0 0 0 0 1"`
- 180°: `"-1 0 1 0 -1 1 0 0 1"`
- 270° CW: `"0 1 0 -1 0 1 0 0 1"`

4. **Reiniciar X server:**
```bash
sudo systemctl restart lightdm
```

---

## Optimizaciones Aplicadas

### Hardware (RPi 5):
- ✅ CPU: ARM Cortex-A76 optimizado (`-mtune=cortex-a76`)
- ✅ Compilación: `-O3` nivel máximo de optimización
- ✅ SIMD: Instrucciones ARM v8-A + CRC
- ✅ GPU: 256MB dedicados
- ✅ OpenGL: Aceleración por hardware

### Software:
- ✅ Qt6 (si está disponible) o Qt5
- ✅ Renderizado: OpenGL ES 2.0
- ✅ Vsync: Habilitado (sin tearing)
- ✅ Render loop: `basic` (estable para embedded)
- ✅ Antialiasing: Habilitado en componentes

### Rendimiento Esperado:
- 🚀 **60 FPS** en animaciones
- ⚡ **<1 segundo** tiempo de carga
- 🎨 **Gráficos suaves** con antialiasing
- 📊 **~500-700MB RAM** usados (de 8GB disponibles)

---

## Contacto y Soporte

Si tienes problemas:
1. Verifica logs con `QT_DEBUG_PLUGINS=1`
2. Revisa que el display esté en 800x480
3. Confirma que OpenGL esté funcionando
4. Asegúrate de haber reiniciado después del setup

---

**¡Tu Raspberry Pi 5 está lista para PushClone! 🎉**
