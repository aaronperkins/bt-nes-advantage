#include "BLEJoystick.h"
#include <Arduino.h>

// HID Report Descriptor for a single joystick
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
BLEJoystick::BLEJoystick(std::string deviceName, std::string manufacturerName, 
                            uint16_t vendorId, uint16_t productId, uint8_t numDevices) {
    // Validate and set number of devices (1-4 supported)
    this->numDevices = (numDevices >= 1 && numDevices <= 4) ? numDevices : 1;
    
    deviceState = DEVICE_STOPPED;
    batteryLevel = 100;
    
    // Allocate dynamic arrays for HID devices and characteristics
    pHidDevice = new NimBLEHIDDevice*[this->numDevices];
    pInputCharacteristic = new NimBLECharacteristic*[this->numDevices];
    
    // Allocate dynamic arrays for HID report data
    buttons = new uint8_t*[this->numDevices];
    axes = new int16_t*[this->numDevices];
    hat = new uint8_t[this->numDevices];
    
    for (uint8_t i = 0; i < this->numDevices; i++) {
        buttons[i] = new uint8_t[2];
        axes[i] = new int16_t[8];
        memset(buttons[i], 0, 2 * sizeof(uint8_t));
        memset(axes[i], 0, 8 * sizeof(int16_t));
        hat[i] = 0;
    }
    
    // Initialize BLE
    NimBLEDevice::init(deviceName);
    
    // Set security
    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    
    // Create server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));
    pServer->advertiseOnDisconnect(false); // Default to false
    
    // Create HID devices for each player (in reverse order so first device is seen as joystick 0)
    for (int8_t i = this->numDevices - 1; i >= 0; i--) {
        pHidDevice[i] = new NimBLEHIDDevice(pServer);
        pHidDevice[i]->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
        pHidDevice[i]->manufacturer()->setValue(manufacturerName);
        pHidDevice[i]->pnp(0x01, vendorId, productId, 0x0110);
        pHidDevice[i]->hidInfo(0x00, 0x01);
        
        // Create input characteristic for this player
        pInputCharacteristic[i] = pHidDevice[i]->inputReport(1);
        
        // Start the HID device
        pHidDevice[i]->startServices();
    }
    
    // Create battery service on the last HID device
    pBatteryCharacteristic = pHidDevice[this->numDevices - 1]->batteryLevel();
    
    // Set initial battery level
    setBatteryLevel(100);
}

// Destructor implementation
BLEJoystick::~BLEJoystick() {
    // Clean up dynamic arrays
    if (buttons) {
        for (uint8_t i = 0; i < numDevices; i++) {
            delete[] buttons[i];
        }
        delete[] buttons;
    }
    
    if (axes) {
        for (uint8_t i = 0; i < numDevices; i++) {
            delete[] axes[i];
        }
        delete[] axes;
    }
    
    delete[] hat;
    delete[] pHidDevice;
    delete[] pInputCharacteristic;
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
        disconnect();
        updateDeviceState(DEVICE_STOPPED);
    }
}

// Start advertising
void BLEJoystick::startAdvertising() {
    if (deviceState == DEVICE_IDLE) {
        NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
        pAdvertising->setAppearance(HID_GAMEPAD);
        // Advertise HID services in reverse order to match creation order
        for (int8_t i = numDevices - 1; i >= 0; i--) {
            pAdvertising->addServiceUUID(pHidDevice[i]->hidService()->getUUID());
        }
        pAdvertising->setScanResponse(true);
        pAdvertising->start();
        
        updateDeviceState(DEVICE_ADVERTISING);
    }
}

// Stop advertising
void BLEJoystick::stopAdvertising() {
    if (deviceState == DEVICE_ADVERTISING) {
        NimBLEDevice::getAdvertising()->stop();
        updateDeviceState(DEVICE_IDLE);
    }
}

// Set automatic advertising on disconnect
void BLEJoystick::setAdvertiseOnDisconnect(bool advertise) {
    if (pServer != nullptr) {
        pServer->advertiseOnDisconnect(advertise);
    }
}

// Disconnect any active BLE connections
void BLEJoystick::disconnect() {
    if (deviceState == DEVICE_CONNECTED && pServer != nullptr) {
        // First set the state to IDLE to prevent any automatic advertising
        updateDeviceState(DEVICE_IDLE);
        
        // Disconnect all connected clients
        size_t peersNum = this->pServer->getConnectedCount();
        for (int i = 0; i < peersNum; i++) {
            uint16_t connID = this->pServer->getPeerInfo(i).getConnHandle();
            this->pServer->disconnect(connID);
        }
        
        // Small delay to allow disconnection to complete
        delay(100);
    }
}

