#!/bin/bash

# Script para debuggear el polling de TBL_OPCUA
echo "=== Debugging TBL_OPCUA Polling ==="
echo "Tiempo: $(date)"
echo

# Parar el server actual si está corriendo
pkill -f planta_gas_server 2>/dev/null || true
sleep 2

# Ir al directorio del proyecto
cd /media/jose/Datos/Data/Proyectos/PetroSantander/SCADA/pac_to_opcua/planta_gas

# Compilar con información de debug extra
echo "🔨 Compilando con debug extra..."
make clean > /dev/null 2>&1
make build > /dev/null 2>&1

if [ ! -f "build/planta_gas_server" ]; then
    echo "❌ Error de compilación"
    exit 1
fi

echo "✅ Compilación exitosa"
echo

# Crear archivo temporal para capturar logs con más detalle
LOG_FILE="/tmp/opcua_debug_$(date +%Y%m%d_%H%M%S).log"

echo "🚀 Ejecutando servidor con logging detallado..."
echo "Logs guardados en: $LOG_FILE"
echo "Presiona Ctrl+C para parar"
echo "---"

# Ejecutar con timeout de 30 segundos y capturar salida
timeout 30s ./build/planta_gas_server 2>&1 | tee $LOG_FILE | while IFS= read -r line; do
    echo "$(date +%H:%M:%S) | $line"
    
    # Destacar líneas importantes
    if [[ "$line" == *"TBL_OPCUA"* ]] || [[ "$line" == *"readOPCUATable"* ]]; then
        echo "  >>> ⭐ LÍNEA IMPORTANTE: $line"
    fi
    
    if [[ "$line" == *"isConnected"* ]] || [[ "$line" == *"connected_"* ]]; then
        echo "  >>> 🔌 CONEXIÓN: $line"
    fi
done

echo
echo "=== Análisis de logs ==="
echo "Total líneas capturadas: $(wc -l < $LOG_FILE)"
echo
echo "Mensajes de TBL_OPCUA:"
grep -i "tbl_opcua\|opcua.*table" $LOG_FILE | head -10
echo
echo "Mensajes de conexión:"
grep -i "conectado\|connected\|isconnected" $LOG_FILE | head -5
echo
echo "Errores encontrados:"
grep -i "error\|failed\|warning" $LOG_FILE | head -10
echo
echo "Archivo completo de logs: $LOG_FILE"