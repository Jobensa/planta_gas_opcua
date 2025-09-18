#pragma once

#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <fstream>

// Versión del proyecto
#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

// Configuración por defecto
#define DEFAULT_OPC_PORT 4841
#define DEFAULT_HTTP_PORT 8080
#define DEFAULT_POLLING_INTERVAL 1000  // ms
#define DEFAULT_MAX_HISTORY 1000

// Macros de logging con colores
#define RESET_COLOR   "\033[0m"
#define RED_COLOR     "\033[31m"
#define GREEN_COLOR   "\033[32m"
#define YELLOW_COLOR  "\033[33m"
#define BLUE_COLOR    "\033[34m"
#define MAGENTA_COLOR "\033[35m"
#define CYAN_COLOR    "\033[36m"
#define WHITE_COLOR   "\033[37m"

// Macros de logging
#define LOG_INFO(msg) \
    do { \
        auto now = std::chrono::system_clock::now(); \
        auto time_t = std::chrono::system_clock::to_time_t(now); \
        std::cout << BLUE_COLOR << "[INFO] " << std::put_time(std::localtime(&time_t), "%H:%M:%S") \
                  << RESET_COLOR << " " << msg << std::endl; \
    } while(0)

#define LOG_SUCCESS(msg) \
    do { \
        auto now = std::chrono::system_clock::now(); \
        auto time_t = std::chrono::system_clock::to_time_t(now); \
        std::cout << GREEN_COLOR << "[SUCCESS] " << std::put_time(std::localtime(&time_t), "%H:%M:%S") \
                  << RESET_COLOR << " " << msg << std::endl; \
    } while(0)

#define LOG_WARNING(msg) \
    do { \
        auto now = std::chrono::system_clock::now(); \
        auto time_t = std::chrono::system_clock::to_time_t(now); \
        std::cout << YELLOW_COLOR << "[WARNING] " << std::put_time(std::localtime(&time_t), "%H:%M:%S") \
                  << RESET_COLOR << " " << msg << std::endl; \
    } while(0)

#define LOG_ERROR(msg) \
    do { \
        auto now = std::chrono::system_clock::now(); \
        auto time_t = std::chrono::system_clock::to_time_t(now); \
        std::cerr << RED_COLOR << "[ERROR] " << std::put_time(std::localtime(&time_t), "%H:%M:%S") \
                  << RESET_COLOR << " " << msg << std::endl; \
    } while(0)

#define LOG_DEBUG(msg) \
    do { \
        auto now = std::chrono::system_clock::now(); \
        auto time_t = std::chrono::system_clock::to_time_t(now); \
        std::cout << MAGENTA_COLOR << "[DEBUG] " << std::put_time(std::localtime(&time_t), "%H:%M:%S") \
                  << RESET_COLOR << " " << msg << std::endl; \
    } while(0)

#define LOG_WRITE(msg) \
    do { \
        auto now = std::chrono::system_clock::now(); \
        auto time_t = std::chrono::system_clock::to_time_t(now); \
        std::cout << CYAN_COLOR << "[WRITE] " << std::put_time(std::localtime(&time_t), "%H:%M:%S") \
                  << RESET_COLOR << " " << msg << std::endl; \
    } while(0)

// Estructuras de configuración
struct OPCUAConfig {
    bool enabled = true;
    int port = DEFAULT_OPC_PORT;
    std::string server_name = "PlantaGas OPC-UA Server";
    std::string endpoint_url;
    bool enable_security = false;
    std::string certificate_path;
    std::string private_key_path;
    
    void print() const {
        LOG_INFO("Configuración OPC-UA:");
        LOG_INFO("  • Habilitado: " + std::string(enabled ? "Sí" : "No"));
        LOG_INFO("  • Puerto: " + std::to_string(port));
        LOG_INFO("  • Nombre servidor: " + server_name);
        if (!endpoint_url.empty()) {
            LOG_INFO("  • URL endpoint: " + endpoint_url);
        }
        LOG_INFO("  • Seguridad: " + std::string(enable_security ? "Habilitada" : "Deshabilitada"));
    }
};

