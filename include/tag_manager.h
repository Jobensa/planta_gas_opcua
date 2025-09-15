#ifndef TAG_MANAGER_H
#define TAG_MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <bitset>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

// Forward declarations
class PACControlClient;
class OPCUAServer;

enum class PollingGroup {
    FAST = 0,   // 250ms - PV, alarmas activas
    MEDIUM = 1, // 2s - SP, CV, auto_manual  
    SLOW = 2    // 30s - configuración, límites, PID
};

enum class VariableType {
    FLOAT_TYPE,
    INT32_TYPE,
    BOOL_TYPE
};

struct TagVariable {
    std::string name;
    VariableType type;
    bool writable;
    PollingGroup polling_group;
    std::string description;
    
    // Valores actuales
    float float_value = 0.0f;
    int32_t int_value = 0;
    bool bool_value = false;
    
    // Control de cambios
    float last_float_value = 0.0f;
    int32_t last_int_value = 0;
    bool last_bool_value = false;
    bool has_changed = false;
    
    std::chrono::time_point<std::chrono::steady_clock> last_update;
};

struct IndustrialTag {
    std::string name;
    std::string value_table;     // TBL_TB_FT_1601
    std::string alarm_table;     // TBL_TA_FT_1601  
    std::string description;
    std::string units;
    PollingGroup primary_polling_group;
    
    // Variables organizadas por nombre
    std::unordered_map<std::string, TagVariable> variables;
    std::unordered_map<std::string, TagVariable> alarms;
    
    // Control de actualizaciones por grupo
    std::chrono::time_point<std::chrono::steady_clock> last_fast_update;
    std::chrono::time_point<std::chrono::steady_clock> last_medium_update;  
    std::chrono::time_point<std::chrono::steady_clock> last_slow_update;
    
    // Bitset para tracking eficiente de cambios (hasta 64 variables por tag)
    std::bitset<64> changes_mask;
    
    // Estadísticas
    uint64_t total_updates = 0;
    uint64_t fast_updates = 0;
    uint64_t medium_updates = 0;
    uint64_t slow_updates = 0;
};

class TagManager {
public:
    explicit TagManager(const std::string& config_file);
    ~TagManager();
    
    // Gestión del ciclo de vida
    bool initialize();
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Configuración y carga
    bool loadConfiguration(const std::string& config_file);
    bool validateConfiguration() const;
    
    // Acceso a tags y variables
    const std::vector<IndustrialTag>& getTags() const { return tags_; }
    const IndustrialTag* getTag(const std::string& tag_name) const;
    TagVariable* getVariable(const std::string& tag_name, const std::string& var_name);
    const TagVariable* getVariable(const std::string& tag_name, const std::string& var_name) const;
    
    // Operaciones de lectura/escritura
    bool writeVariable(const std::string& tag_name, const std::string& var_name, 
                      const nlohmann::json& value);
    nlohmann::json readVariable(const std::string& tag_name, const std::string& var_name) const;
    
    // Batch operations para eficiencia
    std::vector<std::string> getFastPollingVariables() const;
    std::vector<std::string> getMediumPollingVariables() const;
    std::vector<std::string> getSlowPollingVariables() const;
    
    // Actualización masiva de variables
    bool updateVariablesFromPAC(const std::unordered_map<std::string, nlohmann::json>& pac_data);
    
    // Estadísticas y monitoreo
    struct Statistics {
        uint64_t total_tags = 0;
        uint64_t total_variables = 0;
        uint64_t fast_polling_vars = 0;
        uint64_t medium_polling_vars = 0;
        uint64_t slow_polling_vars = 0;
        uint64_t writable_variables = 0;
        uint64_t readonly_variables = 0;
        uint64_t total_updates = 0;
        uint64_t successful_updates = 0;
        uint64_t failed_updates = 0;
        double avg_fast_latency_ms = 0.0;
        double avg_medium_latency_ms = 0.0;
        double avg_slow_latency_ms = 0.0;
        std::chrono::time_point<std::chrono::steady_clock> start_time;
    };
    
    const Statistics& getStatistics() const { return stats_; }
    void resetStatistics();
    std::string getStatisticsReport() const;
    
