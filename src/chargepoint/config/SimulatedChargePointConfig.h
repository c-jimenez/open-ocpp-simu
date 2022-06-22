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

#ifndef SIMULATEDCHARGEPOINTCONFIG_H
#define SIMULATEDCHARGEPOINTCONFIG_H

#include "ChargePointConfig.h"
#include "MqttConfig.h"
#include "OcppConfig.h"

#include <IniFile.h>

/** @brief Configuration of the simulated charge point */
class SimulatedChargePointConfig
{
  public:
    /** @brief Constructor */
    SimulatedChargePointConfig(const std::string& working_dir, const std::string& config_file)
        : m_working_dir(working_dir), m_config(config_file), m_stack_config(m_config), m_ocpp_config(m_config), m_mqtt_config(m_config)
    {
    }

    /** @brief Working directory */
    const std::string& workingDir() const { return m_working_dir; }

    /** @brief Stack internal configuration */
    ocpp::config::IChargePointConfig& stackConfig() { return m_stack_config; }

    /** @brief Standard OCPP configuration */
    ocpp::config::IOcppConfig& ocppConfig() { return m_ocpp_config; }

    /** @brief MQTT configuration */
    MqttConfig& mqttConfig() { return m_mqtt_config; }

    /** @brief Set the value of a stack internal configuration key */
    void setStackConfigValue(const std::string& key, const std::string& value) { m_stack_config.setConfigValue(key, value); }

    /** @brief Set the value of an OCPP configuration key */
    void setOcppConfigValue(const std::string& key, const std::string& value) { m_ocpp_config.setConfigValue(key, value); }

    /** @brief Set the value of a MQTT configuration key */
    void setMqttConfigValue(const std::string& key, const std::string& value) { m_mqtt_config.setConfigValue(key, value); }

  private:
    /** @brief Working directory */
    std::string m_working_dir;
    /** @brief Configuration file */
    ocpp::helpers::IniFile m_config;

    /** @brief Stack internal configuration */
    ChargePointConfig m_stack_config;
    /** @brief Standard OCPP configuration */
    OcppConfig m_ocpp_config;
    /** @brief MQTT configuration */
    MqttConfig m_mqtt_config;
};

#endif // SIMULATEDCHARGEPOINTCONFIG_H
