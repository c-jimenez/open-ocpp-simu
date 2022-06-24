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

#include "SimulatedChargePoint.h"
#include "ChargePointEventsHandler.h"
#include "MeterSimulator.h"
#include "MqttManager.h"
#include "SimulatedChargePointConfig.h"

#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

using namespace ocpp::types;

/** @brief Constructor */
SimulatedChargePoint::SimulatedChargePoint(SimulatedChargePointConfig& config,
                                           unsigned int                max_charge_point_current,
                                           unsigned int                max_connector_current,
                                           unsigned int                nb_phases)
    : m_config(config),
      m_max_charge_point_current(static_cast<float>(max_charge_point_current)),
      m_max_connector_current(static_cast<float>(max_connector_current)),
      m_nb_phases(nb_phases)
{
}

/** @brief Destructor */
SimulatedChargePoint::~SimulatedChargePoint() { }

/** @brief Start the Charge Point (blocking) */
void SimulatedChargePoint::start()
{
    std::cout << "Starting simulated charge point : " << m_config.stackConfig().chargePointIdentifier() << std::endl;

    // MQTT connectivity
    std::cout << "Starting MQTT connectivity..." << std::endl;
    MqttManager mqtt(m_config);
    std::thread mqtt_thread([&mqtt, this] { mqtt.start(m_nb_phases, m_max_charge_point_current); });

    // OCPP connectivity
    std::cout << "Starting OCPP connectivity..." << std::endl;
    ChargePointEventsHandler                         event_handler(m_config);
    std::unique_ptr<ocpp::chargepoint::IChargePoint> charge_point =
        ocpp::chargepoint::IChargePoint::create(m_config.stackConfig(), m_config.ocppConfig(), event_handler);

    // Allocated data for each connector
    std::vector<MeterSimulator*> meters(m_config.ocppConfig().numberOfConnectors());
    std::vector<ConnectorData>   connectors(meters.size());
    std::vector<float>           voltages(m_nb_phases);
    voltages.assign(voltages.size(), m_config.stackConfig().operatingVoltage());
    for (unsigned int i = 0; i < connectors.size(); i++)
    {
        connectors[i].id           = i + 1u;
        connectors[i].meter        = new MeterSimulator(charge_point->getTimerPool(), m_nb_phases);
        connectors[i].max_setpoint = m_max_connector_current;
        meters[i]                  = connectors[i].meter;
        meters[i]->setVoltages(voltages);
        meters[i]->start();
    }
    event_handler.setConnectors(connectors);

    // Start OCPP
    charge_point->start();

    // Control loop
    loop(mqtt, *charge_point.get(), event_handler, connectors);

    // Stop OCPP
    charge_point->stop();
    for (MeterSimulator*& meter : meters)
    {
        meter->stop();
        delete meter;
    }

    // Wait for end of application
    std::cout << "Waiting end of application..." << std::endl;
    mqtt_thread.join();
}

