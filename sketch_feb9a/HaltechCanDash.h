// HaltechCanDash.h
// ESP32 Arduino (onboard TWAI/CAN controller) receiver + decoder for common Haltech broadcast frames.
// External 3.3V CAN transceiver required (e.g., SN65HVD230/TJA1051T-3.3/etc).
//
// Usage:
//   #include "HaltechCanDash.h"
//   HaltechCanDash dash;
//   void setup() {
//     Serial.begin(115200);
//     // Typical ESP32 TWAI pins (set to your wiring):
//     // RX = GPIO4, TX = GPIO5 is common on dev boards, but any valid GPIO works.
//     dash.begin(/*rxPin=*/4, /*txPin=*/5, HaltechCanDash::Mode::ListenOnly);
//   }
//   void loop() {
//     dash.poll();  // call frequently
//     if (dash.hasRpm()) Serial.println(dash.rpm());
//   }

#pragma once
#include <Arduino.h>
#include <driver/twai.h>

class HaltechCanDash {
public:
  enum class Mode : uint8_t {
    Normal,
    ListenOnly
  };

  // ---- Lifecycle ----
  bool begin(int rxPin, int txPin, Mode mode = Mode::ListenOnly) {
    end();  // safe if called twice

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)txPin, (gpio_num_t)rxPin, TWAI_MODE_NORMAL);
    if (mode == Mode::ListenOnly) g_config.mode = TWAI_MODE_LISTEN_ONLY;

    // Haltech CAN is commonly 1 Mbps.
