#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "DashConfig.h"
#include "HaltechCan.h"

enum class DashSide : uint8_t {
  Left,
  Right
};

class DashboardUi {
public:
  explicit DashboardUi(DashSide side) : side_(side) {}

  void begin() {
    lv_obj_t* root = lv_scr_act();
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x05070A), LV_PART_MAIN);
    lv_obj_set_style_text_color(root, lv_color_hex(0xEAF2F7), LV_PART_MAIN);

    createCanIndicator(root);
    if (side_ == DashSide::Left) {
      createLeft(root);
    } else {
      createRight(root);
    }
  }

  void update(const DashTelemetry& t, uint32_t canAgeMs) {
    if ((uint32_t)(millis() - lastUpdateMs_) < dashconfig::UI_REFRESH_MS) {
      return;
    }
    lastUpdateMs_ = millis();

    const bool canOnline = canAgeMs != UINT32_MAX && canAgeMs <= dashconfig::CAN_STALE_MS;
    lv_obj_set_style_bg_color(canDot_, canOnline ? lv_color_hex(0x13D17B) : lv_color_hex(0xD12828), LV_PART_MAIN);
    lv_label_set_text(canLabel_, canOnline ? "CAN" : "NO CAN");

    if (side_ == DashSide::Left) {
      updateLeft(t);
    } else {
      updateRight(t, canOnline);
    }
  }