struct HTTPConfig {
    bool enabled = true;
    int port = DEFAULT_HTTP_PORT;
    std::string bind_address = "0.0.0.0";
    bool enable_cors = true;
    std::vector<std::string> allowed_origins;
    
    void print() const {
        LOG_INFO("Configuración HTTP API:");
        LOG_INFO("  • Habilitado: " + std::string(enabled ? "Sí" : "No"));
        LOG_INFO("  • Puerto: " + std::to_string(port));
        LOG_INFO("  • Dirección: " + bind_address);
        LOG_INFO("  • CORS: " + std::string(enable_cors ? "Habilitado" : "Deshabilitado"));
    }
};

struct PACConfig {
    bool enabled = false;
    std::string host = "192.168.1.100";
    int port = 502;
    int timeout_ms = 5000;
    int retry_count = 3;
    std::string protocol = "modbus";
    
    void print() const {
        LOG_INFO("Configuración PAC:");
        LOG_INFO("  • Habilitado: " + std::string(enabled ? "Sí" : "No"));
        LOG_INFO("  • Host: " + host + ":" + std::to_string(port));
        LOG_INFO("  • Protocolo: " + protocol);
        LOG_INFO("  • Timeout: " + std::to_string(timeout_ms) + "ms");
        LOG_INFO("  • Reintentos: " + std::to_string(retry_count));
    }
};

struct SystemConfig {
    OPCUAConfig opcua;
    HTTPConfig http;
    PACConfig pac;
    
    // Configuración general
    uint32_t polling_interval_ms = DEFAULT_POLLING_INTERVAL;
    size_t max_history_size = DEFAULT_MAX_HISTORY;
    std::string log_level = "INFO";
    std::string log_file = "logs/planta_gas.log";
    bool enable_backup = true;
    std::string backup_directory = "backup";
    
    void print() const {
        LOG_INFO("🔧 Configuración del sistema PlantaGas v" + std::string(PROJECT_VERSION));
        LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        
        opcua.print();
        std::cout << std::endl;
        
        http.print();
        std::cout << std::endl;
        
        pac.print();
        std::cout << std::endl;
        
        LOG_INFO("Configuración general:");
        LOG_INFO("  • Intervalo polling: " + std::to_string(polling_interval_ms) + "ms");
        LOG_INFO("  • Tamaño histórico: " + std::to_string(max_history_size));
        LOG_INFO("  • Nivel log: " + log_level);
        LOG_INFO("  • Archivo log: " + log_file);
        LOG_INFO("  • Backup: " + std::string(enable_backup ? "Habilitado" : "Deshabilitado"));
        
        std::cout << std::endl;
    }
};

// Utilidades de tiempo
inline std::string getCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    return ss.str();
}

inline uint64_t getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// Utilidades de validación
inline bool isValidPort(int port) {
    return port > 0 && port < 65536;
}

inline bool isValidIPAddress(const std::string& ip) {
    // Validación básica de IP (se puede mejorar)
    return !ip.empty() && ip.find_first_not_of("0123456789.") == std::string::npos;
}

// Banner del sistema
inline void printBanner() {
    std::cout << CYAN_COLOR << R"(
╔══════════════════════════════════════════════════════════════╗
║                     🏭 PLANTA GAS OPC-UA                     ║
║                    Sistema Industrial SCADA                  ║
║                                                              ║
║                        Versión )" << PROJECT_VERSION << R"(                         ║
╚══════════════════════════════════════════════════════════════╝
)" << RESET_COLOR << std::endl;
}

// Funciones de utilidad para archivos
inline bool fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

inline std::string getFileExtension(const std::string& filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        return filename.substr(dot_pos + 1);
    }
    return "";
}

// Cleanup macros
#ifdef DEBUG_MODE
    #define DBG_PRINT(x) std::cout << "[DEBUG] " << x << std::endl
#else
    #define DBG_PRINT(x) do {} while(0)
#endif

// Tipos comunes
using TimePoint = std::chrono::system_clock::time_point;
using Duration = std::chrono::milliseconds;