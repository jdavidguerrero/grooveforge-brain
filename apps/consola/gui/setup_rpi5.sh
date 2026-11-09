#!/bin/bash

# Script de configuración para Raspberry Pi 5
# Ejecutar DIRECTAMENTE en la Raspberry Pi (no desde tu Mac)

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🚀 CONFIGURACIÓN RASPBERRY PI 5 - PUSHCLONE GUI"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Verificar que estamos en una RPi 5
if ! grep -q "Raspberry Pi 5" /proc/device-tree/model 2>/dev/null; then
    echo "⚠️  ADVERTENCIA: No se detectó Raspberry Pi 5"
    echo "   Este script está optimizado para RPi 5"
    read -p "¿Continuar de todas formas? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 PASO 1: Actualizar sistema"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

sudo apt-get update
sudo apt-get upgrade -y

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🎨 PASO 2: Instalar dependencias gráficas"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Instalar OpenGL y drivers gráficos
sudo apt-get install -y \
    libgl1-mesa-dev \
    libgles2-mesa-dev \
    mesa-utils \
    libdrm-dev \
    libgbm-dev

# Verificar si Bookworm (Qt6) o antiguo (Qt5)
if grep -q "bookworm" /etc/os-release 2>/dev/null; then
    echo "✅ Sistema: Raspberry Pi OS Bookworm"
    echo "📦 Instalando Qt6..."

    sudo apt-get install -y \
        qt6-base-dev \
        qt6-declarative-dev \
        qt6-serialport-dev \
        qml6-module-qtquick \
        qml6-module-qtquick-window \
        qml6-module-qtquick-controls

    QT_VERSION="Qt6"
else
    echo "⚠️  Sistema antiguo detectado"
    echo "📦 Instalando Qt5..."

    sudo apt-get install -y \
        qtdeclarative5-dev \
        libqt5serialport5-dev \
        qml-module-qtquick2 \
        qml-module-qtquick-window2 \
        qml-module-qtquick-controls2

    QT_VERSION="Qt5"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "⚙️  PASO 3: Configurar aceleración por hardware"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Habilitar OpenGL en /boot/firmware/config.txt (RPi 5)
CONFIG_FILE="/boot/firmware/config.txt"
if [ ! -f "$CONFIG_FILE" ]; then
    CONFIG_FILE="/boot/config.txt"  # Fallback para sistemas antiguos
fi

echo "📝 Configurando $CONFIG_FILE..."

# Backup del config
sudo cp $CONFIG_FILE ${CONFIG_FILE}.backup

# Agregar configuraciones si no existen
if ! grep -q "dtoverlay=vc4-kms-v3d" $CONFIG_FILE; then
    echo "dtoverlay=vc4-kms-v3d" | sudo tee -a $CONFIG_FILE
fi

# GPU memory (256MB para gráficos fluidos en 8GB RAM)
if ! grep -q "gpu_mem=" $CONFIG_FILE; then
    echo "gpu_mem=256" | sudo tee -a $CONFIG_FILE
else
    sudo sed -i 's/gpu_mem=.*/gpu_mem=256/' $CONFIG_FILE
fi

