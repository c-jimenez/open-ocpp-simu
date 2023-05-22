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
from kivy.properties import StringProperty, NumericProperty, BooleanProperty


# Connector of a Charge Point
class ConnectorWidget(BoxLayout):

    con_id = NumericProperty(None)
    status = StringProperty(None)
    max_setpoint = NumericProperty(None)
    car_consumption_l1 = NumericProperty(None)
    car_consumption_l2 = NumericProperty(None)
    car_consumption_l3 = NumericProperty(None)
    car_cable = NumericProperty(None)
    car_ready = BooleanProperty(None)
    id_tag = StringProperty(None)
    type = StringProperty(None)

    def set_properties(self, car_consumption_l1: float, car_consumption_l2: float, car_consumption_l3: float, car_cable: float, car_ready: bool, id_tag: str) -> None:
        """ Set the properties values """
        self.sl_car_consumption_l1.value = car_consumption_l1
        self.sl_car_consumption_l2.value = car_consumption_l2
        self.sl_car_consumption_l3.value = car_consumption_l3
        self.sl_car_cable.value = car_cable
        self.tb_car_ready.active = not car_ready
        self.ti_id_tag.text = id_tag
