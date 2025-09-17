# 🏭 Planta Gas - Sistema Industrial SCADA

Sistema SCADA industrial que integra **PAC Control** con **OPC UA**, gestionando **600+ variables** distribuidas en **50+ tags** industriales con arquitectura optimizada multi-hilo.

## ✨ Funcionalidades Implementadas

### 🏗️ **Arquitectura Multi-Servicio**
- **Servidor OPC UA** (puerto 4840) - 532 nodos jerárquicos
- **API HTTP REST** (puerto 8080) - Gestión de tags vía web
- **TagManager** - Sistema de gestión centralizada thread-safe
- **PAC Control Client** - Comunicación HTTP con controlador PAC

### 🏭 **Tags Industriales Configurados**
- **50 Tags principales**: FRC, TRC, PRC, FIT, TIT, PIT, ET, LIT, PDIT
- **Controladores PID**: 10 variables cada uno (PV, SP, CV, KP, KI, KD, auto_manual, OUTPUT_HIGH, OUTPUT_LOW, PID_ENABLE)
- **Instrumentos**: 11 variables cada uno (PV, SV, SetHH, SetH, SetL, SetLL, Input, percent, min, max, SIM_Value)
- **Total nodos OPC UA**: 532 nodos organizados jerárquicamente

### 🔧 **Características Técnicas**
- **C++17** con open62541 v1.3.8
- **Polling optimizado**: TBL_OPCUA (único request para 52 floats)
- **Thread Safety**: Acceso concurrente protegido con mutex
- **Configuración JSON**: Hot-reload y validación automática
- **Sistema de respaldo**: Auto-backup en cada inicio

## � Comandos de Uso

```bash
# Compilación estándar
make build

# Ejecutar servidor completo
make run

# Modo test (validación rápida)
./build/planta_gas --test

# Validar configuración JSON
./build/planta_gas --validate-config

# Despliegue optimizado producción
./scripts/production_gas.sh
```

## 🌐 Endpoints y Conectividad

### **OPC UA Server**
- **Endpoint**: `opc.tcp://localhost:4840`
- **Estructura jerárquica**: PlantaGas/[Categorías]/[Tags]/[Variables]
- **Tipos de nodo**: Carpetas organizacionales + Objetos industriales

### **API HTTP REST**
- **Base URL**: `http://localhost:8080/api`
- **Endpoints principales**:
  - `GET /tags` - Lista todos los tags
  - `GET /tags/{name}` - Obtener tag específico
  - `PUT /tags/{name}` - Actualizar valor de tag
  - `GET /status` - Estado del sistema

### **PAC Control Integration**
- **Controlador**: `192.168.1.30:22001`
- **Método**: HTTP requests con optimización TBL_OPCUA
- **Tablas**: TBL_FRC_*, TBL_TRC_*, TBL_PRC_*, etc.

## � Estructura Jerárquica OPC UA

```
PlantaGas/
├── FlowControllers/          # Controladores de Flujo
│   ├── FRC_1303 🏭          # Flow Rate Controller (Objeto Industrial)
│   │   ├── PV               # Process Variable
│   │   ├── SP               # Set Point
│   │   ├── CV               # Control Variable
│   │   ├── KP, KI, KD       # Parámetros PID
│   │   ├── auto_manual      # Modo operación
│   │   ├── OUTPUT_HIGH/LOW  # Límites salida
│   │   └── PID_ENABLE       # Habilitación PID
│   ├── FRC_1404 🏭
│   ├── FRC_1501 🏭
│   ├── FRC_1502 🏭
│   └── FRC_1606 🏭
├── TemperatureControllers/   # Controladores Temperatura
│   ├── TRC_1201 🏭
│   ├── TRC_1303 🏭
│   ├── TRC_1404 🏭
│   ├── TRC_1502 🏭
│   ├── TRC_1506 🏭
│   ├── TRC_1805 🏭
│   └── TRC_1915 🏭
├── PressureControllers/      # Controladores Presión
│   ├── PRC_1201 🏭
│   ├── PRC_1303 🏭
│   ├── PRC_1404 🏭
│   ├── PRC_1502 🏭
│   └── PRC_1758 🏭
├── FlowTransmitters/         # Transmisores Flujo
│   ├── ET_1601 🏭           # Flow Transmitter (Objeto Industrial)
│   │   ├── PV, SP, CV       # Variables PID
│   │   └── [Parámetros PID] # KP, KI, KD, etc.
│   ├── ET_1602 🏭
│   ├── ET_1603 🏭
│   ├── ET_1604 🏭
│   └── ET_1605 🏭
├── FlowIndicators/           # Indicadores Flujo
│   ├── FIT_1303 🏭          # Flow Indicator (Objeto Industrial)
│   │   ├── PV, SV           # Process/Set Value
│   │   ├── SetHH/H/L/LL     # Límites alarma
│   │   ├── Input, percent   # Señal entrada
│   │   ├── min, max         # Rangos
│   │   └── SIM_Value        # Simulación
│   ├── FIT_1404 🏭
│   ├── FIT_1501 🏭
│   ├── FIT_1502 🏭
│   ├── FIT_1505 🏭
│   └── FIT_1606 🏭
├── TemperatureIndicators/    # Indicadores Temperatura
│   ├── TIT_1201A/B 🏭
│   ├── TIT_1303A/B 🏭
│   ├── TIT_1404A/B 🏭
│   ├── TIT_1502A/B 🏭
│   ├── TIT_1506A/B 🏭
│   ├── TIT_1507A/B 🏭
│   ├── TIT_1805 🏭
│   └── TIT_1915 🏭
├── PressureIndicators/       # Indicadores Presión
│   ├── PIT_1201 🏭
│   ├── PIT_1303/A 🏭
│   ├── PIT_1404 🏭
│   ├── PIT_1502 🏭
│   └── PIT_1758 🏭
├── LevelIndicators/          # Indicadores Nivel
│   └── LIT_1501 🏭
└── PressureDifferentialIndicators/
    └── PDIT_1501 🏭          # Differential Pressure
```

