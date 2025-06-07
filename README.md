# ESP32 - Well Water Pump
An ESP32-powered system designed to automate water well pump control and water tank management, ideal for cabins and cottages or any setup requiring efficient tank filling. Features a user-friendly WiFi web interface for seamless monitoring and control.


## Support This Project

If you find this project helpful and it serves you well, consider showing your appreciation by supporting its development. Your contributions are greatly valued!

[![Donate](https://img.shields.io/badge/Donate-PayPal-blue.svg)](https://www.paypal.com/donate/?hosted_button_id=BBFZPW8AC8PUE)


## Hardware

- ESP32-S3-DEVKITC-1-N16R8V
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


## Pump Logic

<a href="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/Pump.Logic.png">
    <img src="https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/Pump.Logic.png" alt="Pump Logic" style="max-width:100%; height:auto;">
</a>
The error logic has since been updated and split into separate **Error States**. Each error condition now has its own dedicated state within the system's state machine for better clarity and manageability. Despite this structural change, the core functionality and error-handling mechanisms remain consistent with the original design.

## Wiring Diagram

The wiring diagram provides a detailed visual guide for setting up the hardware components of the system. Click on the image below to view it in full size.

[![Wiring Diagram](https://github.com/HG-Pilot/WellWaterPump/raw/main/docs/WiringDiagram.png)](https://github.com/HG-Pilot/WellWaterPump/blob/main/docs/WiringDiagram.png)

