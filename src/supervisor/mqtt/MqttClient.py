# -*- coding: utf-8 -*-

# MIT License

# Copyright (c) 2022 Cedric Jimenez

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


from paho.mqtt import client as mqtt
from urllib.parse import urlparse
import random
import time


class MqttClient(object):
    """ MQTT client implementation using the Paho MQTT library """

    def __init__(self, id: str = None) -> None:
        """ Constructor """

        # Generate a random id if none provided
        if id is None:
            self.__id = f'mqtt-client-{random.randint(0, 100000)}'
        else:
            self.__id = id

        # MQTT client
        self.__client = None

        # Connected state
        self.__is_connected = False

        # Callbacks
        self.on_connection_change = None
        self.on_message_received = None

    def connect(self, url: str, clean_session: bool = True, keep_alive: int = 10) -> bool:
        """ Start connection process to a broker """

        def on_connect(client, userdata, flags, rc):
            userdata.__on_connect(flags, rc)

        def on_disconnect(client, userdata, rc):
            userdata.__on_disconnect(rc)

        def on_message(client, userdata, msg):
            userdata.__on_message(msg)

        ret = False

        # Check if started
        if self.__client is None:

            # Instanciate client
            self.__client = mqtt.Client(
                client_id=self.__id, clean_session=clean_session)
            self.__client.on_connect = on_connect
            self.__client.on_disconnect = on_disconnect
            self.__client.on_message = on_message
            self.__client.user_data_set(self)
            self.__client.reconnect_delay_set(1, 10)

            # Start connection
            url = urlparse(url)
            self.__client.connect_async(url.hostname, url.port, keep_alive)
            self.__client.loop_start()

            ret = True

        return ret

    def disconnect(self):
        """ Disconnect from the broker """

        ret = False

        # Check if started
        if not self.__client is None:

            # Disconnect
            self.__client.disconnect()
            time.sleep(1)
            self.__client.loop_stop()
            self.__client = None

            ret = True

        return ret

    def is_connected(self) -> bool:
        """ Indicate if the client is connected to a broker """
        return self.__is_connected

    def publish(self, topic: str,  message: str, qos: int = 0, retain: bool = False) -> bool:
        """ Publish a message on a topic """

        ret = False

        # Check if started
        if not self.__client is None:

            # Publish message
            info = self.__client.publish(topic, message, qos, retain)
            if info.rc == 0:
                ret = True

        return ret

    def subscribe(self, topic: str, qos: int = 0) -> bool:
        """ Subscribe to messages on a topic """

        ret = False

        # Check if started
        if not self.__client is None:

            # Subscribe to the topic
            info = self.__client.subscribe(topic, qos)
            if info[0] == 0:
                ret = True

        return ret

    def __on_connect(self, flags, rc):
        """ The callback for when the client receives a CONNACK response from the server """

        # Update connected state
        self.__is_connected = True

        # User callback
        if not self.on_connection_change is None:
            self.on_connection_change(True)

    def __on_disconnect(self, rc):
        """ The callback for when the client is disconnected from the server """

        # Update connected state
        self.__is_connected = False

        # User callback
        if not self.on_connection_change is None:
            self.on_connection_change(False)

    def __on_message(self, msg):
        """" The callback for when a PUBLISH message is received from the server """

        # User callback
        if not self.on_message_received is None:
            self.on_message_received(
                msg.topic, msg.payload.decode('utf-8'), msg.qos, msg.retain)
