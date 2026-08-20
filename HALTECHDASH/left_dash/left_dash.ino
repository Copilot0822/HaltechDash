#define LV_CONF_INCLUDE_SIMPLE

#include "lv_conf.h"

#include "../common/DashConfig.h"
#include "../common/HaltechCan.h"
#include "../common/Waveshare28CDisplay.h"
#include "../common/DashUi.h"

HaltechCan ecuCan;
waveshare28c::Display display;
DashboardUi ui(DashSide::Left);

bool displayOk = false;
bool canOk = false;

void setup() {
  Serial.begin(dashconfig::SERIAL_BAUD);
  delay(500);

  Serial.println();
  Serial.println("HaltechDash left dash starting");
  Serial.println("Target board: Waveshare ESP32-S3-Touch-LCD-2.8C");

  displayOk = display.begin();
  if (!displayOk) {
    Serial.println("Display init failed");
  } else {
    ui.begin();
  }

  canOk = ecuCan.begin(dashconfig::CAN_RX_PIN, dashconfig::CAN_TX_PIN, HaltechCan::Mode::Normal);
  if (!canOk) {
    Serial.println("CAN init failed");
  } else {
    Serial.printf("CAN normal mode started. RX=%d TX=%d  bitrate=1M\n",
                  dashconfig::CAN_RX_PIN,
                  dashconfig::CAN_TX_PIN);
  }
}

void loop() {
  ecuCan.poll();

  if (displayOk) {
    ui.update(ecuCan.data(), ecuCan.msSinceBusRx());
    display.loop();
  }

  delay(2);
}
