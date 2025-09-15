#include "pac_control_client.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstring>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cmath>  // Para isnan, isinf
#include <fcntl.h>  // Para fcntl y O_NONBLOCK

#include "common.h"  

using namespace std;

PACControlClient::PACControlClient(const string &ip, int port)
    : pac_ip(ip), pac_port(port), sock(-1), connected(false), cache_enabled(false)  // DESHABILITADO PARA DEBUG
{
}

PACControlClient::~PACControlClient()
{
    disconnect();
}

bool PACControlClient::connect()
{
    lock_guard<mutex> lock(comm_mutex);

    if (connected)
        return true;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        cerr << "Error creando socket" << endl;
        return false;
    }

    // Configurar timeout para socket
    struct timeval timeout;
    timeout.tv_sec = 3; // 3 segundos timeout
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(pac_port);

    if (inet_pton(AF_INET, pac_ip.c_str(), &server_addr.sin_addr) <= 0)
    {
        cerr << "Dirección IP inválida: " << pac_ip << endl;
        close(sock);
        return false;
    }

    if (::connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(sock);
        return false;
    }

    connected = true;
    return true;
}

void PACControlClient::disconnect()
{
    lock_guard<mutex> lock(comm_mutex);

    if (sock >= 0)
    {
        close(sock);
        sock = -1;
    }
    connected = false;
}

// CORRECCIÓN: Reemplazar cout por LOG_DEBUG en las funciones clave
vector<float> PACControlClient::readFloatTable(const string &table_name,
                                               int start_pos, int end_pos)
{
    lock_guard<mutex> lock(comm_mutex);

    if (!connected)
    {
        cerr << "No conectado al PAC" << endl;
        return {};
    }

    // Verificar cache solo si está habilitado
    string cache_key = table_name + "_" + to_string(start_pos) + "_" + to_string(end_pos);
    if (cache_enabled && isCacheValid(cache_key))
    {
        LOG_DEBUG("📋 CACHE HIT: Usando datos cached para " << table_name);
        return table_cache[cache_key].data;
    }

    stringstream cmd;
    
    LOG_DEBUG("📊 PARÁMETROS: start_pos=" << start_pos << ", end_pos=" << end_pos);
    
    // 🔧 COMANDO CORRECTO según README.md: Formato "<end_pos> 0 }<tabla> TRange.\r"
    // Ejemplo: "9 0 }TBL_TT_11006 TRange.\r" - SIEMPRE usar end_pos=9 para leer tabla completa (10 elementos, índices 0-9)
    // El PAC siempre devuelve la tabla completa independientemente del mapeo JSON
    if (table_name.find("TBL_DA_") != string::npos) {
        LOG_DEBUG("🚨 LEYENDO TABLA DE ALARMAS: " << table_name);
        cmd << end_pos << " " << start_pos << " }" << table_name << " TRange.\r";
    } else {
        LOG_DEBUG("📊 LEYENDO TABLA DE DATOS: " << table_name);
        cmd << end_pos << " " << start_pos << " }" << table_name << " TRange.\r";
    }

    string command = cmd.str();
    LOG_DEBUG("📊 LEYENDO TABLA DE DATOS: " << table_name);
    LOG_DEBUG("📋 Comando en bytes: ");
    for (char c : command) {
        LOG_DEBUG("0x" << hex << (int)(unsigned char)c << " ");
    }
    LOG_DEBUG(dec);
    LOG_DEBUG("🔍 TIMESTAMP: " << chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch()).count() << "ms");

    // 🔧 SOLUCIÓN: Limpiar buffer del socket antes de enviar comando
    flushSocketBuffer();
    
    if (!sendCommand(command))
    {
        cerr << "Error enviando comando" << endl;
        return {};
    }

    // 🔧 SOLUCIÓN: Las tablas responden en BINARIO con header 00 00
    // A diferencia de variables simples que responden en ASCII terminado en 0x20
    size_t expected_bytes = 4 +end_pos * 4; // 10 floats × 4 bytes = 40 bytes de datos

    vector<uint8_t> raw_data = receiveData(expected_bytes);
    if (raw_data.empty())
    {
        cerr << "Error recibiendo datos binarios de tabla: " << table_name << endl;
        return {};
    }
    
    LOG_DEBUG("📋 DATOS BINARIOS TABLA (" << raw_data.size() << " bytes): ");
    for (size_t i = 0; i < raw_data.size(); i++) {
        LOG_DEBUG(hex << setfill('0') << setw(2) << (int)raw_data[i] << " ");
    }
    LOG_DEBUG(dec);
    
    // Convertir bytes a floats (IEEE 754 little endian)
    vector<float> floats;
    for (size_t i = 0; i + 3 < raw_data.size(); i += 4) {
        uint32_t raw_bits = raw_data[i] | 
                           (raw_data[i+1] << 8) | 
                           (raw_data[i+2] << 16) | 
                           (raw_data[i+3] << 24);
        
        float value;
        memcpy(&value, &raw_bits, 4);
        floats.push_back(value);
        
        LOG_DEBUG("📊 [" << (i/4) << "] Raw: " << hex << setfill('0') << setw(8) << raw_bits 
                     << dec << " -> float: " << value);
    }
    
    LOG_DEBUG("📊 Total valores parseados: " << floats.size());
    
    // Si el cache está habilitado, guardar los datos
    
    // Si el cache está habilitado, guardar los datos
    if (cache_enabled && !floats.empty()) {
        table_cache[cache_key] = {floats, chrono::steady_clock::now(), true};
        LOG_DEBUG("💾 DATOS GUARDADOS EN CACHE: " << table_name << " con " << floats.size() << " valores");
    }

    DEBUG_INFO("✓ Tabla " << table_name << " leída: " << floats.size() << " valores");
    for (size_t i = 0; i < floats.size(); i++)
    {
        LOG_DEBUG("  [" << (start_pos + i) << "] = " << floats[i]);
    }

    return floats;
    LOG_DEBUG("📊 Total valores parseados: " << floats.size());

    // Verificar consistencia de datos comparando con cache anterior
    auto prev_cache = table_cache.find(cache_key);
    if (prev_cache != table_cache.end() && prev_cache->second.valid) {
        bool values_changed = false;
        if (floats.size() == prev_cache->second.data.size()) {
            for (size_t i = 0; i < floats.size(); i++) {
                float diff = abs(floats[i] - prev_cache->second.data[i]);
                if (diff > 0.001f) { // Tolerancia de 0.001 para cambios significativos
                    values_changed = true;
                    LOG_DEBUG("🔄 CAMBIO DETECTADO en [" << (start_pos + i) << "]: " 
                         << prev_cache->second.data[i] << " -> " << floats[i] 
                         << " (diff: " << diff << ")");
                }
            }
            if (!values_changed) {
                LOG_DEBUG("✅ VALORES ESTABLES: Sin cambios significativos en " << table_name);
            }
        }
    }

    // Guardar en cache
    CacheEntry entry;
    entry.data = floats;
    entry.timestamp = chrono::steady_clock::now();
    entry.valid = true;
    table_cache[cache_key] = entry;

    DEBUG_INFO("✓ Tabla " << table_name << " leída: " << floats.size() << " valores");
    for (size_t i = 0; i < floats.size(); i++)
    {
        LOG_DEBUG("  [" << (start_pos + i) << "] = " << floats[i]);
    }

    return floats;
}

float PACControlClient::readFloatVariable(const string &table_name, int index)
{
    vector<float> values = readFloatTable(table_name, index, index);
    return values.empty() ? 0.0f : values[0];
}

int32_t PACControlClient::readInt32Variable(const string &table_name, int index)
{
    vector<int32_t> values = readInt32Table(table_name, index, index);
    return values.empty() ? 0 : values[0];
}

