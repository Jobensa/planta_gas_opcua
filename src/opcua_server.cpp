/*
 * opcua_server.cpp - Implementación del servidor OPC-UA adaptado
 * 
 * Adaptado desde tu implementación original para nueva arquitectura
 */

#include "opcua_server.h"
#include "tag_manager.h"
#include "common.h"

OPCUAServer::OPCUAServer(std::shared_ptr<TagManager> tag_manager)
    : ua_server_(nullptr)
    , server_config_(nullptr)
    , running_(false)
    , server_port_(4840)
    , tag_manager_(tag_manager)
    , callback_id_(0)
{
    LOG_INFO("🏗️ OPCUAServer inicializado con TagManager integrado");
}

OPCUAServer::~OPCUAServer() {
    stop();
}

bool OPCUAServer::start(int port) {
    if (running_) {
        LOG_WARNING("Servidor OPC UA ya está ejecutándose en puerto " + std::to_string(server_port_));
        return true;
    }
    
    server_port_ = port;
    
    try {
        // Crear servidor
        ua_server_ = UA_Server_new();
        if (!ua_server_) {
            LOG_ERROR("Error al crear servidor OPC UA");
            return false;
        }
        
        // Configurar servidor
        if (!setupServerConfiguration(port)) {
            LOG_ERROR("Error al configurar servidor OPC UA");
            return false;
        }
        
        // Crear estructura de nodos
        if (!createOPCUAStructure()) {
            LOG_ERROR("Error al crear estructura OPC UA");
            return false;
        }
        
        // Registrar callback de actualización
        if (!registerUpdateCallback()) {
            LOG_ERROR("Error al registrar callback de actualización");
            return false;
        }
        
        // Iniciar servidor en hilo separado
        running_ = true;
        server_thread_ = std::make_unique<std::thread>(&OPCUAServer::serverThreadFunction, this);
        
        LOG_SUCCESS("✅ Servidor OPC UA iniciado en puerto " + std::to_string(port));
        LOG_INFO("📡 Endpoint: opc.tcp://localhost:" + std::to_string(port));
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Excepción al iniciar servidor OPC UA: " + std::string(e.what()));
        return false;
    }
}

void OPCUAServer::stop() {
    if (!running_) {
        return;
    }
    
    LOG_INFO("🛑 Deteniendo servidor OPC UA...");
    
    running_ = false;
    
    // Detener servidor
    if (ua_server_) {
        UA_Server_run_shutdown(ua_server_);
    }
    
    // Esperar a que termine el hilo
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    // Limpiar recursos
    if (ua_server_) {
        UA_Server_delete(ua_server_);
        ua_server_ = nullptr;
    }
    
    node_map_.clear();
    opcua_to_internal_name_map_.clear();
    
    LOG_SUCCESS("✅ Servidor OPC UA detenido");
}

bool OPCUAServer::setupServerConfiguration(int port) {
    server_config_ = UA_Server_getConfig(ua_server_);
    
    // Configuración básica del servidor
    UA_ServerConfig_setMinimal(server_config_, port, nullptr);
    
    // Configurar información de aplicación
    server_config_->applicationDescription.applicationUri = 
        UA_String_fromChars(APPLICATION_URI);
    server_config_->applicationDescription.productUri = 
        UA_String_fromChars("urn:PlantaGas:SCADA:Product");
    server_config_->applicationDescription.applicationName = 
        UA_LOCALIZEDTEXT("en", (char*)APPLICATION_NAME);
    server_config_->applicationDescription.applicationType = UA_APPLICATIONTYPE_SERVER;
    
    // Configurar límites
    server_config_->maxSessions = 100;
    // Note: maxSessionsPerEndpoint not available in this open62541 version
    
    LOG_DEBUG("🔧 Configuración OPC UA establecida");
    return true;
}

bool OPCUAServer::createOPCUAStructure() {
    try {
        // Crear estructura de carpetas organizadas
        if (!createOrganizedFolders()) {
            LOG_ERROR("Error al crear carpetas organizadas");
            return false;
        }
        
        // Crear nodos para todos los tags
        if (!createTagNodes()) {
            LOG_ERROR("Error al crear nodos de tags");
            return false;
        }
        
        LOG_SUCCESS("✅ Estructura OPC UA creada exitosamente");
        LOG_INFO("   📁 " + std::to_string(folder_map_.size()) + " carpetas organizadas");
        LOG_INFO("   🏷️ " + std::to_string(node_map_.size()) + " nodos de variables");
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Excepción al crear estructura OPC UA: " + std::string(e.what()));
        return false;
    }
}

