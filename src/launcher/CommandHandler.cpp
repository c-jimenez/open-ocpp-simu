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

#include "CommandHandler.h"
#include "Topics.h"

#include <IniFile.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <signal.h>
#include <sstream>

/** @brief Constructor */
CommandHandler::CommandHandler(const std::string broker_url, std::filesystem::path chargepoints_dir)
    : m_broker_url(broker_url), m_chargepoints_dir(chargepoints_dir), m_end(false), m_cp_status(), m_cp_pids()
{
}

/** @brief Destructor */
CommandHandler::~CommandHandler() { }

/** @copydoc void IMqttClient::IListener::mqttConnectionLost() */
void CommandHandler::mqttConnectionLost() { }

/** @copydoc void IMqttClient::IListener::mqttMessageReceived(const char*, const std::string&, IMqttClient::QoS, bool) */
void CommandHandler::mqttMessageReceived(const char* topic, const std::string& message, IMqttClient::QoS qos, bool retained)
{
    (void)qos;
    (void)retained;

    // Decode message
    bool                valid = false;
    rapidjson::Document payload;
    try
    {
        if (!message.empty())
        {
            payload.Parse(message.c_str(), message.size());
            valid = !payload.HasParseError();
        }
        else
        {
            valid = true;
        }
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
            if (!message.empty() && payload.HasMember("type"))
            {
                const char* type = payload["type"].GetString();
                if (strcmp(type, "close") == 0)
                {
                    std::cout << "Close command received" << std::endl;
                    m_end = true;
                }
                else if (strcmp(type, "start") == 0)
                {
                    if (payload.HasMember("charge_points"))
                    {
                        rapidjson::Value& charge_points = payload["charge_points"];
                        if (charge_points.IsArray())
                        {
                            startChargePoints(charge_points, true);
                        }
                    }
                }
                else if (strcmp(type, "kill") == 0)
                {
                    if (payload.HasMember("charge_points"))
                    {
                        rapidjson::Value& charge_points = payload["charge_points"];
                        if (charge_points.IsArray())
                        {
                            killChargePoints(charge_points);
                        }
                    }
                }
                else if (strcmp(type, "restart") == 0)
                {
                    if (payload.HasMember("charge_points"))
                    {
                        rapidjson::Value& charge_points = payload["charge_points"];
                        if (charge_points.IsArray())
                        {
                            startChargePoints(charge_points, false);
                        }
                    }
                }
                else
                {
                    std::cout << "Unknown command : " << type << std::endl;
                }
            }
        }
        else
        {
            // Charge point's status

            // Extract name
            std::filesystem::path topic_path(topic);
            auto                  iter = topic_path.end();
            iter--;
            iter--;
            std::string charge_point = *iter;

            // Extract status
            if (!message.empty() && payload.HasMember("status"))
            {
                // Save status
                m_cp_status[charge_point] = (strcmp("Dead", payload["status"].GetString()) != 0);
                if (m_cp_status[charge_point])
                {
                    m_cp_pids[charge_point] = payload["pid"].GetInt();
                }

                std::cout << "[" << charge_point << "] - " << message << std::endl;
            }
            else
            {
                // Check remove status
                if (message.empty())
                {
                    // Remove charge point
                    m_cp_status.erase(charge_point);
                    m_cp_pids.erase(charge_point);

                    // Clear working directory
                    std::filesystem::path chargepoint_dir(m_chargepoints_dir);
                    chargepoint_dir /= charge_point;
                    std::filesystem::remove_all(chargepoint_dir);

                    std::cout << "[" << charge_point << "] - Removed!" << std::endl;
                }
                else
                {
                    std::cout << "Invalid status : " << message << std::endl;
                }
            }
        }
    }
}

