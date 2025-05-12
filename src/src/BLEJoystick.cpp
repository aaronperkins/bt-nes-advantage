// BLEJoystick.cpp
// Bluetooth HID NES Advantage Joystick
// Copyright (C) 2025 Aaron Perkins

#include "BLEJoystick.h"
#include <Arduino.h>

// HID Report Descriptor for a joystick
const uint8_t BLEJoystick::hidReportDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x05,        // Usage (Gamepad)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        // Report ID (1)
    
    // Buttons (12 buttons)
    0x05, 0x09,        // Usage Page (Button)
    0x19, 0x01,        // Usage Minimum (Button 1)
    0x29, 0x0C,        // Usage Maximum (Button 12)
    0x15, 0x00,        // Logical Minimum (0)
    0x25, 0x01,        // Logical Maximum (1)
    0x75, 0x01,        // Report Size (1)
    0x95, 0x0C,        // Report Count (12)
    0x81, 0x02,        // Input (Data, Variable, Absolute)
    
    // Padding (4 bits to make full byte)
    0x75, 0x01,        // Report Size (1)
    0x95, 0x04,        // Report Count (4)
    0x81, 0x03,        // Input (Constant, Variable, Absolute)
    
    // Hat switch
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x39,        // Usage (Hat Switch)
    0x15, 0x01,        // Logical Minimum (1)
    0x25, 0x08,        // Logical Maximum (8)
    0x35, 0x00,        // Physical Minimum (0)
    0x46, 0x3B, 0x01,  // Physical Maximum (315)
    0x65, 0x14,        // Unit (Degrees)
    0x75, 0x04,        // Report Size (4)
    0x95, 0x01,        // Report Count (1)
    0x81, 0x02,        // Input (Data, Variable, Absolute)
    
    // Padding (4 bits to make full byte)
    0x75, 0x01,        // Report Size (1)
    0x95, 0x04,        // Report Count (4)
    0x81, 0x03,        // Input (Constant, Variable, Absolute)
    
    // X, Y axes
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x01,        // Usage (Pointer)
    0xA1, 0x00,        // Collection (Physical)
    0x09, 0x30,        // Usage (X)
    0x09, 0x31,        // Usage (Y)
    0x15, 0x81,        // Logical Minimum (-127)
    0x25, 0x7F,        // Logical Maximum (127)
    0x75, 0x08,        // Report Size (8)
    0x95, 0x02,        // Report Count (2)
    0x81, 0x02,        // Input (Data, Variable, Absolute)
    0xC0,              // End Collection
    
    0xC0               // End Collection
};

// Constructor implementation
BLEJoystick::BLEJoystick(std::string deviceName) {
    deviceState = DEVICE_STOPPED;
    batteryLevel = 100;
    
    // Initialize HID report data
    memset(buttons, 0, sizeof(buttons));
    memset(axes, 0, sizeof(axes));
    hat = 0;
    
    // Initialize BLE
    NimBLEDevice::init(deviceName);
    
    // Set security
    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    
    // Create server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));
    
    // Create HID device
    pHidDevice = new NimBLEHIDDevice(pServer);
    pHidDevice->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
    
    // Create input characteristic
    pInputCharacteristic = pHidDevice->inputReport(1);
    
    // Create battery service
    pBatteryCharacteristic = pHidDevice->batteryLevel();
    
    // Start device
    pHidDevice->startServices();
    
    // Set device information
    pHidDevice->manufacturer()->setValue("NES Advantage BT");
    pHidDevice->pnp(0x01, 0x02E5, 0xABCD, 0x0110);
    pHidDevice->hidInfo(0x00, 0x01);
    
    // Set initial battery level
    setBatteryLevel(100);
}

// Start the BLE device
void BLEJoystick::start() {
    if (deviceState == DEVICE_STOPPED) {
        updateDeviceState(DEVICE_IDLE);
    }
}

// Stop the BLE device
void BLEJoystick::stop() {
    if (deviceState != DEVICE_STOPPED) {
        stopAdvertising();
        updateDeviceState(DEVICE_STOPPED);
    }
}

// Start advertising
void BLEJoystick::startAdvertising() {
    if (deviceState == DEVICE_IDLE) {
        NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
        pAdvertising->setAppearance(HID_GAMEPAD);
        pAdvertising->addServiceUUID(pHidDevice->hidService()->getUUID());
        pAdvertising->setScanResponse(true);
        pAdvertising->start();
        
        updateDeviceState(DEVICE_ADVERTISING);
        Serial.println("Started advertising");
    }
}

// Stop advertising
void BLEJoystick::stopAdvertising() {
    if (deviceState == DEVICE_ADVERTISING) {
        NimBLEDevice::getAdvertising()->stop();
        updateDeviceState(DEVICE_IDLE);
        Serial.println("Stopped advertising");
    }
}

