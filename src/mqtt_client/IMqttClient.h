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

#ifndef IMQTTCLIENT_H
#define IMQTTCLIENT_H

#include <chrono>
#include <string>

/** @brief Interface for MQTT clients implementations */
class IMqttClient
{
  public:
    // Forward declaration
    class IListener;

    /** @brief QoS of an MQTT message */
    enum class QoS : int
    {
        /** @brief At most once – the message is sent only once and the client and broker 
         *         take no additional steps to acknowledge delivery (fire and forget). */
        QOS_0 = 0,
        /** @brief At least once – the message is re-tried by the sender multiple times 
         *         until acknowledgement is received (acknowledged delivery) */
        QOS_1,
        /** @brief Exactly once – the sender and receiver engage in a two-level handshake
         *         to ensure only one copy of the message is received (assured delivery) */
        QOS_2
    };

    /** @brief Destructor */
    virtual ~IMqttClient() { }

    /**
     * @brief Register a listener for the events
     * @param listener Listener to register
     */
    virtual void registerListener(IListener& listener) = 0;

    /**
     * @brief Configure a will message (must be done before connect)
     * @param topic Topic on which the message must be published
     * @param message Message to publish
     * @param qos Desired QoS
     * @param retained Indicate if the message must be retained on the broker
     * @return true if the message has been published, false otherwise
     */
    virtual bool setWill(const std::string& topic, const std::string& message, QoS qos = QoS::QOS_0, bool retained = false) = 0;

    /** 
     * @brief Connect to a broker 
     * @param url URL of the broker
     * @param clean_session Indicate if the session must be cleaned on connect
     * @param timeout Connection timeout
     * @param keep_alive Connection keep alive interval
     * @return true if the client is connected, false otherwise
     */
    virtual bool connect(const std::string&   url,
                         bool                 clean_session = true,
                         std::chrono::seconds timeout       = std::chrono::seconds(1),
                         std::chrono::seconds keep_alive    = std::chrono::seconds(10)) = 0;

    /**
     * @brief Close the connection with the broker
     * @return true if the connection has been closed, false otherwise
     */
    virtual bool close() = 0;

    /**
     * @brief Indicate if the client is connected to the broker
     * @return true id the client is connected, false otherwise
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Get the URL of the broker
     * @return URL of the broker if the client is connected, empty string otherwise
     */
    virtual std::string brokerUrl() const = 0;

    /**
     * @brief Publish a message on a topic
     * @param topic Topic on which the message must be published
     * @param message Message to publish
     * @param qos Desired QoS
     * @param retained Indicate if the message must be retained on the broker
     * @return true if the message has been published, false otherwise
     */
    virtual bool publish(const std::string& topic, const std::string& message, QoS qos = QoS::QOS_0, bool retained = false) = 0;

    /**
     * @brief Subscribe to messages on a topic
     * @param topic Topic on which the message must be received
     * @param qos Desired QoS
     * @return true if the subscription is accepted, false otherwise
     */
    virtual bool subscribe(const std::string& topic, QoS qos = QoS::QOS_0) = 0;

    /**
     * @brief Unsubscribe from messages on a topic
     * @param topic Topic on which the message must no longer be received
     * @return true if the unsubscription is accepted, false otherwise
     */
    virtual bool unsubscribe(const std::string& topic) = 0;

    /**
     * @brief Instanciate an MQTT client
     * @param id Unique id for the client
     */
    static IMqttClient* create(const std::string& id);

    /** @brief Interface for listeners to MQTT client events */
    class IListener
    {
      public:
        /** @brief Called when the connection with the broker has been lost */
        virtual void mqttConnectionLost() = 0;

        /** 
         * @brief Called when a message has been received
         * @param topic Topic on which the message has been received
         * @param message Received message
         * @param qos QoS of the received message
         * @param retained Indicate if the message has been retained
         */
        virtual void mqttMessageReceived(const char* topic, const std::string& message, QoS qos, bool retained) = 0;
    };
};

#endif // IMQTTCLIENT_H
