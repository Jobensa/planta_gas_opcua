/*
 * pac_control_client.h - Cliente PAC adaptado para planta_gas
 * 
 * Adaptado para trabajar con TagManagerWithAPI y TBL_OPCUA optimizada
 */

#ifndef PAC_CONTROL_CLIENT_H
#define PAC_CONTROL_CLIENT_H

#include <string>
#include <memory>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>

// Forward declarations
class TagManager;

class PACControlClient {
public:
    // Estadísticas
    struct ClientStats {
        uint64_t successful_reads = 0;
        uint64_t failed_reads = 0;
        uint64_t successful_writes = 0;
        uint64_t failed_writes = 0;
        uint64_t opcua_table_reads = 0;
        double avg_response_time_ms = 0.0;
        std::chrono::time_point<std::chrono::steady_clock> last_success;
    };

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
    
    // Socket TCP para protocolo MMP de Opto 22
    int socket_fd_;
    mutable std::mutex socket_mutex_;
    
    // Cache para TBL_OPCUA (optimización crítica)
    std::vector<float> opcua_table_cache_;
    std::chrono::time_point<std::chrono::steady_clock> last_opcua_read_;
    
    // Mapeo de tags a índices de TBL_OPCUA (cargado desde configuración)
    std::unordered_map<std::string, int> tag_opcua_index_map_;
    ClientStats stats_;

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
    
    // OPTIMIZACIÓN CRÍTICA: Leer tabla completa TBL_OPCUA usando protocolo MMP
    bool readOPCUATable();
    
    // NUEVA ESTRATEGIA: Leer tablas individuales con datos reales
    bool readIndividualTables();
    
    // Lectura de tablas usando protocolo MMP de Opto 22
    std::vector<float> readFloatTable(const std::string& table_name, int start_pos = 0, int end_pos = 9);
    std::vector<int32_t> readInt32Table(const std::string& table_name, int start_pos = 0, int end_pos = 4);
    
    // Lectura de variables individuales usando protocolo MMP
    float readSingleFloatVariableByTag(const std::string& tag_name);
    int32_t readSingleInt32VariableByTag(const std::string& tag_name);
    
    // Escritura de variables usando protocolo MMP
    bool writeFloatTableIndex(const std::string& table_name, int index, float value);
    bool writeInt32TableIndex(const std::string& table_name, int index, int32_t value);
    bool writeSingleFloatVariable(const std::string& variable_name, float value);
    bool writeSingleInt32Variable(const std::string& variable_name, int32_t value);
    
    // Estadísticas
    const ClientStats& getStats() const;
    std::string getStatsReport() const;
    void resetStats();

private:
    // Inicialización del socket TCP
    bool initializeSocket();
    void cleanupSocket();
    
    // Comunicación TCP usando protocolo MMP de Opto 22
    bool sendCommand(const std::string& command);
    std::vector<uint8_t> receiveData(size_t expected_bytes);
    std::vector<uint8_t> receiveASCIIResponse();
    bool receiveWriteConfirmation();
    
    // Conversión de datos del protocolo MMP
    std::vector<float> convertBytesToFloats(const std::vector<uint8_t>& data);
    std::vector<int32_t> convertBytesToInt32s(const std::vector<uint8_t>& data);
    std::string convertBytesToASCII(const std::vector<uint8_t>& bytes);
    
    // Utilidades del protocolo
    void flushSocketBuffer();
    bool validateDataIntegrity(const std::vector<uint8_t>& data, const std::string& table_name);
    std::string cleanASCIINumber(const std::string& ascii_str);
    float convertStringToFloat(const std::string& str);
    int32_t convertStringToInt32(const std::string& str);
    
    // Optimización TBL_OPCUA
    bool updateTagManagerFromOPCUATable();
    bool updateTagManagerFromIndividualTable(const std::string& table_name, const std::vector<float>& values);
    int getTagOPCUATableIndex(const std::string& tag_name) const;
    bool loadTagOPCUAMapping(const std::string& config_file);
    
    // Modo simulación temporal
    std::vector<float> generateSimulatedData(size_t num_values);
    
    // Utilidades
    void updateStats(bool success, double response_time_ms);
    void logError(const std::string& operation, const std::string& details);
    void logSuccess(const std::string& operation, const std::string& details = "");
    
    // Callback para CURL
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data);
};

#endif // PAC_CONTROL_CLIENT_H