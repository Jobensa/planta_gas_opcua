/*
 * common.h - Definiciones comunes para planta_gas
 * 
 * Sistema de logging con colores, estructuras compartidas y constantes
 */

#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>

// Códigos de colores ANSI para terminal
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BRIGHT_RED = "\033[91m";
    const std::string BRIGHT_GREEN = "\033[92m";
    const std::string BRIGHT_YELLOW = "\033[93m";
    const std::string BRIGHT_BLUE = "\033[94m";
    const std::string BRIGHT_MAGENTA = "\033[95m";
    const std::string BRIGHT_CYAN = "\033[96m";
}

// Configuración global del sistema de logging
namespace LogConfig {
    extern std::mutex log_mutex;
    extern bool verbose_debug;
    extern bool silent_mode;
    extern bool use_colors;
}

// Funciones de logging con colores y timestamp
inline std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

inline void logMessage(const std::string& level, const std::string& color, 
                      const std::string& emoji, const std::string& message) {
    std::lock_guard<std::mutex> lock(LogConfig::log_mutex);
    
    if (LogConfig::silent_mode && level != "ERROR") {
        return;
    }
    
    if (!LogConfig::verbose_debug && level == "DEBUG") {
        return;
    }
    
    std::string timestamp = getCurrentTimestamp();
    
    if (LogConfig::use_colors) {
        std::cout << color << "[" << timestamp << "] " << emoji << " [" << level << "] " 
                  << Colors::RESET << message << std::endl;
    } else {
        std::cout << "[" << timestamp << "] [" << level << "] " << message << std::endl;
    }
}

// Macros de logging
#define LOG_ERROR(msg) logMessage("ERROR", Colors::BRIGHT_RED, "❌", msg)
#define LOG_WARNING(msg) logMessage("WARNING", Colors::YELLOW, "⚠️", msg)
#define LOG_INFO(msg) logMessage("INFO", Colors::BLUE, "ℹ️", msg)
#define LOG_SUCCESS(msg) logMessage("SUCCESS", Colors::BRIGHT_GREEN, "✅", msg)
#define LOG_DEBUG(msg) logMessage("DEBUG", Colors::CYAN, "🔧", msg)
#define LOG_WRITE(msg) logMessage("WRITE", Colors::MAGENTA, "📝", msg)
#define LOG_PAC(msg) logMessage("PAC", Colors::BRIGHT_CYAN, "🔌", msg)

// Configuración del proyecto
namespace ProjectConfig {
    const std::string PROJECT_NAME = "planta_gas";
    const std::string PROJECT_VERSION = "1.0.0";
    const std::string PROJECT_AUTHOR = "Jose";
    const std::string APPLICATION_URI = "urn:PlantaGas:SCADA:Server";
    
    // Configuración por defecto del PAC
    const std::string DEFAULT_PAC_IP = "192.168.1.30";
    const int DEFAULT_PAC_PORT = 22001;
    const int DEFAULT_OPCUA_PORT = 4840;
    
    // Configuración de polling por defecto
    const int DEFAULT_FAST_INTERVAL_MS = 250;
    const int DEFAULT_MEDIUM_INTERVAL_MS = 2000;
    const int DEFAULT_SLOW_INTERVAL_MS = 30000;
    const float DEFAULT_CHANGE_THRESHOLD = 0.01f;
    
    // Límites del sistema
    const size_t MAX_TAGS = 100;
    const size_t MAX_VARIABLES_PER_TAG = 50;
    const size_t MAX_CONCURRENT_PAC_REQUESTS = 10;
    const size_t BATCH_SIZE_LIMIT = 50;
}

// Estructura para resultados de operaciones
struct OperationResult {
    bool success;
    std::string message;
    int error_code;
    
    OperationResult(bool s = false, const std::string& msg = "", int code = 0) 
        : success(s), message(msg), error_code(code) {}
    
    static OperationResult Success(const std::string& msg = "Operación exitosa") {
        return OperationResult(true, msg, 0);
    }
    