// Lectura de tablas de enteros int32 (para alarmas TBL_TA*, TBL_LA*)
vector<int32_t> PACControlClient::readInt32Table(const string &table_name,
                                                 int start_pos, int end_pos)
{
    lock_guard<mutex> lock(comm_mutex);

    if (!connected)
    {
        cerr << "No conectado al PAC" << endl;
        return {};
    }

    // Verificar cache
    string cache_key = table_name + "_int32_" + to_string(start_pos) + "_" + to_string(end_pos);
    if (isCacheValid(cache_key))
    {
        // Convertir float cache a int32 (necesitamos cache separado para int32)
        // Por simplicidad, leer directamente por ahora
    }

    // Comando PAC Control CORRECTO para int32 (alarmas):
    // Ejemplo: "9 0 }TBL_DA_0001 TRange.\r" - SIEMPRE usar end_pos=9 para tabla completa
    stringstream cmd;
    
    // Detectar tipo de tabla y usar comando apropiado
    if (table_name.find("TBL_DA_") != string::npos) {
        // Para alarmas, usar mismo formato que funciona para floats
        cmd << end_pos << " " << start_pos << " }" << table_name << " TRange.\r";
    } else {
        cmd << end_pos << " " << start_pos << " }" << table_name << " TRange.\r";
    }

    string command = cmd.str();

    if (!sendCommand(command))
    {
        cerr << "Error enviando comando int32" << endl;
        return {};
    }

    // CORRECCIÓN: El PAC siempre envía la tabla completa para alarmas (20 bytes datos + 2 header = 22 total)
    // Para tablas de alarmas (int32): 5 valores × 4 bytes = 20 bytes
    size_t expected_bytes = 4 + end_pos * 4; // Siempre 20 bytes de datos (5 int32 × 4 bytes)

    vector<uint8_t> raw_data = receiveData(expected_bytes);
    if (raw_data.empty())
    {
        cerr << "Error recibiendo datos int32 de tabla" << endl;
        return {};
    }

    // Convertir bytes a int32 (little endian)
    vector<int32_t> ints = convertBytesToInt32s(raw_data);

    return ints;
}

string PACControlClient::readStringVariable(const string &variable_name)
{
    lock_guard<mutex> lock(comm_mutex);

    if (!connected)
    {
        cerr << "No conectado al PAC" << endl;
        return "";
    }

    stringstream cmd;
    cmd << variable_name << " PRINT$\r";

    string command = cmd.str();

    if (!sendCommand(command))
    {
        cerr << "Error enviando comando string" << endl;
        return "";
    }

    string response = receiveResponse();

   return response;

}

bool PACControlClient::writeFloatVariable(const string &table_name, int index, float value)
{
    // TODO: Implementar protocolo de escritura
    // Formato probable: "índice tabla valor TWrite" o similar
    lock_guard<mutex> lock(comm_mutex);

    if (!connected)
        return false;

    stringstream cmd;
    cmd << index << " " << table_name << " " << value << " TWrite\r";

    string command = cmd.str();
    //cout << "Escribiendo float: " << command << endl;

    bool result = sendCommand(command);
    if (result)
    {
        // Invalidar cache para esta tabla
        clearCache();
        //cout << "✓ Variable float escrita: " << table_name << "[" << index << "] = " << value << endl;
    }

    return result;
}

bool PACControlClient::writeInt32Variable(const string &table_name, int index, int32_t value)
{
    lock_guard<mutex> lock(comm_mutex);

    if (!connected)
        return false;

    stringstream cmd;
    cmd << index << " " << table_name << " " << value << " TWrite\r";

    string command = cmd.str();
    //cout << "Escribiendo int32: " << command << endl;

    bool result = sendCommand(command);
    if (result)
    {
        // Invalidar cache para esta tabla
        clearCache();
        //cout << "✓ Variable int32 escrita: " << table_name << "[" << index << "] = 0x"
            // << hex << value << dec << endl;
    }

    return result;
}

bool PACControlClient::writeFloatTable(const string &table_name, const vector<float> &values)
{
    bool success = true;
    for (size_t i = 0; i < values.size(); i++)
    {
        if (!writeFloatVariable(table_name, i, values[i]))
        {
            success = false;
        }
    }
    return success;
}

bool PACControlClient::writeInt32Table(const string &table_name, const vector<int32_t> &values)
{
    bool success = true;
    for (size_t i = 0; i < values.size(); i++)
    {
        if (!writeInt32Variable(table_name, i, values[i]))
        {
            success = false;
        }
    }
    return success;
}

// Implementaciones de comunicación de bajo nivel
bool PACControlClient::sendCommand(const string &command)
{
    if (sock < 0) {
        //cout << "❌ Socket inválido - Marcando como desconectado" << endl;
        connected = false;
        return false;
    }

    ssize_t bytes_sent = send(sock, command.c_str(), command.length(), 0);
    if (bytes_sent != (ssize_t)command.length()) {
        //cout << "❌ Error enviando comando - Esperado: " << command.length() << " Enviado: " << bytes_sent << " (errno: " << errno << ")" << endl;
        //cout << "🔌 ERROR DE ENVÍO - Marcando como desconectado" << endl;
        connected = false;  // MARCAR COMO DESCONECTADO
        return false;
    }
    
    return true;
}

// CORRECCIÓN: Función para recibir datos con header PAC de 2 bytes
vector<uint8_t> PACControlClient::receiveData(size_t expected_bytes)
{
    vector<uint8_t> buffer;
    
    // CORRECCIÓN: El PAC envía 2 bytes de header + datos reales
    size_t total_expected = expected_bytes + 2;  // 2 bytes header + datos
    
    LOG_DEBUG("📥 Esperando " << total_expected << " bytes total (2 header + " << expected_bytes << " datos)");
    
    buffer.resize(total_expected);
    size_t bytes_received = 0;
    
    auto start_time = chrono::steady_clock::now();
    
    // 🔧 CORREGIDO: Loop principal de recepción
    while (bytes_received < total_expected)
    {
        auto current_time = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(current_time - start_time);
        
        // Timeout después de 3 segundos
        if (elapsed.count() > 3000) {
            DEBUG_INFO("⏰ TIMEOUT recibiendo datos después de " << elapsed.count() << "ms");
            break;
        }
        
        // � CORREGIDO: recv() en modo bloqueante normal
        ssize_t result = recv(sock, buffer.data() + bytes_received, total_expected - bytes_received, 0);
        
        if (result > 0)
        {
            bytes_received += result;
            LOG_DEBUG("📡 Recibidos " << result << " bytes, total: " << bytes_received << "/" << total_expected);
        }
        else if (result == 0)
        {
            DEBUG_INFO("❌ Conexión cerrada por el servidor");
            break;
        }
        else
        {
            DEBUG_INFO("❌ Error recv: " << strerror(errno));
            break;
        }
    }
    
    // 🔧 CORREGIDO: Verificar datos después del loop
    if (bytes_received < total_expected)
    {
        DEBUG_INFO("⚠️ Datos incompletos: recibidos " << bytes_received << ", esperados " << total_expected);
        return {};
    }
    
    // CORRECCIÓN: Mostrar header (2 bytes) y datos por separado
    LOG_DEBUG("📋 HEADER PAC (2 bytes): ");
    for (size_t i = 0; i < 2 && i < bytes_received; i++) {
        LOG_DEBUG(hex << setfill('0') << setw(2) << (int)buffer[i] << " ");
    }
    LOG_DEBUG(dec);
    
    LOG_DEBUG("📋 DATOS REALES (" << (bytes_received - 2) << " bytes): ");
    for (size_t i = 2; i < min(bytes_received, size_t(22)); i++) {
        LOG_DEBUG(hex << setfill('0') << setw(2) << (int)buffer[i] << " ");
    }
    LOG_DEBUG(dec);
    
    // CORRECCIÓN: Retornar solo los datos sin el header de 2 bytes
    if (bytes_received >= 2) {
        vector<uint8_t> data_only(buffer.begin() + 2, buffer.end());
        LOG_DEBUG("✅ Tabla válida: Retornando " << data_only.size() << " bytes de datos (sin header de 2 bytes)");
        return data_only;
    } else {
        DEBUG_INFO("❌ No hay suficientes datos para extraer header de 2 bytes");
        return {};
    }
}
// 🔧 NUEVO: Limpiar buffer del socket para evitar datos residuales
void PACControlClient::flushSocketBuffer() {
    if (sock < 0 || !connected) return;
    
    // Configurar socket como no-bloqueante temporalmente
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    char temp_buffer[1024];
    int flushed_bytes = 0;
    
    // Leer cualquier dato residual del buffer del socket
    while (true) {
        ssize_t bytes = recv(sock, temp_buffer, sizeof(temp_buffer), 0);
        if (bytes <= 0) break;
        flushed_bytes += bytes;
    }
    
    // Restaurar el socket a modo bloqueante
    fcntl(sock, F_SETFL, flags);
    
    if (flushed_bytes > 0) {
        // //cout << "🧹 BUFFER LIMPIADO: Eliminados " << flushed_bytes << " bytes residuales del socket" << endl;
    }
}

