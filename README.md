# ESP32 - Well Water Pump
An ESP32-powered system designed to automate water well pump control and water tank management, ideal for cabins and cottages or any setup requiring efficient tank filling. Features a user-friendly WiFi web interface for seamless monitoring and control.


## Support This Project

If you find this project helpful and it serves you well, consider showing your appreciation by supporting its development. Your contributions are greatly valued!

[![Donate](https://img.shields.io/badge/Donate-PayPal-blue.svg)](https://www.paypal.com/donate/?hosted_button_id=BBFZPW8AC8PUE)


## Hardware

- ESP32-S3-DEVKITC-1-N16R8
- Two-Channel Relay Module (High and Low Level Trigger with Dual Optocoupler Isolation)
- I2C Analog-to-Digital Converter (ADS1115) with Current Transformer (CT) for current reading
- Computer ATX Power Supply
- ATX Power Supply Breakout Board
- Cradle for ESP32-S3 Development Boards
- Passive Buzzer Module
- SCT-013 10A 1V AC Current Sensor (Split Core Current Transformer)
- Soil Moisture Sensor (HD-38) for overfill protection
- Temperature Sensor (SHT35)
- Time-of-Flight (TOF200C) Sensor with VL53L0X for water level measurement
- Submersible 12V ECO-WORTHY Water Pump
- Flexahopper LPT-125 Water Tank
- Additional External NeoPixel RGBW LED on IO Pin 48
- Custom Breakout Board for 3V3 and I2C Lines
- [Hardware List with Pictures](https://github.com/HG-Pilot/WellWaterPump/tree/main/docs/parts).

## Pump Logic

<a href="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/Pump.Logic.png">
    <img src="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/Pump.Logic.png" alt="Pump Logic" style="max-width:100%; height:auto;">
</a>
The error logic has since been updated and split into separate **Error States**. Each error condition now has its own dedicated state within the system's state machine for better clarity and manageability. Despite this structural change, the core functionality and error-handling mechanisms remain consistent with the original design.

## Wiring Diagram

The wiring diagram provides a detailed visual guide for setting up the hardware components of the system. Click on the image below to view it in full size.

[![Wiring Diagram](https://github.com/HG-Pilot/WellWaterPump/raw/main/docs/WiringDiagram.png)](https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/WiringDiagram.png)

## Web Interface

The UI is composed of 4 pages, listed below:

- **HOME**  
  Provides a quick system overview and array of system control buttons.  
  <p align="center">
    <a href="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/%F0%9F%92%A7ESP32%20Program%20Home%F0%9F%92%A7.png">
      <img src="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/%F0%9F%92%A7ESP32%20Program%20Home%F0%9F%92%A7.png" alt="HOME Page Screenshot">
    </a>
  </p>

- **STATUS**  
  Shows live values, system running config, error counters, temperature, and current data.  
  <p align="center">
    <a href="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/%F0%9F%92%A7ESP32%20Program%20Status%F0%9F%92%A7.png">
      <img src="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/%F0%9F%92%A7ESP32%20Program%20Status%F0%9F%92%A7.png" alt="STATUS Page Screenshot">
    </a>
  </p>

- **CONFIG**  
  Provides a system startup configuration interface and factory reset option.  
  <p align="center">
    <a href="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/%F0%9F%92%A7ESP32%20Program%20Config%F0%9F%92%A7.png">
      <img src="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/%F0%9F%92%A7ESP32%20Program%20Config%F0%9F%92%A7.png" alt="CONFIG Page Screenshot">
    </a>
  </p>

- **LOGS**  
  Displays logs from a 60KB logging circular buffer and allows users to clear and copy the logs.  
  <p align="center">
    <a href="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/%F0%9F%92%A7ESP32%20Program%20Logs%F0%9F%92%A7.png">
      <img src="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/%F0%9F%92%A7ESP32%20Program%20Logs%F0%9F%92%A7.png" alt="LOGS Page Screenshot">
    </a>
  </p>

The UI interface supports dark and light modes, optimized for both mobile and desktop versions.


## Program Function and Overview

This program automates the control of a well water pump and water tank filling process using an ESP32 microcontroller.  
It is designed for off-grid cabins, cottages, or any water tank system.  
The software integrates multiple features to ensure efficient operation, user convenience, and system safety:

### Program Features:

- Automated pump control based on tank water levels, well recovery intervals, and timed pumping cycles
- ESP32 hardware timer controls well recovery and pumping intervals
- Well recovery interval allows well water to recover before the next pump cycle begins
- Desired tank water level is configurable; the ToF sensor at the top indicates less distance equals more water
- The ToF sensor takes five consecutive readings, stores them in an array, and calculates the median value after sorting. This approach ensures reliability by tolerating up to two failed reads while providing detailed logs for these occurrences.
- Pump cycles are timed based on your well capacity
- Onboard RGBW LED displays the current operational state of the system
- Logs align with State Machine color states and reflect the same LED colors
- Buzzer, when enabled, provides audible confirmation at the start of each state
- CT coil reads A/C side current draw and can be re-programmed to read DC as well
- Water temperature is updated at intervals and recorded pre- and post-pump cycle to display the delta water temperature
- System time is derived via NTP and supports time zones and DST
- Approximate tank fill percentage is displayed for semi-circular tanks
- System uptime is calculated and displayed
- Program states and errors feature verbose logging, timestamping, and color coding
- ADS calibration adjusts the CT current value
- Dark and light UI modes ensure optimal readability on mobile and desktop devices

### Safety Features:

- Stuck pump relay detection logic uses a second relay for emergency pump disconnect
- Overfilling is prevented with a critical water level check using a moisture sensor
- In the event of a ToF sensor failure or inability to retrieve a valid reading after two attempts, the system transitions into a **Yellow Error State**, ensuring safety and preventing erroneous operations.
- Over- and under-current pump protection avoids burning out or dry-running the pump
- Hysteresis threshold prevents rapid pump cycling when the tank is nearly full
- System events are logged with a circular buffer, enabling review and troubleshooting

### Connectivity Features:

- Two WiFi modes: STA (if configured) and AP (as a fallback or if STA is disabled)
- Web server on port 80 provides real-time data tracking, program control, and configuration
- NTP time synchronization operates in STA mode when connected to the internet


## Flashing and Compiling

This code can be compiled using Arduino IDE version 2.3.6 or higher, provided all necessary libraries are installed and dependencies are resolved. The external RTTTL library, included in the Data folder, must be installed manually. For the VL53L0X sensor, search for "Pololu VL53L0X" in the Library Manager to add the required library.

For a simpler approach, if you are using an ESP32-S3-DEVKITC-1-N16R8, you can flash pre-compiled firmware directly using the `flash.cmd` script located at [https://github.com/HG-Pilot/WellWaterPump/tree/main/build](https://github.com/HG-Pilot/WellWaterPump/tree/main/build). To proceed, connect your ESP32 to your computer, identify the assigned COM port (you may need to install drivers for the onboard USB-to-UART chip), and execute the `flash.cmd` script. Enter the COM port number in uppercase as prompted, then follow the on-screen instructions to complete the flashing process.


## Acknowledgments

This extensive project wouldn’t have been possible without the incredible support and patience of my wonderful wife.

A heartfelt thank you to her for making this a reality!
