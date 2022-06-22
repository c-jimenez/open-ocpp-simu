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
target_link_libraries(chargepoint 
    mqtt_client
    PkgConfig::PKG_OPEN_OCPP
    pthread
)

# Copy to binary directory
ADD_CUSTOM_COMMAND(TARGET chargepoint
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/config/config.ini ${BIN_DIR}/
)


