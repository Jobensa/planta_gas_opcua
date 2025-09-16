/*
 * tag_management_api.cpp - Implementación del servidor API HTTP
 * 
 * Implementa funcionalidades CRUD para tags con hot reload
 */

#include "tag_management_api.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace TagManagementAPI {

TagManagementServer::TagManagementServer(std::shared_ptr<TagManager> tag_manager, 
                                       const std::string& config_file)
    : tag_manager_(tag_manager)
    , config_file_path_(config_file)
    , server_running_(false)
    , server_port_(8081)
{
    // Configurar directorio de backups
    backup_directory_ = "backups";
    std::filesystem::create_directories(backup_directory_);
    
    // Inicializar servidor HTTP
    http_server_ = std::make_unique<httplib::Server>();
    
    LOG_INFO("🌐 TagManagementServer inicializado");
    LOG_INFO("   📁 Config: " + config_file_path_);
    LOG_INFO("   💾 Backups: " + backup_directory_);
}

TagManagementServer::~TagManagementServer() {
    stopServer();
}

bool TagManagementServer::startServer(int port) {
    if (server_running_) {
        LOG_WARNING("Servidor API ya está ejecutándose en puerto " + std::to_string(server_port_));
        return true;
    }
    
    server_port_ = port;
    server_config_.port = port;
    
    try {
        // Configurar servidor
        setupRoutes();
        configureCORS();
        configureMiddleware();
        
        // Crear backup automático al iniciar
        createAutomaticBackup("server_start");
        
        // Iniciar servidor en hilo separado
        server_thread_ = std::make_unique<std::thread>(&TagManagementServer::serverThreadFunction, this);
        
        // Esperar a que el servidor inicie
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (server_running_) {
            LOG_SUCCESS("🚀 Servidor API iniciado en http://localhost:" + std::to_string(port));
            return true;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error al iniciar servidor API: " + std::string(e.what()));
    }
    
    return false;
}

void TagManagementServer::stopServer() {
    if (!server_running_) {
        return;
    }
    
    LOG_INFO("🛑 Deteniendo servidor API...");
    
    server_running_ = false;
    
    if (http_server_) {
        http_server_->stop();
    }
    
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    LOG_SUCCESS("✅ Servidor API detenido");
}

void TagManagementServer::setupRoutes() {
    auto server = http_server_.get();
    
    // === TAG CRUD OPERATIONS ===
    server->Get("/api/tags", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetAllTags(req, res);
    });
    
    server->Get(R"(/api/tags/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetTag(req, res);
    });
    
    server->Post("/api/tags", [this](const httplib::Request& req, httplib::Response& res) {
        handleCreateTag(req, res);
    });
    
    server->Put(R"(/api/tags/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleUpdateTag(req, res);
    });
    
    server->Delete(R"(/api/tags/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleDeleteTag(req, res);
    });
    
    // === TBL_OPCUA MANAGEMENT ===
    server->Get("/api/opcua-table", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetOPCUATable(req, res);
    });
    
    server->Put(R"(/api/opcua-table/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleAssignOPCUAIndex(req, res);
    });
    
    server->Delete(R"(/api/opcua-table/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleRemoveOPCUAIndex(req, res);
    });
    
    server->Get("/api/opcua-table/available", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetAvailableOPCUAIndices(req, res);
    });
    
    // === CONFIGURATION MANAGEMENT ===
    server->Get("/api/config", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetConfiguration(req, res);
    });
    
    server->Post("/api/config/save", [this](const httplib::Request& req, httplib::Response& res) {
        handleSaveConfiguration(req, res);
    });
    
    server->Post("/api/config/reload", [this](const httplib::Request& req, httplib::Response& res) {
        handleReloadConfiguration(req, res);
    });
    
    // === BACKUP SYSTEM ===
    server->Get("/api/backups", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetBackups(req, res);
    });
    
    server->Post("/api/backups/create", [this](const httplib::Request& req, httplib::Response& res) {
        handleCreateBackup(req, res);
    });
    
    server->Post(R"(/api/backups/restore/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleRestoreBackup(req, res);
    });
    
    // === VALIDATION & PREVIEW ===
    server->Post("/api/validate", [this](const httplib::Request& req, httplib::Response& res) {
        handleValidateConfiguration(req, res);
    });
    
    server->Get("/api/preview/opcua", [this](const httplib::Request& req, httplib::Response& res) {
        handlePreviewOPCUA(req, res);
    });
    
    // === SYSTEM STATUS ===
    server->Get("/api/status", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetSystemStatus(req, res);
    });
    
    server->Get("/api/statistics", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetStatistics(req, res);
    });
    
    server->Get("/api/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealthCheck(req, res);
    });
    
    // === TEMPLATES ===
    server->Get("/api/templates", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetTemplates(req, res);
    });
    
    LOG_INFO("📋 Rutas API configuradas (15 endpoints)");
}

