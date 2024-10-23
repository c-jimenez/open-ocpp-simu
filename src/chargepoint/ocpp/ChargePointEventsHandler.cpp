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
#include "MeterSimulator.h"
#include "SimulatedChargePointConfig.h"

#include <fstream>
#include <iostream>
#include <openocpp/CertificateRequest.h>
#include <openocpp/PrivateKey.h>
#include <openocpp/Sha2.h>
#include <openocpp/StringHelpers.h>
#include <thread>

// With MSVC compiler, the system() call returns directly the command's return value
#ifdef _MSC_VER
#define WEXITSTATUS(x) (x)
#endif // _MSC_VER

using namespace std;
using namespace ocpp::types::ocpp16;
using namespace ocpp::x509;

/** @brief Constructor */
ChargePointEventsHandler::ChargePointEventsHandler(SimulatedChargePointConfig& config)
    : m_config(config),
      m_chargepoint(nullptr),
      m_connectors(nullptr),
      m_working_dir(m_config.workingDir()),
      m_remote_start_pending(config.ocppConfig().numberOfConnectors()),
      m_remote_stop_pending(m_remote_start_pending.size()),

      m_remote_start_id_tag(m_remote_start_pending.size()),
      m_is_connected(false),
      m_reset_pending(false)
{
    for (unsigned int i = 0; i < m_remote_start_pending.size(); i++)
    {
        m_remote_start_pending[i] = false;
        m_remote_stop_pending[i]  = false;
        m_remote_start_id_tag[i]  = "";
    }
}

/** @brief Destructor */
ChargePointEventsHandler::~ChargePointEventsHandler() { }

/** @copydoc void IChargePointEventsHandler::connectionStateChanged(ocpp::types::ocpp16::RegistrationStatus) */
void ChargePointEventsHandler::connectionFailed(ocpp::types::ocpp16::RegistrationStatus status)
{
    cout << "Connection failed, previous registration status : " << RegistrationStatusHelper.toString(status) << endl;
}

/** @copydoc void IChargePointEventsHandler::connectionStateChanged(bool) */
void ChargePointEventsHandler::connectionStateChanged(bool isConnected)
{
    cout << "Connection state changed : " << isConnected << endl;
    m_is_connected = isConnected;
}

/** @copydoc void IChargePointEventsHandler::bootNotification(ocpp::types::ocpp16::RegistrationStatus, const ocpp::types::DateTime&) */
void ChargePointEventsHandler::bootNotification(ocpp::types::ocpp16::RegistrationStatus status, const ocpp::types::DateTime& datetime)
{
    cout << "Bootnotification : " << RegistrationStatusHelper.toString(status) << " - " << datetime.str() << endl;
}

/** @copydoc void IChargePointEventsHandler::datetimeReceived(const ocpp::types::DateTime&) */
void ChargePointEventsHandler::datetimeReceived(const ocpp::types::DateTime& datetime)
{
    cout << "Date time received : " << datetime.str() << endl;
}

/** @copydoc AvailabilityStatus IChargePointEventsHandler::changeAvailabilityRequested(unsigned int, ocpp::types::ocpp16::AvailabilityType) */
ocpp::types::ocpp16::AvailabilityStatus ChargePointEventsHandler::changeAvailabilityRequested(unsigned int                  connector_id,
                                                                                      ocpp::types::ocpp16::AvailabilityType availability)
{
    AvailabilityStatus ret = AvailabilityStatus::Accepted;
    cout << "Change availability requested : " << connector_id << " - " << AvailabilityTypeHelper.toString(availability) << endl;
    if (connector_id > 0)
    {
        ConnectorData& connector = m_connectors->at(connector_id - 1u);
        if (availability == AvailabilityType::Inoperative)
        {
            if ((connector.status != ChargePointStatus::Available) && (connector.status != ChargePointStatus::Reserved) &&
                (connector.status != ChargePointStatus::Faulted))
            {
                connector.unavailable_pending = true;
                ret                           = AvailabilityStatus::Scheduled;
            }
        }
        else
        {
            connector.unavailable_pending = false;
        }
    }

    return ret;
}

/** @copydoc int IChargePointEventsHandler::getTxStartStopMeterValue(unsigned int) */
int ChargePointEventsHandler::getTxStartStopMeterValue(unsigned int connector_id)
{
    int value = 0;
    cout << "Get start/stop meter value for connector " << connector_id << " : ";
    if (connector_id > 0)
    {
        value = static_cast<int>(m_connectors->at(connector_id - 1u).meter->getEnergy());
    }
    cout << value << endl;
    return value;
}

/** @copydoc void IChargePointEventsHandler::reservationStarted(unsigned int) */
void ChargePointEventsHandler::reservationStarted(unsigned int connector_id)
{
    cout << "Reservation started on connector " << connector_id << endl;
}

/** @copydoc void IChargePointEventsHandler::reservationEnded(unsigned int, bool) */
void ChargePointEventsHandler::reservationEnded(unsigned int connector_id, bool canceled)
{
    cout << "End of reservation on connector " << connector_id << " (" << (canceled ? "canceled" : "expired") << ")" << endl;
}

/** @copydoc ocpp::types::ocpp16::DataTransferStatus IChargePointEventsHandler::dataTransferRequested(const std::string&,
                                                                                                  const std::string&,
                                                                                                  const std::string&,
                                                                                                  std::string&) */
