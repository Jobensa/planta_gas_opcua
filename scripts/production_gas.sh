#!/bin/bash
# Script de producción para planta_gas

set -e

echo "🏭 Iniciando despliegue de Planta Gas..."

# Verificar dependencias
echo "🔍 Verificando dependencias..."
command -v cmake >/dev/null 2>&1 || { echo "❌ CMake requerido"; exit 1; }
pkg-config --exists open62541 || { echo "❌ open62541 requerido"; exit 1; }

# Compilar
echo "🔨 Compilando proyecto..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

echo "✅ Compilación completada"

# Validar configuración
echo "🔧 Validando configuración..."
python3 -m json.tool config/tags_planta_gas.json > /dev/null || { echo "❌ JSON inválido"; exit 1; }

echo "🚀 Sistema listo para ejecutar"
echo "   Ejecutar: ./build/planta_gas"
echo "   Endpoint: opc.tcp://localhost:4841"