/** @brief Control loop */
void SimulatedChargePoint::loop(MqttManager&                     mqtt,
                                ocpp::chargepoint::IChargePoint& charge_point,
                                ChargePointEventsHandler&        event_handler,
                                std::vector<ConnectorData>&      connectors)
{
    bool               status_published = false;
    bool               ocpp_connected   = false;
    RegistrationStatus ocpp_status      = RegistrationStatus::Rejected;
    std::string        status_str       = "Disconnected";

    // Control loop
    do
    {
        // Wait for registration with the Central System
        while (!mqtt.isEndOfApplication() && (charge_point.getRegistrationStatus() != RegistrationStatus::Accepted))
        {
            // New chargepoint status
            bool               connected = event_handler.isConnected();
            RegistrationStatus status    = charge_point.getRegistrationStatus();
            if ((connected != ocpp_connected) || (status != ocpp_status))
            {
                // Update status
                ocpp_connected   = connected;
                ocpp_status      = status;
                status_published = false;
                if (ocpp_connected)
                {
                    status_str = RegistrationStatusHelper.toString(status);
                }
                else
                {
                    status_str = "Disconnected";
                }
            }
            if (!status_published)
            {
                status_published = mqtt.publishStatus(status_str, m_nb_phases, m_max_charge_point_current);
            }

            // Publish connectors status
            mqtt.publishData(connectors);

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Publish charge point status
        bool               connected = event_handler.isConnected();
        RegistrationStatus status    = charge_point.getRegistrationStatus();
        if ((connected != ocpp_connected) || (status != ocpp_status))
        {
            // Update status
            ocpp_connected   = connected;
            ocpp_status      = status;
            status_published = false;
            if (ocpp_connected)
            {
                status_str = RegistrationStatusHelper.toString(status);
            }
            else
            {
                status_str = "Disconnected";
            }
        }
        if (!status_published)
        {
            status_published = mqtt.publishStatus(status_str, m_nb_phases, m_max_charge_point_current);
        }

        // Update connector statuses
        for (auto& connector : connectors)
        {
            // Get new status
            ChargePointStatus new_status = charge_point.getConnectorStatus(connector.id);
            if (new_status != connector.status)
            {
                // New status entry
                switch (new_status)
                {
                    case ChargePointStatus::Available:
                    {
                        // Clear any id tag
                        connector.id_tag        = "";
                        connector.parent_id_tag = "";
                    }
                    break;

                    case ChargePointStatus::Preparing:
                    {
                        // Start preparing timeout
                        connector.preparing_start = std::chrono::steady_clock::now();
                    }
                    break;

                    default:
                    {
                        // Nothing to do
                    }
                    break;
                }

                // Update status
                connector.status = new_status;
            }
        }

        // Compute setpoints
        computeSetpoints(charge_point, connectors);

        // Update connector data with MQTT commands
        mqtt.updateData(connectors);

        // Compute next connector statuses
        for (auto& connector : connectors)
        {
            switch (connector.status)
            {
                case ChargePointStatus::Available:
                {
                    // If car plug => Preparing
                    if (connector.car_cable_capacity != 0.f)
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Preparing);
                    }
                    // If valid id tag => Preparing
                    else if (isValidIdTagPresent(mqtt, charge_point, event_handler, connector, false))
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Preparing);
                    }
                    // If fault pending => Faulted
                    else if (connector.fault_pending)
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Faulted);
                    }
                    else
                    {
                        // Stay in current state
                    }
                }
                break;

                case ChargePointStatus::Preparing:
                {
                    // Check valid id tag
                    if (connector.id_tag.empty())
                    {
                        if (isValidIdTagPresent(mqtt, charge_point, event_handler, connector, false))
                        {
                            // Reset timeout
                            connector.preparing_start = std::chrono::steady_clock::now();
                        }
                    }

                    // Try to start charge if plugged and valid id tag
                    if ((connector.car_cable_capacity != 0.f) && !connector.id_tag.empty())
                    {
                        AuthorizationStatus auth_status = charge_point.startTransaction(connector.id, connector.id_tag);
                        if ((auth_status == AuthorizationStatus::Accepted) || (auth_status == AuthorizationStatus::ConcurrentTx))
                        {
                            // If setpoint = 0 => SuspendedEVSE
                            if (connector.setpoint == 0.f)
                            {
                                charge_point.statusNotification(connector.id, ChargePointStatus::SuspendedEVSE);
                            }
                            // If car not charging => SuspendedEV
                            else if (!connector.car_ready)
                            {
                                charge_point.statusNotification(connector.id, ChargePointStatus::SuspendedEV);
                            }
                            // Else => Charging
                            else
                            {
                                charge_point.statusNotification(connector.id, ChargePointStatus::Charging);
                            }
                        }
                        else
                        {
                            connector.id_tag        = "";
                            connector.parent_id_tag = "";
                        }
                    }
                    // If no more car nor valid id tag => Available
                    else if ((connector.car_cable_capacity == 0.f) && connector.id_tag.empty())
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Available);
                    }
                    // If fault pending => Faulted
                    else if (connector.fault_pending)
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Preparing);
                    }
                    else
                    {
                        // If timeout => Available
                        auto now = std::chrono::steady_clock::now();
                        if ((now - connector.preparing_start) >= m_config.ocppConfig().connectionTimeOut())
                        {
                            charge_point.statusNotification(connector.id, ChargePointStatus::Available);
                        }
                    }
                }
                break;

                case ChargePointStatus::SuspendedEVSE:
                {
                    // If transaction stop condition => Finishing
                    if (isTransactionStopCondition(mqtt, charge_point, event_handler, connector))
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Finishing);
                    }
                    // If setpoint != 0 => Other charging state
                    else if (connector.setpoint != 0.f)
                    {
                        // If car not charging => SuspendedEV
                        if (!connector.car_ready)
                        {
                            charge_point.statusNotification(connector.id, ChargePointStatus::SuspendedEV);
                        }
                        // Else => Charging
                        else
                        {
                            charge_point.statusNotification(connector.id, ChargePointStatus::Charging);
                        }
                    }
                    else
                    {
                        // Stay in current state
                    }
                }
                break;

                case ChargePointStatus::SuspendedEV:
                {
                    // If transaction stop condition => Finishing
                    if (isTransactionStopCondition(mqtt, charge_point, event_handler, connector))
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Finishing);
                    }
                    // If setpoint == 0 => SuspendedEVSE
                    else if (connector.setpoint == 0.f)
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::SuspendedEVSE);
                    }
                    // If car charging => Charging
                    else if (connector.car_ready)
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Charging);
                    }
                    else
                    {
                        // Stay in current state
                    }
                }
                break;

                case ChargePointStatus::Charging:
                {
                    // If transaction stop condition => Finishing
                    if (isTransactionStopCondition(mqtt, charge_point, event_handler, connector))
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Finishing);
                    }
                    // If setpoint == 0 => SuspendedEVSE
                    else if (connector.setpoint == 0.f)
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::SuspendedEVSE);
                    }
                    // If not car charging => SuspendedEV
                    else if (!connector.car_ready)
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::SuspendedEV);
                    }
                    else
                    {
                        // Stay in current state
                    }
                }
                break;

                case ChargePointStatus::Finishing:
                {
                    // If fault pending => Faulted
                    if (connector.fault_pending)
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Faulted, ChargePointErrorCode::OtherError);
                    }
                    // If unplugged => Available
                    else if (connector.car_cable_capacity == 0.f)
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Available);
                    }
                    else
                    {
                        // Stay in current state
                    }
                }
                break;

                case ChargePointStatus::Reserved:
                {
                    // If valid id tag (local or remote) => Preparing
                    // Start preparing timeout
                }
                break;

                case ChargePointStatus::Unavailable:
                {
                    // Wait to be available again
                }
                break;

                case ChargePointStatus::Faulted:
                {
                    // If no pending fault => Available
                    if (!connector.fault_pending)
                    {
                        charge_point.statusNotification(connector.id, ChargePointStatus::Available);
                    }
                }
                break;

                default:
                {
                    // Should never happen :)
                }
                break;
            }

            // Compute current consumption
            computeCurrentConsumption(connector);
        }

        // Publish connectors status
        mqtt.publishData(connectors);

        // Polling period
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    } while (!mqtt.isEndOfApplication());
}

