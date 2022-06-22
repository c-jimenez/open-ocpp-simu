######################################################
#                MQTT client library                 #
######################################################

# Library target
add_library(mqtt_client
    private/PahoMqttClient.cpp
)

# Include directories
target_include_directories(mqtt_client PUBLIC .)

# Dependencies
target_link_libraries(mqtt_client
    paho-mqtt3cs
)

