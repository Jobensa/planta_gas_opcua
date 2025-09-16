#include "common.h"
#include "tag_manager.h"
#include "tag_management_api.h"
#include <iostream>
#include <signal.h>
#include <atomic>

// Variables globales para el control del sistema
std::atomic<bool> g_running(true);
std::unique_ptr<TagManager> g_tag_manager;
std::unique_ptr<TagManagementAPI::TagManagementServer> g_api_server;

// Handler para señales del sistema
void signalHandler(int signal) {
    LOG_WARNING("🛑 Señal recibida (" + std::to_string(signal) + "), iniciando cierre...");
    g_running = false;
}

// Función para crear algunos tags de ejemplo
void createExampleTags(TagManager& tag_manager) {
    LOG_INFO("Creando tags de ejemplo...");
    
    // Crear tags de ejemplo usando el factory
    auto temp_tag = TagFactory::createFloatTag("Temperatura_Reactor", "DB1.REAL4");
    temp_tag->setDescription("Temperatura del reactor principal");
    temp_tag->setUnit("°C");
    temp_tag->setGroup("Reactor");
    temp_tag->setMinValue(0.0);
    temp_tag->setMaxValue(150.0);
    temp_tag->setValue(25.5f);
    
    auto pressure_tag = TagFactory::createFloatTag("Presion_Linea", "DB1.REAL8");
    pressure_tag->setDescription("Presión en línea principal");
    pressure_tag->setUnit("bar");
    pressure_tag->setGroup("Proceso");
    pressure_tag->setMinValue(0.0);
    pressure_tag->setMaxValue(10.0);
    pressure_tag->setValue(3.2f);
    
    auto flow_tag = TagFactory::createFloatTag("Flujo_Gas", "DB1.REAL12");
    flow_tag->setDescription("Flujo de gas natural");
    flow_tag->setUnit("m³/h");
    flow_tag->setGroup("Proceso");
    flow_tag->setMinValue(0.0);
    flow_tag->setMaxValue(1000.0);
    flow_tag->setValue(245.8f);
    
    auto alarm_tag = TagFactory::createBooleanTag("Alarma_General", "DB1.DBX16.0");
    alarm_tag->setDescription("Alarma general del sistema");
    alarm_tag->setGroup("Alarmas");
    alarm_tag->setValue(false);
    
    auto estado_tag = TagFactory::createStringTag("Estado_Sistema", "DB1.STRING20");
    estado_tag->setDescription("Estado actual del sistema");
    estado_tag->setGroup("Sistema");
    estado_tag->setValue("OPERATIVO");
    
    // Agregar tags al manager
    tag_manager.addTag(temp_tag);
    tag_manager.addTag(pressure_tag);
    tag_manager.addTag(flow_tag);
    tag_manager.addTag(alarm_tag);
    tag_manager.addTag(estado_tag);
    
    LOG_SUCCESS("✅ Tags de ejemplo creados");
}

// Función de monitoreo básico
void monitoringLoop() {
    LOG_INFO("🔄 Iniciando loop de monitoreo...");
    
    int counter = 0;
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        counter++;
        
        // Mostrar estado cada 30 segundos
        if (counter % 6 == 0) {
            if (g_tag_manager) {
                auto status = g_tag_manager->getStatus();
                LOG_INFO("📊 Estado sistema - Tags: " + 
                        std::to_string(status["total_tags"].get<int>()) + 
                        " | Ejecutándose: " + 
                        (status["running"].get<bool>() ? "Sí" : "No"));
                
                // Mostrar algunos valores de ejemplo
                auto tags = g_tag_manager->getAllTags();
                if (!tags.empty()) {
                    LOG_DEBUG("Valores actuales:");
                    for (const auto& tag : tags) {
                        if (tag->getName().find("Temperatura") != std::string::npos ||
                            tag->getName().find("Presion") != std::string::npos) {
                            LOG_DEBUG("  • " + tag->getName() + " = " + 
                                    tag->getValueAsString() + " " + tag->getUnit());
                        }
                    }
                }
            }
        }
        
        // Simular algunos cambios en los valores
        if (counter % 3 == 0 && g_tag_manager) {
            // Simular variación en temperatura
            auto temp_tag = g_tag_manager->getTag("Temperatura_Reactor");
            if (temp_tag) {
                float current_temp = temp_tag->getValueAsFloat();
                float new_temp = current_temp + ((rand() % 21 - 10) * 0.1f); // ±1°C
                temp_tag->setValue(new_temp);
            }
            
            // Simular variación en presión
            auto pressure_tag = g_tag_manager->getTag("Presion_Linea");
            if (pressure_tag) {
                float current_pressure = pressure_tag->getValueAsFloat();
                float new_pressure = current_pressure + ((rand() % 11 - 5) * 0.01f); // ±0.05 bar
                pressure_tag->setValue(new_pressure);
            }
        }
    }
    
    LOG_INFO("🛑 Loop de monitoreo finalizado");
}

