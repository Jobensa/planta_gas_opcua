
/*
 * pac_control_client.cpp - Implementación del cliente PAC adaptado
 */

#include "pac_control_client.h"
#include "tag_manager.h"
#include "common.h"
#include <sstream>
#include <iomanip>

// Constructor adaptado para shared_ptr (nueva versión)
PACControlClient::PACControlClient(std::shared_ptr<TagManager> tag_manager)
    : tag_manager_(tag_manager)
    , pac_ip_("192.168.1.30")
    , pac_port_(22001)
    , timeout_ms_(5000)
    , connected_(false)
    , enabled_(true)
    , curl_handle_(nullptr)
{
    opcua_table_cache_.resize(52, 0.0f);
    stats_.last_success = std::chrono::steady_clock::now();
    
    if (!initializeCURL()) {
        LOG_ERROR("Failed to initialize CURL for PAC client");
        enabled_ = false;
    }
    
    // Cargar mapeo de TBL_OPCUA desde configuración
    if (!loadTagOPCUAMapping("config/tags_planta_gas.json")) {
        LOG_WARNING("⚠️ No se pudo cargar mapeo TBL_OPCUA, funcionará en modo básico");
    }
    
    LOG_INFO("🔌 PACControlClient inicializado (shared_ptr version)");
}

// Constructor de compatibilidad (versión antigua)
PACControlClient::PACControlClient(TagManager* tag_manager)
    : PACControlClient(std::shared_ptr<TagManager>(tag_manager, [](TagManager*){})) // No-op deleter
{
    LOG_INFO("🔌 PACControlClient inicializado (compatibility version)");
}

PACControlClient::~PACControlClient() {
    disconnect();
    cleanupCURL();
}

bool PACControlClient::connect() {
    if (!enabled_) {
        LOG_ERROR("PAC client is disabled");
        return false;
    }
    
    if (connected_) {
        LOG_WARNING("PAC client already connected");
        return true;
    }
    
    LOG_INFO("🔌 Conectando al PAC " + pac_ip_ + ":" + std::to_string(pac_port_) + "...");
    
    // Test de conectividad básica
    std::string test_url = buildReadURL("TBL_OPCUA", 0);
    std::string response = performHTTPRequest(test_url);
    
    if (!response.empty()) {
        connected_ = true;
        LOG_SUCCESS("✅ Conectado al PAC exitosamente");
        
        // Leer TBL_OPCUA inicial
        if (readOPCUATable()) {
            LOG_SUCCESS("📊 TBL_OPCUA leída exitosamente (" + std::to_string(opcua_table_cache_.size()) + " variables)");
        }
        
        return true;
    }
    
    LOG_ERROR("❌ Error conectando al PAC");
    return false;
}

void PACControlClient::disconnect() {
    if (connected_) {
        connected_ = false;
        LOG_INFO("🔌 Desconectado del PAC");
    }
}

void PACControlClient::setConnectionParams(const std::string& ip, int port) {
    pac_ip_ = ip;
    pac_port_ = port;
    LOG_INFO("📝 Configuración PAC actualizada: " + ip + ":" + std::to_string(port));
}

void PACControlClient::setCredentials(const std::string& username, const std::string& password) {
    username_ = username;
    password_ = password;
    LOG_DEBUG("🔐 Credenciales PAC actualizadas");
}