ocpp::types::ocpp16::DataTransferStatus ChargePointEventsHandler::dataTransferRequested(const std::string& vendor_id,
                                                                                const std::string& message_id,
                                                                                const std::string& request_data,
                                                                                std::string&       response_data)
{
    (void)response_data;
    cout << "Data transfer received : " << vendor_id << " - " << message_id << " - " << request_data << endl;
    return DataTransferStatus::UnknownVendorId;
}

/** @copydoc bool getMeterValue(unsigned int, const std::pair<ocpp::types::ocpp16::Measurand, ocpp::types::Optional<ocpp::types::ocpp16::Phase>>&, ocpp::types::ocpp16::MeterValue&) */
bool ChargePointEventsHandler::getMeterValue(unsigned int connector_id,
                                             const std::pair<ocpp::types::ocpp16::Measurand, ocpp::types::Optional<ocpp::types::ocpp16::Phase>>& measurand,
                                             ocpp::types::ocpp16::MeterValue& meter_value)
{
    bool ret = false;

    cout << "getMeterValue : " << connector_id << " - " << MeasurandHelper.toString(measurand.first) << endl;
    if (connector_id > 0)
    {

        SampledValue    value;
        MeterSimulator* meter_simulator = m_connectors->at(connector_id - 1u).meter;
        ret                             = true;
        switch (measurand.first)
        {
            case Measurand::CurrentImport:
            {
                auto currents = meter_simulator->getConsumptions();
                if (measurand.second.isSet())
                {
                    unsigned int phase = static_cast<unsigned int>(measurand.second.value());
                    if (phase <= meter_simulator->getNumberOfPhases())
                    {
                        value.value = std::to_string(currents[phase]);
                        value.phase = static_cast<Phase>(phase);
                        value.unit.value() = UnitOfMeasure::A;
                        meter_value.sampledValue.push_back(value);
                    }
                    else
                    {
                        ret = false;
                    }
                }
                else
                {
                    for (size_t i = 0; i < currents.size(); i++)
                    {
                        value.value = std::to_string(currents[i]);
                        value.phase = static_cast<Phase>(i);
                        value.unit.value() = UnitOfMeasure::A;
                        meter_value.sampledValue.push_back(value);
                    }
                }
            }
            break;

            case Measurand::CurrentOffered:
            {
                auto setpoint = m_connectors->at(connector_id - 1u).setpoint;
                value.value   = std::to_string(static_cast<unsigned int>(setpoint));
                value.unit.value() = UnitOfMeasure::A;
                meter_value.sampledValue.push_back(value);
            }
            break;

            case Measurand::PowerOffered:
            {
                auto setpoint = m_connectors->at(connector_id - 1u).setpoint;
                value.value   = std::to_string(static_cast<unsigned int>(setpoint));
                value.unit.value() = UnitOfMeasure::W;
                meter_value.sampledValue.push_back(value);
            }
            break;

            case Measurand::EnergyActiveImportRegister:
            {
                value.value = std::to_string(meter_simulator->getEnergy());
                value.phase = ocpp::types::Optional<Phase>();
                meter_value.sampledValue.push_back(value);
            }
            break;

            case Measurand::PowerActiveImport:
            {
                auto powers = meter_simulator->getInstantPowers();
                if (measurand.second.isSet())
                {
                    // a phase is specified :
                    unsigned int phase = static_cast<unsigned int>(measurand.second.value());
                    if (phase <= meter_simulator->getNumberOfPhases())
                    {
                        value.value        = std::to_string(powers[phase]);
                        value.phase        = static_cast<Phase>(phase);
                        value.unit.value() = UnitOfMeasure::W;
                        meter_value.sampledValue.push_back(value);
                    }
                    else
                    {
                        ret = false;
                    }
                }
                else
                {
                    // no phase specified :
                    if (meter_simulator->getCurrentOutType() == ConnectorData::ConnectorType::DC)
                    {
                        // only first value for DC
                        value.value        = std::to_string(powers[0]);
                        value.unit.value() = UnitOfMeasure::W;
                        meter_value.sampledValue.push_back(value);
                    }
                    else
                    {
                        // all phases for AC station
                        for (size_t i = 0; i < powers.size(); i++)
                        {
                            value.value        = std::to_string(powers[i]);
                            value.phase        = static_cast<Phase>(i);
                            value.unit.value() = UnitOfMeasure::W;
                            meter_value.sampledValue.push_back(value);
                        }
                    }
                }
            }
            break;

            case Measurand::PowerFactor:
            {
                value.value = std::to_string(meter_simulator->getPowerFactor());
                value.phase = ocpp::types::Optional<Phase>();
                meter_value.sampledValue.push_back(value);
            }
            break;

            case Measurand::Voltage:
            {
                auto voltages = meter_simulator->getVoltages();
                if (measurand.second.isSet())
                {
                    unsigned int phase = static_cast<unsigned int>(measurand.second.value());
                    if (phase <= meter_simulator->getNumberOfPhases())
                    {
                        value.value        = std::to_string(voltages[phase]);
                        value.phase        = static_cast<Phase>(phase);
                        value.unit.value() = UnitOfMeasure::V;
                        meter_value.sampledValue.push_back(value);
                    }
                    else
                    {
                        ret = false;
                    }
                }
                else
                {
                    for (size_t i = 0; i < voltages.size(); i++)
                    {
                        value.value = std::to_string(voltages[i]);
                        value.phase = static_cast<Phase>(i);
                        value.unit.value() = UnitOfMeasure::V;
                        meter_value.sampledValue.push_back(value);
                    }
                }
            }
            break;

            default:
            {
                ret = false;
            }
            break;
        }
    }

    return ret;
}