// 🔧 NUEVO: Validar integridad de datos para detectar contaminación
bool PACControlClient::validateDataIntegrity(const vector<uint8_t>& data, const string& table_name) {
    if (data.empty()) return false;
    
    // Verificar tamaño esperado
    bool is_alarm_table = (table_name.find("TBL_DA_") == 0) || 
                         (table_name.find("TBL_PA_") == 0) || 
                         (table_name.find("TBL_LA_") == 0) || 
                         (table_name.find("TBL_TA_") == 0);
    
    size_t expected_size;
    if (is_alarm_table) {
        expected_size = 5 * 4; // 5 int32 = 20 bytes
    } else {
        expected_size = 10 * 4; // 10 float = 40 bytes
    }
    
    // CORRECCIÓN: Ser más tolerante con tamaños variables del PAC
    if (data.size() < 4) { // Al menos 1 valor (4 bytes)
        //cout << "⚠️  VALIDACIÓN FALLIDA: " << table_name << " - Datos insuficientes: " 
            // << data.size() << " bytes (mínimo 4)" << endl;
        return false;
    }
    
    if (data.size() != expected_size) {
        //cout << "⚠️  ADVERTENCIA: " << table_name << " - Tamaño esperado: " << expected_size 
            // << " bytes, recibido: " << data.size() << " bytes (continuando...)" << endl;
        // No fallar, solo advertir y continuar
    }
    
    // Detectar patrones sospechosos (todos ceros o valores muy grandes)
    bool all_zeros = true;
    bool has_large_values = false;
    
    for (size_t i = 0; i < data.size(); i += 4) {
        uint32_t value = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
        
        if (value != 0) all_zeros = false;
        if (value > 1000000) has_large_values = true; // Valores > 1M son sospechosos
    }
    
    // Si es una tabla que debería tener valores pero solo tiene ceros, es sospechoso
    if (all_zeros && table_name.find("11003") != string::npos) {
        //cout << "✅ VALIDACIÓN OK: " << table_name << " - Datos todos en cero (esperado según PAC)" << endl;
    } else if (has_large_values && table_name.find("11003") != string::npos) {
        //cout << "🚨 CONTAMINACIÓN DETECTADA: " << table_name << " tiene valores anómalos grandes" << endl;
        return false;
    }
    
    return true;
}
// CORRECCIÓN: Función para convertir bytes a floats (ahora con header correcto)
vector<float> PACControlClient::convertBytesToFloats(const vector<uint8_t>& data) {
    vector<float> floats;
    
    DEBUG_INFO("🔄 convertBytesToFloats: " << data.size() << " bytes de entrada (sin header de 2 bytes)");
    
    // Mostrar datos raw en hex por bloques de 4
    DEBUG_INFO("🔍 RAW DATA por bloques de 4 bytes:");
    for (size_t i = 0; i < data.size(); i += 4) {
        if (i + 3 < data.size()) {
            DEBUG_INFO("  Bytes[" << setw(2) << i << "-" << setw(2) << (i+3) << "]: " 
                 << hex << setfill('0') << setw(2) << (int)data[i] << " "
                 << setw(2) << (int)data[i+1] << " " << setw(2) << (int)data[i+2] << " "
                 << setw(2) << (int)data[i+3] << dec);
        }
    }
    
    if (data.size() % 4 != 0) {
        DEBUG_INFO("⚠️ ADVERTENCIA: Tamaño de datos no es múltiplo de 4: " << data.size());
    }
    
    for (size_t i = 0; i + 3 < data.size(); i += 4) {
        
        // USAR LITTLE ENDIAN (confirmado por el test Python)
        uint32_t little_endian_val = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
        float little_endian_float;
        memcpy(&little_endian_float, &little_endian_val, sizeof(float));
        
        DEBUG_INFO("  Float[" << i/4 << "]: bytes=" << hex << setfill('0') 
             << setw(2) << (int)data[i] << " " << setw(2) << (int)data[i+1] << " "
             << setw(2) << (int)data[i+2] << " " << setw(2) << (int)data[i+3] << dec
             << " -> uint32=" << little_endian_val << " -> float=" << little_endian_float);
        
        // Verificar si el valor es razonable
        if (std::isfinite(little_endian_float) && !std::isnan(little_endian_float)) {
            floats.push_back(little_endian_float);
            DEBUG_INFO("    ✅ Valor aceptado: " << little_endian_float);
        } else {
            DEBUG_INFO("    ⚠️ Valor extraño, usando 0.0: " << little_endian_float);
            floats.push_back(0.0f);
        }
    }
    
    DEBUG_INFO("✓ Convertidos " << floats.size() << " floats");
    DEBUG_INFO("🎯 Valores finales: ");
    for (size_t i = 0; i < floats.size(); i++) {
        DEBUG_INFO("  [" << i << "] = " << floats[i]);
    }
    
    return floats;
}

// CORRECCIÓN: También actualizar convertBytesToInt32s para header de 2 bytes
vector<int32_t> PACControlClient::convertBytesToInt32s(const vector<uint8_t>& data) {
    vector<int32_t> int32s;
    
    DEBUG_INFO("🔄 convertBytesToInt32s: " << data.size() << " bytes de entrada (sin header de 2 bytes)");
    
    for (size_t i = 0; i + 3 < data.size(); i += 4) {
        // Little endian para int32 también
        uint32_t int_val = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
        
        int32_t signed_val;
        memcpy(&signed_val, &int_val, sizeof(int32_t));
        
        DEBUG_INFO("  Int32[" << i/4 << "]: bytes=" << hex << setfill('0') 
             << setw(2) << (int)data[i] << " " << setw(2) << (int)data[i+1] << " "
             << setw(2) << (int)data[i+2] << " " << setw(2) << (int)data[i+3] << dec
             << " -> uint32=" << int_val << " -> int32=" << signed_val);
        
        int32s.push_back(signed_val);
    }
    
    return int32s;
}
vector<uint8_t> PACControlClient::convertFloatsToBytes(const vector<float> &floats)
{
    vector<uint8_t> bytes;

    for (float f : floats)
    {
        uint32_t int_val;
        memcpy(&int_val, &f, 4);

        // Little endian
        bytes.push_back(int_val & 0xFF);
        bytes.push_back((int_val >> 8) & 0xFF);
        bytes.push_back((int_val >> 16) & 0xFF);
        bytes.push_back((int_val >> 24) & 0xFF);
    }

    return bytes;
}

vector<uint8_t> PACControlClient::convertInt32sToBytes(const vector<int32_t> &ints)
{
    vector<uint8_t> bytes;

    for (int32_t i : ints)
    {
        uint32_t int_val = static_cast<uint32_t>(i);

        // Little endian
        bytes.push_back(int_val & 0xFF);
        bytes.push_back((int_val >> 8) & 0xFF);
        bytes.push_back((int_val >> 16) & 0xFF);
        bytes.push_back((int_val >> 24) & 0xFF);
    }

    return bytes;
}

