#!/bin/bash

# Script de información de conexión - Servidor OPC UA PAC PLANTA_GAS
# Uso: ./connection_info.sh

echo "════════════════════════════════════════════════════════════"
echo "🏭 PAC PLANTA_GAS - Información de Conexión OPC UA"
echo "════════════════════════════════════════════════════════════"
echo
echo "📡 INFORMACIÓN DE CONEXIÓN:"
echo "   • URL del servidor: opc.tcp://localhost:4841"
echo "   • Nombre visible: PAC PLANTA_GAS"
echo "   • URI aplicación: urn:PAC:PLANTA_GAS:Server"
echo "   • Puerto: 4841"
echo
echo "🔧 CLIENTES OPC UA COMPATIBLES:"
echo "   ✅ UAExpert (Unified Automation)"
echo "   ✅ Ignition 8.x (Inductive Automation)"
echo "   ✅ KEPServerEX (PTC/Kepware)"
echo "   ✅ Prosys OPC Client (Prosys OPC)"
echo "   ✅ OPC Expert (Matrikon)"
echo
echo "🏗️ ESTRUCTURA JERÁRQUICA:"
echo "   PlantaGas/"
echo "   ├── Instrumentos/ (ET_*, PIT_*, TIT_*, FIT_*, LIT_*, PDIT_*)"
echo "   └── ControladorsPID/ (PRC_*, TRC_*, FRC_*)"
echo
echo "📝 INSTRUCCIONES DE CONEXIÓN:"
echo "   1. Iniciar servidor: ./build/planta_gas"
echo "   2. Abrir cliente OPC UA (ej: UAExpert)"
echo "   3. Conectar a: opc.tcp://localhost:4841"
echo "   4. Explorar estructura PlantaGas"
echo
echo "🔍 VERIFICAR ESTADO DEL SERVIDOR:"
if pgrep -f "planta_gas" > /dev/null; then
    echo "   🟢 Estado: SERVIDOR EJECUTÁNDOSE"
    echo "   📊 PID: $(pgrep -f planta_gas)"
else
    echo "   🔴 Estado: SERVIDOR DETENIDO"
    echo "   💡 Iniciar con: ./build/planta_gas"
fi
echo
echo "🌐 ENDPOINTS ADICIONALES:"
echo "   • HTTP API: http://localhost:8080/api"
echo "   • Logs: tail -f server.log"
echo
echo "════════════════════════════════════════════════════════════"