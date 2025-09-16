# Copilot Instructions - Planta Gas OPC UA Server

## Architecture Overview

This is an **industrial SCADA system** that bridges PAC Control to OPC UA clients. The system manages **600+ variables** across **25+ tags** for gas plant operations with optimized polling strategies.

### Core Components

- **TagManager** (`include/tag_manager.h`): Central tag registry with threading and history
- **Tag System** (`include/tag.h`): Type-safe variant-based tags with quality and limits  
- **OPCUAServer** (`include/opcua_server.h`): open62541-based server with hierarchical structure
- **PACControlClient** (`include/pac_control_client.h`): HTTP client for PAC communication

### Data Flow Architecture

```
PAC Control (192.168.1.30:22001) → PACControlClient → TagManager → OPCUAServer → OPC UA Clients
```

**Critical Optimization**: TBL_OPCUA table read (52 floats) minimizes PAC requests vs individual variable reads.

## Development Patterns

### Tag Creation Pattern
```cpp
// Always use TagFactory for consistency
auto tag = TagFactory::createFloatTag("Temperature_Reactor", "DB1.REAL4");
tag->setDescription("Reactor main temperature");
tag->setUnit("°C");
tag->setMinValue(0.0);
tag->setMaxValue(150.0);
tag_manager.addTag(tag);
```

### Logging Convention
Use colorized macros from `common.h`:
```cpp
LOG_INFO("System starting...");    // Blue info
LOG_SUCCESS("✅ Connected");       // Green success  
LOG_WARNING("⚠️ Config missing"); // Yellow warning
LOG_ERROR("💥 Connection failed"); // Red error
```

### Configuration Structure
Tags defined in `config/tags_planta_gas.json` with industrial naming:
- **ET_1601** (Flow Transmitter), **PRC_1201** (Pressure Controller)
- Each tag has: `opcua_table_index`, `value_table`, `alarm_table`
- Variables: `["PV", "SP", "CV", "KP", "KI", "KD", "auto_manual", "OUTPUT_HIGH", "OUTPUT_LOW", "PID_ENABLE"]`

## Build System

### Standard Commands
```bash
make build    # CMake + make with parallel compilation
make run      # Build and execute
make clean    # Remove build artifacts
```

### Production Deployment
```bash
./scripts/production_gas.sh  # Full validation and optimized build
```

### Dependencies
- **open62541** (OPC UA), **nlohmann/json**, **CURL**, **httplib**
- CMake 3.16+, C++17 standard

## Key Integration Points

### PAC Communication
- HTTP requests to `192.168.1.30:22001` 
- TBL_OPCUA optimization reads 52 floats in single request
- Individual table reads: `TBL_ET_1601`, `TBL_PRC_1201`, etc.

### OPC UA Structure  
- Endpoint: `opc.tcp://localhost:4840`
- Hierarchical folders by tag category
- Variables as children of tag nodes
- Write callbacks update PAC via TagManager

## Performance Considerations

- **Fast polling**: 250ms for critical variables
- **Medium polling**: 2s for process variables  
- **Slow polling**: 30s for configuration
- TBL_OPCUA cache prevents excessive PAC requests
- Thread-safe TagManager with mutex protection

## Testing & Debugging

- `--test` mode for configuration validation
- `--validate-config` for JSON structure check
- `VERBOSE_DEBUG=1` environment variable for detailed logging
- Tag history tracking with configurable limits

When modifying this system, prioritize industrial reliability patterns: proper error handling, thread safety, and configuration validation.