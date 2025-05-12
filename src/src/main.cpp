// main.cpp
// Bluetooth HID NES Advantage Joystick
// Copyright (C) 2025 Aaron Perkins
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https:#www.gnu.org/licenses/>.

#include <Arduino.h>
#include "BLEJoystick.h"

// --- Battery ADC ---
#define BATTERY_PIN 0
#define POWER_KEY_PIN 1
#define LED0_PIN 8

// NES Pin Mapping
#define CLK_PIN 2
#define LATCH_PIN 3
#define DATA_PIN 4

// Button mapping (NES to HID bit positions)
#define NES_BUTTON_A 0
#define NES_BUTTON_B 1
#define NES_BUTTON_SELECT 2
#define NES_BUTTON_START 3
#define NES_BUTTON_UP 4
#define NES_BUTTON_DOWN 5
#define NES_BUTTON_LEFT 6
#define NES_BUTTON_RIGHT 7

#define IDLE_TIMEOUT 30000  // milliseconds
#define ADVERTISING_TIMEOUT 30000  // milliseconds

// Global objects
BLEJoystick* joystick;
bool buttonState[8] = {false};
bool prevButtonState[8] = {false};
unsigned long lastActivityTime = 0;
unsigned long advertisingStartTime = 0;
int batteryLevel = 0;
int prevBatteryLevel = 0;

// Function prototypes
void joystickStateCallback();
int readBatteryLevel();
void readNESController();
void powerOn();
void powerOff();
void lightOn();
void lightOff();
void checkTimers();

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  Serial.println("NES Advantage BLE Controller starting...");
  
  // Configure pins
  pinMode(POWER_KEY_PIN, OUTPUT);
  pinMode(LED0_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(DATA_PIN, INPUT_PULLUP);
  
  // Turn power on
  powerOn();
  
  // Initialize the joystick
  joystick = new BLEJoystick("NES Advantage");
  joystick->setStateChangeCallback(joystickStateCallback);
  
  // Start the joystick
  joystick->start();
  joystick->startAdvertising();
  advertisingStartTime = millis();
  
  // Initial battery reading
  batteryLevel = readBatteryLevel();
  prevBatteryLevel = batteryLevel;
}

void loop() {
  // Read controller state
  readNESController();
  
  // Check if state has changed
  bool stateChanged = false;
  for (int i = 0; i < 8; i++) {
    if (buttonState[i] != prevButtonState[i]) {
      stateChanged = true;
      prevButtonState[i] = buttonState[i];
    }
  }
  
  // Update joystick if state changed
  if (stateChanged) {
    Serial.print("NES State: ");
    for (int i = 0; i < 8; i++) {
      Serial.print(buttonState[i] ? "1" : "0");
    }
    Serial.println();
    
    // Calculate hat direction
    uint8_t dpadDirection = 0;
    if (buttonState[NES_BUTTON_UP] && buttonState[NES_BUTTON_RIGHT]) {
      dpadDirection = 2;
    } else if (buttonState[NES_BUTTON_RIGHT] && buttonState[NES_BUTTON_DOWN]) {
      dpadDirection = 4;
    } else if (buttonState[NES_BUTTON_DOWN] && buttonState[NES_BUTTON_LEFT]) {
      dpadDirection = 6;
    } else if (buttonState[NES_BUTTON_LEFT] && buttonState[NES_BUTTON_UP]) {
      dpadDirection = 8;
    } else if (buttonState[NES_BUTTON_UP]) {
      dpadDirection = 1;
    } else if (buttonState[NES_BUTTON_RIGHT]) {
      dpadDirection = 3;
    } else if (buttonState[NES_BUTTON_DOWN]) {
      dpadDirection = 5;
    } else if (buttonState[NES_BUTTON_LEFT]) {
      dpadDirection = 7;
    } else {
      dpadDirection = 0;
    }
    
    // Update axis values from directional buttons
    int8_t x = buttonState[NES_BUTTON_RIGHT] ? 127 : (buttonState[NES_BUTTON_LEFT] ? -127 : 0);
    int8_t y = buttonState[NES_BUTTON_DOWN] ? 127 : (buttonState[NES_BUTTON_UP] ? -127 : 0);
    
    if (joystick->getState() == BLEJoystick::DEVICE_CONNECTED) {
      joystick->setHat(dpadDirection);
      joystick->setButtons(
        buttonState[NES_BUTTON_A],  // A button
        buttonState[NES_BUTTON_B],  // B button
        false, false,               // buttons 3-4
        false, false,               // buttons 5-6
        false, false,               // buttons 7-8
        false, false,               // buttons 9-10
        buttonState[NES_BUTTON_SELECT],  // Select button
        buttonState[NES_BUTTON_START]    // Start button
      );
      joystick->notifyHIDReport();
      lastActivityTime = millis();
    } else if (joystick->getState() == BLEJoystick::DEVICE_IDLE) {
      joystick->startAdvertising();
      advertisingStartTime = millis();
    }
  }
  
  // Check battery level periodically
  static unsigned long lastBatteryCheck = 0;
  if (millis() - lastBatteryCheck > 5000) {  // Check every 5 seconds
    batteryLevel = readBatteryLevel();
    if (batteryLevel != prevBatteryLevel && joystick->getState() == BLEJoystick::DEVICE_CONNECTED) {
      prevBatteryLevel = batteryLevel;
      joystick->setBatteryLevel(batteryLevel);
      joystick->notifyBatteryLevel();
    }
    lastBatteryCheck = millis();
  }
  
  // Check timers for idle and advertising timeouts
  checkTimers();
  
  // Short delay to prevent CPU hogging
  delay(10);
}

