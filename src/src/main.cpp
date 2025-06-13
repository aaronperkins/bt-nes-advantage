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
#define BATTERY_VOLTAGE_DIVIDER_R1 100000  // 100k ohm
#define BATTERY_VOLTAGE_DIVIDER_R2 100000  // 100k ohm
#define RED_LED_PIN 7
#define BLUE_LED_PIN 8
#define GREEN_LED_PIN 9
#define BATTERY_CHARGE_PIN 10
#define BATTERY_STANDBY_PIN 20

// NES Pin Mapping
#define CLK_PIN 3
#define LATCH_PIN 2
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

#define IDLE_TIMEOUT 60000  // milliseconds
#define ADVERTISING_TIMEOUT 30000  // milliseconds

// Global objects
BLEJoystick* joystick;
bool buttonState[8] = {false};
bool prevButtonState[8] = {false};
unsigned long lastActivityTime = 0;
unsigned long advertisingStartTime = 0;
int batteryLevel = 0;
int prevBatteryLevel = 0;
unsigned long startButtonPressTime = 0;
unsigned long selectButtonPressTime = 0;
const unsigned long POWER_OFF_HOLD_TIME = 5000; // 5 seconds
const unsigned long RECONNECT_HOLD_TIME = 5000; // 5 seconds

// Function prototypes
void joystickStateCallback();
int readBatteryLevel();
bool readNESController();
void sleep();
void checkTimers();

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  Serial.println("NES Advantage BLE Controller starting...");
  
  // Configure pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(DATA_PIN, INPUT_PULLUP);
  pinMode(BATTERY_CHARGE_PIN, INPUT_PULLUP);
  pinMode(BATTERY_STANDBY_PIN, INPUT_PULLUP);

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

  esp_sleep_enable_timer_wakeup(100000);
}

void loop() {
  // Read controller state
  bool stateChanged = readNESController();
 
  // Update joystick if state changed
  if (stateChanged) {
    Serial.print("NES state change: ");
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
    } else if (joystick->getState() == BLEJoystick::DEVICE_IDLE && selectButtonPressTime == 0) {
      Serial.println("Start advertising ...");
      joystick->startAdvertising();
      advertisingStartTime = millis();
    }
  }
  
  // Check battery level and charging status periodically
  static unsigned long lastBatteryCheck = 0;
  if (millis() - lastBatteryCheck > 5000) {  // Check every 5 seconds
    // Update battery level
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
      Serial.println("Device idle.");
      digitalWrite(BLUE_LED_PIN, HIGH);
      lastActivityTime = millis();
      break;
      
    case BLEJoystick::DEVICE_ADVERTISING:
      Serial.println("Device advertising.");
      advertisingStartTime = millis();
      break;
      
    case BLEJoystick::DEVICE_CONNECTED:
      Serial.println("Device connected.");
      digitalWrite(BLUE_LED_PIN, LOW);
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
  float voltage = (raw / 4095.0) * 3.3; 
  voltage = (voltage * (BATTERY_VOLTAGE_DIVIDER_R1 + BATTERY_VOLTAGE_DIVIDER_R2)) / BATTERY_VOLTAGE_DIVIDER_R2;
  voltage -= 0.35; // Fudge factor
  int percentage = map((int)(voltage * 100), 300, 420, 0, 100);
  return percentage;
}

bool readNESController() {
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

  // Check if state has changed
  bool stateChanged = false;
  for (int i = 0; i < 8; i++) {
    if (buttonState[i] != prevButtonState[i]) {
      stateChanged = true;
      prevButtonState[i] = buttonState[i];
    }
  }
  return stateChanged;
}

void sleep() {
  Serial.println("Sleeping...");
  
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(BLUE_LED_PIN, HIGH);
  delay(1000); // Allow time for serial output to complete

  bool debounce = false;
  bool wakeUp = false;
  while (!wakeUp) {
    esp_light_sleep_start();
    delay(100);
    readNESController();
    if (!buttonState[NES_BUTTON_START]) {
      debounce=true;
    }
    else {
      if (debounce) {
        wakeUp = true;
      }
    }
  }

  Serial.println("Waking up...");
  startButtonPressTime = 0;
  lastActivityTime = 0;
  Serial.println("Start advertising ...");
  joystick->startAdvertising();
  advertisingStartTime = millis();
}

void checkTimers() {
  unsigned long currentTime = millis();
  
  // Check battery charging status and update GREEN LED
  bool isCharging = digitalRead(BATTERY_CHARGE_PIN) == LOW;
  bool isFullyCharged = digitalRead(BATTERY_STANDBY_PIN) == LOW;
  if (isCharging && !isFullyCharged) {
    // Blinking green LED when charging (toggle every 500ms)
    digitalWrite(GREEN_LED_PIN, (currentTime / 500) % 2 == 0);
  } else if (isFullyCharged) {
    // Solid green LED when fully charged
    digitalWrite(GREEN_LED_PIN, LOW); // LED is active LOW
  } else {
    // No charging activity - LED off
    digitalWrite(GREEN_LED_PIN, HIGH); // LED is active LOW
  }

  // Check if battery level is low
  if (batteryLevel < 20) {
    digitalWrite(RED_LED_PIN, (currentTime / 500) % 2 == 0);
  } else {
    digitalWrite(RED_LED_PIN, HIGH); // Turn off red LED
  }

  // Check if device is idle for too long
  if (joystick->getState() == BLEJoystick::DEVICE_IDLE && 
      currentTime - lastActivityTime > IDLE_TIMEOUT) {
    Serial.println("Device idle for too long, going to sleep...");
    sleep();
  }
  
  // Check if device is advertising for too long
  if (joystick->getState() == BLEJoystick::DEVICE_ADVERTISING && 
      currentTime - advertisingStartTime > ADVERTISING_TIMEOUT) {
    Serial.println("Device advertising for too long, stopping...");
    joystick->stopAdvertising();
    digitalWrite(BLUE_LED_PIN, HIGH);
  } else if (joystick->getState() == BLEJoystick::DEVICE_ADVERTISING) {
    // Blink LED while advertising
    digitalWrite(BLUE_LED_PIN, (currentTime / 500) % 2 == 0);
  }

 // Check for start button long press (sleep)
  if (buttonState[NES_BUTTON_START]) {
    // Start button pressed or still being held
    if (startButtonPressTime == 0) {
      // Just pressed, record time
      startButtonPressTime = currentTime;
    } else if (currentTime - startButtonPressTime >= POWER_OFF_HOLD_TIME) {
      // Held for the required duration, sleep
      Serial.println("Start button held for 5 seconds, entering sleep mode ...");
      sleep();
    }
  } else {
    // Start button released
    startButtonPressTime = 0;
  }
  
  // Check for select button long press (disconnect and restart advertising)
  if (buttonState[NES_BUTTON_SELECT]) {
    // Select button pressed or still being held
    if (selectButtonPressTime == 0) {
      // Just pressed, record time
      selectButtonPressTime = currentTime;
    } else if (currentTime - selectButtonPressTime >= RECONNECT_HOLD_TIME) {
      // Held for the required duration, disconnect and start advertising
      Serial.println("Select button held for 5 seconds, disconnecting ...");
      
      // Only perform the action once when threshold is reached
      selectButtonPressTime = 0;
    
      // If connected, disconnect first
      if (joystick->getState() == BLEJoystick::DEVICE_CONNECTED) {
        joystick->disconnect();
      } else {
        // Stop any current advertising
        joystick->stopAdvertising();
      }
    }
  } else {
    // Select button released
    selectButtonPressTime = 0;
  }
}
