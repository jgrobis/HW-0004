# NIXIE CLOCK

ESP32-C6 based Nixie clock firmware with WiFi setup portal, web configuration interface, NTP synchronization, OTA updates, anti-poisoning routine and configurable high-voltage enable driver mode.

> **Warning:** This project uses high voltage for Nixie tubes, typically around 160–170 V DC. Do not touch the circuit while powered. Always discharge the high-voltage output capacitor before working on the hardware.

## Features

* ESP32-C6 firmware based on ESP-IDF
* WiFi station mode with fallback access point
* Web-based configuration panel
* WiFi network scanner and credential storage in NVS
* NTP time synchronization
* Configurable NTP sync interval
* Manual time setting
* OTA firmware update through the web interface
* Admin/OTA token protection for sensitive actions
* Configurable WiFi power mode
* Anti-poisoning routine for Nixie tubes
* Optional motion/presence sensor enable flag
* High-voltage enable control with safe driver modes
* OTA rollback support
* HV fail-safe handling on display/I2C errors

## Hardware

Main hardware used in the project:

* ESP32-C6-MINI module
* TCA9535 I/O expanders
* MMBTA42 high-voltage NPN transistors for cathode switching
* External Nixie tube high-voltage boost converter module
* Nixie tubes with individual anode resistors
* USB-C 5 V power input

## High Voltage Control

The high-voltage converter module has an `EN` pin with an internal pull-up to approximately 5 V.

Because of that, the `EN` pin must **not** be connected directly to an ESP32 GPIO.

Supported HV driver modes:

### 1. MMBTA / Open-collector test mode

Temporary test mode using an NPN transistor.

Connection:

```text
HV module EN  -> MMBTA42 collector
MMBTA42 emitter -> GND
ESP GPIO -> MMBTA42 base
```

In this mode the firmware avoids driving the GPIO hard high. It uses input pull-up / low-output behavior to reduce risk during temporary testing.

This is only a temporary solution. A base resistor is recommended for proper hardware.

### 2. 2N7002 / BSS138 open-drain mode

Recommended simple hardware solution.

Connection:

```text
HV module EN -> 2N7002 drain
2N7002 source -> GND
ESP GPIO -> 2N7002 gate through 100 ohm - 1 kOhm
```

The HV module internal pull-up drives `EN` high when the MOSFET is off.

Behavior:

```text
MOSFET ON  -> EN pulled to GND -> HV OFF
MOSFET OFF -> EN pulled up by HV module -> HV ON
```

Do not short the high-voltage output to ground. Only the low-voltage `EN` pin is controlled.

## First Setup

If the device cannot connect to the configured WiFi network, it starts a fallback access point.

Default fallback AP:

```text
SSID: NIXIE-SETUP
Password: nixie1234
Address: http://192.168.4.1
```

Open the web panel and configure your WiFi network.

## Web Interface

The web interface allows:

* WiFi scanning
* WiFi credential configuration
* HV ON/OFF control
* Manual time setting
* NTP mode selection
* NTP sync interval configuration
* WiFi power mode selection
* Anti-poisoning enable/disable
* Manual anti-poisoning start
* Motion/presence sensor enable flag
* OTA update
* OTA/admin token change
* HV driver mode selection

Sensitive actions require an admin token.

Default token:

```text
nixie-ota
```

Change this token after the first successful setup.

## OTA Updates

OTA firmware update is available through the web interface.

The firmware uses an OTA partition table with two application slots:

```text
ota_0
ota_1
```

The first firmware with OTA partition layout must be flashed through USB.

After that, future updates can be uploaded from the web interface as `.bin` files.

OTA update requires the admin/OTA token.

## Build Requirements

* ESP-IDF
* Target: `esp32c6`

Recommended ESP-IDF version used during development:

```text
ESP-IDF v6.0.x
```

## Build

From the project directory:

```bash
idf.py set-target esp32c6
idf.py build
```

## Flash

For the first flash, especially after changing the partition table:

```bash
idf.py erase-flash
idf.py flash monitor
```

For later development flashes:

```bash
idf.py flash monitor
```

## Project Structure

```text
components/
  display/       Nixie display control
  hv/            High-voltage enable control
  ntp_sync/      NTP synchronization
  tca9535/       TCA9535 I/O expander driver
  timekeeper/    Timekeeping logic
  webserver/     Web interface and API
  wifi/          WiFi setup and power management

main/
  main.c         Main application entry point

partitions_ota.csv
sdkconfig.defaults
```

## Secrets

Do not commit private WiFi credentials.

Use:

```text
main/secrets.example.h
```

as a template for a local:

```text
main/secrets.h
```

The real `secrets.h` file should be ignored by Git.

## Safety Notes

* The high-voltage section can be dangerous.
* The HV output capacitor can remain charged after disabling the converter.
* Add a bleeder resistor on the HV output.
* Recommended bleeder example:

```text
HVDC_170V -> 1 MOhm -> 1 MOhm -> GND
```

or for faster discharge:

```text
HVDC_170V -> 470 kOhm -> 470 kOhm -> GND
```

Use resistors with suitable voltage rating.

## Recommended Hardware Improvements

* Do not connect the HV module `EN` pin directly to ESP32 GPIO if it is pulled up to 5 V.
* Use 2N7002/BSS138 open-drain control for the HV enable pin.
* Add a physical pull-down or fail-safe transistor arrangement where possible.
* Add an HV bleeder resistor.
* Delay HV startup in firmware to avoid USB charger brownout.
* Use a stable 5 V supply capable of handling HV boost startup current.

## Notes

The project is intended for experimental and educational use. High-voltage hardware should be tested carefully with current-limited supplies and proper measurement equipment.