/** @copydoc bool IChargePointEventsHandler::remoteStartTransactionRequested(unsigned int, const std::string&) */
bool ChargePointEventsHandler::remoteStartTransactionRequested(unsigned int connector_id, const std::string& id_tag)
{
    bool ret = false;
    cout << "Remote start transaction : " << connector_id << " - " << id_tag << endl;
    if (connector_id != 0)
    {
        m_remote_start_pending[connector_id - 1u] = true;
        m_remote_start_id_tag[connector_id - 1u]  = id_tag;
        ret                                       = true;
    }
    else
    {
        for (size_t i = 1; i <= m_config.ocppConfig().numberOfConnectors(); i++)
        {
            if (m_chargepoint->getConnectorStatus(i) < ChargePointStatus::Charging)
            {
                m_remote_start_pending[i - 1u] = true;
                m_remote_start_id_tag[i - 1u]  = id_tag;
                ret                            = true;
                break;
            }
        }
    }
    return ret;
}

/** @copydoc bool IChargePointEventsHandler::remoteStopTransactionRequested(unsigned int) */
bool ChargePointEventsHandler::remoteStopTransactionRequested(unsigned int connector_id)
{
    cout << "Remote stop transaction : " << connector_id << endl;
    m_remote_stop_pending[connector_id - 1u] = true;
    return true;
}

/** @copydoc void IChargePointEventsHandler::transactionDeAuthorized(unsigned int) */
void ChargePointEventsHandler::transactionDeAuthorized(unsigned int connector_id)
{
    cout << "Transaction deauthorized on connector : " << connector_id << endl;
}

/** @copydoc bool IChargePointEventsHandler::getLocalLimitationsSchedule(unsigned int, ocpp::types::ocpp16::ChargingSchedule&) */
bool ChargePointEventsHandler::getLocalLimitationsSchedule(unsigned int                   connector_id,
                                                           unsigned int                   duration,
                                                           ocpp::types::ocpp16::ChargingSchedule& schedule)
{
    bool ret = false;

    cout << "Local limitations schedule requested : " << connector_id << " - " << duration << endl;

    if (connector_id > 0)
    {
        // Connector data
        ConnectorData& connector_data = m_connectors->at(connector_id - 1);

        // 1 period
        // local limitation = min of connector capacity and cable plugged
        ChargingSchedulePeriod period;
        if ((connector_data.status >= ChargePointStatus::Charging) && (connector_data.status < ChargePointStatus::Finishing))
        {
            period.limit = std::min(connector_data.max_setpoint, connector_data.car_cable_capacity);
        }
        else
        {
            period.limit = connector_data.max_setpoint;
        }
        period.numberPhases       = connector_data.meter->getNumberOfPhases();
        period.startPeriod        = 0;
        if (connector_data.meter->getCurrentOutType() == ConnectorData::ConnectorType::AC)
        {
            schedule.chargingRateUnit = ChargingRateUnitType::A;
        }
        else
        {
            schedule.chargingRateUnit = ChargingRateUnitType::W;
        }
        schedule.chargingSchedulePeriod.push_back(period);

        ret = true;
    }

    return ret;
}

/** @copydoc bool IChargePointEventsHandler::resetRequested(ocpp::types::ocpp16::ResetType) */
bool ChargePointEventsHandler::resetRequested(ocpp::types::ocpp16::ResetType reset_type)
{
    cout << "Reset requested : " << ResetTypeHelper.toString(reset_type) << endl;
    m_reset_pending = true;
    return true;
}

/** @copydoc ocpp::types::ocpp16::UnlockStatus IChargePointEventsHandler::unlockConnectorRequested(unsigned int) */
ocpp::types::ocpp16::UnlockStatus ChargePointEventsHandler::unlockConnectorRequested(unsigned int connector_id)
{
    cout << "Unlock connector " << connector_id << " requested" << endl;
    return UnlockStatus::Unlocked;
}

/** @copydoc std::string IChargePointEventsHandler::getDiagnostics(const ocpp::types::Optional<ocpp::types::DateTime>&,
                                                                       const ocpp::types::Optional<ocpp::types::DateTime>&) */
std::string ChargePointEventsHandler::getDiagnostics(const ocpp::types::Optional<ocpp::types::DateTime>& start_time,
                                                     const ocpp::types::Optional<ocpp::types::DateTime>& stop_time)
{
    cout << "Get diagnostics" << endl;
    (void)start_time;
    (void)stop_time;

    std::string diag_file = "/tmp/diag.zip";

    std::stringstream ss;
    ss << "zip " << diag_file;

    for (auto filename : m_config.diagFiles())
    {
        std::string filepath = filename;
        if (filepath[0] != '/' && !m_config.workingDir().empty())
        {
            filepath = m_config.workingDir() + "/" + filepath;
        }
        if (std::filesystem::exists(filepath))
        {
            ss << " " << filepath;
        }
        else
        {
            cout << "Unable to add file " << filepath << " in diagnostic zip: not found" << endl;
        }
    }

    int err = WEXITSTATUS(system(ss.str().c_str()));
    cout << "Command line : " << ss.str() << " => " << err << endl;

    return diag_file;
}

