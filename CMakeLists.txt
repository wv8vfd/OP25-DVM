cmake_minimum_required(VERSION 3.14)
project(op25-gateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Threads REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(OpenSSL REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/Config.cpp
    src/Logger.cpp
    src/P25Utils.cpp
    src/OP25Receiver.cpp
    src/FNEClient.cpp
    src/CallManager.cpp
)

# Create executable
add_executable(op25-gateway ${SOURCES})

# Include directories
target_include_directories(op25-gateway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(op25-gateway PRIVATE
    Threads::Threads
    yaml-cpp
    OpenSSL::Crypto
)

# Install target
install(TARGETS op25-gateway DESTINATION bin)
install(FILES config.yml DESTINATION etc/op25-gateway)
