#pragma once

#include <Arduino.h>

namespace dashconfig {

// Waveshare ESP32-S3-Touch-LCD-2.8C exposed UART header pins.
// Use a 3.3 V CAN transceiver. All displays run in CAN normal mode.
static constexpr int CAN_TX_PIN = 43;
static constexpr int CAN_RX_PIN = 44;

static constexpr uint32_t SERIAL_BAUD = 115200;
static constexpr uint32_t CAN_STALE_MS = 6000;
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
