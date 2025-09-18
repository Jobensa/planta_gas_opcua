/*
 * pac_control_client.cpp - Implementación del protocolo MMP de Opto 22
 * Adaptado del repositorio funcional https://github.com/Jobensa/OPC-UA-CPP
 */

#include "pac_control_client.h"
#include "tag_manager.h"
#include "common.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cmath>

// Constructor adaptado para shared_ptr (nueva versión)
PACControlClient::PACControlClient(std::shared_ptr<TagManager> tag_manager)
    : tag_manager_(tag_manager)
    , pac_ip_("192.168.1.30")
    , pac_port_(22001)
    , timeout_ms_(5000)
    , connected_(false)
    , enabled_(true)
    , socket_fd_(-1)
{
    opcua_table_cache_.resize(52, 0.0f);
    stats_.last_success = std::chrono::steady_clock::now();
    
    if (!initializeSocket()) {
        LOG_ERROR("Failed to initialize socket for PAC client");
        enabled_ = false;
    }
    
    // Cargar mapeo de TBL_OPCUA desde configuración
    if (!loadTagOPCUAMapping("config/tags_planta_gas.json")) {
        LOG_WARNING("⚠️ No se pudo cargar mapeo TBL_OPCUA, funcionará en modo básico");
    }
    
    LOG_INFO("🔌 PACControlClient inicializado con protocolo MMP Opto 22");
}

// Constructor de compatibilidad (versión antigua)
PACControlClient::PACControlClient(TagManager* tag_manager)
    : PACControlClient(std::shared_ptr<TagManager>(tag_manager, [](TagManager*){})) // No-op deleter
{
    LOG_INFO("🔌 PACControlClient inicializado (compatibility version)");
}

PACControlClient::~PACControlClient() {
    disconnect();
    cleanupSocket();
}

bool PACControlClient::initializeSocket() {
    // No hay inicialización especial necesaria para sockets TCP
    return true;
}

void PACControlClient::cleanupSocket() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

