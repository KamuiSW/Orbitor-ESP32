#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI _bus;

public:
  LGFX() {
    // SPI bus config
    {
      auto cfg = _bus.config();
      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;   // 40 MHz
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = true;
      cfg.use_lock   = true;

      cfg.pin_sclk = 18;   // SCL
      cfg.pin_mosi = 19;   // SDA (MOSI)
      cfg.pin_miso = -1;
      cfg.pin_dc   = 22;   // DC

      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    // Panel config
    {
      auto cfg = _panel.config();
      cfg.pin_cs   = 23;
      cfg.pin_rst  = 21;
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
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 20);
  tft.println("ESP32 OK");

  tft.drawCircle(120, 120, 100, TFT_GREEN);
  tft.fillCircle(120, 120, 5, TFT_RED);
}

void loop() {}
