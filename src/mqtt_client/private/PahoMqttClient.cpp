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

#include "PahoMqttClient.h"

/** @brief Instanciate an MQTT client */
IMqttClient* IMqttClient::create(const std::string& id)
{
    return new PahoMqttClient(id);
}

/** @brief Constructor */
PahoMqttClient::PahoMqttClient(const std::string& id)
    : m_id(id), m_url(), m_client(nullptr), m_pub_timeout(1), m_listener(nullptr), m_will(MQTTClient_willOptions_initializer)
{
}

/** @brief Destructor */
PahoMqttClient::~PahoMqttClient()
{
    close();
    delete[] m_will.topicName;
    delete[] m_will.message;
}

/** @copydoc bool IMqttClient::setWill(const std::string&, const std::string&, QoS, bool) */
bool PahoMqttClient::setWill(const std::string& topic, const std::string& message, QoS qos, bool retained)
{
    bool ret = false;

    // Check if already connected
    if (!m_client)
    {
        // Release previous will
        delete[] m_will.topicName;
        delete[] m_will.message;

        // Save new will
        char* wtopic = new char[topic.size() + 1u];
        topic.copy(wtopic, topic.size());
        wtopic[topic.size()] = 0;
        m_will.topicName     = wtopic;
        char* wmsg           = new char[message.size() + 1u];
        message.copy(wmsg, message.size());
        wmsg[message.size()] = 0;
        m_will.message       = wmsg;
        m_will.qos           = static_cast<int>(qos);
        m_will.retained      = static_cast<int>(retained);

        ret = true;
    }

    return ret;
}

/** @copydoc bool IMqttClient::connect(const std::string&, bool, std::chrono::seconds, std::chrono::seconds) */
bool PahoMqttClient::connect(const std::string& url, bool clean_session, std::chrono::seconds timeout, std::chrono::seconds keep_alive)
{
    bool ret = false;

    // Check if already connected
    if (!m_client)
    {
        // Create handle
        if (MQTTClient_create(&m_client, url.c_str(), m_id.c_str(), MQTTCLIENT_PERSISTENCE_NONE, nullptr) == MQTTCLIENT_SUCCESS)
        {
            // Connect to the broker
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif // __clang
            MQTTClient_connectOptions options = MQTTClient_connectOptions_initializer;
#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang
            options.cleansession      = static_cast<int>(clean_session);
            options.connectTimeout    = static_cast<int>(timeout.count());
            options.keepAliveInterval = static_cast<int>(keep_alive.count());
            if (m_will.topicName)
            {
                options.will = &m_will;
            }

            MQTTClient_setCallbacks(m_client, this, &PahoMqttClient::onConnectionLost, &PahoMqttClient::onMessageReceived, nullptr);
            if (MQTTClient_connect(m_client, &options) == MQTTCLIENT_SUCCESS)
            {
                m_url = url;
                ret   = true;
            }
            else
            {
                close();
            }
        }
    }

    return ret;
}

/** @copydoc bool IMqttClient::publish(const std::string&, const std::string&, QoS, bool) */
bool PahoMqttClient::publish(const std::string& topic, const std::string& message, QoS qos, bool retained)
{
    bool ret = false;

    // Check if connected
    if (m_client)
    {
        // Publish message
        MQTTClient_deliveryToken token;
        if (MQTTClient_publish(
                m_client, topic.c_str(), message.size(), message.c_str(), static_cast<int>(qos), static_cast<int>(retained), &token) ==
            MQTTCLIENT_SUCCESS)
        {
            // Wait for completion
            if (MQTTClient_waitForCompletion(m_client, token, static_cast<int>(m_pub_timeout.count())) == MQTTCLIENT_SUCCESS)
            {
                ret = true;
            }
        }
    }

    return ret;
}

/** @copydoc bool IMqttClient::subscribe(const std::string&, QoS) */
bool PahoMqttClient::subscribe(const std::string& topic, QoS qos)
{
    bool ret = false;

    // Check if connected
    if (m_client)
    {
        // Subscribe to topic
        if (MQTTClient_subscribe(m_client, topic.c_str(), static_cast<int>(qos)) == MQTTCLIENT_SUCCESS)
        {
            ret = true;
        }
    }

    return ret;
}

/** @copydoc bool IMqttClient::unsubscribe(const std::string&) */
bool PahoMqttClient::unsubscribe(const std::string& topic)
{
    bool ret = false;

    // Check if connected
    if (m_client)
    {
        // Subscribe to topic
        if (MQTTClient_unsubscribe(m_client, topic.c_str()) == MQTTCLIENT_SUCCESS)
        {
            ret = true;
        }
    }

    return ret;
}

/** @copydoc bool IMqttClient::close() */
bool PahoMqttClient::close()
{
    bool ret = false;

    // Check if connected
    if (m_client)
    {
        // Disconnect from the broker
        MQTTClient_disconnect(m_client, 1000);

        // Release memory
        MQTTClient_destroy(&m_client);
        m_client = nullptr;
        m_url    = "";

        ret = true;
    }

    return ret;
}

/** @copydoc bool IMqttClient::isConnected() const */
bool PahoMqttClient::isConnected() const
{
    bool ret = false;

    // Check if connected
    if (m_client)
    {
        ret = static_cast<bool>(MQTTClient_isConnected(m_client));
    }

    return ret;
}

/** @brief Callback for connection loss with broker */
void PahoMqttClient::onConnectionLost(void* context, char* cause)
{
    (void)cause;

    // Get corresponding instance
    if (context)
    {
        // Notify listener
        PahoMqttClient* client = reinterpret_cast<PahoMqttClient*>(context);
        if (client->m_listener)
        {
            client->m_listener->mqttConnectionLost();
        }
    }
}

/** @brief Callback for message reception */
int PahoMqttClient::onMessageReceived(void* context, char* topic, int topic_len, MQTTClient_message* message)
{
    (void)topic_len;

    // Get corresponding instance
    if (context)
    {
        // Notify listener
        PahoMqttClient* client = reinterpret_cast<PahoMqttClient*>(context);
        if (client->m_listener)
        {
            std::string payload(reinterpret_cast<const char*>(message->payload), static_cast<size_t>(message->payloadlen));
            client->m_listener->mqttMessageReceived(topic, payload, static_cast<QoS>(message->qos), static_cast<bool>(message->retained));
        }
    }

    // Release memory
    MQTTClient_free(topic);
    MQTTClient_freeMessage(&message);

    return 1;
}
