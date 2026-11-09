#!/bin/bash

# Script para verificar capacidades de Qt en RPi 5

RPI_USER="pi"
RPI_HOST="raspberrypi.local"

echo "🔍 Verificando Qt en Raspberry Pi 5..."

ssh $RPI_USER@$RPI_HOST << 'REMOTE_CHECK'

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🔧 INFORMACIÓN DEL SISTEMA"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
cat /proc/device-tree/model
echo ""
echo "Memoria RAM:"
free -h | grep Mem
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 VERSIONES DE QT DISPONIBLES"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Verificar Qt5
if dpkg -l | grep -q "qtbase5-dev"; then
    QT5_VERSION=$(dpkg -l | grep "qtbase5-dev" | awk '{print $3}')
    echo "✅ Qt5 instalado: $QT5_VERSION"
else
    echo "❌ Qt5 no instalado"
fi

# Verificar Qt6
if dpkg -l | grep -q "qt6-base-dev"; then
    QT6_VERSION=$(dpkg -l | grep "qt6-base-dev" | awk '{print $3}')
    echo "✅ Qt6 instalado: $QT6_VERSION"
else
    echo "❌ Qt6 no instalado (pero puede instalarse)"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🎯 RECOMENDACIÓN"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Verificar versión de Raspberry Pi OS
if grep -q "bookworm" /etc/os-release 2>/dev/null; then
    echo "✅ Tienes Raspberry Pi OS Bookworm (compatible con Qt6)"
    echo "   Recomendación: USAR QT6 para mejor rendimiento"
else
    echo "⚠️  Sistema operativo antiguo detectado"
    echo "   Recomendación: Actualizar a Bookworm o usar Qt5"
fi

REMOTE_CHECK

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Basado en esta información, puedes decidir:"
echo "  • Qt6: Mejor rendimiento, gráficos modernos (RPi 5)"
echo "  • Qt5: Mayor compatibilidad, estable"
