# SPCloud Device Code

## Overview
This project implements an ESP32-based device for SPCloud, enabling secure file downloads from AWS S3 via MQTT and presigned URLs, with robust SD card handling and OLED status display. The code is structured for reliability, dual-core performance, and ease of integration with the SPCloud backend.

## Project Structure
- **src/**
  - `main.cpp`: Main entry point, device setup, WiFi/MQTT/SD/OLED initialization, and main loop.
  - `app.h`: Project-wide declarations, shared flags, and function prototypes.
  - `file_download_handler.cpp`: Handles chunked file downloads from S3, dual-core FreeRTOS tasks (network and SD write), and download progress.
  - `mqtt_handler.cpp`: Handles MQTT subscriptions, message parsing, and device registration logic.
  - `OLED_handler.cpp`: All OLED display functions for status, progress, and notifications.
  - `sd_handler.cpp`: SD card initialization, status, and utility functions.
  - `network_handler.cpp`: WiFi and AWS IoT Core connection logic.
  - `secrets.h`: WiFi, AWS, and other sensitive credentials (not committed; see below).
- **lib/**: External libraries (Adafruit SSD1306, GFX, BusIO, etc.).
- **include/**: Additional headers if needed.
- **platformio.ini**: PlatformIO build configuration.

## How It Works
- On boot, the device initializes the OLED, SD card, WiFi, and MQTT.
- If not linked, a registration code is displayed on the OLED. The device blocks until it receives a 'linked' notification via MQTT.
- Once linked, the device subscribes to presigned URL topics and downloads files from S3, writing them to the SD card in chunks.
- Download/network operations run on one ESP32 core; SD writes run on the other for maximum performance.
- The OLED displays progress, errors, and status throughout.

## Setup Instructions

### 1. Hardware
- ESP32 development board (e.g., ESP32 DevKitC)
- MicroSD card and SD card module (wired to ESP32)
- SSD1306 OLED display (I2C, 128x32 or 128x64)

### 2. Software Prerequisites
- [PlatformIO](https://platformio.org/) (VSCode recommended)
- Python 3.x (for PlatformIO)

### 3. Clone the Repository
```sh
git clone <your-repo-url>
cd SP404SXCloudStorageDeviceV2
```

### 4. Configure Secrets
Create a file at `src/secrets.h` (not committed for security) with the following content:
```cpp
#pragma once
#define WIFI_SSID     "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define AWS_IOT_ENDPOINT "your-aws-iot-endpoint.amazonaws.com"
#define AWS_CERT_CA   "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n"
#define AWS_CERT_CRT  "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n"
#define AWS_CERT_PRIVATE "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n"
// Add any other required secrets here
```

### 5. Build and Upload
Connect your ESP32 via USB and run:
```sh
pio run --target upload
```
Or use the PlatformIO "Upload" button in VSCode.

### 6. Monitor Serial Output
```sh
pio device monitor
```
Or use the PlatformIO "Monitor" button.

### 7. Usage
- On first boot, the OLED will show a registration code. Link the device via the SPCloud web interface.
- Once linked, the device will automatically begin processing file download jobs sent via MQTT.
- Progress and errors are shown on the OLED and serial console.

## Troubleshooting
- **SD Card Not Detected:** Check wiring and SD card format (FAT32 recommended).
- **WiFi/AWS Connection Issues:** Double-check credentials in `secrets.h`.
- **Out of Memory:** Reduce chunk size or queue length in `file_download_handler.cpp`.
- **Device Not Linking:** Ensure MQTT topics and device ID match your backend setup.

