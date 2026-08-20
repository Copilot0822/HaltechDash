#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "DashConfig.h"

struct GpsData {
  float speedKmh = 0.0f;
  bool fixValid = false;
  uint8_t satellites = 0;
  uint32_t updatedMs = 0;

  bool fresh(uint32_t maxAgeMs) const {
    return updatedMs != 0 && (uint32_t)(millis() - updatedMs) <= maxAgeMs;
  }
};

class UbxGps {
public:
  void begin() {
    lineLen_ = 0;
  }

  void poll() {
    uint16_t available = bytesAvailable();
    if (available == 0 || available == 0xFFFF) {
      return;
    }

    while (available > 0) {
      const uint8_t chunk = (uint8_t)(available > 32 ? 32 : available);
      Wire.beginTransmission(dashconfig::UBLOX_DDC_ADDR);
      Wire.write(0xFF);
      if (Wire.endTransmission(false) != 0) {
        return;
      }

      const uint8_t got = Wire.requestFrom((int)dashconfig::UBLOX_DDC_ADDR, (int)chunk);
      for (uint8_t i = 0; i < got; i++) {
        const char c = (char)Wire.read();
        if ((uint8_t)c != 0xFF) {
          feed(c);
        }
      }

      if (got == 0 || got > available) {
        break;
      }
      available -= got;
    }
  }

  const GpsData& data() const {
    return data_;
  }

private:
  GpsData data_;
  char line_[96] = {};
  uint8_t lineLen_ = 0;

  static uint8_t hexValue(char c) {
    if (c >= '0' && c <= '9') {
      return (uint8_t)(c - '0');
    }
    if (c >= 'A' && c <= 'F') {
      return (uint8_t)(c - 'A' + 10);
    }
    if (c >= 'a' && c <= 'f') {
      return (uint8_t)(c - 'a' + 10);
    }
    return 0xFF;
  }

  static bool checksumOk(const char* sentence) {
    if (sentence[0] != '$') {
      return false;
    }

    uint8_t checksum = 0;
    uint8_t i = 1;
    while (sentence[i] != '\0' && sentence[i] != '*') {
      checksum ^= (uint8_t)sentence[i++];
    }

    if (sentence[i] != '*' || sentence[i + 1] == '\0' || sentence[i + 2] == '\0') {
      return false;
    }

    const uint8_t hi = hexValue(sentence[i + 1]);
    const uint8_t lo = hexValue(sentence[i + 2]);
    if (hi == 0xFF || lo == 0xFF) {
      return false;
    }
    return checksum == (uint8_t)((hi << 4) | lo);
  }

  static bool fieldEquals(const char* sentence, uint8_t fieldIndex, const char* expected) {
    char field[14];
    return readField(sentence, fieldIndex, field, sizeof(field)) && strcmp(field, expected) == 0;
  }

  static bool readField(const char* sentence, uint8_t fieldIndex, char* out, uint8_t outLen) {
    uint8_t current = 0;
    uint8_t outPos = 0;

    for (uint8_t i = 0; sentence[i] != '\0'; i++) {
      const char c = sentence[i];
      if (c == '$') {
        continue;
      }
      if (c == ',' || c == '*') {
        if (current == fieldIndex) {
          out[outPos] = '\0';
          return true;
        }
        current++;
        outPos = 0;
        if (c == '*') {
          return false;
        }
        continue;
      }
      if (current == fieldIndex && outPos < outLen - 1) {
        out[outPos++] = c;
      }
    }

    if (current == fieldIndex) {
      out[outPos] = '\0';
      return true;
    }
    return false;
  }

  uint16_t bytesAvailable() {
    Wire.beginTransmission(dashconfig::UBLOX_DDC_ADDR);
    Wire.write(0xFD);
    if (Wire.endTransmission(false) != 0) {
      return 0;
    }

    if (Wire.requestFrom((int)dashconfig::UBLOX_DDC_ADDR, 2) != 2) {
      return 0;
    }

    const uint8_t high = Wire.read();
    const uint8_t low = Wire.read();
    return ((uint16_t)high << 8) | low;
  }

  void feed(char c) {
    if (c == '$') {
      lineLen_ = 0;
    }

    if (c == '\r') {
      return;
    }

    if (c == '\n') {
      line_[lineLen_] = '\0';
      parseLine(line_);
      lineLen_ = 0;
      return;
    }

    if (lineLen_ < sizeof(line_) - 1) {
      line_[lineLen_++] = c;
    } else {
      lineLen_ = 0;
    }
  }

  void parseLine(const char* sentence) {
    if (!checksumOk(sentence)) {
      return;
    }

    if (strstr(sentence, "RMC,") != nullptr) {
      parseRmc(sentence);
    } else if (strstr(sentence, "VTG,") != nullptr) {
      parseVtg(sentence);
    } else if (strstr(sentence, "GGA,") != nullptr) {
      parseGga(sentence);
    }
  }

  void parseRmc(const char* sentence) {
    char field[16];
    const bool active = fieldEquals(sentence, 2, "A");
    if (!readField(sentence, 7, field, sizeof(field))) {
      return;
    }

    data_.speedKmh = atof(field) * 1.852f;
    data_.fixValid = active;
    data_.updatedMs = millis();
  }

  void parseVtg(const char* sentence) {
    char field[16];
    if (!readField(sentence, 7, field, sizeof(field))) {
      return;
    }

    data_.speedKmh = atof(field);
    data_.updatedMs = millis();
  }

  void parseGga(const char* sentence) {
    char field[8];
    if (readField(sentence, 6, field, sizeof(field))) {
      data_.fixValid = atoi(field) > 0;
    }
    if (readField(sentence, 7, field, sizeof(field))) {
      data_.satellites = (uint8_t)constrain(atoi(field), 0, 255);
    }
  }
};