/** @copydoc std::string IChargePointEventsHandler::updateFirmwareRequested() */
std::string ChargePointEventsHandler::updateFirmwareRequested()
{
    cout << "Firmware update requested" << endl;
    return "/tmp/firmware.tar.gz";
}

/** @copydoc void IChargePointEventsHandler::installFirmware() */
void ChargePointEventsHandler::installFirmware(const std::string& firmware_file)
{
    cout << "Firmware to install : " << firmware_file << endl;
    std::thread response_thread= std::thread([&]{
        std::this_thread::sleep_for(5s);
    m_chargepoint->notifyFirmwareUpdateStatus(true);});
    response_thread.detach();
}

/** @copydoc bool IChargePointEventsHandler::uploadFile(const std::string&, const std::string&) */
bool ChargePointEventsHandler::uploadFile(const std::string& file, const std::string& url)
{
    bool ret = true;

    cout << "Uploading " << file << " to " << url << endl;

    std::string connection_url = url;
    std::string params;
    if (connection_url.find("ftp://") == 0)
    {
        // FTP => no specific params
    }
    else if (connection_url.find("ftps://") == 0)
    {
        // FTPS
        params = "--insecure --ssl";
        ocpp::helpers::replace(connection_url, "ftps://", "ftp://", false);
    }
    else if (connection_url.find("http://") == 0)
    {
        // HTTP => no specific params
    }
    else if (connection_url.find("https://") == 0)
    {
        // HTTP
        params = "--insecure";
    }
    else
    {
        // Unsupported protocol
        ret = false;
    }
    if (ret)
    {
        std::stringstream ss;
        ss << "curl --silent " << params << " -T " << file << " " << connection_url;
        int err = WEXITSTATUS(system(ss.str().c_str()));
        cout << "Command line : " << ss.str() << endl;
        ret = (err == 0);
    }

    return ret;
}

/** @copydoc bool IChargePointEventsHandler::downloadFile(const std::string&, const std::string&) */
bool ChargePointEventsHandler::downloadFile(const std::string& url, const std::string& file)
{
    bool ret = true;
    cout << "Downloading from " << url << " to " << file << endl;

    std::string connection_url = url;
    std::string params;
    if (connection_url.find("ftp://") == 0)
    {
        // FTP => no specific params
    }
    else if (connection_url.find("ftps://") == 0)
    {
        // FTPS
        params = "--insecure --ssl";
        ocpp::helpers::replace(connection_url, "ftps://", "ftp://", false);
    }
    else if (connection_url.find("http://") == 0)
    {
        // HTTP => no specific params
    }
    else if (connection_url.find("https://") == 0)
    {
        // HTTP
        params = "--insecure";
    }
    else
    {
        // Unsupported protocol
        ret = false;
    }
    if (ret)
    {
        std::stringstream ss;
        ss << "curl --silent " << params << " -o " << file << " " << connection_url;
        int err = WEXITSTATUS(system(ss.str().c_str()));
        cout << "Command line : " << ss.str() << endl;
        ret = (err == 0);
    }

    return ret;
}

// Security extensions

/** @copydoc ocpp::types::ocpp16::CertificateStatusEnumType IChargePointEventsHandler::caCertificateReceived(ocpp::types::ocpp16::CertificateUseEnumType,
                                                                                                     const ocpp::x509::Certificate&) */
ocpp::types::ocpp16::CertificateStatusEnumType ChargePointEventsHandler::caCertificateReceived(ocpp::types::ocpp16::CertificateUseEnumType type,
                                                                                       const ocpp::x509::Certificate&      certificate)
{
    std::string               ca_filename;
    CertificateStatusEnumType ret = CertificateStatusEnumType::Rejected;

    cout << "CA certificate installation requested : type = " << CertificateUseEnumTypeHelper.toString(type)
         << " - certificate subject = " << certificate.subjectString() << endl;

    // Check number of installed certificates
    if (getNumberOfCaCertificateInstalled(true, true, true) < m_config.ocppConfig().certificateStoreMaxLength())
    {
        // Compute SHA256 to generate filename
        Sha2 sha256;
        sha256.compute(certificate.pem().c_str(), certificate.pem().size());

        if (type == CertificateUseEnumType::ManufacturerRootCertificate)
        {
            // Manufacturer => generate a filename to add the new CA

            std::stringstream name;
            name << "fw_" << sha256.resultString() << ".pem";
            ca_filename = (m_working_dir / name.str()).string();
        }
        else
        {
            // Central System => Check AdditionalRootCertificateCheck configuration key

            if (m_config.ocppConfig().additionalRootCertificateCheck() && (getNumberOfCaCertificateInstalled(false, true, false) == 0))
            {
                // Additionnal checks :
                // - only 1 CA certificate allowed
                // - new certificate must be signed by the old one

                // TODO :)
            }

            std::stringstream name;
            name << "cs_" << sha256.resultString() << ".pem";
            ca_filename = (m_working_dir / name.str()).string();
        }

        // Check if the certificate must be saved
        if (!ca_filename.empty())
        {
            if (certificate.toFile(ca_filename))
            {
                ret = CertificateStatusEnumType::Accepted;
                cout << "Certificate saved : " << ca_filename << endl;

                if (type == CertificateUseEnumType::CentralSystemRootCertificate)
                {
                    // Use the new certificate
                    m_config.setStackConfigValue("TlsServerCertificateCa", ca_filename);
                    if (m_chargepoint)
                    {
                        m_chargepoint->reconnect();
                    }
                }
            }
            else
            {
                ret = CertificateStatusEnumType::Failed;
                cout << "Unable to save certificate : " << ca_filename << endl;
            }
        }
    }
    else
    {
        cout << "Maximum number of certificates reached" << endl;
    }

    return ret;
}

