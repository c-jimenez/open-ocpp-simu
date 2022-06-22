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

#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include "IMqttClient.h"
#include "json.h"

#include <filesystem>
#include <map>

/** @brief Handler for incoming MQTT events */
class CommandHandler : public IMqttClient::IListener
{
  public:
    /** @brief Constructor */
    CommandHandler(IMqttClient& client, const std::string broker_url, std::filesystem::path chargepoints_dir);

    /** @brief Destructor */
    virtual ~CommandHandler();

    /** @copydoc void IMqttClient::IListener::mqttConnectionLost() */
    void mqttConnectionLost() override;

    /** @copydoc void IMqttClient::IListener::mqttMessageReceived(const char*, const std::string&, IMqttClient::QoS, bool) */
    void mqttMessageReceived(const char* topic, const std::string& message, IMqttClient::QoS qos, bool retained) override;

    /** @brief Indicate that an end of application command has been received */
    bool isEndOfApplication() const { return m_end; }

    /** @brief Start simulated charge points */
    bool startChargePoints(const rapidjson::Value& charge_points, bool clean_env);

    /** @brief Kill simulated charge points */
    bool killChargePoints(const rapidjson::Value& charge_points);

  private:
    /** @brief MQTT client */
    IMqttClient& m_client;
    /** @brief URL of the broker */
    const std::string m_broker_url;
    /** @brief Directory to store charge points data */
    const std::filesystem::path m_chargepoints_dir;
    /** @brief Indicate that an end of application command has been received */
    bool m_end;
    /** @brief Simulated charge points' statuses */
    std::map<std::string, bool> m_cp_status;
    /** @brief Simulated charge points' pids */
    std::map<std::string, int> m_cp_pids;
};

#endif // COMMANDHANDLER_H
