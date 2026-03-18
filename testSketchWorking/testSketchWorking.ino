#include <Arduino.h>
#include "driver/twai.h"

// --------------------------------------------------
// ESP32 TWAI / CAN pins
// Change to match your transceiver wiring
// --------------------------------------------------
static const gpio_num_t CAN_TX = GPIO_NUM_18;
static const gpio_num_t CAN_RX = GPIO_NUM_19;

// --------------------------------------------------
// Tracked Haltech dash-relevant IDs
// --------------------------------------------------
struct TrackedFrame {
  uint16_t id;
  const char* name;
  bool seen;
  uint32_t count;
  uint8_t lastData[8];
  uint8_t lastDlc;
  uint32_t lastSeenMs;
};

TrackedFrame tracked[] = {
  {0x360, "Engine Core Rapid Update", false, 0, {0}, 0, 0},
  {0x361, "Pressure and Demand Update", false, 0, {0}, 0, 0},
  {0x362, "Injector Duty and Ignition", false, 0, {0}, 0, 0},
  {0x36A, "Knock Sensor Activity", false, 0, {0}, 0, 0},
  {0x370, "Vehicle Speed and Intake Cams", false, 0, {0}, 0, 0},
  {0x371, "Fuel Flow and Return", false, 0, {0}, 0, 0},
  {0x372, "Battery Boost and Baro", false, 0, {0}, 0, 0},
  {0x3E0, "Core Temperature Group", false, 0, {0}, 0, 0},
  {0x3E2, "Fuel Level", false, 0, {0}, 0, 0},
  {0x3E4, "Status Flags A", false, 0, {0}, 0, 0},
  {0x469, "ECU Internal Temperature", false, 0, {0}, 0, 0},
  {0x470, "Wideband and Gear", false, 0, {0}, 0, 0},
  {0x471, "Injector DeltaP APP and Exhaust", false, 0, {0}, 0, 0},
  {0x473, "Fuel Used and System States", false, 0, {0}, 0, 0},
};

static const size_t NUM_TRACKED = sizeof(tracked) / sizeof(tracked[0]);

const uint32_t PRINT_INTERVAL_MS = 100;
uint32_t lastPrintMs = 0;

// --------------------------------------------------
// Helpers
// --------------------------------------------------
uint16_t u16be(const uint8_t* d, uint8_t i) {
  return (uint16_t(d[i]) << 8) | uint16_t(d[i + 1]);
}

int16_t s16be(const uint8_t* d, uint8_t i) {
  return int16_t((uint16_t(d[i]) << 8) | uint16_t(d[i + 1]));
}

uint32_t u32be(const uint8_t* d, uint8_t i) {
  return (uint32_t(d[i]) << 24) |
         (uint32_t(d[i + 1]) << 16) |
         (uint32_t(d[i + 2]) << 8) |
         (uint32_t(d[i + 3]));
}

bool readBitField(const uint8_t* d, uint8_t byteIndex, uint8_t bitIndex) {
  return (d[byteIndex] >> bitIndex) & 0x01;
}

float kRawToC(uint16_t raw) {
  return (raw / 10.0f) - 273.1f;
}

void printHex11(uint16_t id) {
  Serial.printf("0x%03X", id);
}

TrackedFrame* findTracked(uint16_t id) {
  for (size_t i = 0; i < NUM_TRACKED; i++) {
    if (tracked[i].id == id) return &tracked[i];
  }
  return nullptr;
}

void updateTracked(uint16_t id, const uint8_t* data, uint8_t dlc) {
  TrackedFrame* f = findTracked(id);
  if (!f) return;

  f->seen = true;
  f->count++;
  f->lastDlc = dlc;
  f->lastSeenMs = millis();

  for (uint8_t i = 0; i < 8; i++) {
    f->lastData[i] = (i < dlc) ? data[i] : 0;
  }
}

void printBoolField(const char* label, bool value) {
  Serial.printf("    %s: %s\n", label, value ? "ON" : "OFF");
}

// --------------------------------------------------
// Per-frame decode printers
// --------------------------------------------------
void printDecoded360(const uint8_t* d) {
  float rpm = u16be(d, 0);
  float mapAbs = u16be(d, 2) / 10.0f;
  float tps = u16be(d, 4) / 10.0f;
  float coolantPress = (u16be(d, 6) / 10.0f) - 101.3f;

  Serial.printf("    RPM: %.0f rpm\n", rpm);
  Serial.printf("    MAP: %.1f kPa abs\n", mapAbs);
  Serial.printf("    TPS: %.1f %%\n", tps);
  Serial.printf("    Coolant Pressure: %.1f kPa gauge\n", coolantPress);
}

