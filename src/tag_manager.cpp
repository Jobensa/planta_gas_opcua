#include "tag_manager.h"
#include "common.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <atomic>

TagManager::TagManager() 
    : running_(false)
    , polling_interval_(1000)
    , max_history_size_(1000)
{
    std::cout << "TagManager inicializado" << std::endl;
}

TagManager::~TagManager() {
    stop();
}

bool TagManager::loadFromFile(const std::string& config_file) {
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            std::cerr << "Error: No se pudo abrir " << config_file << std::endl;
            return false;
        }
        
        nlohmann::json config;
        file >> config;
        
        return loadFromConfig(config);
        
    } catch (const std::exception& e) {
        std::cerr << "Error cargando configuración: " << e.what() << std::endl;
        return false;
    }
}

bool TagManager::loadFromConfig(const nlohmann::json& config) {
    try {
        // Limpiar tags existentes
        std::lock_guard<std::mutex> lock(tags_mutex_);
        tags_.clear();
        
        // Configuración general
        if (config.contains("polling_interval_ms")) {
            polling_interval_ = config["polling_interval_ms"].get<uint32_t>();
        }
        
        if (config.contains("max_history_size")) {
            max_history_size_ = config["max_history_size"].get<size_t>();
        }
        
        // Cargar tags desde un solo array unificado
        if (config.contains("tags")) {
            for (const auto& tag_config : config["tags"]) {
                auto tag = std::make_shared<Tag>();
                
                if (tag_config.contains("name")) {
                    tag->setName(tag_config["name"].get<std::string>());
                }
                
                // Mapear value_table como address
                if (tag_config.contains("value_table")) {
                    tag->setAddress(tag_config["value_table"].get<std::string>());
                }
                
                // Establecer tipo por defecto como float para controladores industriales
                tag->setDataType("float");
                
                // Mapear units como unit
                if (tag_config.contains("units")) {
                    tag->setUnit(tag_config["units"].get<std::string>());
                }
                
                if (tag_config.contains("description")) {
                    tag->setDescription(tag_config["description"].get<std::string>());
                }
                
                // Valor inicial
                if (tag_config.contains("default_value")) {
                    const auto& default_val = tag_config["default_value"];
                    
                    // Asignar valor según el tipo del tag
                    switch (tag->getDataType()) {
                        case TagDataType::BOOLEAN:
                            tag->setValue(default_val.get<bool>());
                            break;
                        case TagDataType::INT32:
                            tag->setValue(default_val.get<int32_t>());
                            break;
                        case TagDataType::UINT32:
                            tag->setValue(default_val.get<uint32_t>());
                            break;
                        case TagDataType::INT64:
                            tag->setValue(default_val.get<int64_t>());
                            break;
                        case TagDataType::FLOAT:
                            tag->setValue(default_val.get<float>());
                            break;
                        case TagDataType::DOUBLE:
                            tag->setValue(default_val.get<double>());
                            break;
                        case TagDataType::STRING:
                        default:
                            tag->setValue(default_val.get<std::string>());
                            break;
                    }
                }
                
                // Agregar tag principal
                tags_[tag->getName()] = tag;
                
                // Crear sub-tags basados en las variables definidas
                if (tag_config.contains("variables")) {
                    createSubTags(tag->getName(), tag_config["variables"], tag_config);
                }
            }
        }
        
        // Cargar controladores PID desde array separado
        if (config.contains("PID_controllers")) {
            for (const auto& tag_config : config["PID_controllers"]) {
                auto tag = std::make_shared<Tag>();
                
                if (tag_config.contains("name")) {
                    tag->setName(tag_config["name"].get<std::string>());
                }
                
                // Mapear value_table como address
                if (tag_config.contains("value_table")) {
                    tag->setAddress(tag_config["value_table"].get<std::string>());
                }
                
                // Establecer tipo por defecto como float para controladores industriales
                tag->setDataType("float");
                
                // Mapear units como unit
                if (tag_config.contains("units")) {
                    tag->setUnit(tag_config["units"].get<std::string>());
                }
                
                if (tag_config.contains("description")) {
                    tag->setDescription(tag_config["description"].get<std::string>());
                }
                
                // Valor inicial
                if (tag_config.contains("default_value")) {
                    const auto& default_val = tag_config["default_value"];
                    
                    // Asignar valor según el tipo del tag
                    switch (tag->getDataType()) {
                        case TagDataType::BOOLEAN:
                            tag->setValue(default_val.get<bool>());
                            break;
                        case TagDataType::INT32:
                            tag->setValue(default_val.get<int32_t>());
                            break;
                        case TagDataType::UINT32:
                            tag->setValue(default_val.get<uint32_t>());
                            break;
                        case TagDataType::INT64:
                            tag->setValue(default_val.get<int64_t>());
                            break;
                        case TagDataType::FLOAT:
                            tag->setValue(default_val.get<float>());
                            break;
                        case TagDataType::DOUBLE:
                            tag->setValue(default_val.get<double>());
                            break;
                        case TagDataType::STRING:
                        default:
                            tag->setValue(default_val.get<std::string>());
                            break;
                    }
                }
                
                tags_[tag->getName()] = tag;
            }
        }
        // Compatibilidad con formato anterior (TBL_tags)
        else if (config.contains("TBL_tags")) {
            for (const auto& tag_config : config["TBL_tags"]) {
                auto tag = std::make_shared<Tag>();
                
                if (tag_config.contains("name")) {
                    tag->setName(tag_config["name"].get<std::string>());
                }
                
                // Mapear value_table como address
                if (tag_config.contains("value_table")) {
                    tag->setAddress(tag_config["value_table"].get<std::string>());
                }
                
                // Establecer tipo por defecto como float para controladores industriales
                tag->setDataType("float");
                
                // Mapear units como unit
                if (tag_config.contains("units")) {
                    tag->setUnit(tag_config["units"].get<std::string>());
                }
                
                if (tag_config.contains("description")) {
                    tag->setDescription(tag_config["description"].get<std::string>());
                }
                
                // Valor inicial
                if (tag_config.contains("default_value")) {
                    const auto& default_val = tag_config["default_value"];
                    
                    // Asignar valor según el tipo del tag
                    switch (tag->getDataType()) {
                        case TagDataType::BOOLEAN:
                            tag->setValue(default_val.get<bool>());
                            break;
                        case TagDataType::INT32:
                            tag->setValue(default_val.get<int32_t>());
                            break;
                        case TagDataType::UINT32:
                            tag->setValue(default_val.get<uint32_t>());
                            break;
                        case TagDataType::INT64:
                            tag->setValue(default_val.get<int64_t>());
                            break;
                        case TagDataType::FLOAT:
                            tag->setValue(default_val.get<float>());
                            break;
                        case TagDataType::DOUBLE:
                            tag->setValue(default_val.get<double>());
                            break;
                        case TagDataType::STRING:
                        default:
                            tag->setValue(default_val.get<std::string>());
                            break;
                    }
                }
                
                tags_[tag->getName()] = tag;
            }
            
            // Cargar devices adicionales si existen
            if (config.contains("devices")) {
                for (const auto& tag_config : config["devices"]) {
                    auto tag = std::make_shared<Tag>();
                    
                    if (tag_config.contains("name")) {
                        tag->setName(tag_config["name"].get<std::string>());
                    }
                    
                    // Mapear value_table como address
                    if (tag_config.contains("value_table")) {
                        tag->setAddress(tag_config["value_table"].get<std::string>());
                    }
                    
                    // Establecer tipo por defecto como float para controladores industriales
                    tag->setDataType("float");
                    
                    // Mapear units como unit
                    if (tag_config.contains("units")) {
                        tag->setUnit(tag_config["units"].get<std::string>());
                    }
                    
                    if (tag_config.contains("description")) {
                        tag->setDescription(tag_config["description"].get<std::string>());
                    }
                    
                    // Valor inicial
                    if (tag_config.contains("default_value")) {
                        const auto& default_val = tag_config["default_value"];
                        
                        // Asignar valor según el tipo del tag
                        switch (tag->getDataType()) {
                            case TagDataType::BOOLEAN:
                                tag->setValue(default_val.get<bool>());
                                break;
                            case TagDataType::INT32:
                                tag->setValue(default_val.get<int32_t>());
                                break;
                            case TagDataType::UINT32:
                                tag->setValue(default_val.get<uint32_t>());
                                break;
                            case TagDataType::INT64:
                                tag->setValue(default_val.get<int64_t>());
                                break;
                            case TagDataType::FLOAT:
                                tag->setValue(default_val.get<float>());
                                break;
                            case TagDataType::DOUBLE:
                                tag->setValue(default_val.get<double>());
                                break;
                            case TagDataType::STRING:
                            default:
                                tag->setValue(default_val.get<std::string>());
                                break;
                        }
                    }
                    
                    tags_[tag->getName()] = tag;
                }
            }
            
            // Cargar PID_tags adicionales si existen
            if (config.contains("PID_tags")) {
                for (const auto& tag_config : config["PID_tags"]) {
                    auto tag = std::make_shared<Tag>();
                    
                    if (tag_config.contains("name")) {
                        tag->setName(tag_config["name"].get<std::string>());
                    }
                    
                    // Mapear value_table como address
                    if (tag_config.contains("value_table")) {
                        tag->setAddress(tag_config["value_table"].get<std::string>());
                    }
                    
                    // Establecer tipo por defecto como float para controladores industriales
                    tag->setDataType("float");
                    
                    // Mapear units como unit
                    if (tag_config.contains("units")) {
                        tag->setUnit(tag_config["units"].get<std::string>());
                    }
                    
                    if (tag_config.contains("description")) {
                        tag->setDescription(tag_config["description"].get<std::string>());
                    }
                    
                    // Valor inicial
                    if (tag_config.contains("default_value")) {
                        const auto& default_val = tag_config["default_value"];
                        
                        // Asignar valor según el tipo del tag
                        switch (tag->getDataType()) {
                            case TagDataType::BOOLEAN:
                                tag->setValue(default_val.get<bool>());
                                break;
                            case TagDataType::INT32:
                                tag->setValue(default_val.get<int32_t>());
                                break;
                            case TagDataType::UINT32:
                                tag->setValue(default_val.get<uint32_t>());
                                break;
                            case TagDataType::INT64:
                                tag->setValue(default_val.get<int64_t>());
                                break;
                            case TagDataType::FLOAT:
                                tag->setValue(default_val.get<float>());
                                break;
                            case TagDataType::DOUBLE:
                                tag->setValue(default_val.get<double>());
                                break;
                            case TagDataType::STRING:
                            default:
                                tag->setValue(default_val.get<std::string>());
                                break;
                        }
                    }
                    
                    tags_[tag->getName()] = tag;
                }
            }
        }
        
        std::cout << "Cargados " << tags_.size() << " tags desde configuración" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error procesando configuración: " << e.what() << std::endl;
        return false;
    }
}

