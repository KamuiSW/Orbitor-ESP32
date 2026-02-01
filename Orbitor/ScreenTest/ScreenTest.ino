#include <LovyanGFX.hpp>
#include <math.h>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI _bus;

public:
  LGFX() {
    // SPI bus
    {
      auto cfg = _bus.config();
      cfg.spi_host   = VSPI_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 10000000;
      cfg.freq_read  = 10000000;
      cfg.spi_3wire  = true;
      cfg.use_lock   = true;

      cfg.pin_sclk = 18;  // SCL/CLK
      cfg.pin_mosi = 19;  // SDA/DIN (MOSI)
      cfg.pin_miso = -1;
      cfg.pin_dc   = 22;  // DC

      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    // Panel
    {
      auto cfg = _panel.config();
      cfg.pin_cs   = 23;   // CS
      cfg.pin_rst  = 21;   // RST
      cfg.pin_busy = -1;

      cfg.panel_width  = 240;
      cfg.panel_height = 240;
      cfg.offset_x = 0;
      cfg.offset_y = 0;

      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.readable = false;

      _panel.config(cfg);
    }

    setPanel(&_panel);
  }
};

LGFX tft;

void setup() {
  Serial.begin(115200);
  delay(200);

  tft.init();
  tft.setRotation(0);

  tft.fillScreen(TFT_RED);   delay(500);
  tft.fillScreen(TFT_GREEN); delay(500);
  tft.fillScreen(TFT_BLUE);  delay(500);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("GC9A01 TEST");

  tft.drawCircle(120, 120, 110, TFT_WHITE);
  tft.fillCircle(120, 120, 6, TFT_YELLOW);
}

void loop() {}
