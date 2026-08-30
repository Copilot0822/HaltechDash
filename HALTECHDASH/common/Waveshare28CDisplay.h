#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_timer.h"

namespace waveshare28c {

static constexpr uint16_t LCD_WIDTH = 480;
static constexpr uint16_t LCD_HEIGHT = 480;

static constexpr int I2C_SDA = 15;
static constexpr int I2C_SCL = 7;
static constexpr uint8_t TCA9554_ADDR = 0x20;

static constexpr int LCD_SPI_MOSI = 1;
static constexpr int LCD_SPI_CLK = 2;
static constexpr int LCD_BACKLIGHT = 6;

static constexpr uint8_t EXIO_LCD_RESET = 1;
static constexpr uint8_t EXIO_TOUCH_RESET = 2;
static constexpr uint8_t EXIO_LCD_CS = 3;
static constexpr uint8_t EXIO_MISC_ENABLE = 8;

static spi_device_handle_t spiHandle = nullptr;
static esp_lcd_panel_handle_t panelHandle = nullptr;
static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static void* buf1 = nullptr;
static uint8_t tcaOutputState = 0x00;

struct InitCmd {
  uint8_t cmd;
  uint8_t data[16];
  uint8_t len;
  uint16_t delayMs;
};

static const InitCmd ST7701_INIT[] = {
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, {0x08}, 1, 0},
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, {0x3B, 0x00}, 2, 0},
    {0xC1, {0x10, 0x0C}, 2, 0},
    {0xC2, {0x07, 0x0A}, 2, 0},
    {0xC7, {0x00}, 1, 0},
    {0xCC, {0x10}, 1, 0},
    {0xCD, {0x08}, 1, 0},
    {0xB0, {0x05, 0x12, 0x98, 0x0E, 0x0F, 0x07, 0x07, 0x09,
            0x09, 0x23, 0x05, 0x52, 0x0F, 0x67, 0x2C, 0x11}, 16, 0},
    {0xB1, {0x0B, 0x11, 0x97, 0x0C, 0x12, 0x06, 0x06, 0x08,
            0x08, 0x22, 0x03, 0x51, 0x11, 0x66, 0x2B, 0x0F}, 16, 0},
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, {0x5D}, 1, 0},
    {0xB1, {0x3E}, 1, 0},
    {0xB2, {0x81}, 1, 0},
    {0xB3, {0x80}, 1, 0},
    {0xB5, {0x4E}, 1, 0},
    {0xB7, {0x85}, 1, 0},
    {0xB8, {0x20}, 1, 0},
    {0xC1, {0x78}, 1, 0},
    {0xC2, {0x78}, 1, 0},
    {0xD0, {0x88}, 1, 0},
    {0xE0, {0x00, 0x00, 0x02}, 3, 0},
    {0xE1, {0x06, 0x30, 0x08, 0x30, 0x05, 0x30, 0x07, 0x30,
            0x00, 0x33, 0x33}, 11, 0},
    {0xE2, {0x11, 0x11, 0x33, 0x33, 0xF4, 0x00, 0x00, 0x00,
            0xF4, 0x00, 0x00, 0x00}, 12, 0},
    {0xE3, {0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE4, {0x44, 0x44}, 2, 0},
    {0xE5, {0x0D, 0xF5, 0x30, 0xF0, 0x0F, 0xF7, 0x30, 0xF0,
            0x09, 0xF1, 0x30, 0xF0, 0x0B, 0xF3, 0x30, 0xF0}, 16, 0},
    {0xE6, {0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE7, {0x44, 0x44}, 2, 0},
    {0xE8, {0x0C, 0xF4, 0x30, 0xF0, 0x0E, 0xF6, 0x30, 0xF0,
            0x08, 0xF0, 0x30, 0xF0, 0x0A, 0xF2, 0x30, 0xF0}, 16, 0},
    {0xE9, {0x36, 0x01}, 2, 0},
    {0xEB, {0x00, 0x01, 0xE4, 0xE4, 0x44, 0x88, 0x40}, 7, 0},
    {0xED, {0xFF, 0x10, 0xAF, 0x76, 0x54, 0x2B, 0xCF, 0xFF,
            0xFF, 0xFC, 0xB2, 0x45, 0x67, 0xFA, 0x01, 0xFF}, 16, 0},
    {0xEF, {0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, {}, 0, 120},
    {0x3A, {0x66}, 1, 0},
    {0x36, {0x00}, 1, 0},
    {0x35, {0x00}, 1, 0},
    {0x29, {}, 0, 0},
};

static bool tcaWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool tcaSet(uint8_t pin, bool high) {
  const uint8_t mask = (uint8_t)(1u << (pin - 1));
  if (high) {
    tcaOutputState |= mask;
  } else {
    tcaOutputState &= (uint8_t)~mask;
  }
  return tcaWrite(0x01, tcaOutputState);
}

static bool tcaBegin() {
  tcaOutputState = 0x00;
  return tcaWrite(0x01, tcaOutputState) && tcaWrite(0x03, 0x00);
}

static bool spiWrite(bool data, uint8_t value) {
  spi_transaction_t tx = {};
  tx.cmd = data ? 1 : 0;
  tx.addr = value;
  return spi_device_polling_transmit(spiHandle, &tx) == ESP_OK;
}

static bool panelCommand(uint8_t cmd, const uint8_t* data, uint8_t len) {
  if (!spiWrite(false, cmd)) {
    return false;
  }
  for (uint8_t i = 0; i < len; i++) {
    if (!spiWrite(true, data[i])) {
      return false;
    }
  }
  return true;
}

static bool initSt7701() {
  spi_bus_config_t bus = {};
  bus.mosi_io_num = LCD_SPI_MOSI;
  bus.miso_io_num = -1;
  bus.sclk_io_num = LCD_SPI_CLK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = 64;

  esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return false;
  }

  spi_device_interface_config_t dev = {};
  dev.command_bits = 1;
  dev.address_bits = 8;
  dev.mode = SPI_MODE0;
  dev.clock_speed_hz = 40000000;
  dev.spics_io_num = -1;
  dev.queue_size = 1;

  err = spi_bus_add_device(SPI2_HOST, &dev, &spiHandle);
  if (err != ESP_OK) {
    return false;
  }

  tcaSet(EXIO_LCD_CS, false);
  delay(10);

  for (const InitCmd& entry : ST7701_INIT) {
    if (!panelCommand(entry.cmd, entry.data, entry.len)) {
      return false;
    }
    if (entry.delayMs > 0) {
      delay(entry.delayMs);
    }
  }

  tcaSet(EXIO_LCD_CS, true);
  return true;
}

static bool initRgbPanel() {
  const int dataPins[16] = {5, 45, 48, 47, 21, 14, 13, 12, 11, 10, 9, 46, 3, 8, 18, 17};

  esp_lcd_rgb_panel_config_t rgb = {};
  rgb.clk_src = LCD_CLK_SRC_PLL240M;
  rgb.timings.pclk_hz = 30 * 1000 * 1000;
  rgb.timings.h_res = LCD_WIDTH;
  rgb.timings.v_res = LCD_HEIGHT;
  rgb.timings.hsync_pulse_width = 8;
  rgb.timings.hsync_back_porch = 10;
  rgb.timings.hsync_front_porch = 50;
  rgb.timings.vsync_pulse_width = 2;
  rgb.timings.vsync_back_porch = 18;
  rgb.timings.vsync_front_porch = 8;
  rgb.timings.flags.pclk_active_neg = 0;
  rgb.data_width = 16;
  rgb.bits_per_pixel = 16;
  rgb.num_fbs = 1;
  rgb.bounce_buffer_size_px = 10 * LCD_HEIGHT;
  rgb.psram_trans_align = 64;
  rgb.hsync_gpio_num = 38;
  rgb.vsync_gpio_num = 39;
  rgb.de_gpio_num = 40;
  rgb.pclk_gpio_num = 41;
  rgb.disp_gpio_num = -1;
  for (uint8_t i = 0; i < 16; i++) {
    rgb.data_gpio_nums[i] = dataPins[i];
  }
  rgb.flags.fb_in_psram = true;

  if (esp_lcd_new_rgb_panel(&rgb, &panelHandle) != ESP_OK) {
    return false;
  }
  return esp_lcd_panel_reset(panelHandle) == ESP_OK && esp_lcd_panel_init(panelHandle) == ESP_OK;
}

static void lvFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color) {
  esp_lcd_panel_draw_bitmap(panelHandle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color);
  lv_disp_flush_ready(disp);
}