void joystickStateCallback() {
  switch (joystick->getState()) {
    case BLEJoystick::DEVICE_IDLE:
      Serial.println("Device idle...");
      lightOff();
      lastActivityTime = millis();
      break;
      
    case BLEJoystick::DEVICE_ADVERTISING:
      Serial.println("Device advertising...");
      advertisingStartTime = millis();
      break;
      
    case BLEJoystick::DEVICE_CONNECTED:
      Serial.println("Device connected...");
      lightOn();
      lastActivityTime = millis();
      // Send initial battery level
      joystick->setBatteryLevel(batteryLevel);
      joystick->notifyBatteryLevel();
      break;
      
    default:
      break;
  }
}

int readBatteryLevel() {
  int raw = analogRead(BATTERY_PIN);
  float voltage = (raw / 4095.0) * 3.3;  // ESP32 has 12-bit ADC
  int percentage = min(100, static_cast<int>((voltage / 3.0) * 100));
  return percentage;
}

void readNESController() {
  // Latch current button states
  digitalWrite(LATCH_PIN, HIGH);
  delayMicroseconds(12);  // Latch pulse (min 12µs)
  digitalWrite(LATCH_PIN, LOW);
  
  // Read all 8 buttons
  for (int i = 0; i < 8; i++) {
    // NES buttons are active low, so invert the reading
    buttonState[i] = !digitalRead(DATA_PIN);
    
    // Clock pulse
    digitalWrite(CLK_PIN, HIGH);
    delayMicroseconds(6);
    digitalWrite(CLK_PIN, LOW);
    delayMicroseconds(6);
  }
}

void powerOn() {
  Serial.println("Powering on...");
  digitalWrite(POWER_KEY_PIN, LOW);
  delay(200);
  digitalWrite(POWER_KEY_PIN, HIGH);
}

void powerOff() {
  Serial.println("Powering off...");
  // Sequence to trigger power off
  digitalWrite(POWER_KEY_PIN, LOW);
  delay(100);
  digitalWrite(POWER_KEY_PIN, HIGH);
  delay(100);
  digitalWrite(POWER_KEY_PIN, LOW);
  delay(100);
  digitalWrite(POWER_KEY_PIN, HIGH);
  
  // Deep sleep
  esp_deep_sleep_start();
}

void lightOn() {
  digitalWrite(LED0_PIN, LOW);  // Active low
}

void lightOff() {
  digitalWrite(LED0_PIN, HIGH); // Active low
}

void checkTimers() {
  unsigned long currentTime = millis();
  
  // Check if device is idle for too long
  if (joystick->getState() == BLEJoystick::DEVICE_IDLE && 
      currentTime - lastActivityTime > IDLE_TIMEOUT) {
    Serial.println("Device idle for too long, going to sleep...");
    powerOff();
  }
  
  // Check if device is advertising for too long
  if (joystick->getState() == BLEJoystick::DEVICE_ADVERTISING && 
      currentTime - advertisingStartTime > ADVERTISING_TIMEOUT) {
    Serial.println("Device advertising for too long, stopping...");
    joystick->stopAdvertising();
    lightOff();
  } else if (joystick->getState() == BLEJoystick::DEVICE_ADVERTISING) {
    // Blink LED while advertising
    digitalWrite(LED0_PIN, (currentTime / 500) % 2 == 0);
  }
}
