# 🏭 Planta Gas - Servidor OPC UA

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