static void lvTick(void*) {
  lv_tick_inc(2);
}

class Display {
public:
  bool begin() {
    Serial.println("Display init: I2C expander");
    Wire.begin(I2C_SDA, I2C_SCL);
    if (!tcaBegin()) {
      Serial.println("Display init failed: TCA9554 I2C expander not found");
      return false;
    }

    Serial.println("Display init: reset sequence");
    tcaSet(EXIO_MISC_ENABLE, false);
    tcaSet(EXIO_TOUCH_RESET, true);
    tcaSet(EXIO_LCD_RESET, false);
    delay(10);
    tcaSet(EXIO_LCD_RESET, true);
    delay(50);

    Serial.println("Display init: ST7701 command init");
    if (!initSt7701()) {
      Serial.println("Display init failed: ST7701 command init");
      return false;
    }

    Serial.println("Display init: RGB panel");
    if (!initRgbPanel()) {
      Serial.println("Display init failed: RGB panel");
      return false;
    }

    Serial.println("Display init: backlight");
    ledcAttach(LCD_BACKLIGHT, 20000, 10);
    setBacklight(85);

    Serial.println("Display init: LVGL");
    lv_init();
    const uint32_t bufPixels = LCD_WIDTH * 40;
    buf1 = heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf1 == nullptr) {
      buf1 = heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (buf1 == nullptr) {
      Serial.println("Display init failed: LVGL draw buffer allocation");
      return false;
    }

    lv_disp_draw_buf_init(&drawBuf, buf1, nullptr, bufPixels);
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = LCD_WIDTH;
    dispDrv.ver_res = LCD_HEIGHT;
    dispDrv.flush_cb = lvFlush;
    dispDrv.draw_buf = &drawBuf;
    lv_disp_drv_register(&dispDrv);

    const esp_timer_create_args_t tickArgs = {
        .callback = &lvTick,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t tickTimer = nullptr;
    if (esp_timer_create(&tickArgs, &tickTimer) != ESP_OK ||
        esp_timer_start_periodic(tickTimer, 2000) != ESP_OK) {
      Serial.println("Display init failed: LVGL tick timer");
      return false;
    }

    Serial.println("Display init OK");
    return true;
  }

  void loop() {
    lv_timer_handler();
  }

  void setBacklight(uint8_t percent) {
    if (percent > 100) {
      percent = 100;
    }
    uint32_t duty = map(percent, 0, 100, 0, 1023);
    ledcWrite(LCD_BACKLIGHT, duty);
  }
};

}  // namespace waveshare28c
