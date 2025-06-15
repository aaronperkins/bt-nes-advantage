# bt-nes-advantage
Integrated Bluetooth adapter for the NES Advantage (NES-026)

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
1. **Turn on the controller** - The controller will automatically wake up when any button is pressed.
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


7. **Installation in Controller**


### PCB Layout Notes

- The board is designed to fit within the NES Advantage controller case without modification
- Battery placement should avoid contact with moving parts of the joystick
- DC jack should be accessible through a hole in the case
- RGB LED should be positioned to be visible from outside the controller