void printDecoded361(const uint8_t* d) {
  float fuelPress = (u16be(d, 0) / 10.0f) - 101.3f;
  float oilPress = (u16be(d, 2) / 10.0f) - 101.3f;
  float engineDemand = u16be(d, 4) / 10.0f;
  float wastegatePress = (u16be(d, 6) / 10.0f) - 101.3f;

  Serial.printf("    Fuel Pressure: %.1f kPa gauge\n", fuelPress);
  Serial.printf("    Oil Pressure: %.1f kPa gauge\n", oilPress);
  Serial.printf("    Engine Demand: %.1f %%\n", engineDemand);
  Serial.printf("    Wastegate Pressure: %.1f kPa gauge\n", wastegatePress);
}

void printDecoded362(const uint8_t* d) {
  float inj1 = u16be(d, 0) / 10.0f;
  float inj2 = u16be(d, 2) / 10.0f;
  float ign = s16be(d, 4) / 10.0f;

  Serial.printf("    Inj Stage 1 Duty: %.1f %%\n", inj1);
  Serial.printf("    Inj Stage 2 Duty: %.1f %%\n", inj2);
  Serial.printf("    Ignition Angle: %.1f deg\n", ign);
}

void printDecoded36A(const uint8_t* d) {
  float knock1 = u16be(d, 0) / 100.0f;
  float knock2 = u16be(d, 2) / 100.0f;

  Serial.printf("    Knock Level 1: %.2f dB\n", knock1);
  Serial.printf("    Knock Level 2: %.2f dB\n", knock2);
}

void printDecoded370(const uint8_t* d) {
  float speed = u16be(d, 0) / 10.0f;
  float intakeCam1 = s16be(d, 4) / 10.0f;
  float intakeCam2 = s16be(d, 6) / 10.0f;

  Serial.printf("    Vehicle Speed: %.1f km/h\n", speed);
  Serial.printf("    Intake Cam Angle 1: %.1f deg\n", intakeCam1);
  Serial.printf("    Intake Cam Angle 2: %.1f deg\n", intakeCam2);
}

void printDecoded371(const uint8_t* d) {
  uint16_t fuelFlow = u16be(d, 0);
  uint16_t fuelReturn = u16be(d, 2);

  Serial.printf("    Fuel Flow: %u cc/min\n", fuelFlow);
  Serial.printf("    Fuel Return: %u cc/min\n", fuelReturn);
}

void printDecoded372(const uint8_t* d) {
  float batt = u16be(d, 0) / 10.0f;
  float targetBoost = u16be(d, 4) / 10.0f;
  float baro = u16be(d, 6) / 10.0f;

  Serial.printf("    Battery Voltage: %.1f V\n", batt);
  Serial.printf("    Target Boost: %.1f kPa\n", targetBoost);
  Serial.printf("    Barometric Pressure: %.1f kPa abs\n", baro);
}

void printDecoded3E0(const uint8_t* d) {
  float clt = kRawToC(u16be(d, 0));
  float iat = kRawToC(u16be(d, 2));
  float fuelTemp = kRawToC(u16be(d, 4));
  float oilTemp = kRawToC(u16be(d, 6));

  Serial.printf("    Coolant Temp: %.1f C\n", clt);
  Serial.printf("    Air Temp: %.1f C\n", iat);
  Serial.printf("    Fuel Temp: %.1f C\n", fuelTemp);
  Serial.printf("    Oil Temp: %.1f C\n", oilTemp);
}

void printDecoded3E2(const uint8_t* d) {
  float fuelLevel = u16be(d, 0) / 10.0f;
  Serial.printf("    Fuel Level: %.1f L\n", fuelLevel);
}