/** @copydoc bool IChargePointEventsHandler::chargePointCertificateReceived(const ocpp::x509::Certificate&) */
bool ChargePointEventsHandler::chargePointCertificateReceived(const ocpp::x509::Certificate& certificate)
{
    std::string ca_filename;
    bool        ret = false;

    cout << "Charge point certificate installation requested : certificate subject = " << certificate.subjectString() << endl;

    // Compute SHA256 to generate filename
    Sha2 sha256;
    sha256.compute(certificate.pem().c_str(), certificate.pem().size());

    std::stringstream name;
    name << "cp_" << sha256.resultString() << ".pem";
    std::string cert_filename = (m_working_dir / name.str()).string();

    // Save certificate
    if (certificate.toFile(cert_filename))
    {
        cout << "Certificate saved : " << cert_filename << endl;

        // Retrieve and save the corresponding key/pair with the new certificate
        std::string cert_key_filename = cert_filename + ".key";
        std::filesystem::copy("/tmp/charge_point_key.key", cert_key_filename);

        // Use the new certificate
        m_config.setStackConfigValue("TlsClientCertificate", cert_filename);
        m_config.setStackConfigValue("TlsClientCertificatePrivateKey", cert_key_filename);
        if (m_chargepoint)
        {
            m_chargepoint->reconnect();
        }

        ret = true;
    }
    else
    {
        cout << "Unable to save certificate : " << cert_filename << endl;
    }

    return ret;
}

/** @copydoc ocpp::types::ocpp16::DeleteCertificateStatusEnumType IChargePointEventsHandler::deleteCertificate(ocpp::types::ocpp16::HashAlgorithmEnumType,
                                                                                                           const std::string&,
                                                                                                           const std::string&,
                                                                                                           const std::string&) */
ocpp::types::ocpp16::DeleteCertificateStatusEnumType ChargePointEventsHandler::deleteCertificate(ocpp::types::ocpp16::HashAlgorithmEnumType hash_algorithm,
                                                                                         const std::string& issuer_name_hash,
                                                                                         const std::string& issuer_key_hash,
                                                                                         const std::string& serial_number)
{
    DeleteCertificateStatusEnumType ret = DeleteCertificateStatusEnumType::NotFound;

    cout << "CA certificate deletion requested : hash = " << HashAlgorithmEnumTypeHelper.toString(hash_algorithm)
         << " - serial number = " << serial_number << endl;

    // Prepare for hash computation
    Sha2::Type sha_type;
    if (hash_algorithm == HashAlgorithmEnumType::SHA256)
    {
        sha_type = Sha2::Type::SHA256;
    }
    else if (hash_algorithm == HashAlgorithmEnumType::SHA384)
    {
        sha_type = Sha2::Type::SHA384;
    }
    else
    {
        sha_type = Sha2::Type::SHA512;
    }

    // Look for installed certificates
    for (auto const& dir_entry : std::filesystem::directory_iterator{m_working_dir})
    {
        if (!dir_entry.is_directory())
        {
            std::string filename = dir_entry.path().filename().string();
            if ((ocpp::helpers::startsWith(filename, "fw_") || ocpp::helpers::startsWith(filename, "cs_")) &&
                ocpp::helpers::endsWith(filename, ".pem"))
            {
                Certificate certificate(dir_entry.path());
                if (certificate.isValid() && certificate.serialNumberHexString() == serial_number)
                {
                    Sha2 sha(sha_type);
                    sha.compute(certificate.issuerString().c_str(), certificate.issuerString().size());
                    if (issuer_name_hash == sha.resultString())
                    {
                        sha.compute(&certificate.publicKey()[0], certificate.publicKey().size());
                        if (issuer_key_hash == sha.resultString())
                        {
                            if ((filename == m_config.stackConfig().tlsServerCertificateCa()) || !std::filesystem::remove(dir_entry.path()))
                            {
                                ret = DeleteCertificateStatusEnumType::Failed;
                            }
                            else
                            {
                                ret = DeleteCertificateStatusEnumType::Accepted;
                            }
                        }
                    }
                }
            }
        }
    }

    return ret;
}

