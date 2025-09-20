/*
 * opcua_server.cpp - Implementación del servidor OPC-UA optimizado
 * 
 * Sistema simplificado con:
 * - Actualización manual desde PAC vía updateTagsFromPAC()
 * - Sin callbacks automáticos periódicos (sistema deshabilitado por diseño)
 * - Funciones obsoletas eliminadas para mejor legibilidad
 */

#include "opcua_server.h"
#include "tag_manager.h"
#include "pac_control_client.h"
#include "common.h"
#include <thread>
#include <chrono>
#include <set>
#include <unordered_set>

OPCUAServer::OPCUAServer(std::shared_ptr<TagManager> tag_manager)
    : ua_server_(nullptr)
    , server_config_(nullptr)
    , running_(false)
    , server_port_(4841)
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
        
        LOG_SUCCESS("✅ Servidor OPC UA '" + std::string(APPLICATION_NAME) + "' iniciado en puerto " + std::to_string(port));
        LOG_INFO("📡 URL del servidor: opc.tcp://localhost:" + std::to_string(port));
        LOG_INFO("🏷️ Nombre visible: " + std::string(APPLICATION_NAME));
        LOG_INFO("🆔 URI de aplicación: " + std::string(APPLICATION_URI));
        
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
    
    // Agregar namespace personalizado para PAC PLANTA_GAS
    namespace_index_ = UA_Server_addNamespace(ua_server_, "urn:PAC:PLANTA_GAS:PlantaGas");
    LOG_SUCCESS("✅ Namespace registrado con índice: " + std::to_string(namespace_index_));
    
    // Configurar información de aplicación
    server_config_->applicationDescription.applicationUri = 
        UA_String_fromChars(APPLICATION_URI);
    server_config_->applicationDescription.productUri = 
        UA_String_fromChars("urn:PAC:PLANTA_GAS:Product");
    server_config_->applicationDescription.applicationName = 
        UA_LOCALIZEDTEXT("en", (char*)APPLICATION_NAME);
    server_config_->applicationDescription.applicationType = UA_APPLICATIONTYPE_SERVER;
    
    // Configurar límites
    server_config_->maxSessions = 100;
    // Note: maxSessionsPerEndpoint not available in this open62541 version
    
    LOG_DEBUG("🔧 Configuración OPC UA establecida en puerto " + std::to_string(port));
    LOG_INFO("🌐 URL del servidor: opc.tcp://localhost:" + std::to_string(port));
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
    
    // Definir estructura de carpetas simplificada - Solo 2 categorías principales
    std::vector<std::pair<std::string, std::string>> folder_definitions = {
        {"Instrumentos", "Instrumentos de Campo"},
        {"ControladorsPID", "Controladores PID"}
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
    
    // Paso 1: Identificar tags padre únicos (sin punto en el nombre)
    std::set<std::string> parent_tags;
    for (const auto& tag : tags) {
        std::string tag_name = tag->getName();
        size_t dot_pos = tag_name.find('.');
        if (dot_pos != std::string::npos) {
            // Es un sub-tag, extraer el nombre padre
            std::string parent_name = tag_name.substr(0, dot_pos);
            parent_tags.insert(parent_name);
        } else {
            // Es un tag padre
            parent_tags.insert(tag_name);
        }
    }
    
    LOG_INFO("📊 Identificados " + std::to_string(parent_tags.size()) + " tags padre únicos");
    
    // Paso 2: Crear estructura jerárquica para cada tag padre
    for (const auto& parent_tag_name : parent_tags) {
        try {
            // Determinar carpeta padre basada en el nombre del tag
            std::string folder_key = getFolderForTag(parent_tag_name);
            
            if (folder_map_.find(folder_key) == folder_map_.end()) {
                LOG_WARNING("Carpeta no encontrada para tag: " + parent_tag_name + ", usando Instrumentos");
                folder_key = "Instrumentos";
            }
            
            UA_NodeId parent_folder = folder_map_[folder_key].folder_id;
            
            // Crear nodo según grupo/categoría
            bool success = false;
            
            // Identificar controladores PID por el prefijo del nombre del tag
            bool isPIDController = (parent_tag_name.substr(0, 3) == "TRC" ||  // Temperature Rate Controller
                                  parent_tag_name.substr(0, 3) == "PRC" ||   // Pressure Rate Controller
                                  parent_tag_name.substr(0, 3) == "FRC" ||   // Flow Rate Controller
                                  parent_tag_name.substr(0, 3) == "LRC");    // Level Rate Controller
            
            // Buscar tag padre en la lista para usar como referencia
            std::shared_ptr<Tag> reference_tag = nullptr;
            for (const auto& tag : tags) {
                if (tag->getName() == parent_tag_name) {
                    reference_tag = tag;
                    break;
                }
            }
            
            // Si no hay tag padre, usar el primer sub-tag como referencia
            if (!reference_tag) {
                for (const auto& tag : tags) {
                    if (tag->getName().substr(0, parent_tag_name.length() + 1) == parent_tag_name + ".") {
                        reference_tag = tag;
                        break;
                    }
                }
            }
            
            if (reference_tag) {
                if (isPIDController) {
                    success = createPIDTagNode(reference_tag, parent_folder, parent_tag_name);
                } else {
                    success = createInstrumentTagNode(reference_tag, parent_folder, parent_tag_name);
                }
                
                if (success) {
                    created_tags++;
                    
                    // Mapear nombre OPC UA a nombre interno
                    opcua_to_internal_name_map_[parent_tag_name] = parent_tag_name;
                    
                    LOG_DEBUG("🏷️ Tag jerárquico creado: " + parent_tag_name);
                } else {
                    LOG_ERROR("Error al crear tag: " + parent_tag_name);
                }
            } else {
                LOG_WARNING("No se encontró tag de referencia para: " + parent_tag_name);
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Excepción al crear tag " + parent_tag_name + ": " + std::string(e.what()));
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
            // ✅ USAR tag_name completo como node_path (formato: "ET_1601.PV")
            std::string node_path = tag->getName() + "." + variable_name;
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
            // ✅ USAR tag_name completo como node_path (formato: "PRC_1201.PV")
            std::string node_path = tag->getName() + "." + variable_name;
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

// Versión sobrecargada para crear tag de instrumento con nombre específico
bool OPCUAServer::createInstrumentTagNode(std::shared_ptr<Tag> reference_tag, const UA_NodeId& parent_folder, const std::string& parent_tag_name) {
    // Crear nodo del tag como objeto industrial
    UA_NodeId tag_folder = createFolderNode(parent_folder, parent_tag_name, reference_tag->getDescription(), true);
    if (UA_NodeId_isNull(&tag_folder)) {
        return false;
    }
    
    // Obtener todas las variables de este tag padre desde TagManager
    const auto& all_tags = tag_manager_->getAllTags();
    std::vector<std::string> variables;
    
    // Buscar todos los sub-tags que corresponden a este tag padre
    for (const auto& tag : all_tags) {
        std::string tag_name = tag->getName();
        if (tag_name.length() > parent_tag_name.length() + 1 && 
            tag_name.substr(0, parent_tag_name.length() + 1) == parent_tag_name + ".") {
            // Es un sub-tag de este padre, extraer la variable
            std::string variable = tag_name.substr(parent_tag_name.length() + 1);
            variables.push_back(variable);
        }
    }
    
    // Si no hay sub-tags, usar configuración JSON
    if (variables.empty()) {
        variables = {"PV", "SV", "SetHH", "SetH", "SetL", "SetLL", "Input", "percent", "min", "max", "SIM_Value"};
        
        if (!tag_config_.is_null() && tag_config_.contains("tags")) {
            for (const auto& tag_config : tag_config_["tags"]) {
                if (tag_config.contains("name") && tag_config["name"].get<std::string>() == parent_tag_name) {
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
    }
    
    // Crear variables OPC UA para cada propiedad del tag
    int created_vars = 0;
    for (const std::string& variable_name : variables) {
        // Buscar el tag correspondiente en TagManager
        std::shared_ptr<Tag> sub_tag = nullptr;
        std::string full_tag_name = parent_tag_name + "." + variable_name;
        
        for (const auto& tag : all_tags) {
            if (tag->getName() == full_tag_name) {
                sub_tag = tag;
                break;
            }
        }
        
        // Usar reference_tag si no se encuentra el sub-tag específico
        if (!sub_tag) {
            sub_tag = reference_tag;
        }
        
        UA_NodeId var_node = createVariableNode(tag_folder, variable_name, sub_tag);
        if (!UA_NodeId_isNull(&var_node)) {
            // ✅ USAR formato correcto (parent_tag + "." + variable)
            std::string node_path = parent_tag_name + "." + variable_name;
            node_map_[node_path] = var_node;
            LOG_DEBUG("🗺️ Registrado en mapa: " + node_path);
            created_vars++;
        }
    }
    
    LOG_DEBUG("🏷️ Tag instrumento " + parent_tag_name + " creado con " + std::to_string(created_vars) + " variables");
    return created_vars > 0;
}

// Versión sobrecargada para crear tag PID con nombre específico
bool OPCUAServer::createPIDTagNode(std::shared_ptr<Tag> reference_tag, const UA_NodeId& parent_folder, const std::string& parent_tag_name) {
    // Crear nodo del controlador PID como objeto industrial
    UA_NodeId pid_folder = createFolderNode(parent_folder, parent_tag_name, reference_tag->getDescription(), true);
    if (UA_NodeId_isNull(&pid_folder)) {
        return false;
    }
    
    // Obtener todas las variables de este tag padre desde TagManager
    const auto& all_tags = tag_manager_->getAllTags();
    std::vector<std::string> variables;
    
    // Buscar todos los sub-tags que corresponden a este tag padre
    for (const auto& tag : all_tags) {
        std::string tag_name = tag->getName();
        if (tag_name.length() > parent_tag_name.length() + 1 && 
            tag_name.substr(0, parent_tag_name.length() + 1) == parent_tag_name + ".") {
            // Es un sub-tag de este padre, extraer la variable
            std::string variable = tag_name.substr(parent_tag_name.length() + 1);
            variables.push_back(variable);
        }
    }
    
    // Si no hay sub-tags, usar configuración por defecto para PID
    if (variables.empty()) {
        variables = {"PV", "SP", "CV", "KP", "KI", "KD", "auto_manual", "OUTPUT_HIGH", "OUTPUT_LOW", "PID_ENABLE"};
        
        // Buscar en configuración JSON
        if (!tag_config_.is_null() && tag_config_.contains("PID_controllers")) {
            for (const auto& tag_config : tag_config_["PID_controllers"]) {
                if (tag_config.contains("name") && tag_config["name"].get<std::string>() == parent_tag_name) {
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
    }
    
    // Crear variables OPC UA para cada propiedad del controlador PID
    int created_vars = 0;
    for (const std::string& variable_name : variables) {
        // Buscar el tag correspondiente en TagManager
        std::shared_ptr<Tag> sub_tag = nullptr;
        std::string full_tag_name = parent_tag_name + "." + variable_name;
        
        for (const auto& tag : all_tags) {
            if (tag->getName() == full_tag_name) {
                sub_tag = tag;
                break;
            }
        }
        
        // Usar reference_tag si no se encuentra el sub-tag específico
        if (!sub_tag) {
            sub_tag = reference_tag;
        }
        
        UA_NodeId var_node = createVariableNode(pid_folder, variable_name, sub_tag);
        if (!UA_NodeId_isNull(&var_node)) {
            // ✅ USAR formato correcto (parent_tag + "." + variable)
            std::string node_path = parent_tag_name + "." + variable_name;
            node_map_[node_path] = var_node;
            created_vars++;
        }
    }
    
    LOG_DEBUG("🏷️ Tag PID " + parent_tag_name + " creado con " + std::to_string(created_vars) + " variables");
    return created_vars > 0;
}

UA_NodeId OPCUAServer::createVariableNode(const UA_NodeId& parent, const std::string& variable_name,
                                         std::shared_ptr<Tag> tag) {
    // Extraer el nombre del tag padre correctamente
    std::string tag_name = tag->getName();
    std::string parent_tag_name;
    
    // Si el tag name contiene un punto, extraer la parte padre
    size_t dot_pos = tag_name.find('.');
    if (dot_pos != std::string::npos) {
        parent_tag_name = tag_name.substr(0, dot_pos);
    } else {
        parent_tag_name = tag_name;
    }
    
    // Crear NodeId correcto: parent_tag + "." + variable_name
    std::string node_id_str = parent_tag_name + "." + variable_name;
    // 🔧 FIX CRÍTICO: Usar UA_STRING_ALLOC para hacer copia permanente del string
    UA_NodeId variable_id = UA_NODEID_STRING_ALLOC(namespace_index_, node_id_str.c_str());
    
    LOG_DEBUG("🏷️ Creando variable NodeId: " + node_id_str + " (tag: " + tag_name + ")");
    
    UA_VariableAttributes var_attr = UA_VariableAttributes_default;
    var_attr.displayName = UA_LOCALIZEDTEXT("en", (char*)variable_name.c_str());
    var_attr.description = UA_LOCALIZEDTEXT("en", (char*)tag->getDescription().c_str());
    
    // Configurar valor inicial y tipo
    var_attr.value = convertTagToUAVariant(tag);
    
    // DEBUG: Verificar estado read-only del tag
    bool tag_readonly = tag->isReadOnly();
    LOG_DEBUG("🔍 Tag " + variable_name + " isReadOnly(): " + (tag_readonly ? "SÍ" : "NO"));
    
    // Configurar acceso (writable o read-only)
    if (!tag_readonly) {
        var_attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
        var_attr.userAccessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
        LOG_DEBUG("   ✅ Configurado como READ_WRITE");
    } else {
        var_attr.accessLevel = UA_ACCESSLEVELMASK_READ;
        var_attr.userAccessLevel = UA_ACCESSLEVELMASK_READ;
        LOG_DEBUG("   ⚠️ Configurado como READ_ONLY");
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
        
        // Configurar el callback con contexto
        UA_StatusCode callback_result = UA_Server_setVariableNode_valueCallback(ua_server_, variable_id, callback);
        
        // También establecer el contexto del nodo
        UA_Server_setNodeContext(ua_server_, variable_id, this);
        
        if (callback_result == UA_STATUSCODE_GOOD) {
            LOG_DEBUG("   📝 WriteCallback configurado para: " + variable_name);
        }
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

// Static wrapper function for the callback - DESHABILITADO
void OPCUAServer::staticUpdateCallback(UA_Server* server, void* data) {
    // Callback deshabilitado: Sistema de actualización manual vía updateTagsFromPAC()
    return;
}


// Nuevo método para actualizar solo tags específicos cuando cambian
void OPCUAServer::updateSpecificTag(std::shared_ptr<Tag> tag) {
    if (!tag || !running_) {
        return;
    }
    
    // 🛡️ PROTECCIÓN: No sobrescribir valores escritos recientemente por clientes
    if (tag->wasRecentlyWrittenByClient(5000)) { // Protección de 5 segundos
        // Log detallado de la protección
        uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        static std::unordered_map<std::string, uint64_t> last_log_time;
        uint64_t& last_logged = last_log_time[tag->getName()];
        
        // Log cada 2 segundos para evitar spam pero mantener visibilidad
        if (current_time - last_logged > 2000) {
            LOG_WARNING("🛡️ PROTECCIÓN ACTIVA: " + tag->getName() + " (escrito por cliente - no actualizando desde PAC)");
            last_logged = current_time;
        }
        return;
    }
    
    // ✅ SOLUCIÓN: Usar tag_name directamente como NodeId (como en implementación funcional anterior)
    std::string tag_name = tag->getName();
    size_t dot_pos = tag_name.find('.');
    
    if (dot_pos != std::string::npos) {
        // 🔧 USAR TAG_NAME COMPLETO COMO NodeId (no buildNodePath)
        // En la implementación funcional, se usa var.opcua_name directamente
        std::string opcua_node_id = tag_name;  // "ET_1601.PV", "PRC_1201.SP", etc.
        
        // DEBUG: Mostrar mapping correcto - MÁS DETALLADO
        static int debug_count = 0;
        if ((tag_name.find(".PV") != std::string::npos || 
             tag_name.find(".SP") != std::string::npos || 
             tag_name.find(".CV") != std::string::npos) && debug_count < 10) {
            LOG_DEBUG("🔍 DEBUG updateSpecificTag: \"" + tag_name + "\" -> NodeId: \"" + opcua_node_id + "\"");
            LOG_DEBUG("🔍     node_map_.size() = " + std::to_string(node_map_.size()));
            LOG_DEBUG("🔍     Buscando en node_map_...");
            debug_count++;
        }
        
        // 🔧 BUSCAR EN node_map_ usando tag_name completo
        auto it = node_map_.find(opcua_node_id);
        if (it != node_map_.end()) {
            // Comentado para debug limpio de escrituras
            // LOG_DEBUG("🔍     ENCONTRADO en node_map_!");
            UA_DataValue data_value;
            UA_DataValue_init(&data_value);
            data_value.value = convertTagToUAVariant(tag);
            data_value.hasValue = true;
            data_value.hasStatus = true;
            data_value.status = UA_STATUSCODE_GOOD;
            
            UA_StatusCode result = UA_Server_writeDataValue(ua_server_, it->second, data_value);
            if (result == UA_STATUSCODE_GOOD) {
                // Solo loguear PV y valores importantes para debug
                if (tag_name.find(".PV") != std::string::npos || 
                    tag_name.find(".SP") != std::string::npos || 
                    tag_name.find(".CV") != std::string::npos) {
                    std::string value_str = "?";
                    if (auto* f_val = std::get_if<float>(&tag->getValue())) {
                        value_str = std::to_string(*f_val);
                    }
                    LOG_DEBUG("✅ " + opcua_node_id + " = " + value_str);
                }
            } else {
                // Solo reportar errores para variables críticas
                if (tag_name.find(".PV") != std::string::npos || 
                    tag_name.find(".SP") != std::string::npos || 
                    tag_name.find(".CV") != std::string::npos) {
                    LOG_DEBUG("❌ Error actualizando " + opcua_node_id + ": " + std::string(UA_StatusCode_name(result)));
                }
            }
            
            UA_DataValue_clear(&data_value);
        } else {
            LOG_DEBUG("🔍     NO ENCONTRADO en node_map_! Clave buscada: \"" + opcua_node_id + "\"");
            if (opcua_node_id.find(".PV") != std::string::npos && node_map_.size() < 50) {
                LOG_DEBUG("🔍     Primeras 10 claves en node_map_:");
                int count = 0;
                for (const auto& pair : node_map_) {
                    if (count < 10) {
                        LOG_DEBUG("🔍       [" + std::to_string(count) + "] \"" + pair.first + "\"");
                        count++;
                    } else break;
                }
            }
        }
        // SILENCIOSAMENTE ignorar tags que no existen en OPC UA
    }
}

// Método para actualizar solo tags que han recibido datos del PAC recientemente
void OPCUAServer::updateTagsFromPAC() {
    if (!tag_manager_ || !running_) {
        return;
    }
    
    try {
        const auto& tags = tag_manager_->getAllTags();
        int updated_count = 0;
        int skipped_count = 0;
        
        // DEBUG: Mostrar contenido del node_map_ para diagnóstico
        static bool debug_shown = false;
        if (!debug_shown && !node_map_.empty()) {
            LOG_DEBUG("🔍 DEBUG node_map_ contiene " + std::to_string(node_map_.size()) + " entries:");
            int count = 0;
            for (const auto& pair : node_map_) {
                if (count < 10) {  // Solo mostrar primeros 10
                    LOG_DEBUG("  [" + std::to_string(count) + "] \"" + pair.first + "\"");
                    count++;
                } else if (count == 10) {
                    LOG_DEBUG("  ... y " + std::to_string(node_map_.size() - 10) + " más");
                    break;
                }
            }
            debug_shown = true;
        }
        
        for (const auto& tag : tags) {
            // Solo actualizar sub-tags (que contienen punto) Y que existan en node_map
            if (tag->getName().find('.') != std::string::npos) {
                std::string tag_name = tag->getName();
                // ✅ USAR tag_name completo como node_path (ya tiene formato "TAG.VARIABLE")
                std::string node_path = tag_name;
                
                // CRÍTICO: Solo procesar si el tag existe en node_map
                if (node_map_.find(node_path) != node_map_.end()) {
                    updateSpecificTag(tag);
                    updated_count++;
                } else {
                    skipped_count++;
                }
            }
        }
        
        if (updated_count > 0) {
            // Comentado para debug limpio de escrituras
            // LOG_DEBUG("🔄 Actualizados " + std::to_string(updated_count) + " tags en OPC UA desde datos PAC recientes");
        }
        if (skipped_count > 0) {
            LOG_DEBUG("⏭️ Saltados " + std::to_string(skipped_count) + " tags no registrados en node_map");
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error en updateTagsFromPAC: " + std::string(e.what()));
    }
}

// 🔧 FUNCIÓN PERFECTA PARA DETECTAR ORIGEN (solo agregar esta función)
bool OPCUAServer::isWriteFromClient(const UA_NodeId *sessionId) {
    if (!sessionId) {
        return false; // Sin SessionId = escritura interna del servidor
    }
    
    // 🎯 LA CLAVE: NamespaceIndex
    // - namespaceIndex = 0 → Escrituras INTERNAS del servidor
    // - namespaceIndex != 0 → Escrituras de CLIENTES EXTERNOS
    return sessionId->namespaceIndex != 0;
}

void OPCUAServer::writeCallback(UA_Server* server, const UA_NodeId* sessionId,
                                void* sessionContext, const UA_NodeId* nodeId,
                                void* nodeContext, const UA_NumericRange* range,
                                const UA_DataValue* data) {
    auto* opcua_server = static_cast<OPCUAServer*>(nodeContext);
    if (!opcua_server || !data || !data->hasValue) {
        return;
    }

    try {
        // 🎯 ESCRITURA DESDE CLIENTE (UAExpert): Solo debug para estas
        if (!opcua_server->isWriteFromClient(sessionId)) {
            return; // Ignorar escrituras internas silenciosamente
        }

        // DEBUG: Información de sesión
        LOG_SUCCESS("🖊️  ESCRITURA DESDE CLIENTE");
        LOG_INFO("📋 SessionId: " + std::to_string(sessionId->namespaceIndex) + 
                 ":" + std::to_string(sessionId->identifier.numeric));

        // Buscar el NodeId en node_map_ para obtener el path del tag
        std::string found_node_path;
        for (const auto& [path, node_id] : opcua_server->node_map_) {
            if (UA_NodeId_equal(nodeId, &node_id)) {
                found_node_path = path;
                break;
            }
        }

        if (found_node_path.empty()) {
            LOG_ERROR("❌ NodeId no encontrado en node_map");
            return;
        }

        LOG_INFO("🎯 Variable: " + found_node_path);

        // Extraer tag parent y variable desde el path (formato: "TAG.Variable")
        size_t dot_pos = found_node_path.find('.');
        if (dot_pos == std::string::npos) {
            LOG_ERROR("❌ Formato de path inválido: " + found_node_path);
            return;
        }
        
        std::string parent_tag = found_node_path.substr(0, dot_pos);
        std::string variable_name = found_node_path.substr(dot_pos + 1);

        LOG_INFO("🏷️  Tag: " + parent_tag + " → Variable: " + variable_name);        // Convertir el valor de OPC UA a float
        float new_value = 0.0f;
        if (data->value.type == &UA_TYPES[UA_TYPES_FLOAT]) {
            new_value = *((float*)data->value.data);
        } else if (data->value.type == &UA_TYPES[UA_TYPES_DOUBLE]) {
            new_value = (float)*((double*)data->value.data);
        } else if (data->value.type == &UA_TYPES[UA_TYPES_INT32]) {
            new_value = (float)*((int32_t*)data->value.data);
        } else {
            LOG_ERROR("❌ Tipo de dato no soportado para " + found_node_path);
            return;
        }

        LOG_SUCCESS("� Valor recibido: " + std::to_string(new_value));

        // Buscar el tag correspondiente en TagManager y actualizarlo
        if (opcua_server->tag_manager_) {
            std::string full_tag_name = parent_tag + "." + variable_name;
            auto tag = opcua_server->tag_manager_->getTag(full_tag_name);
            if (tag) {
                // CRÍTICO: Marcar timestamp de escritura por cliente para evitar sobrescritura
                uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                tag->setClientWriteTimestamp(current_time);
                
                // Actualizar el valor
                tag->setValue(new_value);
                LOG_SUCCESS("✅ CLIENT WRITE: " + full_tag_name + " = " + std::to_string(new_value));
                LOG_SUCCESS("🛡️ PROTECCIÓN ACTIVADA - timestamp: " + std::to_string(current_time));
                
                // 🎯 CRÍTICO: Enviar el valor al PAC Control inmediatamente
                // Usar la variable global g_pac_client desde main.cpp
                extern std::unique_ptr<PACControlClient> g_pac_client;
                if (g_pac_client && g_pac_client->isConnected()) {
                    // DETECTAR SI ES VARIABLE DE ALARMA (requiere tratamiento especial)
                    bool is_alarm_variable = (variable_name == "ALARM_HH" || variable_name == "ALARM_H" || 
                                            variable_name == "ALARM_L" || variable_name == "ALARM_LL" || 
                                            variable_name == "ALARM_Color");
                    
                    if (is_alarm_variable) {
                        // VARIABLES DE ALARMA: Usar tabla TBL_XA_XXXX, índices 0-4, y tipo int32
                        std::string pac_alarm_table = "TBL_XA_" + parent_tag;
                        int alarm_index = -1;
                        
                        // Mapeo correcto para variables de alarma (índices 0-4)
                        if (variable_name == "ALARM_HH") alarm_index = 0;
                        else if (variable_name == "ALARM_H") alarm_index = 1;
                        else if (variable_name == "ALARM_L") alarm_index = 2;
                        else if (variable_name == "ALARM_LL") alarm_index = 3;
                        else if (variable_name == "ALARM_Color") alarm_index = 4;
                        
                        if (alarm_index >= 0) {
                            LOG_INFO("🚨 Enviando ALARMA a PAC: " + pac_alarm_table + "[" + std::to_string(alarm_index) + "] = " + std::to_string((int32_t)new_value));
                            
                            bool alarm_write_success = g_pac_client->writeInt32TableIndex(pac_alarm_table, alarm_index, (int32_t)new_value);
                            if (alarm_write_success) {
                                LOG_SUCCESS("🎉 ÉXITO ALARMA: Enviado a PAC " + pac_alarm_table + "[" + std::to_string(alarm_index) + "]");
                            } else {
                                LOG_ERROR("💥 FALLO ALARMA: No se pudo enviar a PAC");
                            }
                        }
                    } else {
                        // VARIABLES REGULARES: Usar tabla TBL_XXXX y tipo float
                        std::string pac_table_name = "TBL_" + parent_tag;
                        int variable_index = -1;
                        
                        // Determinar tipo de tag basado en prefijo para mapeo correcto
                        if (parent_tag.substr(0, 3) == "PRC" || parent_tag.substr(0, 3) == "FRC" || 
                            parent_tag.substr(0, 3) == "TRC" || parent_tag.substr(0, 3) == "LRC") {
                            // CONTROLLERS: ["PV", "SP", "CV", "KP", "KI", "KD", "auto_manual", "OUTPUT_HIGH", "OUTPUT_LOW", "PID_ENABLE"]
                            if (variable_name == "PV") variable_index = 0;
                            else if (variable_name == "SP") variable_index = 1;
                            else if (variable_name == "CV") variable_index = 2;
                            else if (variable_name == "KP") variable_index = 3;
                            else if (variable_name == "KI") variable_index = 4;
                            else if (variable_name == "KD") variable_index = 5;
                            else if (variable_name == "auto_manual") variable_index = 6;
                            else if (variable_name == "OUTPUT_HIGH") variable_index = 7;
                            else if (variable_name == "OUTPUT_LOW") variable_index = 8;
                            else if (variable_name == "PID_ENABLE") variable_index = 9;
                        } 
                        else if (parent_tag.substr(0, 2) == "ET" || parent_tag.substr(0, 3) == "FIT" || 
                                 parent_tag.substr(0, 3) == "PIT" || parent_tag.substr(0, 3) == "TIT" || 
                                 parent_tag.substr(0, 3) == "LIT" || parent_tag.substr(0, 4) == "PDIT") {
                            // TRANSMITTERS: ["Input", "SetHH", "SetH", "SetL", "SetLL", "SIM_Value", "PV", "min", "max", "percent"]
                            // ⚠️ ADVERTENCIA: Los transmisores suelen ser de solo lectura en el PAC
                            LOG_WARNING("⚠️ TRANSMITTER WRITE: " + parent_tag + "." + variable_name + 
                                      " (transmisores suelen ser read-only)");
                            // NOTA: Variables ALARM_XX ya fueron procesadas arriba
                            if (variable_name == "Input") variable_index = 0;
                            else if (variable_name == "SetHH") variable_index = 1;
                            else if (variable_name == "SetH") variable_index = 2;
                            else if (variable_name == "SetL") variable_index = 3;
                            else if (variable_name == "SetLL") variable_index = 4;
                            else if (variable_name == "SIM_Value") variable_index = 5;
                            else if (variable_name == "PV") variable_index = 6;
                            else if (variable_name == "min") variable_index = 7;
                            else if (variable_name == "max") variable_index = 8;
                            else if (variable_name == "percent") variable_index = 9;
                        }
                        else {
                            // TAG TYPE DESCONOCIDO: intentar mapeo genérico
                            LOG_WARNING("⚠️ Tipo de tag no reconocido: " + parent_tag + ", intentando mapeo genérico");
                            // Mapeo genérico básico para compatibilidad
                            if (variable_name == "PV") variable_index = 0;
                            else if (variable_name == "SP") variable_index = 1;
                            else if (variable_name == "CV") variable_index = 2;
                        }
                        
                        if (variable_index >= 0) {
                            LOG_INFO("📋 Enviando a PAC: " + pac_table_name + "[" + std::to_string(variable_index) + "] = " + std::to_string(new_value));
                            
                            bool write_success = g_pac_client->writeFloatTableIndex(pac_table_name, variable_index, new_value);
                            if (write_success) {
                                LOG_SUCCESS("🎉 ÉXITO: Enviado a PAC " + pac_table_name + "[" + std::to_string(variable_index) + "]");
                            } else {
                                LOG_ERROR("💥 FALLO: No se pudo enviar a PAC");
                            }
                        } else {
                            LOG_ERROR("❌ Variable no mapeada: " + variable_name + " en tag " + parent_tag);
                        }
                    }
                } else {
                    LOG_ERROR("❌ PAC no conectado");
                }
            } else {
                LOG_DEBUG("⚠️ Tag no encontrado en TagManager: " + full_tag_name);
            }
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error en writeCallback: " + std::string(e.what()));
    }
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

std::string OPCUAServer::categorizeTagByName(const std::string& tag_name) {
    // Clasificación simplificada: Solo 2 categorías principales
    
    // Controladores PID - Todos los prefijos de control
    if (tag_name.find("TRC_") == 0 ||   // Temperature Rate Controller
        tag_name.find("PRC_") == 0 ||   // Pressure Rate Controller  
        tag_name.find("FRC_") == 0 ||   // Flow Rate Controller
        tag_name.find("LRC_") == 0) {   // Level Rate Controller
        return "ControladorsPID";
    }
    
    // Instrumentos de Campo - Todos los demás (transmisores, indicadores, etc.)
    // Incluye: ET_, FIT_, PIT_, TIT_, LIT_, PDIT_, etc.
    return "Instrumentos";
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
    
    int error_count = 0;
    while (running_) {
        // Ejecutar un ciclo del servidor con timeout corto
        retval = UA_Server_run_iterate(ua_server_, 100); // 100ms timeout
        
        if (retval != UA_STATUSCODE_GOOD) {
            error_count++;
            // Solo loggear cada 100 errores para evitar spam
            if (error_count % 100 == 1) {
                LOG_WARNING("⚠️ Errores en ciclo del servidor: " + std::to_string(error_count) + " (último código: " + std::to_string(retval) + ")");
            }
        } else {
            // Reset counter on successful iteration
            if (error_count > 0) {
                LOG_INFO("✅ Servidor recuperado después de " + std::to_string(error_count) + " errores");
                error_count = 0;
            }
        }
        
        // Pequeña pausa para no saturar CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Limpiar el servidor
    UA_Server_run_shutdown(ua_server_);
    LOG_INFO("🛑 Hilo del servidor OPC UA terminado");
}