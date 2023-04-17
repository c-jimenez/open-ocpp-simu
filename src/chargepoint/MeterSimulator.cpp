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

#include "MeterSimulator.h"

#include <openocpp/ITimerPool.h>

/** @brief Constructor */
MeterSimulator::MeterSimulator(ocpp::helpers::ITimerPool& timer_pool, unsigned int phases_count, ConnectorData::Type type)
    : m_update_timer(timer_pool),
      m_phases_count(phases_count),
      m_voltages(m_phases_count),
      m_consumptions(m_phases_count),
      m_powers(m_phases_count),
      m_energy(0),
      m_mutex(),
      m_current_out_type(type)
{

    // Register to timer events
    m_update_timer.setCallback(std::bind(&MeterSimulator::update, this));
}

/** @brief Destructor */
MeterSimulator::~MeterSimulator()
{
    stop();
}

/** @brief Start the meter */
void MeterSimulator::start()
{
    // Start update timer
    m_update_timer.start(UPDATE_PERIOD);
}

/** @brief Stop the meter */
void MeterSimulator::stop()
{
    // Stop update timer
    m_update_timer.stop();
}

/** @brief Set the voltages in V */
void MeterSimulator::setVoltages(const std::vector<float>& voltages)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; (i < m_phases_count) && (i < voltages.size()); i++)
    {
        m_voltages[i] = voltages[i];
    }
}

/** @brief Set the currents (in A for AC, in W for DC) */
void MeterSimulator::setConsumptions(const std::vector<float>& consumptions)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; (i < m_phases_count) && (i < consumptions.size()); i++)
    {
        m_consumptions[i] = consumptions[i];
    }
}

/** @brief Set the power factor */
void MeterSimulator::setPowerFactor(float powerFactor)
{
    std::lock_guard<std::mutex> lock(m_mutex);
        m_power_factor = powerFactor;

}

/** @brief Get the voltages in V */
std::vector<float> MeterSimulator::getVoltages()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<float>          ret = m_voltages;
    return ret;
}

/** @brief Get the consumptions (in A for AC, in W for DC) */
std::vector<float> MeterSimulator::getConsumptions()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<float>          ret = m_consumptions;
    return ret;
}

/** @brief Get the instant powers in W */
std::vector<float> MeterSimulator::getInstantPowers()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<float>          ret = m_powers;
    return ret;
}

/** @brief Get the total energy in Wh */
int64_t MeterSimulator::getEnergy()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_energy / 1000ll);
}

/** @brief Get the power factor */
float MeterSimulator::getPowerFactor()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    float                       ret = m_power_factor;
    return ret;
}

/** @brief Get the power factor */
ConnectorData::Type MeterSimulator::getCurrentOutType()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ConnectorData::Type          ret = m_current_out_type;
    return ret;
}

/** @brief Periodically update the meter values */
void MeterSimulator::update()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Compute powers
    if (m_current_out_type == ConnectorData::Type::AC)
    {
        for (size_t i = 0; i < m_phases_count; i++)
        {
            m_powers[i] = m_voltages[i] * m_consumptions[i];
        }
    }
    //DC case 
    else
    {   // simple phase in DC, the consumption is already in power for DC
         m_powers[0] = m_consumptions[0]; 
    }

    // Compute energy
    unsigned int energy_mwh = 0;
    for (size_t i = 0; i < m_phases_count; i++)
    {
        energy_mwh += static_cast<unsigned int>(static_cast<int64_t>(m_powers[i]) * UPDATE_PERIOD.count() / 3600ll);
    }
    m_energy += energy_mwh;
}
