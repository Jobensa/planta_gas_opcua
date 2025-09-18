#pragma once

#include <string>
#include <variant>
#include <chrono>
#include <memory>
#include <vector>
#include <iostream>
#include <sstream>

// Tipos de datos soportados para los tags
using TagValue = std::variant<
    bool,           // Boolean
    int32_t,        // Integer 32-bit
    uint32_t,       // Unsigned Integer 32-bit
    int64_t,        // Integer 64-bit
    float,          // Float 32-bit
    double,         // Double 64-bit
    std::string     // String
>;

// Calidad del tag
enum class TagQuality {
    GOOD = 0,       // Valor válido y confiable
    BAD = 1,        // Valor no válido
    UNCERTAIN = 2,  // Valor dudoso
    STALE = 3,      // Valor obsoleto
    UNKNOWN = 4     // Estado desconocido
};

// Tipos de datos
enum class TagDataType {
    BOOLEAN,
    INT32,
    UINT32,
    INT64,
    FLOAT,
    DOUBLE,
    STRING,
    UNKNOWN
};

// Modo de acceso
enum class TagAccessMode {
    READ_ONLY,
    WRITE_ONLY,
    READ_WRITE
};

class Tag {
public:
    // Constructores
    Tag();
    Tag(const std::string& name, const std::string& address, TagDataType type);
    
    // Destructor
    ~Tag() = default;
    
    // Información básica del tag
    void setName(const std::string& name) { name_ = name; }
    const std::string& getName() const { return name_; }
    
    void setAddress(const std::string& address) { address_ = address; }
    const std::string& getAddress() const { return address_; }
    
    void setDescription(const std::string& description) { description_ = description; }
    const std::string& getDescription() const { return description_; }
    
    void setUnit(const std::string& unit) { unit_ = unit; }
    const std::string& getUnit() const { return unit_; }
    
    void setGroup(const std::string& group) { group_ = group; }
    const std::string& getGroup() const { return group_; }
    
    // Tipo de datos
    void setDataType(TagDataType type) { data_type_ = type; }
    void setDataType(const std::string& type_str);
    TagDataType getDataType() const { return data_type_; }
    std::string getDataTypeString() const;
    
    // Modo de acceso
    void setAccessMode(TagAccessMode mode) { access_mode_ = mode; }
    TagAccessMode getAccessMode() const { return access_mode_; }
    
    // Valor del tag
    void setValue(const TagValue& value);
    void setValue(bool value);
    void setValue(int32_t value);
    void setValue(uint32_t value);
    void setValue(int64_t value);
    void setValue(float value);
    void setValue(double value);
    void setValue(const std::string& value);
    
    const TagValue& getValue() const { return value_; }
    
    // Conversiones de valor
    std::string getValueAsString() const;
    bool getValueAsBool() const;
    int32_t getValueAsInt32() const;
    uint32_t getValueAsUInt32() const;
    int64_t getValueAsInt64() const;
    float getValueAsFloat() const;
    double getValueAsDouble() const;
    
    // Calidad del tag
    void setQuality(TagQuality quality) { quality_ = quality; }
    TagQuality getQuality() const { return quality_; }
    std::string getQualityString() const;
    
    // Timestamp
    void updateTimestamp();
    void setTimestamp(uint64_t timestamp) { timestamp_ = timestamp; }
    uint64_t getTimestamp() const { return timestamp_; }
    std::string getTimestampString() const;
    
    // Protección contra sobrescritura por actualizaciones automáticas
    void setClientWriteTimestamp(uint64_t timestamp) { client_write_timestamp_ = timestamp; }
    uint64_t getClientWriteTimestamp() const { return client_write_timestamp_; }
    bool wasRecentlyWrittenByClient(uint64_t protection_window_ms = 5000) const;
    
    // Límites y validación
    void setMinValue(double min) { min_value_ = min; has_limits_ = true; }
    void setMaxValue(double max) { max_value_ = max; has_limits_ = true; }
    double getMinValue() const { return min_value_; }
    double getMaxValue() const { return max_value_; }
    bool hasLimits() const { return has_limits_; }
    
    // Validación
    bool isValid() const;
    bool isInRange() const;
    
    // Estado
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }
    
    void setReadOnly(bool readonly) { 
        access_mode_ = readonly ? TagAccessMode::READ_ONLY : TagAccessMode::READ_WRITE; 
    }
    bool isReadOnly() const { return access_mode_ == TagAccessMode::READ_ONLY; }
    
    // Operadores
    bool operator==(const Tag& other) const;
    bool operator!=(const Tag& other) const;
    
    // Serialización/Debug
    std::string toString() const;
    void print() const;

private:
    // Datos básicos
    std::string name_;
    std::string address_;
    std::string description_;
    std::string unit_;
    std::string group_;
    
    // Tipo y configuración
    TagDataType data_type_;
    TagAccessMode access_mode_;
    
    // Valor y estado
    TagValue value_;
    TagQuality quality_;
    uint64_t timestamp_;
    uint64_t client_write_timestamp_; // Timestamp de última escritura por cliente OPC UA
    
    // Límites
    double min_value_;
    double max_value_;
    bool has_limits_;
    
    // Estado
    bool enabled_;
    
    // Métodos auxiliares
    double getValueAsNumeric() const;
    void validateValue();
};

// Funciones auxiliares
std::string tagDataTypeToString(TagDataType type);
TagDataType stringToTagDataType(const std::string& type_str);
std::string tagQualityToString(TagQuality quality);
std::string tagValueToString(const TagValue& value);

// Operador de salida para debugging
std::ostream& operator<<(std::ostream& os, const Tag& tag);

// Punteros inteligentes
using TagPtr = std::shared_ptr<Tag>;
using TagWeakPtr = std::weak_ptr<Tag>;

// Factory para crear tags
class TagFactory {
public:
    static TagPtr createBooleanTag(const std::string& name, const std::string& address);
    static TagPtr createIntegerTag(const std::string& name, const std::string& address);
    static TagPtr createFloatTag(const std::string& name, const std::string& address);
    static TagPtr createStringTag(const std::string& name, const std::string& address);
    static TagPtr createTag(const std::string& name, const std::string& address, TagDataType type);
};