/** @copydoc void IChargePointEventsHandler::generateCsr(std::string&) */
void ChargePointEventsHandler::generateCsr(std::string& csr)
{
    cout << "Generate CSR requested" << endl;

    // Generata a new public/private key pair
    PrivateKey private_key(PrivateKey::Type::EC,
                           static_cast<unsigned int>(PrivateKey::Curve::PRIME256_V1),
                           m_config.stackConfig().tlsClientCertificatePrivateKeyPassphrase());
    private_key.privateToFile("/tmp/charge_point_key.key");

    // Generate the CSR
    CertificateRequest::Subject subject;
    subject.country           = m_config.stackConfig().clientCertificateRequestSubjectCountry();
    subject.state             = m_config.stackConfig().clientCertificateRequestSubjectState();
    subject.location          = m_config.stackConfig().clientCertificateRequestSubjectLocation();
    subject.organization      = m_config.ocppConfig().cpoName();
    subject.organization_unit = m_config.stackConfig().clientCertificateRequestSubjectOrganizationUnit();
    subject.common_name       = m_config.stackConfig().chargePointSerialNumber();
    subject.email_address     = m_config.stackConfig().clientCertificateRequestSubjectEmail();
    CertificateRequest certificate_request(subject, private_key);
    csr = certificate_request.pem();
}

/** @copydoc void IChargePointEventsHandler::getInstalledCertificates(ocpp::types::ocpp16::CertificateUseEnumType,
 *                                                                    std::vector<ocpp::x509::Certificate>&) */
void ChargePointEventsHandler::getInstalledCertificates(ocpp::types::ocpp16::CertificateUseEnumType   type,
                                                        std::vector<ocpp::x509::Certificate>& certificates)
{
    cout << "Get installed CA certificates requested : type = " << CertificateUseEnumTypeHelper.toString(type) << endl;

    for (auto const& dir_entry : std::filesystem::directory_iterator{m_working_dir})
    {
        if (!dir_entry.is_directory())
        {
            std::string filename = dir_entry.path().filename().string();
            if (type == CertificateUseEnumType::ManufacturerRootCertificate)
            {
                if (ocpp::helpers::startsWith(filename, "fw_") && ocpp::helpers::endsWith(filename, ".pem"))
                {
                    certificates.emplace_back(dir_entry.path());
                }
            }
            else
            {
                if (ocpp::helpers::startsWith(filename, "cs_") && ocpp::helpers::endsWith(filename, ".pem"))
                {
                    certificates.emplace_back(dir_entry.path());
                }
            }
        }
    }
}

/** @copydoc std::string IChargePointEventsHandler::getLog(ocpp::types::ocpp16::LogEnumType,
                                                           const ocpp::types::Optional<ocpp::types::DateTime>&,
                                                           const ocpp::types::Optional<ocpp::types::DateTime>&) */
std::string ChargePointEventsHandler::getLog(ocpp::types::ocpp16::LogEnumType                            type,
                                             const ocpp::types::Optional<ocpp::types::DateTime>& start_time,
                                             const ocpp::types::Optional<ocpp::types::DateTime>& stop_time)
{
    cout << "Get log : type = " << LogEnumTypeHelper.toString(type) << endl;
    (void)start_time;
    (void)stop_time;

    std::string log_file = "";
    if (type == LogEnumType::SecurityLog)
    {
        // Security logs :
        // if security logs are handled by the Open OCPP stack, just return a path where
        // the stack can generate the log file, otherwise you'll have to generate your
        // own log file as for the diagnostics logs
        if (m_config.stackConfig().securityLogMaxEntriesCount() > 0)
        {
            // The stack will generate the log file into the follwing folder
            log_file = "/tmp/";
        }
        else
        {
            // You'll have to implement the log file generation and provide the path
            // to the generated file
        }
    }
    else
    {
        // Dianostic logs
        log_file = "/tmp/diag.zip";

        std::stringstream ss;
        ss << "zip " << log_file << " " << m_config.stackConfig().databasePath();
        int err = WEXITSTATUS(system(ss.str().c_str()));
        cout << "Command line : " << ss.str() << " => " << err << endl;
    }

    return log_file;
}

bool ChargePointEventsHandler::hasCentralSystemCaCertificateInstalled()
{
    // A better implementation would also check the validity dates of the certificates
    return ((getNumberOfCaCertificateInstalled(false, true, false) != 0) && (!m_config.stackConfig().tlsServerCertificateCa().empty()));
}

/** @copydoc bool IChargePointEventsHandler::hasChargePointCertificateInstalled() */
bool ChargePointEventsHandler::hasChargePointCertificateInstalled()
{
    // A better implementation would also check the validity dates of the certificates
    for (auto const& dir_entry : std::filesystem::directory_iterator{m_working_dir})
    {
        if (!dir_entry.is_directory())
        {
            std::string filename = dir_entry.path().filename().string();
            if (ocpp::helpers::startsWith(filename, "cp_") && ocpp::helpers::endsWith(filename, ".pem"))
            {
                std::string certificate_key = dir_entry.path().string() + ".key";
                if (std::filesystem::exists(certificate_key) && !m_config.stackConfig().tlsClientCertificate().empty() &&
                    !m_config.stackConfig().tlsClientCertificatePrivateKey().empty())
                {
                    return true;
                }
            }
        }
    }
    return false;
}

/** @copydoc ocpp::types::ocpp16::UpdateFirmwareStatusEnumType IChargePointEventsHandler::checkFirmwareSigningCertificate(
 *                                            const ocpp::x509::Certificate&) */