bool OPCUAServer::createOrganizedFolders() {
    // Crear carpeta raíz PlantaGas
    UA_NodeId objects_folder = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId planta_gas_folder = createFolderNode(objects_folder, "PlantaGas", "Planta Gas SCADA");
    
    if (UA_NodeId_isNull(&planta_gas_folder)) {
        LOG_ERROR("Error al crear carpeta raíz PlantaGas");
        return false;
    }
    
    // Definir estructura de carpetas por categorías
    std::vector<std::pair<std::string, std::string>> folder_definitions = {
        {"FlowTransmitters", "Flow Transmitters"},
        {"FlowIndicators", "Flow Indicators"},
        {"PressureIndicators", "Pressure Indicators"},
        {"TemperatureIndicators", "Temperature Indicators"},
        {"LevelIndicators", "Level Indicators"},
        {"FlowControllers", "Flow Controllers"},
        {"PressureControllers", "Pressure Controllers"},
        {"TemperatureControllers", "Temperature Controllers"}
    };
    
    // Crear cada carpeta
    for (const auto& [folder_key, display_name] : folder_definitions) {
        UA_NodeId folder_id = createFolderNode(planta_gas_folder, folder_key, display_name);
        
        if (!UA_NodeId_isNull(&folder_id)) {
            FolderStructure folder_struct;
            folder_struct.folder_id = folder_id;
            folder_struct.display_name = display_name;
            
            // Note: tag_patterns removed as it's not part of FolderStructure
            
            folder_map_[folder_key] = folder_struct;
            LOG_DEBUG("📁 Carpeta creada: " + display_name);
        } else {
            LOG_ERROR("Error al crear carpeta: " + display_name);
            return false;
        }
    }
    
    return true;
}

UA_NodeId OPCUAServer::createFolderNode(const UA_NodeId& parent, const std::string& folder_name, 
                                       const std::string& display_name) {
    UA_NodeId folder_id = UA_NODEID_STRING(NAMESPACE_INDEX, (char*)folder_name.c_str());
    
    UA_ObjectAttributes folder_attr = UA_ObjectAttributes_default;
    folder_attr.displayName = UA_LOCALIZEDTEXT("en", (char*)display_name.c_str());
    folder_attr.description = UA_LOCALIZEDTEXT("en", (char*)display_name.c_str());
    
    UA_StatusCode result = UA_Server_addObjectNode(
        ua_server_,
        folder_id,
        parent,
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(NAMESPACE_INDEX, (char*)folder_name.c_str()),
        UA_NODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE),
        folder_attr,
        nullptr,
        nullptr
    );
    
    if (result != UA_STATUSCODE_GOOD) {
        LOG_ERROR("Error al crear nodo carpeta " + folder_name + ": " + std::to_string(result));
        return UA_NODEID_NULL;
    }
    
    return folder_id;
}

