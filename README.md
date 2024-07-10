# C2DS ESP32 Coop Snooper

The `C2DS-esp32-coop-snooper` project is part of the Chicken Coop Door Sensor (C2DS) system, designed to passively monitor and report the status of a FarmLite Chicken Coop Door. This system ensures the safety and security of chickens by providing real-time notifications about the door's status through various communication channels.

## Project Structure

```
C2DS-esp32-coop-snooper-main/
│
├── .gitattributes
├── .gitignore
├── CMakeLists.txt
├── Kconfig
├── README.md
├── dependencies.lock
├── partitions.csv
├── sdkconfig
├── sdkconfig.old
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   ├── idf_component.yml
│   ├── led.c
│   ├── led.h
│   ├── main.c
│   ├── mp3.c
│   ├── mp3.h
│   ├── mqtt.c
│   ├── mqtt.h
│   ├── ota.c
│   ├── ota.h
│   ├── spiffs.c
│   ├── spiffs.h
│   ├── squawk_mp3.h
│   ├── state_handler.c
│   ├── state_handler.h
│   ├── wifi.c
│   ├── wifi.h
│   ├── certs/
│   │   ├── amazon_root_ca1.c
│   │   ├── coop_snooper_cert.c
│   │   ├── coop_snooper_private_key.c
│   │   ├── iot-policy/
│   │   │   └── coop-snooper-Policy
│   │   ├── original/
│   │       ├── AmazonRootCA1.pem
│   │       ├── coop-snooper.cert.pem
│   │       ├── coop-snooper.private.key
│   │       ├── coop-snooper.public.key
├── scripts/
│   ├── convert_to_headers.sh
│   ├── create_certs.sh
│   ├── create_things.sh
│   ├── iot-access-policy.json
│   └── things.json
└── .vscode/
    ├── c_cpp_properties.json
    └── settings.json
```

## Overview

The Coop Snooper is an ESP32-based appliance that remotely reports the current status of the chicken coop door. It is an AWS IoT Thing that subscribes to an MQTT topic conveying the door status and uses an onboard LED to indicate this status.

### Features

- **Real-time Door Status:** The Coop Snooper indicates the chicken coop door status via an RGB LED.
- **Remote Monitoring:** Designed to be used remotely, it doesn't need to be on the same Wi-Fi network as the Coop Controller.
- **AWS IoT Integration:** Uses AWS IoT Core, Lambda functions, and DynamoDB for processing and state management.
- **OTA Updates:** Supports Over-The-Air (OTA) firmware updates via AWS S3.

## Hardware Requirements

- ESP32-C3-DevKitM-1
- RGB LED

## Software Requirements

- ESP-IDF
- AWS Account
- CMake

### Wiring Diagram

#### ESP32-C3-DevKitM-1 to RGB LED:
- **Red LED**: GPIO 2 (Red wire)
- **Green LED**: GPIO 3 (Green wire)
- **Blue LED**: GPIO 4 (Blue wire)

#### ESP32-C3-DevKitM-1 to Max98357 Amplifier:
- **DIN**: GPIO 25 (Purple wire)
- **BCLK**: GPIO 26 (Orange wire)
- **LRC**: GPIO 27 (Brown wire)
- **Vin**: 5V (Red wire)
- **GND**: GND (Black wire)
- **GAIN**: GPIO 14 (Yellow wire)
- **SD**: GPIO 13 (Green wire)

## Installation

### Setting Up the Development Environment

1. **Clone the Repository:**
   ```bash
   git clone <repository-url>
   cd C2DS-esp32-coop-snooper-main
   ```

2. **Configure the ESP-IDF:**
   Follow the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to set up your development environment.

3. **Configure AWS IoT:**
   Use the scripts provided to create AWS IoT Things and policies.
   ```bash
   cd scripts
   ./create_things.sh
   ```

4. **Set Up Certificates:**
   Ensure that the certificates for the device are correctly placed in the `main/certs/original` directory.

### Building and Flashing the Firmware

1. **Build the Project:**
   ```bash
   idf.py build
   ```

2. **Flash the Firmware:**
   ```bash
   idf.py flash
   ```

3. **Monitor the Output:**
   ```bash
   idf.py monitor
   ```

## Usage

1. **Power Up the Device:**
   Connect the ESP32 to a power source.

2. **Check the LED Status:**
   The LED will indicate the current status of the chicken coop door:
   - **Green:** Door is open during the day.
   - **Red (Flashing):** Error conditions such as door open at night or closed during the day.
   - **Blue:** Normal closed status at night.

3. **Monitor Door Status Remotely:**
   The status can be monitored remotely via the AWS IoT Console.

## Project Configuration

- **AWS IoT Core:** Manages the communication between the Coop Controller and Coop Snooper.
- **Lambda Functions:**
  - `C2DS-coop-controller-handler`
  - `C2DS-query-state`
  - `C2DS-set-twilight-times`
  - `C2DS-upgrade-handler`
- **DynamoDB Tables:**
  - `local-twilight-table`: Stores sunrise and sunset times.
  - `coop-state-table`: Stores the latest coop state.
- **S3 Buckets:**
  - Stores OTA images for the Coop Controller and Coop Snooper.

## Development

### Adding New Features

1. **Modify Source Code:**
   Source code is located in the `main` directory. Implement new features or bug fixes in the respective `.c` and `.h` files.

2. **Build and Test:**
   Follow the build and flash instructions to test your changes.

### Code Structure

- `main.c`: Entry point of the application.
- `led.c/h`: Manages LED operations.
- `mqtt.c/h`: Handles MQTT communication.
- `ota.c/h`: Manages OTA updates.
- `wifi.c/h`: Manages Wi-Fi connectivity.
- `state_handler.c/h`: Handles state transitions and logic.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## Contributing

1. **Fork the Repository**
2. **Create a Feature Branch:**
   ```bash
   git checkout -b feature/new-feature
   ```
3. **Commit Changes:**
   ```bash
   git commit -m "Add new feature"
   ```
4. **Push to the Branch:**
   ```bash
   git push origin feature/new-feature
   ```
5. **Create a Pull Request**

