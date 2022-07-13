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
#include "IMqttClient.h"
#include "Topics.h"
#include "json.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

/** @brief Entry point */
int main(int argc, char* argv[])
{
    // Default parameters
    std::string working_dir       = "";
    std::string broker_url        = "tcp://localhost:1883";
    std::string config_file       = "";
    bool        reset_working_dir = false;

    // Check parameters
    if (argc > 1)
    {
        const char* param     = nullptr;
        bool        bad_param = false;
        argv++;
        while ((argc != 1) && !bad_param)
        {
            if (strcmp(*argv, "-h") == 0)
            {
                bad_param = true;
            }
            else if ((strcmp(*argv, "-w") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                working_dir = *argv;
            }
            else if ((strcmp(*argv, "-b") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                broker_url = *argv;
            }
            else if ((strcmp(*argv, "-c") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                config_file = *argv;
            }
            else if (strcmp(*argv, "-r") == 0)
            {
                reset_working_dir = true;
            }
            else
            {
                param     = *argv;
                bad_param = true;
            }

            // Next param
            argc--;
            argv++;
        }
        if (bad_param)
        {
            if (param)
            {
                std::cout << "Invalid parameter : " << param << std::endl;
            }
            std::cout << "Usage : launcher [-w working_dir] [-b broker_url] [-c config_file] [-r]" << std::endl;
            std::cout << "    -w : Working directory where to store the charge point persistent data (Default = current directory)"
                      << std::endl;
            std::cout << "    -b : Url of the MQTT broker (Default = tcp://localhost:1883)" << std::endl;
            std::cout << "    -c : Configuration file (Default = none)" << std::endl;
            std::cout << "    -r : Reset working directory (Default = False)" << std::endl;
            return 1;
        }
    }

    std::cout << "OCPP charge point simulator launcher" << std::endl;

    // Cleanup existing charge point directory
    std::filesystem::path chargepoint_dir(working_dir);
    chargepoint_dir /= "chargepoints";
    if (reset_working_dir)
    {
        std::filesystem::remove_all(chargepoint_dir);
    }

    // Creating charge point directory
    if (!std::filesystem::exists(chargepoint_dir) && !std::filesystem::create_directories(chargepoint_dir))
    {
        std::cout << "Error : could not create charge point directory" << std::endl;
        return 1;
    }

    // MQTT client
    IMqttClient* mqtt = IMqttClient::create("OCPP charge point simulator launcher");

    // Command handler
    CommandHandler cmd_handler(broker_url, chargepoint_dir);
    mqtt->registerListener(cmd_handler);

    // Configuration file
    if (!config_file.empty())
    {
        std::string  config_data;
        std::fstream file(config_file, file.in | file.binary | file.ate);
        if (file.is_open())
        {
            // Read the whole file
            auto filesize = file.tellg();
            file.seekg(0, file.beg);
            config_data.resize(filesize);
            file.read(&config_data[0], filesize);

            // Parse JSON data
            bool                valid = false;
            rapidjson::Document json_config;
            try
            {
                json_config.Parse(config_data.c_str(), config_data.size());
                valid = !json_config.HasParseError();
            }
            catch (...)
            {
            }
            if (valid)
            {
                // Look for charge points list
                if (json_config.HasMember("charge_points") && json_config["charge_points"].IsArray())
                {
                    // Start charge points
                    if (!cmd_handler.startChargePoints(json_config["charge_points"], true))
                    {
                        std::cout << "Warning : unable to start all the defined charge points" << std::endl;
                    }
                }
                else
                {
                    valid = false;
                }
            }
            if (!valid)
            {
                std::cout << "Error : invalid configuration file" << std::endl;
                return 1;
            }
        }
        else
        {
            std::cout << "Error : unable to load configuration file" << std::endl;
            return 1;
        }
    }

    // Set the will message
    mqtt->setWill(LAUNCHER_STATUS_TOPIC, "Dead", IMqttClient::QoS::QOS_0, true);

    // Connection loop
    do
    {
        // Connection to the broker
        std::cout << "Connecting to the broker (" << broker_url << ")..." << std::endl;
        if (mqtt->connect(broker_url))
        {
            std::cout << "Subscribing to simulated charge point's topics..." << std::endl;
            if (mqtt->subscribe(CHARGE_POINTS_TOPIC "+/status"))
            {
                std::cout << "Subscribing to launcher's command topic..." << std::endl;
                if (mqtt->subscribe(LAUNCHER_CMD_TOPIC))
                {
                    // Set the status message
                    mqtt->publish(LAUNCHER_STATUS_TOPIC, "Alive", IMqttClient::QoS::QOS_0, true);

                    // Wait for disconnection or end of application
                    std::cout << "Ready!" << std::endl;
                    while (!cmd_handler.isEndOfApplication() && mqtt->isConnected())
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }
                    if (!mqtt->isConnected())
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
        if (!cmd_handler.isEndOfApplication())
        {
            mqtt->close();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        else
        {
            // Update the status message
            mqtt->publish(LAUNCHER_STATUS_TOPIC, "Dead", IMqttClient::QoS::QOS_0, true);
        }

    } while (!cmd_handler.isEndOfApplication());

    // Release resources
    delete mqtt;

    return 0;
}
