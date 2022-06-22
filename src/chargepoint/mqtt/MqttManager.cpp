/*
MIT License

Copyright (c) 2022 Cedric Jimenez

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "MqttManager.h"
#include "SimulatedChargePointConfig.h"
#include "Topics.h"
#include "json.h"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <thread>
#include <unistd.h>

/** @brief Constructor */
MqttManager::MqttManager(SimulatedChargePointConfig& config)
    : m_config(config),
      m_mutex(),
      m_end(false),
      m_connectors(config.ocppConfig().numberOfConnectors()),
      m_mqtt(nullptr),
      m_status_topic(),
      m_connectors_topic()
{
}

/** @brief Destructor */
MqttManager::~MqttManager() { }

/** @copydoc void IMqttClient::IListener::mqttConnectionLost() */
void MqttManager::mqttConnectionLost() { }

/** @copydoc void IMqttClient::IListener::mqttMessageReceived(const char*, const std::string&, IMqttClient::QoS, bool) */
void MqttManager::mqttMessageReceived(const char* topic, const std::string& message, IMqttClient::QoS qos, bool retained)
{
    (void)qos;
    (void)retained;

    // Decode message
    bool                valid = false;
    rapidjson::Document payload;
    try
    {
        payload.Parse(message.c_str(), message.size());
        valid = !payload.HasParseError();
    }
    catch (...)
    {
    }
    if (!valid)
    {
        std::cout << "Invalid message : " << message << std::endl;
    }
    else
    {
        // Split topic name
        std::filesystem::path topic_path(topic);

        // Check message type
        if (topic_path.filename().compare("cmd") == 0)
        {
            // Command
            if (payload.HasMember("type"))
            {
                const char* type = payload["type"].GetString();
                if (strcmp(type, "close") == 0)
                {
                    std::cout << "Close command received" << std::endl;
                    m_end = true;
                }
            }
            else
            {
                std::cout << "Unknown command : " << message << std::endl;
            }
        }
        else
        {
            // Get connector number
            auto iter = topic_path.end();
            iter--;
            iter--;
            std::string  connector_str = *iter;
            unsigned int connector     = static_cast<unsigned int>(std::atoi(connector_str.c_str()));
            if ((connector > 0) && (connector <= m_connectors.size()))
            {
                // Update connector data
                ConnectorData&              connector_data = m_connectors[connector - 1u];
                std::lock_guard<std::mutex> lock(m_mutex);

                if (topic_path.filename().compare("car") == 0)
                {
                    if (payload.HasMember("cable"))
                    {
                        rapidjson::Value& cable = payload["cable"];
                        if (cable.IsFloat())
                        {
                            connector_data.car_cable_capacity = cable.GetFloat();
                        }
                    }
                    if (payload.HasMember("ready"))
                    {
                        rapidjson::Value& ready = payload["ready"];
                        if (ready.IsBool())
                        {
                            connector_data.car_ready = ready.GetBool();
                        }
                    }
                    if (payload.HasMember("consumption"))
                    {
                        rapidjson::Value& consumption = payload["consumption"];
                        if (consumption.IsFloat())
                        {
                            connector_data.car_consumption = consumption.GetFloat();
                        }
                    }
                }
                else
                {
                    if (payload.HasMember("id"))
                    {
                        rapidjson::Value& id = payload["id"];
                        if (id.IsString())
                        {
                            connector_data.id_tag = id.GetString();
                        }
                    }
                }
            }
            else
            {
                std::cout << "Invalid connector : " << connector_str << std::endl;
            }
        }
    }
}