bool PACControlClient::isCacheValid(const string &key)
{
    auto it = table_cache.find(key);
    if (it == table_cache.end() || !it->second.valid)
    {
        return false;
    }

    auto now = chrono::steady_clock::now();
    auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                       now - it->second.timestamp)
                       .count();

    return elapsed < CACHE_TIMEOUT_MS;
}

void PACControlClient::clearCache()
{
    table_cache.clear();
}

// Comandos básicos heredados del análisis anterior
string PACControlClient::receiveResponse()
{
    char buffer[2048] = {0}; // Buffer más grande
    ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        return string(buffer);
    }
    return "";
}

// Función para detectar automáticamente el tipo de datos en una tabla
bool PACControlClient::detectDataType(const string& table_name, bool& is_integer_data) {
    lock_guard<mutex> lock(comm_mutex);
    
    if (!connected) {
        return false;
    }
    
    //cout << "🔍 ANÁLISIS: Detectando tipo de datos para " << table_name << endl;
    
    // PASO 1: Detección basada en nombre de tabla (criterio principal)
    if (table_name.find("TBL_DA_") == 0 || table_name.find("TBL_LA_") == 0 || table_name.find("TBL_PA_") == 0) {
        //cout << "🏷️  TIPO POR NOMBRE: " << table_name << " -> ENTEROS (DA/LA/PA = datos digitales/alarmas)" << endl;
        is_integer_data = true;
        return true;
    }
    
    if (table_name.find("TBL_DT_") == 0 || table_name.find("TBL_PT_") == 0 || 
        table_name.find("TBL_TT_") == 0 || table_name.find("TBL_LT_") == 0) {
        //cout << "🏷️  TIPO POR NOMBRE: " << table_name << " -> FLOTANTES (DT/PT/TT/LT = transmisores)" << endl;
        is_integer_data = false;
        return true;
    }
    
    // PASO 2: Si no coincide con nomenclatura conocida, usar análisis de contenido
    //cout << "⚠️  TABLA DESCONOCIDA: " << table_name << " - Analizando contenido..." << endl;
    
    // Leer una muestra pequeña de datos
    stringstream cmd;
    cmd << "9 0 }" << table_name << " TRange.\r";
    
    string command = cmd.str();
    
    if (!sendCommand(command)) {
        return false;
    }
    
    vector<uint8_t> raw_data = receiveData(8); // Solo 2 valores para análisis
    if (raw_data.size() < 8) {
        return false;
    }
    
    // Interpretar como float
    vector<float> as_floats = convertBytesToFloats(raw_data);
    // Interpretar como int32
    vector<int32_t> as_ints = convertBytesToInt32s(raw_data);
    
    // Análisis: Si los valores float son muy pequeños (< 1e-30) pero los int32 son razonables
    // es probable que sean datos integer
    int reasonable_floats = 0;
    int reasonable_ints = 0;
    
    for (size_t i = 0; i < min(as_floats.size(), as_ints.size()); i++) {
        // Verificar si el float es un valor razonable
        if (abs(as_floats[i]) > 1e-30 && abs(as_floats[i]) < 1e30 && 
            !std::isnan(as_floats[i]) && !std::isinf(as_floats[i])) {
            reasonable_floats++;
        }
        
        // Verificar si el int32 está en rango razonable (0-65535 típico para tags)
        if (as_ints[i] >= 0 && as_ints[i] <= 100000) {
            reasonable_ints++;
        }
    }
    
    //cout << "🔍 ANÁLISIS " << table_name << ": floats razonables=" << reasonable_floats 
         //<< ", ints razonables=" << reasonable_ints << endl;
    //cout << "    Como float: ";
    for (float f : as_floats) //cout << f << " ";
    //cout << endl << "    Como int32: ";
    for (int32_t i : as_ints) //cout << i << " ";
    //cout << endl;
    
    // Si hay más integers razonables que floats, es probable que sea integer data
    is_integer_data = (reasonable_ints > reasonable_floats);
    
    //cout << "✅ DETECCIÓN: " << table_name << " -> " 
        // << (is_integer_data ? "DATOS INTEGER" : "DATOS FLOAT") << endl;
    
    return true;
}

// Función inteligente para leer tabla como float (con detección automática)
vector<float> PACControlClient::readTableAsFloat(const string& table_name, 
                                                int start_pos, int end_pos) {
    // Primero detectar el tipo de datos
    bool is_integer_data;
    if (!detectDataType(table_name, is_integer_data)) {
        //cout << "⚠️  No se pudo detectar tipo de datos para " << table_name << endl;
        return readFloatTable(table_name, start_pos, end_pos); // Fallback
    }
    
    if (is_integer_data) {
        //cout << "🔄 CONVERSIÓN: Leyendo " << table_name << " como int32 y convirtiendo a float" << endl;
        vector<int32_t> int_values = readTableAsInt32(table_name, start_pos, end_pos);
        vector<float> float_values;
        for (int32_t val : int_values) {
            float_values.push_back(static_cast<float>(val));
        }
        return float_values;
    } else {
        //cout << "📊 DIRECTO: Leyendo " << table_name << " como float nativo" << endl;
        return readFloatTable(table_name, start_pos, end_pos);
    }
}

// Función para leer tabla como int32 (optimizada)
vector<int32_t> PACControlClient::readTableAsInt32(const string& table_name,
                                                  int start_pos, int end_pos) {
    lock_guard<mutex> lock(comm_mutex);

    if (!connected) {
        cerr << "No conectado al PAC" << endl;
        return {};
    }

    // Verificar cache solo si está habilitado
    string cache_key = table_name + "_int32_" + to_string(start_pos) + "_" + to_string(end_pos);
    
    stringstream cmd;
    cmd << "9 0 }" << table_name << " TRange.\r";

    string command = cmd.str();
    //cout << "📊 LEYENDO TABLA INT32: " << table_name << endl;

    // 🔧 SOLUCIÓN: Limpiar buffer del socket antes de enviar comando
    flushSocketBuffer();
    
    if (!sendCommand(command)) {
        cerr << "Error enviando comando int32" << endl;
        return {};
    }

    // Calcular bytes esperados (cada int32 = 4 bytes)
    int num_ints = end_pos - start_pos + 1;
    size_t expected_bytes = num_ints * 4;

    vector<uint8_t> raw_data = receiveData(expected_bytes);
    if (raw_data.empty()) {
        cerr << "Error recibiendo datos int32 de tabla" << endl;
        return {};
    }
    
    // 🔧 SOLUCIÓN: Validar integridad de los datos recibidos
    if (!validateDataIntegrity(raw_data, table_name)) {
        //cout << "⚠️  DATOS INT32 RECHAZADOS por validación de integridad: " << table_name << endl;
        
        // 🔧 RETRY: Intentar una segunda vez con delay si hay contaminación
        //cout << "🔄 REINTENTANDO lectura int32 después de 100ms..." << endl;
        this_thread::sleep_for(chrono::milliseconds(100));
        
        flushSocketBuffer();
        if (sendCommand(command)) {
            vector<uint8_t> retry_data = receiveData(expected_bytes);
            if (!retry_data.empty() && validateDataIntegrity(retry_data, table_name)) {
                //cout << "✅ RETRY INT32 EXITOSO: Datos válidos obtenidos en segundo intento" << endl;
                raw_data = retry_data;
            } else {
                //cout << "❌ RETRY INT32 FALLIDO: Datos siguen siendo inválidos" << endl;
                return {};
            }
        } else {
            return {};
        }
    }

    // Análisis de estabilidad de datos  
    analyzeDataStability(table_name, raw_data);

    // Convertir bytes a int32 (little endian)
    vector<int32_t> ints = convertBytesToInt32s(raw_data);

    // //cout << "✓ Tabla int32 " << table_name << " leída: " << ints.size() << " valores" << endl;
    for (size_t i = 0; i < ints.size(); i++) {
        // //cout << "  [" << (start_pos + i) << "] = " << ints[i] << endl;
    }

    return ints;
}

