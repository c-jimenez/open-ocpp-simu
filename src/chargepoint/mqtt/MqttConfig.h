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

#ifndef MQTTCONFIG_H
#define MQTTCONFIG_H

#include <openocpp/IniFile.h>

/** @brief Section name for the parameters */
static const std::string MQTT_PARAMS = "Mqtt";

/** @brief Charge Point MQTT configuration */
class MqttConfig
{
  public:
    /** @brief Constructor */
    MqttConfig(ocpp::helpers::IniFile& config) : m_config(config) { }

    /** @brief Set the value of a MQTT configuration key */
    void setConfigValue(const std::string& key, const std::string& value) { m_config.set(MQTT_PARAMS, key, value); }

    // Communication parameters

    /** @brief Broker URL */
    std::string brokerUrl() const { return getString("BrokerUrl"); };

  private:
    /** @brief Configuration file */
    ocpp::helpers::IniFile& m_config;

    /** @brief Get a boolean parameter */
    bool getBool(const std::string& param) const { return m_config.get(MQTT_PARAMS, param).toBool(); }
    /** @brief Get a floating point parameter */
    double getFloat(const std::string& param) const { return m_config.get(MQTT_PARAMS, param).toFloat(); }
    /** @brief Get a string parameter */
    std::string getString(const std::string& param) const { return m_config.get(MQTT_PARAMS, param); }
    /** @brief Get a value which can be created from an unsigned integer */
    template <typename T>
    T get(const std::string& param) const
    {
        return T(m_config.get(MQTT_PARAMS, param).toUInt());
    }
};

#endif // MQTTCONFIG_H
