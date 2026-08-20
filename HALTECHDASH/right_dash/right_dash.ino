#define LV_CONF_INCLUDE_SIMPLE

#include "lv_conf.h"

#include "../common/DashConfig.h"
#include "../common/HaltechCan.h"
#include "../common/UbxGps.h"
#include "../common/Waveshare28CDisplay.h"
#include "../common/DashUi.h"

HaltechCan ecuCan;
UbxGps gps;
waveshare28c::Display display;
DashboardUi ui(DashSide::Right);

bool displayOk = false;
bool canOk = false;
uint32_t lastGpsBroadcastMs = 0;

void setup() {
  Serial.begin(dashconfig::SERIAL_BAUD);
  delay(500);

  Serial.println();
  Serial.println("HaltechDash right dash starting");
  Serial.println("Target board: Waveshare ESP32-S3-Touch-LCD-2.8C");
  Serial.println("GPS source: u-blox MAX-M10S on I2C/DDC address 0x42");

  displayOk = display.begin();
  if (!displayOk) {
    Serial.println("Display init failed");
  } else {
    ui.begin();
  }

  gps.begin();

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
  gps.poll();
  ecuCan.poll();

  const uint32_t now = millis();
  const GpsData& gpsData = gps.data();
  if (canOk && gpsData.updatedMs != 0 && (uint32_t)(now - lastGpsBroadcastMs) >= dashconfig::GPS_SPEED_BROADCAST_MS) {
    lastGpsBroadcastMs = now;
    ecuCan.transmitGpsSpeedKmh(gpsData.speedKmh, gpsData.fixValid, gpsData.satellites);
  }

  if (displayOk) {
    ui.update(ecuCan.data(), ecuCan.msSinceBusRx());
    display.loop();
  }

  delay(2);
}
