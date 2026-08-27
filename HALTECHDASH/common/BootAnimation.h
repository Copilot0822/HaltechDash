#pragma once

#include <Arduino.h>

#include "DashUi.h"

namespace bootanimation {

static inline float easeInOut(float t) {
  t = constrain(t, 0.0f, 1.0f);
  return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) * 0.5f;
}

template <typename DisplayT>
void runMainGaugeSweep(DashboardUi& ui, DisplayT& display) {
  const uint16_t maxValue = ui.mainGaugeMax();
  const uint32_t startMs = millis();
  const uint32_t upMs = 850;
  const uint32_t holdMs = 180;
  const uint32_t downMs = 700;
  const uint32_t totalMs = upMs + holdMs + downMs;

  while ((uint32_t)(millis() - startMs) <= totalMs) {
    const uint32_t elapsed = millis() - startMs;
    float valueRatio = 0.0f;

    if (elapsed <= upMs) {
      valueRatio = easeInOut((float)elapsed / (float)upMs);
    } else if (elapsed <= upMs + holdMs) {
      valueRatio = 1.0f;
    } else {
      const uint32_t downElapsed = elapsed - upMs - holdMs;
      valueRatio = 1.0f - easeInOut((float)downElapsed / (float)downMs);
    }

    ui.setBootSweepValue((uint16_t)lroundf(maxValue * valueRatio));
    display.loop();
    delay(12);
  }

  ui.setBootSweepValue(0);
  display.loop();
}

}  // namespace bootanimation
