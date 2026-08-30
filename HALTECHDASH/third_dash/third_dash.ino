#define LV_CONF_INCLUDE_SIMPLE

#include "lv_conf.h"

#include "../common/BootAnimation.h"
#include "../common/DashConfig.h"
#include "../common/HaltechCan.h"
#include "../common/Waveshare28CDisplay.h"
#include "../common/DashUi.h"

HaltechCan ecuCan;
waveshare28c::Display display;
DashboardUi ui(DashSide::Third);

bool displayOk = false;
bool canOk = false;

static void printBoardIdentity() {
  const uint64_t mac = ESP.getEfuseMac();
  Serial.printf("Board MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                (uint8_t)(mac >> 40),
                (uint8_t)(mac >> 32),
                (uint8_t)(mac >> 24),
                (uint8_t)(mac >> 16),
                (uint8_t)(mac >> 8),
                (uint8_t)mac);
  Serial.printf("Free heap: %lu  PSRAM: %lu\n",
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getPsramSize());
}

void setup() {
  Serial.begin(dashconfig::SERIAL_BAUD);
  delay(500);

  Serial.println();
  Serial.println("HaltechDash third dash starting");
  Serial.println("Target board: Waveshare ESP32-S3 2.8C");
  printBoardIdentity();

  displayOk = display.begin();
  if (!displayOk) {
    Serial.println("Display init failed");
  } else {
    ui.begin();
    bootanimation::runMainGaugeSweep(ui, display);
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