// Función para analizar estabilidad de datos
void PACControlClient::analyzeDataStability(const string& table_name, const vector<uint8_t>& raw_data) {
    // Agregar a historial
    read_history[table_name].push_back(raw_data);
    
    // Mantener solo las últimas 5 lecturas
    if (read_history[table_name].size() > 5) {
        read_history[table_name].erase(read_history[table_name].begin());
    }
    
    if (read_history[table_name].size() >= 2) {
       // //cout << "📊 ANÁLISIS DE ESTABILIDAD " << table_name << ":" << endl;
        
        // Comparar con lectura anterior
        auto& current = read_history[table_name].back();
        auto& previous = read_history[table_name][read_history[table_name].size() - 2];
        
        if (current.size() == previous.size()) {
            int differences = 0;
            for (size_t i = 0; i < current.size(); i++) {
                if (current[i] != previous[i]) {
                    differences++;
                    //cout << "  BYTE " << i << ": " << hex << (int)previous[i] << " -> " << (int)current[i] << dec << endl;
                }
            }
            
            if (differences == 0) {
                //cout << "  ✅ DATOS IDÉNTICOS - Sin cambios" << endl;
            } else {
                //cout << "  ⚠️  DATOS CAMBIARON - " << differences << " bytes diferentes" << endl;
                
                // Analizar si es ruido o cambios reales
                float change_percentage = (float)differences / current.size() * 100;
                if (change_percentage < 10) {
                    //cout << "  🔍 POSIBLE RUIDO (" << change_percentage << "% cambió)" << endl;
                } else {
                    //cout << "  📈 CAMBIO SIGNIFICATIVO (" << change_percentage << "% cambió)" << endl;
                }
            }
        }
        
        // Análisis de todas las lecturas
        if (read_history[table_name].size() >= 3) {
            //cout << "  📈 TENDENCIA (últimas " << read_history[table_name].size() << " lecturas):" << endl;
            bool all_identical = true;
            for (size_t i = 1; i < read_history[table_name].size(); i++) {
                if (read_history[table_name][i] != read_history[table_name][0]) {
                    all_identical = false;
                    break;
                }
            }
            
            if (all_identical) {
                //cout << "    ✅ COMPLETAMENTE ESTABLE" << endl;
            } else {
                //cout << "    ⚠️  VALORES VARIABLES" << endl;
            }
        }
    }
}

vector<string> PACControlClient::getTasks()
{
    string response = sendRawCommand("ANY.TASKS?");
    // TODO: Parsear respuesta
    return {};
}

// CORRECCIÓN: Funciones auxiliares con LOG_DEBUG
string PACControlClient::cleanASCIINumber(const string& ascii_str)
{
    string result;
    bool decimal_found = false;
    bool negative_found = false;
    bool exponent_found = false;
    bool exponent_sign_found = false;
    
    LOG_DEBUG("🔍 LIMPIANDO NÚMERO ASCII: '" << ascii_str << "'");
    
    // Procesar cada caracter para extraer un número válido
    for (char c : ascii_str) {
        // Ignorar bytes nulos y caracteres de control
        if (c == '\0' || c == '\r' || c == '\n' || c == '\t') {
            continue;
        }
        
        // Dígitos siempre son válidos
        if (std::isdigit(c)) {
            result += c;
        }
        // Signo negativo solo al principio
        else if (c == '-' && !negative_found && result.empty()) {
            result += c;
            negative_found = true;
        }
        // Punto decimal solo una vez
        else if (c == '.' && !decimal_found && !exponent_found) {
            result += c;
            decimal_found = true;
        }
        // Notación científica (e/E)
        else if ((c == 'e' || c == 'E') && !exponent_found && !result.empty()) {
            result += c;
            exponent_found = true;
        }
        // Signo en exponente
        else if ((c == '+' || c == '-') && exponent_found && !exponent_sign_found && 
                 (result.back() == 'e' || result.back() == 'E')) {
            result += c;
            exponent_sign_found = true;
        }
        // Espacios al final terminan el número
        else if (c == ' ' && !result.empty()) {
            break;
        }
        // Otros caracteres no válidos se ignoran
    }
    
    LOG_DEBUG("🔍 NÚMERO LIMPIO: '" << result << "'");
    
    // Validar que el resultado es un número válido
    if (result.empty() || result == "-" || result == "." || result == "e" || result == "E") {
        LOG_DEBUG("⚠️  NÚMERO INVÁLIDO después de limpiar: '" << result << "'");
        return "0";
    }
    
    return result;
}


float PACControlClient::convertStringToFloat(const string& str) {
    if (str.empty()) {
        return 0.0f;
    }
    
    try {
        float value = std::stof(str);
        
        if (std::isnan(value) || std::isinf(value)) {
            LOG_DEBUG("⚠️  VALOR FLOAT INVÁLIDO: " << str << " -> " << value);
            return 0.0f;
        }
        
        LOG_DEBUG("✅ CONVERSIÓN EXITOSA: '" << str << "' -> " << value);
        return value;
        
    } catch (const std::invalid_argument& e) {
        LOG_DEBUG("❌ ERROR: Argumento inválido para conversión: '" << str << "' - " << e.what());
        return 0.0f;
    } catch (const std::out_of_range& e) {
        LOG_DEBUG("❌ ERROR: Valor fuera de rango: '" << str << "' - " << e.what());
        return 0.0f;
    } catch (const exception& e) {
        LOG_DEBUG("❌ ERROR: Excepción general en conversión: '" << str << "' - " << e.what());
        return 0.0f;
    }
}


int32_t PACControlClient::convertStringToInt32(const string& str) {
    if (str.empty()) {
        return 0;
    }
    
    try {
        double double_value = std::stod(str);
        
        if (double_value > INT32_MAX || double_value < INT32_MIN) {
            LOG_DEBUG("⚠️  VALOR FUERA DE RANGO INT32: " << str << " -> " << double_value);
            return 0;
        }
        
        int32_t value = static_cast<int32_t>(double_value);
        LOG_DEBUG("✅ CONVERSIÓN INT32 EXITOSA: '" << str << "' -> " << value);
        return value;
        
    } catch (const std::invalid_argument& e) {
        LOG_DEBUG("❌ ERROR: Argumento inválido para conversión int32: '" << str << "' - " << e.what());
        return 0;
    } catch (const std::out_of_range& e) {
        LOG_DEBUG("❌ ERROR: Valor fuera de rango int32: '" << str << "' - " << e.what());
        return 0;
    } catch (const exception& e) {
        LOG_DEBUG("❌ ERROR: Excepción general en conversión int32: '" << str << "' - " << e.what());
        return 0;
    }
}