/** @brief Start the MQTT connection process (blocking) */
void MqttManager::start(unsigned int nb_phases, unsigned int max_charge_point_current)
{
    // Compute topics path
    std::string chargepoint_topic(CHARGE_POINTS_TOPIC);
    chargepoint_topic += m_config.stackConfig().chargePointIdentifier() + "/";

    std::string chargepoint_cmd_topic  = chargepoint_topic + "cmd";
    std::string chargepoint_car_topics = chargepoint_topic + "connectors/+/car";
    std::string chargepoint_tag_topics = chargepoint_topic + "connectors/+/id_tag";
    m_status_topic                     = chargepoint_topic + "status";
    m_connectors_topic                 = chargepoint_topic + "connectors/";

    // MQTT client
    m_mqtt = IMqttClient::create(m_config.stackConfig().chargePointIdentifier());
    m_mqtt->registerListener(*this);

    // Set the will message
    m_mqtt->setWill(
        m_status_topic, buildStatusMessage("Dead", nb_phases, static_cast<float>(max_charge_point_current)), IMqttClient::QoS::QOS_0, true);

    // Connection loop
    do
    {
        // Connection to the broker
        std::cout << "Connecting to the broker (" << m_config.mqttConfig().brokerUrl() << ")..." << std::endl;
        if (m_mqtt->connect(m_config.mqttConfig().brokerUrl()))
        {
            std::cout << "Subscribing to charge point's command topic..." << std::endl;
            if (m_mqtt->subscribe(chargepoint_cmd_topic))
            {
                std::cout << "Subscribing to charge point's connector topics..." << std::endl;
                if (m_mqtt->subscribe(chargepoint_car_topics) && m_mqtt->subscribe(chargepoint_tag_topics))
                {
                    // Wait for disconnection or end of application
                    std::cout << "Ready!" << std::endl;
                    while (!m_end && m_mqtt->isConnected())
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }
                    if (!m_mqtt->isConnected())
                    {
                        std::cout << "Disconnected, next retry in 5s..." << std::endl;
                    }
                }
                else
                {
                    std::cout << "Couldn't subscribe, next retry in 5s..." << std::endl;
                }
            }
            else
            {
                std::cout << "Couldn't subscribe, next retry in 5s..." << std::endl;
            }
        }
        else
        {
            std::cout << "Couldn't connect to the broker, next retry in 5s..." << std::endl;
        }

        // Delay before retry
        if (!m_end)
        {
            m_mqtt->close();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        else
        {
            // Update the status message
            m_mqtt->publish(m_status_topic,
                            buildStatusMessage("Dead", nb_phases, static_cast<float>(max_charge_point_current)),
                            IMqttClient::QoS::QOS_0,
                            true);
        }
    } while (!m_end);

    // Release resources
    delete m_mqtt;
}

/** @brief Indicate a pending Id tag */
bool MqttManager::isIdTagPending(unsigned int connector_id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_connectors[connector_id - 1u].id_tag.empty();
}

/** @brief Reset the pending Id tag */
void MqttManager::resetIdTagPending(unsigned int connector_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connectors[connector_id - 1u].id_tag = "";
}

/** @brief Pending Id tag for the remote start request */
const std::string& MqttManager::pendingIdTag(unsigned int connector_id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connectors[connector_id - 1u].id_tag;
}

/** @brief Update the data of a connector */
void MqttManager::updateData(std::vector<ConnectorData>& connectors) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (ConnectorData& connector : connectors)
    {
        const ConnectorData& mqtt_data = m_connectors[connector.id - 1u];
        connector.car_cable_capacity   = mqtt_data.car_cable_capacity;
        connector.car_ready            = mqtt_data.car_ready;
        connector.car_consumption      = mqtt_data.car_consumption;
        connector.fault_pending        = mqtt_data.fault_pending;
    }
}

/** @brief Publish the status of the charge point */
bool MqttManager::publishStatus(const std::string& status, unsigned int nb_phases, float max_setpoint)
{
    bool ret = false;

    // Check connectivity
    if (m_mqtt->isConnected())
    {
        // Publish
        ret = m_mqtt->publish(m_status_topic, buildStatusMessage(status.c_str(), nb_phases, max_setpoint), IMqttClient::QoS::QOS_0, true);
    }

    return ret;
}

