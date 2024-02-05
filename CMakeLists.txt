#################################################################################
#                      OCPP charge points simulator                             #
#################################################################################

cmake_minimum_required(VERSION 3.13)

project(OCPPChargePointsSimulator DESCRIPTION "OCPP Charge Points simulator" VERSION 1.2.0)

# Build options
include(CMakeLists_Options.txt)

# Target toolchain definition and specific compiler flags
if(NOT DEFINED TARGET)
    if (NOT MSVC)
        set(TARGET "gcc")
    else()
        set(TARGET "msvc")
    endif()
endif()
include(CMakeLists_${TARGET}.txt)

# 3rd party
add_subdirectory(3rdparty)

# Paho MQTT is mandatory
if(NOT MSVC)
    find_package(eclipse-paho-mqtt-c REQUIRED)
else()
    include_directories(3rdparty/eclipse-paho-mqtt-c/include)
    link_directories(3rdparty/eclipse-paho-mqtt-c/lib)

    find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)
endif()

# Open OCPP is mandatory
if(NOT MSVC)
    find_package(PkgConfig)
    pkg_search_module(PKG_OPEN_OCPP REQUIRED IMPORTED_TARGET libopen-ocpp)
    set(OPENOCPP_LIB PkgConfig::PKG_OPEN_OCPP)
else()
    set(OPENOCPP_INCLUDE_DIR $ENV{OPENOCPP_INCLUDE_DIR})
    set(OPENOCPP_LIB_DIR $ENV{OPENOCPP_INCLUDE_DIR})
    message("OPENOCPP_INCLUDE_DIR : ${OPENOCPP_INCLUDE_DIR}")
    message("OPENOCPP_LIB_DIR : ${OPENOCPP_LIB_DIR}")
    include_directories(${OPENOCPP_INCLUDE_DIR})
    link_directories($ENV{OPENOCPP_LIB_DIR})
    set(OPENOCPP_LIB open-ocpp_static.lib)
endif()


# Default output directories
if(NOT DEFINED BIN_DIR)
    set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin/${TARGET})
endif()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BIN_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BIN_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

# Sources
add_subdirectory(src)
