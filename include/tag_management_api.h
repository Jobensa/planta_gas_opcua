/*
 * tag_management_api.h - API HTTP para gestión dinámica de tags
 * 
 * Integra servidor HTTP embebido en TagManager para:
 * - CRUD de tags
 * - Gestión TBL_OPCUA
 * - Hot reload
 * - Backup/Restore
 */

#ifndef TAG_MANAGEMENT_API_H
#define TAG_MANAGEMENT_API_H

#include "tag_manager.h"
#include "common.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <thread>
#include <mutex>
#include <fstream>
#include <filesystem>

// Usaremos httplib (header-only library)
// Agregar a CMakeLists.txt: 
// find_package(httplib REQUIRED)
// target_link_libraries(planta_gas httplib::httplib)
#include <httplib.h>

namespace TagManagementAPI {

// Estructura para respuestas API estandarizadas
struct APIResponse {
    bool success;
    std::string message;
    nlohmann::json data;
    int status_code;
    
    APIResponse(bool s = true, const std::string& msg = "OK", int code = 200)
        : success(s), message(msg), status_code(code) {}
    
    nlohmann::json toJSON() const {
        return {
            {"success", success},
            {"message", message},
            {"data", data},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }
    
    static APIResponse Success(const nlohmann::json& data = {}, const std::string& msg = "Operation successful") {
        APIResponse resp(true, msg, 200);
        resp.data = data;
        return resp;
    }
    
    static APIResponse Error(const std::string& msg, int code = 400) {
        return APIResponse(false, msg, code);
    }
};

// Clase principal para el servidor de gestión de tags
class TagManagementServer {
private:
    std::shared_ptr<TagManager> tag_manager_;
    std::unique_ptr<httplib::Server> http_server_;
    std::unique_ptr<std::thread> server_thread_;
    
    std::string config_file_path_;
    std::string backup_directory_;
    std::atomic<bool> server_running_;
    int server_port_;
    
    mutable std::mutex api_mutex_;
    
    // Configuración del servidor
    struct ServerConfig {
        int port = 8081;
        std::string bind_address = "0.0.0.0";
        bool enable_cors = true;
        int max_connections = 100;
        int timeout_seconds = 30;
    } server_config_;

public:
    explicit TagManagementServer(std::shared_ptr<TagManager> tag_manager, 
                                const std::string& config_file);
    ~TagManagementServer();
    
    // Lifecycle del servidor
    bool startServer(int port = 8081);
    void stopServer();
    bool isRunning() const { return server_running_; }
    
    // Configuración inicial
    void setupRoutes();
    void configureCORS();
    void configureMiddleware();
    
private:
    // === TAG CRUD OPERATIONS ===
    
    // GET /api/tags - Listar todos los tags
    void handleGetAllTags(const httplib::Request& req, httplib::Response& res);
    
    // GET /api/tags/{name} - Obtener tag específico
    void handleGetTag(const httplib::Request& req, httplib::Response& res);
    
    // POST /api/tags - Crear nuevo tag
    void handleCreateTag(const httplib::Request& req, httplib::Response& res);
    
    // PUT /api/tags/{name} - Actualizar tag existente
    void handleUpdateTag(const httplib::Request& req, httplib::Response& res);
    
    // DELETE /api/tags/{name} - Borrar tag
    void handleDeleteTag(const httplib::Request& req, httplib::Response& res);
    
    // === TBL_OPCUA MANAGEMENT ===
    
    // GET /api/opcua-table - Ver mapeo completo TBL_OPCUA
    void handleGetOPCUATable(const httplib::Request& req, httplib::Response& res);
    
    // PUT /api/opcua-table/{index} - Asignar variable a índice específico
    void handleAssignOPCUAIndex(const httplib::Request& req, httplib::Response& res);
    
    // DELETE /api/opcua-table/{index} - Remover variable de índice
    void handleRemoveOPCUAIndex(const httplib::Request& req, httplib::Response& res);
    
    // GET /api/opcua-table/available - Índices disponibles
    void handleGetAvailableOPCUAIndices(const httplib::Request& req, httplib::Response& res);
    
    // === CONFIGURATION MANAGEMENT ===
    
    // GET /api/config - Obtener configuración completa
    void handleGetConfiguration(const httplib::Request& req, httplib::Response& res);
    
    // POST /api/config/save - Guardar configuración actual
    void handleSaveConfiguration(const httplib::Request& req, httplib::Response& res);
    
    // POST /api/config/load - Cargar configuración desde archivo
    void handleLoadConfiguration(const httplib::Request& req, httplib::Response& res);
    
    // POST /api/config/reload - Hot reload configuración
    void handleReloadConfiguration(const httplib::Request& req, httplib::Response& res);
    
    // === BACKUP/RESTORE SYSTEM ===
    
    // GET /api/backups - Listar archivos de backup
    void handleGetBackups(const httplib::Request& req, httplib::Response& res);
    
    // POST /api/backups/create - Crear backup manual
    void handleCreateBackup(const httplib::Request& req, httplib::Response& res);
    
    // POST /api/backups/restore/{filename} - Restaurar desde backup
    void handleRestoreBackup(const httplib::Request& req, httplib::Response& res);
    
    // DELETE /api/backups/{filename} - Borrar backup
    void handleDeleteBackup(const httplib::Request& req, httplib::Response& res);
    
    // === VALIDATION & PREVIEW ===
    
    // POST /api/validate - Validar configuración
    void handleValidateConfiguration(const httplib::Request& req, httplib::Response& res);
    
    // GET /api/preview/opcua - Preview estructura OPC UA
    void handlePreviewOPCUA(const httplib::Request& req, httplib::Response& res);
    
