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

#ifndef CONNECTORDATA_H
#define CONNECTORDATA_H

#include <map>
#include <openocpp/IChargePoint.h>
#include <openocpp/EnumToStringFromString.h>

using namespace ocpp::types;

class MeterSimulator;


/** @brief Data associated to a connector */
struct ConnectorData
{
    /** @brief Connector type (AC/DC) */
    enum class ConnectorType { AC, DC };

    static inline const EnumToStringFromString<ConnectorType> ConnectorTypeHelper{{{ConnectorType::AC, "AC"}, {ConnectorType::DC, "DC"}}};

    /** @brief Default constructor */
    ConnectorData()
        : id(0),
          status(),
          id_tag(),
          parent_id_tag(),
          max_setpoint(0.f),
          ocpp_setpoint(0.f),
          setpoint(0.f),
          car_consumption_l1(0.f),
          car_consumption_l2(0.f),
          car_consumption_l3(0.f),
          car_cable_capacity(0.f),
          car_ready(true),
          preparing_start(),
          fault_pending(false),
          unavailable_pending(false)
    {
    }

    /** @brief Id */
    unsigned int id;
    /** @brief Status */
    ocpp::types::ChargePointStatus status;
    /** @brief Id tag in use */
    std::string id_tag;
    /** @brief Parent id of the id tag in use */
    std::string parent_id_tag;
    /** @brief Maximum setpoint */
    float max_setpoint;
    /** @brief OCPP setpoint */
    float ocpp_setpoint;
    /** @brief Setpoint */
    float setpoint;
    /** @brief Car consumption */
    float car_consumption_l1;
    float car_consumption_l2;
    float car_consumption_l3;
    /** @brief Car cable capacity */
    float car_cable_capacity;
    /** @brief Indicate that the car is ready to charge */
    bool car_ready;
    /** @brief Time point when entering Preparing state */
    std::chrono::steady_clock::time_point preparing_start;
    /** @brief Indicate that a fault occured */
    bool fault_pending;
    /** @brief Indicate that an unavailable request has be scheduled */
    bool unavailable_pending;
    /** @brief Meter */
    MeterSimulator* meter;
};

#endif // CONNECTORDATA_H
