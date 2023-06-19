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

#ifndef SIMULATEDCHARGEPOINT_H
#define SIMULATEDCHARGEPOINT_H

#include "ConnectorData.h"

#include <vector>
#include <string>

class SimulatedChargePointConfig;
class MqttManager;
class ChargePointEventsHandler;

/** @brief Simulated Charge Point */
class SimulatedChargePoint
{
  public:
    /**
     * @brief Constructor
     * @param config Configuration
     * @param max_charge_point_setpoint Max setpoint (in A for AC, in W forDC) for the whole Charge Point
     * @param max_connector_setpoint Max setpoint (in A for AC, in W forDC) for a connector of the Charge Point
     * @param nb_phases Number of phases alimenting the Charge Point
     * @param smart_charge_enabled Smart Charge enabled (true/false)
     */
    SimulatedChargePoint(SimulatedChargePointConfig&  config,
                         unsigned int                 max_charge_point_setpoint,
                         unsigned int                 max_connector_setpoint,
                         unsigned int                 nb_phases,
                         ConnectorData::ConnectorType chargepoint_type,
                         bool                         smart_charge_enabled);

    /** @brief Destructor */
    virtual ~SimulatedChargePoint();

    /** @brief Start the Charge Point (blocking) */
    void start();

  private:
    /** @brief Configuration */
    SimulatedChargePointConfig& m_config;

    /** @brief Max setpoint (in A for AC, in W forDC) for the whole Charge Point */
    float m_max_charge_point_setpoint;
    /** @brief Max setpoint (in A for AC, in W forDC) for a connector of the Charge Point */
    float m_max_connector_setpoint;
    /** @brief Number of phases alimenting the Charge Point */
    unsigned int m_nb_phases;
    /** @brief The Charge Point type (AC/DC) */
    ConnectorData::ConnectorType m_charge_point_type;
    /** @brief Smart Charge enabled (true/false) */
    bool m_smart_charge_enabled;


    /** @brief Control loop */
    void loop(MqttManager&                     mqtt,
              ocpp::chargepoint::IChargePoint& charge_point,
              ChargePointEventsHandler&        event_handler,
              std::vector<ConnectorData>&      connectors);

    /** @brief Check if a valid id tag has been presented (locally or remotely) */
    bool isValidIdTagPresent(MqttManager&                     mqtt,
                             ocpp::chargepoint::IChargePoint& charge_point,
                             ChargePointEventsHandler&        event_handler,
                             ConnectorData&                   connector,
                             bool                             local_only);

    /** @brief Check if a stop condition for the transaction has been encountered */
    bool isTransactionStopCondition(MqttManager&                     mqtt,
                                    ocpp::chargepoint::IChargePoint& charge_point,
                                    ChargePointEventsHandler&        event_handler,
                                    ConnectorData&                   connector);

    /** @brief Compute the setpoint for each connector */
    void computeSetpoints(ocpp::chargepoint::IChargePoint& charge_point, std::vector<ConnectorData>& connectors);

    /** @brief Compute the consumption (current or power) for a connector */
    void computeConsumption(ConnectorData& connector);
};

#endif // SIMULATEDCHARGEPOINT_H
