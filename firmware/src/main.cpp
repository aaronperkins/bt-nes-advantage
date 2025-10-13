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
#include "NESController.h"
#include <Preferences.h>

#define BATTERY_PIN 0
#define BATTERY_VOLTAGE_DIVIDER_R1 100000         // 100k ohm (nominal)
#define BATTERY_VOLTAGE_DIVIDER_R2 100000         // 100k ohm (nominal)
#define BATTERY_VOLTAGE_CALIBRATION_FACTOR 0.928  // Adjust this factor for calibration
#define BATTERY_MIN_VOLTAGE 3.00                  // Minimum battery voltage (in volts)
#define BATTERY_MAX_VOLTAGE 4.20                  // Maximum battery voltage (in volts)
#define BATTERY_SMOOTHING_FACTOR 0.1              // Smoothing factor for EMA (0.0-1.0, lower means more smoothing)
#define BATTERY_SAMPLES 5                         // Number of samples to average for initial reading
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

// Timing constants
#define SLEEP_CYCLE_TIME 5000000        // How long the controller sleeps before waking to check input (microseconds)
#define BATTERY_POLL_TIME 1000          // How often to poll battery status (milliseconds)
#define CONNECTION_IDLE_TIMEOUT 60000   // How long the controller remain unconnected before going to sleep (milliseconds)
#define IDLE_TIMEOUT 300000             // How long the controller remains idle (no buttons presses) before going to sleep (milliseconds)
#define ADVERTISING_TIMEOUT 30000       // How long the controller advertises before giving up (milliseconds)
#define POWER_OFF_HOLD_TIME 5000        // How long the start button must be held to power off (milliseconds)
#define RECONNECT_HOLD_TIME 5000        // How long the select button must be held to reconnect (milliseconds)
#define PROFILE_CHANGE_HOLD_TIME 5000   // How long A+B+Up must be held to change profile (milliseconds)
#define DPAD_MODE_CHANGE_HOLD_TIME 5000 // How long Down+A+B must be held to change dpad mode (milliseconds)

// Directional input modes
enum DirectionalMode {
  DPAD_ONLY = 0,    // Send directional input to dpad only
  AXES_ONLY = 1,    // Send directional input to axes only  
  BOTH = 2          // Send directional input to both dpad and axes
};

const char* directionalModeNames[] = {
  "D-Pad Only",
  "Axes Only", 
  "Both"
};

const uint8_t NUM_DIRECTIONAL_MODES = sizeof(directionalModeNames) / sizeof(directionalModeNames[0]);

// Button mapping profiles
struct ButtonProfile {
  const char* name;
  uint8_t buttonA;      // Joystick button for NES A
  uint8_t buttonB;      // Joystick button for NES B
  uint8_t buttonSelect; // Joystick button for NES SELECT
  uint8_t buttonStart;  // Joystick button for NES START
};

// Define available profiles
const ButtonProfile profiles[] = {
  {"Default", 1, 2, 11, 12},      // Profile 1: Default mapping
  {"Blue Retro", 1, 4, 11, 12}   // Profile 2: Blue Retro mapping
};

const uint8_t NUM_PROFILES = sizeof(profiles) / sizeof(profiles[0]);

// Global state
BLEJoystick* joystick;
NESController* nesController;
Preferences preferences;
int playerSelection = 0; // 0 for P1, 1 for P2
uint8_t currentProfile = 0; // Current button mapping profile
DirectionalMode currentDirectionalMode = DPAD_ONLY; // Current directional input mode
unsigned int batteryLevel = 0;
float batteryVoltage = 0.0;
float smoothedBatteryVoltage = 0.0;
bool isBatteryCharging = false;
bool isBatteryStandBy = false;
bool isExternalPower = false;
unsigned long idleStartTime = 0;
unsigned long advertisingStartTime = 0;
unsigned long lastBatteryCheck = 0;
unsigned long startButtonPressTime = 0;
unsigned long selectButtonPressTime = 0;
unsigned long profileChangeStartTime = 0;
unsigned long dpadModeChangeStartTime = 0;
RTC_DATA_ATTR bool sleeping = false;

