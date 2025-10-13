#ifndef BLE_JOYSTICK_H
#define BLE_JOYSTICK_H

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLEHIDDevice.h>
#include <functional>

class BLEJoystick {
public:
    // Device states
    static const uint8_t DEVICE_STOPPED = 0;
    static const uint8_t DEVICE_IDLE = 1;
    static const uint8_t DEVICE_ADVERTISING = 2;
    static const uint8_t DEVICE_CONNECTED = 3;

    BLEJoystick(std::string deviceName, std::string manufacturerName, 
        uint16_t vendorId, uint16_t productId, uint8_t numDevices);
    
    // Destructor
    ~BLEJoystick();
    
    // Device control methods
    void start();
    void stop();
    void startAdvertising();
    void stopAdvertising();
    void setAdvertiseOnDisconnect(bool advertise);
    void disconnect();
    
    // Player-specific input state setters
    void setPlayerButtons(uint8_t player, bool b1 = false, bool b2 = false, bool b3 = false, bool b4 = false, 
                         bool b5 = false, bool b6 = false, bool b7 = false, bool b8 = false,
                         bool b9 = false, bool b10 = false, bool b11 = false, bool b12 = false);
    void setPlayerAxes(uint8_t player, int16_t x = 0, int16_t y = 0, int16_t z = 0, int16_t rZ = 0, 
                      int16_t rX = 0, int16_t rY = 0, int16_t slider1 = 0, int16_t slider2 = 0);
    void setPlayerHat(uint8_t player, uint8_t hat);
    void notifyPlayerHIDReport(uint8_t player);
    
    // Legacy input state setters (for backwards compatibility)
    void setButtons(bool b1 = false, bool b2 = false, bool b3 = false, bool b4 = false, 
                    bool b5 = false, bool b6 = false, bool b7 = false, bool b8 = false,
                    bool b9 = false, bool b10 = false, bool b11 = false, bool b12 = false);
    void setAxes(int16_t x = 0, int16_t y = 0, int16_t z = 0, int16_t rZ = 0, 
                int16_t rX = 0, int16_t rY = 0, int16_t slider1 = 0, int16_t slider2 = 0);
    void setHat(uint8_t hat);
    
    // Battery level methods
    void setBatteryLevel(uint8_t level);
    void notifyBatteryLevel();
    
    // State methods
    uint8_t getState() const;
    void setStateChangeCallback(std::function<void()> callback);
    
    // Utility methods
    uint8_t getNumDevices() const;
    
private:
    // BLE objects
    NimBLEServer* pServer;
    NimBLEHIDDevice** pHidDevice;        // Dynamic array of HID devices
    NimBLECharacteristic** pInputCharacteristic;  // Dynamic array of input characteristics
    NimBLECharacteristic* pBatteryCharacteristic;
    
    // Device configuration
    uint8_t numDevices;                  // Number of players/joysticks
    uint8_t deviceState;
    uint8_t batteryLevel;
    std::function<void()> stateChangeCallback;
    
    // HID report data (dynamic arrays)
    uint8_t** buttons;      // [player][byte] - 12 buttons per player (12 bits)
    int16_t** axes;         // [player][axis] - 8 axes per player (X, Y, Z, RZ, RX, RY, Slider1, Slider2)
    uint8_t* hat;           // [player] - hat direction (0-8) per player
    
    // HID report descriptor (single joystick)
    static const uint8_t hidReportDescriptor[];
    
    // Connection callbacks
    class ServerCallbacks : public NimBLEServerCallbacks {
    public:
        ServerCallbacks(BLEJoystick* device);
        void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc);
        void onDisconnect(NimBLEServer* pServer);
        
    private:
        BLEJoystick* device;
    };
    
    void updateDeviceState(uint8_t newState);
};

#endif // BLE_JOYSTICK_H
