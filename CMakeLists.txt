cmake_minimum_required(VERSION 3.16)
project(planta_gas VERSION 1.0.0)

# Configuración C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tipo de build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Buscar dependencias
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# open62541
pkg_check_modules(OPEN62541 REQUIRED open62541)

# nlohmann_json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json no encontrado, usando version de sistema")
endif()

# CURL
find_package(CURL REQUIRED)

# httplib (buscar en ubicaciones comunes)
find_path(HTTPLIB_INCLUDE_DIR
    NAMES httplib.h
    PATHS 
        /usr/include
        /usr/local/include
        ${CMAKE_SOURCE_DIR}/third_party
    DOC "Path to httplib header"
)

if(NOT HTTPLIB_INCLUDE_DIR)
    message(WARNING "httplib no encontrado. API HTTP estará deshabilitada.")
    set(ENABLE_HTTP_API OFF)
else()
    message(STATUS "Using httplib from: ${HTTPLIB_INCLUDE_DIR}")
    set(ENABLE_HTTP_API ON)
endif()

# Directorios
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Verificar que los archivos existen antes de compilar
set(REQUIRED_SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/tag.cpp
    ${SRC_DIR}/tag_manager.cpp
)

set(OPTIONAL_SOURCES
    ${SRC_DIR}/opcua_server.cpp
    ${SRC_DIR}/pac_control_client.cpp
    ${SRC_DIR}/tag_management_api.cpp
)

# Solo agregar archivos que existen
set(SOURCES)
foreach(src ${REQUIRED_SOURCES})
    if(EXISTS ${src})
        list(APPEND SOURCES ${src})
    else()
        message(FATAL_ERROR "Archivo requerido no encontrado: ${src}")
    endif()
endforeach()

foreach(src ${OPTIONAL_SOURCES})
    if(EXISTS ${src})
        list(APPEND SOURCES ${src})
        message(STATUS "Archivo opcional encontrado: ${src}")
    else()
        message(STATUS "Archivo opcional no encontrado: ${src}")
    endif()
endforeach()

# Headers requeridos
set(REQUIRED_HEADERS
    ${INCLUDE_DIR}/tag.h
    ${INCLUDE_DIR}/common.h
    ${INCLUDE_DIR}/tag_manager.h
)

foreach(hdr ${REQUIRED_HEADERS})
    if(NOT EXISTS ${hdr})
        message(WARNING "Header requerido no encontrado: ${hdr}")
    endif()
endforeach()

# Mostrar información de configuración
message(STATUS "")
message(STATUS "=== PlantaGas OPC-UA Server Build Configuration ===")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Source Directory: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Binary Directory: ${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  open62541: ${OPEN62541_VERSION}")
if(HTTPLIB_INCLUDE_DIR)
    message(STATUS "  httplib: ${HTTPLIB_INCLUDE_DIR}")
endif()
if(nlohmann_json_FOUND)
    message(STATUS "  nlohmann_json: Found")
endif()
message(STATUS "  CURL: ${CURL_VERSION_STRING}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  OPC-UA Server: Enabled")
if(ENABLE_HTTP_API)
    message(STATUS "  HTTP API: Enabled")
else()
    message(STATUS "  HTTP API: Disabled (httplib not found)")
endif()
message(STATUS "  TBL_OPCUA Optimization: Enabled")
message(STATUS "  Hot Reload: Enabled")
message(STATUS "  Backup System: Enabled")
message(STATUS "")
message(STATUS "Build targets:")
message(STATUS "  make planta_gas  - Build main executable")
message(STATUS "  make install       - Install system")
message(STATUS "  make package       - Create package")
message(STATUS "  make test          - Run tests")
message(STATUS "  make validate-config - Validate JSON config")
message(STATUS "  make clean-logs    - Clean log files")
message(STATUS "=================================================")

# Crear directorio include si no existe
file(MAKE_DIRECTORY ${INCLUDE_DIR})

# Verificar si tenemos al menos un archivo fuente
if(NOT SOURCES)
    message(FATAL_ERROR "No se encontraron archivos fuente para compilar")
endif()

# Crear ejecutable
add_executable(planta_gas ${SOURCES})

# Directorios de includes
target_include_directories(planta_gas PRIVATE
    ${INCLUDE_DIR}
    ${OPEN62541_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

if(HTTPLIB_INCLUDE_DIR)
    target_include_directories(planta_gas PRIVATE ${HTTPLIB_INCLUDE_DIR})
endif()

# Librerías
target_link_libraries(planta_gas
    ${OPEN62541_LIBRARIES}
    ${CURL_LIBRARIES}
    Threads::Threads
)

# Flags de compilación
target_compile_options(planta_gas PRIVATE
    ${OPEN62541_CFLAGS_OTHER}
    -Wall -Wextra -Wno-unused-parameter
)

# nlohmann_json
if(nlohmann_json_FOUND)
    target_link_libraries(planta_gas nlohmann_json::nlohmann_json)
else()
    # Asumir que está en los headers del sistema
    target_compile_definitions(planta_gas PRIVATE -DUSE_SYSTEM_JSON)
endif()

# Definiciones de preprocesador
if(ENABLE_HTTP_API)
    target_compile_definitions(planta_gas PRIVATE ENABLE_HTTP_API)
endif()

target_compile_definitions(planta_gas PRIVATE 
    PROJECT_VERSION="${PROJECT_VERSION}"
    TBL_OPCUA_OPTIMIZATION
)

# Configuración Debug/Release
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(planta_gas PRIVATE DEBUG_MODE)
    target_compile_options(planta_gas PRIVATE -g -O0)
else()
    target_compile_options(planta_gas PRIVATE -O3 -DNDEBUG)
endif()

# Targets personalizados
add_custom_target(validate-config
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/planta_gas --validate-config
    DEPENDS planta_gas
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Validating JSON configuration"
)

add_custom_target(clean-logs
    COMMAND rm -rf logs/*.log
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Cleaning log files"
)

add_custom_target(test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/planta_gas --test
    DEPENDS planta_gas
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running system tests"
)

# Instalación
install(TARGETS planta_gas 
    RUNTIME DESTINATION bin
)

install(FILES config/tags_planta_gas.json
    DESTINATION etc/planta_gas
    OPTIONAL
)

install(DIRECTORY ${INCLUDE_DIR}/
    DESTINATION include/planta_gas
    FILES_MATCHING PATTERN "*.h"
)

# Crear directorios necesarios
install(DIRECTORY DESTINATION var/log/planta_gas)
install(DIRECTORY DESTINATION etc/planta_gas)

# Package configuration
set(CPACK_PACKAGE_NAME "planta_gas")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "Servidor OPC-UA para Planta de Gas Industrial")
set(CPACK_PACKAGE_CONTACT "Industrial Solutions")
set(CPACK_GENERATOR "TGZ;DEB")

# DEB package
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libopen62541-1, libcurl4, nlohmann-json3-dev")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

include(CPack)
