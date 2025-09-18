#include "tag.h"
#include <iomanip>
#include <ctime>
#include <stdexcept>
#include <algorithm>

// Constructor por defecto
Tag::Tag() 
    : name_(""), address_(""), description_(""), unit_(""), group_("")
    , data_type_(TagDataType::UNKNOWN), access_mode_(TagAccessMode::READ_WRITE)
    , value_(std::string("")), quality_(TagQuality::UNKNOWN)
    , timestamp_(0), client_write_timestamp_(0), min_value_(0.0), max_value_(0.0), has_limits_(false)
    , enabled_(true)
{
    updateTimestamp();
}

// Constructor con parámetros
Tag::Tag(const std::string& name, const std::string& address, TagDataType type)
    : name_(name), address_(address), description_(""), unit_(""), group_("")
    , data_type_(type), access_mode_(TagAccessMode::READ_WRITE)
    , quality_(TagQuality::UNKNOWN), timestamp_(0), client_write_timestamp_(0)
    , min_value_(0.0), max_value_(0.0), has_limits_(false), enabled_(true)
{
    // Inicializar valor según el tipo
    switch (type) {
        case TagDataType::BOOLEAN:
            value_ = false;
            break;
        case TagDataType::INT32:
            value_ = int32_t(0);
            break;
        case TagDataType::UINT32:
            value_ = uint32_t(0);
            break;
        case TagDataType::INT64:
            value_ = int64_t(0);
            break;
        case TagDataType::FLOAT:
            value_ = 0.0f;
            break;
        case TagDataType::DOUBLE:
            value_ = 0.0;
            break;
        case TagDataType::STRING:
        default:
            value_ = std::string("");
            break;
    }
    updateTimestamp();
}

// Configurar tipo de datos desde string
void Tag::setDataType(const std::string& type_str) {
    data_type_ = stringToTagDataType(type_str);
}

// Obtener tipo como string
std::string Tag::getDataTypeString() const {
    return tagDataTypeToString(data_type_);
}

// Métodos setValue
void Tag::setValue(const TagValue& value) {
    value_ = value;
    validateValue();
    updateTimestamp();
}

void Tag::setValue(bool value) {
    value_ = value;
    updateTimestamp();
}

void Tag::setValue(int32_t value) {
    value_ = value;
    validateValue();
    updateTimestamp();
}

void Tag::setValue(uint32_t value) {
    value_ = value;
    validateValue();
    updateTimestamp();
}

void Tag::setValue(int64_t value) {
    value_ = value;
    validateValue();
    updateTimestamp();
}

void Tag::setValue(float value) {
    value_ = value;
    validateValue();
    updateTimestamp();
}

void Tag::setValue(double value) {
    value_ = value;
    validateValue();
    updateTimestamp();
}

void Tag::setValue(const std::string& value) {
    value_ = value;
    updateTimestamp();
}

// Conversiones de valor
std::string Tag::getValueAsString() const {
    return tagValueToString(value_);
}

bool Tag::getValueAsBool() const {
    if (std::holds_alternative<bool>(value_)) {
        return std::get<bool>(value_);
    }
    
    // Intentar conversión desde otros tipos
    if (std::holds_alternative<std::string>(value_)) {
        const std::string& str = std::get<std::string>(value_);
        return (str == "true" || str == "1" || str == "TRUE");
    }
    
    // Conversión numérica
    double num = getValueAsNumeric();
    return num != 0.0;
}

int32_t Tag::getValueAsInt32() const {
    if (std::holds_alternative<int32_t>(value_)) {
        return std::get<int32_t>(value_);
    }
    return static_cast<int32_t>(getValueAsNumeric());
}

uint32_t Tag::getValueAsUInt32() const {
    if (std::holds_alternative<uint32_t>(value_)) {
        return std::get<uint32_t>(value_);
    }
    return static_cast<uint32_t>(getValueAsNumeric());
}

int64_t Tag::getValueAsInt64() const {
    if (std::holds_alternative<int64_t>(value_)) {
        return std::get<int64_t>(value_);
    }
    return static_cast<int64_t>(getValueAsNumeric());
}

float Tag::getValueAsFloat() const {
    if (std::holds_alternative<float>(value_)) {
        return std::get<float>(value_);
    }
    return static_cast<float>(getValueAsNumeric());
}

double Tag::getValueAsDouble() const {
    if (std::holds_alternative<double>(value_)) {
        return std::get<double>(value_);
    }
    return getValueAsNumeric();
}

// Obtener calidad como string
std::string Tag::getQualityString() const {
    return tagQualityToString(quality_);
}

