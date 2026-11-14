#!/bin/bash

# Script todo-en-uno: Configura RPi 5 y hace deploy del proyecto
# Ejecutar desde tu Mac

RPI_USER="pi"
RPI_HOST="raspberrypi.local"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🚀 SETUP COMPLETO RASPBERRY PI 5 + DEPLOY"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Paso 1: Copiar script de setup a la RPi
echo "📤 Paso 1/2: Copiando script de configuración a RPi..."
scp setup_rpi5.sh $RPI_USER@$RPI_HOST:~/

# Paso 2: Ejecutar configuración en la RPi
echo ""
echo "⚙️  Paso 2/2: Ejecutando configuración en RPi..."
echo "   (Esto puede tomar 5-10 minutos)"
echo ""

ssh -t $RPI_USER@$RPI_HOST 'bash ~/setup_rpi5.sh'

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Después del reinicio, ejecuta:"
echo "  ./deploy_rpi5.sh"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
