#pragma once

#include "tag.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include <unordered_map>
#include <map>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <fstream>
#include "common.h"

// Estructura para el histórico de tags
struct TagHistory {
    std::string tag_name;
    TagValue value;
    TagQuality quality;
    uint64_t timestamp;
};

class TagManager {
public:
    TagManager();
    ~TagManager();
    
    // Configuración
    bool loadFromFile(const std::string& config_file);
    bool loadFromConfig(const nlohmann::json& config);
    
    // Control del sistema
    void start();
    void stop();
    bool isRunning() const { return running_; }
    
    // Gestión de tags
    std::shared_ptr<Tag> getTag(const std::string& name);
    std::vector<std::shared_ptr<Tag>> getAllTags();
    std::vector<std::shared_ptr<Tag>> getTagsByGroup(const std::string& group);
    
    bool addTag(std::shared_ptr<Tag> tag);
    bool removeTag(const std::string& name);
    
    // Actualización de valores
    void updateTagValue(const std::string& name, const TagValue& value);
    
    // Histórico
    std::vector<TagHistory> getTagHistory(const std::string& tag_name, size_t max_entries = 100);
    void clearHistory();
    
    // Estado y exportación
    nlohmann::json getStatus();
    nlohmann::json exportTags();
    
    // Configuración
    void setPollingInterval(uint32_t interval_ms) { polling_interval_ = interval_ms; }
    uint32_t getPollingInterval() const { return polling_interval_; }
    
    void setMaxHistorySize(size_t max_size) { max_history_size_ = max_size; }
    size_t getMaxHistorySize() const { return max_history_size_; }

private:
    // Datos internos
    std::unordered_map<std::string, std::shared_ptr<Tag>> tags_;
    std::multimap<std::string, TagHistory> history_;
    
    // Control de threading
    std::atomic<bool> running_;
    std::thread polling_thread_;
    mutable std::mutex tags_mutex_;
    mutable std::mutex history_mutex_;
    
    // Configuración
    uint32_t polling_interval_;     // ms
    size_t max_history_size_;
    
    // Métodos internos
    void pollingLoop();
    void addToHistory(std::shared_ptr<Tag> tag);
    void createSubTags(const std::string& parent_name, const nlohmann::json& variables, const nlohmann::json& tag_config);
};