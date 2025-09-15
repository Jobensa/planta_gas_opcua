cmake_minimum_required(VERSION 3.16)
project(planta_gas)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configuración de build
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Dependencias
find_package(PkgConfig REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)

# open62541
pkg_check_modules(OPEN62541 REQUIRED open62541)

# Headers
include_directories(include)
include_directories(${OPEN62541_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/tag_manager.cpp
    src/opcua_server.cpp
    src/pac_control_client.cpp
)

# Executable
add_executable(planta_gas ${SOURCES})

# Linking
target_link_libraries(planta_gas
    ${OPEN62541_LIBRARIES}
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
    pthread
)

# Install
install(TARGETS planta_gas DESTINATION bin)