// CORRECCIÓN: Función readSingleFloatVariableByTag con LOG_DEBUG
float PACControlClient::readSingleFloatVariableByTag(const string& tag_name)
{
    lock_guard<mutex> lock(comm_mutex);

    if (!connected) {
        cerr << "No conectado al PAC" << endl;
        return 0.0f;
    }

   // DEBUG_INFO("📊 LEYENDO VARIABLE FLOAT INDIVIDUAL: " << tag_name);

    // COMANDO CORRECTO confirmado por pruebas Python
    string command = "^" + tag_name + " @@ F.\r";
    
    LOG_DEBUG("📋 Enviando comando: '" << command.substr(0, command.length()-1) << "\\r'");

    flushSocketBuffer();
    
    if (!sendCommand(command)) {
        cerr << "❌ Error enviando comando variable float individual" << endl;
        return 0.0f;
    }

    // Recibir respuesta ASCII terminada en espacio 0x20
    vector<uint8_t> raw_data = receiveASCIIResponse();
    if (raw_data.empty()) {
        cerr << "❌ Error: respuesta vacía para variable float individual" << endl;
        return 0.0f;
    }

    // 🔍 DIAGNÓSTICO: Mostrar datos RAW recibidos
    LOG_DEBUG("🔍 VARIABLE FLOAT RAW DATA (" << raw_data.size() << " bytes): ");
    for (size_t i = 0; i < raw_data.size(); i++) {
        LOG_DEBUG(hex << setfill('0') << setw(2) << (int)raw_data[i] << " ");
    }
    LOG_DEBUG(dec);

    // Convertir bytes a string ASCII limpio
    string ascii_response = convertBytesToASCII(raw_data);
    LOG_DEBUG("🔍 ASCII RESPONSE: '" << ascii_response << "'");
    
    // Limpiar y extraer número (ahora con soporte correcto para notación científica)
    string clean_value = cleanASCIINumber(ascii_response);
    LOG_DEBUG("🔍 CLEAN VALUE: '" << clean_value << "'");
    
    if (clean_value.empty() || clean_value == "0") {
        LOG_DEBUG("⚠️  ADVERTENCIA: Valor limpio vacío o cero para " << tag_name);
        return 0.0f;
    }
    
    // CORRECCIÓN: Usar función mejorada de conversión
    float value = convertStringToFloat(clean_value);
    
    DEBUG_INFO("✅ Variable float individual leída: " << tag_name << " = " << value);
    
    // Mostrar también en notación científica si es un número grande
    if (abs(value) >= 1000) {
        LOG_DEBUG(" (científica: " << scientific << value << fixed << ")");
    }

    return value;
}


// CORRECCIÓN: Función readSingleInt32VariableByTag con LOG_DEBUG
int32_t PACControlClient::readSingleInt32VariableByTag(const string& tag_name)
{
    lock_guard<mutex> lock(comm_mutex);

    if (!connected) {
        cerr << "No conectado al PAC" << endl;
        return 0;
    }

    DEBUG_INFO("📊 LEYENDO VARIABLE INT32 INDIVIDUAL: " << tag_name);

    // COMANDO CORRECTO para int32 (sin F)
    string command = "^" + tag_name + " @@ .\r";
    
    LOG_DEBUG("📋 Enviando comando: '" << command.substr(0, command.length()-1) << "\\r'");

    flushSocketBuffer();
    
    if (!sendCommand(command)) {
        cerr << "❌ Error enviando comando variable int32 individual" << endl;
        return 0;
    }

    // Recibir respuesta ASCII terminada en espacio 0x20
    vector<uint8_t> raw_data = receiveASCIIResponse();
    if (raw_data.empty()) {
        cerr << "❌ Error: respuesta vacía para variable int32 individual" << endl;
        return 0;
    }

    // 🔍 DIAGNÓSTICO: Mostrar datos RAW recibidos
    LOG_DEBUG("🔍 VARIABLE INT32 RAW DATA (" << raw_data.size() << " bytes): ");
    for (size_t i = 0; i < raw_data.size(); i++) {
        LOG_DEBUG(hex << setfill('0') << setw(2) << (int)raw_data[i] << " ");
    }
    LOG_DEBUG(dec);

    // Convertir bytes a string ASCII limpio
    string ascii_response = convertBytesToASCII(raw_data);
    LOG_DEBUG("🔍 ASCII RESPONSE: '" << ascii_response << "'");
    
    string clean_value = cleanASCIINumber(ascii_response);
    LOG_DEBUG("🔍 CLEAN VALUE: '" << clean_value << "'");
    
    if (clean_value.empty() || clean_value == "0") {
        LOG_DEBUG("⚠️  ADVERTENCIA: Valor limpio vacío o cero para " << tag_name);
        return 0;
    }
    
    // CORRECCIÓN: Usar función mejorada de conversión
    int32_t value = convertStringToInt32(clean_value);
    
    DEBUG_INFO("✅ Variable int32 individual leída: " << tag_name << " = " << value 
         << " (0x" << hex << value << dec << ")");

    return value;
}

// CORRECCIÓN: Función receiveASCIIResponse con LOG_DEBUG
vector<uint8_t> PACControlClient::receiveASCIIResponse()
{
    vector<uint8_t> raw_data;
    char byte;
    
    auto start_time = chrono::steady_clock::now();
    const int timeout_ms = 3000; // 3 segundos timeout

    LOG_DEBUG("📋 Esperando respuesta ASCII (terminador: espacio 0x20)...");
    
    while (true) {
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - start_time).count();
        
        if (elapsed > timeout_ms) {
            LOG_DEBUG("⏰ TIMEOUT recibiendo respuesta ASCII después de " << elapsed << "ms");
            break;
        }
        
        ssize_t bytes = recv(sock, &byte, 1, 0); // Bloqueante para mejor estabilidad
        
        if (bytes == 1) {
            // CORRECCIÓN: Detectar terminador espacio 0x20
            if (static_cast<uint8_t>(byte) == 0x20) {
                LOG_DEBUG("📋 Terminador espacio (0x20) detectado - Fin de respuesta");
                break;
            }
            
            // Agregar byte a la respuesta
            raw_data.push_back(static_cast<uint8_t>(byte));
            
            // Debug: mostrar cada byte recibido
            LOG_DEBUG("📡 Byte recibido: 0x" << hex << setfill('0') << setw(2) 
                 << (int)(uint8_t)byte << dec);
            if (byte >= 32 && byte <= 126) {
                LOG_DEBUG(" ('" << byte << "')");
            }
            
            // Protección contra respuestas muy largas
            if (raw_data.size() > 50) {
                LOG_DEBUG("⚠️  Respuesta muy larga (>50 bytes), cortando");
                break;
            }
            
        } else if (bytes == 0) {
            LOG_DEBUG("🔌 Conexión cerrada por el PAC");
            connected = false;
            break;
        } else if (bytes < 0) {
            LOG_DEBUG("❌ Error en recv: errno=" << errno);
            connected = false;
            break;
        }
    }
    
    LOG_DEBUG("📋 Respuesta ASCII completa recibida: " << raw_data.size() << " bytes");
    
    // Mostrar datos completos en hex
    LOG_DEBUG("🔍 Datos hex: ");
    for (uint8_t b : raw_data) {
        LOG_DEBUG(hex << setfill('0') << setw(2) << (int)b << " ");
    }
    LOG_DEBUG(dec);
    
    return raw_data;
}

// Función para convertir bytes a string ASCII
string PACControlClient::convertBytesToASCII(const vector<uint8_t>& bytes)
{
    string result;
    for (uint8_t byte : bytes) {
        if (byte >= 32 && byte <= 126) { // Solo caracteres ASCII imprimibles
            result += static_cast<char>(byte);
        }
    }
    return result;
}

// NUEVA FUNCIÓN: Función auxiliar para validación de variable individual
bool PACControlClient::validateSingleVariableIntegrity(const vector<uint8_t>& data, 
                                                      const string& tag_name) {
    if (data.empty()) {
        //cout << "⚠️  VALIDACIÓN INDIVIDUAL FALLIDA: " << tag_name << " - Datos vacíos" << endl;
        return false;
    }
    
    // Para variables individuales, esperamos datos ASCII válidos
    bool has_ascii_printable = false;
    for (uint8_t byte : data) {
        if (byte >= 32 && byte <= 126) { // Caracteres ASCII imprimibles
            has_ascii_printable = true;
            break;
        }
    }
    
    if (!has_ascii_printable) {
        //cout << "⚠️  VALIDACIÓN INDIVIDUAL FALLIDA: " << tag_name << " - Sin caracteres ASCII válidos" << endl;
        return false;
    }
    
    //cout << "✅ VALIDACIÓN INDIVIDUAL OK: " << tag_name << " - Datos ASCII válidos" << endl;
    return true;
}