// Set button states
void BLEJoystick::setButtons(bool b1, bool b2, bool b3, bool b4, bool b5, bool b6,
                            bool b7, bool b8, bool b9, bool b10, bool b11, bool b12) {
    // First byte contains buttons 1-8
    buttons[0] = 0;
    if (b1) buttons[0] |= (1 << 0);
    if (b2) buttons[0] |= (1 << 1);
    if (b3) buttons[0] |= (1 << 2);
    if (b4) buttons[0] |= (1 << 3);
    if (b5) buttons[0] |= (1 << 4);
    if (b6) buttons[0] |= (1 << 5);
    if (b7) buttons[0] |= (1 << 6);
    if (b8) buttons[0] |= (1 << 7);
    
    // Second byte contains buttons 9-12 (and padding)
    buttons[1] = 0;
    if (b9) buttons[1] |= (1 << 0);
    if (b10) buttons[1] |= (1 << 1);
    if (b11) buttons[1] |= (1 << 2);
    if (b12) buttons[1] |= (1 << 3);
}

// Set axis values
void BLEJoystick::setAxes(int16_t x, int16_t y, int16_t z, int16_t rZ, 
                          int16_t rX, int16_t rY, int16_t slider1, int16_t slider2) {
    axes[0] = x;
    axes[1] = y;
    axes[2] = z;
    axes[3] = rZ;
    axes[4] = rX;
    axes[5] = rY;
    axes[6] = slider1;
    axes[7] = slider2;
}

// Set hat direction
void BLEJoystick::setHat(uint8_t hatDirection) {
    hat = hatDirection <= 8 ? hatDirection : 0;
}

// Notify HID report to connected client
void BLEJoystick::notifyHIDReport() {
    if (deviceState == DEVICE_CONNECTED) {
        uint8_t report[5];  // Increased size to include Y axis
        
        report[0] = buttons[0];
        report[1] = buttons[1];
        report[2] = (hat & 0x0F);
        report[3] = axes[0]; // X axis
        report[4] = axes[1]; // Y axis - added
        
        // Debug output in a human-readable format
        Serial.println("=== HID REPORT DEBUG ===");
        
        // Buttons 1-8 (first byte)
        Serial.println("Buttons 1-8:");
        for (int i = 0; i < 8; i++) {
            Serial.print("  Button ");
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.println((report[0] & (1 << i)) ? "PRESSED" : "released");
        }
        
        // Buttons 9-12 (second byte, first 4 bits)
        Serial.println("Buttons 9-12:");
        for (int i = 0; i < 4; i++) {
            Serial.print("  Button ");
            Serial.print(i + 9);
            Serial.print(": ");
            Serial.println((report[1] & (1 << i)) ? "PRESSED" : "released");
        }
        
        // Hat switch (third byte)
        Serial.print("Hat Direction: ");
        switch(report[2]) {
            case 0: Serial.println("CENTERED"); break;
            case 1: Serial.println("UP"); break;
            case 2: Serial.println("UP-RIGHT"); break;
            case 3: Serial.println("RIGHT"); break;
            case 4: Serial.println("DOWN-RIGHT"); break;
            case 5: Serial.println("DOWN"); break;
            case 6: Serial.println("DOWN-LEFT"); break;
            case 7: Serial.println("LEFT"); break;
            case 8: Serial.println("UP-LEFT"); break;
            default: Serial.println("UNKNOWN"); break;
        }
        
        // X and Y axes
        Serial.print("X-Axis: ");
        Serial.println(report[3]);
        Serial.print("Y-Axis: ");
        Serial.println(report[4]);
        
        // Raw HID report values
        Serial.print("Raw HID Report: [");
        for (int i = 0; i < 5; i++) {
            Serial.print("0x");
            if (report[i] < 16) Serial.print("0"); // Ensure 2 digit hex
            Serial.print(report[i], HEX);
            if (i < 4) Serial.print(", ");
        }
        Serial.println("]");
        Serial.println("======================");
        
        pInputCharacteristic->setValue(report, sizeof(report));
        pInputCharacteristic->notify();
    }
}

// Set battery level
void BLEJoystick::setBatteryLevel(uint8_t level) {
    batteryLevel = level > 100 ? 100 : level;
}

// Notify battery level to connected client
void BLEJoystick::notifyBatteryLevel() {
    if (deviceState == DEVICE_CONNECTED) {
        pBatteryCharacteristic->setValue(&batteryLevel, 1);
        pBatteryCharacteristic->notify();
    }
}

// Get current device state
uint8_t BLEJoystick::getState() const {
    return deviceState;
}

// Set state change callback
void BLEJoystick::setStateChangeCallback(std::function<void()> callback) {
    stateChangeCallback = callback;
}

// Update device state and call callback if set
void BLEJoystick::updateDeviceState(uint8_t newState) {
    if (deviceState != newState) {
        deviceState = newState;
        if (stateChangeCallback) {
            stateChangeCallback();
        }
    }
}

// Server callbacks implementation
BLEJoystick::ServerCallbacks::ServerCallbacks(BLEJoystick* device) : device(device) {}

void BLEJoystick::ServerCallbacks::onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
    device->updateDeviceState(BLEJoystick::DEVICE_CONNECTED);
    Serial.println("Client connected");
}

void BLEJoystick::ServerCallbacks::onDisconnect(NimBLEServer* pServer) {
    device->updateDeviceState(BLEJoystick::DEVICE_IDLE);
    Serial.println("Client disconnected");
}