// OPTIMIZACIÓN CRÍTICA: Leer tabla completa TBL_OPCUA
bool PACControlClient::readOPCUATable() {
    if (!connected_ || !enabled_) {
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        std::string url = buildOPCUATableURL();
        std::string response = performHTTPRequest(url);
        
        if (response.empty()) {
            LOG_ERROR("Empty response from TBL_OPCUA");
            updateStats(false, 0);
            return false;
        }
        
        // Parsear respuesta JSON
        nlohmann::json json_data = nlohmann::json::parse(response);
        
        if (!json_data.is_array()) {
            LOG_ERROR("TBL_OPCUA response is not an array");
            updateStats(false, 0);
            return false;
        }
        
        // Copiar datos al cache
        size_t values_read = 0;
        for (size_t i = 0; i < json_data.size() && i < opcua_table_cache_.size(); ++i) {
            if (json_data[i].is_number()) {
                opcua_table_cache_[i] = json_data[i].get<float>();
                values_read++;
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        last_opcua_read_ = end_time;
        stats_.opcua_table_reads++;
        updateStats(true, elapsed.count());
        
        // Actualizar TagManager con los datos críticos
        if (updateTagManagerFromOPCUATable()) {
            LOG_DEBUG("📊 TBL_OPCUA: " + std::to_string(values_read) + " variables en " + 
                     std::to_string(elapsed.count()) + "ms");
            return true;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error reading TBL_OPCUA: " + std::string(e.what()));
        updateStats(false, 0);
    }
    
    return false;
}

// Lectura individual para variables no críticas
std::string PACControlClient::readVariable(const std::string& table_name, int index) {
    if (!connected_ || !enabled_) {
        return "";
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::string url = buildReadURL(table_name, index);
    std::string response = performHTTPRequest(url);
    
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    updateStats(!response.empty(), elapsed.count());
    
    return response;
}

float PACControlClient::readFloatVariable(const std::string& table_name, int index) {
    std::string response = readVariable(table_name, index);
    float value = 0.0f;
    
    if (parseFloatResponse(response, value)) {
        return value;
    }
    
    return 0.0f;
}

int PACControlClient::readIntVariable(const std::string& table_name, int index) {
    std::string response = readVariable(table_name, index);
    int value = 0;
    
    if (parseIntResponse(response, value)) {
        return value;
    }
    
    return 0;
}

bool PACControlClient::writeVariable(const std::string& table_name, int index, float value) {
    if (!connected_ || !enabled_) {
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        std::string url = buildWriteURL(table_name, index);
        
        // Crear payload JSON para escritura
        nlohmann::json payload = {
            {"value", value}
        };
        
        std::string response = performHTTPPOST(url, payload.dump());
        
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        bool success = !response.empty();
        updateStats(success, elapsed.count());
        
        if (success) {
            stats_.successful_writes++;
            LOG_WRITE("📝 Write: " + table_name + "[" + std::to_string(index) + "] = " + std::to_string(value));
        } else {
            stats_.failed_writes++;
            LOG_ERROR("Failed write: " + table_name + "[" + std::to_string(index) + "]");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error writing variable: " + std::string(e.what()));
        stats_.failed_writes++;
        return false;
    }
}

bool PACControlClient::writeVariable(const std::string& table_name, int index, int value) {
    return writeVariable(table_name, index, static_cast<float>(value));
}

// Implementaciones privadas

bool PACControlClient::initializeCURL() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    std::lock_guard<std::mutex> lock(curl_mutex_);
    curl_handle_ = curl_easy_init();
    
    if (!curl_handle_) {
        LOG_ERROR("Failed to initialize CURL handle");
        return false;
    }
    
    // Configurar opciones básicas de CURL
    curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT_MS, timeout_ms_);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_handle_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle_, CURLOPT_USERAGENT, "PlantaGas-PAC-Client/1.0");
    
    // Configurar autenticación si está disponible
    if (!username_.empty() && !password_.empty()) {
        std::string credentials = username_ + ":" + password_;
        curl_easy_setopt(curl_handle_, CURLOPT_USERPWD, credentials.c_str());
    }
    
    return true;
}

void PACControlClient::cleanupCURL() {
    std::lock_guard<std::mutex> lock(curl_mutex_);
    
    if (curl_handle_) {
        curl_easy_cleanup(curl_handle_);
        curl_handle_ = nullptr;
    }
    
    curl_global_cleanup();
}

std::string PACControlClient::performHTTPRequest(const std::string& url) {
    if (!curl_handle_) {
        return "";
    }
    
    std::lock_guard<std::mutex> lock(curl_mutex_);
    std::string response_data;
    
    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl_handle_, CURLOPT_HTTPGET, 1L);
    
    CURLcode res = curl_easy_perform(curl_handle_);
    
    if (res != CURLE_OK) {
        LOG_ERROR("CURL error: " + std::string(curl_easy_strerror(res)));
        return "";
    }
    
    long response_code;
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (response_code != 200) {
        LOG_ERROR("HTTP error: " + std::to_string(response_code));
        return "";
    }
    
    return response_data;
}

std::string PACControlClient::performHTTPPOST(const std::string& url, const std::string& data) {
    if (!curl_handle_) {
        return "";
    }
    
    std::lock_guard<std::mutex> lock(curl_mutex_);
    std::string response_data;
    
    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDSIZE, data.length());
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl_handle_);
    
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        LOG_ERROR("CURL POST error: " + std::string(curl_easy_strerror(res)));
        return "";
    }
    
    return response_data;
}

