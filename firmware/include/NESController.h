#ifndef NES_CONTROLLER_H
#define NES_CONTROLLER_H

#include <Arduino.h>

// Forward declaration for callback function type
typedef void (*PlayerSelectionCallback)(uint8_t newPlayer);

class NESController {
public:
    // Button mapping (NES polling order)
    static const uint8_t BUTTON_A = 0;
    static const uint8_t BUTTON_B = 1;
    static const uint8_t BUTTON_SELECT = 2;
    static const uint8_t BUTTON_START = 3;
    static const uint8_t BUTTON_UP = 4;
    static const uint8_t BUTTON_DOWN = 5;
    static const uint8_t BUTTON_LEFT = 6;
    static const uint8_t BUTTON_RIGHT = 7;

    // Constructor
    NESController(uint8_t clkPin1, uint8_t clkPin2, uint8_t latchPin, uint8_t dataPin1, uint8_t dataPin2);

    // Initialize controller pins
    void begin();
    
    // Read the controller state
    void read();
    
    // Check if state changed for a player
    bool stateChanged(uint8_t player);
    
    // Get button state for a player and button
    bool getButtonState(uint8_t player, uint8_t button);
    
    // Get the current player selection
    uint8_t getPlayerSelection();
    
    // Set callback for player selection changes
    void setPlayerSelectionCallback(PlayerSelectionCallback callback);
    
    // Update last activity time
    unsigned long getLastActivityTime();

    // Get hat direction based on current button states
    uint8_t getHatDirection(uint8_t player);
    
    // Get X axis value based on button state
    int8_t getXAxis(uint8_t player);
    
    // Get Y axis value based on button state
    int8_t getYAxis(uint8_t player);

private:
    // Pin definitions
    uint8_t _clkPin1;
    uint8_t _clkPin2;
    uint8_t _latchPin;
    uint8_t _dataPin1;
    uint8_t _dataPin2;
    
    // Button state arrays
    bool _buttonState[2][8];
    bool _prevButtonState[2][8];
    bool _stateChanged[2];
    
    // Player selection (0 for P1, 1 for P2)
    uint8_t _playerSelection;
    
    // Last activity time
    unsigned long _lastActivityTime;
    
    // Player selection callback
    PlayerSelectionCallback _playerSelectionCallback;
    
    // Check if all values in an array are true
    bool _allTrue(bool array[], int size);
};

#endif // NES_CONTROLLER_H
