// Minimal example sketch (for reference only)
// main.cpp / .ino

#include "HaltechCanDash.h"
HaltechCanDash dash;

void setup() {
  Serial.begin(115200);
  // Set pins to match your wiring; keep ListenOnly unless you need to transmit.
  if (!dash.begin(4, 5, HaltechCanDash::Mode::ListenOnly)) {
    Serial.println("CAN init failed");
  }
}

void loop() {
  dash.poll();

  if (dash.hasRpm() && HaltechCanDash::isFresh(dash.rpmLastMs(), 200)) {
    Serial.print("RPM: "); Serial.println(dash.rpm());
  }
  dash.
}