#if defined(TWAI_TIMING_CONFIG_1MBITS)
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
#else
    // Fallback timing (works on many ESP32 setups; adjust if needed)
    // This fallback is intentionally conservative; prefer a core that defines TWAI_TIMING_CONFIG_1MBITS().
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
#endif

    // Accept all frames; we filter by ID in software.
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) return false;

    err = twai_start();
    if (err != ESP_OK) {
      twai_driver_uninstall();
      return false;
    }

    _started = true;
    _lastBusRxMs = 0;
    return true;
  }

  void end() {
    if (!_started) return;
    twai_stop();
    twai_driver_uninstall();
    _started = false;
  }

  // Call often in loop(). Non-blocking: drains RX queue and decodes frames.
  void poll() {
    if (!_started) return;

    twai_message_t msg;
    while (twai_receive(&msg, 0) == ESP_OK) {
      _lastBusRxMs = millis();
      if (msg.extd) continue;           // Haltech broadcast uses standard 11-bit IDs
      if (msg.rtr) continue;            // ignore remote frames
      if (msg.data_length_code != 8) {  // Haltech frames here are 8 bytes
        // still decode if you want; for now just ignore non-8
        continue;
      }
      decode_(msg.identifier & 0x7FF, msg.data);
    }
  }

  // Optional: how long since we saw ANY CAN frame (ms). Returns UINT32_MAX if never.
  uint32_t msSinceBusRx() const {
    if (_lastBusRxMs == 0) return UINT32_MAX;
    return (uint32_t)(millis() - _lastBusRxMs);
  }

  // ---- Generic helpers ----
  static bool isFresh(uint32_t lastUpdateMs, uint32_t maxAgeMs) {
    if (lastUpdateMs == 0) return false;
    return (uint32_t)(millis() - lastUpdateMs) <= maxAgeMs;
  }

  // ---- Getters (common dash channels) ----
  // 0x360 @ 50 Hz
  bool hasRpm() const { return _rpm_ms != 0; }
  uint16_t rpm() const { return _rpm; }
  uint32_t rpmLastMs() const { return _rpm_ms; }

  bool hasMapKpaAbs() const { return _map_ms != 0; }
  float map_kpa_abs() const { return _map_kpa_abs; }
  uint32_t mapLastMs() const { return _map_ms; }

  bool hasTps() const { return _tps_ms != 0; }
  float tps_percent() const { return _tps_percent; }
  uint32_t tpsLastMs() const { return _tps_ms; }

  bool hasCoolantPressure() const { return _coolantP_ms != 0; }
  float coolant_pressure_kpa_gauge() const { return _coolantP_kpa_g; }
  uint32_t coolantPressureLastMs() const { return _coolantP_ms; }

  // 0x361 @ 50 Hz
  bool hasFuelPressure() const { return _fuelP_ms != 0; }
  float fuel_pressure_kpa_gauge() const { return _fuelP_kpa_g; }
  uint32_t fuelPressureLastMs() const { return _fuelP_ms; }

  bool hasOilPressure() const { return _oilP_ms != 0; }
  float oil_pressure_kpa_gauge() const { return _oilP_kpa_g; }
  uint32_t oilPressureLastMs() const { return _oilP_ms; }

  bool hasEngineDemand() const { return _engDemand_ms != 0; }
  float engine_demand_percent() const { return _engDemand_percent; }
  uint32_t engineDemandLastMs() const { return _engDemand_ms; }

  bool hasWastegatePressure() const { return _wgP_ms != 0; }
  float wastegate_pressure_kpa_gauge() const { return _wgP_kpa_g; }
  uint32_t wastegatePressureLastMs() const { return _wgP_ms; }

  // 0x362 @ 50 Hz
  bool hasInjDuty1() const { return _inj1_ms != 0; }
  float inj_duty1_percent() const { return _inj1_percent; }
  uint32_t injDuty1LastMs() const { return _inj1_ms; }

  bool hasInjDuty2() const { return _inj2_ms != 0; }
  float inj_duty2_percent() const { return _inj2_percent; }
  uint32_t injDuty2LastMs() const { return _inj2_ms; }

  bool hasIgnAngle() const { return _ign_ms != 0; }
  float ignition_angle_deg() const { return _ign_deg; } // leading (+) / as transmitted
  uint32_t ignAngleLastMs() const { return _ign_ms; }

  // 0x368 @ 20 Hz (Lambda 1-4)
  bool hasLambda1() const { return _lam1_ms != 0; }
  bool hasLambda2() const { return _lam2_ms != 0; }
  bool hasLambda3() const { return _lam3_ms != 0; }
  bool hasLambda4() const { return _lam4_ms != 0; }
  float lambda1() const { return _lam1; }
  float lambda2() const { return _lam2; }
  float lambda3() const { return _lam3; }
  float lambda4() const { return _lam4; }
  uint32_t lambda1LastMs() const { return _lam1_ms; }
  uint32_t lambda2LastMs() const { return _lam2_ms; }
  uint32_t lambda3LastMs() const { return _lam3_ms; }
  uint32_t lambda4LastMs() const { return _lam4_ms; }

  // 0x36A @ 20 Hz (Knock 1-2)
  bool hasKnock1() const { return _knk1_ms != 0; }
  bool hasKnock2() const { return _knk2_ms != 0; }
  float knock1_db() const { return _knk1_db; }
  float knock2_db() const { return _knk2_db; }
  uint32_t knock1LastMs() const { return _knk1_ms; }
  uint32_t knock2LastMs() const { return _knk2_ms; }

  // 0x36B @ 20 Hz (BrakeP, NOS, TurboSpeed, LatG)
  bool hasBrakePressureFront() const { return _brakeP_ms != 0; }
  float brake_pressure_front_kpa_gauge() const { return _brakeP_kpa_g; }
  uint32_t brakePressureFrontLastMs() const { return _brakeP_ms; }

  bool hasNosPressure1() const { return _nosP_ms != 0; }
  float nos_pressure1_kpa_gauge() const { return _nosP_kpa_g; }
  uint32_t nosPressure1LastMs() const { return _nosP_ms; }

  bool hasTurboSpeed1() const { return _turbo_ms != 0; }
  uint32_t turbo_speed1_rpm() const { return _turbo_rpm; }
  uint32_t turboSpeed1LastMs() const { return _turbo_ms; }

  bool hasLateralAccel() const { return _latG_ms != 0; }
  float lateral_accel_mps2() const { return _latG_mps2; }
  uint32_t lateralAccelLastMs() const { return _latG_ms; }

  // 0x36C @ 20 Hz (Wheel speeds)
  bool hasWheelSpeeds() const { return _wsFL_ms != 0 || _wsFR_ms != 0 || _wsRL_ms != 0 || _wsRR_ms != 0; }
  float wheel_speed_fl_kmh() const { return _wsFL_kmh; }
  float wheel_speed_fr_kmh() const { return _wsFR_kmh; }
  float wheel_speed_rl_kmh() const { return _wsRL_kmh; }
  float wheel_speed_rr_kmh() const { return _wsRR_kmh; }
  uint32_t wheelSpeedFLLastMs() const { return _wsFL_ms; }
  uint32_t wheelSpeedFRLastMs() const { return _wsFR_ms; }
  uint32_t wheelSpeedRLLastMs() const { return _wsRL_ms; }
  uint32_t wheelSpeedRRLastMs() const { return _wsRR_ms; }

  // 0x370 @ 20 Hz (VehSpeed + Intake cams)
  bool hasVehicleSpeed() const { return _vss_ms != 0; }
  float vehicle_speed_kmh() const { return _vss_kmh; }
  uint32_t vehicleSpeedLastMs() const { return _vss_ms; }

  bool hasIntakeCam1() const { return _cam1_ms != 0; }
  bool hasIntakeCam2() const { return _cam2_ms != 0; }
  float intake_cam1_deg() const { return _cam1_deg; }
  float intake_cam2_deg() const { return _cam2_deg; }
  uint32_t intakeCam1LastMs() const { return _cam1_ms; }
  uint32_t intakeCam2LastMs() const { return _cam2_ms; }

  // 0x372 @ 10 Hz (Battery + Target boost + Baro)
  bool hasBatteryVoltage() const { return _bat_ms != 0; }
  float battery_voltage_v() const { return _bat_v; }
  uint32_t batteryVoltageLastMs() const { return _bat_ms; }

  bool hasTargetBoost() const { return _tboost_ms != 0; }
  float target_boost_kpa() const { return _tboost_kpa; }
  uint32_t targetBoostLastMs() const { return _tboost_ms; }

  bool hasBaro() const { return _baro_ms != 0; }
  float baro_kpa_abs() const { return _baro_kpa_abs; }
  uint32_t baroLastMs() const { return _baro_ms; }

