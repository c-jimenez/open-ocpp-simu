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

#include "ChargePointEventsHandler.h"
#include "IMqttClient.h"
#include "SimulatedChargePoint.h"
#include "SimulatedChargePointConfig.h"

#include <openocpp/IChargePoint.h>

#include <chrono>
#include <iostream>
#include <set>
#include <string.h>
#include <thread>

using namespace std;

IMqttClient* client = nullptr;

class Listener : public IMqttClient::IListener
{
  public:
    /** @copydoc void IMqttClient::IListener::mqttConnectionLost() */
    void mqttConnectionLost() override
    {
        cout << "Communication lost!" << endl;
        client->close();
        if (client->connect("tcp://localhost:1883"))
        {
            cout << "Connected!" << endl;
        }
    }

    /** @copydoc void IMqttClient::IListener::mqttMessageReceived(const char*, const std::string&, IMqttClient::QoS, bool) */
    void mqttMessageReceived(const char* topic, const std::string& message, IMqttClient::QoS qos, bool retained) override
    {
        cout << "Rx message : [" << topic << "] - QoS" << static_cast<int>(qos) << " - " << (retained ? "" : "not ") << "retained - "
             << message << endl;
    }
};

/** @brief Entry point */
int main(int argc, char* argv[])
{
    // Default parameters
    std::string           working_dir               = "";
    std::string           connection_url            = "";
    std::string           chargepoint_id            = "";
    std::string           serial_number             = "";
    unsigned int          nb_connectors             = 1u;
    unsigned int          nb_phases                 = 3u;
    std::string           mqtt_broker_url           = "tcp://localhost:1883";
    unsigned int          max_charge_point_setpoint = 32u;
    unsigned int          max_connector_setpoint    = 32u;
    std::set<std::string> diag_files                = {"ocpp.db"};
    std::string           chargepoint_type          = "AC";
    std::string           vendor_name               = "";
    unsigned int          operating_voltage         = 0u;
    std::string           ocpp_version              = "1.6"; 

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
            else if ((strcmp(*argv, "-t") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                connection_url = *argv;
            }
            else if ((strcmp(*argv, "-c") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                chargepoint_id = *argv;
            }
            else if ((strcmp(*argv, "-s") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                serial_number = *argv;
            }
            else if ((strcmp(*argv, "-n") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                nb_connectors = static_cast<unsigned int>(std::atoi(*argv));
            }
            else if ((strcmp(*argv, "-p") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                nb_phases = static_cast<unsigned int>(std::atoi(*argv));
            }
            else if ((strcmp(*argv, "-b") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                mqtt_broker_url = *argv;
            }
            else if ((strcmp(*argv, "-m") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                max_charge_point_setpoint = static_cast<unsigned int>(std::atoi(*argv));
            }
            else if ((strcmp(*argv, "-i") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                max_connector_setpoint = static_cast<unsigned int>(std::atoi(*argv));
            }
            else if ((strcmp(*argv, "-f") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                // add all files in diag file list:
                diag_files.insert(*argv);
                while ((argc > 2) && (*argv[1] != '-'))
                {
                    argv++;
                    argc--;
                    diag_files.insert(*argv);
                }
            }
            else if ((strcmp(*argv, "-e") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                chargepoint_type = *argv;
            }
            else if ((strcmp(*argv, "-v") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                vendor_name = *argv;
                while ((argc > 2) && (*argv[1] != '-'))
                {
                    argv++;
                    argc--;
                    vendor_name += " ";
                    vendor_name += *argv;
                }
            }
            else if ((strcmp(*argv, "-o") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                operating_voltage = static_cast<unsigned int>(std::atoi(*argv));
            }
            else if ((strcmp(*argv, "-a") == 0) && (argc > 1))
            {
                argv++;
                argc--;
                ocpp_version = *argv;
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
            std::cout << "Usage : chargepoint -w working_dir -t connection_url -c chargepoint_id -s serial_number [-n "
                         "nb_connectors] [-p "
                         "nb_phases] [-b mqtt_broker_url]"
                      << std::endl;
            std::cout << "    -w : Working directory where to store the persistant data" << std::endl;
            std::cout << "    -t : URL of the OCPP Central System" << std::endl;
            std::cout << "    -c : OCPP Charge Point Identifier" << std::endl;
            std::cout << "    -s : Charge Point's serial number" << std::endl;
            std::cout << "    -n : Number of connectors (Default = 1)" << std::endl;
            std::cout << "    -p : Number of phases (Default = 3)" << std::endl;
            std::cout << "    -b : URL of the MQTT broker (Default = tcp://localhost:1883)" << std::endl;
            std::cout << "    -m : Max setpoint (in A for AC, in W for DC) for the whole Charge Point (Default = 32A)" << std::endl;
            std::cout << "    -i : Max setpoint (in A for AC, in W for DC) for a connector of the Charge Point (Default = 32A)"
                      << std::endl;
            std::cout << "    -e : Charge Point's type (AC/DC) (Default = AC)" << std::endl;
            std::cout << "    -a : OCPP stack version (1.6/2.0) (Default = 1.6)" << std::endl;
            std::cout << "    -v : Vendor name (Default = OpenOCPP)" << std::endl;
            std::cout << "    -o : Operating voltage (Default = 230)" << std::endl;
            std::cout << "    -f : Files to put in diagnostic zip. Absolute path or relative path from working directory. " << std::endl;
            std::cout << "         (Default = ocpp.db)" << std::endl;
            return 1;
        }
    }

    // Open configuration file
    std::filesystem::path path(working_dir);
    path /= "config.ini";
    SimulatedChargePointConfig config(working_dir, path.string(), diag_files);

    // Update configuration file
    std::filesystem::path db_path(working_dir);
    db_path /= "ocpp.db";
    config.setStackConfigValue("DatabasePath", db_path.string());
    config.setStackConfigValue("ConnexionUrl", connection_url);
    config.setStackConfigValue("ChargePointIdentifier", chargepoint_id);
    config.setStackConfigValue("ChargePointSerialNumber", serial_number);
    config.setOcppConfigValue("NumberOfConnectors", std::to_string(nb_connectors));

    ChargePointData::OCPPVersion cp_ocpp_version = ChargePointData::OCPPVersionHelper.fromString(ocpp_version);

    ConnectorData::ConnectorType cp_current_out_type = ConnectorData::ConnectorTypeHelper.fromString(chargepoint_type);

    if (cp_current_out_type == ConnectorData::ConnectorType::AC)
    {
        config.setOcppConfigValue("MeterValuesSampledData", "Current.Import,Energy.Active.Import.Register,Current.Offered");
        config.setOcppConfigValue("ChargingScheduleAllowedChargingRateUnit", "Current");
    }
    else
    {
        config.setOcppConfigValue("MeterValuesSampledData",
                                  "Energy.Active.Import.Register,Power.Active.Import,Power.Factor,Voltage,Power.Offered");
        config.setOcppConfigValue("ChargingScheduleAllowedChargingRateUnit", "Power");
    }

    config.setOcppConfigValue("ConnectorPhaseRotationMaxLength", std::to_string(nb_connectors));
    std::stringstream connector_phase_rotation;
    for (unsigned int i = 1; i <= nb_connectors; i++)
    {
        connector_phase_rotation << i << ".";
        if ((cp_current_out_type == ConnectorData::ConnectorType::DC) || (nb_phases == 1u))
        {
            connector_phase_rotation << "NotApplicable";
        }
        else
        {
            connector_phase_rotation << "RST";
        }
        if (i != nb_connectors)
        {
            connector_phase_rotation << ",";
        }
    }
    config.setOcppConfigValue("ConnectorPhaseRotation", connector_phase_rotation.str());

    config.setMqttConfigValue("BrokerUrl", mqtt_broker_url);

    if (!vendor_name.empty())
    {
        config.setStackConfigValue("ChargePointVendor", vendor_name);
    }

    if (operating_voltage != 0)
    {
        config.setStackConfigValue("OperatingVoltage", std::to_string(operating_voltage));
    }

    // Start simulated charge point
    SimulatedChargePoint chargepoint(config, max_charge_point_setpoint, max_connector_setpoint, nb_phases, cp_current_out_type, cp_ocpp_version);
    chargepoint.start();

    return 0;
}
