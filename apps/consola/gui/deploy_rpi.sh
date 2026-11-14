#!/bin/bash

# Configuración
RPI_USER="pi"
RPI_HOST="raspberrypi.local"  # Usar mDNS
PROJECT_NAME="PushClone"

echo "📦 Empaquetando proyecto..."

# Crear tarball con todo (incluyendo CMakeLists.txt y subdirectorios)
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

echo "📤 Copiando a Raspberry Pi..."
scp ${PROJECT_NAME}.tar.gz $RPI_USER@$RPI_HOST:~/

echo "🔨 Compilando en Raspberry Pi..."
ssh $RPI_USER@$RPI_HOST << 'REMOTE_SCRIPT'

# Limpiar build anterior
rm -rf ~/PushClone
mkdir -p ~/PushClone
cd ~/PushClone

# Extraer
tar xzf ~/PushClone.tar.gz

# Dependencias (Qt5 + SerialPort)
echo "📦 Instalando dependencias de Qt..."
sudo apt-get update
sudo apt-get install -y qtdeclarative5-dev libqt5serialport5-dev

# Crear directorio de build
mkdir -p build
cd build

# Compilar con CMake para Qt5
echo "⚙️  Configurando con CMake (Qt5)..."
cmake -DUSE_QT6=OFF -DCMAKE_BUILD_TYPE=Release ..

echo "🔧 Compilando (esto puede tardar 2-5 minutos)..."
make -j4

if [ -f "./appPushClone" ]; then
    echo "✅ Compilación exitosa!"
    echo "Ejecutable: ~/PushClone/build/appPushClone"
else
    echo "❌ Error en compilación"
    exit 1
fi

REMOTE_SCRIPT

echo ""
echo "✅ Proyecto compilado en RPi"
echo ""
echo "Para ejecutar:"
echo "  ssh $RPI_USER@$RPI_HOST 'cd PushClone/build && ./appPushClone'"
