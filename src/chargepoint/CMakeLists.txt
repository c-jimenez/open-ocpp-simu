######################################################
#              Charge point simulator                #
######################################################

# Common includes
include_directories(. ../common)

# Executable target
add_executable(chargepoint
    main.cpp
    MeterSimulator.cpp
    SimulatedChargePoint.cpp
    mqtt/MqttManager.cpp
    ocpp/ChargePointEventsHandler.cpp
    ocpp/OcppConfig.cpp
)
include_directories(config mqtt ocpp)

# Additionnal libraries path
target_link_directories(chargepoint PRIVATE ${BIN_DIR})

# Dependencies
if (NOT MSVC)
    set(OPENOCPP_SIMU_CHARGEPOINT_LIBS pthread)
else()
    set(OPENOCPP_SIMU_CHARGEPOINT_LIBS websockets_static.lib sqlite3 OpenSSL::SSL OpenSSL::Crypto Ws2_32 Crypt32)
endif()
target_link_libraries(chargepoint 
        mqtt_client
        ${OPENOCPP_LIB}
        ${OPENOCPP_SIMU_CHARGEPOINT_LIBS}
    )

# Copy to binary directory
ADD_CUSTOM_COMMAND(TARGET chargepoint
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/config/config.ini ${BIN_DIR}/
)
if (MSVC)
ADD_CUSTOM_COMMAND(TARGET chargepoint
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/3rdparty/eclipse-paho-mqtt-c/lib/paho-mqtt3cs.dll ${BIN_DIR}/
)
endif()
