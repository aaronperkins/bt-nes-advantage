// BLEJoystick.h
// Bluetooth HID NES Advantage Joystick
// Copyright (C) 2025 Aaron Perkins

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

    // Constructor
    BLEJoystick(std::string deviceName);
    
    // Device control methods
    void start();
    void stop();
    void startAdvertising();
    void stopAdvertising();
    void disconnect();
    
    // Input state setters
    void setButtons(bool b1 = false, bool b2 = false, bool b3 = false, bool b4 = false, 
                    bool b5 = false, bool b6 = false, bool b7 = false, bool b8 = false,
                    bool b9 = false, bool b10 = false, bool b11 = false, bool b12 = false);
    void setAxes(int16_t x = 0, int16_t y = 0, int16_t z = 0, int16_t rZ = 0, 
                int16_t rX = 0, int16_t rY = 0, int16_t slider1 = 0, int16_t slider2 = 0);
    void setHat(uint8_t hat);
    void notifyHIDReport();
    
    // Battery level methods
    void setBatteryLevel(uint8_t level);
    void notifyBatteryLevel();
    
    // State methods
    uint8_t getState() const;
    void setStateChangeCallback(std::function<void()> callback);
    
private:
    // BLE objects
    NimBLEServer* pServer;
    NimBLEHIDDevice* pHidDevice;
    NimBLECharacteristic* pInputCharacteristic;
    NimBLECharacteristic* pBatteryCharacteristic;
    
    // Device state
    uint8_t deviceState;
    uint8_t batteryLevel;
    std::function<void()> stateChangeCallback;
    
    // HID report data
    uint8_t buttons[2];         // 12 buttons (12 bits)
    int16_t axes[8];            // 8 axes (X, Y, Z, RZ, RX, RY, Slider1, Slider2)
    uint8_t hat;                // hat direction (0-8)
    
    // HID report descriptor
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