ocpp::types::ocpp16::UpdateFirmwareStatusEnumType ChargePointEventsHandler::checkFirmwareSigningCertificate(
    const ocpp::x509::Certificate& signing_certificate)
{
    UpdateFirmwareStatusEnumType ret = UpdateFirmwareStatusEnumType::InvalidCertificate;

    cout << "Check of firmware signing certificate requested : subject = " << signing_certificate.subjectString()
         << " - issuer = " << signing_certificate.issuerString() << endl;

    // Load all installed Manufacturer CA certificates
    std::vector<Certificate> ca_certificates;
    for (auto const& dir_entry : std::filesystem::directory_iterator{m_working_dir})
    {
        if (!dir_entry.is_directory())
        {
            std::string filename = dir_entry.path().filename().string();
            if (ocpp::helpers::startsWith(filename, "fw_") && ocpp::helpers::endsWith(filename, ".pem"))
            {
                ca_certificates.emplace_back(dir_entry.path());
            }
        }
    }
    if (!ca_certificates.empty())
    {
        // Check signing certificate
        if (signing_certificate.verify(ca_certificates))
        {
            ret = UpdateFirmwareStatusEnumType::Accepted;
        }
    }
    else
    {
        cout << "No manufacturer CA installed" << endl;
    }

    return ret;
}

// ISO 15118 PnC extensions

/** @copydoc bool IChargePointEventsHandler::iso15118CheckEvCertificate(const ocpp::x509::Certificate&) */
bool ChargePointEventsHandler::iso15118CheckEvCertificate(const ocpp::x509::Certificate& certificate)
{
    bool ret = false;

    cout << "ISO15118 EV certificate verification requested : certificate subject = " << certificate.subjectString() << endl;

    // Look for MO certificates
    for (auto const& dir_entry : std::filesystem::directory_iterator{m_working_dir})
    {
        if (!dir_entry.is_directory())
        {
            std::string filename = dir_entry.path().filename().string();
            if (ocpp::helpers::startsWith(filename, "iso_mo_root_") && ocpp::helpers::endsWith(filename, ".pem"))
            {
                Certificate mo_cert(dir_entry.path());
                if (certificate.verify(mo_cert.certificateChain()))
                {
                    cout << "Validated against certificate : " << mo_cert.subjectString() << endl;
                    ret = true;
                    break;
                }
            }
        }
    }

    cout << "EV certificate validated : " << (ret ? "yes" : "no") << endl;

    return ret;
}

/** @copydoc bool IChargePointEventsHandler::iso15118ChargePointCertificateReceived(const ocpp::x509::Certificate&) */
bool ChargePointEventsHandler::iso15118ChargePointCertificateReceived(const ocpp::x509::Certificate& certificate)
{
    std::string ca_filename;
    bool        ret = false;

    cout << "ISO15118 Charge point certificate installation requested : certificate subject = " << certificate.subjectString() << endl;

    // Compute SHA256 to generate filename
    Sha2 sha256;
    sha256.compute(certificate.pem().c_str(), certificate.pem().size());

    std::stringstream name;
    name << "iso_cp_" << sha256.resultString() << ".pem";
    std::string cert_filename = (m_working_dir / name.str()).string();

    // Save certificate
    if (certificate.toFile(cert_filename))
    {
        // Retrieve and save the corresponding key/pair with the new certificate
        std::string cert_key_filename = cert_filename + ".key";
        std::filesystem::copy("/tmp/charge_point_key.key", cert_key_filename);

        cout << "Certificate saved : " << cert_filename << endl;
        ret = true;
    }
    else
    {
        cout << "Unable to save certificate : " << cert_filename << endl;
    }

    return ret;
}

/** @copydoc ocpp::types::ocpp16::DeleteCertificateStatusEnumType IChargePointEventsHandler::iso15118DeleteCertificate(ocpp::types::ocpp16::HashAlgorithmEnumType,
                                                                                                                   const std::string&,
                                                                                                                   const std::string&,
                                                                                                                   const std::string&) */
ocpp::types::ocpp16::DeleteCertificateStatusEnumType ChargePointEventsHandler::iso15118DeleteCertificate(
    ocpp::types::ocpp16::HashAlgorithmEnumType hash_algorithm,
    const std::string&                 issuer_name_hash,
    const std::string&                 issuer_key_hash,
    const std::string&                 serial_number)
{
    cout << "ISO15118 certificate deletion requested : hash = " << HashAlgorithmEnumTypeHelper.toString(hash_algorithm)
         << " - serial number = " << serial_number << endl;
    return deleteCertificate(hash_algorithm, issuer_name_hash, issuer_key_hash, serial_number);
}

/** @copydoc void IChargePointEventsHandler::iso15118GetInstalledCertificates(
                                    bool,
                                    bool,
                                    bool,
                                    std::vector<std::tuple<GetCertificateIdUseEnumType, Certificate, std::vector<Certificate>>>&) */