### 🎯 **Iconografía UAExpert**
- 📁 **Carpetas organizacionales**: FlowControllers, TemperatureControllers, etc.
- 🏭 **Objetos industriales**: FRC_1404, TIT_1201A, PIT_1303, etc.
- 🔧 **Variables**: PV, SP, CV, SetHH, auto_manual, etc.

## 🔧 Configuración y Personalización

### **Archivo de Configuración**: `config/tags_planta_gas.json`
```json
{
  "tags": [
    {
      "name": "FRC_1404",
      "description": "Flow Rate Controller 1404", 
      "type": "FLOAT",
      "pac_address": "TBL_FRC_1404",
      "opcua_table_index": 15,
      "variables": ["PV", "SP", "CV", "KP", "KI", "KD", 
                   "auto_manual", "OUTPUT_HIGH", "OUTPUT_LOW", "PID_ENABLE"]
    }
  ]
}
```

### **Categorización Automática**
- **Controladores PID**: TRC*, FRC*, PRC*, ET* (10 variables c/u)
- **Instrumentos**: FIT*, TIT*, PIT*, LIT*, PDIT* (11 variables c/u)
- **Detección por prefijo**: Identificación automática por nombre de tag

## 🔍 Monitoreo y Debugging

### **Logs Estructurados**
```bash
# Logs colorizados con niveles
[INFO] 📊 Estado sistema - Tags: 50 | Nodos OPC: 532
[SUCCESS] ✅ API HTTP iniciada en puerto 8080  
[WARNING] ⚠️ PAC Control no disponible (modo offline)
[ERROR] 💥 Error en comunicación PAC: timeout
```

### **Validación de Sistema**
```bash
# Test completo de configuración
./build/planta_gas --test

# Validar solo estructura JSON  
./build/planta_gas --validate-config

# Verificar logs en tiempo real
tail -f server.log
```

## 🏗️ Arquitectura del Sistema

### **Componentes Principales**
```cpp
// TagManager - Gestión centralizada thread-safe
class TagManager {
    std::mutex tags_mutex_;           // Protección concurrente
    std::vector<std::shared_ptr<Tag>> tags_;  // Colección tags
    std::atomic<bool> running_;       // Estado sistema
};

// OPCUAServer - Servidor industrial con jerarquía
class OPCUAServer {
    std::unique_ptr<std::thread> server_thread_;  // Hilo dedicado
    UA_Server* ua_server_;            // Servidor open62541
    std::map<std::string, UA_NodeId> node_map_;   // Mapeo nodos
};

// TagManagementAPI - API REST para integración web
class TagManagementServer {
    std::unique_ptr<std::thread> server_thread_;  // Hilo HTTP
    httplib::Server http_server_;     // Servidor REST
};
```

### **Flujo de Datos**
```
PAC Control (192.168.1.30:22001) 
    ↓ HTTP requests optimizadas
TagManager (thread-safe)
    ↓ Acceso concurrente  
┌─ OPC UA Server (puerto 4840) ← UAExpert/SCADA clients
└─ HTTP API (puerto 8080) ← Web clients/integrations
```

## �📁 Estructura del Proyecto

