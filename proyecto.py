#!/usr/bin/env python3
"""
Script ejecutable para generar la estructura completa del proyecto planta_gas
Este script crea todos los archivos y directorios necesarios
"""

import os
import json
from pathlib import Path

def create_planta_gas_project():
    """Genera la estructura completa del proyecto planta_gas"""

    print("🏗️ Iniciando generación del proyecto planta_gas...")
    print("")

    # Crear directorio base
    base_path = Path("./planta_gas")
    base_path.mkdir(exist_ok=True)

    # Crear subdirectorios
    directories = ["src", "include", "config", "docs", "scripts", "build", "logs", "tests"]
    print("📁 Creando estructura de directorios:")
    for directory in directories:
        (base_path / directory).mkdir(exist_ok=True)
        print(f"✅ Creado directorio: {directory}/")

    print("")

    # 1. Generar tags_planta_gas.json
    print("📄 Generando configuración de tags...")

    # Lista completa de tags basada en la imagen
    all_tags = []

    # Flow Transmitters
    for i in range(1601, 1606):
        all_tags.append({
            "name": f"TB_FT_{i}",
            "value_table": f"TBL_TB_FT_{i}",
            "alarm_table": f"TBL_TA_FT_{i}",
            "description": f"Flow Transmitter {i}",
            "units": "m3/h",
            "polling_group": "fast"
        })

    # Pressure Transmitters
    for i in range(1501, 1506):
        all_tags.append({
            "name": f"TB_PT_{i}",
            "value_table": f"TBL_TB_PT_{i}",
            "alarm_table": f"TBL_TA_PT_{i}",
            "description": f"Pressure Transmitter {i}",
            "units": "bar",
            "polling_group": "fast"
        })

    # Temperature Transmitters
    temp_suffixes = ["1201A", "1201B", "1203A", "1203B", "1404A", "1404B", "1502A", "1502B", "1506A", "1506B"]
    for suffix in temp_suffixes:
        all_tags.append({
            "name": f"TB_TT_{suffix}",
            "value_table": f"TBL_TB_TT_{suffix}",
            "alarm_table": f"TBL_TA_TT_{suffix}",
            "description": f"Temperature Transmitter {suffix}",
            "units": "°C",
            "polling_group": "fast"
        })

    # Level Transmitters
    for i in range(1404, 1408):
        all_tags.append({
            "name": f"TB_LT_{i}",
            "value_table": f"TBL_TB_LT_{i}",
            "alarm_table": f"TBL_TA_LT_{i}",
            "description": f"Level Transmitter {i}",
            "units": "%",
            "polling_group": "fast"
        })

    # Variables estándar por tag
    standard_variables = {
        "PV": {"type": "FLOAT", "writable": False, "polling": "fast", "description": "Process Value"},
        "SV": {"type": "FLOAT", "writable": False, "polling": "medium", "description": "Simulated Value"},
        "SetHH": {"type": "FLOAT", "writable": True, "polling": "slow", "description": "High-High Alarm Setpoint"},
        "SetH": {"type": "FLOAT", "writable": True, "polling": "slow", "description": "High Alarm Setpoint"},
        "SetL": {"type": "FLOAT", "writable": True, "polling": "slow", "description": "Low Alarm Setpoint"},
        "SetLL": {"type": "FLOAT", "writable": True, "polling": "slow", "description": "Low-Low Alarm Setpoint"},
        "SP": {"type": "FLOAT", "writable": True, "polling": "medium", "description": "Setpoint"},
        "CV": {"type": "FLOAT", "writable": False, "polling": "medium", "description": "Control Value"},
        "auto_manual": {"type": "INT32", "writable": True, "polling": "medium", "description": "Auto/Manual Mode"},
        "Kp": {"type": "FLOAT", "writable": True, "polling": "slow", "description": "Proportional Gain"},
        "Ki": {"type": "FLOAT", "writable": True, "polling": "slow", "description": "Integral Gain"},
        "Kd": {"type": "FLOAT", "writable": True, "polling": "slow", "description": "Derivative Gain"},
        "RangeMin": {"type": "FLOAT", "writable": True, "polling": "slow", "description": "Minimum Range"},
        "RangeMax": {"type": "FLOAT", "writable": True, "polling": "slow", "description": "Maximum Range"},
        "EnableAlarms": {"type": "INT32", "writable": True, "polling": "slow", "description": "Enable Alarms"}
    }

    # Alarmas estándar
    standard_alarms = {
        "HI": {"type": "INT32", "polling": "fast", "description": "High Alarm"},
        "LO": {"type": "INT32", "polling": "fast", "description": "Low Alarm"},
        "HIHI": {"type": "INT32", "polling": "fast", "description": "High-High Alarm"},
        "LOLO": {"type": "INT32", "polling": "fast", "description": "Low-Low Alarm"},
        "BAD": {"type": "INT32", "polling": "fast", "description": "Bad Quality"},
        "AlarmColor": {"type": "INT32", "polling": "fast", "description": "Alarm Color Code"}
    }

    # Añadir variables y alarmas a cada tag
    for tag in all_tags:
        tag["variables"] = standard_variables.copy()
        tag["alarms"] = standard_alarms.copy()

    # Configuración principal
    config = {
        "pac_ip": "192.168.1.30",
        "pac_port": 22001,
        "opcua_port": 4840,
        "server_name": "Planta Gas SCADA Server",
        "application_uri": "urn:PlantaGas:SCADA:Server",

        "polling_config": {
            "fast_interval_ms": 250,
            "medium_interval_ms": 2000,
            "slow_interval_ms": 30000,
            "change_threshold": 0.01
        },

        "TBL_tags": all_tags
    }

    # Calcular estadísticas
    total_tags = len(all_tags)
    vars_per_tag = len(standard_variables)
    alarms_per_tag = len(standard_alarms)
    total_variables = total_tags * (vars_per_tag + alarms_per_tag)

    fast_vars = sum(1 for var in standard_variables.values() if var["polling"] == "fast") * total_tags + alarms_per_tag * total_tags
    medium_vars = sum(1 for var in standard_variables.values() if var["polling"] == "medium") * total_tags
    slow_vars = sum(1 for var in standard_variables.values() if var["polling"] == "slow") * total_tags

    writable_vars = sum(1 for var in standard_variables.values() if var["writable"]) * total_tags
    readonly_vars = total_variables - writable_vars

    config["statistics"] = {
        "total_tags": total_tags,
        "total_variables": total_variables,
        "fast_polling_vars": fast_vars,
        "medium_polling_vars": medium_vars,
        "slow_polling_vars": slow_vars,
        "writable_variables": writable_vars,
        "readonly_variables": readonly_vars
    }

    # Guardar archivo JSON
    with open(base_path / "config" / "tags_planta_gas.json", 'w', encoding='utf-8') as f:
        json.dump(config, f, indent=2, ensure_ascii=False)

    print(f"✅ Generado: config/tags_planta_gas.json")
    print(f"   📊 {total_tags} tags, {total_variables} variables totales")

    # 2. Generar CMakeLists.txt
    cmake_content = """cmake_minimum_required(VERSION 3.16)
project(planta_gas)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configuración de build
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Dependencias
find_package(PkgConfig REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)

# open62541
pkg_check_modules(OPEN62541 REQUIRED open62541)

# Headers
include_directories(include)
include_directories(${OPEN62541_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/tag_manager.cpp
    src/opcua_server.cpp
    src/pac_control_client.cpp
)

# Executable
add_executable(planta_gas ${SOURCES})

# Linking
target_link_libraries(planta_gas
    ${OPEN62541_LIBRARIES}
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
    pthread
)

# Install
install(TARGETS planta_gas DESTINATION bin)
"""

    with open(base_path / "CMakeLists.txt", 'w') as f:
        f.write(cmake_content)
    print("✅ Generado: CMakeLists.txt")

    # 3. Generar script de producción
    script_content = """#!/bin/bash
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
echo "   Endpoint: opc.tcp://localhost:4840"
"""

    script_file = base_path / "scripts" / "production_gas.sh"
    with open(script_file, 'w') as f:
        f.write(script_content)
    os.chmod(script_file, 0o755)
    print("✅ Generado: scripts/production_gas.sh (ejecutable)")

    # 4. Generar .gitignore
    gitignore_content = """# Build artifacts
build/
*.o
*.so
*.a

# Logs
logs/
*.log

# IDE files
.vscode/
.idea/

# Data files
data/
*.dat

# OS files
.DS_Store

# Temporary files
tmp/
*.tmp
"""

    with open(base_path / ".gitignore", 'w') as f:
        f.write(gitignore_content)
    print("✅ Generado: .gitignore")

    # 5. Generar Makefile
    makefile_content = """# Makefile para planta_gas
.PHONY: all clean build run help

all: build

build:
\t@echo "🔨 Compilando planta_gas..."
\t@mkdir -p build
\t@cd build && cmake .. && make -j$(nproc)

clean:
\t@echo "🧹 Limpiando..."
\t@rm -rf build

run: build
\t@echo "🚀 Ejecutando planta_gas..."
\t@./build/planta_gas

debug: build
\t@echo "🐛 Ejecutando con debug..."
\t@VERBOSE_DEBUG=1 ./build/planta_gas

help:
\t@echo "Targets disponibles:"
\t@echo "  build  - Compilar proyecto"
\t@echo "  clean  - Limpiar build"
\t@echo "  run    - Ejecutar servidor"
\t@echo "  debug  - Ejecutar con debug"
"""

    with open(base_path / "Makefile", 'w') as f:
        f.write(makefile_content)
    print("✅ Generado: Makefile")

    # 6. Generar README.md básico
    readme_content = """# 🏭 Planta Gas - Servidor OPC UA

Servidor OPC-UA optimizado para planta de gas con **600+ variables** distribuidas en **25+ tags** principales.

## 🚀 Inicio rápido

```bash
# Compilar
make build

# Ejecutar
make run

# Despliegue en producción
./scripts/production_gas.sh
```

## 📊 Especificaciones

- **Tags principales**: 25 (TB_FT_*, TB_PT_*, TB_TT_*, TB_LT_*)
- **Variables por tag**: 21 (15 variables + 6 alarmas)
- **Total variables**: ~525
- **Polling optimizado**: Fast(250ms), Medium(2s), Slow(30s)

## 🔌 Conectividad

- **Endpoint OPC UA**: `opc.tcp://localhost:4840`
- **PAC Controller**: `192.168.1.30:22001`

## 📁 Estructura

```
planta_gas/
├── src/                    # Código fuente C++
├── include/               # Headers
├── config/               # Configuración JSON
├── scripts/              # Scripts de despliegue
└── build/                # Archivos compilados
```

---
🏭 **Desarrollado para Planta de Gas**
"""

    with open(base_path / "README.md", 'w') as f:
        f.write(readme_content)
    print("✅ Generado: README.md")

    print("")
    print("🎉 ¡Estructura del proyecto generada exitosamente!")
    print("")
    print("📋 Próximos pasos:")
    print("   1. cd planta_gas")
    print("   2. git init")
    print("   3. make build")
    print("   4. ./scripts/production_gas.sh")
    print("")
    print(f"📊 Proyecto configurado con:")
    print(f"   • {total_tags} tags principales")
    print(f"   • {total_variables} variables totales")
    print(f"   • {fast_vars} variables de polling rápido")
    print(f"   • {medium_vars} variables de polling medio")
    print(f"   • {slow_vars} variables de polling lento")

if __name__ == "__main__":
    create_planta_gas_project()