    // POST /api/preview/tag - Preview de tag antes de crear
    void handlePreviewTag(const httplib::Request& req, httplib::Response& res);
    
    // === SYSTEM STATUS & STATISTICS ===
    
    // GET /api/status - Estado del sistema
    void handleGetSystemStatus(const httplib::Request& req, httplib::Response& res);
    
    // GET /api/statistics - Estadísticas en tiempo real
    void handleGetStatistics(const httplib::Request& req, httplib::Response& res);
    
    // GET /api/health - Health check
    void handleHealthCheck(const httplib::Request& req, httplib::Response& res);
    
    // === TEMPLATE MANAGEMENT ===
    
    // GET /api/templates - Obtener plantillas de tags
    void handleGetTemplates(const httplib::Request& req, httplib::Response& res);
    
    // POST /api/templates/{type} - Crear tag desde plantilla
    void handleCreateFromTemplate(const httplib::Request& req, httplib::Response& res);

private:
    // === UTILITY FUNCTIONS ===
    
    // Respuesta estándar
    void sendResponse(httplib::Response& res, const APIResponse& response);
    void sendJSONResponse(httplib::Response& res, const nlohmann::json& data, int status = 200);
    void sendErrorResponse(httplib::Response& res, const std::string& error, int status = 400);
    
    // Validación de requests
    bool validateJSON(const std::string& json_str, nlohmann::json& parsed);
    bool validateTagName(const std::string& name);
    bool validateOPCUAIndex(int index);
    
    // Backup system
    bool createAutomaticBackup(const std::string& operation);
    std::string generateBackupFilename(const std::string& prefix = "auto");
    std::vector<std::string> listBackupFiles();
    bool cleanOldBackups(int max_backups = 20);
    
    // Configuration helpers
    bool saveConfigurationToFile(const std::string& filepath);
    bool loadConfigurationFromFile(const std::string& filepath);
    nlohmann::json generateConfigurationJSON();
    
    // Tag operations
    bool createTagInternal(const nlohmann::json& tag_config);
    bool updateTagInternal(const std::string& tag_name, const nlohmann::json& updates);
    bool deleteTagInternal(const std::string& tag_name);
    
    // TBL_OPCUA helpers
    nlohmann::json buildOPCUATableStatus();
    std::vector<int> getAvailableOPCUAIndices();
    bool assignVariableToOPCUAIndex(int index, const std::string& tag_name, const std::string& var_name);
    
    // Validation
    std::vector<std::string> validateTagConfiguration(const nlohmann::json& tag_config);
    std::vector<std::string> validateSystemConfiguration();
    
    // Preview generation
    nlohmann::json generateOPCUAStructurePreview();
    nlohmann::json generateTagPreview(const nlohmann::json& tag_config);
    
    // Templates
    nlohmann::json getTagTemplates();
    nlohmann::json createTagFromTemplate(const std::string& template_type, const nlohmann::json& params);
    
    // Logging específico para API
    void logAPICall(const std::string& method, const std::string& path, const std::string& client_ip);
    void logAPIError(const std::string& operation, const std::string& error);
    void logAPISuccess(const std::string& operation, const std::string& details = "");
    
    // Threading y sincronización
    void serverThreadFunction();
    void gracefulShutdown();
    
    // Constants
    static constexpr int MAX_BACKUP_FILES = 50;
    static constexpr size_t MAX_REQUEST_SIZE = 10 * 1024 * 1024; // 10MB
    static constexpr int DEFAULT_TIMEOUT_SECONDS = 30;
};

// === INTEGRATION WITH EXISTING TAGMANAGER ===

// Extensión del TagManager para incluir API
class TagManagerWithAPI : public TagManager {
private:
    std::unique_ptr<TagManagementServer> api_server_;
    bool api_enabled_;
    std::string config_file_path_;

public:
    TagManagerWithAPI(const std::string& config_file) 
        : TagManager(config_file), api_enabled_(false), config_file_path_(config_file) {}
    
    // Inicializar API server
    bool enableAPI(int port = 8081) {
        try {
            api_server_ = std::make_unique<TagManagementServer>(
                std::shared_ptr<TagManager>(this, [](TagManager*){}), // No-op deleter
                config_file_path_
            );
            
            if (api_server_->startServer(port)) {
                api_enabled_ = true;
                LOG_SUCCESS("API de gestión de tags iniciada en puerto " + std::to_string(port));
                LOG_INFO("Acceso web: http://localhost:" + std::to_string(port));
                return true;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error al inicializar API: " + std::string(e.what()));
        }
        return false;
    }
    
    void disableAPI() {
        if (api_server_ && api_enabled_) {
            api_server_->stopServer();
            api_enabled_ = false;
            LOG_INFO("API de gestión de tags detenida");
        }
    }
    
    bool isAPIEnabled() const { return api_enabled_; }
    
    // Override stop para incluir API
    void stop() {
        disableAPI();
        TagManager::stop();
    }
    
    // Acceso al servidor API (para testing/debugging)
    TagManagementServer* getAPIServer() { return api_server_.get(); }
};

// === FACTORY FUNCTIONS ===

// Crear TagManager con API integrada
std::unique_ptr<TagManagerWithAPI> createTagManagerWithAPI(const std::string& config_file, int api_port = 8081);

// Crear servidor API standalone
std::unique_ptr<TagManagementServer> createTagManagementServer(
    std::shared_ptr<TagManager> tag_manager, 
    const std::string& config_file
);

} // namespace TagManagementAPI

#endif // TAG_MANAGEMENT_API_H