# Bluetooth NES Advantage
Integrated Bluetooth adapter for the NES Advantage (NES-026)

![BT-NES-Advantage Hero Shot](https://raw.githubusercontent.com/aaronperkins/bt-nes-advantage/refs/heads/main/docs/images/main.jpg)

## Overview
The BT-NES-Advantage is a custom Bluetooth adapter built into the NES Advantage controller, allowing it to connect wirelessly to modern devices as a standard Bluetooth gamepad. This project uses an ESP32-C3 microcontroller to read the NES controller's inputs and transmit them via Bluetooth HID protocol.

## Features
- Wireless Bluetooth connectivity
- Standard gamepad HID implementation
- Long battery life with low power sleep mode
- RGB status LED indication
- Battery level monitoring and reporting
- 5V DC barrel jack charging port

## Usage Instructions

### Pairing the Controller
1. **Turn on the controller** - The controller will automatically wake up when the Start button is held.
2. **Enter pairing mode** - The controller will automatically enter pairing mode when turned on, indicated by a blinking blue LED.
3. **Pair with your device** - On your device (computer, smartphone, game console, etc.), go to Bluetooth settings and select "NES Advantage" from the list of available devices.
4. **Connection successful** - When successfully connected, the blue LED will turn solid.

### Button Mapping
The NES Advantage controller buttons are mapped to standard gamepad controls:
- **D-Pad** - Maps to the directional pad or left analog stick
- **A button** - Maps to Button 1
- **B button** - Maps to Button 4
- **Start button** - Maps to Start button
- **Select button** - Maps to Select button

### Special Functions
- **Sleep mode** - To manually put the controller into sleep mode, hold the **Start button** for 5 seconds. The LEDs will turn off when the controller enters sleep mode.
- **Reconnect/Pair new device** - To disconnect from the current device and enter pairing mode again, hold the **Select button** for 5 seconds.
- **Wake from sleep** - Hold the **Start button** to wake the controller from sleep mode.

### LED Indicators
- **Blue LED**:
  - Solid ON - Connected to a device
  - Blinking - In pairing mode (advertising)
  - OFF - Not connected or in sleep mode
- **Green LED**:
  - Blinking - Battery charging
  - Solid ON - Battery fully charged
  - OFF - Not charging
- **Red LED**:
  - Blinking - Low battery (below 20%)
  - OFF - Battery level normal

### Battery and Power
- The controller uses a rechargeable LiPo battery.
- To charge, connect a 5V DC power adapter to the barrel jack.
- Battery level is reported to the connected device when supported.
- The controller will automatically enter sleep mode after 5 minutes of inactivity to conserve power.
- When advertising (pairing mode), the controller will stop advertising after 30 seconds to save battery.

![Controller Charging](https://raw.githubusercontent.com/aaronperkins/bt-nes-advantage/refs/heads/main/docs/images/charging.jpg)
*NES Advantage controller charging via 5V DC barrel jack*

### Troubleshooting
- If unable to pair with a new device, ensure the controller is in pairing mode (blue LED blinking).
- For persistent issues, try charging the controller fully before use.

## Technical Specifications
- MCU: ESP32-C3 (LOLIN C3 Mini)
- Bluetooth: BLE 5.0
- Battery: 3.7V LiPo 
- Charging: Via 5V DC barrel jack
- Power consumption: ~120mA when connected, <1mA in sleep mode
- Original NES controller compatibility: 100% hardware compatible

## Support
For issues, questions, or contributions, please visit the project repository.

## PCB Assembly Instructions

### Bill of Materials (BOM)

| Designator | Qty | Value | Footprint | Description | Notes |
|------------|-----|-------|-----------|-------------|-------|
| C1, C3 | 2 | 10µF | C_0603_1608Metric | Unpolarized capacitor | SMT |
| C2 | 1 | 22µF | C_0603_1608Metric | Unpolarized capacitor | SMT |
| D1, D2, D3 | 3 | 1N5817W | D_SOD-123 | Schottky diode | SMT |
| D4, D5, D6 | 3 | LED | LED_0603_1608Metric | RGB LED (R,G,B) | SMT, 1=K 2=A |
| J1 | 1 | PJ-043-SMT-TR | CUI_PJ-040-SMT-TR | DC Power Jack | SMT, 5V input |
| J2 | 1 | Conn_01x08_Socket | JST_XH_S8B-XH-A_1x08_P2.50mm | 8-pin NES controller connector | Horizontal, THT |
| J3 | 1 | JST Connector | JST_PH_S2B-PH-K_1x02_P2.00mm | LiPo battery connector | Horizontal, THT |
| L1 | 1 | 2.2µH | L_1008_2520Metric | Inductor | SMT |
| R1, R2, R3 | 3 | 200Ω | R_0603_1608Metric | Resistor for LEDs | SMT |
| R7, R8, R9, R10 | 4 | 1KΩ | R_0603_1608Metric | Resistor | SMT |
| R4 | 1 | 2KΩ | R_0603_1608Metric | Resistor | SMT |
| R5, R6 | 2 | 100KΩ | R_0603_1608Metric | Voltage divider for battery monitoring | SMT |
| U1 | 1 | TP4057 | TSOT-23-6 | Li-ion battery charger IC | SMT |
| U2 | 1 | TPS613222ADBV | SOT-23-5 | 5V boost converter | SMT |
| U3 | 1 | ESP32-C3_SUPERMINI_TH | ESP32-C3 SuperMini | ESP32-C3 WiFi/BLE module | THT |

### Parts Sourcing

- **PCB**: [PCBWay Project - Order PCB](https://www.pcbway.com/project/shareproject/Bluetooth_NES_Advantage_865d24ef.html)
- **Parts List**: [Digikey Parts List](https://www.digikey.com/en/mylists/list/EVZPX74W7P)
- **ESP32-C3 SuperMini Module**: [Amazon Link](https://a.co/d/1LoWyFs)
- **J2 Connector Harness Cable**: [Amazon Link](https://a.co/d/1LoWyFs)
- **Recommended Battery**: [103450 3.7V LiPo Battery](https://a.co/d/7UFYwX9) (IMPORTANT: Check polarity before connecting)
- **Charging cable**: [DC 2.5x0.7mm Barrel Jack Power Cable](https://a.co/d/4bPUQj2)

### Assembly Steps

1. **PCB Preparation**
   - Ensure you have the latest PCB design files from the `pcb` directory
   - Verify that all components from the BOM are available before starting

2. **Component Placement Order**
   - Start with SMD components in this order:
     1. Resistors (R1-R10)
     2. Capacitors (C1-C3)
     3. Diodes (D1-D3)
     4. LEDs (D4-D6) - Note the polarity! Cathode (K) is marked
     5. ICs (U1, U2)
     6. Inductor (L1)
     7. DC Jack (J1)
   - Then place through-hole components:
     1. JST connectors (J2, J3)
     2. ESP32-C3 module (U3)

3. **Soldering Guidelines**
   - For SMD components:
     - Use a fine-tip soldering iron (0.5mm or smaller)
     - Recommended temperature: 320-350°C
     - Apply small amount of flux for better results
     - Use tweezers for precise placement
   - For THT components:
     - Secure components flush to PCB before soldering
     - Solder from the lowest-profile components to the highest

4. **Power Connections**
   - The battery connector (J3) is for a 3.7V LiPo battery
   - Use appropriate JST PH connector with correct polarity
   - The power jack (J1) accepts 5V DC input

5. **Controller Connections**
   - Connect the PCB to the NES Advantage controller using the 8-pin JST XH connector (J2)
   - Pinout from left to right: DATA_P1, LATCH, CLK_P1, DATA_P2, CLK_P2, N/C, +5V, GND
    6. **Pin Mapping**
        - Refer to this pin mapping diagram for connecting the PCB to the NES Advantage controller:
        
        ![NES Advantage Pin Mapping](https://raw.githubusercontent.com/aaronperkins/bt-nes-advantage/refs/heads/main/docs/images/pin_mapping.jpg)
        
7. **Installation in Controller**
    - Carefully open the NES Advantage controller by removing the screws from the bottom
    - Place the PCB in the controller as shown below:
    
    ![PCB Placement](https://raw.githubusercontent.com/aaronperkins/bt-nes-advantage/refs/heads/main/docs/images/board%20placement.jpg)
    
    - Install the battery in the recommended position:
    
    ![Battery Placement](https://raw.githubusercontent.com/aaronperkins/bt-nes-advantage/refs/heads/main/docs/images/battery_placement.jpg)
    
    - Route wires to avoid interference with moving parts of the joystick
    - Ensure the RGB LED is visible or positioned near an existing hole

### PCB Layout Notes

- The board is designed to fit within the NES Advantage controller case without modification
- Battery placement should avoid contact with moving parts of the joystick
- DC jack should be accessible through a hole in the case
- RGB LED should be positioned to be visible from outside the controller

## Firmware Instructions

### Prerequisites
- [PlatformIO](https://platformio.org/install) (recommended) or Arduino IDE
- USB cable for connecting to the ESP32-C3 SuperMini module
- Required libraries:
  - NimBLE-Arduino (v1.4.1 or later)
  - Adafruit GFX Library (v1.11.3 or later)

### Building the Firmware with PlatformIO

1. **Install PlatformIO**
   - Install PlatformIO IDE as an extension for VSCode
   - Or install the PlatformIO Core CLI

2. **Clone the Repository**
   ```bash
   git clone https://github.com/aaronperkins/bt-nes-advantage.git
   cd bt-nes-advantage
   ```

3. **Open the Project in PlatformIO**
   - In VSCode with PlatformIO extension, select "Open Project" and choose the `src` folder
   - Or from the command line:
     ```bash
     cd src
     pio project init
     ```

4. **Configure the Project**
   - The `platformio.ini` file is already configured with the correct settings:
     ```ini
     [env:lolin_c3_mini]
     platform = espressif32
     board = lolin_c3_mini
     framework = arduino
     lib_deps =
         h2zero/NimBLE-Arduino@^1.4.1
         adafruit/Adafruit GFX Library@^1.11.3
     ```

5. **Build the Project**
   - In PlatformIO IDE, click on the "Build" button
   - Or from the command line:
     ```bash
     pio run
     ```

### Uploading the Firmware

1. **Connect the ESP32-C3 SuperMini**
   - Connect the ESP32-C3 SuperMini module to your computer via USB
   - If you've already installed it in the controller, you'll need to either:
     - Connect to the USB port on the module (if accessible)
     - Or temporarily remove the module from the PCB for programming

2. **Upload the Firmware**
   - In PlatformIO IDE, click on the "Upload" button
   - Or from the command line:
     ```bash
     pio run -t upload
     ```

3. **Monitor Serial Output (Optional)**
   - For debugging, you can monitor the serial output at 9600 baud
   - In PlatformIO IDE, click on the "Serial Monitor" button
   - Or from the command line:
     ```bash
     pio device monitor -b 9600
     ```

### Building with Arduino IDE (Alternative)

1. **Install Arduino IDE**
   - Download and install the latest Arduino IDE from [arduino.cc](https://www.arduino.cc/en/software)

2. **Install ESP32 Board Support**
   - Open Arduino IDE
   - Go to File > Preferences
   - Add the following URL to the "Additional Board Manager URLs" field:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to Tools > Board > Boards Manager
   - Search for "esp32" and install the "ESP32 by Espressif Systems"

3. **Install Required Libraries**
   - Go to Sketch > Include Library > Manage Libraries
   - Search for and install:
     - "NimBLE-Arduino" by h2zero
     - "Adafruit GFX Library" by Adafruit

4. **Open the Project**
   - Copy the contents from:
     - `src/src/main.cpp` to a new sketch
     - `src/src/BLEJoystick.cpp` and `src/include/BLEJoystick.h` to your Arduino libraries folder

5. **Configure Board Settings**
   - Select Tools > Board > ESP32 Arduino > LOLIN C3 Mini

6. **Upload the Sketch**
   - Click the Upload button

### Testing After Upload

1. **Basic Functionality Test**
   - After uploading the firmware, the controller will automatically enter pairing mode
   - The blue LED should start blinking
   - Use a Bluetooth device to test pairing and button functionality

2. **Troubleshooting Upload Issues**
   - If you can't upload, ensure you're in upload mode by holding the BOOT button while connecting USB
   - Check that the correct port is selected in the IDE
   - Verify your USB cable is a data cable, not just a charging cable

3. **Firmware Configuration Options**
   - You can customize the controller behavior by modifying these parameters in `main.cpp`:
     - `SLEEP_TIMEOUT`: Time before sleep mode activates from inactivity
     - `BATTERY_VOLTAGE_CALIBRATION_FACTOR`: Adjust if battery level reading is incorrect
     - Device name: Change `"NES Advantage"` in the constructor if desired