// Function prototypes
void joystickStateCallback();
void playerSelectionCallback(uint8_t newPlayer);
void loadProfile();
void saveProfile();
void changeProfile();
void blinkIndicator(uint8_t profile, uint8_t count);
void loadDirectionalMode();
void saveDirectionalMode();
void changeDirectionalMode();
void updateJoystickDirectional(uint8_t player);
void updateJoystickButtons(uint8_t player);
bool readBatteryState();
void checkTimers();
void enterSleepMode();
void reportControllerState();

void setup() {
  // Initialize preferences for persistent storage
  preferences.begin("nes-advantage", false);
  
  // Initialize NES controller
  nesController = new NESController(CLK_PIN_P1, CLK_PIN_P2, LATCH_PIN, DATA_PIN_P1, DATA_PIN_P2);
  nesController->begin();
  nesController->read();
  playerSelection = nesController->getPlayerSelection();
  
  // Initialize battery state
  pinMode(BATTERY_CHARGE_PIN, INPUT_PULLUP);
  pinMode(BATTERY_STANDBY_PIN, INPUT_PULLUP);
  readBatteryState();

  // If sleeping, keep sleeping until the start button is held or battery is charging
  if (sleeping) {
    if (nesController->getButtonState(playerSelection, NESController::BUTTON_START)
          || isExternalPower) {
      sleeping = false;
    }
    else {
      enterSleepMode();   
    }
  }

  // Initialize serial for debugging
  Serial.begin(9600);

  // Set up NES controller player selection callback
  nesController->setPlayerSelectionCallback(playerSelectionCallback);

  // Configure LED pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(BLUE_LED_PIN, HIGH);

  // Load saved profile
  loadProfile();
  
  // Load saved directional mode
  loadDirectionalMode();
    
  // Initialize timers
  idleStartTime = millis();
  advertisingStartTime = millis();
  lastBatteryCheck = millis();

  // Initialize the BLE joystick and begin advertising
  joystick = new BLEJoystick("NES Advantage", "Cajun Panda's Retro Gaming", 0x42CA, 0x42CD, 2);
  joystick->setStateChangeCallback(joystickStateCallback);
  joystick->start();
  joystick->startAdvertising();
}

void loop() {
  nesController->read();
 
  // Update joystick if state changed for either player
  bool anyStateChanged = nesController->stateChanged(0) || nesController->stateChanged(1);
  
  if (anyStateChanged) {

    reportControllerState();

    // If connected, update BLE Joystick state
    if (joystick->getState() == BLEJoystick::DEVICE_CONNECTED) {
      
      // Update joystick state based on current player selection
      updateJoystickDirectional(playerSelection);
      updateJoystickButtons(playerSelection);
      joystick->notifyPlayerHIDReport(playerSelection);
      
    // else begin advertising
    } else if (joystick->getState() == BLEJoystick::DEVICE_IDLE 
                && selectButtonPressTime == 0) {
      joystick->startAdvertising();
    }
  }
      
  checkTimers();
}

