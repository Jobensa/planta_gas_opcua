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

// Forward declarations
class TagManager;
struct IndustrialTag;
struct TagVariable;

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
    
private:
    // === CREACIÓN DE ESTRUCTURA OPC UA ===
    
    // Crear estructura jerárquica
    UA_NodeId createFolderNode(const UA_NodeId& parent, const std::string& folder_name, 
                              const std::string& display_name);
    
    // Crear nodos de tags y variables
    bool createTagNodes();
    bool createInstrumentTagNode(const IndustrialTag& tag, const UA_NodeId& parent_folder);
    bool createPIDTagNode(const IndustrialTag& tag, const UA_NodeId& parent_folder);
    
    // Crear variables individuales
    UA_NodeId createVariableNode(const UA_NodeId& parent, const std::string& variable_name,
                                 const TagVariable& variable, const std::string& tag_name);
    
    // Crear alarmas
    UA_NodeId createAlarmNode(const UA_NodeId& parent, const std::string& alarm_name,
                             const TagVariable& alarm, const std::string& tag_name);
    
    // === CALLBACK SYSTEM ===
    
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
    void updateTagVariables(const IndustrialTag& tag);
    void updateSingleVariable(const std::string& node_path, const TagVariable& variable);
    static void staticUpdateCallback(UA_Server* server, void* data);
    
    // Escribir valores al TagManager (desde clientes OPC UA)
    bool writeVariableToTagManager(const std::string& node_path, const UA_Variant& value);
    
    // === UTILIDADES ===
    
    // Mapeo de nombres
    std::string buildNodePath(const std::string& tag_opcua_name, const std::string& variable_name);
    std::string getInternalTagName(const std::string& opcua_name);
    std::string getFolderForTag(const std::string& tag_opcua_name);
    
    // Conversión de tipos
    UA_Variant convertTagVariableToUAVariant(const TagVariable& variable);
    bool convertUAVariantToTagVariable(const UA_Variant& ua_value, TagVariable& variable);
    
    // Logging específico
    void logOPCUAOperation(const std::string& operation, const std::string& details);
    void logOPCUAError(const std::string& operation, const std::string& error);
    
    // Thread management
    void serverThreadFunction();
    
    // === CONFIGURACIÓN DE ESTRUCTURA JERÁRQUICA ===
    
    struct FolderStructure {
        UA_NodeId folder_id;
        std::string display_name;
        std::vector<std::string> tag_patterns; // Para determinar qué tags van aquí
    };
    
    // Crear estructura de carpetas organizadas
    bool createOrganizedFolders();
    std::unordered_map<std::string, FolderStructure> folder_map_;
    
    // Constantes
    static constexpr UA_UInt32 UPDATE_INTERVAL_MS = 1000; // 1 segundo
    static constexpr UA_UInt16 NAMESPACE_INDEX = 1;
    static constexpr const char* APPLICATION_URI = "urn:PlantaGas:SCADA:Server";
    static constexpr const char* APPLICATION_NAME = "Planta Gas SCADA Server";
};

#endif // OPCUA_SERVER_H