#include "NESController.h"

NESController::NESController(uint8_t clkPin1, uint8_t clkPin2, uint8_t latchPin, uint8_t dataPin1, uint8_t dataPin2) 
    : _clkPin1(clkPin1), _clkPin2(clkPin2), _latchPin(latchPin), _dataPin1(dataPin1), _dataPin2(dataPin2), _playerSelection(0), _playerSelectionCallback(nullptr) {
    
    // Initialize button states
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 8; j++) {
            _buttonState[i][j] = false;
            _prevButtonState[i][j] = false;
        }
        _stateChanged[i] = false;
    }
    
    _lastActivityTime = millis();
}

void NESController::begin() {
    // Configure NES controller pins
    pinMode(_clkPin1, OUTPUT);
    pinMode(_clkPin2, OUTPUT);
    pinMode(_latchPin, OUTPUT);
    pinMode(_dataPin1, INPUT_PULLUP);
    pinMode(_dataPin2, INPUT_PULLUP);
}

void NESController::read() {
    // Reset state change flags
    _stateChanged[0] = false;
    _stateChanged[1] = false;
    
    // Latch current button states
    digitalWrite(_latchPin, HIGH);
    delayMicroseconds(12);  // Latch pulse (min 12µs)
    digitalWrite(_latchPin, LOW);
    
    // Read all 8 buttons
    for (int i = 0; i < 8; i++) {
        // NES buttons are active low, so invert the reading
        _buttonState[0][i] = !digitalRead(_dataPin1);
        _buttonState[1][i] = !digitalRead(_dataPin2);
        
        // Clock pulse
        digitalWrite(_clkPin1, HIGH);
        digitalWrite(_clkPin2, HIGH);
        delayMicroseconds(6);
        digitalWrite(_clkPin1, LOW);
        digitalWrite(_clkPin2, LOW);
        delayMicroseconds(6);
    }

    // Check if state has changed
    for (int i = 0; i < 8; i++) {
        if (_buttonState[0][i] != _prevButtonState[0][i]) {
            _stateChanged[0] = true;
            _prevButtonState[0][i] = _buttonState[0][i];
        }
        if (_buttonState[1][i] != _prevButtonState[1][i]) {
            _stateChanged[1] = true;
            _prevButtonState[1][i] = _buttonState[1][i];
        }
    }

    // Update player selection based on button state
    // When all buttons are pressed we assume the player is not selected
    uint8_t previousPlayerSelection = _playerSelection;
    
    if (_allTrue(_buttonState[0], 8) && _playerSelection != 1) {
        _playerSelection = 1; // P2 selected
    } else if (_allTrue(_buttonState[1], 8) && _playerSelection != 0) {
        _playerSelection = 0; // P1 selected
    }
    
    // Call the callback if player selection changed and callback is set
    if (_playerSelection != previousPlayerSelection) {
        // wait a little to debounce the selection change
        delay(100);
        if (_playerSelectionCallback != nullptr) {
            _playerSelectionCallback(_playerSelection);
        }
    }
    
    // Update last activity time if state changed
    if (_stateChanged[0] || _stateChanged[1]) {
        _lastActivityTime = millis();
    }
}

bool NESController::stateChanged(uint8_t player) {
    return (player < 2) ? _stateChanged[player] : false;
}

bool NESController::getButtonState(uint8_t player, uint8_t button) {
    return (player < 2 && button < 8) ? _buttonState[player][button] : false;
}

uint8_t NESController::getPlayerSelection() {
    return _playerSelection;
}

void NESController::setPlayerSelectionCallback(PlayerSelectionCallback callback) {
    _playerSelectionCallback = callback;
}

unsigned long NESController::getLastActivityTime() {
    return _lastActivityTime;
}

void NESController::resetLastActivityTime() {
    _lastActivityTime = millis();
}

uint8_t NESController::getHatDirection(uint8_t player) {
    if (player >= 2) return 0;
    
    uint8_t dpadDirection = 0;
    if (_buttonState[player][BUTTON_UP] && _buttonState[player][BUTTON_RIGHT]) {
        dpadDirection = 2;
    } else if (_buttonState[player][BUTTON_RIGHT] && _buttonState[player][BUTTON_DOWN]) {
        dpadDirection = 4;
    } else if (_buttonState[player][BUTTON_DOWN] && _buttonState[player][BUTTON_LEFT]) {
        dpadDirection = 6;
    } else if (_buttonState[player][BUTTON_LEFT] && _buttonState[player][BUTTON_UP]) {
        dpadDirection = 8;
    } else if (_buttonState[player][BUTTON_UP]) {
        dpadDirection = 1;
    } else if (_buttonState[player][BUTTON_RIGHT]) {
        dpadDirection = 3;
    } else if (_buttonState[player][BUTTON_DOWN]) {
        dpadDirection = 5;
    } else if (_buttonState[player][BUTTON_LEFT]) {
        dpadDirection = 7;
    } else {
        dpadDirection = 0;
    }
    
    return dpadDirection;
}

int8_t NESController::getXAxis(uint8_t player) {
    if (player >= 2) return 0;
    return _buttonState[player][BUTTON_RIGHT] ? 127 : (_buttonState[player][BUTTON_LEFT] ? -127 : 0);
}

int8_t NESController::getYAxis(uint8_t player) {
    if (player >= 2) return 0;
    return _buttonState[player][BUTTON_DOWN] ? 127 : (_buttonState[player][BUTTON_UP] ? -127 : 0);
}

bool NESController::_allTrue(bool array[], int size) {
    for (int i = 0; i < size; i++) {
        if (!array[i]) return false;
    }
    return true;
}
