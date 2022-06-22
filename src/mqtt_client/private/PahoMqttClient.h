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

#ifndef PAHOMQTTCLIENT_H
#define PAHOMQTTCLIENT_H

#include "IMqttClient.h"

#include <MQTTClient.h>

/** @brief MQTT client implementation using Paho MQTT library */
class PahoMqttClient : public IMqttClient
{
  public:
    /**
     * @brief Constructor
     * @param id Unique id
     */
    PahoMqttClient(const std::string& id);

    /** @brief Destructor */
    virtual ~PahoMqttClient();

    /** @copydoc void IMqttClient::registerListener(IListener&) */
    void registerListener(IListener& listener) override { m_listener = &listener; }

    /** @copydoc bool IMqttClient::setWill(const std::string&, const std::string&, QoS, bool) */
    bool setWill(const std::string& topic, const std::string& message, QoS qos, bool retained) override;

    /** @copydoc bool IMqttClient::connect(const std::string&, bool, std::chrono::seconds, std::chrono::seconds) */
    bool connect(const std::string& url, bool clean_session, std::chrono::seconds timeout, std::chrono::seconds keep_alive) override;

    /** @copydoc bool IMqttClient::close() */
    bool close() override;

    /** @copydoc bool IMqttClient::isConnected() const */
    bool isConnected() const override;

    /** @copydoc std::string IMqttClient::brokerUrl() const */
    std::string brokerUrl() const override { return m_url; }

    /** @copydoc bool IMqttClient::publish(const std::string&, const std::string&, QoS, bool) */
    bool publish(const std::string& topic, const std::string& message, QoS qos, bool retained) override;

    /** @copydoc bool IMqttClient::subscribe(const std::string&, QoS) */
    bool subscribe(const std::string& topic, QoS qos) override;

    /** @copydoc bool IMqttClient::unsubscribe(const std::string&) */
    bool unsubscribe(const std::string& topic) override;

  private:
    /** @brief Unique id */
    std::string m_id;
    /** @brief Broker's URL */
    std::string m_url;
    /** @brief Paho's handle */
    MQTTClient m_client;
    /** @brief Publish timeout */
    std::chrono::seconds m_pub_timeout;
    /** @brief Listener */
    IListener* m_listener;
    /** @brief Will message */
    MQTTClient_willOptions m_will;

    /** @brief Callback for connection loss with broker */
    static void onConnectionLost(void* context, char* cause);
    /** @brief Callback for message reception */
    static int onMessageReceived(void* context, char* topic, int topic_len, MQTTClient_message* message);
};

#endif // PAHOMQTTCLIENT_H
