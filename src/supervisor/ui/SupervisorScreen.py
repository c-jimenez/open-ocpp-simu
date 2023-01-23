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

from kivy.uix.boxlayout import BoxLayout
from kivy.uix.popup import Popup
from kivy.uix.label import Label
from kivy.properties import ObjectProperty
from kivy.clock import Clock

from ui.ConnectorWidget import ConnectorWidget
from ui.ChargePointWidget import ChargePointWidget
from ui.NewChargePointWidget import NewChargePointWidget
from ui.FileDialogWidget import FileDialogWidget
from ChargePointManager import ChargePointManager, ChargePoint, Connector

import os.path


class SupervisorScreen(BoxLayout):
    """ Main screen """

    lbl_mqtt_status = ObjectProperty(None)
    lbl_launcher_status = ObjectProperty(None)

    ti_broker_url = ObjectProperty(None)

    bt_connect = ObjectProperty(None)
    bt_add_cp = ObjectProperty(None)
    bt_remove_cp = ObjectProperty(None)
    bt_kill_restart_cp = ObjectProperty(None)

    rv_chargers = ObjectProperty(None)

    def __init__(self, **kwargs) -> None:
        """ Constructor """
        super().__init__(**kwargs)

        # Charge point manager
        self.__cp_mgr = ChargePointManager()

        # Selected charge point identifier
        self.__selected_cp_id = None

    def bt_connect_click(self) -> None:
        """ Called after a click on the connect button """

        # MQTT connection / disconnection
        if self.bt_connect.connected:
            self.__cp_mgr.stop()
            self.lbl_mqtt_status.state = "Idle"
            self.rv_chargers.data = []
            self.rv_chargers.selected_index = -1
            self.stk_connectors.clear_widgets()
        else:
            self.rv_chargers.bind(
                selected_index=self.on_cp_selection_changed)
            self.lbl_mqtt_status.state = "Ko"
            self.__cp_mgr.on_connection_change = self.__on_connection_change
            self.__cp_mgr.on_cp_update = self.__on_cp_update
            self.__cp_mgr.on_launcher_update = self.__on_launcher_update
            self.__cp_mgr.start(self.ti_broker_url.text)

        # Invert button state
        self.bt_connect.connected = not self.bt_connect.connected

    def bt_add_cp_click(self) -> None:
        """ Called after a click on the add charge point button """

        # Callback for the popup
        def popup_callback(popup):
            content = popup.content
            if not content.cancel:
                # Send command
                cp = ChargePoint(content.cp_id)
                cp.vendor = content.vendor
                cp.model = content.model
                cp.serial = content.serial
                cp.nb_phases = content.nb_phases
                cp.max_setpoint = content.max_current
                cp.central_system = content.central_system
                for i in range(1, content.nb_connectors + 1):
                    connector = Connector(i)
                    connector.max_setpoint = content.max_current_per_connector
                    cp.connectors[i] = connector
                self.__cp_mgr.new_charge_point(cp)

        # New charge point popup
        content = NewChargePointWidget()
        popup = Popup(title='New simulated charge point',
                      content=content,
                      size_hint=(None, None), size=(430, 470),
                      auto_dismiss=False)
        content.popup = popup
        popup.bind(on_dismiss=popup_callback)
        popup.open()

    def bt_remove_cp_click(self) -> None:
        """ Called after a click on the remove charge point button """

        # Get selected charge point
        if not self.__selected_cp_id is None:
            # Remove charge point
            self.__cp_mgr.remove_charge_point(self.__selected_cp_id)

    def bt_kill_restart_cp_click(self) -> None:
        """ Called after a click on the kill or restart button """

        # Get selected charge point
        if not self.__selected_cp_id is None:
            if self.bt_kill_restart_cp.kill:

                # Kill charge point
                self.__cp_mgr.kill_charge_point(self.__selected_cp_id)

            else:

                # Restart charge point
                self.__cp_mgr.restart_charge_point(self.__selected_cp_id)

    def bt_save_setup_click(self) -> None:
        """ Called after a click on the save setup button """

        # Callback for save
        def save_callback(filepath: str, filename: str):
            fullpath = os.path.join(filepath, filename)
            if self.__cp_mgr.save_setup(fullpath):
                popup = Popup(title="Setup saved!",
                              content=Label(
                                  text="Setup saved to : " + fullpath),
                              size_hint=(None, None), size=(500, 100))
            else:
                popup = Popup(title="Error!",
                              content=Label(
                                  text="Failed to save setup to : " + fullpath),
                              size_hint=(None, None), size=(500, 100))
            popup.open()

        # Check if there are charge points
        if not (len(self.rv_chargers.data) == 0):
            # Ask for a file path
            content = FileDialogWidget(text="Save", save=save_callback)
            popup = Popup(title="Save file", content=content,
                                size_hint=(None, 0.8), size=(500, 0))
            content.popup = popup
            popup.open()

    def bt_load_setup_click(self) -> None:
        """ Called after a click on the load setup button """

        # Callback for load
        def load_callback(filepath: str, filename: str):
            fullpath = os.path.join(filepath, filename)
            if not self.__cp_mgr.load_setup(fullpath):
                popup = Popup(title="Error!",
                              content=Label(
                                  text="Failed to load setup from : " + fullpath),
                              size_hint=(None, None), size=(500, 100))
                popup.open()

        # Ask for a file path
        content = FileDialogWidget(text="Load", save=load_callback)
        popup = Popup(title="Load file", content=content,
                            size_hint=(None, 0.8), size=(500, 0))
        content.popup = popup
        popup.open()

    def bt_restart_all_cp(self) -> None:
        self.__cp_mgr.restart_all_charge_points()
    def bt_kill_all_cp(self) -> None:
        self.__cp_mgr.kill_all_charge_points()

    def __on_connection_change(self, connected: bool) -> None:
        """ Called when MQTT connection state has changed """
        if connected:
            self.lbl_mqtt_status.state = "Ok"
        else:
            self.lbl_mqtt_status.state = "Ko"

    def __on_launcher_update(self, is_launcher_alive: bool) -> bool:
        """ Called when the launcher's status changes """
        self.lbl_launcher_status.alive = is_launcher_alive

    def __on_cp_update(self, cp: ChargePoint, deleted: bool) -> None:
        """ Called when a charge point has updated its data"""

        # Check if it's a known charge point
        found = False
        cp_index = 0
        for lbl_cp in self.rv_chargers.data:
            if lbl_cp['text'] == cp.id:
                found = True
                break
            else:
                cp_index += 1
        if found:
            # Check if it is a deleted charge point
            if deleted:
                # Remove from the list
                if cp.id == self.__selected_cp_id:
                    Clock.schedule_once(
                        lambda unused: self.stk_connectors.clear_widgets(), 0)
                for data in self.rv_chargers.data:
                    if data['text'] == cp.id:
                        self.rv_chargers.data.remove(data)
                        return
            else:
                # Check if it is the currently selected charge point
                if cp.id == self.__selected_cp_id:
                    # Update charge point status
                    for widget in self.stk_connectors.children:
                        if isinstance(widget, ChargePointWidget):
                            widget.status = cp.status
                            break
                    # Update connectors status
                    for con_id in cp.connectors:
                        connector = cp.connectors[con_id]
                        for con_index in range(len(self.stk_connectors.children)):
                            connector_widget = self.stk_connectors.children[con_index]
                            if isinstance(connector_widget, ConnectorWidget) and connector_widget.con_id == con_id:
                                connector_widget.status = connector.status
                                connector_widget.consumption_l1 = connector.consumption_l1
                                connector_widget.consumption_l2 = connector.consumption_l2
                                connector_widget.consumption_l3 = connector.consumption_l3
                                break

        else:
            # Add charge point
            self.rv_chargers.data.append(
                {'text': cp.id, 'alive': True})
            cp_index = -1

        # Check charge point status
        if cp.status == "Dead":
            if self.rv_chargers.data[cp_index]['alive']:
                self.rv_chargers.data[cp_index]['alive'] = False
                self.rv_chargers.data.append(self.rv_chargers.data[cp_index])
                self.rv_chargers.data.pop(cp_index)
        else:
            if not self.rv_chargers.data[cp_index]['alive']:
                self.rv_chargers.data[cp_index]['alive'] = True
                self.rv_chargers.data.append(self.rv_chargers.data[cp_index])
                self.rv_chargers.data.pop(cp_index)

    def on_cp_selection_changed(self, value, *args):
        """ Called when the selected charge point has changed """

        index = args[0]
        if index < 0:
            # Not selected, clear connector view
            self.stk_connectors.clear_widgets()
            self.__selected_cp_id = None
            self.bt_kill_restart_cp.active = False
        else:
            # Selected, fill connector view
            cp_id = self.rv_chargers.data[index]['text']
            cp = self.__cp_mgr.get_cp(cp_id)
            if cp is None:
                # Update buttons
                self.stk_connectors.clear_widgets()
                self.__selected_cp_id = None
                self.bt_kill_restart_cp.active = False
            else:
                # Update buttons
                self.__selected_cp_id = cp_id
                self.bt_kill_restart_cp.active = True
                if cp.status == "Dead":
                    self.bt_kill_restart_cp.kill = False
                else:
                    self.bt_kill_restart_cp.kill = True

                # Add charge point widget
                cp_widget = ChargePointWidget()
                cp_widget.cp_id = cp_id
                cp_widget.status = cp.status
                cp_widget.vendor = cp.vendor
                cp_widget.model = cp.model
                cp_widget.serial = cp.serial
                cp_widget.nb_phases = cp.nb_phases
                cp_widget.max_current = cp.max_setpoint
                self.stk_connectors.add_widget(cp_widget)

                # Add connectors widgets
                for con_id in cp.connectors:

                    # Fill connector data
                    connector = cp.connectors[con_id]
                    widget = ConnectorWidget()
                    widget.con_id = connector.id
                    widget.status = connector.status
                    widget.max_current = connector.max_setpoint
                    widget.consumption_l1 = connector.consumption_l1
                    widget.consumption_l2 = connector.consumption_l2
                    widget.consumption_l3 = connector.consumption_l3
                    widget.set_properties(connector.car_consumption_l1, connector.car_consumption_l2, connector.car_consumption_l3, connector.car_cable_capacity,
                                          connector.car_ready, connector.id_tag)

                    # Bind to events
                    widget.bind(
                        car_consumption_l1=self.on_connector_value_changed)
                    widget.bind(
                        car_consumption_l2=self.on_connector_value_changed)
                    widget.bind(
                        car_consumption_l3=self.on_connector_value_changed)
                    widget.bind(
                        car_cable=self.on_connector_value_changed)
                    widget.bind(
                        car_ready=self.on_connector_value_changed)
                    widget.bt_id_tag.bind(
                        on_release=self.on_connector_id_tag_passed)

                    # Add connector widget
                    self.stk_connectors.add_widget(widget)

    def on_connector_value_changed(self, connector, *args):
        """ Called when a value in the connector has changed """
        # Send new connector values
        self.__cp_mgr.send_connector_values(
            self.__selected_cp_id, connector.con_id, connector.car_consumption_l1, connector.car_consumption_l2, connector.car_consumption_l3, connector.car_cable, connector.car_ready)

    def on_connector_id_tag_passed(self, connector, *args):
        """ Called when an id tag has been passed on a connector """
        # Send id tag event
        if not (connector.id_tag == ""):
            self.__cp_mgr.send_connector_id_tag(
                self.__selected_cp_id, connector.con_id, connector.id_tag)