bool PACControlClient::receiveWriteConfirmation()
{
    // PAC responde con 2 bytes (FF FF) para confirmación exitosa de escritura
    vector<uint8_t> response(2);
    size_t bytes_received = 0;
    
    auto start_time = chrono::steady_clock::now();
    
    while (bytes_received < 2)
    {
        auto current_time = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(current_time - start_time);
        
        if (elapsed.count() > 1000) // Timeout 1 segundo
        {
            LOG_DEBUG("⚠️ TIMEOUT esperando confirmación de escritura después de " << elapsed.count() << "ms");
            LOG_DEBUG("📊 Recibidos " << bytes_received << " de 2 bytes");
            return false;
        }
        
        ssize_t result = recv(sock, response.data() + bytes_received, 2 - bytes_received, MSG_DONTWAIT);
        
        if (result > 0)
        {
            bytes_received += result;
            LOG_DEBUG("📡 Confirmación: recibidos " << result << " bytes, total: " << bytes_received << "/2");
        }
        else if (result == 0)
        {
            LOG_DEBUG("🔌 Conexión cerrada por el servidor durante confirmación");
            connected = false;
            return false;
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            LOG_DEBUG("❌ Error recibiendo confirmación: " << strerror(errno));
            return false;
        }
        
        // Pequeña pausa para evitar uso excesivo de CPU
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    
    // Verificar si la respuesta es la confirmación esperada (00 00)
    if (bytes_received == 2 && response[0] == 0x00 && response[1] == 0x00)
    {
        LOG_DEBUG("✅ Confirmación de escritura exitosa: 00 00");
        return true;
    }
    else
    {
        LOG_DEBUG("❌ Confirmación de escritura inválida - Esperado: 00 00, Recibido: " 
                     << hex << setfill('0') << setw(2) << (int)response[0] << " " 
                     << setw(2) << (int)response[1] << dec);
        return false;
    }
}

void PACControlClient::clearCacheForTable(const std::string &table_name)
{
}

string PACControlClient::sendRawCommand(const string &command)
{
    lock_guard<mutex> lock(comm_mutex);

    if (!connected)
        return "";

    string full_cmd = command + "\r";
    if (!sendCommand(full_cmd))
        return "";

    return receiveResponse();
}

// CORRECCIÓN: Actualizar writeSingleFloatVariable para usar confirmación de 2 bytes
bool PACControlClient::writeSingleFloatVariable(const std::string& variable_name, float value) {
    lock_guard<mutex> lock(comm_mutex);

    if (!connected) {
        cerr << "No conectado al PAC" << endl;
        return false;
    }

    // Formato: <valor> ^<variable> @!\r
    std::ostringstream cmd;
    cmd << std::setprecision(7) << value << " ^" << variable_name << " @!\r";
    std::string command = cmd.str();

    DEBUG_INFO("🔥 Escribiendo variable FLOAT individual: " << variable_name << " = " << value);
    LOG_DEBUG("📋 Comando: '" << command.substr(0, command.length()-1) << "\\r'");

    flushSocketBuffer();

    if (!sendCommand(command)) {
        DEBUG_INFO("❌ Error enviando comando de escritura float");
        return false;
    }

    // CORRECCIÓN: Usar función específica para confirmación de escritura
    bool success = receiveWriteConfirmation();
    
    if (success) {
        DEBUG_INFO("✅ Variable FLOAT escrita exitosamente: " << variable_name << " = " << value);
    } else {
        DEBUG_INFO("❌ Error en escritura FLOAT: " << variable_name);
    }

    return success;
}

// CORRECCIÓN: Actualizar writeSingleInt32Variable para usar confirmación de 2 bytes
bool PACControlClient::writeSingleInt32Variable(const std::string& variable_name, int32_t value) {
    lock_guard<mutex> lock(comm_mutex);

    if (!connected) {
        cerr << "No conectado al PAC" << endl;
        return false;
    }

    // Formato: <valor> ^<variable> @!\r
    std::ostringstream cmd;
    cmd << value << " ^" << variable_name << " @!\r";
    std::string command = cmd.str();

    DEBUG_INFO("🔥 Escribiendo variable INT32 individual: " << variable_name << " = " << value);
    LOG_DEBUG("📋 Comando: '" << command.substr(0, command.length()-1) << "\\r'");

    flushSocketBuffer();

    if (!sendCommand(command)) {
        DEBUG_INFO("❌ Error enviando comando de escritura int32");
        return false;
    }

    // CORRECCIÓN: Usar función específica para confirmación de escritura
    bool success = receiveWriteConfirmation();
    
    if (success) {
        DEBUG_INFO("✅ Variable INT32 escrita exitosamente: " << variable_name << " = " << value 
             << " (0x" << hex << value << dec << ")");
    } else {
        DEBUG_INFO("❌ Error en escritura INT32: " << variable_name);
    }

    return success;
}// CORRECCIÓN: writeFloatTableIndex SIN verificación que causa deadlock
bool PACControlClient::writeFloatTableIndex(const std::string& table_name, int index, float value) {
    lock_guard<mutex> lock(comm_mutex);

    if (!connected) {
        cerr << "No conectado al PAC" << endl;
        return false;
    }

    // FORMATO CORRECTO confirmado por Python: <valor> <index> }<table> TABLE!\r
    std::ostringstream cmd;
    cmd << std::fixed << std::setprecision(3) << value << " " << index << " }" << table_name << " TABLE!\r";
    std::string command = cmd.str();

    DEBUG_INFO("🔥 Escribiendo tabla FLOAT (FORMATO CORRECTO): " << table_name << "[" << index << "] = " << value);
    LOG_DEBUG("📋 Comando correcto: '" << command.substr(0, command.length()-1) << "\\r'");

    flushSocketBuffer();

    if (!sendCommand(command)) {
        DEBUG_INFO("❌ Error enviando comando de escritura tabla float");
        return false;
    }

    // Usar función específica para confirmación de escritura
    bool success = receiveWriteConfirmation();
    
    if (success) {
        DEBUG_INFO("✅ Tabla FLOAT escrita exitosamente: " << table_name << "[" << index << "] = " << value);
        
        // Invalidar cache para esta tabla
        clearCacheForTable(table_name);
        
        // CORRECCIÓN: NO hacer verificación aquí por deadlock
        // La verificación se hará en la próxima lectura normal
        LOG_DEBUG("✅ Escritura completada, verificación en próxima lectura");
        
    } else {
        DEBUG_INFO("❌ Error en escritura tabla FLOAT: " << table_name << "[" << index << "]");
    }

    return success;
}

// CORRECCIÓN: writeInt32TableIndex SIN verificación que causa deadlock
bool PACControlClient::writeInt32TableIndex(const std::string& table_name, int index, int32_t value) {
    lock_guard<mutex> lock(comm_mutex);

    if (!connected) {
        cerr << "No conectado al PAC" << endl;
        return false;
    }

    // FORMATO CORRECTO confirmado: <valor> <index> }<table> TABLE!\r
    std::ostringstream cmd;
    cmd << value << " " << index << " }" << table_name << " TABLE!\r";
    std::string command = cmd.str();

    DEBUG_INFO("🔥 Escribiendo tabla INT32 (FORMATO CORRECTO): " << table_name << "[" << index << "] = " << value);
    LOG_DEBUG("📋 Comando correcto: '" << command.substr(0, command.length()-1) << "\\r'");

    flushSocketBuffer();

    if (!sendCommand(command)) {
        DEBUG_INFO("❌ Error enviando comando de escritura tabla int32");
        return false;
    }

    // Usar función específica para confirmación de escritura
    bool success = receiveWriteConfirmation();
    
    if (success) {
        DEBUG_INFO("✅ Tabla INT32 escrita exitosamente: " << table_name << "[" << index << "] = " << value 
             << " (0x" << hex << value << dec << ")");
        
        // Invalidar cache para esta tabla
        clearCacheForTable(table_name);
        
        // CORRECCIÓN: NO hacer verificación aquí por deadlock
        LOG_DEBUG("✅ Escritura int32 completada, verificación en próxima lectura");
        
    } else {
        DEBUG_INFO("❌ Error en escritura tabla INT32: " << table_name << "[" << index << "]");
    }

    return success;
}

// ALTERNATIVA: Función de verificación asíncrona (opcional)
void PACControlClient::scheduleWriteVerification(const std::string& table_name, int index, float expected_value) {
    // Esta función podría programar una verificación para después, pero por ahora
    // es mejor confiar en la confirmación 00 00 del PAC y la próxima lectura normal
    LOG_DEBUG("📋 Programando verificación para " << table_name << "[" << index << "] = " << expected_value);
    
    // Marcar en cache que hay un valor pendiente de verificar
    string verify_key = table_name + "_verify_" + to_string(index);
    CacheEntry verify_entry;
    verify_entry.data.push_back(expected_value);
    verify_entry.timestamp = chrono::steady_clock::now();
    verify_entry.valid = true;
    table_cache[verify_key] = verify_entry;
}

// CORRECCIÓN: Función debugWriteOperation mejorada SIN deadlock
void PACControlClient::debugWriteOperation(const std::string& table_name, int index, float value) {
    DEBUG_INFO("🧪 DEBUG ESCRITURA TABLA:");
    DEBUG_INFO("  Tabla: " << table_name);
    DEBUG_INFO("  Índice: " << index);
    DEBUG_INFO("  Valor a escribir: " << value);
    
    // CORRECCIÓN: Leer valor ANTES de hacer writeFloatTableIndex para evitar deadlock
    float value_before = 0.0f;
    bool read_before_success = false;
    
    try {
        // Hacer esta lectura en un scope separado
        {
            vector<float> before = readFloatTable(table_name, index, index);
            if (!before.empty()) {
                value_before = before[0];
                read_before_success = true;
                DEBUG_INFO("  Valor ANTES: " << value_before);
            }
        }
    } catch (...) {
        DEBUG_INFO("  Error leyendo valor antes de escribir");
    }
    
    // Realizar la escritura (que ya tiene su propio lock)
    bool write_success = writeFloatTableIndex(table_name, index, value);
    DEBUG_INFO("  Resultado escritura: " << (write_success ? "✅ ÉXITO" : "❌ FALLO"));
    
    // CORRECCIÓN: NO leer inmediatamente después por deadlock
    // En su lugar, programar verificación o confiar en confirmación PAC
    if (write_success) {
        DEBUG_INFO("  ✅ ESCRITURA COMPLETADA - Confirmación PAC recibida");
        DEBUG_INFO("  📋 Verificación en próxima lectura automática del sistema");
        
        if (read_before_success) {
            float expected_change = abs(value - value_before);
            DEBUG_INFO("  📊 Cambio esperado: " << value_before << " -> " << value << " (Δ=" << expected_change << ")");
        }
    }
}

// ============== IMPLEMENTACIÓN DEL SISTEMA DE ESCRITURAS CRÍTICAS ==============

// Definición de variables estáticas
std::map<std::string, PendingWrite> WriteRegistrationManager::pending_writes;
std::mutex WriteRegistrationManager::write_mutex;
std::chrono::steady_clock::time_point WriteRegistrationManager::last_update_time = std::chrono::steady_clock::now();

void WriteRegistrationManager::registerCriticalWrite(const std::string& nodeId, const std::string& client_info) {
    std::lock_guard<std::mutex> lock(write_mutex);
    pending_writes[nodeId] = PendingWrite(nodeId, true, client_info);
    
    DEBUG_INFO("🔴 ESCRITURA CRÍTICA REGISTRADA: " << nodeId 
              << " (Cliente: " << client_info << ")");
}

void WriteRegistrationManager::registerWrite(const std::string& nodeId, const std::string& client_info) {
    std::lock_guard<std::mutex> lock(write_mutex);
    pending_writes[nodeId] = PendingWrite(nodeId, false, client_info);
    
    DEBUG_INFO("🟡 ESCRITURA REGISTRADA: " << nodeId 
              << " (Cliente: " << client_info << ")");
}

bool WriteRegistrationManager::isWriteRegistered(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(write_mutex);
    return pending_writes.find(nodeId) != pending_writes.end();
}

bool WriteRegistrationManager::isCriticalWrite(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(write_mutex);
    auto it = pending_writes.find(nodeId);
    return (it != pending_writes.end() && it->second.is_critical);
}

void WriteRegistrationManager::consumeWrite(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(write_mutex);
    auto it = pending_writes.find(nodeId);
    if (it != pending_writes.end()) {
        DEBUG_INFO("✅ ESCRITURA CONSUMIDA: " << nodeId 
                  << " (Crítica: " << (it->second.is_critical ? "SÍ" : "NO") << ")");
        pending_writes.erase(it);
    }
}

void WriteRegistrationManager::cleanExpiredWrites() {
    std::lock_guard<std::mutex> lock(write_mutex);
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = pending_writes.begin(); it != pending_writes.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.registered_time).count();
        
        // Escrituras críticas expiran en 30 segundos, normales en 10
        int timeout = it->second.is_critical ? 30 : 10;
        
        if (elapsed > timeout) {
            DEBUG_INFO("⏰ ESCRITURA EXPIRADA: " << it->first 
                      << " (Tiempo: " << elapsed << "s)");
            it = pending_writes.erase(it);
        } else {
            ++it;
        }
    }
}