/** @brief Start simulated charge points */
bool CommandHandler::startChargePoints(const rapidjson::Value& charge_points, bool clean_env)
{
    unsigned int total_count   = 0;
    unsigned int total_started = 0;

    for (auto it_charge_point = charge_points.Begin(); it_charge_point != charge_points.End(); ++it_charge_point)
    {
        // Check charge point parameters
        const rapidjson::Value& charge_point = *it_charge_point;
        if (charge_point.HasMember("id") && charge_point.HasMember("vendor") && charge_point.HasMember("model") &&
            charge_point.HasMember("serial") && charge_point.HasMember("max_current") && charge_point.HasMember("nb_connectors") &&
            charge_point.HasMember("max_current_per_connector") && charge_point.HasMember("nb_phases") &&
            charge_point.HasMember("central_system"))
        {
            // Extract charge point parameters
            std::string  id                        = charge_point["id"].GetString();
            std::string  vendor                    = charge_point["vendor"].GetString();
            std::string  model                     = charge_point["model"].GetString();
            std::string  serial                    = charge_point["serial"].GetString();
            std::string  central_system            = charge_point["central_system"].GetString();
            unsigned int nb_connectors             = charge_point["nb_connectors"].GetUint();
            unsigned int nb_phases                 = charge_point["nb_phases"].GetUint();
            unsigned int max_current               = charge_point["max_current"].GetUint();
            unsigned int max_current_per_connector = charge_point["max_current_per_connector"].GetUint();

            // Check if charge point is already running
            if ((m_cp_status.find(id) == m_cp_status.end()) || !m_cp_status[id])
            {
                // Clean and (re)-create working directory
                std::filesystem::path chargepoint_dir(m_chargepoints_dir);
                chargepoint_dir /= id;
                if (clean_env)
                {
                    std::filesystem::remove_all(chargepoint_dir);
                    std::filesystem::create_directories(chargepoint_dir);
                }

                // Copy default configuration file
                std::filesystem::path config_path(chargepoint_dir);
                config_path /= "config.ini";
                if (clean_env)
                {
                    std::filesystem::copy("config.ini", config_path);
                }

                // Update configuration
                ocpp::helpers::IniFile config(config_path);
                config.set("ChargePoint", "ChargePointVendor", vendor);
                config.set("ChargePoint", "ChargePointModel", model);
                config.set("ChargePoint", "DatabasePath", (chargepoint_dir / "ocpp.db").c_str());

                // Build command line
                std::stringstream cmd;
                cmd << "./chargepoint";
                cmd << " -w \"" << chargepoint_dir << "\"";
                cmd << " -t " << central_system;
                cmd << " -c \"" << id << "\"";
                cmd << " -s \"" << serial << "\"";
                cmd << " -n " << nb_connectors;
                cmd << " -p " << nb_phases;
                cmd << " -b " << m_broker_url;
                cmd << " -m " << max_current;
                cmd << " -i " << max_current_per_connector;
                cmd << " &" << std::endl;

                // Start charge point
                system(cmd.str().c_str());
                total_started++;
            }
        }
        total_count++;
    }

    return (total_started == total_count);
}

/** @brief Kill simulated charge points */
bool CommandHandler::killChargePoints(const rapidjson::Value& charge_points)
{
    unsigned int total_count  = 0;
    unsigned int total_killed = 0;

    for (auto it_charge_point = charge_points.Begin(); it_charge_point != charge_points.End(); ++it_charge_point)
    {
        // Check charge point parameters
        const rapidjson::Value& charge_point = *it_charge_point;
        if (charge_point.HasMember("id"))
        {
            // Look for the corresponding charge point
            std::string id      = charge_point["id"].GetString();
            auto        iter_cp = m_cp_pids.find(id);
            if ((iter_cp != m_cp_pids.end()) && m_cp_status[id])
            {
                // Kill charge point
                int pid = iter_cp->second;
                int err = kill(pid, SIGKILL);
                if (err == 0)
                {
                    total_killed++;
                }
            }
        }
        total_count++;
    }

    return (total_killed == total_count);
}
