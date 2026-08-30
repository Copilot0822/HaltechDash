# HaltechDash Arduino Starter

Two Arduino sketches for a pair of Waveshare ESP32-S3 2.8C round displays reading Haltech Elite 550 CAN broadcast data from a naturally aspirated K24.

## Sketches

- `left_dash/left_dash.ino`: driver-facing engine page with RPM, coolant temp, TPS, MAP, IAT, and ignition angle.
- `right_dash/right_dash.ino`: coolant-focused page with coolant temp on the main arc, injector duty, battery voltage, lambda, intake cam angle, and warnings.
- `third_dash/third_dash.ino`: temporary RPM-focused page with coolant temp, TPS, MAP, battery, lambda, and warnings.
- `simulator/index.html`: browser preview of both round displays with animated demo data and manual telemetry controls.
- `common/`: shared CAN decoder, display driver, UI code, and configuration.

## Browser Simulator

Open `simulator/index.html` in a browser. It does not need a dev server.

The simulator mirrors the starter layout:

- Left display: RPM, coolant temp, TPS, MAP, IAT, ignition angle.
- Right display: coolant temp on the main gauge, injector duty, battery, lambda, cam angle, warning state.
- Controls: idle/cruise/pull/hot presets, pause/resume animation, sliders, CAN online, and check engine toggles.

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
powershell -ExecutionPolicy Bypass -File .\HALTECHDASH\tools\compile-third.ps1
powershell -ExecutionPolicy Bypass -File .\HALTECHDASH\tools\upload-left.ps1 -Port COM3
powershell -ExecutionPolicy Bypass -File .\HALTECHDASH\tools\upload-right.ps1 -Port COM3
powershell -ExecutionPolicy Bypass -File .\HALTECHDASH\tools\upload-third.ps1 -Port COM3
```

Avoid `--clean` during normal UI iteration. A clean build recompiles LVGL and is much slower.

## Two Display Troubleshooting

Windows may assign a different COM port each time a display resets. The sketches print the physical ESP32 MAC address at boot so you can identify which display is actually flashed.

If one display works and the other stays blank, upload the same side sketch to the failing display and open serial at `115200`. The boot log now reports each display init stage:

- `I2C expander`: checks the onboard TCA9554 reset/control expander
- `ST7701 command init`: sends the LCD controller setup commands
- `RGB panel`: starts the ESP32-S3 RGB peripheral
- `backlight`: enables the LCD backlight and flashes the panel blue briefly
- `LVGL`: allocates the draw buffer and starts the UI renderer

If the log reaches `Display init OK` but the screen is black, suspect backlight/panel hardware or a display revision mismatch. If it stops before that, the last printed stage is the first place to check.

## CAN Wiring

Install one 3.3 V CAN transceiver per display unless you are building a shared receiver board. The ESP32-S3 has a built-in TWAI/CAN controller, but it still needs a CAN transceiver.

All displays run the CAN controller in normal mode.

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

## Current Assumptions

- Haltech broadcast CAN is 1 Mbit/s with standard 11-bit IDs.
- Common frames decoded: `0x360`, `0x361`, `0x362`, `0x368`, `0x370`, `0x372`, `0x3E0`, `0x3E4`, `0x469`, `0x470`.
- K24 is naturally aspirated, so the starter UI avoids boost/wastegate/turbo displays.
- Temperatures are shown in Fahrenheit.

If a channel reads wrong, adjust the scaling in `common/HaltechCan.h` after comparing against the Haltech NSP/ESP live data screen.