    // Configuración dinámica
    bool setPollingInterval(PollingGroup group, std::chrono::milliseconds interval);
    std::chrono::milliseconds getPollingInterval(PollingGroup group) const;
    bool setChangeThreshold(float threshold);
    float getChangeThreshold() const { return change_threshold_; }

private:
    // Hilos de polling
    void fastPollingThread();
    void mediumPollingThread();
    void slowPollingThread();
    
    // Funciones auxiliares de polling
    void updatePollingGroup(PollingGroup group);
    bool shouldUpdateVariable(const TagVariable& var, PollingGroup group) const;
    bool hasVariableChanged(const TagVariable& var) const;
    void markVariableUpdated(TagVariable& var);
    
    // Comunicación con PAC y OPC UA
    bool updateVariableFromPAC(IndustrialTag& tag, TagVariable& var);
    bool sendVariableToPAC(const IndustrialTag& tag, const TagVariable& var);
    bool updateOPCUANode(const std::string& node_id, const TagVariable& var);
    
    // Gestión de configuración
    bool parseTagFromJSON(const nlohmann::json& tag_json, IndustrialTag& tag);
    bool parseVariableFromJSON(const nlohmann::json& var_json, TagVariable& var);
    std::string buildNodeId(const std::string& tag_name, const std::string& var_name) const;
    std::string buildPACTablePath(const std::string& table_name, const std::string& var_name) const;
    
    // Logging y debugging
    void logPollingActivity(PollingGroup group, size_t variables_updated);
    void logVariableChange(const std::string& tag_name, const std::string& var_name, 
                          const std::string& old_value, const std::string& new_value);
    void logError(const std::string& operation, const std::string& details);
    
    // Miembros privados
    std::vector<IndustrialTag> tags_;
    std::unordered_map<std::string, size_t> tag_index_map_; // Para búsqueda rápida
    
    // Configuración
    nlohmann::json config_;
    std::string pac_ip_;
    int pac_port_;
    int opcua_port_;
    std::string server_name_;
    float change_threshold_;
    
    // Intervalos de polling
    std::chrono::milliseconds fast_interval_;
    std::chrono::milliseconds medium_interval_;
    std::chrono::milliseconds slow_interval_;
    
    // Control de hilos
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> fast_thread_;
    std::unique_ptr<std::thread> medium_thread_;
    std::unique_ptr<std::thread> slow_thread_;
    
    // Sincronización
    mutable std::mutex tags_mutex_;
    mutable std::mutex stats_mutex_;
    
    // Clientes externos
    std::shared_ptr<PACControlClient> pac_client_;
    std::shared_ptr<OPCUAServer> opcua_server_;
    
    // Estadísticas
    mutable Statistics stats_;
    
    // Constantes
    static constexpr size_t MAX_VARIABLES_PER_TAG = 64;
    static constexpr float DEFAULT_CHANGE_THRESHOLD = 0.01f; // 1%
    static constexpr std::chrono::milliseconds DEFAULT_FAST_INTERVAL{250};
    static constexpr std::chrono::milliseconds DEFAULT_MEDIUM_INTERVAL{2000};
    static constexpr std::chrono::milliseconds DEFAULT_SLOW_INTERVAL{30000};
};

// Funciones auxiliares inline para conversiones
inline std::string pollingGroupToString(PollingGroup group) {
    switch (group) {
        case PollingGroup::FAST: return "fast";
        case PollingGroup::MEDIUM: return "medium";
        case PollingGroup::SLOW: return "slow";
        default: return "unknown";
    }
}

inline PollingGroup stringToPollingGroup(const std::string& str) {
    if (str == "fast") return PollingGroup::FAST;
    if (str == "medium") return PollingGroup::MEDIUM;
    if (str == "slow") return PollingGroup::SLOW;
    return PollingGroup::MEDIUM; // default
}

inline std::string variableTypeToString(VariableType type) {
    switch (type) {
        case VariableType::FLOAT_TYPE: return "FLOAT";
        case VariableType::INT32_TYPE: return "INT32";
        case VariableType::BOOL_TYPE: return "BOOL";
        default: return "UNKNOWN";
    }
}

inline VariableType stringToVariableType(const std::string& str) {
    if (str == "FLOAT") return VariableType::FLOAT_TYPE;
    if (str == "INT32") return VariableType::INT32_TYPE;
    if (str == "BOOL") return VariableType::BOOL_TYPE;
    return VariableType::FLOAT_TYPE; // default
}

#endif // TAG_MANAGER_H