bool WriteRegistrationManager::isSafeToUpdate() {
    std::lock_guard<std::mutex> lock(write_mutex);
    
    // Si hay escrituras críticas pendientes, NO es seguro actualizar
    for (const auto& pair : pending_writes) {
        if (pair.second.is_critical) {
            DEBUG_INFO("⚠️ ACTUALIZACIÓN BLOQUEADA: Escritura crítica pendiente: " 
                      << pair.first);
            return false;
        }
    }
    
    // Si hay escrituras normales muy recientes (< 2 segundos), esperar
    auto now = std::chrono::steady_clock::now();
    for (const auto& pair : pending_writes) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - pair.second.registered_time).count();
        if (elapsed < 2000) {  // 2 segundos buffer
            DEBUG_INFO("⚠️ ACTUALIZACIÓN RETRASADA: Escritura reciente: " 
                      << pair.first << " (" << elapsed << "ms)");
            return false;
        }
    }
    
    return true;
}

void WriteRegistrationManager::markUpdateTime() {
    std::lock_guard<std::mutex> lock(write_mutex);
    last_update_time = std::chrono::steady_clock::now();
}

std::string WriteRegistrationManager::getWriteInfo(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(write_mutex);
    auto it = pending_writes.find(nodeId);
    if (it != pending_writes.end()) {
        return "Cliente: " + it->second.client_info + 
               ", Crítica: " + (it->second.is_critical ? "SÍ" : "NO");
    }
    return "No registrada";
}

bool WriteRegistrationManager::isVariableCritical(const std::string& nodeId) {
    // Identificar automáticamente variables críticas por nombre
    // Setpoints son críticos
    if (nodeId.find("SetHH") != std::string::npos ||
        nodeId.find("SetH") != std::string::npos ||
        nodeId.find("SetL") != std::string::npos ||
        nodeId.find("SetLL") != std::string::npos) {
        return true;
    }
    
    // Variables de modo/control son críticas
    if (nodeId.find("Mode") != std::string::npos ||
        nodeId.find("Manual") != std::string::npos ||
        nodeId.find("Auto") != std::string::npos ||
        nodeId.find("Enable") != std::string::npos ||
        nodeId.find("Disable") != std::string::npos) {
        return true;
    }
    
    // Variables de emergencia son críticas
    if (nodeId.find("Emergency") != std::string::npos ||
        nodeId.find("Stop") != std::string::npos ||
        nodeId.find("Shutdown") != std::string::npos ||
        nodeId.find("Trip") != std::string::npos) {
        return true;
    }
    
    return false; // Por defecto, no críticas
}