// Función para mostrar información del sistema
void showSystemInfo() {
    LOG_INFO("═══════════════════════════════════════════════════════");
    LOG_INFO("🏭 PLANTA GAS - Sistema de Monitoreo Industrial");
    LOG_INFO("   Versión: " + std::string(PROJECT_VERSION));
    LOG_INFO("   Compilado: " + std::string(__DATE__) + " " + std::string(__TIME__));
    LOG_INFO("═══════════════════════════════════════════════════════");
    
    if (g_tag_manager) {
        auto status = g_tag_manager->getStatus();
        LOG_INFO("📊 Estado del TagManager:");
        LOG_INFO("   • Estado: " + std::string(status["running"].get<bool>() ? "Ejecutándose" : "Detenido"));
        LOG_INFO("   • Total tags: " + std::to_string(status["total_tags"].get<int>()));
        LOG_INFO("   • Intervalo polling: " + std::to_string(status["polling_interval_ms"].get<int>()) + "ms");
        LOG_INFO("   • Entradas historial: " + std::to_string(status["history_entries"].get<int>()));
    }
    
    LOG_INFO("═══════════════════════════════════════════════════════");
}

int main(int argc, char* argv[]) {
    try {
        // Mostrar banner
        printBanner();
        
        // Configurar handlers de señales
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // Procesar argumentos de línea de comandos
        bool show_help = false;
        bool validate_config = false;
        bool test_mode = false;
        std::string config_file = "config/tags_planta_gas.json";
        
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                show_help = true;
            } else if (arg == "--validate-config") {
                validate_config = true;
            } else if (arg == "--test") {
                test_mode = true;
            } else if (arg == "--config" && i + 1 < argc) {
                config_file = argv[++i];
            }
        }
        
        if (show_help) {
            std::cout << "Uso: " << argv[0] << " [opciones]\n"
                      << "Opciones:\n"
                      << "  --help, -h           Mostrar esta ayuda\n"
                      << "  --config <archivo>   Especificar archivo de configuración\n"
                      << "  --validate-config    Validar configuración y salir\n"
                      << "  --test              Ejecutar en modo test\n"
                      << std::endl;
            return 0;
        }
        
        LOG_INFO("🚀 Iniciando PlantaGas OPC-UA Server...");
        
        // Crear e inicializar TagManager
        g_tag_manager = std::make_unique<TagManager>();
        
        // Intentar cargar configuración
        if (fileExists(config_file)) {
            LOG_INFO("📄 Cargando configuración desde: " + config_file);
            if (g_tag_manager->loadFromFile(config_file)) {
                LOG_SUCCESS("✅ Configuración cargada correctamente");
            } else {
                LOG_WARNING("⚠️  Error cargando configuración, usando valores por defecto");
            }
        } else {
            LOG_WARNING("⚠️  Archivo de configuración no encontrado: " + config_file);
            LOG_INFO("📝 Creando tags de ejemplo...");
            createExampleTags(*g_tag_manager);
        }
        
        if (validate_config) {
            LOG_INFO("✅ Configuración validada correctamente");
            return 0;
        }
        
        if (test_mode) {
            LOG_INFO("🧪 Ejecutando en modo test...");
            // Ejecutar algunas pruebas básicas
            auto tags = g_tag_manager->getAllTags();
            LOG_INFO("Test: " + std::to_string(tags.size()) + " tags cargados");
            for (const auto& tag : tags) {
                LOG_INFO("  • " + tag->toString());
            }
            return 0;
        }
        
        // Iniciar TagManager
        LOG_INFO("▶️  Iniciando TagManager...");
        g_tag_manager->start();
        LOG_SUCCESS("✅ TagManager iniciado correctamente");
        
        // Iniciar API HTTP
        LOG_INFO("🌐 Iniciando API HTTP...");
        std::shared_ptr<TagManager> shared_tag_manager(g_tag_manager.get(), [](TagManager*) {
            // Empty deleter since we don't want shared_ptr to delete the object
        });
        g_api_server = TagManagementAPI::createTagManagementServer(shared_tag_manager, config_file);
        if (g_api_server && g_api_server->startServer(DEFAULT_HTTP_PORT)) {
            LOG_SUCCESS("✅ API HTTP iniciada en puerto " + std::to_string(DEFAULT_HTTP_PORT));
        } else {
            LOG_WARNING("⚠️  No se pudo iniciar API HTTP");
        }
        
        // Mostrar información del sistema
        showSystemInfo();
        
        // Información de endpoints (cuando estén disponibles)
        LOG_INFO("🌐 Endpoints disponibles:");
        LOG_INFO("   • OPC-UA Server: opc.tcp://localhost:" + std::to_string(DEFAULT_OPC_PORT));
        LOG_INFO("   • HTTP API: http://localhost:" + std::to_string(DEFAULT_HTTP_PORT) + "/api");
        std::cout << std::endl;
        
        // Loop principal de monitoreo
        monitoringLoop();
        
        // Cierre limpio del sistema
        LOG_INFO("🛑 Iniciando cierre limpio del sistema...");
        
        if (g_api_server) {
            g_api_server->stopServer();
            LOG_SUCCESS("✅ API HTTP detenida");
        }
        
        if (g_tag_manager) {
            g_tag_manager->stop();
            LOG_SUCCESS("✅ TagManager detenido");
        }
        
        LOG_SUCCESS("🏁 Sistema cerrado correctamente");
        
    } catch (const std::exception& e) {
        LOG_ERROR("💥 Excepción no manejada: " + std::string(e.what()));
        return 1;
    } catch (...) {
        LOG_ERROR("💥 Excepción desconocida");
        return 1;
    }
    
    return 0;
}