/** @brief Check if a valid id tag has been presented (locally or remotely) */
bool SimulatedChargePoint::isValidIdTagPresent(MqttManager&                     mqtt,
                                               ocpp::chargepoint::IChargePoint& charge_point,
                                               ChargePointEventsHandler&        event_handler,
                                               ConnectorData&                   connector,
                                               bool                             local_only)
{
    bool ret = false;

    // Check local id tag
    if (mqtt.isIdTagPending(connector.id))
    {
        AuthorizationStatus auth_status;
        connector.id_tag = mqtt.pendingIdTag(connector.id);
        auth_status      = charge_point.authorize(connector.id, connector.id_tag, connector.parent_id_tag);
        if ((auth_status == AuthorizationStatus::Accepted) || (auth_status == AuthorizationStatus::ConcurrentTx))
        {
            ret = true;
        }
        mqtt.resetIdTagPending(connector.id);
    }
    // Check remote id tag
    else if (!local_only && event_handler.isRemoteStartPending(connector.id))
    {
        AuthorizationStatus auth_status = AuthorizationStatus::Accepted;
        connector.id_tag                = event_handler.remoteStartIdTag(connector.id);
        if (m_config.ocppConfig().authorizeRemoteTxRequests())
        {
            auth_status = charge_point.authorize(connector.id, connector.id_tag, connector.parent_id_tag);
        }
        if ((auth_status == AuthorizationStatus::Accepted) || (auth_status == AuthorizationStatus::ConcurrentTx))
        {
            ret = true;
        }
        event_handler.resetRemoteStartPending(connector.id);
    }
    if (!ret)
    {
        connector.id_tag        = "";
        connector.parent_id_tag = "";
    }

    return ret;
}