// Set button states for a specific player
void BLEJoystick::setPlayerButtons(uint8_t player, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6,
                                  bool b7, bool b8, bool b9, bool b10, bool b11, bool b12) {
    if (player >= numDevices) return; // Invalid player
    
    // First byte contains buttons 1-8
    buttons[player][0] = 0;
    if (b1) buttons[player][0] |= (1 << 0);
    if (b2) buttons[player][0] |= (1 << 1);
    if (b3) buttons[player][0] |= (1 << 2);
    if (b4) buttons[player][0] |= (1 << 3);
    if (b5) buttons[player][0] |= (1 << 4);
    if (b6) buttons[player][0] |= (1 << 5);
    if (b7) buttons[player][0] |= (1 << 6);
    if (b8) buttons[player][0] |= (1 << 7);
    
    // Second byte contains buttons 9-12 (and padding)
    buttons[player][1] = 0;
    if (b9) buttons[player][1] |= (1 << 0);
    if (b10) buttons[player][1] |= (1 << 1);
    if (b11) buttons[player][1] |= (1 << 2);
    if (b12) buttons[player][1] |= (1 << 3);
}

// Set button states (legacy method - uses player 0)
void BLEJoystick::setButtons(bool b1, bool b2, bool b3, bool b4, bool b5, bool b6,
                            bool b7, bool b8, bool b9, bool b10, bool b11, bool b12) {
    setPlayerButtons(0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12);
}

// Set axis values for a specific player
void BLEJoystick::setPlayerAxes(uint8_t player, int16_t x, int16_t y, int16_t z, int16_t rZ, 
                               int16_t rX, int16_t rY, int16_t slider1, int16_t slider2) {
    if (player >= numDevices) return; // Invalid player
    
    axes[player][0] = x;
    axes[player][1] = y;
    axes[player][2] = z;
    axes[player][3] = rZ;
    axes[player][4] = rX;
    axes[player][5] = rY;
    axes[player][6] = slider1;
    axes[player][7] = slider2;
}

// Set axis values (legacy method - uses player 0)
void BLEJoystick::setAxes(int16_t x, int16_t y, int16_t z, int16_t rZ, 
                          int16_t rX, int16_t rY, int16_t slider1, int16_t slider2) {
    setPlayerAxes(0, x, y, z, rZ, rX, rY, slider1, slider2);
}

// Set hat direction for a specific player
void BLEJoystick::setPlayerHat(uint8_t player, uint8_t hatDirection) {
    if (player >= numDevices) return; // Invalid player
    hat[player] = hatDirection <= 8 ? hatDirection : 0;
}

// Set hat direction (legacy method - uses player 0)
void BLEJoystick::setHat(uint8_t hatDirection) {
    setPlayerHat(0, hatDirection);
}

// Notify HID report to connected client for a specific player
void BLEJoystick::notifyPlayerHIDReport(uint8_t player) {
    if (player >= numDevices || deviceState != DEVICE_CONNECTED) return;
    
    uint8_t report[5];  // Report includes 2 button bytes, 1 hat byte, and 2 axis bytes
    
    report[0] = buttons[player][0];
    report[1] = buttons[player][1];
    report[2] = (hat[player] & 0x0F);
    report[3] = axes[player][0]; // X axis
    report[4] = axes[player][1]; // Y axis
    
    // Debug output in a human-readable format
    Serial.print("=== HID REPORT DEBUG PLAYER ");
    Serial.print(player + 1);
    Serial.println(" ===");
    
    for (int i = 0; i < 8; i++) {
        Serial.print("  Button ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.println((report[0] & (1 << i)) ? "PRESSED" : "released");
    }
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
    Serial.print(report[3]);
    Serial.print(" Y-Axis: ");
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
    
    pInputCharacteristic[player]->setValue(report, sizeof(report));
    pInputCharacteristic[player]->notify();
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

// Get number of players
uint8_t BLEJoystick::getNumDevices() const {
    return numDevices;
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
}

void BLEJoystick::ServerCallbacks::onDisconnect(NimBLEServer* pServer) {
    device->updateDeviceState(BLEJoystick::DEVICE_IDLE);
}