private:
  DashSide side_;
  uint32_t lastUpdateMs_ = 0;

  lv_obj_t* canDot_ = nullptr;
  lv_obj_t* canLabel_ = nullptr;
  lv_obj_t* mainArc_ = nullptr;
  lv_obj_t* mainValue_ = nullptr;
  lv_obj_t* mainUnit_ = nullptr;
  lv_obj_t* auxLabels_[8] = {};
  lv_obj_t* warnLabel_ = nullptr;

  static void setText(lv_obj_t* label, const char* text) {
    if (label != nullptr) {
      lv_label_set_text(label, text);
    }
  }

  static void setTextFmt(lv_obj_t* label, const char* fmt, float value) {
    if (label == nullptr) {
      return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, value);
    lv_label_set_text(label, buf);
  }

  static void setField(lv_obj_t* label, const DashField& field, const char* fmt, float scale = 1.0f, float offset = 0.0f) {
    if (field.fresh(dashconfig::CAN_STALE_MS)) {
      setTextFmt(label, fmt, field.value * scale + offset);
    } else {
      setText(label, "--");
    }
  }

  static lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(label, 0, LV_PART_MAIN);
    return label;
  }

  static lv_obj_t* makeMetric(lv_obj_t* parent, int centerX, int y, const char* name) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 108, 44);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(box, LV_ALIGN_TOP_MID, centerX - 240, y);

    lv_obj_t* nameLabel = makeLabel(box, LV_FONT_DEFAULT, lv_color_hex(0x7E8B95));
    lv_label_set_text(nameLabel, name);
    lv_obj_align(nameLabel, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* valueLabel = makeLabel(box, LV_FONT_DEFAULT, lv_color_hex(0xEAF2F7));
    lv_label_set_text(valueLabel, "--");
    lv_obj_align(valueLabel, LV_ALIGN_TOP_MID, 0, 20);
    return valueLabel;
  }

  void createCanIndicator(lv_obj_t* root) {
    canDot_ = lv_obj_create(root);
    lv_obj_remove_style_all(canDot_);
    lv_obj_set_size(canDot_, 12, 12);
    lv_obj_set_style_radius(canDot_, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(canDot_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(canDot_, lv_color_hex(0xD12828), LV_PART_MAIN);
    lv_obj_align(canDot_, LV_ALIGN_TOP_MID, -24, 62);

    canLabel_ = makeLabel(root, LV_FONT_DEFAULT, lv_color_hex(0x9DABB4));
    lv_label_set_text(canLabel_, "NO CAN");
    lv_obj_align(canLabel_, LV_ALIGN_TOP_MID, 18, 58);
  }

  void createMainArc(lv_obj_t* root, int min, int max, const char* unit, lv_color_t color) {
    mainArc_ = lv_arc_create(root);
    lv_obj_set_size(mainArc_, 462, 462);
    lv_arc_set_rotation(mainArc_, 135);
    lv_arc_set_bg_angles(mainArc_, 0, 270);
    lv_arc_set_range(mainArc_, min, max);
    lv_arc_set_value(mainArc_, min);
    lv_obj_remove_style(mainArc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(mainArc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(mainArc_, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(mainArc_, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(mainArc_, lv_color_hex(0x18212A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(mainArc_, color, LV_PART_INDICATOR);
    lv_obj_align(mainArc_, LV_ALIGN_CENTER, 0, 0);

    mainValue_ = makeLabel(root, LV_FONT_DEFAULT, lv_color_hex(0xFFFFFF));
    lv_label_set_text(mainValue_, "0");
    lv_obj_align(mainValue_, LV_ALIGN_CENTER, 0, -10);

    mainUnit_ = makeLabel(root, LV_FONT_DEFAULT, lv_color_hex(0x8D9AA3));
    lv_label_set_text(mainUnit_, unit);
    lv_obj_align(mainUnit_, LV_ALIGN_CENTER, 0, 42);
  }

  void createLeft(lv_obj_t* root) {
    createMainArc(root, 0, dashconfig::RPM_MAX, "RPM", lv_color_hex(0x16A8F5));

    auxLabels_[0] = makeMetric(root, 120, 334, "COOLANT");
    auxLabels_[1] = makeMetric(root, 240, 334, "TPS");
    auxLabels_[2] = makeMetric(root, 360, 334, "MAP");
    auxLabels_[3] = makeMetric(root, 180, 394, "IAT");
    auxLabels_[4] = makeMetric(root, 300, 394, "IGN");
  }

  void createRight(lv_obj_t* root) {
    createMainArc(root, 0, 260, "KM/H", lv_color_hex(0x16A8F5));

    warnLabel_ = makeLabel(root, LV_FONT_DEFAULT, lv_color_hex(0xFFCE4A));
    lv_label_set_text(warnLabel_, "");
    lv_obj_align(warnLabel_, LV_ALIGN_CENTER, 0, 88);

    auxLabels_[0] = makeMetric(root, 120, 334, "COOLANT");
    auxLabels_[1] = makeMetric(root, 240, 334, "INJ DUTY");
    auxLabels_[2] = makeMetric(root, 360, 334, "BATT");
    auxLabels_[3] = makeMetric(root, 180, 394, "LAMBDA");
    auxLabels_[4] = makeMetric(root, 300, 394, "CAM");
  }

  void updateLeft(const DashTelemetry& t) {
    const uint16_t rpm = t.rpm.fresh(dashconfig::CAN_STALE_MS) ? (uint16_t)t.rpm.value : 0;
    lv_arc_set_value(mainArc_, rpm);
    char rpmBuf[16];
    snprintf(rpmBuf, sizeof(rpmBuf), "%u", rpm);
    lv_label_set_text(mainValue_, rpmBuf);

    setField(auxLabels_[0], t.coolantTempC, "%.0f F", 9.0f / 5.0f, 32.0f);
    setField(auxLabels_[1], t.throttlePercent, "%.0f%%");
    setField(auxLabels_[2], t.mapKpaAbs, "%.0f kPa");
    setField(auxLabels_[3], t.intakeAirTempC, "%.0f F", 9.0f / 5.0f, 32.0f);
    setField(auxLabels_[4], t.ignitionAngleDeg, "%.0f deg");
  }

  void updateRight(const DashTelemetry& t, bool canOnline) {
    const float speedKmh = t.vehicleSpeedKmh.fresh(dashconfig::GPS_STALE_MS) ? t.vehicleSpeedKmh.value : 0.0f;
    lv_arc_set_value(mainArc_, constrain((int)speedKmh, 0, 260));
    setTextFmt(mainValue_, "%.0f", speedKmh);

    setField(auxLabels_[0], t.coolantTempC, "%.0f F", 9.0f / 5.0f, 32.0f);
    setField(auxLabels_[1], t.injectorDutyPercent, "%.0f%%");
    setField(auxLabels_[2], t.batteryVoltage, "%.1f V");
    setField(auxLabels_[3], t.lambda1, "%.3f");
    setField(auxLabels_[4], t.intakeCamDeg, "%.0f deg");

    const bool hotCoolant = t.coolantTempC.fresh(dashconfig::CAN_STALE_MS) &&
                            dashconfig::cToF(t.coolantTempC.value) >= dashconfig::HOT_COOLANT_F;
    const bool lowBattery = t.batteryVoltage.fresh(dashconfig::CAN_STALE_MS) &&
                            t.batteryVoltage.value < dashconfig::LOW_BATTERY_V;
    const bool cel = t.status.fresh(dashconfig::CAN_STALE_MS) && t.status.checkEngine;

    if (!canOnline) {
      lv_label_set_text(warnLabel_, "NO CAN DATA");
      lv_obj_set_style_text_color(warnLabel_, lv_color_hex(0xE03C32), LV_PART_MAIN);
    } else if (hotCoolant) {
      lv_label_set_text(warnLabel_, "COOLANT HOT");
      lv_obj_set_style_text_color(warnLabel_, lv_color_hex(0xE03C32), LV_PART_MAIN);
    } else if (cel) {
      lv_label_set_text(warnLabel_, "CHECK ENGINE");
      lv_obj_set_style_text_color(warnLabel_, lv_color_hex(0xFFCE4A), LV_PART_MAIN);
    } else if (lowBattery) {
      lv_label_set_text(warnLabel_, "LOW BATTERY");
      lv_obj_set_style_text_color(warnLabel_, lv_color_hex(0xFFCE4A), LV_PART_MAIN);
    } else {
      lv_label_set_text(warnLabel_, "SYSTEM OK");
      lv_obj_set_style_text_color(warnLabel_, lv_color_hex(0x13D17B), LV_PART_MAIN);
    }
  }
};
