#!/bin/bash

# Deploy optimizado para Raspberry Pi 5 (8GB RAM)

# Configuración
RPI_USER="pi"
RPI_HOST="raspberrypi.local"  # Usar mDNS en lugar de IP
PROJECT_NAME="PushClone"

# Opciones de compilación para RPi 5
USE_QT6="AUTO"  # AUTO, YES, NO
OPTIMIZE_LEVEL="3"  # 2=normal, 3=aggressive
PARALLEL_JOBS="8"  # RPi 5 tiene 4 cores, 8 threads virtuales

echo "🚀 ═══════════════════════════════════════════════════════"
echo "   DEPLOY OPTIMIZADO PARA RASPBERRY PI 5"
echo "   ═══════════════════════════════════════════════════════"
echo ""

echo "📦 Empaquetando proyecto..."

# Crear tarball con todo
tar czf ${PROJECT_NAME}.tar.gz \
    --exclude='.git' \
    --exclude='build*' \
    --exclude='*.tar.gz' \
    CMakeLists.txt \
    *.cpp \
    *.h \
    *.qml \
    *.qrc \
    qmldir \
    assets/ \
    components/ \
    views/

echo "📤 Copiando a Raspberry Pi 5..."
scp ${PROJECT_NAME}.tar.gz $RPI_USER@$RPI_HOST:~/

echo "🔨 Compilando en Raspberry Pi 5 (optimizado)..."
ssh $RPI_USER@$RPI_HOST << REMOTE_SCRIPT

# Limpiar build anterior
rm -rf ~/PushClone
mkdir -p ~/PushClone
cd ~/PushClone

# Extraer
tar xzf ~/PushClone.tar.gz

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 Verificando e instalando dependencias..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Detectar si Bookworm (Qt6 compatible) o Bullseye (Qt5 solo)
if grep -q "bookworm" /etc/os-release 2>/dev/null; then
    echo "✅ Detectado: Raspberry Pi OS Bookworm"
    OS_VERSION="bookworm"

    # Verificar si Qt6 está disponible
    if apt-cache search qt6-base-dev | grep -q "qt6-base-dev"; then
        echo "✅ Qt6 disponible en repositorios"
        USE_QT6_FINAL="YES"

        echo "📦 Instalando Qt6 + SerialPort..."
        sudo apt-get update -qq
        sudo apt-get install -y \
            qt6-base-dev \
            qt6-declarative-dev \
            qt6-serialport-dev \
            qml6-module-qtquick \
            qml6-module-qtquick-window \
            qml6-module-qtquick-controls \
            libgl1-mesa-dev
    else
        echo "⚠️  Qt6 no disponible, usando Qt5"
        USE_QT6_FINAL="NO"
    fi
else
    echo "⚠️  Detectado: Sistema antiguo (no Bookworm)"
    OS_VERSION="older"
    USE_QT6_FINAL="NO"
fi

# Si no se pudo determinar, instalar Qt5
if [ "\$USE_QT6_FINAL" != "YES" ]; then
    echo "📦 Instalando Qt5 + SerialPort..."
    sudo apt-get update -qq
    sudo apt-get install -y \
        qtdeclarative5-dev \
        libqt5serialport5-dev \
        qml-module-qtquick2 \
        qml-module-qtquick-window2 \
        qml-module-qtquick-controls2
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "⚙️  Configuración de compilación"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Crear directorio de build
mkdir -p build
cd build

# Configurar CMake según Qt version
if [ "\$USE_QT6_FINAL" == "YES" ]; then
    echo "🎯 Usando Qt6 con optimizaciones nivel ${OPTIMIZE_LEVEL}"
    cmake \
        -DUSE_QT6=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O${OPTIMIZE_LEVEL} -march=armv8-a+crc -mtune=cortex-a76" \
        ..
else
    echo "🎯 Usando Qt5 con optimizaciones nivel ${OPTIMIZE_LEVEL}"
    cmake \
        -DUSE_QT6=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O${OPTIMIZE_LEVEL} -march=armv8-a+crc -mtune=cortex-a76" \
        ..
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🔧 Compilando (${PARALLEL_JOBS} jobs paralelos)..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Compilar con todos los cores disponibles
time make -j${PARALLEL_JOBS}

echo ""
if [ -f "./appPushClone" ]; then
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "✅ COMPILACIÓN EXITOSA"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📍 Ejecutable: ~/PushClone/build/appPushClone"
    if [ "\$USE_QT6_FINAL" == "YES" ]; then
        echo "🎯 Qt Version: Qt6 (optimizado para RPi 5)"
    else
        echo "🎯 Qt Version: Qt5 (compatible)"
    fi
    echo "🚀 Optimización: -O${OPTIMIZE_LEVEL} + ARM Cortex-A76"
    ls -lh appPushClone
else
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "❌ ERROR EN COMPILACIÓN"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    exit 1
fi

REMOTE_SCRIPT

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ DEPLOY COMPLETADO"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Para ejecutar:"
echo "  ssh $RPI_USER@$RPI_HOST"
echo "  cd PushClone/build"
echo "  ./appPushClone"
echo ""
echo "Para ejecutar directamente:"
echo "  ssh $RPI_USER@$RPI_HOST 'cd PushClone/build && ./appPushClone'"
