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

#ifndef MQTTMANAGER_H
#define MQTTMANAGER_H

#include "ConnectorData.h"
#include "IMqttClient.h"

#include <mutex>
#include <string>
#include <vector>

class SimulatedChargePointConfig;

/** @brief Manage MQTT connectivity */
class MqttManager : public IMqttClient::IListener
{
  public:
    /**
     * @brief Constructor
     * @param config Configuration
     */
    MqttManager(SimulatedChargePointConfig& config);

    /** @brief Destructor */
    virtual ~MqttManager();

    /** @copydoc void IMqttClient::IListener::mqttConnectionLost() */
    void mqttConnectionLost() override;

    /** @copydoc void IMqttClient::IListener::mqttMessageReceived(const char*, const std::string&, IMqttClient::QoS, bool) */
    void mqttMessageReceived(const char* topic, const std::string& message, IMqttClient::QoS qos, bool retained) override;

    /** @brief Indicate that an end of application command has been received */
    bool isEndOfApplication() const { return m_end; }

    /** @brief Start the MQTT connection process (blocking) */
    void start(unsigned int nb_phases, unsigned int max_charge_point_current);

    /** @brief Indicate a pending Id tag */
    bool isIdTagPending(unsigned int connector_id) const;

    /** @brief Reset the pending Id tag */
    void resetIdTagPending(unsigned int connector_id);

    /** @brief Pending Id tag for the remote start request */
    const std::string& pendingIdTag(unsigned int connector_id) const;

    /** @brief Update the data of the connectors */
    void updateData(std::vector<ConnectorData>& connectors) const;

    /** @brief Publish the status of the charge point */
    bool publishStatus(const std::string& status, unsigned int nb_phases, float max_setpoint);

    /** @brief Publish the data of the connectors */
    void publishData(const std::vector<ConnectorData>& connectors);

  private:
    /** @brief Configuration */
    SimulatedChargePointConfig& m_config;

    /** @brief Mutex to access connector data */
    mutable std::mutex m_mutex;
    /** @brief Indicate that an end of application command has been received */
    bool m_end;
    /** @brief Connector data */
    std::vector<ConnectorData> m_connectors;

    /** @brief MQTT client */
    IMqttClient* m_mqtt;
    /** @brief Status topic */
    std::string m_status_topic;
    /** @brief Connectors topic */
    std::string m_connectors_topic;

    /** @brief Build the status message of the charge point */
    std::string buildStatusMessage(const char* status, unsigned int nb_phases, float max_setpoint);
};

#endif // MQTTMANAGER_H