bool PACControlClient::connect() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    
    if (!enabled_) {
        LOG_ERROR("PAC client is disabled");
        return false;
    }
    
    if (connected_) {
        LOG_WARNING("PAC client already connected");
        return true;
    }
    
    LOG_INFO("🔌 Conectando al PAC " + pac_ip_ + ":" + std::to_string(pac_port_) + " usando protocolo MMP...");
    
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        LOG_ERROR("Error creando socket: " + std::string(strerror(errno)));
        return false;
    }
    
    // Configurar timeout para socket
    struct timeval timeout;
    timeout.tv_sec = 3; // 3 segundos timeout
    timeout.tv_usec = 0;
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(pac_port_);
    
    if (inet_pton(AF_INET, pac_ip_.c_str(), &server_addr.sin_addr) <= 0) {
        LOG_ERROR("Dirección IP inválida: " + pac_ip_);
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    if (::connect(socket_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("❌ Error conectando al PAC: " + std::string(strerror(errno)));
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    connected_ = true;
    LOG_SUCCESS("✅ Conectado al PAC exitosamente usando protocolo MMP");
    
    // DEBUGGING TEMPORAL: Comentar lectura inicial para evitar bloqueo
    /*
    // Leer TBL_OPCUA inicial para verificar comunicación
    if (readOPCUATable()) {
        LOG_SUCCESS("📊 TBL_OPCUA leída exitosamente (" + std::to_string(opcua_table_cache_.size()) + " variables)");
    } else {
        LOG_WARNING("⚠️ No se pudo leer TBL_OPCUA inicial, pero conexión establecida");
    }
    */
    LOG_INFO("🔄 Lectura inicial de TBL_OPCUA diferida a monitoringLoop()");
    
    return true;
}

void PACControlClient::disconnect() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    
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

// OPTIMIZACIÓN CRÍTICA: Leer tabla completa TBL_OPCUA usando protocolo MMP
bool PACControlClient::readOPCUATable() {
    if (!connected_ || !enabled_) {
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // Comando MMP para leer TBL_OPCUA completa (52 floats)
        std::vector<float> values = readFloatTable("TBL_OPCUA", 0, 51);
        
        if (values.empty()) {
            LOG_ERROR("Empty response from TBL_OPCUA");
            updateStats(false, 0);
            return false;
        }
        
        // **MODO SIMULACIÓN TEMPORAL**: Si todos los valores son 0, generar datos realistas
        bool all_zeros = true;
        for (const auto& val : values) {
            if (val != 0.0f) {
                all_zeros = false;
                break;
            }
        }
        
        // TEMPORALMENTE DESHABILITADO - TBL_OPCUA ahora tiene datos reales
        /*
        if (all_zeros) {
            LOG_INFO("🔄 PAC devuelve todos ceros - Activando modo simulación temporal");
            values = generateSimulatedData(values.size());
        }
        */
        
        if (all_zeros) {
            LOG_WARNING("⚠️ TBL_OPCUA contiene solo ceros - usando datos como están");
        } else {
            LOG_SUCCESS("✅ TBL_OPCUA contiene datos reales - Total: " + std::to_string(values.size()) + " valores");
        }
        
        // Copiar datos al cache
        opcua_table_cache_ = values;
        
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        last_opcua_read_ = end_time;
        stats_.opcua_table_reads++;
        updateStats(true, elapsed.count());
        
        // Actualizar TagManager con los datos críticos
        if (updateTagManagerFromOPCUATable()) {
            LOG_DEBUG("📊 TBL_OPCUA: " + std::to_string(values.size()) + " variables en " + 
                     std::to_string(elapsed.count()) + "ms");
            return true;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error reading TBL_OPCUA: " + std::string(e.what()));
        updateStats(false, 0);
    }
    
    return false;
}

// **NUEVA ESTRATEGIA**: Leer tablas individuales con datos reales
bool PACControlClient::readIndividualTables() {
    if (!connected_ || !enabled_) {
        return false;
    }
    
    LOG_INFO("🔄 Leyendo tablas individuales con datos reales...");
    auto start_time = std::chrono::steady_clock::now();
    size_t total_updates = 0;
    
    // Lista de tablas principales basada en configuración
    std::vector<std::string> main_tables = {
        "TBL_ET_1601",   // Flow Transmitter 1601 - valores 1-10 ✓
        "TBL_ET_1602",   // Flow Transmitter 1602
        "TBL_ET_1603",   // Flow Transmitter 1603
        "TBL_ET_1604",   // Flow Transmitter 1604
        "TBL_ET_1605",   // Flow Transmitter 1605
        "TBL_PIT_1201",  // Pressure Transmitter
        "TBL_PIT_1303",  // Pressure Transmitter
        "TBL_PIT_1303A", // Pressure Transmitter
        "TBL_PIT_1404",  // Pressure Transmitter
        "TBL_PIT_1502",  // Pressure Transmitter
        "TBL_PIT_1758"   // Pressure Transmitter
    };
    
    for (const auto& table_name : main_tables) {
        try {
            // Leer tabla individual (típicamente 11 variables por tabla)
            std::vector<float> table_values = readFloatTable(table_name, 0, 10);
            
            if (!table_values.empty()) {
                // Actualizar TagManager con los valores de esta tabla
                if (updateTagManagerFromIndividualTable(table_name, table_values)) {
                    total_updates += table_values.size();
                    LOG_DEBUG("✅ " + table_name + ": " + std::to_string(table_values.size()) + " valores actualizados");
                }
            } else {
                LOG_DEBUG("⚠️ " + table_name + " devolvió datos vacíos");
            }
            
            // Pequeña pausa para no saturar el PAC
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
        } catch (const std::exception& e) {
            LOG_ERROR("Error leyendo " + table_name + ": " + std::string(e.what()));
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (total_updates > 0) {
        updateStats(true, elapsed.count());
        LOG_SUCCESS("📊 Tablas individuales: " + std::to_string(total_updates) + 
                   " variables actualizadas en " + std::to_string(elapsed.count()) + "ms");
        return true;
    }
    
    updateStats(false, elapsed.count());
    return false;
}

// Lectura de tablas usando protocolo MMP de Opto 22
std::vector<float> PACControlClient::readFloatTable(const std::string& table_name, int start_pos, int end_pos) {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    
    if (!connected_) {
        LOG_ERROR("No conectado al PAC");
        return {};
    }
    
    LOG_DEBUG("📊 LEYENDO TABLA DE FLOATS: " + table_name + " [" + std::to_string(start_pos) + "-" + std::to_string(end_pos) + "]");
    
    // Comando MMP: "end_pos start_pos }tabla TRange.\r"
    std::stringstream cmd;
    cmd << end_pos << " " << start_pos << " }" << table_name << " TRange.\r";
    std::string command = cmd.str();
    
    LOG_DEBUG("📋 Comando MMP: '" + command.substr(0, command.length()-1) + "\\r'");
    
    // Limpiar buffer del socket
    flushSocketBuffer();
    
    if (!sendCommand(command)) {
        LOG_ERROR("Error enviando comando MMP");
        return {};
    }
    
    // Calcular bytes esperados: header (2 bytes) + datos (num_floats * 4 bytes)
    int num_floats = end_pos - start_pos + 1;
    size_t expected_bytes = num_floats * 4;
    
    std::vector<uint8_t> raw_data = receiveData(expected_bytes);
    if (raw_data.empty()) {
        LOG_ERROR("Error recibiendo datos binarios de tabla: " + table_name);
        return {};
    }
    
    if (!validateDataIntegrity(raw_data, table_name)) {
        LOG_WARNING("⚠️ Posible contaminación en datos de " + table_name);
    }
    
    // Convertir bytes a floats usando little endian
    std::vector<float> floats = convertBytesToFloats(raw_data);
    
    LOG_DEBUG("✓ Tabla " + table_name + " leída: " + std::to_string(floats.size()) + " valores");
    for (size_t i = 0; i < floats.size(); i++) {
        LOG_DEBUG("  [" + std::to_string(start_pos + i) + "] = " + std::to_string(floats[i]));
    }
    
    return floats;
}

// Comunicación TCP usando protocolo MMP de Opto 22
bool PACControlClient::sendCommand(const std::string& command) {
    if (socket_fd_ < 0) {
        LOG_ERROR("Socket inválido - Marcando como desconectado");
        connected_ = false;
        return false;
    }
    
    ssize_t bytes_sent = send(socket_fd_, command.c_str(), command.length(), 0);
    if (bytes_sent != (ssize_t)command.length()) {
        LOG_ERROR("Error enviando comando MMP - Esperado: " + std::to_string(command.length()) + 
                 " Enviado: " + std::to_string(bytes_sent) + " (errno: " + std::to_string(errno) + ")");
        connected_ = false;
        return false;
    }
    
    return true;
}

// Recibir datos binarios con header PAC de 2 bytes
std::vector<uint8_t> PACControlClient::receiveData(size_t expected_bytes) {
    std::vector<uint8_t> buffer;
    
    // El PAC envía 2 bytes de header + datos reales
    size_t total_expected = expected_bytes + 2;
    
    LOG_DEBUG("📥 Esperando " + std::to_string(total_expected) + " bytes total (2 header + " + 
             std::to_string(expected_bytes) + " datos)");
    
    buffer.resize(total_expected);
    size_t bytes_received = 0;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Loop principal de recepción
    while (bytes_received < total_expected) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
        
        // Timeout después de 3 segundos
        if (elapsed.count() > 3000) {
            LOG_DEBUG("⏰ TIMEOUT recibiendo datos después de " + std::to_string(elapsed.count()) + "ms - Marcando como desconectado");
            connected_ = false;
            break;
        }
        
        ssize_t result = recv(socket_fd_, buffer.data() + bytes_received, 
                             total_expected - bytes_received, 0);
        
        if (result > 0) {
            bytes_received += result;
            LOG_DEBUG("📡 Recibidos " + std::to_string(result) + " bytes, total: " + 
                     std::to_string(bytes_received) + "/" + std::to_string(total_expected));
        } else if (result == 0) {
            LOG_DEBUG("❌ Conexión cerrada por el servidor - Marcando como desconectado");
            connected_ = false;
            break;
        } else {
            LOG_DEBUG("❌ Error recv: " + std::string(strerror(errno)) + " - Marcando como desconectado");
            connected_ = false;
            break;
        }
    }
    
    if (bytes_received < total_expected) {
        LOG_DEBUG("⚠️ Datos incompletos: recibidos " + std::to_string(bytes_received) + 
                 ", esperados " + std::to_string(total_expected));
        return {};
    }
    
    // Mostrar header (2 bytes) y datos por separado
    LOG_DEBUG("📋 HEADER PAC (2 bytes): ");
    for (size_t i = 0; i < 2 && i < bytes_received; i++) {
        LOG_DEBUG(std::to_string((int)buffer[i]) + " ");
    }
    
    // Retornar solo los datos sin el header de 2 bytes
    if (bytes_received >= 2) {
        std::vector<uint8_t> data_only(buffer.begin() + 2, buffer.begin() + bytes_received);
        LOG_DEBUG("📊 Retornando " + std::to_string(data_only.size()) + " bytes de datos (sin header de 2 bytes)");
        return data_only;
    } else {
        LOG_DEBUG("❌ No hay suficientes datos para extraer header de 2 bytes");
        return {};
    }
}

// Limpiar buffer del socket para evitar datos residuales
void PACControlClient::flushSocketBuffer() {
    if (socket_fd_ < 0 || !connected_) return;
    
    // Configurar socket como no-bloqueante temporalmente
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);
    
    char temp_buffer[1024];
    int flushed_bytes = 0;
    
    // Leer cualquier dato residual del buffer del socket
    while (true) {
        ssize_t bytes = recv(socket_fd_, temp_buffer, sizeof(temp_buffer), 0);
        if (bytes <= 0) break;
        flushed_bytes += bytes;
    }
    
    // Restaurar el socket a modo bloqueante
    fcntl(socket_fd_, F_SETFL, flags);
    
    if (flushed_bytes > 0) {
        LOG_DEBUG("🧹 Limpiados " + std::to_string(flushed_bytes) + " bytes residuales del socket");
    }
}

// Conversión de datos del protocolo MMP
std::vector<float> PACControlClient::convertBytesToFloats(const std::vector<uint8_t>& data) {
    std::vector<float> floats;
    
    LOG_DEBUG("🔄 convertBytesToFloats: " + std::to_string(data.size()) + " bytes de entrada");
    
    if (data.size() % 4 != 0) {
        LOG_DEBUG("⚠️ ADVERTENCIA: Tamaño de datos no es múltiplo de 4: " + std::to_string(data.size()));
    }
    
    for (size_t i = 0; i + 3 < data.size(); i += 4) {
        // USAR LITTLE ENDIAN (confirmado por el protocolo MMP de Opto 22)
        uint32_t little_endian_val = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
        float little_endian_float;
        memcpy(&little_endian_float, &little_endian_val, sizeof(float));
        
        LOG_DEBUG("  Float[" + std::to_string(i/4) + "]: bytes=" + 
                 std::to_string(data[i]) + " " + std::to_string(data[i+1]) + " " + 
                 std::to_string(data[i+2]) + " " + std::to_string(data[i+3]) +
                 " -> uint32=" + std::to_string(little_endian_val) + 
                 " -> float=" + std::to_string(little_endian_float));
        
        // Verificar si el valor es razonable
        if (std::isfinite(little_endian_float) && !std::isnan(little_endian_float)) {
            floats.push_back(little_endian_float);
            LOG_DEBUG("    ✅ Valor aceptado: " + std::to_string(little_endian_float));
        } else {
            LOG_DEBUG("    ⚠️ Valor extraño, usando 0.0: " + std::to_string(little_endian_float));
            floats.push_back(0.0f);
        }
    }
    
    LOG_DEBUG("✓ Convertidos " + std::to_string(floats.size()) + " floats");
    return floats;
}

std::vector<int32_t> PACControlClient::convertBytesToInt32s(const std::vector<uint8_t>& data) {
    std::vector<int32_t> int32s;
    
    LOG_DEBUG("🔄 convertBytesToInt32s: " + std::to_string(data.size()) + " bytes de entrada");
    
    for (size_t i = 0; i + 3 < data.size(); i += 4) {
        // Little endian para int32 también
        uint32_t int_val = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
        
        int32_t signed_val;
        memcpy(&signed_val, &int_val, sizeof(int32_t));
        
        LOG_DEBUG("  Int32[" + std::to_string(i/4) + "]: bytes=" + 
                 std::to_string(data[i]) + " " + std::to_string(data[i+1]) + " " + 
                 std::to_string(data[i+2]) + " " + std::to_string(data[i+3]) +
                 " -> uint32=" + std::to_string(int_val) + 
                 " -> int32=" + std::to_string(signed_val));
        
        int32s.push_back(signed_val);
    }
    
    return int32s;
}

std::string PACControlClient::convertBytesToASCII(const std::vector<uint8_t>& bytes) {
    std::string result;
    for (uint8_t byte : bytes) {
        if (byte >= 32 && byte <= 126) { // Solo caracteres ASCII imprimibles
            result += static_cast<char>(byte);
        }
    }
    return result;
}

// Validar integridad de datos para detectar contaminación
bool PACControlClient::validateDataIntegrity(const std::vector<uint8_t>& data, const std::string& table_name) {
    if (data.empty()) return false;
    
    // Ser más tolerante con tamaños variables del PAC
    if (data.size() < 4) { // Al menos 1 valor (4 bytes)
        return false;
    }
    
    return true;
}

// Funciones auxiliares que deben estar implementadas para compatibilidad
bool PACControlClient::updateTagManagerFromOPCUATable() {
    if (!tag_manager_) {
        LOG_ERROR("TagManager no disponible para actualización TBL_OPCUA");
        return false;
    }
    
    if (opcua_table_cache_.empty()) {
        LOG_WARNING("Cache TBL_OPCUA vacío, no hay datos para actualizar");
        return false;
    }
    
    LOG_INFO("🔄 Iniciando actualización TagManager desde TBL_OPCUA - Cache: " + std::to_string(opcua_table_cache_.size()) + " valores, Mapeos: " + std::to_string(tag_opcua_index_map_.size()));
    
    size_t updates_processed = 0;
    
    // CORRECCIÓN: Solo iterar sobre tags que tienen mapeo en TBL_OPCUA
    for (const auto& mapping : tag_opcua_index_map_) {
        const std::string& tag_name = mapping.first;
        int opcua_index = mapping.second;
        
        try {
            if (opcua_index >= 0 && opcua_index < static_cast<int>(opcua_table_cache_.size())) {
                float new_value = opcua_table_cache_[opcua_index];
                
                // TBL_OPCUA contiene solo valores PV, actualizar SOLO el PV del tag
                std::string pv_tag_name = tag_name + ".PV";
                auto pv_tag = tag_manager_->getTag(pv_tag_name);
                
                if (pv_tag) {
                    TagValue new_tag_value = new_value;
                    tag_manager_->updateTagValue(pv_tag_name, new_tag_value);
                    updates_processed++;
                    LOG_DEBUG("✅ Actualizado " + pv_tag_name + " [índice " + std::to_string(opcua_index) + "] = " + std::to_string(new_value));
                } else {
                    LOG_DEBUG("⚠️ Tag PV no encontrado: " + pv_tag_name);
                }
            } else {
                LOG_DEBUG("⚠️ Índice fuera de rango para " + tag_name + ": " + std::to_string(opcua_index));
            }
        } catch (const std::exception& e) {
            LOG_DEBUG("Error actualizando tag desde TBL_OPCUA " + tag_name + ": " + std::string(e.what()));
        }
    }
    
    if (updates_processed > 0) {
        LOG_SUCCESS("📊 TBL_OPCUA: " + std::to_string(updates_processed) + " tags actualizados exitosamente");
        return true;
    } else {
        LOG_WARNING("⚠️ TBL_OPCUA: No se procesaron actualizaciones - verificar mapeos");
    }
    
    return false;
}

// Actualizar TagManager desde tabla individual con datos reales
bool PACControlClient::updateTagManagerFromIndividualTable(const std::string& table_name, const std::vector<float>& values) {
    if (!tag_manager_ || values.empty()) {
        return false;
    }
    
    // Extraer nombre del tag de la tabla (ej: "TBL_ET_1601" -> "ET_1601")
    std::string tag_name = table_name;
    if (tag_name.substr(0, 4) == "TBL_") {
        tag_name = tag_name.substr(4); // Remover prefijo "TBL_"
    }
    
    // Variables típicas por orden en tablas PAC (según configuración)
    std::vector<std::string> variable_names = {
        "PV", "SV", "SetHH", "SetH", "SetL", "SetLL", 
        "Input", "percent", "min", "max", "SIM_Value"
    };
    
    size_t updates_processed = 0;
    
    // CORRECCIÓN CRÍTICA: No sobrescribir valores PV que vienen de TBL_OPCUA
    // Los valores PV reales están en TBL_OPCUA, las tablas individuales pueden tener datos obsoletos
    
    // Actualizar cada variable del tag, EXCEPTO PV si tiene mapeo en TBL_OPCUA
    for (size_t i = 0; i < values.size() && i < variable_names.size(); i++) {
        try {
            std::string variable_name = variable_names[i];
            
            // SKIP PV si este tag tiene mapeo en TBL_OPCUA (datos más actualizados)
            if (variable_name == "PV" && tag_opcua_index_map_.find(tag_name) != tag_opcua_index_map_.end()) {
                LOG_DEBUG("⏭️ Saltando " + tag_name + ".PV (se actualiza desde TBL_OPCUA)");
                continue;
            }
            
            std::string full_tag_name = tag_name + "." + variable_name;
            TagValue new_tag_value = values[i];
            
            // 🛡️ PROTECCIÓN CRÍTICA: No sobrescribir si fue escrito por cliente recientemente
            auto tag = tag_manager_->getTag(full_tag_name);
            if (tag) {
                uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                
                uint64_t client_write_time = tag->getClientWriteTimestamp();
                uint64_t time_since_client_write = (current_time > client_write_time) ? 
                    (current_time - client_write_time) : 0;
                
                // Si fue escrito por cliente en los últimos 60 segundos, NO sobrescribir
                if (client_write_time > 0 && time_since_client_write < 60000) {
                    LOG_SUCCESS("🛡️ PROTECCIÓN: " + full_tag_name + " escrito por cliente hace " + 
                              std::to_string(time_since_client_write) + "ms - NO sobrescribir");
                    continue;
                }
            }
            
            // Actualizar la variable en el TagManager
            tag_manager_->updateTagValue(full_tag_name, new_tag_value);
            updates_processed++;
            
            LOG_DEBUG("📊 " + full_tag_name + " = " + std::to_string(values[i]));
            
        } catch (const std::exception& e) {
            LOG_DEBUG("Error actualizando " + tag_name + "." + variable_names[i] + ": " + std::string(e.what()));
        }
    }
    
    if (updates_processed > 0) {
        LOG_DEBUG("✅ " + table_name + ": " + std::to_string(updates_processed) + " variables actualizadas");
        return true;
    }
    
    return false;
}

int PACControlClient::getTagOPCUATableIndex(const std::string& tag_name) const {
    auto it = tag_opcua_index_map_.find(tag_name);
    if (it != tag_opcua_index_map_.end()) {
        return it->second;
    }
    return -1;
}

bool PACControlClient::loadTagOPCUAMapping(const std::string& config_file) {
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            LOG_WARNING("No se pudo abrir archivo de configuración: " + config_file);
            return false;
        }
        
        nlohmann::json config;
        file >> config;
        
        if (config.contains("tags")) {
            for (const auto& tag_config : config["tags"]) {
                if (tag_config.contains("name") && tag_config.contains("opcua_table_index")) {
                    std::string tag_name = tag_config["name"];
                    int index = tag_config["opcua_table_index"];
                    tag_opcua_index_map_[tag_name] = index;
                }
            }
        }
        
        LOG_INFO("📊 Cargado mapeo TBL_OPCUA: " + std::to_string(tag_opcua_index_map_.size()) + " tags");
        
        // DEBUG: Mostrar algunos mapeos cargados
        LOG_DEBUG("Mapeos cargados:");
        for (const auto& pair : tag_opcua_index_map_) {
            LOG_DEBUG("  " + pair.first + " -> índice " + std::to_string(pair.second));
        }
        
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
        
        double total_time = stats_.avg_response_time_ms * (stats_.successful_reads - 1) + response_time_ms;
        stats_.avg_response_time_ms = total_time / stats_.successful_reads;
    } else {
        stats_.failed_reads++;
    }
}

const PACControlClient::ClientStats& PACControlClient::getStats() const {
    return stats_;
}

std::string PACControlClient::getStatsReport() const {
    std::stringstream ss;
    ss << "PAC Control Client Statistics:\n";
    ss << "  Connected: " << (connected_ ? "Yes" : "No") << "\n";
    ss << "  Successful reads: " << stats_.successful_reads << "\n";
    ss << "  Failed reads: " << stats_.failed_reads << "\n";
    ss << "  TBL_OPCUA reads: " << stats_.opcua_table_reads << "\n";
    ss << "  Average response time: " << stats_.avg_response_time_ms << " ms\n";
    return ss.str();
}

void PACControlClient::resetStats() {
    stats_ = ClientStats{};
    stats_.last_success = std::chrono::steady_clock::now();
}

std::string PACControlClient::cleanASCIINumber(const std::string& ascii_str) {
    std::string result;
    bool decimal_found = false;
    bool negative_found = false;
    bool exponent_found = false;
    bool exponent_sign_found = false;
    
    for (char c : ascii_str) {
        if (c >= '0' && c <= '9') {
            result += c;
        }
        else if (c == '-' && result.empty() && !negative_found) {
            result += c;
            negative_found = true;
        }
        else if (c == '.' && !decimal_found && !exponent_found) {
            result += c;
            decimal_found = true;
        }
        else if ((c == 'e' || c == 'E') && !result.empty() && !exponent_found) {
            result += c;
            exponent_found = true;
        }
        else if ((c == '+' || c == '-') && exponent_found && 
                 !exponent_sign_found &&
                 (result.back() == 'e' || result.back() == 'E')) {
            result += c;
            exponent_sign_found = true;
        }
        else if (c == ' ' && !result.empty()) {
            break;
        }
    }
    
    if (result.empty() || result == "-" || result == "." || result == "e" || result == "E") {
        return "0";
    }
    
    return result;
}

float PACControlClient::convertStringToFloat(const std::string& str) {
    if (str.empty()) {
        return 0.0f;
    }
    
    try {
        float value = std::stof(str);
        
        if (std::isnan(value) || std::isinf(value)) {
            LOG_DEBUG("⚠️ VALOR FLOAT INVÁLIDO: " + str + " -> " + std::to_string(value));
            return 0.0f;
        }
        
        return value;
        
    } catch (const std::exception& e) {
        LOG_DEBUG("❌ ERROR: Conversión float fallida: '" + str + "' - " + e.what());
        return 0.0f;
    }
}

int32_t PACControlClient::convertStringToInt32(const std::string& str) {
    if (str.empty()) {
        return 0;
    }
    
    try {
        double double_value = std::stod(str);
        
        if (double_value > INT32_MAX || double_value < INT32_MIN) {
            LOG_DEBUG("⚠️ VALOR FUERA DE RANGO INT32: " + str + " -> " + std::to_string(double_value));
            return 0;
        }
        
        int32_t value = static_cast<int32_t>(double_value);
        return value;
        
    } catch (const std::exception& e) {
        LOG_DEBUG("❌ ERROR: Conversión int32 fallida: '" + str + "' - " + e.what());
        return 0;
    }
}

// Implementaciones stub para mantener compatibilidad
std::vector<int32_t> PACControlClient::readInt32Table(const std::string& table_name, int start_pos, int end_pos) {
    // TODO: Implementar lectura de tablas int32 con protocolo MMP
    return {};
}

float PACControlClient::readSingleFloatVariableByTag(const std::string& tag_name) {
    // TODO: Implementar lectura de variables individuales con protocolo MMP
    return 0.0f;
}

int32_t PACControlClient::readSingleInt32VariableByTag(const std::string& tag_name) {
    // TODO: Implementar lectura de variables individuales int32 con protocolo MMP
    return 0;
}

bool PACControlClient::writeFloatTableIndex(const std::string& table_name, int index, float value) {
    if (!connected_) {
        LOG_ERROR("🔴 PAC no conectado para escritura");
        return false;
    }
    
    LOG_INFO("📝 ESCRIBIENDO AL PAC: " + table_name + "[" + std::to_string(index) + "] = " + std::to_string(value));
    
    std::lock_guard<std::mutex> lock(socket_mutex_);
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // Construir comando MMP para escritura de float en tabla
        // Formato: 's index }table_name valor\r' (set float at index in table)
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << value;
        std::string command = "s " + std::to_string(index) + " }" + table_name + " " + oss.str() + "\r";
        
        LOG_INFO("📤 Comando MMP: '" + command.substr(0, command.length()-1) + "'");
        
        // Enviar comando
        if (!sendCommand(command)) {
            LOG_ERROR("❌ Error enviando comando de escritura");
            updateStats(false, 0.0);
            return false;
        }
        
        // Recibir confirmación (el PAC devuelve datos si fue exitoso)
        if (!receiveWriteConfirmation()) {
            LOG_ERROR("❌ No se recibió confirmación de escritura");
            updateStats(false, 0.0);
            return false;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        double response_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        updateStats(true, response_time);
        stats_.successful_writes++;
        
        LOG_SUCCESS("✅ ESCRITURA EXITOSA: " + table_name + "[" + std::to_string(index) + "] = " + std::to_string(value));
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("💥 Excepción en writeFloatTableIndex: " + std::string(e.what()));
        updateStats(false, 0.0);
        stats_.failed_writes++;
        return false;
    }
}

bool PACControlClient::writeInt32TableIndex(const std::string& table_name, int index, int32_t value) {
    if (!connected_) {
        LOG_ERROR("🔴 PAC no conectado para escritura int32");
        return false;
    }
    
    LOG_INFO("📝 ESCRIBIENDO INT32 AL PAC: " + table_name + "[" + std::to_string(index) + "] = " + std::to_string(value));
    
    std::lock_guard<std::mutex> lock(socket_mutex_);
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // Construir comando MMP para escritura de int32 en tabla
        // Formato: 's index }table_name valor\r' (set int32 at index in table)
        std::string command = "s " + std::to_string(index) + " }" + table_name + " " + std::to_string(value) + "\r";
        
        LOG_INFO("📤 Comando MMP int32: '" + command.substr(0, command.length()-1) + "'");
        
        // Enviar comando
        if (!sendCommand(command)) {
            LOG_ERROR("❌ Error enviando comando de escritura int32");
            updateStats(false, 0.0);
            return false;
        }
        
        // Recibir confirmación (el PAC devuelve datos si fue exitoso)
        if (!receiveWriteConfirmation()) {
            LOG_ERROR("❌ No se recibió confirmación de escritura int32");
            updateStats(false, 0.0);
            return false;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        double response_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        updateStats(true, response_time);
        stats_.successful_writes++;
        
        LOG_SUCCESS("✅ ESCRITURA INT32 EXITOSA: " + table_name + "[" + std::to_string(index) + "] = " + std::to_string(value));
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("💥 Excepción en writeInt32TableIndex: " + std::string(e.what()));
        updateStats(false, 0.0);
        stats_.failed_writes++;
        return false;
    }
}

bool PACControlClient::writeSingleFloatVariable(const std::string& variable_name, float value) {
    if (!connected_) {
        LOG_ERROR("💥 PAC desconectado - no se puede escribir variable " + variable_name);
        return false;
    }

    auto start_time = std::chrono::steady_clock::now();
    LOG_INFO("📤 Escribiendo variable individual: " + variable_name + " = " + std::to_string(value));

    std::lock_guard<std::mutex> lock(socket_mutex_);
    
    try {
        flushSocketBuffer();
        
        // Formato MMP para escribir variable individual: 's }variable_name value\r'
        std::ostringstream command_stream;
        command_stream << "s }" << variable_name << " " << std::fixed << std::setprecision(6) << value << "\r";
        std::string command = command_stream.str();
        
        LOG_DEBUG("📋 Comando MMP variable: '" + command.substr(0, command.length()-1) + "\\r'");
        
        if (!sendCommand(command)) {
            LOG_ERROR("💥 Error enviando comando de escritura para variable " + variable_name);
            updateStats(false, 0);
            return false;
        }
        
        // Esperar confirmación del PAC
        if (!receiveWriteConfirmation()) {
            LOG_ERROR("💥 PAC no confirmó escritura de variable " + variable_name);
            updateStats(false, 0);
            return false;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        double response_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        LOG_SUCCESS("✅ Variable " + variable_name + " = " + std::to_string(value) + " escrita exitosamente en " + std::to_string(response_time) + "ms");
        updateStats(true, response_time);
        stats_.successful_writes++;
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("💥 Excepción escribiendo variable " + variable_name + ": " + e.what());
        updateStats(false, 0);
        return false;
    }
}

// **MODO SIMULACIÓN TEMPORAL**: Generar datos realistas cuando PAC devuelve solo ceros
std::vector<float> PACControlClient::generateSimulatedData(size_t num_values) {
    static bool simulation_initialized = false;
    static std::chrono::steady_clock::time_point start_time;
    
    if (!simulation_initialized) {
        start_time = std::chrono::steady_clock::now();
        simulation_initialized = true;
        LOG_INFO("🎭 Iniciando modo simulación con datos realistas para planta de gas");
    }
    
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
    
    std::vector<float> simulated_data(num_values);
    
    // Generar datos simulados realistas para planta de gas
    for (size_t i = 0; i < num_values; i++) {
        float base_value = 0.0f;
        float variation = 0.0f;
        
        // Diferentes tipos de variables según el índice
        if (i < 10) {
            // Transmisores de flujo (ET): 0-1000 m3/h con variación sinusoidal
            base_value = 150.0f + (i * 50.0f);
            variation = 10.0f * std::sin(elapsed_seconds * 0.1f + i);
        } else if (i < 20) {
            // Transmisores de presión (PIT): 0-10 bar con pequeña variación
            base_value = 2.5f + (i * 0.3f);
            variation = 0.1f * std::sin(elapsed_seconds * 0.15f + i);
        } else if (i < 35) {
            // Transmisores de temperatura (TIT): 20-150°C con variación lenta
            base_value = 45.0f + (i * 2.0f);
            variation = 2.0f * std::sin(elapsed_seconds * 0.05f + i);
        } else if (i < 45) {
            // Controladores (setpoints): valores más estables
            base_value = 100.0f + (i * 10.0f);
            variation = 1.0f * std::sin(elapsed_seconds * 0.02f + i);
        } else {
            // Otras variables: valores diversos
            base_value = 50.0f + (i * 5.0f);
            variation = 5.0f * std::sin(elapsed_seconds * 0.08f + i);
        }
        
        simulated_data[i] = base_value + variation;
        
        // Asegurar valores positivos para medidas físicas
        if (simulated_data[i] < 0.0f) {
            simulated_data[i] = 0.1f;
        }
    }
    
    // Log ocasional para mostrar que la simulación está activa
    if (elapsed_seconds % 30 == 0) {
        LOG_INFO("🎭 Simulación activa - Datos: T=" + std::to_string(simulated_data[25]) + "°C, " +
                "P=" + std::to_string(simulated_data[15]) + "bar, " +
                "F=" + std::to_string(simulated_data[5]) + "m3/h");
    }
    
    return simulated_data;
}

bool PACControlClient::writeSingleInt32Variable(const std::string& variable_name, int32_t value) {
    // TODO: Implementar escritura de variables individuales int32 con protocolo MMP
    return false;
}

std::vector<uint8_t> PACControlClient::receiveASCIIResponse() {
    // TODO: Implementar recepción de respuestas ASCII
    return {};
}

bool PACControlClient::receiveWriteConfirmation() {
    // TODO: Implementar recepción de confirmación de escritura
    return false;
}