void TagManager::start() {
    if (running_) {
        std::cout << "TagManager ya está ejecutándose" << std::endl;
        return;
    }
    
    running_ = true;
    polling_thread_ = std::thread(&TagManager::pollingLoop, this);
    
    std::cout << "TagManager iniciado - Polling cada " << polling_interval_ << "ms" << std::endl;
}

void TagManager::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (polling_thread_.joinable()) {
        polling_thread_.join();
    }
    
    std::cout << "TagManager detenido" << std::endl;
}

std::shared_ptr<Tag> TagManager::getTag(const std::string& name) {
    std::lock_guard<std::mutex> lock(tags_mutex_);
    
    auto it = tags_.find(name);
    if (it != tags_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<Tag>> TagManager::getAllTags() {
    std::lock_guard<std::mutex> lock(tags_mutex_);
    
    std::vector<std::shared_ptr<Tag>> result;
    result.reserve(tags_.size());
    
    for (const auto& pair : tags_) {
        result.push_back(pair.second);
    }
    
    return result;
}

std::vector<std::shared_ptr<Tag>> TagManager::getTagsByGroup(const std::string& group) {
    std::lock_guard<std::mutex> lock(tags_mutex_);
    
    std::vector<std::shared_ptr<Tag>> result;
    
    for (const auto& pair : tags_) {
        if (pair.second->getGroup() == group) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

bool TagManager::addTag(std::shared_ptr<Tag> tag) {
    if (!tag) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(tags_mutex_);
    
    if (tags_.find(tag->getName()) != tags_.end()) {
        std::cerr << "Tag '" << tag->getName() << "' ya existe" << std::endl;
        return false;
    }
    
    tags_[tag->getName()] = tag;
    std::cout << "Tag '" << tag->getName() << "' agregado" << std::endl;
    
    return true;
}

bool TagManager::removeTag(const std::string& name) {
    std::lock_guard<std::mutex> lock(tags_mutex_);
    
    auto it = tags_.find(name);
    if (it == tags_.end()) {
        return false;
    }
    
    tags_.erase(it);
    std::cout << "Tag '" << name << "' eliminado" << std::endl;
    
    return true;
}

void TagManager::updateTagValue(const std::string& name, const TagValue& value) {
    std::lock_guard<std::mutex> lock(tags_mutex_);
    
    auto it = tags_.find(name);
    if (it != tags_.end()) {
        it->second->setValue(value);
        it->second->updateTimestamp();
        
        // Agregar a histórico
        addToHistory(it->second);
    }
}

std::vector<TagHistory> TagManager::getTagHistory(const std::string& tag_name, size_t max_entries) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    std::vector<TagHistory> result;
    
    auto range = history_.equal_range(tag_name);
    for (auto it = range.first; it != range.second && result.size() < max_entries; ++it) {
        result.push_back(it->second);
    }
    
    // Ordenar por timestamp descendente (más reciente primero)
    std::sort(result.begin(), result.end(), 
        [](const TagHistory& a, const TagHistory& b) {
            return a.timestamp > b.timestamp;
        });
    
    return result;
}

void TagManager::clearHistory() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.clear();
}

nlohmann::json TagManager::getStatus() {
    std::lock_guard<std::mutex> lock(tags_mutex_);
    
    nlohmann::json status;
    status["running"] = running_.load();  // Convertir atomic<bool> a bool
    status["total_tags"] = tags_.size();
    status["polling_interval_ms"] = polling_interval_;
    status["max_history_size"] = max_history_size_;
    
    std::lock_guard<std::mutex> history_lock(history_mutex_);
    status["history_entries"] = history_.size();
    
    return status;
}

nlohmann::json TagManager::exportTags() {
    std::lock_guard<std::mutex> lock(tags_mutex_);
    
    nlohmann::json export_data;
    export_data["tags"] = nlohmann::json::array();
    
    for (const auto& pair : tags_) {
        auto tag = pair.second;
        nlohmann::json tag_json;
        
        tag_json["name"] = tag->getName();
        tag_json["address"] = tag->getAddress();
        tag_json["type"] = tag->getDataType();
        tag_json["unit"] = tag->getUnit();
        tag_json["description"] = tag->getDescription();
        tag_json["group"] = tag->getGroup();
        tag_json["value"] = tag->getValueAsString();
        tag_json["quality"] = static_cast<int>(tag->getQuality());
        tag_json["timestamp"] = tag->getTimestamp();
        
        export_data["tags"].push_back(tag_json);
    }
    
    export_data["exported_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    return export_data;
}

void TagManager::pollingLoop() {
    while (running_) {
        try {
            // Aquí irían las actualizaciones reales de los tags
            // Por ahora solo simulamos actividad
            
            std::this_thread::sleep_for(std::chrono::milliseconds(polling_interval_));
            
        } catch (const std::exception& e) {
            std::cerr << "Error en polling loop: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void TagManager::addToHistory(std::shared_ptr<Tag> tag) {
    TagHistory history_entry;
    history_entry.tag_name = tag->getName();
    history_entry.value = tag->getValue();
    history_entry.quality = tag->getQuality();
    history_entry.timestamp = tag->getTimestamp();
    
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    // Agregar nueva entrada
    history_.emplace(tag->getName(), history_entry);
    
    // Limpiar entradas antiguas si excedemos el límite
    if (history_.size() > max_history_size_) {
        // Encontrar la entrada más antigua y eliminarla
        auto oldest_it = history_.begin();
        for (auto it = history_.begin(); it != history_.end(); ++it) {
            if (it->second.timestamp < oldest_it->second.timestamp) {
                oldest_it = it;
            }
        }
        history_.erase(oldest_it);
    }
}

void TagManager::createSubTags(const std::string& parent_name, const nlohmann::json& variables, const nlohmann::json& tag_config) {
    if (!variables.is_array()) {
        return;
    }
    
    // Crear un sub-tag para cada variable definida
    for (const auto& var_name : variables) {
        if (!var_name.is_string()) {
            continue;
        }
        
        std::string variable_name = var_name.get<std::string>();
        std::string sub_tag_name = parent_name + "." + variable_name;
        
        // Verificar si el sub-tag ya existe
        if (tags_.find(sub_tag_name) != tags_.end()) {
            continue;
        }
        
        // Crear sub-tag
        auto sub_tag = std::make_shared<Tag>();
        sub_tag->setName(sub_tag_name);
        
        // Heredar propiedades del tag padre
        if (tag_config.contains("units")) {
            sub_tag->setUnit(tag_config["units"].get<std::string>());
        }
        
        // Descripción específica por variable
        std::string description = parent_name + " - " + variable_name;
        if (variable_name == "PV") {
            description += " (Process Variable)";
        } else if (variable_name == "SP") {
            description += " (Set Point)";
        } else if (variable_name == "CV") {
            description += " (Control Variable)";
        }
        sub_tag->setDescription(description);
        
        // Establecer tipo de dato (por defecto float para variables industriales)
        sub_tag->setDataType("float");
        
        // Valor inicial por defecto
        if (variable_name == "auto_manual") {
            sub_tag->setDataType("boolean");
            sub_tag->setValue(true); // Automático por defecto
        } else if (variable_name == "PID_ENABLE") {
            sub_tag->setDataType("boolean");
            sub_tag->setValue(true); // PID habilitado por defecto
        } else {
            sub_tag->setValue(0.0f); // Valor numérico por defecto
        }
        
        // Establecer dirección basada en el tag padre y la variable
        if (tag_config.contains("value_table")) {
            std::string address = tag_config["value_table"].get<std::string>() + "." + variable_name;
            sub_tag->setAddress(address);
        }
        
        // Agregar sub-tag al mapa
        tags_[sub_tag_name] = sub_tag;
        
        LOG_DEBUG("Sub-tag creado: " + sub_tag_name);
    }
}