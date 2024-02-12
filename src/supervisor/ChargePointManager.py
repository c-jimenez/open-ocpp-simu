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


import json
import time

from setuptools import setup
from mqtt.MqttClient import MqttClient


class Connector(object):
    """ Data associated with a connector of a Charge Point"""

    def __init__(self, id: int) -> None:
        """ Constructor """

        # Id
        self.id = id
        # Status
        self.status = ""
        # Id tag in use
        self.id_tag = ""
        # Max setpoint
        self.max_setpoint = 0
        # OCPP setpoint
        self.ocpp_setpoint = 0
        # Setpoint
        self.setpoint = 0
        # Consumption
        self.consumption_l1 = 0
        self.consumption_l2 = 0
        self.consumption_l3 = 0
        # Car consumption
        self.car_consumption_l1 = 0
        self.car_consumption_l2 = 0
        self.car_consumption_l3 = 0
        # Car cable capacity
        self.car_cable_capacity = 0
        # Car ready for charging
        self.car_ready = False
        # type (AC/DC)
        self.type = ""


class ChargePoint(object):
    """ Data associated with a Charge Point """

    def __init__(self, id: str) -> None:
        """ Constructor """

        # Id
        self.id = id
        # Type
        self.type = ""
        # Status
        self.status = ""
        # Vendor
        self.vendor = ""
        # Model
        self.model = ""
        # Serial number
        self.serial = ""
        # Number of phases
        self.nb_phases = 0
        # Max setpoint
        self.max_setpoint = 0
        # Connectors
        self.connectors = {}
        # Central System URL
        self.central_system = ""
        # Operating voltage
        self.voltage = 0
        # Smart charge enable (true/false)
        self.smart_charge_enabled = True


