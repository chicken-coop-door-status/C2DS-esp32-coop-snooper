# Coop Snooper Overview

The Coop Snooper is an ESP32-based device designed to monitor the status of a chicken coop door remotely. It provides real-time updates on the door's state through visual indicators and ensures the safety and security of the chickens. Here’s a detailed summary of its functionality:

## Key Features

1. **Remote Monitoring**: The Coop Snooper is capable of monitoring the chicken coop door's status from a remote location.
2. **Visual Status Indicators**: It uses a single RGB LED to display the current status of the door through different colors:
    - **Pulsing WHITE**: Indicates the system is starting up and attempting to establish an MQTT connection.
    - **RED**: Indicates that the door is open.
    - **GREEN**: Indicates that the door is closed.
    - **Pulsing BLUE**: Indicates an error in the system.

## Components

- **ESP32 Controller**: Acts as the main processing unit, handling Wi-Fi connectivity and MQTT communications.
- **RGB LED**: Provides visual feedback on the door's status.
- **AWS IoT Integration**: The Coop Snooper communicates with AWS IoT Core for real-time status updates via MQTT.

## System Workflow

1. **Startup and Initialization**:
    - The ESP32 initializes and attempts to connect to the configured Wi-Fi network.
    - During this period, the RGB LED pulses WHITE to indicate the system is starting up and not yet connected to the MQTT broker.

2. **Establishing MQTT Connection**:
    - Once connected to the Wi-Fi network, the ESP32 attempts to establish a connection with the AWS IoT MQTT broker.
    - Successful connection stops the WHITE pulsing, and the device subscribes to the relevant MQTT topics.

3. **Status Monitoring**:
    - The Coop Snooper listens for messages on the subscribed MQTT topic (`coop/status`).
    - Based on the received message, the RGB LED is set to different colors:
        - **Open Status**: The LED turns RED.
        - **Closed Status**: The LED turns GREEN.
        - **Error Status**: The LED pulses BLUE.

## LED Control

The LED is controlled using PWM (Pulse Width Modulation) via the ESP32's GPIO pins. The color and pulsing behavior are managed through dedicated tasks that adjust the LED's duty cycle to achieve the desired visual effect.

## Error Handling

- If an error message is received via MQTT, the LED begins pulsing BLUE to alert the user of a problem.
- The system will continue to pulse BLUE until the error condition is resolved or a new status message is received.

## Summary

The Coop Snooper provides an efficient and reliable way to monitor the status of a chicken coop door remotely. Through its integration with AWS IoT and the use of visual indicators, it ensures that users are promptly notified of any changes or issues, thereby enhancing the security and management of the chicken coop.
