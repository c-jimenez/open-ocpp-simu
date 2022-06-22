#################################################################################
#                      OCPP charge points simulator                             #
#################################################################################

cmake_minimum_required(VERSION 3.13)

project(OCPPChargePointsSimulator DESCRIPTION "OCPP Charge Points simulator" VERSION 1.0.0)

# Build options
include(CMakeLists_Options.txt)

# Target toolchain definition and specific compiler flags
if(NOT DEFINED TARGET)
    set(TARGET native)
endif()
include(CMakeLists_${TARGET}.txt)

# 3rd party
add_subdirectory(3rdparty)

# Paho MQTT is mandatory
find_package(eclipse-paho-mqtt-c REQUIRED)

# Open OCPP is mandatory
find_package(PkgConfig)
pkg_search_module(PKG_OPEN_OCPP REQUIRED IMPORTED_TARGET libopen-ocpp)


# Default output directories
if(NOT DEFINED BIN_DIR)
    set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin/${TARGET})
endif()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BIN_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BIN_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

# Sources
add_subdirectory(src)