class ChargePointManager(object):
    """ Handle the simulated Charge Points' data """

    def __init__(self) -> None:
        """ Constructor """

        # MQTT client
        self.__client = MqttClient()

        # Charge Points
        self.__cps = {}

        # Callback when MQTT connection state has changed
        self.on_connection_change = None

        # Callback when charge point data has been updated
        self.on_cp_update = None

        # Callback when launcher connection state has changed
        self.on_launcher_update = None

    def get_cp(self, id: str) -> ChargePoint:
        """ Get the data associated to a Charge Point """
        cp = None
        try:
            cp = self.__cps[id]
        except:
            pass
        return cp

    def start(self, url: str) -> bool:
        """ Start listening to Charge Points' data """

        # Register to callbacks
        self.__client.on_connection_change = self.__on_connection_change
        self.__client.on_message_received = self.__on_message_received

        # Connect to broker
        ret = self.__client.connect(url)
        if ret:

            # Clear previous data
            self.__cps.clear()

        return ret

    def stop(self) -> bool:
        """ Stop listening to Charge Points' data """

        # Disconnect from broker
        ret = self.__client.disconnect()

        return ret

    def send_connector_values(self, cp_id: str, con_id: int, car_consumption_l1: float, car_consumption_l2: float, car_consumption_l3: float, car_cable: float, car_ready: bool) -> bool:
        """ Send new connector values for a Charge Point """

        ret = False

        if self.__client.is_connected():

            # Build topic name
            topic_name = "cp_simu/cps/"+cp_id+"/connectors/"+str(con_id)+"/car"
            # Build message
            payload = {"cable": car_cable, "ready": car_ready,
                           "consumption_l1": car_consumption_l1 , "consumption_l2": car_consumption_l2, "consumption_l3": car_consumption_l3}


            # Publish message
            ret = self.__client.publish(topic_name, json.dumps(payload))

        return ret

    def send_connector_id_tag(self, cp_id: str, con_id: int, id_tag: str) -> bool:
        """ Send new id tag for a Charge Point's connector """

        ret = False

        if self.__client.is_connected():

            # Build topic name
            topic_name = "cp_simu/cps/"+cp_id + \
                "/connectors/"+str(con_id)+"/id_tag"

            # Build message
            payload = {"id": id_tag}

            # Publish message
            ret = self.__client.publish(topic_name, json.dumps(payload))

        return ret

    def new_charge_point(self, cp: ChargePoint) -> bool:
        """ Send the command to instanciate a new Charge Point """
        ret = self.__new_charge_points([self.__cp_to_dict(cp)])

    def kill_charge_point(self, cp_id: str) -> bool:
        """ Send the command to kill the simulated charge point """
        ret = self.__kill_charge_points([{"id": cp_id}])

    def remove_charge_point(self, cp_id: str) -> bool:
        """ Send the command to remove the simulated charge point """
        ret = self.__remove_charge_points([{"id": cp_id}])

    def restart_all_charge_points(self) -> bool:
        """ send restart command to all simulated charge points """
        ret = True

        for cp_id in self.__cps.keys():
            ret = ret and self.restart_charge_point(cp_id)

        return ret

    def kill_all_charge_points(self):
        """ send kill command to all simulated charge points """
        cp_ids = self.__cps.keys()
        for cp_id in cp_ids:
            self.kill_charge_point(cp_id)

    def restart_charge_point(self, cp_id: str) -> bool:
        """ Send the command to restart the simulated charge point """

        ret = False

        cp = self.__cps.get(cp_id)
        if self.__client.is_connected() and not cp is None:

            # Build topic name
            topic_name = "cp_simu/launcher/cmd"

            # Build message
            cp_dict = self.__cp_to_dict(cp)
            payload = {
                "type": "restart",
                "charge_points": [cp_dict]}

            # Publish message
            ret = self.__client.publish(topic_name, json.dumps(payload))

        return ret

    def save_setup(self, filepath: str) -> bool:
        """ Save the environment setup into a JSON file """
        ret = False

        # Build setup data
        cp_list = []
        for cp in self.__cps.values():
            cp_dict = self.__cp_to_dict(cp)
            cp_list.append(cp_dict)
        setup_data = {"charge_points": cp_list}

        # Save to file
        try:
            f = open(filepath, 'w')
            f.write(json.dumps(setup_data))
            ret = True
        except:
            pass

        return ret

    def load_setup(self, filepath: str) -> bool:
        """ Load the environment setup from a JSON file """
        ret = False

        # Load the file
        setup_data = None
        try:
            f = open(filepath, 'r')
            json_data = f.read()
            setup_data = json.loads(json_data)
            ret = ("charge_points" in setup_data)
        except:
            pass
        if ret and not setup_data is None:

            # Kill all the charge points
            charge_points = []
            for cp in self.__cps.keys():
                charge_points.append({"id": cp})
            ret = self.__kill_charge_points(charge_points)
            if ret:
                # Wait for all the charge points to be dead
                total_dead = 0
                retries = 0
                while (retries < 4) and (not (total_dead == len(self.__cps))):
                    total_dead = 0
                    for cp in self.__cps.values():
                        if cp.status == "Dead":
                            total_dead += 1
                    time.sleep(0.500)
                    retries += 1
                if retries < 4:
                    # Reset internal list
                    cps = self.__cps
                    self.__cps = {}

                    # Clean broker state
                    ret = self.__clean_state(cps.values())

                    # Start new charge points
                    ret = self.__new_charge_points(setup_data["charge_points"])
                else:
                    ret = False

        return ret

    def __on_connection_change(self, connected: bool) -> None:
        """ Called when MQTT connection state has changed """

        if connected:
            # Subscribe to charge point's topics
            self.__client.subscribe("cp_simu/cps/+/status")
            self.__client.subscribe("cp_simu/cps/+/connectors/+/status")
            self.__client.subscribe("cp_simu/launcher/status")

        # Notify change
        if not self.on_connection_change is None:
            self.on_connection_change(connected)

    def __on_message_received(self, topic: str, payload: str, qos: int, retained: bool) -> None:
        """ Called on MQTT message reception """

        # Check launcher topic
        if topic == "cp_simu/launcher/status":

            # Launcher
            if not self.on_launcher_update is None:
                self.on_launcher_update(payload == "Alive")

        else:
            # Extract Charge Point's id
            parts = topic.split("/")
            cp_id = parts[2]

            # Empty messages = clean of corresponding topic
            if payload == "":
                # Notify delete
                if (topic.find("connectors") < 0) and not self.on_cp_update is None:
                    self.on_cp_update(ChargePoint(cp_id), True)
            else:
                # Get chargepoint
                cp = self.__cps.get(cp_id)
                if cp is None:
                    # Create charge point
                    self.__cps[cp_id] = ChargePoint(cp_id)
                    cp = self.__cps.get(cp_id)

                # Check topic
                if topic.find("connectors") < 0:
                    # Charge Point's status

                    # Fill data
                    data = json.loads(payload)
                    cp.status = data["status"]
                    cp.type = data["type"]
                    cp.vendor = data["vendor"]
                    cp.model = data["model"]
                    cp.serial = data["serial"]
                    cp.nb_phases = data["nb_phases"]
                    cp.max_setpoint = data["max_setpoint"]
                    cp.central_system = data["central_system"]
                    cp.voltage = data["voltage"]

                else:
                    # Connector's status

                    # Extract connector's id
                    con_id = int(parts[4])

                    # Get connector
                    con = cp.connectors.get(con_id)
                    if con is None:
                        # Create connector
                        cp.connectors[con_id] = Connector(con_id)
                        con = cp.connectors.get(con_id)

                    # Fill data
                    data = json.loads(payload)
                    con.status = data["status"]
                    con.id_tag = data["id_tag"]
                    con.max_setpoint = data["max_setpoint"]
                    con.ocpp_setpoint = data["ocpp_setpoint"]
                    con.setpoint = data["setpoint"]
                    if cp.nb_phases == 1:
                        con.consumption_l1 = self.display_value(cp.type, data["consumption_l1"])
                    if cp.nb_phases == 2: 
                        con.consumption_l1 = data["consumption_l1"]
                        con.consumption_l2 = data["consumption_l2"] 
                    if cp.nb_phases == 3: 
                        con.consumption_l1 = data["consumption_l1"]
                        con.consumption_l2 = data["consumption_l2"] 
                        con.consumption_l3 = data["consumption_l3"]
                    con.car_consumption_l1 = self.display_value(cp.type, data["car_consumption_l1"])
                    con.car_consumption_l2 = data["car_consumption_l2"]
                    con.car_consumption_l3 = data["car_consumption_l3"]
                    con.car_cable_capacity = self.display_value(cp.type, data["car_cable_capacity"])

                    con.car_ready = data["car_ready"]

                # Notify update
                if not self.on_cp_update is None:
                    self.on_cp_update(cp, False)

    def __cp_to_dict(self, cp: ChargePoint) -> dict:
        """ Convert a ChargePoint object to a dictionary for JSON encoding """
        cp_dict = {}
        cp_dict["id"] = cp.id
        cp_dict["type"] = cp.type
        cp_dict["vendor"] = cp.vendor
        cp_dict["model"] = cp.model
        cp_dict["serial"] = cp.serial
        cp_dict["nb_phases"] = cp.nb_phases
        cp_dict["nb_connectors"] = len(cp.connectors)
        cp_dict["max_setpoint"] = int(cp.max_setpoint)
        cp_dict["voltage"] = cp.voltage
        cp_dict["smart_charge_enabled"] = cp.smart_charge_enabled
        if len(cp.connectors) > 0:
            cp_dict["max_setpoint_per_connector"] = int(
                cp.connectors[1].max_setpoint)
        else:
            cp_dict["max_setpoint_per_connector"] = 0
        cp_dict["central_system"] = cp.central_system
        return cp_dict

    def __kill_charge_points(self, charge_points: list) -> bool:
        """ Send the command to kill simulated charge points """

        ret = False

        if self.__client.is_connected():

            # Build topic name
            topic_name = "cp_simu/launcher/cmd"

            # Build message
            payload = {
                "type": "kill",
                "charge_points": charge_points}

            # Publish message
            ret = self.__client.publish(topic_name, json.dumps(payload))

        return ret

    def __remove_charge_points(self, charge_points: list) -> bool:
        """ Send the command to remove simulated charge points """

        ret = False

        if self.__client.is_connected():

            # Kill charge points
            self.__kill_charge_points(charge_points)

            # List of charge points to remove
            cps_to_remove = []
            for cp_id in charge_points:
                cp = self.__cps.get(cp_id["id"])
                if not cp is None:
                    cps_to_remove.append(cp)

            # Wait for the charge points to be dead
            total_dead = 0
            retries = 0
            while (retries < 4) and (not (total_dead == len(cps_to_remove))):
                total_dead = 0
                for cp in cps_to_remove:
                    if cp.status == "Dead":
                        self.__cps.pop(cp.id)
                        total_dead += 1
                time.sleep(0.500)
                retries += 1

            if retries < 4:
                # Remove charge points
                ret = self.__clean_state(cps_to_remove)

        return ret

    def __new_charge_points(self, charge_points: list) -> bool:
        """ Send the command to instanciate new Charge Points """

        ret = False

        if self.__client.is_connected():

            # Build topic name
            topic_name = "cp_simu/launcher/cmd"

            # Build message
            payload = {
                "type": "start",
                "charge_points": charge_points}

            # Publish message
            ret = self.__client.publish(topic_name, json.dumps(payload))

        return ret

    def __clean_state(self, charge_points: list) -> bool:
        """ Clean the retained status message of simulated charge points """

        ret = False

        if self.__client.is_connected():
            for cp in charge_points:

                # Clean connectors status
                for con_id in cp.connectors.keys():
                    cp_status_topic = "cp_simu/cps/" + \
                        cp.id+"/connectors/" + \
                        str(con_id) + "/status"
                    self.__client.publish(cp_status_topic, "", 0, True)

                # Clean charge point status
                cp_status_topic = "cp_simu/cps/" + \
                    cp.id + "/status"
                self.__client.publish(cp_status_topic, "", 0, True)

            ret = True

        return ret
    
    def display_value(self, connector_type, value):
        if connector_type == "DC":
            value = value / 1000  # Convert from W to Kw to display always in Kw
        return value
