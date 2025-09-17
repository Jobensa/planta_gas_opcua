/*
 * pac_control_client.h - Cliente PAC adaptado para planta_gas
 * 
 * Adaptado para trabajar con TagManagerWithAPI y TBL_OPCUA optimizada
 */

#ifndef PAC_CONTROL_CLIENT_H
#define PAC_CONTROL_CLIENT_H

#include <string>
#include <memory>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>

// Forward declarations
class TagManager;

class PACControlClient {
private:
    // Referencia al TagManager (ahora shared_ptr compatible)
    std::shared_ptr<TagManager> tag_manager_;
    
    // Configuración de conexión
    std::string pac_ip_;
    int pac_port_;
    std::string username_;
    std::string password_;
    int timeout_ms_;
    
    // Estado de conexión
    std::atomic<bool> connected_;
    std::atomic<bool> enabled_;
    
    // CURL handles para eficiencia
    CURL* curl_handle_;
    mutable std::mutex curl_mutex_;
    
    // Cache para TBL_OPCUA (optimización crítica)
    std::vector<float> opcua_table_cache_;
    std::chrono::time_point<std::chrono::steady_clock> last_opcua_read_;
    
    // Mapeo de tags a índices de TBL_OPCUA (cargado desde configuración)
    std::unordered_map<std::string, int> tag_opcua_index_map_;
    
    // Estadísticas
    struct ClientStats {
        uint64_t successful_reads = 0;
        uint64_t failed_reads = 0;
        uint64_t successful_writes = 0;
        uint64_t failed_writes = 0;
        uint64_t opcua_table_reads = 0;
        double avg_response_time_ms = 0.0;
        std::chrono::time_point<std::chrono::steady_clock> last_success;
    } stats_;

public:
    // Constructor adaptado para shared_ptr (nueva versión)
    explicit PACControlClient(std::shared_ptr<TagManager> tag_manager);
    
    // Constructor de compatibilidad (versión antigua)
    explicit PACControlClient(TagManager* tag_manager);
    
    ~PACControlClient();
    
    // Gestión de conexión
    bool connect();
    void disconnect();
    bool isConnected() const { return connected_; }
    bool isEnabled() const { return enabled_; }
    
    // Configuración
    void setConnectionParams(const std::string& ip, int port);
    void setCredentials(const std::string& username, const std::string& password);
    void setTimeout(int timeout_ms) { timeout_ms_ = timeout_ms; }
    
    // Operaciones principales
    
    // OPTIMIZACIÓN CRÍTICA: Leer tabla completa TBL_OPCUA
    bool readOPCUATable();
    
    // Lectura individual de variables (para variables no críticas)
    std::string readVariable(const std::string& table_name, int index);
    float readFloatVariable(const std::string& table_name, int index);
    int readIntVariable(const std::string& table_name, int index);
    
    // Escritura de variables
    bool writeVariable(const std::string& table_name, int index, float value);
    bool writeVariable(const std::string& table_name, int index, int value);
    
    // Lectura de tabla completa (genérica)
    std::vector<float> readFloatTable(const std::string& table_name);
    std::vector<int> readIntTable(const std::string& table_name);
    
    // Estadísticas
    const ClientStats& getStats() const { return stats_; }
    std::string getStatsReport() const;
    void resetStats();

private:
    // Inicialización
    bool initializeCURL();
    void cleanupCURL();
    
    // Comunicación HTTP
    std::string performHTTPRequest(const std::string& url);
    std::string performHTTPPOST(const std::string& url, const std::string& data);
    
    // URLs específicas del PAC
    std::string buildReadURL(const std::string& table_name, int index = -1) const;
    std::string buildWriteURL(const std::string& table_name, int index) const;
    std::string buildOPCUATableURL() const;
    
    // Procesamiento de respuestas
    bool parseFloatResponse(const std::string& response, float& value);
    bool parseIntResponse(const std::string& response, int& value);
    bool parseFloatArrayResponse(const std::string& response, std::vector<float>& values);
    
    // Optimización TBL_OPCUA
    bool updateTagManagerFromOPCUATable();
    int getTagOPCUATableIndex(const std::string& tag_name) const;
    bool loadTagOPCUAMapping(const std::string& config_file);
    
    // Utilidades
    void updateStats(bool success, double response_time_ms);
    void logError(const std::string& operation, const std::string& details);
    void logSuccess(const std::string& operation, const std::string& details = "");
    
    // Callback para CURL
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data);
};

#endif // PAC_CONTROL_CLIENT_H