/** @brief Check if a stop condition for the transaction has been encountered */
bool SimulatedChargePoint::isTransactionStopCondition(MqttManager&                     mqtt,
                                                      ocpp::chargepoint::IChargePoint& charge_point,
                                                      ChargePointEventsHandler&        event_handler,
                                                      ConnectorData&                   connector)
{
    bool ret = true;

    // Check local id tag
    if (mqtt.isIdTagPending(connector.id))
    {
        std::string id_tag = mqtt.pendingIdTag(connector.id);
        if (id_tag != connector.id_tag)
        {
            AuthorizationStatus auth_status;
            std::string         parent_id_tag;
            auth_status = charge_point.authorize(connector.id, id_tag, parent_id_tag);
            if ((auth_status == AuthorizationStatus::Accepted) || (auth_status == AuthorizationStatus::ConcurrentTx))
            {
                ret = !parent_id_tag.empty() && (parent_id_tag == connector.parent_id_tag);
            }
            else
            {
                ret = false;
            }
        }
        mqtt.resetIdTagPending(connector.id);
        if (ret)
        {
            charge_point.stopTransaction(connector.id, id_tag, Reason::Local);
        }
    }
    // Check remote stop
    else if (event_handler.isRemoteStopPending(connector.id))
    {
        ret = true;
        event_handler.resetRemoteStopPending(connector.id);
        charge_point.stopTransaction(connector.id, "", Reason::Remote);
    }
    // Check EV side unplugged
    else if (connector.car_cable_capacity == 0.f)
    {
        ret = true;
        charge_point.stopTransaction(connector.id, "", Reason::EVDisconnected);
    }
    // Check failure
    else if (connector.fault_pending)
    {
        charge_point.stopTransaction(connector.id, "", Reason::Other);
    }
    else
    {
        ret = false;
    }

    return ret;
}

/** @brief Compute the setpoint for each connector */
void SimulatedChargePoint::computeSetpoints(ocpp::chargepoint::IChargePoint& charge_point, std::vector<ConnectorData>& connectors)
{
    Optional<SmartChargingSetpoint> charge_point_setpoint;
    Optional<SmartChargingSetpoint> connector_setpoint;

    // Default setpoint is max current
    float whole_charge_point_setpoint = m_max_charge_point_current;

    // Get the smart charging setpoint for each connectors
    for (ConnectorData& connector : connectors)
    {
        // Default setpoint is max current per connector
        connector.ocpp_setpoint = connector.max_setpoint;

        // Get the smart charging setpoint
        if (charge_point.getSetpoint(connector.id, charge_point_setpoint, connector_setpoint))
        {
            // Apply setpoints
            if (charge_point_setpoint.isSet())
            {
                if (charge_point_setpoint.value().value < whole_charge_point_setpoint)
                {
                    whole_charge_point_setpoint = charge_point_setpoint.value().value;
                }
            }
            if (connector_setpoint.isSet())
            {
                if (connector_setpoint.value().value < connector.max_setpoint)
                {
                    connector.ocpp_setpoint = connector_setpoint.value().value;
                }
            }
        }
    }

    // If cable plugged, check its limit
    for (ConnectorData& connector : connectors)
    {
        connector.setpoint = connector.ocpp_setpoint;
        if (connector.setpoint > connector.car_cable_capacity)
        {
            connector.setpoint = connector.car_cable_capacity;
        }
    }

    // Check that the sum of all connectors setpoints doesn't exceed
    // the charge point setpoint
    float total_connectors = 0.f;
    for (ConnectorData& connector : connectors)
    {
        if (connector.status == ChargePointStatus::Charging)
        {
            if (connector.setpoint > 0.f)
            {
                total_connectors += connector.setpoint;
            }
        }
    }
    if (total_connectors > whole_charge_point_setpoint)
    {
        // Remove the same percentage of current on each charging connector to not exceed
        // the charge point capacity
        float per_connector_exceed_current_ratio = (whole_charge_point_setpoint / total_connectors);
        for (ConnectorData& connector : connectors)
        {
            if (connector.setpoint > 0.f)
            {
                connector.setpoint *= per_connector_exceed_current_ratio;
            }
        }
    }

    // Floor the setpoints to get integral values
    for (ConnectorData& connector : connectors)
    {
        connector.setpoint = floor(connector.setpoint);
    }
}

/** @brief Compute the current consumption for each connector */
void SimulatedChargePoint::computeCurrentConsumption(ConnectorData& connector)
{
    // Default is no consumption
    connector.consumption = 0.f;

    // Check if charging
    if (connector.status == ChargePointStatus::Charging)
    {
        // Consumption must match both setpoint and car needs
        connector.consumption = std::min(connector.car_consumption, connector.setpoint);
    }

    // Apply consumption in the meter
    std::vector<float> currents(connector.meter->getNumberOfPhases());
    currents.assign(currents.size(), connector.consumption);
    connector.meter->setCurrents(currents);
}