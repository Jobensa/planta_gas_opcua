/*
 * opcua_server.h - Servidor OPC-UA adaptado para TagManager con API
 * 
 * Adaptado desde tu código original para trabajar con:
 * - TagManagerWithAPI
 * - Estructura jerárquica de tags
 * - Nombres limpios (sin TB_)
 * - TBL_OPCUA optimization
 */

#ifndef OPCUA_SERVER_H
#define OPCUA_SERVER_H

#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/plugin/log_stdout.h>
#include <memory>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Forward declarations
class TagManager;
class Tag;

class OPCUAServer {
private:
    // Servidor OPC UA
    UA_Server* ua_server_;
    UA_ServerConfig* server_config_;
    std::unique_ptr<std::thread> server_thread_;
    std::atomic<bool> running_;
    int server_port_;
    
    // Integración con TagManager
    std::shared_ptr<TagManager> tag_manager_;
    
    // Configuración jerárquica de tags
    nlohmann::json tag_config_;
    
    // Mapeo de nodos OPC UA
    std::unordered_map<std::string, UA_NodeId> node_map_;
    std::unordered_map<std::string, std::string> opcua_to_internal_name_map_;
    
    // Callback timer para updates
    UA_UInt64 callback_id_;
    
public:
    // Constructor adaptado para nueva arquitectura
    explicit OPCUAServer(std::shared_ptr<TagManager> tag_manager);
    ~OPCUAServer();
    
    // Lifecycle del servidor
    bool start(int port = 4840);
    void stop();
    bool isRunning() const { return running_; }
    
    // Configuración del servidor
    bool setupServerConfiguration(int port);
    bool createOPCUAStructure();
    bool registerUpdateCallback();
    
    // Configuración de tags jerárquicos
    void setTagConfiguration(const nlohmann::json& config);
    
    // === ACTUALIZACIÓN DE VARIABLES PÚBLICAS ===
    
    // Actualizar tags específicos cuando cambien (llamado desde TagManager)
    void updateSpecificTag(std::shared_ptr<Tag> tag);
    void updateTagsFromPAC(); // Solo para datos recientes del PAC
    
private:
    
    // === CREACIÓN DE ESTRUCTURA OPC UA ===
    
    // Crear estructura jerárquica
    UA_NodeId createFolderNode(const UA_NodeId& parent, const std::string& folder_name, 
                              const std::string& display_name, bool is_tag_object = false);
    
    // Crear nodos de tags y variables
    bool createTagNodes();
    bool createInstrumentTagNode(std::shared_ptr<Tag> tag, const UA_NodeId& parent_folder);
    bool createPIDTagNode(std::shared_ptr<Tag> tag, const UA_NodeId& parent_folder);
    
    // Versiones con nombre de tag padre específico (sobrecargas)
    bool createInstrumentTagNode(std::shared_ptr<Tag> tag, const UA_NodeId& parent_folder, const std::string& parent_tag_name);
    bool createPIDTagNode(std::shared_ptr<Tag> tag, const UA_NodeId& parent_folder, const std::string& parent_tag_name);
    
    // Crear variables individuales
    UA_NodeId createVariableNode(const UA_NodeId& parent, const std::string& variable_name,
                                 std::shared_ptr<Tag> tag);
    
    // Crear alarmas (placeholder por ahora)
    UA_NodeId createAlarmNode(const UA_NodeId& parent, const std::string& alarm_name,
                             std::shared_ptr<Tag> tag);
    
    // Función auxiliar para crear variable de prueba simple
    bool createSimpleTestVariable(const UA_NodeId& parent_folder);
    
    // === CALLBACK SYSTEM ===
    
    // 🔧 FUNCIÓN PERFECTA PARA DETECTAR ORIGEN DE ESCRITURA
    bool isWriteFromClient(const UA_NodeId *sessionId);
    
// Callback de escritura desde clientes OPC UA (static for OPC UA C API)
static void writeCallback(UA_Server* server, const UA_NodeId* sessionId,
                                  void* sessionContext, const UA_NodeId* nodeId,
                                  void* nodeContext, const UA_NumericRange* range,
                                  const UA_DataValue* data);

// Internal write handler (non-static)
void handleWrite(UA_Server* server, const UA_NodeId* sessionId,
                void* sessionContext, const UA_NodeId* nodeId,
                void* nodeContext, const UA_NumericRange* range,
                const UA_DataValue* data);
    
    // === MANEJO DE DATOS ===
    
    // Actualizar valores desde TagManager
    void updateAllVariables();
    void updateTagVariables(std::shared_ptr<Tag> tag);
    static void staticUpdateCallback(UA_Server* server, void* data);
    
    // Escribir valores al TagManager (desde clientes OPC UA)
    bool writeVariableToTagManager(const std::string& node_path, const UA_Variant& value);
    
    // === UTILIDADES ===
    
    // Mapeo de nombres
    std::string buildNodePath(const std::string& tag_opcua_name, const std::string& variable_name);
    std::string getInternalTagName(const std::string& opcua_name);
    std::string getFolderForTag(const std::string& tag_opcua_name);
    std::string categorizeTagByName(const std::string& tag_name);
    
    // Conversión de tipos
    UA_Variant convertTagToUAVariant(std::shared_ptr<Tag> tag);
    bool convertUAVariantToTag(const UA_Variant& ua_value, std::shared_ptr<Tag> tag);
    
    // Logging específico
    void logOPCUAOperation(const std::string& operation, const std::string& details);
    void logOPCUAError(const std::string& operation, const std::string& error);
    
    // Thread management
    void serverThreadFunction();
    
    // === CONFIGURACIÓN DE ESTRUCTURA JERÁRQUICA ===
    
    struct FolderInfo {
        UA_NodeId folder_id;
        std::string display_name;
    };
    
    // Crear estructura de carpetas organizadas
    bool createOrganizedFolders();
    std::unordered_map<std::string, FolderInfo> folder_map_;
    
    // Namespace dinámico
    UA_UInt16 namespace_index_;
    
    // Constantes
    static constexpr UA_UInt32 UPDATE_INTERVAL_MS = 1000; // 1 segundo
    static constexpr const char* APPLICATION_URI = "urn:PlantaGas:SCADA:Server";
    static constexpr const char* APPLICATION_NAME = "Planta Gas SCADA Server";
};

#endif // OPCUA_SERVER_H