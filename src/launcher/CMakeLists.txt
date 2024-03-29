######################################################
#       OCPP charge point simulator launcher         #
######################################################

# Common includes
include_directories(../common)

# Executable target
add_executable(launcher
    main.cpp
    CommandHandler.cpp
)

# Additionnal libraries path
target_link_directories(launcher PRIVATE ${BIN_DIR})

# Dependencies
target_link_libraries(launcher 
    mqtt_client
    ${OPENOCPP_LIB}
)

# Copy to binary directory
ADD_CUSTOM_COMMAND(TARGET launcher
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/config/env_setup.json ${BIN_DIR}/
)
if (MSVC)
ADD_CUSTOM_COMMAND(TARGET launcher
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/3rdparty/eclipse-paho-mqtt-c/lib/paho-mqtt3cs.dll ${BIN_DIR}/
)
endif()