    static OperationResult Error(const std::string& msg, int code = -1) {
        return OperationResult(false, msg, code);
    }
};

// Enumeración de estados del sistema
enum class SystemState {
    STOPPED,
    INITIALIZING,
    CONNECTING,
    RUNNING,
    ERROR,
    STOPPING
};

inline std::string systemStateToString(SystemState state) {
    switch (state) {
        case SystemState::STOPPED: return "STOPPED";
        case SystemState::INITIALIZING: return "INITIALIZING";
        case SystemState::CONNECTING: return "CONNECTING";
        case SystemState::RUNNING: return "RUNNING";
        case SystemState::ERROR: return "ERROR";
        case SystemState::STOPPING: return "STOPPING";
        default: return "UNKNOWN";
    }
}

// Estructura para configuración de conexión PAC
struct PACConnectionConfig {
    std::string ip_address;
    int port;
    int timeout_ms;
    int retry_attempts;
    int retry_delay_ms;
    bool use_authentication;
    std::string username;
    std::string password;
    
    PACConnectionConfig() 
        : ip_address(ProjectConfig::DEFAULT_PAC_IP)
        , port(ProjectConfig::DEFAULT_PAC_PORT)
        , timeout_ms(5000)
        , retry_attempts(3)
        , retry_delay_ms(1000)
        , use_authentication(false) {}
};

// Estructura para configuración OPC UA
struct OPCUAConfig {
    int port;
    std::string server_name;
    std::string application_uri;
    bool security_enabled;
    std::string certificate_path;
    std::string private_key_path;
    int max_sessions;
    int max_subscriptions;
    
    OPCUAConfig()
        : port(ProjectConfig::DEFAULT_OPCUA_PORT)
        , server_name("Planta Gas SCADA Server")
        , application_uri(ProjectConfig::APPLICATION_URI)
        , security_enabled(false)
        , max_sessions(100)
        , max_subscriptions(1000) {}
};

// Utilidades de tiempo
namespace TimeUtils {
    inline std::chrono::milliseconds getCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
    }
    
    inline std::string formatDuration(std::chrono::milliseconds duration) {
        auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration % std::chrono::hours(1));
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration % std::chrono::minutes(1));
        auto ms = duration % std::chrono::seconds(1);
        
        std::stringstream ss;
        if (hours.count() > 0) {
            ss << hours.count() << "h ";
        }
        if (minutes.count() > 0) {
            ss << minutes.count() << "m ";
        }
        if (seconds.count() > 0) {
            ss << seconds.count() << "s ";
        }
        if (ms.count() > 0 && hours.count() == 0) {
            ss << ms.count() << "ms";
        }
        
        std::string result = ss.str();
        if (!result.empty() && result.back() == ' ') {
            result.pop_back();
        }
        return result.empty() ? "0ms" : result;
    }
}

// Utilidades de validación
namespace ValidationUtils {
    inline bool isValidIPAddress(const std::string& ip) {
        // Implementación básica de validación de IP
        return !ip.empty() && ip != "0.0.0.0";
    }
    
    inline bool isValidPort(int port) {
        return port > 0 && port <= 65535;
    }
    
    inline bool isValidTagName(const std::string& name) {
        return !name.empty() && name.size() <= 64;
    }
    
    inline bool isValidVariableName(const std::string& name) {
        return !name.empty() && name.size() <= 32;
    }
}

// Configuración de variables de entorno
namespace EnvironmentConfig {
    inline void initializeFromEnvironment() {
        // Configurar logging basado en variables de entorno
        if (std::getenv("VERBOSE_DEBUG")) {
            LogConfig::verbose_debug = true;
        }
        
        if (std::getenv("SILENT_MODE")) {
            LogConfig::silent_mode = true;
        }
        
        if (std::getenv("NO_COLORS")) {
            LogConfig::use_colors = false;
        }
    }
}

// Definiciones de variables globales (implementar en common.cpp)
namespace LogConfig {
    std::mutex log_mutex;
    bool verbose_debug = false;
    bool silent_mode = false;
    bool use_colors = true;
}

#endif // COMMON_H