void TagManagementServer::configureCORS() {
    auto server = http_server_.get();
    
    // Middleware CORS
    server->set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // Handle OPTIONS requests
    server->Options(".*", [](const httplib::Request&, httplib::Response& res) {
        return;
    });
}

void TagManagementServer::configureMiddleware() {
    auto server = http_server_.get();
    
    // Logging middleware
    server->set_logger([this](const httplib::Request& req, const httplib::Response& res) {
        logAPICall(req.method, req.path, req.get_header_value("Host"));
    });
    
    // Configurar límites
    server->set_payload_max_length(MAX_REQUEST_SIZE);
    server->set_read_timeout(DEFAULT_TIMEOUT_SECONDS);
    server->set_write_timeout(DEFAULT_TIMEOUT_SECONDS);
}

// === TAG CRUD OPERATIONS IMPLEMENTATION ===

void TagManagementServer::handleGetAllTags(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        const auto& tags = tag_manager_->getTags();
        nlohmann::json tag_list = nlohmann::json::array();
        
        for (const auto& tag : tags) {
            nlohmann::json tag_json = {
                {"name", tag.name},
                {"opcua_name", tag.opcua_name},
                {"description", tag.description},
                {"units", tag.units},
                {"category", tag.category},
                {"variable_count", tag.variables.size()},
                {"alarm_count", tag.alarms.size()},
                {"last_update", std::chrono::duration_cast<std::chrono::seconds>(
                    tag.last_fast_update.time_since_epoch()).count()}
            };
            
            // Incluir información de TBL_OPCUA si aplica
            // Note: opcua_table_index information would need to be retrieved from elsewhere
            tag_json["is_critical"] = false; // Default value since opcua_table_index is not available
            
            tag_list.push_back(tag_json);
        }
        
        auto response = APIResponse::Success(tag_list, "Tags retrieved successfully");
        sendResponse(res, response);
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error retrieving tags: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handleGetTag(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        std::string tag_name = req.matches[1];
        const auto* tag = tag_manager_->getTag(tag_name);
        
        if (!tag) {
            sendErrorResponse(res, "Tag not found: " + tag_name, 404);
            return;
        }
        
        // Construir JSON completo del tag
        nlohmann::json tag_json = {
            {"name", tag->name},
            {"opcua_name", tag->opcua_name},
            {"value_table", tag->value_table},
            {"alarm_table", tag->alarm_table},
            {"description", tag->description},
            {"units", tag->units},
            {"category", tag->category},
            {"associated_instrument", tag->associated_instrument},
            {"variables", nlohmann::json::object()},
            {"alarms", nlohmann::json::object()},
            {"statistics", {
                {"total_updates", tag->total_updates},
                {"fast_updates", tag->fast_updates},
                {"medium_updates", tag->medium_updates},
                {"slow_updates", tag->slow_updates}
            }}
        };
        
        // Agregar variables
        for (const auto& [name, var] : tag->variables) {
            tag_json["variables"][name] = {
                {"type", variableTypeToString(var.type)},
                {"writable", var.writable},
                {"polling_group", pollingGroupToString(var.polling_group)},
                {"description", var.description},
                {"current_value", var.float_value}, // Simplificado por ahora
                {"last_update", std::chrono::duration_cast<std::chrono::seconds>(
                    var.last_update.time_since_epoch()).count()}
            };
        }
        
        // Agregar alarmas
        for (const auto& [name, alarm] : tag->alarms) {
            tag_json["alarms"][name] = {
                {"type", variableTypeToString(alarm.type)},
                {"polling_group", pollingGroupToString(alarm.polling_group)},
                {"description", alarm.description},
                {"current_value", alarm.int_value}
            };
        }
        
        auto response = APIResponse::Success(tag_json, "Tag retrieved successfully");
        sendResponse(res, response);
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error retrieving tag: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handleCreateTag(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        nlohmann::json tag_config;
        if (!validateJSON(req.body, tag_config)) {
            sendErrorResponse(res, "Invalid JSON in request body", 400);
            return;
        }
        
        // Validar configuración del tag
        auto validation_errors = validateTagConfiguration(tag_config);
        if (!validation_errors.empty()) {
            nlohmann::json error_response = {
                {"validation_errors", validation_errors}
            };
            sendErrorResponse(res, "Tag validation failed", 400);
            return;
        }
        
        // Crear backup antes de modificar
        createAutomaticBackup("create_tag_" + tag_config["name"].get<std::string>());
        
        // Crear tag
        if (createTagInternal(tag_config)) {
            // Hot reload configuración
            tag_manager_->loadConfiguration(config_file_path_);
            
            auto response = APIResponse::Success({}, "Tag created successfully");
            sendResponse(res, response);
            
            logAPISuccess("CREATE_TAG", "Tag: " + tag_config["name"].get<std::string>());
        } else {
            sendErrorResponse(res, "Failed to create tag", 500);
        }
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error creating tag: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handleUpdateTag(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        std::string tag_name = req.matches[1];
        nlohmann::json updates;
        
        if (!validateJSON(req.body, updates)) {
            sendErrorResponse(res, "Invalid JSON in request body", 400);
            return;
        }
        
        // Verificar que el tag existe
        if (!tag_manager_->getTag(tag_name)) {
            sendErrorResponse(res, "Tag not found: " + tag_name, 404);
            return;
        }
        
        // Crear backup antes de modificar
        createAutomaticBackup("update_tag_" + tag_name);
        
        // Actualizar tag
        if (updateTagInternal(tag_name, updates)) {
            // Hot reload configuración
            tag_manager_->loadConfiguration(config_file_path_);
            
            auto response = APIResponse::Success({}, "Tag updated successfully");
            sendResponse(res, response);
            
            logAPISuccess("UPDATE_TAG", "Tag: " + tag_name);
        } else {
            sendErrorResponse(res, "Failed to update tag", 500);
        }
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error updating tag: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handleDeleteTag(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        std::string tag_name = req.matches[1];
        
        // Verificar que el tag existe
        if (!tag_manager_->getTag(tag_name)) {
            sendErrorResponse(res, "Tag not found: " + tag_name, 404);
            return;
        }
        
        // Crear backup antes de borrar
        createAutomaticBackup("delete_tag_" + tag_name);
        
        // Borrar tag
        if (deleteTagInternal(tag_name)) {
            // Hot reload configuración
            tag_manager_->loadConfiguration(config_file_path_);
            
            auto response = APIResponse::Success({}, "Tag deleted successfully");
            sendResponse(res, response);
            
            logAPISuccess("DELETE_TAG", "Tag: " + tag_name);
        } else {
            sendErrorResponse(res, "Failed to delete tag", 500);
        }
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error deleting tag: " + std::string(e.what()), 500);
    }
}

// === TBL_OPCUA MANAGEMENT IMPLEMENTATION ===

void TagManagementServer::handleGetOPCUATable(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        nlohmann::json opcua_table = buildOPCUATableStatus();
        auto response = APIResponse::Success(opcua_table, "TBL_OPCUA status retrieved");
        sendResponse(res, response);
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error retrieving TBL_OPCUA: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handleAssignOPCUAIndex(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        int index = std::stoi(req.matches[1]);
        
        if (!validateOPCUAIndex(index)) {
            sendErrorResponse(res, "Invalid TBL_OPCUA index: " + std::to_string(index), 400);
            return;
        }
        
        nlohmann::json assignment;
        if (!validateJSON(req.body, assignment)) {
            sendErrorResponse(res, "Invalid JSON in request body", 400);
            return;
        }
        
        if (!assignment.contains("tag_name") || !assignment.contains("variable_name")) {
            sendErrorResponse(res, "Missing tag_name or variable_name in request", 400);
            return;
        }
        
        std::string tag_name = assignment["tag_name"];
        std::string var_name = assignment["variable_name"];
        
        // Crear backup antes de modificar
        createAutomaticBackup("assign_opcua_" + std::to_string(index));
        
        // Asignar variable al índice
        if (assignVariableToOPCUAIndex(index, tag_name, var_name)) {
            auto response = APIResponse::Success({}, 
                "Variable assigned to TBL_OPCUA[" + std::to_string(index) + "]");
            sendResponse(res, response);
            
            logAPISuccess("ASSIGN_OPCUA", 
                "Index " + std::to_string(index) + ": " + tag_name + "." + var_name);
        } else {
            sendErrorResponse(res, "Failed to assign variable to TBL_OPCUA", 500);
        }
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error assigning TBL_OPCUA index: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handleGetAvailableOPCUAIndices(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        auto available_indices = getAvailableOPCUAIndices();
        
        nlohmann::json result = {
            {"available_indices", available_indices},
            {"total_available", available_indices.size()},
            {"total_capacity", 52},
            {"utilization_percent", ((52 - available_indices.size()) * 100) / 52}
        };
        
        auto response = APIResponse::Success(result, "Available TBL_OPCUA indices retrieved");
        sendResponse(res, response);
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error getting available indices: " + std::string(e.what()), 500);
    }
}

// === CONFIGURATION MANAGEMENT IMPLEMENTATION ===

void TagManagementServer::handleGetConfiguration(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        nlohmann::json config = generateConfigurationJSON();
        auto response = APIResponse::Success(config, "Configuration retrieved");
        sendResponse(res, response);
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error retrieving configuration: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handleSaveConfiguration(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        // Crear backup antes de guardar
        createAutomaticBackup("manual_save");
        
        // Guardar configuración actual
        if (saveConfigurationToFile(config_file_path_)) {
            auto response = APIResponse::Success({}, "Configuration saved successfully");
            sendResponse(res, response);
            
            logAPISuccess("SAVE_CONFIG", "File: " + config_file_path_);
        } else {
            sendErrorResponse(res, "Failed to save configuration", 500);
        }
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error saving configuration: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handleReloadConfiguration(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        // Validar configuración antes de recargar
        auto validation_errors = validateSystemConfiguration();
        if (!validation_errors.empty()) {
            nlohmann::json error_response = {
                {"validation_errors", validation_errors}
            };
            sendErrorResponse(res, "Configuration validation failed", 400);
            return;
        }
        
        // Hot reload
        if (tag_manager_->loadConfiguration(config_file_path_)) {
            auto response = APIResponse::Success({}, "Configuration reloaded successfully");
            sendResponse(res, response);
            
            logAPISuccess("RELOAD_CONFIG", "Hot reload completed");
        } else {
            sendErrorResponse(res, "Failed to reload configuration", 500);
        }
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error reloading configuration: " + std::string(e.what()), 500);
    }
}

// === BACKUP SYSTEM IMPLEMENTATION ===

void TagManagementServer::handleGetBackups(const httplib::Request& req, httplib::Response& res) {
    try {
        auto backup_files = listBackupFiles();
        
        nlohmann::json backups = nlohmann::json::array();
        for (const auto& filename : backup_files) {
            std::filesystem::path filepath = std::filesystem::path(backup_directory_) / filename;
            
            nlohmann::json backup_info = {
                {"filename", filename},
                {"size_bytes", std::filesystem::file_size(filepath)},
                {"created_time", std::filesystem::last_write_time(filepath).time_since_epoch().count()},
                {"is_auto", filename.find("auto_") == 0}
            };
            
            backups.push_back(backup_info);
        }
        
        nlohmann::json result = {
            {"backups", backups},
            {"total_count", backups.size()},
            {"backup_directory", backup_directory_}
        };
        
        auto response = APIResponse::Success(result, "Backup files retrieved");
        sendResponse(res, response);
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error retrieving backups: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handleCreateBackup(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(api_mutex_);
    
    try {
        std::string backup_filename = generateBackupFilename("manual");
        
        if (saveConfigurationToFile(std::filesystem::path(backup_directory_) / backup_filename)) {
            nlohmann::json result = {
                {"backup_filename", backup_filename},
                {"backup_path", std::filesystem::path(backup_directory_) / backup_filename}
            };
            
            auto response = APIResponse::Success(result, "Backup created successfully");
            sendResponse(res, response);
            
            logAPISuccess("CREATE_BACKUP", "File: " + backup_filename);
        } else {
            sendErrorResponse(res, "Failed to create backup", 500);
        }
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error creating backup: " + std::string(e.what()), 500);
    }
}

// === VALIDATION & PREVIEW IMPLEMENTATION ===

void TagManagementServer::handleValidateConfiguration(const httplib::Request& req, httplib::Response& res) {
    try {
        auto validation_errors = validateSystemConfiguration();
        
        nlohmann::json result = {
            {"is_valid", validation_errors.empty()},
            {"errors", validation_errors},
            {"error_count", validation_errors.size()},
            {"validation_time", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        auto response = APIResponse::Success(result, 
            validation_errors.empty() ? "Configuration is valid" : "Configuration has errors");
        sendResponse(res, response);
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error validating configuration: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handlePreviewOPCUA(const httplib::Request& req, httplib::Response& res) {
    try {
        nlohmann::json opcua_structure = generateOPCUAStructurePreview();
        auto response = APIResponse::Success(opcua_structure, "OPC UA structure preview generated");
        sendResponse(res, response);
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error generating OPC UA preview: " + std::string(e.what()), 500);
    }
}

// === SYSTEM STATUS IMPLEMENTATION ===

void TagManagementServer::handleGetSystemStatus(const httplib::Request& req, httplib::Response& res) {
    try {
        const auto& stats = tag_manager_->getStatistics();
        
        nlohmann::json status = {
            {"system_running", tag_manager_->is_running()},
            {"api_running", server_running_.load()},
            {"total_tags", stats.total_tags},
            {"total_variables", stats.total_variables},
            {"successful_updates", stats.successful_updates},
            {"failed_updates", stats.failed_updates},
            {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - stats.start_time).count()},
            {"config_file", config_file_path_},
            {"backup_directory", backup_directory_},
            {"last_config_save", std::filesystem::last_write_time(config_file_path_).time_since_epoch().count()}
        };
        
        auto response = APIResponse::Success(status, "System status retrieved");
        sendResponse(res, response);
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error retrieving system status: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handleGetStatistics(const httplib::Request& req, httplib::Response& res) {
    try {
        const auto& stats = tag_manager_->getStatistics();
        
        nlohmann::json statistics = {
            {"total_tags", stats.total_tags},
            {"total_variables", stats.total_variables},
            {"fast_polling_vars", stats.fast_polling_vars},
            {"medium_polling_vars", stats.medium_polling_vars},
            {"slow_polling_vars", stats.slow_polling_vars},
            {"writable_variables", stats.writable_variables},
            {"readonly_variables", stats.readonly_variables},
            {"total_updates", stats.total_updates},
            {"successful_updates", stats.successful_updates},
            {"failed_updates", stats.failed_updates},
            {"avg_fast_latency_ms", stats.avg_fast_latency_ms},
            {"avg_medium_latency_ms", stats.avg_medium_latency_ms},
            {"avg_slow_latency_ms", stats.avg_slow_latency_ms},
            {"success_rate_percent", stats.total_updates > 0 ? 
                (stats.successful_updates * 100.0) / stats.total_updates : 100.0}
        };
        
        auto response = APIResponse::Success(statistics, "Statistics retrieved");
        sendResponse(res, response);
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error retrieving statistics: " + std::string(e.what()), 500);
    }
}

void TagManagementServer::handleHealthCheck(const httplib::Request& req, httplib::Response& res) {
    nlohmann::json health = {
        {"status", "healthy"},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"version", "1.0.0"},
        {"services", {
            {"tag_manager", tag_manager_->is_running()},
            {"api_server", server_running_.load()},
            {"config_valid", std::filesystem::exists(config_file_path_)}
        }}
    };
    
    auto response = APIResponse::Success(health, "System healthy");
    sendResponse(res, response);
}

// === TEMPLATES IMPLEMENTATION ===

void TagManagementServer::handleGetTemplates(const httplib::Request& req, httplib::Response& res) {
    try {
        nlohmann::json templates = getTagTemplates();
        auto response = APIResponse::Success(templates, "Tag templates retrieved");
        sendResponse(res, response);
        
    } catch (const std::exception& e) {
        sendErrorResponse(res, "Error retrieving templates: " + std::string(e.what()), 500);
    }
}

// === UTILITY FUNCTIONS IMPLEMENTATION ===

void TagManagementServer::sendResponse(httplib::Response& res, const APIResponse& response) {
    res.set_header("Content-Type", "application/json");
    res.status = response.status_code;
    res.body = response.toJSON().dump(2);
}

void TagManagementServer::sendJSONResponse(httplib::Response& res, const nlohmann::json& data, int status) {
    res.set_header("Content-Type", "application/json");
    res.status = status;
    res.body = data.dump(2);
}

void TagManagementServer::sendErrorResponse(httplib::Response& res, const std::string& error, int status) {
    auto response = APIResponse::Error(error, status);
    sendResponse(res, response);
    logAPIError("API_ERROR", error);
}

bool TagManagementServer::validateJSON(const std::string& json_str, nlohmann::json& parsed) {
    try {
        parsed = nlohmann::json::parse(json_str);
        return true;
    } catch (const std::exception& e) {
        logAPIError("JSON_VALIDATION", "Invalid JSON: " + std::string(e.what()));
        return false;
    }
}

bool TagManagementServer::validateTagName(const std::string& name) {
    return !name.empty() && name.length() <= 64 && 
           name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos;
}

bool TagManagementServer::validateOPCUAIndex(int index) {
    return index >= 0 && index < 52;
}

bool TagManagementServer::createAutomaticBackup(const std::string& operation) {
    try {
        std::string backup_filename = generateBackupFilename("auto_" + operation);
        std::filesystem::path backup_path = std::filesystem::path(backup_directory_) / backup_filename;
        
        if (saveConfigurationToFile(backup_path)) {
            // Limpiar backups antiguos
            cleanOldBackups(MAX_BACKUP_FILES);
            
            LOG_DEBUG("💾 Backup automático creado: " + backup_filename);
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error creating automatic backup: " + std::string(e.what()));
    }
    return false;
}

std::string TagManagementServer::generateBackupFilename(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << prefix << "_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".json";
    return ss.str();
}

void TagManagementServer::serverThreadFunction() {
    try {
        server_running_ = true;
        
        if (!http_server_->listen(server_config_.bind_address, server_config_.port)) {
            LOG_ERROR("Failed to start HTTP server on " + server_config_.bind_address + 
                     ":" + std::to_string(server_config_.port));
            server_running_ = false;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Server thread exception: " + std::string(e.what()));
        server_running_ = false;
    }
}

void TagManagementServer::logAPICall(const std::string& method, const std::string& path, const std::string& client_ip) {
    LOG_DEBUG("🌐 API: " + method + " " + path + " from " + client_ip);
}

void TagManagementServer::logAPIError(const std::string& operation, const std::string& error) {
    LOG_ERROR("API " + operation + ": " + error);
}

void TagManagementServer::logAPISuccess(const std::string& operation, const std::string& details) {
    LOG_SUCCESS("API " + operation + (details.empty() ? "" : ": " + details));
}

// === FACTORY FUNCTIONS ===

std::unique_ptr<TagManagerWithAPI> createTagManagerWithAPI(const std::string& config_file, int api_port) {
    auto manager = std::make_unique<TagManagerWithAPI>(config_file);
    
    if (manager->initialize()) {
        if (manager->enableAPI(api_port)) {
            return manager;
        }
    }
    
    return nullptr;
}

std::unique_ptr<TagManagementServer> createTagManagementServer(
    std::shared_ptr<TagManager> tag_manager, 
    const std::string& config_file) {
    
    auto server = std::make_unique<TagManagementServer>(tag_manager, config_file);
    return server;
}

} // namespace TagManagementAPI