void reportControllerState()
{
  // Report both players
  for (int player = 0; player < 2; player++) {
    Serial.print("NES State P");
    Serial.print(player + 1);
    Serial.print(": ");
    for (int i = 0; i < 8; i++)
    {
      Serial.print(nesController->getButtonState(player, i) ? "1" : "0");
    }
    Serial.println();
  }
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

void playerSelectionCallback(uint8_t newPlayer) {
  // Clear previous player state
  joystick->setPlayerButtons(playerSelection,
    false, false, false, false,
    false, false, false, false,
    false, false, false, false
  );
  joystick->setPlayerHat(playerSelection, 0);
  joystick->setPlayerAxes(playerSelection, 0, 0, 0, 0, 0, 0, 0, 0);
  joystick->notifyPlayerHIDReport(playerSelection);

  // set new player selection
  playerSelection = newPlayer;
  Serial.print("Player selection changed to P");
  Serial.println(playerSelection + 1);
}

void checkTimers() {
  unsigned long currentTime = millis();
  
  // Check battery state periodically
  if (currentTime - lastBatteryCheck > BATTERY_POLL_TIME) {
    if (readBatteryState()) {
      Serial.print("Battery level: ");
      Serial.print(batteryLevel);
      Serial.print("% - Battery voltage: ");
      Serial.print(batteryVoltage, 3);
      Serial.print("V - Charging: ");
      Serial.print(isBatteryCharging ? "Yes" : "No");
      Serial.print(" - Standby: ");
      Serial.println(isBatteryStandBy ? "Yes" : "No");
      
      // If connected, update BLE Joystick battery level
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

  if (isBatteryCharging && !isBatteryStandBy) {
    // Blinking green LED when charging (toggle every 500ms)
    digitalWrite(GREEN_LED_PIN, (currentTime / 500) % 2 == 0);
  } else if (isBatteryStandBy) {
    // Solid green LED when fully charged
    digitalWrite(GREEN_LED_PIN, LOW); // LED is active LOW
  } else {
    // No charging activity - LED off
    digitalWrite(GREEN_LED_PIN, HIGH); // LED is active LOW
  }

  // Check for connection inactivity - put device to sleep
  if (joystick->getState() == BLEJoystick::DEVICE_IDLE
      && !isExternalPower
      && currentTime - idleStartTime > CONNECTION_IDLE_TIMEOUT) {
    Serial.println("Connection timeout, going to sleep...");
    enterSleepMode();
  } else if (isExternalPower) {
    // Reset idle timer if charging
    idleStartTime = currentTime;
  }

  // Check for inactivity - put device to sleep if no button press 
  if (currentTime - nesController->getLastActivityTime() > IDLE_TIMEOUT
      && !isExternalPower) {
    Serial.println("No button activity, going to sleep...");
    enterSleepMode();
  } else if (isExternalPower) {
    // Reset timer if charging
    nesController->resetLastActivityTime();
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
  if (nesController->getButtonState(playerSelection, NESController::BUTTON_START)) {
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
  if (nesController->getButtonState(playerSelection, NESController::BUTTON_SELECT)) {
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
  
  // Check for profile change combination (A + B + Up held for 5 seconds)
  if (nesController->getButtonState(playerSelection, NESController::BUTTON_A) &&
      nesController->getButtonState(playerSelection, NESController::BUTTON_B) &&
      nesController->getButtonState(playerSelection, NESController::BUTTON_UP)) {
    // Profile change combination pressed or still being held
    if (profileChangeStartTime == 0) {
      // Just pressed, record time
      profileChangeStartTime = currentTime;
    } else if (currentTime - profileChangeStartTime >= PROFILE_CHANGE_HOLD_TIME) {
      // Held for the required duration, change profile
      Serial.println("A+B+Up held for 5 seconds, changing profile...");
      changeProfile();
      
      // Only perform the action once when threshold is reached
      profileChangeStartTime = 0;
    }
  } else {
    // Profile change combination released
    profileChangeStartTime = 0;
  }
  
  // Check for directional mode change combination (Down + A + B held for 5 seconds)
  if (nesController->getButtonState(playerSelection, NESController::BUTTON_DOWN) &&
      nesController->getButtonState(playerSelection, NESController::BUTTON_A) &&
      nesController->getButtonState(playerSelection, NESController::BUTTON_B)) {
    // Directional mode change combination pressed or still being held
    if (dpadModeChangeStartTime == 0) {
      // Just pressed, record time
      dpadModeChangeStartTime = currentTime;
    } else if (currentTime - dpadModeChangeStartTime >= DPAD_MODE_CHANGE_HOLD_TIME) {
      // Held for the required duration, change directional mode
      Serial.println("Down+A+B held for 5 seconds, changing directional mode...");
      changeDirectionalMode();
      
      // Only perform the action once when threshold is reached
      dpadModeChangeStartTime = 0;
    }
  } else {
    // Directional mode change combination released
    dpadModeChangeStartTime = 0;
  }
}

bool readBatteryState() {
  bool wasCharging = isBatteryCharging;
  bool wasFullyCharged = isBatteryStandBy;
  isBatteryCharging = digitalRead(BATTERY_CHARGE_PIN) == LOW;
  isBatteryStandBy = digitalRead(BATTERY_STANDBY_PIN) == LOW;
  isExternalPower = isBatteryCharging || isBatteryStandBy;
  // If charging state changed, reset smoothed voltage
  if (wasCharging != isBatteryCharging || wasFullyCharged != isBatteryStandBy)
  {
    smoothedBatteryVoltage = 0.0; // Reset smoothed voltage when charging state changes
  }
  
  // Read raw ADC value and convert to voltage
  float currentVoltage = (analogRead(BATTERY_PIN) / 4095.0) * 3.3;
  currentVoltage = (currentVoltage * (BATTERY_VOLTAGE_DIVIDER_R1 + BATTERY_VOLTAGE_DIVIDER_R2)) / BATTERY_VOLTAGE_DIVIDER_R2;
  currentVoltage *= BATTERY_VOLTAGE_CALIBRATION_FACTOR;
  
  // Apply exponential moving average filter for smoothing
  if (smoothedBatteryVoltage == 0.0) {
    // First reading or after reset
    smoothedBatteryVoltage = currentVoltage;
  } else {
    // Apply EMA filter: smoothed = (1-alpha) * smoothed + alpha * current
    smoothedBatteryVoltage = (1 - BATTERY_SMOOTHING_FACTOR) * smoothedBatteryVoltage + 
                            BATTERY_SMOOTHING_FACTOR * currentVoltage;
  }
  
  // Update the global voltage variable with the smoothed value
  batteryVoltage = smoothedBatteryVoltage;
  
  // Calculate percentage based on smoothed voltage
  unsigned int prevBatteryLevel = batteryLevel;
  batteryLevel = (unsigned int)(((smoothedBatteryVoltage - BATTERY_MIN_VOLTAGE) / 
                         (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) * 100.0);
  if (batteryLevel > 100) batteryLevel = 100;

  return wasCharging != isBatteryCharging || 
         wasFullyCharged != isBatteryStandBy || 
         prevBatteryLevel != batteryLevel;
}

void enterSleepMode() {
  Serial.flush();
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(BLUE_LED_PIN, HIGH);
  sleeping = true;
  esp_sleep_enable_timer_wakeup(SLEEP_CYCLE_TIME);
  esp_deep_sleep_start();
}

void loadProfile() {
  currentProfile = preferences.getUChar("profile", 0);
  if (currentProfile >= NUM_PROFILES) {
    currentProfile = 0; // Default to first profile if invalid
  }
  
  Serial.print("Loaded profile: ");
  Serial.print(currentProfile + 1);
  Serial.print(" (");
  Serial.print(profiles[currentProfile].name);
  Serial.println(")");
}

void saveProfile() {
  preferences.putUChar("profile", currentProfile);
  Serial.print("Saved profile: ");
  Serial.print(currentProfile + 1);
  Serial.print(" (");
  Serial.print(profiles[currentProfile].name);
  Serial.println(")");
}

void changeProfile() {
  currentProfile = (currentProfile + 1) % NUM_PROFILES;
  saveProfile();
  
  Serial.print("Changed to profile: ");
  Serial.print(currentProfile + 1);
  Serial.print(" (");
  Serial.print(profiles[currentProfile].name);
  Serial.println(")");
  
  // Blink LED to indicate new profile
  blinkIndicator(RED_LED_PIN, currentProfile + 1);
}

void blinkIndicator(uint8_t pin, uint8_t count) {
  // Turn LED first
  digitalWrite(pin, HIGH);
  
  delay(500); // Initial pause
  
  // Blink LED the number of times corresponding to the profile
  for (uint8_t i = 0; i < count; i++) {
    digitalWrite(pin, LOW);  // Turn on LED (active low)
    delay(300);
    digitalWrite(pin, HIGH); // Turn off LED
    delay(300);
  }
  
  delay(500); // Final pause
}

void updateJoystickButtons(uint8_t player) {
  // Get current profile
  const ButtonProfile& profile = profiles[currentProfile];
  
  // Create button array (12 buttons total)
  bool buttons[12] = {false};
  
  // Map NES buttons to joystick buttons based on current profile
  buttons[profile.buttonA - 1] = nesController->getButtonState(player, NESController::BUTTON_A);
  buttons[profile.buttonB - 1] = nesController->getButtonState(player, NESController::BUTTON_B);
  buttons[profile.buttonSelect - 1] = nesController->getButtonState(player, NESController::BUTTON_SELECT);
  buttons[profile.buttonStart - 1] = nesController->getButtonState(player, NESController::BUTTON_START);
  
  // Set all buttons at once for the specific player
  joystick->setPlayerButtons(player,
    buttons[0], buttons[1], buttons[2], buttons[3],
    buttons[4], buttons[5], buttons[6], buttons[7],
    buttons[8], buttons[9], buttons[10], buttons[11]
  );
}

void loadDirectionalMode() {
  uint8_t savedMode = preferences.getUChar("directionalMode", DPAD_ONLY);
  if (savedMode >= NUM_DIRECTIONAL_MODES) {
    savedMode = DPAD_ONLY; // Default to dpad only if invalid
  }
  currentDirectionalMode = (DirectionalMode)savedMode;
  
  Serial.print("Loaded directional mode: ");
  Serial.print((uint8_t)currentDirectionalMode + 1);
  Serial.print(" (");
  Serial.print(directionalModeNames[currentDirectionalMode]);
  Serial.println(")");
}

void saveDirectionalMode() {
  preferences.putUChar("directionalMode", (uint8_t)currentDirectionalMode);
  Serial.print("Saved directional mode: ");
  Serial.print((uint8_t)currentDirectionalMode + 1);
  Serial.print(" (");
  Serial.print(directionalModeNames[currentDirectionalMode]);
  Serial.println(")");
}

void changeDirectionalMode() {
  currentDirectionalMode = (DirectionalMode)(((uint8_t)currentDirectionalMode + 1) % NUM_DIRECTIONAL_MODES);
  saveDirectionalMode();
  
  Serial.print("Changed to directional mode: ");
  Serial.print((uint8_t)currentDirectionalMode + 1);
  Serial.print(" (");
  Serial.print(directionalModeNames[currentDirectionalMode]);
  Serial.println(")");
  
  // Blink LED to indicate new directional mode
  blinkIndicator(RED_LED_PIN, (uint8_t)currentDirectionalMode + 1);
}

void updateJoystickDirectional(uint8_t player) {
  // Get directional values from controller for the specific player
  uint8_t dpadDirection = nesController->getHatDirection(player);
  int8_t x = nesController->getXAxis(player);
  int8_t y = nesController->getYAxis(player);
  
  // Apply directional input based on current mode for the specific player
  switch (currentDirectionalMode) {
    case DPAD_ONLY:
      joystick->setPlayerHat(player, dpadDirection);
      joystick->setPlayerAxes(player, 0, 0, 0, 0, 0, 0, 0, 0); // Clear axes
      break;
      
    case AXES_ONLY:
      joystick->setPlayerHat(player, 0); // Clear hat/dpad
      joystick->setPlayerAxes(player, x, y, 0, 0, 0, 0, 0, 0);
      break;
      
    case BOTH:
      joystick->setPlayerHat(player, dpadDirection);
      joystick->setPlayerAxes(player, x, y, 0, 0, 0, 0, 0, 0);
      break;
  }
}
