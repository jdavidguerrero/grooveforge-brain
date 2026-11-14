# 📱 Guía Display Freenove FNK0078 - Raspberry Pi 5

## Especificaciones
- **Modelo**: Freenove FNK0078
- **Tamaño**: 5 pulgadas
- **Resolución**: 800x480 pixels
- **Tecnología**: IPS LCD
- **Conexión**: MIPI DSI (no HDMI)
- **Touch**: Capacitivo 5 puntos
- **Driver**: Driver-free (plug and play en Raspberry Pi OS)

---

## 🔌 Conexión Física (IMPORTANTE)

### En Raspberry Pi 5:

1. **Apaga completamente** la Raspberry Pi
   ```bash
   sudo shutdown -h now
   ```
   Espera a que se apaguen todas las luces

2. **Localiza el puerto DSI:**
   - En RPi 5, el puerto está marcado como **"CAM/DISP"**
   - Está al lado opuesto del puerto USB-C de alimentación
   - Es un conector FPC de 22 pines

3. **Conecta el cable FPC:**
   - **Levanta** el latch del conector (jala suavemente hacia arriba)
   - **Inserta** el cable FPC del display:
     - Contactos dorados hacia **ABAJO** (hacia el PCB)
     - Cable recto, sin torcer
   - **Cierra** el latch firmemente (presiona hacia abajo)

   ⚠️ **MUY IMPORTANTE**: Los contactos dorados del cable deben mirar hacia el PCB de la Raspberry Pi

4. **Conecta la alimentación del display:**
   - El Freenove FNK0078 se alimenta del puerto GPIO
   - Ya viene con cable pre-conectado en el display
   - Conecta los pines GPIO según el manual (normalmente 5V y GND)

5. **Enciende la Raspberry Pi**

---

## ⚙️ Configuración de Software

### Configuración Automática (Recomendada)

El script `setup_rpi5.sh` ya está configurado para el Freenove:

```bash
# Desde tu Mac
./setup_and_deploy_rpi5.sh
```

Esto configurará:
- ✅ Drivers DSI
- ✅ Overlay vc4-kms-v3d
- ✅ Touchscreen capacitivo
- ✅ Resolución 800x480

### Configuración Manual

Si prefieres configurar manualmente:

1. **Editar config.txt:**
   ```bash
   sudo nano /boot/firmware/config.txt
   ```

2. **Agregar estas líneas:**
   ```ini
   # Freenove 5" DSI Display (800x480)
   dtoverlay=vc4-kms-v3d
   dtparam=dsi1
   gpu_mem=256
   ```

3. **Guardar y reiniciar:**
   ```bash
   sudo reboot
   ```

---

## 🖱️ Configuración del Touchscreen

### Instalar drivers de touch:

```bash
sudo apt-get update
sudo apt-get install -y xserver-xorg-input-evdev
```

### Crear archivo de configuración:

```bash
sudo mkdir -p /usr/share/X11/xorg.conf.d
sudo nano /usr/share/X11/xorg.conf.d/40-libinput.conf
```

Agregar:
```
Section "InputClass"
    Identifier "libinput touchscreen catchall"
    MatchIsTouchscreen "on"
    MatchDevicePath "/dev/input/event*"
    Driver "libinput"
EndSection
```

### Verificar que funciona:

```bash
# Ver eventos táctiles en tiempo real
sudo apt-get install evtest
sudo evtest

# Selecciona el dispositivo touchscreen
# Toca la pantalla y deberías ver eventos
```

---

## 🔄 Rotar Display y Touch

### Rotar solo la pantalla (no el touch):

Editar config.txt:
```bash
sudo nano /boot/firmware/config.txt
```

Agregar:
```ini
# Rotar 180 grados
display_rotate=2

# Otras opciones:
# display_rotate=0  # Normal
# display_rotate=1  # 90° CW
# display_rotate=2  # 180°
# display_rotate=3  # 270° CW
```

### Rotar pantalla Y touchscreen juntos:

Para PushClone (que usa Qt y corre sin X11):

En `main.cpp`, ya está configurado para pantalla completa. Si necesitas rotar:

```cpp
// En main.cpp, antes de engine.load():
qputenv("QT_QPA_EGLFS_ROTATION", "180");  // 0, 90, 180, 270
```

O mediante variable de entorno al ejecutar:
```bash
QT_QPA_EGLFS_ROTATION=180 ./appPushClone
```

---