// Actualizar timestamp
void Tag::updateTimestamp() {
    timestamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// Obtener timestamp como string
std::string Tag::getTimestampString() const {
    auto time_point = std::chrono::system_clock::from_time_t(timestamp_ / 1000);
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << (timestamp_ % 1000);
    
    return ss.str();
}

// Verificar si fue escrito recientemente por un cliente OPC UA
bool Tag::wasRecentlyWrittenByClient(uint64_t protection_window_ms) const {
    if (client_write_timestamp_ == 0) {
        return false; // Nunca fue escrito por cliente
    }
    
    uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    return (current_time - client_write_timestamp_) < protection_window_ms;
}

// Validación
bool Tag::isValid() const {
    return quality_ == TagQuality::GOOD && enabled_;
}

bool Tag::isInRange() const {
    if (!has_limits_) {
        return true;
    }
    
    double numeric_value = getValueAsNumeric();
    return (numeric_value >= min_value_ && numeric_value <= max_value_);
}

// Operadores
bool Tag::operator==(const Tag& other) const {
    return name_ == other.name_ && address_ == other.address_;
}

bool Tag::operator!=(const Tag& other) const {
    return !(*this == other);
}

// Serialización
std::string Tag::toString() const {
    std::stringstream ss;
    ss << "Tag[" << name_ << "] = " << getValueAsString() 
       << " (" << getDataTypeString() << ") "
       << "[" << getQualityString() << "] "
       << "@ " << getTimestampString();
    return ss.str();
}

void Tag::print() const {
    std::cout << toString() << std::endl;
}

// Métodos privados
double Tag::getValueAsNumeric() const {
    return std::visit([](const auto& value) -> double {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, bool>) {
            return value ? 1.0 : 0.0;
        } else if constexpr (std::is_arithmetic_v<T>) {
            return static_cast<double>(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            try {
                return std::stod(value);
            } catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    }, value_);
}

void Tag::validateValue() {
    if (has_limits_ && !isInRange()) {
        quality_ = TagQuality::BAD;
    } else {
        quality_ = TagQuality::GOOD;
    }
}

// Funciones auxiliares
std::string tagDataTypeToString(TagDataType type) {
    switch (type) {
        case TagDataType::BOOLEAN: return "BOOLEAN";
        case TagDataType::INT32: return "INT32";
        case TagDataType::UINT32: return "UINT32";
        case TagDataType::INT64: return "INT64";
        case TagDataType::FLOAT: return "FLOAT";
        case TagDataType::DOUBLE: return "DOUBLE";
        case TagDataType::STRING: return "STRING";
        case TagDataType::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

TagDataType stringToTagDataType(const std::string& type_str) {
    std::string upper_type = type_str;
    std::transform(upper_type.begin(), upper_type.end(), upper_type.begin(), ::toupper);
    
    if (upper_type == "BOOLEAN" || upper_type == "BOOL") return TagDataType::BOOLEAN;
    if (upper_type == "INT32" || upper_type == "INTEGER") return TagDataType::INT32;
    if (upper_type == "UINT32") return TagDataType::UINT32;
    if (upper_type == "INT64" || upper_type == "LONG") return TagDataType::INT64;
    if (upper_type == "FLOAT") return TagDataType::FLOAT;
    if (upper_type == "DOUBLE") return TagDataType::DOUBLE;
    if (upper_type == "STRING") return TagDataType::STRING;
    
    return TagDataType::UNKNOWN;
}

std::string tagQualityToString(TagQuality quality) {
    switch (quality) {
        case TagQuality::GOOD: return "GOOD";
        case TagQuality::BAD: return "BAD";
        case TagQuality::UNCERTAIN: return "UNCERTAIN";
        case TagQuality::STALE: return "STALE";
        case TagQuality::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

std::string tagValueToString(const TagValue& value) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else {
            return std::to_string(v);
        }
    }, value);
}

// Operador de salida
std::ostream& operator<<(std::ostream& os, const Tag& tag) {
    os << tag.toString();
    return os;
}

// Factory methods
TagPtr TagFactory::createBooleanTag(const std::string& name, const std::string& address) {
    return std::make_shared<Tag>(name, address, TagDataType::BOOLEAN);
}

TagPtr TagFactory::createIntegerTag(const std::string& name, const std::string& address) {
    return std::make_shared<Tag>(name, address, TagDataType::INT32);
}

TagPtr TagFactory::createFloatTag(const std::string& name, const std::string& address) {
    return std::make_shared<Tag>(name, address, TagDataType::FLOAT);
}

TagPtr TagFactory::createStringTag(const std::string& name, const std::string& address) {
    return std::make_shared<Tag>(name, address, TagDataType::STRING);
}

TagPtr TagFactory::createTag(const std::string& name, const std::string& address, TagDataType type) {
    return std::make_shared<Tag>(name, address, type);
}