# Habilitar audio si está disponible
if ! grep -q "dtparam=audio=on" $CONFIG_FILE; then
    echo "dtparam=audio=on" | sudo tee -a $CONFIG_FILE
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🖥️  PASO 4: Configurar display 800x480"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo "Detectando display Freenove FNK0078 (5\" DSI 800x480)..."

# Freenove FNK0078 es un display MIPI DSI
# Se conecta al puerto DSI-1 (marcado CAM/DISP en RPi 5)

echo "📝 Configurando Freenove DSI display..."

# Para RPi 5, el display DSI necesita configuración específica
# Verificar si ya existe la configuración
if grep -q "dtoverlay=vc4-kms-dsi" $CONFIG_FILE; then
    echo "✅ DSI overlay ya configurado"
else
    echo "# Freenove 5\" DSI Display (800x480)" | sudo tee -a $CONFIG_FILE
    echo "dtoverlay=vc4-kms-v3d" | sudo tee -a $CONFIG_FILE
fi

# Configurar el display para usar DSI-1 (puerto correcto en RPi 5)
if ! grep -q "dtparam=dsi" $CONFIG_FILE; then
    echo "dtparam=dsi1" | sudo tee -a $CONFIG_FILE
fi

# Touch screen configuration (capacitive 5-point touch)
echo ""
echo "🖱️  Configurando touchscreen capacitivo..."
sudo apt-get install -y xserver-xorg-input-evdev

# Crear archivo de configuración para touch
sudo tee /usr/share/X11/xorg.conf.d/40-libinput.conf > /dev/null << 'TOUCH_EOF'
Section "InputClass"
    Identifier "libinput touchscreen catchall"
    MatchIsTouchscreen "on"
    MatchDevicePath "/dev/input/event*"
    Driver "libinput"
EndSection
TOUCH_EOF

echo "✅ Display Freenove DSI 800x480 configurado"
echo "   Puerto: DSI-1 (CAM/DISP)"
echo "   Touchscreen: Capacitivo 5 puntos"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🔧 PASO 5: Optimizaciones de rendimiento"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Crear archivo de variables de entorno para Qt
QT_ENV_FILE="$HOME/.config/qt_env.sh"
mkdir -p "$HOME/.config"

cat > $QT_ENV_FILE << 'EOF'
# Optimizaciones Qt para Raspberry Pi 5

# Usar OpenGL ES 2.0 (mejor rendimiento en RPi)
export QT_QPA_EGLFS_PHYSICAL_WIDTH=154    # mm (ajusta según tu display)
export QT_QPA_EGLFS_PHYSICAL_HEIGHT=85.92 # mm (ajusta según tu display)

# Habilitar threaded rendering
export QSG_RENDER_LOOP=basic

# Reducir warnings de Qt
export QT_LOGGING_RULES="*.debug=false;qt.qpa.*=false"

# Platform plugin
export QT_QPA_PLATFORM=eglfs

# Habilitar vsync para evitar tearing
export QT_QPA_EGLFS_FORCEVSYNC=1
EOF

echo "✅ Archivo de configuración Qt creado: $QT_ENV_FILE"

# Agregar al .bashrc si no existe
if ! grep -q "qt_env.sh" ~/.bashrc; then
    echo "" >> ~/.bashrc
    echo "# Qt optimizations for PushClone" >> ~/.bashrc
    echo "[ -f ~/.config/qt_env.sh ] && source ~/.config/qt_env.sh" >> ~/.bashrc
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🔐 PASO 6: Configurar UART (Puerto Serial)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo "📝 Habilitando UART en Raspberry Pi 5..."

# Habilitar UART y deshabilitar console serial
if ! grep -q "enable_uart=1" $CONFIG_FILE; then
    echo "# Habilitar UART para comunicación serial" | sudo tee -a $CONFIG_FILE
    echo "enable_uart=1" | sudo tee -a $CONFIG_FILE
fi

# Deshabilitar Bluetooth para liberar UART0 (opcional pero recomendado)
if ! grep -q "dtoverlay=disable-bt" $CONFIG_FILE; then
    echo "# Deshabilitar Bluetooth para liberar UART0" | sudo tee -a $CONFIG_FILE
    echo "dtoverlay=disable-bt" | sudo tee -a $CONFIG_FILE
fi

# Deshabilitar console serial en cmdline.txt
CMDLINE_FILE="/boot/firmware/cmdline.txt"
if [ ! -f "$CMDLINE_FILE" ]; then
    CMDLINE_FILE="/boot/cmdline.txt"
fi

if [ -f "$CMDLINE_FILE" ]; then
    echo "📝 Deshabilitando console serial en cmdline.txt..."
    sudo cp $CMDLINE_FILE ${CMDLINE_FILE}.backup

    # Remover console=serial0,115200 y console=ttyAMA0,115200
    sudo sed -i 's/console=serial0,[0-9]\+ //g' $CMDLINE_FILE
    sudo sed -i 's/console=ttyAMA0,[0-9]\+ //g' $CMDLINE_FILE
    sudo sed -i 's/console=ttyS0,[0-9]\+ //g' $CMDLINE_FILE
fi

# Deshabilitar servicio de console serial
sudo systemctl disable serial-getty@ttyAMA0.service 2>/dev/null || true
sudo systemctl disable serial-getty@serial0.service 2>/dev/null || true

# Agregar usuario al grupo dialout para acceso serial
sudo usermod -a -G dialout $USER

echo ""
echo "✅ UART configurado correctamente"
echo "   Puerto principal: /dev/ttyAMA0 (GPIO 14/15)"
echo "   Velocidad: Configurable (9600, 115200, etc.)"
echo "   Usuario agregado al grupo 'dialout'"
echo ""
echo "⚠️  Importante: Requiere REINICIO para tomar efecto"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🚀 PASO 7: (Opcional) Autoarranque"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

read -p "¿Quieres que PushClone arranque automáticamente? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    AUTOSTART_DIR="$HOME/.config/autostart"
    mkdir -p "$AUTOSTART_DIR"

    cat > "$AUTOSTART_DIR/pushclone.desktop" << EOF
[Desktop Entry]
Type=Application
Name=PushClone
Exec=/home/$USER/PushClone/build/appPushClone
Terminal=false
X-GNOME-Autostart-enabled=true
EOF

    echo "✅ Autoarranque configurado"
    echo "   Ubicación: $AUTOSTART_DIR/pushclone.desktop"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ CONFIGURACIÓN COMPLETADA"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "📊 RESUMEN:"
echo "  • Sistema actualizado: ✅"
echo "  • Qt instalado: $QT_VERSION"
echo "  • OpenGL habilitado: ✅"
echo "  • GPU Memory: 256MB"
echo "  • Permisos serial: ✅"
echo "  • Variables Qt: ~/.config/qt_env.sh"
echo ""
echo "⚠️  IMPORTANTE: Se requiere REINICIAR para aplicar todos los cambios"
echo ""
read -p "¿Reiniciar ahora? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "🔄 Reiniciando en 3 segundos..."
    sleep 3
    sudo reboot
else
    echo ""
    echo "Recuerda reiniciar más tarde con: sudo reboot"
    echo ""
    echo "Después del reinicio, compila el proyecto con:"
    echo "  cd ~/PushClone/build"
    echo "  cmake -DUSE_QT6=OFF .."  # o =ON si tienes Qt6
    echo "  make -j8"
    echo "  ./appPushClone"
fi
