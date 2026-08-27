# HaltechDash Arduino Starter

Two Arduino sketches for a pair of Waveshare ESP32-S3-Touch-LCD-2.8C round displays reading Haltech Elite 550 CAN broadcast data from a naturally aspirated K24.

## Sketches

- `left_dash/left_dash.ino`: driver-facing engine page with RPM, coolant temp, TPS, MAP, IAT, and ignition angle.
- `right_dash/right_dash.ino`: GPS/CAN publisher and speed page with vehicle speed in km/h, coolant temp, injector duty, battery voltage, lambda, intake cam angle, and warnings.
- `simulator/index.html`: browser preview of both round displays with animated demo data and manual telemetry controls.
- `common/`: shared CAN decoder, display driver, UI code, and configuration.

## Browser Simulator

Open `simulator/index.html` in a browser. It does not need a dev server.

The simulator mirrors the starter layout:

- Left display: RPM, coolant temp, TPS, MAP, IAT, ignition angle.
- Right display: vehicle speed in km/h, coolant temp, injector duty, battery, lambda, cam angle, warning state.
- Controls: idle/cruise/pull/hot presets, pause/resume animation, sliders, CAN online, and check engine toggles.
- Vehicle speed is treated as GPS-derived data broadcast onto CAN by one display, not a native Haltech ECU channel.

## Arduino IDE Setup

Use Arduino IDE or Arduino CLI with:

- Board package: `esp32` by Espressif, tested with `3.3.8`
- Board: `ESP32S3 Dev Module`
- PSRAM: enabled, preferably `OPI PSRAM`
- Flash size: match the module on the Waveshare board, commonly `16MB`
- USB CDC on boot: enabled if you want serial over USB
- Library: `lvgl` `8.3.10` from the Waveshare 2.8C Arduino example package

The sketches were compile-checked with:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries "<Waveshare demo>\Arduino\libraries" .\HALTECHDASH\left_dash
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries "<Waveshare demo>\Arduino\libraries" .\HALTECHDASH\right_dash
```

The installed global LVGL on this machine is `9.5.0`; this starter uses the Waveshare-compatible LVGL `8.3.10` API.

For faster repeat builds, use the helper scripts. They set the ESP32-S3 options, use the Waveshare LVGL 8.3.10 library, and keep persistent build directories under `.arduino-build/`.

```powershell
powershell -ExecutionPolicy Bypass -File .\HALTECHDASH\tools\compile-left.ps1
powershell -ExecutionPolicy Bypass -File .\HALTECHDASH\tools\compile-right.ps1
powershell -ExecutionPolicy Bypass -File .\HALTECHDASH\tools\upload-left.ps1 -Port COM3
powershell -ExecutionPolicy Bypass -File .\HALTECHDASH\tools\upload-right.ps1 -Port COM3
```

Avoid `--clean` during normal UI iteration. A clean build recompiles LVGL and is much slower.

## CAN Wiring

Install one 3.3 V CAN transceiver per display unless you are building a shared receiver board. The ESP32-S3 has a built-in TWAI/CAN controller, but it still needs a CAN transceiver.

Both displays now run the CAN controller in normal mode. Do not use listen-only mode for this setup because the GPS display must transmit vehicle speed onto the bus.

Default pins are in `common/DashConfig.h`:

```cpp
static constexpr int CAN_TX_PIN = 43;
static constexpr int CAN_RX_PIN = 44;
```

Those pins are the exposed UART pins on the Waveshare 2.8C board and can be changed if you wire the transceiver elsewhere.

Suggested connection:

- ECU CAN H to transceiver CAN H
- ECU CAN L to transceiver CAN L
- ESP32 GPIO43 to transceiver TXD
- ESP32 GPIO44 to transceiver RXD
- ESP32 3V3 to transceiver VCC
- ESP32 GND to transceiver GND and ECU sensor/bus ground reference

## GPS Speed Broadcast

The right display is the GPS owner. It reads the u-blox 10th generation MAX-M10S-00B-01 over I2C/DDC and broadcasts vehicle speed onto CAN for the other display and the ECU.

MAX-M10S wiring to the right display:

- GPS SDA to Waveshare I2C SDA, GPIO15
- GPS SCL to Waveshare I2C SCL, GPIO7
- GPS VCC to 3.3 V
- GPS GND to common ground

The MAX-M10S default DDC/I2C address is `0x42`. The starter reads its NMEA stream from the u-blox DDC data portal and parses RMC/VTG speed.

Custom GPS speed CAN frame:

```cpp
static constexpr uint16_t GPS_SPEED_CAN_ID = 0x520;
```

Frame layout:

- CAN ID: standard 11-bit `0x520`
- Byte `0..1`: vehicle speed in km/h * 10, unsigned 16-bit big-endian
- Byte `2`: GPS fix valid, `0` or `1`
- Byte `3`: satellites used, if known
- Byte `4..7`: reserved

The ECU will only read this if you configure a matching custom CAN receive channel in Haltech NSP/ESP. Suggested first setup is speed from CAN ID `0x520`, bytes `0..1`, big-endian, scale `0.1`, units `km/h`.

## Current Assumptions

- Haltech broadcast CAN is 1 Mbit/s with standard 11-bit IDs.
- Common frames decoded: `0x360`, `0x361`, `0x362`, `0x368`, `0x370`, `0x372`, `0x3E0`, `0x3E4`, `0x469`, `0x470`.
- Vehicle speed is GPS-derived from the right display's MAX-M10S and rebroadcast on custom CAN ID `0x520`.
- K24 is naturally aspirated, so the starter UI avoids boost/wastegate/turbo displays.
- Temperatures are shown in Fahrenheit. Vehicle speed is shown in km/h.

If a channel reads wrong, adjust the scaling in `common/HaltechCan.h` after comparing against the Haltech NSP/ESP live data screen.
