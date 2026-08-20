#pragma once

#include <Arduino.h>

namespace dashconfig {

// Waveshare ESP32-S3-Touch-LCD-2.8C exposed UART header pins.
// Use a 3.3 V CAN transceiver. Both displays run in CAN normal mode.
static constexpr int CAN_TX_PIN = 43;
static constexpr int CAN_RX_PIN = 44;

// Custom standard 11-bit CAN frame published by the GPS display.
// Bytes 0..1: GPS vehicle speed, uint16 big-endian, km/h * 10
// Byte 2: GPS fix valid, 0/1
// Byte 3: satellites used, if known
// Bytes 4..7: reserved
static constexpr uint16_t GPS_SPEED_CAN_ID = 0x520;
static constexpr uint32_t GPS_SPEED_BROADCAST_MS = 100;

// u-blox MAX-M10S DDC/I2C interface. It shares the Waveshare I2C bus.
static constexpr uint8_t UBLOX_DDC_ADDR = 0x42;

static constexpr uint32_t SERIAL_BAUD = 115200;
static constexpr uint32_t CAN_STALE_MS = 500;
static constexpr uint32_t GPS_STALE_MS = 1500;
static constexpr uint32_t UI_REFRESH_MS = 50;

static constexpr uint16_t RPM_WARN = 7800;
static constexpr uint16_t RPM_REDLINE = 8400;
static constexpr uint16_t RPM_MAX = 9000;

static constexpr float PSI_PER_KPA = 0.1450377377f;

static constexpr float LOW_OIL_PSI_RUNNING = 15.0f;
static constexpr float HOT_COOLANT_F = 220.0f;
static constexpr float LOW_BATTERY_V = 12.0f;

static inline float cToF(float c) {
  return c * 9.0f / 5.0f + 32.0f;
}

static inline float kpaToPsi(float kpa) {
  return kpa * PSI_PER_KPA;
}

}  // namespace dashconfig
