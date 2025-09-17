#include "common.h"
#include "tag_manager.h"
#include "tag_management_api.h"
#include "opcua_server.h"
#include "pac_control_client.h"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <fstream>
#include <nlohmann/json.hpp>

// Variables globales para el control del sistema
std::atomic<bool> g_running(true);
std::unique_ptr<TagManager> g_tag_manager;
std::unique_ptr<TagManagementAPI::TagManagementServer> g_api_server;
std::unique_ptr<OPCUAServer> g_opcua_server;
std::unique_ptr<PACControlClient> g_pac_client;

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
    // ¡MENSAJE CRÍTICO PARA DEBUG!
    std::cout << "🚀🚀🚀 MONITORINGLOOP INICIADO - HILO PRINCIPAL FUNCIONA 🚀🚀🚀" << std::endl;
    std::cerr << "🚀🚀🚀 MONITORINGLOOP INICIADO - HILO PRINCIPAL FUNCIONA 🚀🚀🚀" << std::endl;
    LOG_SUCCESS("🚀 PUNTO DE CONTROL - Entrando en monitoringLoop()");
    LOG_INFO("🧪 g_running al inicio = " + std::string(g_running ? "true" : "false"));
    LOG_INFO("🔄 Iniciando loop de monitoreo...");
    
    int counter = 0;
    auto last_opcua_read = std::chrono::steady_clock::now();
    const auto opcua_polling_interval = std::chrono::milliseconds(2000); // Polling cada 2 segundos para TBL_OPCUA
    
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        counter++;
        
        // Polling de TBL_OPCUA (crítico - cada 2 segundos)
        auto now = std::chrono::steady_clock::now();
        if (g_pac_client && g_pac_client->isConnected() && 
            (now - last_opcua_read) >= opcua_polling_interval) {
            
            LOG_INFO("🔄 Intentando leer TBL_OPCUA...");
            if (g_pac_client->readOPCUATable()) {
                LOG_SUCCESS("📊 TBL_OPCUA actualizada exitosamente");
            } else {
                LOG_ERROR("� Error leyendo TBL_OPCUA");
            }
            last_opcua_read = now;
        } else if (counter % 10 == 0) { // Debug de por qué no se ejecuta
            if (!g_pac_client) {
                LOG_WARNING("⚠️ g_pac_client es null");
            } else if (!g_pac_client->isConnected()) {
                LOG_WARNING("⚠️ PAC no está conectado según isConnected()");
            } else {
                auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_opcua_read);
                LOG_DEBUG("🕐 Esperando polling interval - Faltan " + std::to_string(opcua_polling_interval.count() - time_since_last.count()) + "ms");
            }
        }
        
        // Mostrar estado cada 30 segundos (counter * 0.5s = tiempo)
        if (counter % 60 == 0) {
            if (g_tag_manager) {
                auto status = g_tag_manager->getStatus();
                LOG_INFO("📊 Estado sistema - Tags: " + 
                        std::to_string(status["total_tags"].get<int>()) + 
                        " | Ejecutándose: " + 
                        (status["running"].get<bool>() ? "Sí" : "No"));
                
                // Mostrar estadísticas PAC si está disponible
                if (g_pac_client && g_pac_client->isConnected()) {
                    auto stats_report = g_pac_client->getStatsReport();
                    LOG_DEBUG("Estadísticas PAC:\n" + stats_report);
                }
                
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
    LOG_SUCCESS("🚀 PUNTO DE CONTROL - Entrando en showSystemInfo()");
    LOG_INFO("═══════════════════════════════════════════════════════");
    LOG_INFO("🏭 PLANTA GAS - Sistema de Monitoreo Industrial");
    LOG_INFO("   Versión: " + std::string(PROJECT_VERSION));
    LOG_INFO("   Compilado: " + std::string(__DATE__) + " " + std::string(__TIME__));
    LOG_INFO("═══════════════════════════════════════════════════════");
    
    LOG_SUCCESS("🚀 PUNTO DE CONTROL - Antes de g_tag_manager->getStatus()");
    if (g_tag_manager) {
        auto status = g_tag_manager->getStatus();
        LOG_SUCCESS("🚀 PUNTO DE CONTROL - Después de g_tag_manager->getStatus()");
        LOG_INFO("📊 Estado del TagManager:");
        LOG_INFO("   • Estado: " + std::string(status["running"].get<bool>() ? "Ejecutándose" : "Detenido"));
        LOG_INFO("   • Total tags: " + std::to_string(status["total_tags"].get<int>()));
        LOG_INFO("   • Intervalo polling: " + std::to_string(status["polling_interval_ms"].get<int>()) + "ms");
        LOG_INFO("   • Entradas historial: " + std::to_string(status["history_entries"].get<int>()));
    }
    
    LOG_INFO("═══════════════════════════════════════════════════════");
    LOG_SUCCESS("🚀 PUNTO DE CONTROL - Saliendo de showSystemInfo()");
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
        
        // Variable para almacenar configuración JSON completa
        nlohmann::json full_config;
        
        // Intentar cargar configuración
        if (fileExists(config_file)) {
            LOG_INFO("📄 Cargando configuración desde: " + config_file);
            
            // Cargar JSON completo para OPCUAServer
            try {
                std::ifstream config_stream(config_file);
                config_stream >> full_config;
            } catch (const std::exception& e) {
                LOG_ERROR("Error al cargar JSON: " + std::string(e.what()));
            }
            
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
        
        // Iniciar servidor OPC UA
        LOG_INFO("🔌 Iniciando servidor OPC UA...");
        try {
            g_opcua_server = std::make_unique<OPCUAServer>(shared_tag_manager);
            
            // Pasar configuración de tags jerárquicos al servidor OPC UA
            if (!full_config.is_null()) {
                g_opcua_server->setTagConfiguration(full_config);
            }
            
            if (g_opcua_server->start(DEFAULT_OPC_PORT)) {
                LOG_SUCCESS("✅ Servidor OPC UA ejecutándose en opc.tcp://localhost:" + std::to_string(DEFAULT_OPC_PORT));
            } else {
                LOG_ERROR("💥 Error al iniciar servidor OPC UA");
                g_opcua_server.reset();
            }
        } catch (const std::exception& e) {
            LOG_ERROR("💥 Excepción al inicializar servidor OPC UA: " + std::string(e.what()));
            g_opcua_server.reset();
        }
        
        // Iniciar cliente PAC Control para TBL_OPCUA
        LOG_INFO("🔗 Iniciando cliente PAC Control...");
        try {
            g_pac_client = std::make_unique<PACControlClient>(shared_tag_manager);
            
            // Configurar conexión desde la configuración JSON
            if (!full_config.is_null() && full_config.contains("pac_ip") && full_config.contains("pac_port")) {
                std::string pac_ip = full_config["pac_ip"];
                int pac_port = full_config["pac_port"];
                g_pac_client->setConnectionParams(pac_ip, pac_port);
            }
            
            // Intentar conectar
            if (g_pac_client->connect()) {
                LOG_SUCCESS("✅ Cliente PAC conectado correctamente");
                
                // Realizar lectura inicial de TBL_OPCUA
                if (g_pac_client->readOPCUATable()) {
                    LOG_SUCCESS("📊 TBL_OPCUA leída exitosamente en inicialización");
                }
            } else {
                LOG_WARNING("⚠️ No se pudo conectar al PAC Control, funcionando en modo offline");
            }
        } catch (const std::exception& e) {
            LOG_ERROR("💥 Excepción al inicializar cliente PAC: " + std::string(e.what()));
            LOG_WARNING("⚠️ Continuando sin cliente PAC (modo offline)");
            g_pac_client.reset();
        }
        
        // Mostrar información del sistema
        showSystemInfo();
        
        // Información de endpoints (cuando estén disponibles)
        LOG_INFO("🌐 Endpoints disponibles:");
        LOG_INFO("   • OPC-UA Server: opc.tcp://localhost:" + std::to_string(DEFAULT_OPC_PORT));
        LOG_INFO("   • HTTP API: http://localhost:" + std::to_string(DEFAULT_HTTP_PORT) + "/api");
        std::cout << std::endl;
        
        // DEBUGGING: Verificar que llegamos aquí
        LOG_SUCCESS("🚀 PUNTO DE CONTROL - Antes de llamar monitoringLoop()");
        LOG_INFO("🧪 g_running = " + std::string(g_running ? "true" : "false"));
        LOG_INFO("🧪 g_pac_client = " + std::string(g_pac_client ? "valid" : "null"));
        if (g_pac_client) {
            LOG_INFO("🧪 g_pac_client->isConnected() = " + std::string(g_pac_client->isConnected() ? "true" : "false"));
        }
        
        // Loop principal de monitoreo
        monitoringLoop();
        
        // Cierre limpio del sistema
        LOG_INFO("🛑 Iniciando cierre limpio del sistema...");
        
        if (g_pac_client) {
            g_pac_client->disconnect();
            LOG_SUCCESS("✅ Cliente PAC desconectado");
            g_pac_client.reset();
        }
        
        if (g_opcua_server) {
            g_opcua_server->stop();
            LOG_SUCCESS("✅ Servidor OPC UA detenido");
            g_opcua_server.reset();
        }
        
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