void printDecoded3E4(const uint8_t* d) {
  Serial.println("    Status bits:");

  // Byte 1
  printBoolField("Neutral Switch", readBitField(d, 1, 7));
  printBoolField("Reverse Switch", readBitField(d, 1, 6));
  printBoolField("Gear Switch", readBitField(d, 1, 5));
  printBoolField("Decel Cut Active", readBitField(d, 1, 4));
  printBoolField("Transient Throttle Active", readBitField(d, 1, 3));
  printBoolField("Brake Pedal Switch", readBitField(d, 1, 2));
  printBoolField("Clutch Switch", readBitField(d, 1, 1));
  printBoolField("Oil Pressure Light", readBitField(d, 1, 0));

  // Byte 2
  printBoolField("Launch Control Active", readBitField(d, 2, 7));
  printBoolField("Launch Control Switch", readBitField(d, 2, 6));
  printBoolField("Aux RPM Limiter Active", readBitField(d, 2, 5));
  printBoolField("Flat Shift Switch", readBitField(d, 2, 3));
  printBoolField("Torque Reduction Active", readBitField(d, 2, 1));

  // Byte 3
  printBoolField("Traction Control Enabled", readBitField(d, 3, 7));
  printBoolField("Traction Control Active", readBitField(d, 3, 6));
  printBoolField("Air Con Request", readBitField(d, 3, 5));
  printBoolField("Air Con Output", readBitField(d, 3, 4));
  printBoolField("Thermo-fan 4 On", readBitField(d, 3, 3));
  printBoolField("Thermo-fan 3 On", readBitField(d, 3, 2));
  printBoolField("Thermo-fan 2 On", readBitField(d, 3, 1));
  printBoolField("Thermo-fan 1 On", readBitField(d, 3, 0));

  // Bytes 4,5,6
  Serial.printf("    Rotary Trim Pot 1: %d\n", int8_t(d[4]));
  Serial.printf("    Rotary Trim Pot 2: %d\n", int8_t(d[5]));
  Serial.printf("    Rotary Trim Pot 3: %d\n", int8_t(d[6]));

  // Byte 7
  printBoolField("Check Engine Light", readBitField(d, 7, 7));
  printBoolField("Battery Light Active", readBitField(d, 7, 6));
  printBoolField("Hand Brake State", readBitField(d, 7, 1));
  printBoolField("Traction Control Light", readBitField(d, 7, 0));
}

void printDecoded469(const uint8_t* d) {
  float ecuTemp = kRawToC(u16be(d, 0));
  Serial.printf("    ECU Temp: %.1f C\n", ecuTemp);
}

void printDecoded470(const uint8_t* d) {
  float wbOverall = u16be(d, 0) / 1000.0f;
  float wbBank1 = u16be(d, 2) / 1000.0f;
  float wbBank2 = u16be(d, 4) / 1000.0f;
  int8_t selector = int8_t(d[6]);
  int8_t gear = int8_t(d[7]);

  Serial.printf("    Wideband Overall: %.3f lambda\n", wbOverall);
  Serial.printf("    Wideband Bank 1: %.3f lambda\n", wbBank1);
  Serial.printf("    Wideband Bank 2: %.3f lambda\n", wbBank2);
  Serial.printf("    Gear Selector Position (raw enum): %d\n", selector);
  Serial.printf("    Gear (raw enum): %d\n", gear);
}

void printDecoded471(const uint8_t* d) {
  float deltaP = s16be(d, 0) / 10.0f;
  float app = u16be(d, 2) / 10.0f;
  float exhPress = u16be(d, 4) / 10.0f;

  Serial.printf("    Injector Delta P: %.1f kPa\n", deltaP);
  Serial.printf("    APP: %.1f %%\n", app);
  Serial.printf("    Exhaust Manifold Pressure: %.1f kPa\n", exhPress);
}

