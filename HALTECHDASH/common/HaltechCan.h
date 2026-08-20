#pragma once

#include <Arduino.h>
#include <driver/twai.h>

#include "DashConfig.h"

struct DashField {
  float value = 0.0f;
  uint32_t updatedMs = 0;

  bool seen() const {
    return updatedMs != 0;
  }

  bool fresh(uint32_t maxAgeMs) const {
    return seen() && (uint32_t)(millis() - updatedMs) <= maxAgeMs;
  }
};

struct DashStatus {
  bool checkEngine = false;
  bool oilPressureLight = false;
  bool fan1 = false;
  bool fan2 = false;
  uint32_t updatedMs = 0;

  bool fresh(uint32_t maxAgeMs) const {
    return updatedMs != 0 && (uint32_t)(millis() - updatedMs) <= maxAgeMs;
  }
};

struct DashTelemetry {
  DashField rpm;
  DashField mapKpaAbs;
  DashField throttlePercent;
  DashField coolantPressureKpaGauge;
  DashField fuelPressureKpaGauge;
  DashField oilPressureKpaGauge;
  DashField injectorDutyPercent;
  DashField ignitionAngleDeg;
  DashField lambda1;
  DashField vehicleSpeedKmh;
  DashField intakeCamDeg;
  DashField batteryVoltage;
  DashField baroKpaAbs;
  DashField coolantTempC;
  DashField intakeAirTempC;
  DashField oilTempC;
  DashField ecuTempC;
  DashField gear;
  DashStatus status;
};

class HaltechCan {
public:
  enum class Mode : uint8_t {
    Normal
  };

  bool begin(int rxPin, int txPin, Mode mode = Mode::Normal) {
    end();

    twai_general_config_t general =
        TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)txPin, (gpio_num_t)rxPin, TWAI_MODE_NORMAL);

#if defined(TWAI_TIMING_CONFIG_1MBITS)
    twai_timing_config_t timing = TWAI_TIMING_CONFIG_1MBITS();
#else
    twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
#endif
    twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&general, &timing, &filter);
    if (err != ESP_OK) {
      return false;
    }

    err = twai_start();
    if (err != ESP_OK) {
      twai_driver_uninstall();
      return false;
    }

    started_ = true;
    lastBusRxMs_ = 0;
    return true;
  }

  void end() {
    if (!started_) {
      return;
    }
    twai_stop();
    twai_driver_uninstall();
    started_ = false;
  }

  void poll() {
    if (!started_) {
      return;
    }

    twai_message_t msg;
    while (twai_receive(&msg, 0) == ESP_OK) {
      lastBusRxMs_ = millis();
      if (msg.extd || msg.rtr || msg.data_length_code < 8) {
        continue;
      }
      decode((uint16_t)(msg.identifier & 0x7FF), msg.data);
    }
  }

  const DashTelemetry& data() const {
    return data_;
  }

  bool transmitGpsSpeedKmh(float speedKmh, bool fixValid, uint8_t satellites) {
    if (!started_) {
      return false;
    }

    const uint32_t now = millis();
    put(data_.vehicleSpeedKmh, speedKmh, now);

    const uint16_t rawSpeed = (uint16_t)constrain((int)lroundf(speedKmh * 10.0f), 0, 65535);
    twai_message_t msg = {};
    msg.identifier = dashconfig::GPS_SPEED_CAN_ID;
    msg.data_length_code = 8;
    msg.data[0] = (uint8_t)(rawSpeed >> 8);
    msg.data[1] = (uint8_t)(rawSpeed & 0xFF);
    msg.data[2] = fixValid ? 1 : 0;
    msg.data[3] = satellites;

    return twai_transmit(&msg, 0) == ESP_OK;
  }

  uint32_t msSinceBusRx() const {
    if (lastBusRxMs_ == 0) {
      return UINT32_MAX;
    }
    return (uint32_t)(millis() - lastBusRxMs_);
  }

private:
  bool started_ = false;
  uint32_t lastBusRxMs_ = 0;
  DashTelemetry data_;

  static uint16_t u16be(const uint8_t* d, uint8_t i) {
    return (uint16_t(d[i]) << 8) | uint16_t(d[i + 1]);
  }

  static int16_t s16be(const uint8_t* d, uint8_t i) {
    return (int16_t)u16be(d, i);
  }

  static float kelvinRawToC(uint16_t raw) {
    return (raw / 10.0f) - 273.15f;
  }

  static bool readBit(const uint8_t* d, uint8_t byteIndex, uint8_t bitIndex) {
    return ((d[byteIndex] >> bitIndex) & 0x01) != 0;
  }

  static void put(DashField& field, float value, uint32_t now) {
    field.value = value;
    field.updatedMs = now;
  }

  void decode(uint16_t id, const uint8_t* d) {
    const uint32_t now = millis();

    switch (id) {
      case 0x360:
        put(data_.rpm, (float)u16be(d, 0), now);
        put(data_.mapKpaAbs, u16be(d, 2) / 10.0f, now);
        put(data_.throttlePercent, u16be(d, 4) / 10.0f, now);
        put(data_.coolantPressureKpaGauge, (u16be(d, 6) / 10.0f) - 101.3f, now);
        break;

      case 0x361:
        put(data_.fuelPressureKpaGauge, (u16be(d, 0) / 10.0f) - 101.3f, now);
        put(data_.oilPressureKpaGauge, (u16be(d, 2) / 10.0f) - 101.3f, now);
        break;

      case 0x362:
        put(data_.injectorDutyPercent, u16be(d, 0) / 10.0f, now);
        put(data_.ignitionAngleDeg, s16be(d, 4) / 10.0f, now);
        break;

      case 0x368:
        put(data_.lambda1, u16be(d, 0) / 1000.0f, now);
        break;

      case 0x370:
        put(data_.intakeCamDeg, s16be(d, 4) / 10.0f, now);
        break;

      case 0x372:
        put(data_.batteryVoltage, u16be(d, 0) / 10.0f, now);
        put(data_.baroKpaAbs, u16be(d, 6) / 10.0f, now);
        break;

      case 0x3E0:
        put(data_.coolantTempC, kelvinRawToC(u16be(d, 0)), now);
        put(data_.intakeAirTempC, kelvinRawToC(u16be(d, 2)), now);
        put(data_.oilTempC, kelvinRawToC(u16be(d, 6)), now);
        break;

      case 0x3E4:
        data_.status.oilPressureLight = readBit(d, 1, 0);
        data_.status.fan1 = readBit(d, 3, 0);
        data_.status.fan2 = readBit(d, 3, 1);
        data_.status.checkEngine = readBit(d, 7, 7);
        data_.status.updatedMs = now;
        break;

      case 0x469:
        put(data_.ecuTempC, kelvinRawToC(u16be(d, 0)), now);
        break;

      case 0x470:
        put(data_.lambda1, u16be(d, 0) / 1000.0f, now);
        put(data_.gear, (float)((int8_t)d[7]), now);
        break;

      case dashconfig::GPS_SPEED_CAN_ID:
        put(data_.vehicleSpeedKmh, u16be(d, 0) / 10.0f, now);
        break;

      default:
        break;
    }
  }
};