```
planta_gas/
├── src/                      # 🔧 Código fuente C++17
│   ├── main.cpp             # • Coordinador principal multi-hilo
│   ├── opcua_server.cpp     # • Servidor OPC UA (532 nodos)  
│   ├── tag_management_api.cpp # • API HTTP REST (puerto 8080)
│   └── pac_control_client.cpp # • Cliente PAC Control HTTP
├── include/                  # 📋 Headers y declaraciones
│   ├── opcua_server.h       # • Interfaz servidor OPC UA
│   ├── tag_manager.h        # • Gestión centralizada tags
│   ├── tag.h                # • Definición clase Tag thread-safe
│   └── common.h             # • Utilidades y logging colorizado
├── config/                   # ⚙️ Configuración del sistema
│   └── tags_planta_gas.json # • 50 tags industriales configurados
├── scripts/                  # 🚀 Scripts de despliegue
│   ├── production_gas.sh    # • Deploy optimizado producción
│   └── [utilidades build]   # • Scripts auxiliares
├── build/                    # 🏗️ Archivos compilados
│   └── planta_gas           # • Ejecutable final
├── logs/                     # 📊 Logs del sistema
├── backups/                  # 💾 Respaldos automáticos configuración  
├── web/                      # 🌐 Recursos web (futuro dashboard)
├── tests/                    # 🧪 Tests unitarios
└── docs/                     # 📖 Documentación adicional
```

## 🚀 Casos de Uso Implementados

### **1. Monitoreo Industrial en Tiempo Real**
```bash
# Cliente OPC UA (UAExpert)
Conectar a: opc.tcp://localhost:4840
Navegar: PlantaGas/FlowControllers/FRC_1404/PV
```

### **2. Integración Web/API**
```bash
# Obtener todos los tags
curl http://localhost:8080/api/tags

# Leer valor específico
curl http://localhost:8080/api/tags/FRC_1404

# Actualizar valor 
curl -X PUT http://localhost:8080/api/tags/FRC_1404 \
     -H "Content-Type: application/json" \
     -d '{"value": 125.5}'
```

### **3. Integración con PAC Control**
```cpp
// Optimización TBL_OPCUA - Un request para 52 floats
GET http://192.168.1.30:22001/TBL_OPCUA
// vs 52 requests individuales
```

### **4. Dashboard de Supervisión** 
```bash
# Estado del sistema en tiempo real
curl http://localhost:8080/api/status
{
  "total_tags": 50,
  "opcua_nodes": 532,
  "pac_connection": "offline",
  "uptime": "00:15:32"
}
```

## 📋 Estado del Desarrollo

### ✅ **Completado y Funcional**
- [x] Servidor OPC UA con 532 nodos jerárquicos
- [x] API HTTP REST multi-hilo (puerto 8080)
- [x] TagManager thread-safe con 50 tags industriales  
- [x] Configuración JSON con hot-reload
- [x] Logging colorizado con niveles de severidad
- [x] Sistema de respaldos automáticos
- [x] Arquitectura multi-hilo (OPC UA + HTTP + Main)
- [x] Iconografía industrial correcta (objetos vs carpetas)
- [x] Integración PAC Control HTTP (modo offline funcional)
- [x] Categorización automática por prefijo de tag
- [x] Variables específicas por tipo (PID=10, Instruments=11)

### 🔄 **En Desarrollo**
- [ ] Dashboard web interactivo
- [ ] Históricos de variables
- [ ] Sistema de alarmas avanzado
- [ ] Métricas de performance

### � **Roadmap Futuro**
- [ ] Cliente OPC UA integrado
- [ ] Base de datos para históricos
- [ ] Notificaciones push
- [ ] Escalado horizontal

## 🤝 Integración y Compatibilidad

### **Clientes OPC UA Compatibles**
- ✅ **UAExpert** - Navegación jerárquica completa
- ✅ **OPC Expert** - Lectura/escritura variables  
- ✅ **Ignition SCADA** - Integración industrial
- ✅ **FactoryTalk** - Conexión Rockwell
- ✅ **WinCC OA** - SIEMENS integration

### **APIs REST Compatibles**
- ✅ **JSON Standard** - Content-Type: application/json
- ✅ **CORS Enabled** - Cross-origin requests
- ✅ **HTTP Methods** - GET, PUT, POST, DELETE  
- ✅ **Error Handling** - HTTP status codes apropiados

---

## 📞 Soporte y Mantenimiento

### **Comandos de Diagnóstico**
```bash
# Estado completo del sistema
./build/planta_gas --test

# Verificar configuración
./build/planta_gas --validate-config  

# Logs en vivo
tail -f logs/server.log

# Verificar puertos activos
netstat -tulpn | grep -E ":(4840|8080)"
```

### **Resolución de Problemas Comunes**
- **OPC UA no conecta**: Verificar puerto 4840 libre
- **API HTTP no responde**: Verificar puerto 8080 libre  
- **Tags no actualizan**: PAC Control en `192.168.1.30:22001`
- **Memoria alta**: Verificar límites histórico de tags

---

🏭 **Sistema SCADA Industrial - Planta de Gas**  
⚡ **Desarrollado con C++17, open62541, nlohmann/json, httplib**  
🎯 **Optimizado para entornos industriales críticos**
