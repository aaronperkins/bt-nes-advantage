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
#define BATTERY_VOLTAGE_DIVIDER_R1 100000         // 100k ohm (nominal)
#define BATTERY_VOLTAGE_DIVIDER_R2 100000         // 100k ohm (nominal)
#define BATTERY_VOLTAGE_CALIBRATION_FACTOR 0.928  // Adjust this factor for calibration
#define BATTERY_MIN_VOLTAGE 3.00                  // Minimum battery voltage (in volts)
#define BATTERY_MAX_VOLTAGE 4.20                  // Maximum battery voltage (in volts)
#define RED_LED_PIN 7
#define BLUE_LED_PIN 8
#define GREEN_LED_PIN 9
#define BATTERY_CHARGE_PIN 10
#define BATTERY_STANDBY_PIN 20

// NES Pin Mapping
#define CLK_PIN_P1 3
#define CLK_PIN_P2 5
#define LATCH_PIN 2
#define DATA_PIN_P1 4
#define DATA_PIN_P2 6

// Button mapping (NES polling order)
#define NES_BUTTON_A 0
#define NES_BUTTON_B 1
#define NES_BUTTON_SELECT 2
#define NES_BUTTON_START 3
#define NES_BUTTON_UP 4
#define NES_BUTTON_DOWN 5
#define NES_BUTTON_LEFT 6
#define NES_BUTTON_RIGHT 7

#define SLEEP_TIMEOUT 5000000           // microseconds
#define CONNECTION_IDLE_TIMEOUT 60000   // milliseconds
#define IDLE_TIMEOUT 300000             // milliseconds
#define ADVERTISING_TIMEOUT 30000       // milliseconds
#define POWER_OFF_HOLD_TIME 5000        // milliseconds
#define RECONNECT_HOLD_TIME 5000        // milliseconds

// Global state
BLEJoystick* joystick;
bool buttonState[8] = {false};
bool prevButtonState[8] = {false};
unsigned int batteryLevel = 0;
float batteryVoltage = 0.0;
unsigned int prevBatteryLevel = 0;
unsigned long idleStartTime = 0;
unsigned long lastActivityTime = 0;
unsigned long advertisingStartTime = 0;
unsigned long lastBatteryCheck = 0;
unsigned long startButtonPressTime = 0;
unsigned long selectButtonPressTime = 0;
RTC_DATA_ATTR bool sleeping = false;

// Function prototypes
void joystickStateCallback();
int readBatteryLevel();
bool readNESController();
void checkTimers();
void enterSleepMode();

void setup() {
  // Configure NES controller pins
  // We need these pins to wake from sleep
  pinMode(CLK_PIN_P1, OUTPUT);
  pinMode(CLK_PIN_P2, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(DATA_PIN_P1, INPUT_PULLUP);
  pinMode(DATA_PIN_P2, INPUT_PULLUP);

  // If sleeping, keep sleeping until the start button is held
  if (sleeping) {
    readNESController();
    if (!buttonState[NES_BUTTON_START]) {
      enterSleepMode();
    }
    else {
      sleeping = false;
    }
  }

  // Configure other pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(BATTERY_CHARGE_PIN, INPUT_PULLUP);
  pinMode(BATTERY_STANDBY_PIN, INPUT_PULLUP);

  // Set initial pin states
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(BLUE_LED_PIN, HIGH);

  // Initialize serial for debugging
  Serial.begin(9600);
    
  // Initialize timers
  idleStartTime = millis();
  lastActivityTime = millis();
  advertisingStartTime = millis();
  lastBatteryCheck = millis();

  // Update battery level
  batteryLevel = readBatteryLevel();

  // Initialize the joystick and begin advertising
  joystick = new BLEJoystick("NES Advantage", "Cajun Panda's Retro Gaming");
  joystick->setStateChangeCallback(joystickStateCallback);
  joystick->start();
  joystick->startAdvertising();
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
    
    // If connected, update joystick state
    if (joystick->getState() == BLEJoystick::DEVICE_CONNECTED) {
      joystick->setHat(dpadDirection);
      joystick->setAxes(x, y, 0, 0, 0, 0, 0, 0); // Set axes values
      joystick->setButtons(
        buttonState[NES_BUTTON_A], false,   // buttons 1-2
        false, buttonState[NES_BUTTON_B],   // buttons 3-4
        false, false,                       // buttons 5-6
        false, false,                       // buttons 7-8
        false, false,                       // buttons 9-10
        buttonState[NES_BUTTON_SELECT],     // Select button
        buttonState[NES_BUTTON_START]       // Start button
      );
      joystick->notifyHIDReport();
      lastActivityTime = millis();
    // else begin advertising
    } else if (joystick->getState() == BLEJoystick::DEVICE_IDLE 
                && selectButtonPressTime == 0) {
      joystick->startAdvertising();
    }
  }
      
  checkTimers();
}

