/*
 * opcua_server.cpp - Implementación del servidor OPC-UA adaptado
 * 
 * Adaptado desde tu implementación origi    // Agregar namespace personalizado para PlantaGas
    namespace_index_ = UA_Server_addNamespace(ua_server_, "urn:PlantaGas:SCADA:PlantaGas");
    LOG_SUCCESS("✅ Namespace registrado con índice: " + std::to_string(namespace_index_)); nueva arquitectura
 */

#include "opcua_server.h"
#include "tag_manager.h"
#include "common.h"
#include <thread>
#include <chrono>

OPCUAServer::OPCUAServer(std::shared_ptr<TagManager> tag_manager)
    : ua_server_(nullptr)
    , server_config_(nullptr)
    , running_(false)
    , server_port_(4840)
    , tag_manager_(tag_manager)
    , callback_id_(0)
    , namespace_index_(1)  // Valor por defecto, se actualizará dinámicamente
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
    
    // Agregar namespace personalizado para PlantaGas
    namespace_index_ = UA_Server_addNamespace(ua_server_, "urn:PlantaGas:SCADA:PlantaGas");
    LOG_SUCCESS("✅ Namespace registrado con índice: " + std::to_string(namespace_index_));
    
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
    LOG_DEBUG("🏗️ Iniciando creación de estructura OPC UA completa...");
    
    try {
        // Crear estructura de carpetas organizadas
        if (!createOrganizedFolders()) {
            LOG_ERROR("❌ Error al crear carpetas organizadas");
            return false;
        }
        
        // Crear nodos para todos los tags industriales
        if (!createTagNodes()) {
            LOG_ERROR("❌ Error al crear nodos de tags");
            return false;
        }
        
        LOG_SUCCESS("✅ Estructura OPC UA completa creada exitosamente");
        LOG_INFO("   📁 " + std::to_string(folder_map_.size()) + " carpetas organizadas");
        LOG_INFO("   🏷️ " + std::to_string(node_map_.size()) + " nodos de variables");
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("💥 Excepción al crear estructura OPC UA: " + std::string(e.what()));
        return false;
    }
}

UA_NodeId OPCUAServer::createFolderNode(const UA_NodeId& parent, const std::string& folder_name, 
                                       const std::string& display_name, bool is_tag_object) {
    // Crear NodeId con string copiado
    UA_String folder_name_ua = UA_STRING_ALLOC(folder_name.c_str());
    UA_NodeId folder_id = UA_NODEID_STRING(namespace_index_, (char*)folder_name_ua.data);
    
    UA_ObjectAttributes folder_attr = UA_ObjectAttributes_default;
    folder_attr.displayName = UA_LOCALIZEDTEXT("en", (char*)folder_name.c_str());  // Usar nombre del TAG
    folder_attr.description = UA_LOCALIZEDTEXT("en", (char*)display_name.c_str()); // Descripción para tooltip
    
    // Crear QualifiedName con copia del string
    UA_String qualified_name_str = UA_STRING_ALLOC(folder_name.c_str());
    UA_QualifiedName qualified_name = UA_QUALIFIEDNAME_ALLOC(namespace_index_, (char*)qualified_name_str.data);
    
    UA_StatusCode result = UA_Server_addObjectNode(
        ua_server_,
        folder_id,
        parent,
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        qualified_name,
        UA_NODEID_NUMERIC(0, is_tag_object ? UA_NS0ID_BASEOBJECTTYPE : UA_NS0ID_FOLDERTYPE),
        folder_attr,
        nullptr,
        nullptr
    );
    
    // Limpiar strings temporales
    UA_QualifiedName_clear(&qualified_name);
    
    if (result != UA_STATUSCODE_GOOD) {
        LOG_ERROR("Error al crear nodo carpeta " + folder_name + ": " + std::to_string(result));
        // Limpiar NodeId si falló la creación
        UA_String_clear(&folder_name_ua);
        return UA_NODEID_NULL;
    }
    
    // Retornar NodeId con string copiado
    UA_NodeId result_id;
    UA_NodeId_copy(&folder_id, &result_id);
    
    return result_id;
}