private:
  bool _started = false;
  uint32_t _lastBusRxMs = 0;

  // ---- Decoded storage + timestamps ----
  uint16_t _rpm = 0;          uint32_t _rpm_ms = 0;
  float _map_kpa_abs = 0;     uint32_t _map_ms = 0;
  float _tps_percent = 0;     uint32_t _tps_ms = 0;
  float _coolantP_kpa_g = 0;  uint32_t _coolantP_ms = 0;

  float _fuelP_kpa_g = 0;     uint32_t _fuelP_ms = 0;
  float _oilP_kpa_g = 0;      uint32_t _oilP_ms = 0;
  float _engDemand_percent=0; uint32_t _engDemand_ms = 0;
  float _wgP_kpa_g = 0;       uint32_t _wgP_ms = 0;

  float _inj1_percent = 0;    uint32_t _inj1_ms = 0;
  float _inj2_percent = 0;    uint32_t _inj2_ms = 0;
  float _ign_deg = 0;         uint32_t _ign_ms = 0;

  float _lam1=0,_lam2=0,_lam3=0,_lam4=0;
  uint32_t _lam1_ms=0,_lam2_ms=0,_lam3_ms=0,_lam4_ms=0;

  float _knk1_db=0,_knk2_db=0;
  uint32_t _knk1_ms=0,_knk2_ms=0;

  float _brakeP_kpa_g=0;      uint32_t _brakeP_ms=0;
  float _nosP_kpa_g=0;        uint32_t _nosP_ms=0;
  uint32_t _turbo_rpm=0;      uint32_t _turbo_ms=0;
  float _latG_mps2=0;         uint32_t _latG_ms=0;

  float _wsFL_kmh=0,_wsFR_kmh=0,_wsRL_kmh=0,_wsRR_kmh=0;
  uint32_t _wsFL_ms=0,_wsFR_ms=0,_wsRL_ms=0,_wsRR_ms=0;

  float _vss_kmh=0;           uint32_t _vss_ms=0;
  float _cam1_deg=0,_cam2_deg=0;
  uint32_t _cam1_ms=0,_cam2_ms=0;

  float _bat_v=0;             uint32_t _bat_ms=0;
  float _tboost_kpa=0;        uint32_t _tboost_ms=0;
  float _baro_kpa_abs=0;      uint32_t _baro_ms=0;

  // ---- Decode helpers ----
  static uint16_t u16be_(const uint8_t* d, int i) {
    return (uint16_t)((d[i] << 8) | d[i + 1]);
  }
  static int16_t s16be_(const uint8_t* d, int i) {
    return (int16_t)u16be_(d, i);
  }

  // NOTE: The scaling below matches common Haltech broadcast tables.
  // If your ECU/firmware uses a different mapping, adjust per your protocol sheet.
  void decode_(uint16_t id, const uint8_t* d) {
    const uint32_t now = millis();

    switch (id) {
      case 0x360: {
        _rpm = u16be_(d, 0);              _rpm_ms = now;
        _map_kpa_abs = u16be_(d, 2) / 10.0f; _map_ms = now;
        _tps_percent = u16be_(d, 4) / 10.0f; _tps_ms = now;
        // gauge-ish pressure adjustment often uses -101.3 kPa (sea-level). Keep as transmitted logic.
        _coolantP_kpa_g = (u16be_(d, 6) / 10.0f) - 101.3f; _coolantP_ms = now;
      } break;

      case 0x361: {
        _fuelP_kpa_g = (u16be_(d, 0) / 10.0f) - 101.3f; _fuelP_ms = now;
        _oilP_kpa_g  = (u16be_(d, 2) / 10.0f) - 101.3f; _oilP_ms = now;
        _engDemand_percent = u16be_(d, 4) / 10.0f;      _engDemand_ms = now;
        _wgP_kpa_g   = (u16be_(d, 6) / 10.0f) - 101.3f; _wgP_ms = now;
      } break;

      case 0x362: {
        _inj1_percent = u16be_(d, 0) / 10.0f; _inj1_ms = now;
        _inj2_percent = u16be_(d, 2) / 10.0f; _inj2_ms = now;
        _ign_deg      = s16be_(d, 4) / 10.0f; _ign_ms  = now;
      } break;

      case 0x368: {
        _lam1 = u16be_(d, 0) / 1000.0f; _lam1_ms = now;
        _lam2 = u16be_(d, 2) / 1000.0f; _lam2_ms = now;
        _lam3 = u16be_(d, 4) / 1000.0f; _lam3_ms = now;
        _lam4 = u16be_(d, 6) / 1000.0f; _lam4_ms = now;
      } break;

      case 0x36A: {
        _knk1_db = u16be_(d, 0) / 100.0f; _knk1_ms = now;
        _knk2_db = u16be_(d, 2) / 100.0f; _knk2_ms = now;
      } break;

      case 0x36B: {
        // Some channels here have model/firmware-specific scaling; these are common defaults.
        _brakeP_kpa_g = (float)u16be_(d, 0) - 101.3f; _brakeP_ms = now;
        _nosP_kpa_g   = (u16be_(d, 2) * (11.0f / 50.0f)) - 101.3f; _nosP_ms = now;
        _turbo_rpm    = (uint32_t)u16be_(d, 4) * 10u; _turbo_ms = now;
        _latG_mps2    = s16be_(d, 6) / 10.0f; _latG_ms = now;
      } break;

      case 0x36C: {
        _wsFL_kmh = u16be_(d, 0) / 10.0f; _wsFL_ms = now;
        _wsFR_kmh = u16be_(d, 2) / 10.0f; _wsFR_ms = now;
        _wsRL_kmh = u16be_(d, 4) / 10.0f; _wsRL_ms = now;
        _wsRR_kmh = u16be_(d, 6) / 10.0f; _wsRR_ms = now;
      } break;

      case 0x370: {
        _vss_kmh = u16be_(d, 0) / 10.0f; _vss_ms = now;
        _cam1_deg = s16be_(d, 4) / 10.0f; _cam1_ms = now;
        _cam2_deg = s16be_(d, 6) / 10.0f; _cam2_ms = now;
      } break;

      case 0x372: {
        _bat_v = u16be_(d, 0) / 10.0f; _bat_ms = now;
        _tboost_kpa = u16be_(d, 4) / 10.0f; _tboost_ms = now;
        _baro_kpa_abs = u16be_(d, 6) / 10.0f; _baro_ms = now;
      } break;

      default:
        // Unknown/unused ID for this decoder
        break;
    }
  }
};