/** @brief Publish the data of the connectors */
void MqttManager::publishData(const std::vector<ConnectorData>& connectors)
{
    // Check connectivity
    if (m_mqtt->isConnected())
    {
        // Publish for each connector
        for (const ConnectorData& connector : connectors)
        {
            // Compute topic name
            std::stringstream topic;
            topic << m_connectors_topic << connector.id << "/status";

            // Create the JSON message
            rapidjson::Document msg;
            msg.Parse("{}");
            msg.AddMember(
                rapidjson::StringRef("status"),
                rapidjson::Value(ocpp::types::ChargePointStatusHelper.toString(connector.status).c_str(), msg.GetAllocator()).Move(),
                msg.GetAllocator());
            msg.AddMember(
                rapidjson::StringRef("id_tag"), rapidjson::Value(connector.id_tag.c_str(), msg.GetAllocator()).Move(), msg.GetAllocator());
            msg.AddMember(rapidjson::StringRef("max_setpoint"), rapidjson::Value(connector.max_setpoint), msg.GetAllocator());
            msg.AddMember(rapidjson::StringRef("ocpp_setpoint"), rapidjson::Value(connector.ocpp_setpoint), msg.GetAllocator());
            msg.AddMember(rapidjson::StringRef("setpoint"), rapidjson::Value(connector.setpoint), msg.GetAllocator());
            msg.AddMember(rapidjson::StringRef("consumption"), rapidjson::Value(connector.consumption), msg.GetAllocator());
            msg.AddMember(rapidjson::StringRef("car_consumption"), rapidjson::Value(connector.car_consumption), msg.GetAllocator());
            msg.AddMember(rapidjson::StringRef("car_cable_capacity"), rapidjson::Value(connector.car_cable_capacity), msg.GetAllocator());
            msg.AddMember(rapidjson::StringRef("car_ready"), rapidjson::Value(connector.car_ready), msg.GetAllocator());

            rapidjson::StringBuffer                    buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            msg.Accept(writer);

            // Publish
            m_mqtt->publish(topic.str(), buffer.GetString(), IMqttClient::QoS::QOS_0, true);
        }
    }
}

/** @brief Build the status message of the charge point */
std::string MqttManager::buildStatusMessage(const char* status, unsigned int nb_phases, float max_setpoint)
{
    rapidjson::Document msg;
    msg.Parse("{}");
    msg.AddMember(rapidjson::StringRef("pid"), rapidjson::Value(getpid()), msg.GetAllocator());
    msg.AddMember(rapidjson::StringRef("status"), rapidjson::Value(status, msg.GetAllocator()).Move(), msg.GetAllocator());
    msg.AddMember(rapidjson::StringRef("vendor"),
                  rapidjson::Value(m_config.stackConfig().chargePointVendor().c_str(), msg.GetAllocator()).Move(),
                  msg.GetAllocator());
    msg.AddMember(rapidjson::StringRef("model"),
                  rapidjson::Value(m_config.stackConfig().chargePointModel().c_str(), msg.GetAllocator()).Move(),
                  msg.GetAllocator());
    msg.AddMember(rapidjson::StringRef("serial"),
                  rapidjson::Value(m_config.stackConfig().chargePointSerialNumber().c_str(), msg.GetAllocator()).Move(),
                  msg.GetAllocator());
    msg.AddMember(rapidjson::StringRef("nb_phases"), rapidjson::Value(nb_phases), msg.GetAllocator());
    msg.AddMember(rapidjson::StringRef("max_setpoint"), rapidjson::Value(max_setpoint), msg.GetAllocator());
    msg.AddMember(rapidjson::StringRef("central_system"),
                  rapidjson::Value(m_config.stackConfig().connexionUrl().c_str(), msg.GetAllocator()).Move(),
                  msg.GetAllocator());

    rapidjson::StringBuffer                    buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    msg.Accept(writer);

    return buffer.GetString();
}