void OPCUAServer::setTagConfiguration(const nlohmann::json& config) {
    tag_config_ = config;
    LOG_DEBUG("💾 Configuración de tags jerárquicos establecida");
}

bool OPCUAServer::createOrganizedFolders() {
    LOG_DEBUG("📁 Creando estructura de carpetas organizadas...");
    
    // Crear carpeta raíz PlantaGas
    UA_NodeId objects_folder = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId planta_gas_folder = createFolderNode(objects_folder, "PlantaGas", "Planta Gas SCADA");
    
    if (UA_NodeId_isNull(&planta_gas_folder)) {
        LOG_ERROR("❌ Error al crear carpeta raíz PlantaGas");
        return false;
    }
    
    // Almacenar la carpeta raíz en el mapa para referencia posterior
    FolderInfo root_info;
    root_info.folder_id = planta_gas_folder;
    root_info.display_name = "Planta Gas SCADA";
    folder_map_["PlantaGas"] = root_info;
    
    // Definir estructura de carpetas por categorías de tags industriales
    std::vector<std::pair<std::string, std::string>> folder_definitions = {
        {"FlowTransmitters", "Flow Transmitters"},
        {"FlowIndicators", "Flow Indicators"}, 
        {"PressureIndicators", "Pressure Indicators"},
        {"TemperatureIndicators", "Temperature Indicators"},
        {"LevelIndicators", "Level Indicators"},
        {"PressureDifferentialIndicators", "Pressure Differential Indicators"},
        {"FlowControllers", "Flow Rate Controllers"},
        {"PressureControllers", "Pressure Controllers"},
        {"TemperatureControllers", "Temperature Controllers"}
    };
    
    // Usar una copia fija del NodeId de la carpeta padre
    UA_NodeId parent_folder_copy;
    UA_NodeId_copy(&planta_gas_folder, &parent_folder_copy);
    
    // Crear cada carpeta de categoría
    for (const auto& [folder_key, display_name] : folder_definitions) {
        LOG_DEBUG("📁 Carpeta: " + display_name);
        UA_NodeId folder_id = createFolderNode(parent_folder_copy, folder_key, display_name);
        
        if (UA_NodeId_isNull(&folder_id)) {
            LOG_ERROR("❌ Error al crear carpeta: " + display_name);
            UA_NodeId_clear(&parent_folder_copy);
            return false;
        }
        
        // Guardar referencia de la carpeta
        FolderInfo folder_info;
        UA_NodeId_copy(&folder_id, &folder_info.folder_id);
        folder_info.display_name = display_name;
        folder_map_[folder_key] = folder_info;
        
        LOG_SUCCESS("✅ Carpeta creada: " + display_name);
    }
    
    // Limpiar la copia del NodeId
    UA_NodeId_clear(&parent_folder_copy);
    
    LOG_SUCCESS("✅ " + std::to_string(folder_definitions.size()) + " carpetas organizadas creadas");
    return true;
}