## ✅ Verificación de Funcionamiento

### 1. Verificar que el display es detectado:

```bash
# Ver dispositivos de display
ls /dev/fb*
# Debería mostrar /dev/fb0

# Información del display
fbset -fb /dev/fb0
# Debería mostrar: 800x480
```

### 2. Verificar touchscreen:

```bash
ls /dev/input/event*
# Debería listar varios dispositivos

# Ver cuál es el touchscreen
cat /proc/bus/input/devices | grep -A 5 "Touchscreen"
```

### 3. Test de imagen:

```bash
# Instalar fbi (framebuffer image viewer)
sudo apt-get install fbi

# Mostrar una imagen de prueba
sudo fbi -T 1 -d /dev/fb0 -noverbose -a /usr/share/pixmaps/debian-logo.png
```

---

## 🔧 Troubleshooting

### Problema: Pantalla en blanco

**Causas posibles:**

1. **Cable FPC mal conectado:**
   - Apaga la RPi
   - Reconecta el cable FPC
   - Asegúrate de que los contactos miren hacia abajo

2. **Overlay incorrecto:**
   ```bash
   # Verificar config.txt
   cat /boot/firmware/config.txt | grep -E "dsi|dtoverlay"

   # Debe tener:
   # dtoverlay=vc4-kms-v3d
   # dtparam=dsi1
   ```

3. **Puerto incorrecto:**
   - En RPi 5, debe estar en **DSI-1** (CAM/DISP)
   - NO en el puerto de cámara DSI-0

### Problema: Display funciona pero touchscreen no

```bash
# Instalar driver
sudo apt-get install -y xserver-xorg-input-evdev

# Reiniciar sistema
sudo reboot
```

### Problema: Touch detecta pero coordenadas invertidas

Editar `/usr/share/X11/xorg.conf.d/40-libinput.conf`:

```
Section "InputClass"
    Identifier "libinput touchscreen catchall"
    MatchIsTouchscreen "on"
    MatchDevicePath "/dev/input/event*"
    Driver "libinput"
    Option "TransformationMatrix" "-1 0 1 0 -1 1 0 0 1"
EndSection
```

Reiniciar:
```bash
sudo systemctl restart lightdm
```

---

## 📊 Rendimiento Óptimo

Para PushClone en RPi 5 con este display:

### Configuración recomendada en config.txt:

```ini
# GPU Memory (256MB para gráficos fluidos)
gpu_mem=256

# Display DSI
dtoverlay=vc4-kms-v3d
dtparam=dsi1

# Audio (si lo necesitas)
dtparam=audio=on

# Overclock conservador (opcional, mejora fluidez)
# over_voltage=2
# arm_freq=2400
```

### Variables de entorno Qt (PushClone):

El script de setup ya crea `~/.config/qt_env.sh` con:

```bash
export QSG_RENDER_LOOP=basic
export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_FORCEVSYNC=1
```

---

## 🎯 Performance Esperado

Con RPi 5 + Freenove FNK0078:

| Métrica | Valor |
|---------|-------|
| **Resolución nativa** | 800x480 @60Hz |
| **FPS en PushClone** | 60 constantes |
| **Touch latency** | <10ms |
| **Viewing angle** | 178° (IPS) |
| **Backlight** | Ajustable por software |

---

## 📚 Recursos Adicionales

- **Manual oficial**: [GitHub Freenove](https://github.com/Freenove/Freenove_Touchscreen_Monitor_for_Raspberry_Pi)
- **FAQs**: `FNK0078 FAQs.pdf` en el repositorio
- **Soporte**: support@freenove.com

---

## ✨ Tips Adicionales

### Ajustar brillo del backlight:

```bash
# Ver brillo actual
cat /sys/class/backlight/*/brightness

# Cambiar brillo (0-255)
echo 200 | sudo tee /sys/class/backlight/*/brightness
```

### Crear script para ajustar brillo:

```bash
nano ~/set_brightness.sh
```

Contenido:
```bash
#!/bin/bash
echo $1 | sudo tee /sys/class/backlight/*/brightness
```

Uso:
```bash
chmod +x ~/set_brightness.sh
./set_brightness.sh 150  # Brillo medio
```

### Deshabilitar screensaver para PushClone:

```bash
# Agregar a ~/.config/qt_env.sh
export DISPLAY=:0
xset s off
xset -dpms
xset s noblank
```

---

¡Tu display Freenove está listo para PushClone! 🎉
