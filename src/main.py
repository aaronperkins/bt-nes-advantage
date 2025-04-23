# Bluetooth HID NES Advantage Joystick
# Copyright (C) 2025 Aaron Perkins
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https:#www.gnu.org/licenses/>.

# Implements a BLE HID joystick
import time
from machine import Pin, ADC, deepsleep, Timer
from hid_services import Joystick
import utime

# --- Battery ADC ---
BATTERY = ADC(Pin(0))
BATTERY.atten(ADC.ATTN_11DB)  # For full voltage range (0-3.3V)

POWER_KEY = Pin(1, Pin.OUT)

LED0 = Pin(8, Pin.OUT)

# NES Pin Mapping
CLK = Pin(2, Pin.OUT)
LATCH = Pin(3, Pin.OUT)
DATA = Pin(4, Pin.IN)

# Button mapping (NES to HID bit positions)
# ['A', 'B', 'Select', 'Start', 'Up', 'Down', 'Left', 'Right']
NES_BUTTON_A = 0
NES_BUTTON_B = 1
NES_BUTTON_SELECT = 2
NES_BUTTON_START = 3
NES_BUTTON_UP = 4
NES_BUTTON_DOWN = 5
NES_BUTTON_LEFT = 6
NES_BUTTON_RIGHT = 7

IDLE_TIMEOUT = 30  # seconds
ADVERTISING_TIMEOUT = 30  # seconds

class Device:
    def __init__(self):
        # Define state
        self.state = []
        self.prev_state = []
        self.idle_count = 0
        self.advertising_count = 0
        self.prev_battery_level = 0
        self.battery_level = 0

        # Turn power regulator on
        self.power_on()

        self.timer = Timer(0)
        self.timer.init(period=1000, mode=Timer.PERIODIC, callback=self.timer_callback)

        # Create our device
        self.joystick = Joystick("NES Advantage")
        # Set a callback function to catch changes of device state
        self.joystick.set_state_change_callback(self.joystick_state_callback)
        # Start our device
        self.joystick.start()
        # Start advertising immediately
        self.joystick.start_advertising()

    def timer_callback(self, t):
        self.battery_level = self.read_battery_level()
        if self.joystick.get_state() is Joystick.DEVICE_IDLE:
            self.light_off()
            self.idle_count += 1
            if self.idle_count > IDLE_TIMEOUT: 
                print("Device idle for too long, going to sleep ...")    
                self.idle_count = 0             
                self.power_off()                              
            return
        elif self.joystick.get_state() is Joystick.DEVICE_ADVERTISING:
            self.advertising_count += 1
            if self.advertising_count > ADVERTISING_TIMEOUT:
                print("Device advertising for too long, stopping ...")
                self.joystick.stop_advertising()
                self.light_off()
                self.advertising_count = 0
            else:
                LED0.value(self.advertising_count % 2)
            return
        elif self.joystick.get_state() is Joystick.DEVICE_CONNECTED:
            self.light_on()
            if self.battery_level != self.prev_battery_level:
                self.prev_battery_level = self.battery_level
                self.joystick.set_battery_level(self.battery_level)
                self.joystick.notify_battery_level()
            return
        else:
            return

    def joystick_state_callback(self):
        if self.joystick.get_state() is Joystick.DEVICE_IDLE:
            self.idle_count = 0
            print("Device idle ...")
            return
        elif self.joystick.get_state() is Joystick.DEVICE_ADVERTISING:
            self.advertising_count = 0
            self.idle_count = 0
            print("Device advertising ...")
            return
        elif self.joystick.get_state() is Joystick.DEVICE_CONNECTED:
            self.idle_count = 0
            print("Device connected ...")
            return
        else:
            return

    def read_battery_level(self):
        raw = BATTERY.read()
        voltage = (raw / 4095.0) * 3.3  # Assuming 10-bit ADC and 3.3V max
        battery_percent = min(100, int((voltage / 3.0) * 100))
        return battery_percent

    def read_nes_controller(self):
        # Latch current button states
        LATCH.value(1)
        time.sleep_us(12)  # Latch pulse (min 12µs)
        LATCH.value(0)

        buttons = []
        for i in range(8):
            bit = not DATA.value()  # NES buttons are active low
            # bit = TEST.value()  # NES buttons are active low
            buttons.append(bit)
            CLK.value(1)
            utime.sleep_us(6)
            CLK.value(0)
            utime.sleep_us(6)
        return buttons

    def power_on(self):
        print("Powering on ...")
        POWER_KEY.value(0)
        time.sleep_ms(200)
        POWER_KEY.value(1)

    def power_off(self):
        print("Powering off ...")
        POWER_KEY.value(0)
        time.sleep_ms(100)
        POWER_KEY.value(1)
        time.sleep_ms(100)
        POWER_KEY.value(0)
        time.sleep_ms(100)
        POWER_KEY.value(1)

    def light_on(self):
        LED0.value(0)

    def light_off(self):
        LED0.value(1)

    # Main loop
    def start(self):      
        while True:
            # Read pin values and update variables
            self.state = self.read_nes_controller()

            # If the variables changed do something depending on the device state
            if (self.state != self.prev_state):
                # Update values
                self.prev_state = self.state
                print("NES State:", self.state)
                
                x = self.state[NES_BUTTON_RIGHT] * 127 - self.state[NES_BUTTON_LEFT] * 127
                y = self.state[NES_BUTTON_DOWN] * 127 - self.state[NES_BUTTON_UP] * 127

                if self.state[NES_BUTTON_UP] and self.state[NES_BUTTON_RIGHT]:
                    dpad_direction = 2
                elif self.state[NES_BUTTON_RIGHT] and self.state[NES_BUTTON_DOWN]:
                    dpad_direction = 4
                elif self.state[NES_BUTTON_DOWN] and self.state[NES_BUTTON_LEFT]:
                    dpad_direction = 6
                elif self.state[NES_BUTTON_LEFT] and self.state[NES_BUTTON_UP]:
                    dpad_direction = 8
                elif self.state[NES_BUTTON_UP]:
                    dpad_direction = 1
                elif self.state[NES_BUTTON_RIGHT]:
                    dpad_direction = 3
                elif self.state[NES_BUTTON_DOWN]:
                    dpad_direction = 5
                elif self.state[NES_BUTTON_LEFT]:
                    dpad_direction = 7
                else:
                    dpad_direction = 0

                # If connected set axes and notify
                # If idle start advertising for 30s or until connected
                if self.joystick.get_state() is Joystick.DEVICE_CONNECTED:
                    # self.joystick.set_axes(x, y)
                    self.joystick.set_hat(dpad_direction)
                    self.joystick.set_buttons(b4=self.state[NES_BUTTON_B], b1=self.state[NES_BUTTON_A], b11=self.state[NES_BUTTON_SELECT], b12=self.state[NES_BUTTON_START])
                    self.joystick.notify_hid_report()
                elif self.joystick.get_state() is Joystick.DEVICE_IDLE:
                    self.joystick.start_advertising()
              
d = Device()
d.start()