void printDecoded473(const uint8_t* d) {
  uint32_t fuelUsed = u32be(d, 0);

  Serial.printf("    Total Fuel Used: %lu cc\n", (unsigned long)fuelUsed);

  Serial.println("    System states:");
  printBoolField("Rolling Antilag Switch", readBitField(d, 4, 7));
  printBoolField("Antilag Switch", readBitField(d, 4, 6));
  printBoolField("Antilag Output", readBitField(d, 4, 5));
  printBoolField("Traction Control Switch", readBitField(d, 4, 4));
  printBoolField("Primary Fuel Pump Output", readBitField(d, 4, 3));
  printBoolField("Aux 1 Fuel Pump Output", readBitField(d, 4, 2));
  printBoolField("Aux 2 Fuel Pump Output", readBitField(d, 4, 1));
  printBoolField("Aux 3 Fuel Pump Output", readBitField(d, 4, 0));

  printBoolField("Nitrous Enable 1 Switch", readBitField(d, 5, 7));
  printBoolField("Nitrous Enable 1 Output", readBitField(d, 5, 6));
  printBoolField("Nitrous Enable 2 Switch", readBitField(d, 5, 5));
  printBoolField("Nitrous Enable 2 Output", readBitField(d, 5, 4));
  printBoolField("Nitrous Enable 3 Switch", readBitField(d, 5, 3));
  printBoolField("Nitrous Enable 3 Output", readBitField(d, 5, 2));
  printBoolField("Nitrous Enable 4 Switch", readBitField(d, 5, 1));
  printBoolField("Nitrous Enable 4 Output", readBitField(d, 5, 0));

  printBoolField("Nitrous Override 1 Switch", readBitField(d, 6, 7));
  printBoolField("Nitrous Override 1 Output", readBitField(d, 6, 6));
  printBoolField("Nitrous Override 2 Switch", readBitField(d, 6, 5));
  printBoolField("Nitrous Override 2 Output", readBitField(d, 6, 4));
  printBoolField("Nitrous Override 3 Switch", readBitField(d, 6, 3));
  printBoolField("Nitrous Override 3 Output", readBitField(d, 6, 2));
  printBoolField("Nitrous Override 4 Switch", readBitField(d, 6, 1));
  printBoolField("Nitrous Override 4 Output", readBitField(d, 6, 0));

  printBoolField("Water Injection Adv Enable Switch", readBitField(d, 7, 7));
  printBoolField("Water Injection Adv Enable Output", readBitField(d, 7, 6));
  printBoolField("Water Injection Adv Override Switch", readBitField(d, 7, 5));
  printBoolField("Water Injection Adv Override Output", readBitField(d, 7, 4));

  uint8_t cutMethod = d[7] & 0x0F;
  Serial.printf("    Cut Percentage Method: %u\n", cutMethod);
}

void printDecoded(const TrackedFrame& f) {
  switch (f.id) {
    case 0x360: printDecoded360(f.lastData); break;
    case 0x361: printDecoded361(f.lastData); break;
    case 0x362: printDecoded362(f.lastData); break;
    case 0x36A: printDecoded36A(f.lastData); break;
    case 0x370: printDecoded370(f.lastData); break;
    case 0x371: printDecoded371(f.lastData); break;
    case 0x372: printDecoded372(f.lastData); break;
    case 0x3E0: printDecoded3E0(f.lastData); break;
    case 0x3E2: printDecoded3E2(f.lastData); break;
    case 0x3E4: printDecoded3E4(f.lastData); break;
    case 0x469: printDecoded469(f.lastData); break;
    case 0x470: printDecoded470(f.lastData); break;
    case 0x471: printDecoded471(f.lastData); break;
    case 0x473: printDecoded473(f.lastData); break;
    default:
      Serial.println("    No decoder for this frame.");
      break;
  }
}

void printSeenFrames() {
  Serial.println("============================================================");
  Serial.println("Seen tracked Haltech IDs:");

  bool anySeen = false;

  for (size_t i = 0; i < NUM_TRACKED; i++) {
    const TrackedFrame& f = tracked[i];
    if (!f.seen) continue;

    anySeen = true;
    Serial.print("  - ");
    Serial.print(f.name);
    Serial.print(" (");
    printHex11(f.id);
    Serial.printf(")  count=%lu  age=%lums\n",
                  (unsigned long)f.count,
                  (unsigned long)(millis() - f.lastSeenMs));

    printDecoded(f);
    Serial.println();
  }

  if (!anySeen) {
    Serial.println("  - none yet");
    Serial.println();
  }
}

// --------------------------------------------------
// Setup / loop
// --------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("Starting ESP32 TWAI listener...");

  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
      CAN_TX,
      CAN_RX,
      TWAI_MODE_LISTEN_ONLY
  );

  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
  if (err != ESP_OK) {
    Serial.printf("twai_driver_install failed: %d\n", err);
    while (true) delay(1000);
  }

  err = twai_start();
  if (err != ESP_OK) {
    Serial.printf("twai_start failed: %d\n", err);
    while (true) delay(1000);
  }

  Serial.println("TWAI started.");
  Serial.println("Listening for selected Haltech dash IDs...");
  Serial.println();
  delay(5000);
}

void loop() {
  twai_message_t msg;

  if (twai_receive(&msg, 0) == ESP_OK) {
    if (!msg.extd) {
      uint16_t id = msg.identifier & 0x7FF;
      updateTracked(id, msg.data, msg.data_length_code);
    }
  }

  uint32_t now = millis();
  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;
    printSeenFrames();
  }
}