std::string PACControlClient::buildReadURL(const std::string& table_name, int index) const {
    std::stringstream ss;
    ss << "http://" << pac_ip_ << ":" << pac_port_ << "/api/v1/device/strategy/tables/floats/" << table_name;
    
    if (index >= 0) {
        ss << "/" << index;
    }
    
    return ss.str();
}

std::string PACControlClient::buildWriteURL(const std::string& table_name, int index) const {
    std::stringstream ss;
    ss << "http://" << pac_ip_ << ":" << pac_port_ << "/api/v1/device/strategy/tables/floats/" << table_name << "/" << index;
    return ss.str();
}

std::string PACControlClient::buildOPCUATableURL() const {
    return "http://" + pac_ip_ + ":" + std::to_string(pac_port_) + "/api/v1/device/strategy/tables/floats/TBL_OPCUA";
}

bool PACControlClient::parseFloatResponse(const std::string& response, float& value) {
    try {
        nlohmann::json json_data = nlohmann::json::parse(response);
        
        if (json_data.is_number()) {
            value = json_data.get<float>();
            return true;
        } else if (json_data.is_array() && !json_data.empty() && json_data[0].is_number()) {
            value = json_data[0].get<float>();
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing float response: " + std::string(e.what()));
    }
    
    return false;
}

bool PACControlClient::parseIntResponse(const std::string& response, int& value) {
    try {
        nlohmann::json json_data = nlohmann::json::parse(response);
        
        if (json_data.is_number()) {
            value = json_data.get<int>();
            return true;
        } else if (json_data.is_array() && !json_data.empty() && json_data[0].is_number()) {
            value = json_data[0].get<int>();
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing int response: " + std::string(e.what()));
    }
    
    return false;
}

bool PACControlClient::updateTagManagerFromOPCUATable() {
    // Esta función actualiza el TagManager con los datos de TBL_OPCUA
    // Implementación basada en el mapeo del archivo tags_planta_gas.json
    
    if (!tag_manager_) {
        LOG_ERROR("TagManager no disponible para actualización TBL_OPCUA");
        return false;
    }
    
    if (opcua_table_cache_.empty()) {
        LOG_WARNING("Cache TBL_OPCUA vacío, no hay datos para actualizar");
        return false;
    }
    
    size_t updates_processed = 0;
    auto all_tags = tag_manager_->getAllTags();
    
    for (auto& tag : all_tags) {
        if (!tag) continue;
        
        // Buscar el opcua_table_index en la configuración del tag
        // Intentamos extraer el índice del tag mediante su configuración
        try {
            // Para cada tag, buscamos su variable PV y la actualizamos con el valor de TBL_OPCUA
            std::string tag_name = tag->getName();
            
            // Buscar si este tag tiene un índice en TBL_OPCUA
            int opcua_index = getTagOPCUATableIndex(tag_name);
            
            if (opcua_index >= 0 && opcua_index < static_cast<int>(opcua_table_cache_.size())) {
                // Actualizar el valor PV del tag con el valor de TBL_OPCUA
                float new_value = opcua_table_cache_[opcua_index];
                
                // Crear un sub-tag para PV si no existe, o actualizarlo
                std::string pv_tag_name = tag_name + "_PV";
                auto pv_tag = tag_manager_->getTag(pv_tag_name);
                
                if (pv_tag) {
                    TagValue new_tag_value = new_value; // Direct assignment to variant
                    tag_manager_->updateTagValue(pv_tag_name, new_tag_value);
                    updates_processed++;
                } else {
                    // Si no existe el tag PV específico, actualizar el tag principal
                    TagValue new_tag_value = new_value; // Direct assignment to variant
                    tag_manager_->updateTagValue(tag_name, new_tag_value);
                    updates_processed++;
                }
                
                LOG_DEBUG("Actualizado " + tag_name + " [índice " + std::to_string(opcua_index) + "] = " + std::to_string(new_value));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error actualizando tag " + tag->getName() + ": " + std::string(e.what()));
        }
    }
    
    if (updates_processed > 0) {
        LOG_DEBUG("TBL_OPCUA: Actualizados " + std::to_string(updates_processed) + " tags PV");
        return true;
    }
    
    return false;
}

int PACControlClient::getTagOPCUATableIndex(const std::string& tag_name) const {
    auto it = tag_opcua_index_map_.find(tag_name);
    if (it != tag_opcua_index_map_.end()) {
        return it->second;
    }
    return -1; // No encontrado
}

bool PACControlClient::loadTagOPCUAMapping(const std::string& config_file) {
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            LOG_ERROR("No se pudo abrir archivo de configuración: " + config_file);
            return false;
        }
        
        nlohmann::json config;
        file >> config;
        
        if (!config.contains("tags") || !config["tags"].is_array()) {
            LOG_ERROR("Configuración inválida: no se encontró array 'tags'");
            return false;
        }
        
        size_t mappings_loaded = 0;
        tag_opcua_index_map_.clear();
        
        for (const auto& tag_config : config["tags"]) {
            if (tag_config.contains("name") && tag_config.contains("opcua_table_index")) {
                std::string tag_name = tag_config["name"];
                int opcua_index = tag_config["opcua_table_index"];
                
                tag_opcua_index_map_[tag_name] = opcua_index;
                mappings_loaded++;
            }
        }
        
        LOG_SUCCESS("✅ Cargados " + std::to_string(mappings_loaded) + " mapeos TBL_OPCUA desde " + config_file);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error cargando mapeo TBL_OPCUA: " + std::string(e.what()));
        return false;
    }
}

void PACControlClient::updateStats(bool success, double response_time_ms) {
    if (success) {
        stats_.successful_reads++;
        stats_.last_success = std::chrono::steady_clock::now();
    } else {
        stats_.failed_reads++;
    }
    
    // Actualizar promedio de tiempo de respuesta
    stats_.avg_response_time_ms = (stats_.avg_response_time_ms * 0.9) + (response_time_ms * 0.1);
}

std::string PACControlClient::getStatsReport() const {
    std::stringstream ss;
    ss << "PAC Client Statistics:\n";
    ss << "  Connected: " << (connected_ ? "Yes" : "No") << "\n";
    ss << "  Successful reads: " << stats_.successful_reads << "\n";
    ss << "  Failed reads: " << stats_.failed_reads << "\n";
    ss << "  Successful writes: " << stats_.successful_writes << "\n";
    ss << "  Failed writes: " << stats_.failed_writes << "\n";
    ss << "  TBL_OPCUA reads: " << stats_.opcua_table_reads << "\n";
    ss << "  Avg response time: " << std::fixed << std::setprecision(2) << stats_.avg_response_time_ms << "ms";
    
    return ss.str();
}

void PACControlClient::resetStats() {
    stats_ = ClientStats{};
    stats_.last_success = std::chrono::steady_clock::now();
}

// Callback estático para CURL
size_t PACControlClient::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    data->append(static_cast<char*>(contents), total_size);
    return total_size;
}

void PACControlClient::logError(const std::string& operation, const std::string& details) {
    LOG_ERROR("PAC " + operation + ": " + details);
}

void PACControlClient::logSuccess(const std::string& operation, const std::string& details) {
    if (!details.empty()) {
        LOG_SUCCESS("PAC " + operation + ": " + details);
    } else {
        LOG_SUCCESS("PAC " + operation);
    }
}