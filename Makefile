# Makefile para planta_gas
.PHONY: all clean build run help

all: build

build:
	@echo "🔨 Compilando planta_gas..."
	@mkdir -p build
	@cd build && cmake .. && make -j$(nproc)

clean:
	@echo "🧹 Limpiando..."
	@rm -rf build

run: build
	@echo "🚀 Ejecutando planta_gas..."
	@./build/planta_gas

debug: build
	@echo "🐛 Ejecutando con debug..."
	@VERBOSE_DEBUG=1 ./build/planta_gas

help:
	@echo "Targets disponibles:"
	@echo "  build  - Compilar proyecto"
	@echo "  clean  - Limpiar build"
	@echo "  run    - Ejecutar servidor"
	@echo "  debug  - Ejecutar con debug"