bool OPCUAServer::createTagNodes() {
    const auto& tags = tag_manager_->getAllTags();
    size_t created_tags = 0;
    
    for (const auto& tag : tags) {
        try {
            // Determinar carpeta padre basada en el grupo del tag
            std::string folder_key = getFolderForTag(tag->getGroup());
            
            if (folder_map_.find(folder_key) == folder_map_.end()) {
                LOG_WARNING("Carpeta no encontrada para tag: " + tag->getName() + ", usando FlowTransmitters");
                folder_key = "FlowTransmitters";
            }
            
            UA_NodeId parent_folder = folder_map_[folder_key].folder_id;
            
            // Crear nodo según grupo/categoría
            bool success = false;
            if (tag->getGroup() == "PID_CONTROLLER") {
                success = createPIDTagNode(tag, parent_folder);
            } else {
                success = createInstrumentTagNode(tag, parent_folder);
            }
            
            if (success) {
                created_tags++;
                
                // Mapear nombre OPC UA a nombre interno
                opcua_to_internal_name_map_[tag->getName()] = tag->getName();
                
                LOG_DEBUG("🏷️ Tag creado: " + tag->getName() + " (1 variable)");
            } else {
                LOG_ERROR("Error al crear tag: " + tag->getName());
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Excepción al crear tag " + tag->getName() + ": " + std::string(e.what()));
        }
    }
    
    LOG_INFO("📊 Tags OPC UA creados: " + std::to_string(created_tags) + "/" + std::to_string(tags.size()));
    return created_tags > 0;
}

bool OPCUAServer::createInstrumentTagNode(std::shared_ptr<Tag> tag, const UA_NodeId& parent_folder) {
    // Crear carpeta del tag
    UA_NodeId tag_folder = createFolderNode(parent_folder, tag->getName(), tag->getDescription());
    if (UA_NodeId_isNull(&tag_folder)) {
        return false;
    }
    
    // Crear variable principal del tag
    UA_NodeId var_node = createVariableNode(tag_folder, "Value", tag);
    if (!UA_NodeId_isNull(&var_node)) {
        std::string node_path = buildNodePath(tag->getName(), "Value");
        node_map_[node_path] = var_node;
    }
    
    return true;
}

bool OPCUAServer::createPIDTagNode(std::shared_ptr<Tag> tag, const UA_NodeId& parent_folder) {
    // Crear carpeta del controlador PID
    UA_NodeId pid_folder = createFolderNode(parent_folder, tag->getName(), tag->getDescription());
    if (UA_NodeId_isNull(&pid_folder)) {
        return false;
    }
    
    // Crear variable principal del tag
    UA_NodeId var_node = createVariableNode(pid_folder, "Value", tag);
    if (!UA_NodeId_isNull(&var_node)) {
        std::string node_path = buildNodePath(tag->getName(), "Value");
        node_map_[node_path] = var_node;
    }
    
    return true;
}

UA_NodeId OPCUAServer::createVariableNode(const UA_NodeId& parent, const std::string& variable_name,
                                         std::shared_ptr<Tag> tag) {
    std::string node_id_str = tag->getName() + "." + variable_name;
    UA_NodeId variable_id = UA_NODEID_STRING(NAMESPACE_INDEX, (char*)node_id_str.c_str());
    
    UA_VariableAttributes var_attr = UA_VariableAttributes_default;
    var_attr.displayName = UA_LOCALIZEDTEXT("en", (char*)variable_name.c_str());
    var_attr.description = UA_LOCALIZEDTEXT("en", (char*)tag->getDescription().c_str());
    
    // Configurar valor inicial y tipo
    var_attr.value = convertTagToUAVariant(tag);
    
    // Configurar acceso (writable o read-only)
    if (!tag->isReadOnly()) {
        var_attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
        var_attr.userAccessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    } else {
        var_attr.accessLevel = UA_ACCESSLEVELMASK_READ;
        var_attr.userAccessLevel = UA_ACCESSLEVELMASK_READ;
    }
    
    // Crear nodo
    UA_StatusCode result = UA_Server_addVariableNode(
        ua_server_,
        variable_id,
        parent,
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
        UA_QUALIFIEDNAME(NAMESPACE_INDEX, (char*)variable_name.c_str()),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        var_attr,
        nullptr,
        nullptr
    );
    
    // Configurar callback de escritura si es writable
    if (result == UA_STATUSCODE_GOOD && !tag->isReadOnly()) {
        UA_ValueCallback callback;
        callback.onRead = nullptr;
        callback.onWrite = writeCallback;
        UA_Server_setVariableNode_valueCallback(ua_server_, variable_id, callback);
    }
    
    // Limpiar variant
    UA_Variant_clear(&var_attr.value);
    
    if (result != UA_STATUSCODE_GOOD) {
        LOG_ERROR("Error al crear variable " + node_id_str + ": " + std::to_string(result));
        return UA_NODEID_NULL;
    }
    
    return variable_id;
}

bool OPCUAServer::registerUpdateCallback() {
    UA_StatusCode result = UA_Server_addRepeatedCallback(
        ua_server_,
        staticUpdateCallback,
        this,
        UPDATE_INTERVAL_MS,
        &callback_id_
    );
    
    if (result == UA_STATUSCODE_GOOD) {
        LOG_DEBUG("🔄 Callback de actualización registrado (cada " + 
                 std::to_string(UPDATE_INTERVAL_MS) + "ms)");
        return true;
    } else {
        LOG_ERROR("Error al registrar callback: " + std::to_string(result));
        return false;
    }
}

// Static wrapper function for the callback
void OPCUAServer::staticUpdateCallback(UA_Server* server, void* data) {
    auto* opcua_server = static_cast<OPCUAServer*>(data);
    if (opcua_server && opcua_server->running_) {
        opcua_server->updateAllVariables();
    }
}

void OPCUAServer::updateAllVariables() {
    try {
        const auto& tags = tag_manager_->getAllTags();
        
        for (const auto& tag : tags) {
            updateTagVariables(tag);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error en updateAllVariables: " + std::string(e.what()));
    }
}

void OPCUAServer::updateTagVariables(std::shared_ptr<Tag> tag) {
    // Actualizar variable principal 
    std::string node_path = buildNodePath(tag->getName(), "Value");
    auto it = node_map_.find(node_path);
    if (it != node_map_.end()) {
        UA_Variant value = convertTagToUAVariant(tag);
        UA_StatusCode result = UA_Server_writeValue(ua_server_, it->second, value);
        
        if (result != UA_STATUSCODE_GOOD) {
            LOG_DEBUG("Error al actualizar " + node_path + ": " + std::to_string(result));
        }
        
        UA_Variant_clear(&value);
    }
}

void OPCUAServer::writeCallback(UA_Server* server, const UA_NodeId* sessionId,
                                void* sessionContext, const UA_NodeId* nodeId,
                                void* nodeContext, const UA_NumericRange* range,
                                const UA_DataValue* data) {
    // Esta función será llamada cuando un cliente OPC UA escriba a una variable
    // TODO: Implementar escritura al TagManager
    
    LOG_WRITE("🔽 Escritura OPC UA recibida en nodo");
}

// === UTILIDADES ===

std::string OPCUAServer::buildNodePath(const std::string& tag_opcua_name, const std::string& variable_name) {
    return tag_opcua_name + "." + variable_name;
}

std::string OPCUAServer::getFolderForTag(const std::string& tag_opcua_name) {
    if (tag_opcua_name.substr(0, 3) == "ET_") return "FlowTransmitters";
    if (tag_opcua_name.substr(0, 4) == "FIT_") return "FlowIndicators";
    if (tag_opcua_name.substr(0, 4) == "PIT_" || tag_opcua_name.substr(0, 5) == "PDIT_") return "PressureIndicators";
    if (tag_opcua_name.substr(0, 4) == "TIT_") return "TemperatureIndicators";
    if (tag_opcua_name.substr(0, 4) == "LIT_") return "LevelIndicators";
    if (tag_opcua_name.substr(0, 4) == "FRC_") return "FlowControllers";
    if (tag_opcua_name.substr(0, 4) == "PRC_") return "PressureControllers";
    if (tag_opcua_name.substr(0, 4) == "TRC_") return "TemperatureControllers";
    
    return "FlowTransmitters"; // Default
}

UA_Variant OPCUAServer::convertTagToUAVariant(std::shared_ptr<Tag> tag) {
    UA_Variant variant;
    UA_Variant_init(&variant);
    
    switch (tag->getDataType()) {
        case TagDataType::FLOAT: {
            UA_Float* value = UA_Float_new();
            *value = tag->getValueAsFloat();
            UA_Variant_setScalar(&variant, value, &UA_TYPES[UA_TYPES_FLOAT]);
            break;
        }
        case TagDataType::DOUBLE: {
            UA_Double* value = UA_Double_new();
            *value = tag->getValueAsDouble();
            UA_Variant_setScalar(&variant, value, &UA_TYPES[UA_TYPES_DOUBLE]);
            break;
        }
        case TagDataType::INT32: {
            UA_Int32* value = UA_Int32_new();
            *value = tag->getValueAsInt32();
            UA_Variant_setScalar(&variant, value, &UA_TYPES[UA_TYPES_INT32]);
            break;
        }
        case TagDataType::BOOLEAN: {
            UA_Boolean* value = UA_Boolean_new();
            *value = tag->getValueAsBool();
            UA_Variant_setScalar(&variant, value, &UA_TYPES[UA_TYPES_BOOLEAN]);
            break;
        }
        case TagDataType::STRING: {
            std::string str_value = tag->getValueAsString();
            UA_String* value = UA_String_new();
            *value = UA_STRING_ALLOC(str_value.c_str());
            UA_Variant_setScalar(&variant, value, &UA_TYPES[UA_TYPES_STRING]);
            break;
        }
        default: {
            UA_Float* value = UA_Float_new();
            *value = 0.0f;
            UA_Variant_setScalar(&variant, value, &UA_TYPES[UA_TYPES_FLOAT]);
            break;
        }
    }
    
    return variant;
}

void OPCUAServer::serverThreadFunction() {
    LOG_INFO("🚀 Hilo del servidor OPC UA iniciado");
    
    UA_Boolean server_running = true;
    UA_StatusCode result = UA_Server_run(ua_server_, &server_running);
    
    if (result != UA_STATUSCODE_GOOD) {
        LOG_ERROR("Error en servidor OPC UA: " + std::to_string(result));
    }
    
    LOG_INFO("🛑 Hilo del servidor OPC UA terminado");
}