bool OPCUAServer::createTagNodes() {
    const auto& tags = tag_manager_->getAllTags();
    size_t created_tags = 0;
    
    for (const auto& tag : tags) {
        try {
            // Determinar carpeta padre basada en el nombre del tag
            std::string folder_key = getFolderForTag(tag->getName());
            
            if (folder_map_.find(folder_key) == folder_map_.end()) {
                LOG_WARNING("Carpeta no encontrada para tag: " + tag->getName() + ", usando FlowTransmitters");
                folder_key = "FlowTransmitters";
            }
            
            UA_NodeId parent_folder = folder_map_[folder_key].folder_id;
            
            // Crear nodo según grupo/categoría
            bool success = false;
            std::string tag_name = tag->getName();
            
            // Identificar controladores PID por el prefijo del nombre del tag
            bool isPIDController = (tag_name.substr(0, 3) == "TRC" ||  // Temperature Rate Controller
                                  tag_name.substr(0, 3) == "PRC" ||   // Pressure Rate Controller
                                  tag_name.substr(0, 3) == "FRC" ||   // Flow Rate Controller
                                  tag_name.substr(0, 3) == "LRC");    // Level Rate Controller
            
            if (isPIDController) {
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
    // Crear nodo del tag como objeto industrial (no como carpeta)
    UA_NodeId tag_folder = createFolderNode(parent_folder, tag->getName(), tag->getDescription(), true);
    if (UA_NodeId_isNull(&tag_folder)) {
        return false;
    }
    
    // Buscar la configuración del tag en el JSON para obtener las variables
    std::vector<std::string> variables = {"Value"}; // Por defecto
    
    if (!tag_config_.is_null() && tag_config_.contains("tags")) {
        for (const auto& tag_config : tag_config_["tags"]) {
            if (tag_config.contains("name") && tag_config["name"].get<std::string>() == tag->getName()) {
                if (tag_config.contains("variables") && tag_config["variables"].is_array()) {
                    variables.clear();
                    for (const auto& var : tag_config["variables"]) {
                        variables.push_back(var.get<std::string>());
                    }
                }
                break;
            }
        }
    }
    
    // Crear variables OPC UA para cada propiedad del tag
    int created_vars = 0;
    for (const std::string& variable_name : variables) {
        UA_NodeId var_node = createVariableNode(tag_folder, variable_name, tag);
        if (!UA_NodeId_isNull(&var_node)) {
            std::string node_path = buildNodePath(tag->getName(), variable_name);
            node_map_[node_path] = var_node;
            created_vars++;
        }
    }
    
    LOG_DEBUG("🏷️ Tag " + tag->getName() + " creado con " + std::to_string(created_vars) + " variables: " + 
             [&variables]() {
                 std::string vars_str = "";
                 for (size_t i = 0; i < variables.size(); ++i) {
                     vars_str += variables[i];
                     if (i < variables.size() - 1) vars_str += ", ";
                 }
                 return vars_str;
             }());
    
    return created_vars > 0;
}

bool OPCUAServer::createPIDTagNode(std::shared_ptr<Tag> tag, const UA_NodeId& parent_folder) {
    // Crear nodo del controlador PID como objeto industrial (no como carpeta)
    UA_NodeId pid_folder = createFolderNode(parent_folder, tag->getName(), tag->getDescription(), true);
    if (UA_NodeId_isNull(&pid_folder)) {
        return false;
    }
    
    // Buscar la configuración del tag en el JSON para obtener las variables
    std::vector<std::string> variables = {"PV", "SP", "CV"}; // PID típico por defecto
    
    if (!tag_config_.is_null() && tag_config_.contains("tags")) {
        for (const auto& tag_config : tag_config_["tags"]) {
            if (tag_config.contains("name") && tag_config["name"].get<std::string>() == tag->getName()) {
                if (tag_config.contains("variables") && tag_config["variables"].is_array()) {
                    variables.clear();
                    for (const auto& var : tag_config["variables"]) {
                        variables.push_back(var.get<std::string>());
                    }
                }
                break;
            }
        }
    }
    
    // También revisar en PID_controllers si existe
    if (!tag_config_.is_null() && tag_config_.contains("PID_controllers")) {
        for (const auto& tag_config : tag_config_["PID_controllers"]) {
            if (tag_config.contains("name") && tag_config["name"].get<std::string>() == tag->getName()) {
                if (tag_config.contains("variables") && tag_config["variables"].is_array()) {
                    variables.clear();
                    for (const auto& var : tag_config["variables"]) {
                        variables.push_back(var.get<std::string>());
                    }
                }
                break;
            }
        }
    }
    
    // Crear variables OPC UA para cada propiedad del controlador PID
    int created_vars = 0;
    for (const std::string& variable_name : variables) {
        UA_NodeId var_node = createVariableNode(pid_folder, variable_name, tag);
        if (!UA_NodeId_isNull(&var_node)) {
            std::string node_path = buildNodePath(tag->getName(), variable_name);
            node_map_[node_path] = var_node;
            created_vars++;
        }
    }
    
    LOG_DEBUG("🎛️ Controlador PID " + tag->getName() + " creado con " + std::to_string(created_vars) + " variables: " + 
             [&variables]() {
                 std::string vars_str = "";
                 for (size_t i = 0; i < variables.size(); ++i) {
                     vars_str += variables[i];
                     if (i < variables.size() - 1) vars_str += ", ";
                 }
                 return vars_str;
             }());
    
    return created_vars > 0;
}

UA_NodeId OPCUAServer::createVariableNode(const UA_NodeId& parent, const std::string& variable_name,
                                         std::shared_ptr<Tag> tag) {
    std::string node_id_str = tag->getName() + "." + variable_name;
    UA_NodeId variable_id = UA_NODEID_STRING(namespace_index_, (char*)node_id_str.c_str());
    
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
        UA_QUALIFIEDNAME(namespace_index_, (char*)variable_name.c_str()),
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
    // Buscar el tag en la configuración JSON
    std::string tag_name = tag->getName();
    
    // DEBUG: Mostrar tamaño del mapa de nodos
    LOG_DEBUG("🔍 Buscando variables para tag " + tag_name + " en mapa con " + std::to_string(node_map_.size()) + " nodos");
    
    // Buscar en tags regulares
    if (tag_config_.contains("tags")) {
        for (const auto& json_tag : tag_config_["tags"]) {
            if (json_tag["name"] == tag_name && json_tag.contains("variables")) {
                // Actualizar todas las variables del tag
                for (const auto& variable : json_tag["variables"]) {
                    std::string node_path = buildNodePath(tag_name, variable);
                    auto it = node_map_.find(node_path);
                    if (it != node_map_.end()) {
                        UA_Variant value = convertTagToUAVariant(tag);
                        UA_StatusCode result = UA_Server_writeValue(ua_server_, it->second, value);
                        
                        if (result != UA_STATUSCODE_GOOD) {
                            LOG_DEBUG("Error al actualizar " + node_path + ": " + std::to_string(result));
                        }
                        
                        UA_Variant_clear(&value);
                    } else {
                        LOG_DEBUG("⚠️ Nodo no encontrado en mapa: " + node_path);
                    }
                }
                return;
            }
        }
    }
    
    // Buscar en PID controllers
    if (tag_config_.contains("PID_controllers")) {
        for (const auto& json_tag : tag_config_["PID_controllers"]) {
            if (json_tag["name"] == tag_name && json_tag.contains("variables")) {
                // Actualizar todas las variables del tag
                for (const auto& variable : json_tag["variables"]) {
                    std::string node_path = buildNodePath(tag_name, variable);
                    auto it = node_map_.find(node_path);
                    if (it != node_map_.end()) {
                        UA_Variant value = convertTagToUAVariant(tag);
                        UA_StatusCode result = UA_Server_writeValue(ua_server_, it->second, value);
                        
                        if (result != UA_STATUSCODE_GOOD) {
                            LOG_DEBUG("Error al actualizar " + node_path + ": " + std::to_string(result));
                        }
                        
                        UA_Variant_clear(&value);
                    } else {
                        LOG_DEBUG("⚠️ Nodo no encontrado en mapa: " + node_path);
                    }
                }
                return;
            }
        }
    }
    
    // Fallback: buscar variable "Value" para compatibilidad
    std::string node_path = buildNodePath(tag_name, "Value");
    auto it = node_map_.find(node_path);
    if (it != node_map_.end()) {
        UA_Variant value = convertTagToUAVariant(tag);
        UA_StatusCode result = UA_Server_writeValue(ua_server_, it->second, value);
        
        if (result != UA_STATUSCODE_GOOD) {
            LOG_DEBUG("Error al actualizar " + node_path + ": " + std::to_string(result));
        }
        
        UA_Variant_clear(&value);
    } else {
        LOG_DEBUG("⚠️ Nodo fallback no encontrado en mapa: " + node_path);
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

std::string OPCUAServer::getFolderForTag(const std::string& tag_name) {
    return categorizeTagByName(tag_name);
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

bool OPCUAServer::createSimpleTestVariable(const UA_NodeId& parent_folder) {
    LOG_DEBUG("🧪 Creando variable de prueba simple...");
    
    UA_NodeId variable_id = UA_NODEID_STRING(namespace_index_, (char*)"TestVariable");
    
    UA_VariableAttributes var_attr = UA_VariableAttributes_default;
    var_attr.displayName = UA_LOCALIZEDTEXT("en", (char*)"Test Variable");
    var_attr.description = UA_LOCALIZEDTEXT("en", (char*)"Simple test variable");
    
    // Crear valor inicial
    UA_Float test_value = 42.0f;
    UA_Variant_setScalar(&var_attr.value, &test_value, &UA_TYPES[UA_TYPES_FLOAT]);
    
    UA_StatusCode result = UA_Server_addVariableNode(
        ua_server_,
        variable_id,
        parent_folder,
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
        UA_QUALIFIEDNAME(namespace_index_, (char*)"TestVariable"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        var_attr,
        nullptr,
        nullptr
    );
    
    if (result != UA_STATUSCODE_GOOD) {
        LOG_ERROR("❌ Error al crear variable de prueba: " + std::to_string(result));
        return false;
    }
    
    LOG_SUCCESS("✅ Variable de prueba creada exitosamente");
    return true;
}

std::string OPCUAServer::categorizeTagByName(const std::string& tag_name) {
    // Mapeo de prefijos industriales a categorías de carpetas
    struct CategoryMapping {
        std::string prefix;
        std::string folder_key;
    };
    
    std::vector<CategoryMapping> mappings = {
        {"ET_", "FlowTransmitters"},      // Flow Transmitters
        {"FI_", "FlowIndicators"},        // Flow Indicators  
        {"PI_", "PressureIndicators"},    // Pressure Indicators
        {"TI_", "TemperatureIndicators"}, // Temperature Indicators
        {"LI_", "LevelIndicators"},       // Level Indicators
        {"PDI_", "PressureDifferentialIndicators"}, // Pressure Differential Indicators
        {"FRC_", "FlowControllers"},      // Flow Rate Controllers
        {"PRC_", "PressureControllers"},  // Pressure Controllers
        {"TRC_", "TemperatureControllers"} // Temperature Controllers
    };
    
    // Buscar coincidencia de prefijo
    for (const auto& mapping : mappings) {
        if (tag_name.find(mapping.prefix) == 0) {
            return mapping.folder_key;
        }
    }
    
    // Categoría por defecto si no coincide ningún prefijo
    return "FlowTransmitters";
}

void OPCUAServer::serverThreadFunction() {
    LOG_INFO("🚀 Hilo del servidor OPC UA iniciado");
    
    // Inicializar el servidor
    UA_StatusCode retval = UA_Server_run_startup(ua_server_);
    if (retval != UA_STATUSCODE_GOOD) {
        LOG_ERROR("💥 Error al inicializar servidor OPC UA: " + std::to_string(retval));
        running_ = false;
        return;
    }
    
    LOG_SUCCESS("✅ Servidor OPC UA inicializado correctamente");
    
    while (running_) {
        // Ejecutar un ciclo del servidor con timeout corto
        retval = UA_Server_run_iterate(ua_server_, 100); // 100ms timeout
        
        if (retval != UA_STATUSCODE_GOOD) {
            LOG_DEBUG("⚠️ Error en ciclo del servidor: " + std::to_string(retval));
        }
        
        // Pequeña pausa para no saturar CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Limpiar el servidor
    UA_Server_run_shutdown(ua_server_);
    LOG_INFO("🛑 Hilo del servidor OPC UA terminado");
}