void joystickStateCallback() {
  switch (joystick->getState()) {
    case BLEJoystick::DEVICE_IDLE:
      Serial.println("Device idle.");
      digitalWrite(BLUE_LED_PIN, HIGH);
      idleStartTime = millis();
      break;
      
    case BLEJoystick::DEVICE_ADVERTISING:
      Serial.println("Device advertising.");
      advertisingStartTime = millis();
      break;
      
    case BLEJoystick::DEVICE_CONNECTED:
      Serial.println("Device connected.");
      digitalWrite(BLUE_LED_PIN, LOW);
      // Send initial battery level
      joystick->setBatteryLevel(batteryLevel);
      joystick->notifyBatteryLevel();
      break;
      
    default:
      break;
  }
}

void checkTimers() {
  unsigned long currentTime = millis();
  
  // Check battery level periodically
  if (currentTime - lastBatteryCheck > 5000) {
    // Update battery level
    batteryLevel = readBatteryLevel();
    if (batteryLevel != prevBatteryLevel) {
      Serial.print("Battery level: ");
      Serial.print(batteryLevel);
      Serial.print(" Battery voltage: ");
      Serial.println(batteryVoltage);
      prevBatteryLevel = batteryLevel;
      if (joystick->getState() == BLEJoystick::DEVICE_CONNECTED) {
        joystick->setBatteryLevel(batteryLevel);
        joystick->notifyBatteryLevel();
      }
    }  
    lastBatteryCheck = millis();
  }

  // Check if battery level is low
  if (batteryLevel < 20) {
    digitalWrite(RED_LED_PIN, (currentTime / 500) % 2 == 0);
  } else {
    digitalWrite(RED_LED_PIN, HIGH); // Turn off red LED
  }

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

  // Check for connection inactivity - put device to sleep
  if (joystick->getState() == BLEJoystick::DEVICE_IDLE && 
      currentTime - idleStartTime > CONNECTION_IDLE_TIMEOUT) {
    Serial.println("Connection timeout, going to sleep...");
    enterSleepMode();
  }

  // Check for inactivity - put device to sleep if no button press 
  if (currentTime - lastActivityTime > IDLE_TIMEOUT) {
    Serial.println("No button activity, going to sleep...");
    enterSleepMode();
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
      enterSleepMode();
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

int readBatteryLevel() {
  int raw = analogRead(BATTERY_PIN);
  batteryVoltage = (raw / 4095.0) * 3.3; 
  batteryVoltage = (batteryVoltage * (BATTERY_VOLTAGE_DIVIDER_R1 + BATTERY_VOLTAGE_DIVIDER_R2)) / BATTERY_VOLTAGE_DIVIDER_R2;
  batteryVoltage *= BATTERY_VOLTAGE_CALIBRATION_FACTOR;
  int percentage = (int)(((batteryVoltage - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) * 100.0);
  if (percentage > 100) percentage = 100;
  if (percentage < 0) percentage = 0;
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
    buttonState[i] = !digitalRead(DATA_PIN_P1);
    
    // Clock pulse
    digitalWrite(CLK_PIN_P1, HIGH);
    digitalWrite(CLK_PIN_P2, HIGH);
    delayMicroseconds(6);
    digitalWrite(CLK_PIN_P1, LOW);
    digitalWrite(CLK_PIN_P2, LOW);
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

  // Update last activity time if state changed
  if (stateChanged) {
    lastActivityTime = millis();
  }

  return stateChanged;
}

void enterSleepMode() {
  Serial.flush();
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(BLUE_LED_PIN, HIGH);
  sleeping = true;
  esp_sleep_enable_timer_wakeup(SLEEP_TIMEOUT);
  esp_deep_sleep_start();
}