void ChargePointEventsHandler::iso15118GetInstalledCertificates(
    bool v2g_root_certificate,
    bool mo_root_certificate,
    bool v2g_certificate_chain,
    std::vector<std::tuple<ocpp::types::ocpp16::GetCertificateIdUseEnumType, ocpp::x509::Certificate, std::vector<ocpp::x509::Certificate>>>&
        certificates)
{
    cout << "ISO15118 get installed certificates requested : v2g_root_certificate = " << (v2g_root_certificate ? "yes" : "no")
         << " - mo_root_certificate = " << (mo_root_certificate ? "yes" : "no")
         << " - v2g_certificate_chain = " << (v2g_certificate_chain ? "yes" : "no") << endl;

    for (auto const& dir_entry : std::filesystem::directory_iterator{m_working_dir})
    {
        if (!dir_entry.is_directory())
        {
            std::string filename = (dir_entry.path().filename()).string();
            if (v2g_root_certificate)
            {
                if (ocpp::helpers::startsWith(filename, "iso_v2g_root_") && ocpp::helpers::endsWith(filename, ".pem"))
                {
                    auto tuple = std::make_tuple(GetCertificateIdUseEnumType::V2GRootCertificate,
                                                 Certificate(dir_entry.path()),
                                                 std::vector<ocpp::x509::Certificate>());
                    certificates.emplace_back(std::move(tuple));
                }
            }
            if (mo_root_certificate)
            {
                if (ocpp::helpers::startsWith(filename, "iso_mo_root_") && ocpp::helpers::endsWith(filename, ".pem"))
                {
                    auto tuple = std::make_tuple(GetCertificateIdUseEnumType::MORootCertificate,
                                                 Certificate(dir_entry.path()),
                                                 std::vector<ocpp::x509::Certificate>());
                    certificates.emplace_back(std::move(tuple));
                }
            }
            if (v2g_certificate_chain)
            {
                if (ocpp::helpers::startsWith(filename, "iso_v2g_chain_") && ocpp::helpers::endsWith(filename, ".pem"))
                {
                    auto tuple = std::make_tuple(GetCertificateIdUseEnumType::V2GCertificateChain,
                                                 Certificate(dir_entry.path()),
                                                 std::vector<ocpp::x509::Certificate>());
                    certificates.emplace_back(std::move(tuple));
                }
            }
        }
    }
}

/** @copydoc ocpp::types::ocpp16::InstallCertificateStatusEnumType IChargePointEventsHandler::iso15118CertificateReceived(
                                    ocpp::types::ocpp16::InstallCertificateUseEnumType type,
                                    const ocpp::x509::Certificate&) */
ocpp::types::ocpp16::InstallCertificateStatusEnumType ChargePointEventsHandler::iso15118CertificateReceived(
    ocpp::types::ocpp16::InstallCertificateUseEnumType type, const ocpp::x509::Certificate& certificate)
{
    std::string                      cert_filename;
    InstallCertificateStatusEnumType ret = InstallCertificateStatusEnumType::Rejected;

    cout << "ISO15118 certificate installation requested : type = " << InstallCertificateUseEnumTypeHelper.toString(type)
         << " - certificate subject = " << certificate.subjectString() << endl;

    // Check number of installed certificates
    if (getNumberOfCaCertificateInstalled(true, true, true) < m_config.ocppConfig().certificateStoreMaxLength())
    {
        // Compute SHA256 to generate filename
        Sha2 sha256;
        sha256.compute(certificate.pem().c_str(), certificate.pem().size());

        if (type == InstallCertificateUseEnumType::V2GRootCertificate)
        {
            // V2 root certificate
            std::stringstream name;
            name << "iso_v2g_root_" << sha256.resultString() << ".pem";
            cert_filename = (m_working_dir / name.str()).string();
        }
        else
        {
            // MO root certificate
            std::stringstream name;
            name << "iso_mo_root_" << sha256.resultString() << ".pem";
            cert_filename = (m_working_dir / name.str()).string();
        }

        // Save certificate
        if (certificate.toFile(cert_filename))
        {
            ret = InstallCertificateStatusEnumType::Accepted;
            cout << "Certificate saved : " << cert_filename << endl;
        }
        else
        {
            ret = InstallCertificateStatusEnumType::Failed;
            cout << "Unable to save certificate : " << cert_filename << endl;
        }
    }
    else
    {
        cout << "Maximum number of certificates reached" << endl;
    }

    return ret;
}

/** @copydoc void IChargePointEventsHandler::iso15118GenerateCsr(std::string&) */
void ChargePointEventsHandler::iso15118GenerateCsr(std::string& csr)
{
    cout << "Generate ISO15118 CSR requested" << endl;
    generateCsr(csr);
}

/** @brief Get the number of installed CA certificates */
unsigned int ChargePointEventsHandler::getNumberOfCaCertificateInstalled(bool manufacturer, bool central_system, bool iso15118)
{
    unsigned int count = 0;
    for (auto const& dir_entry : std::filesystem::directory_iterator{m_working_dir})
    {
        if (!dir_entry.is_directory())
        {
            std::string filename = dir_entry.path().filename().string();
            if (manufacturer && ocpp::helpers::startsWith(filename, "fw_") && ocpp::helpers::endsWith(filename, ".pem"))
            {
                count++;
            }
            if (central_system && ocpp::helpers::startsWith(filename, "cs_") && ocpp::helpers::endsWith(filename, ".pem"))
            {
                count++;
            }
            if (iso15118 && ocpp::helpers::startsWith(filename, "iso_") && ocpp::helpers::endsWith(filename, ".pem"))
            {
                count++;
            }
        }
    }
    return count;
}
