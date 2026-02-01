#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <time.h>
#include <math.h>
#include <pgmspace.h>

#include "earth_240x120.h"

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"

static const char* NTP_SERVER = "pool.ntp.org";
static const long  GMT_OFFSET_SEC = 0;
static const int   DST_OFFSET_SEC = 0;

static const uint32_t SPI_HZ = 40000000;   // try 40000000 if stable

// render at 120 -> scale 2x -> 240 full
static const int N = 120;

static const float AMBIENT      = 0.72f;   // base brightness
static const float DIFFUSE      = 0.28f;   // sunlight contrast
static const float NIGHT_FACTOR = 0.86f;   // night side brightness (closer to 1 = brighter)
static const float LIMB_MIN     = 0.86f;   // rim brightness (closer to 1 = less dark rim)

static const float SPIN_RAD_PER_SEC = 2.0f * (float)M_PI / 20.0f; // 1 rev / 20s

static const int CROSS_HALF = 8;
static const int CIRCLE_R   = 12;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI _bus;
public:
  LGFX() {
    { auto cfg = _bus.config();
      cfg.spi_host   = VSPI_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = SPI_HZ;
      cfg.freq_read  = 10000000;
      cfg.spi_3wire  = true;
      cfg.use_lock   = true;

      cfg.pin_sclk = 18;
      cfg.pin_mosi = 19;
      cfg.pin_miso = -1;
      cfg.pin_dc   = 22;

      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    { auto cfg = _panel.config();
      cfg.pin_cs   = 23;
      cfg.pin_rst  = 21;
      cfg.pin_busy = -1;
      cfg.panel_width  = 240;
      cfg.panel_height = 240;
      cfg.offset_x = 0;
      cfg.offset_y = 0;

      cfg.invert    = true;
      cfg.rgb_order = false;
      cfg.readable  = false;

      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};

LGFX tft;

static const int SCALE_Q15 = 32767;
static int16_t xs[N*N];     // screen disk x (right)
static int16_t ys[N*N];     // screen disk y (up)
static uint8_t mask[N*N];   // inside disk
static uint16_t line2x[240];

static inline uint8_t clamp_u8(int v){ return (v<0)?0:(v>255?255:(uint8_t)v); }
static inline uint16_t pack565(uint8_t r,uint8_t g,uint8_t b){
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static inline void unpack565(uint16_t c, int &r, int &g, int &b) {
  int r5 = (c >> 11) & 0x1F;
  int g6 = (c >> 5)  & 0x3F;
  int b5 = (c)       & 0x1F;
  r = (r5 * 255) / 31;
  g = (g6 * 255) / 63;
  b = (b5 * 255) / 31;
}

static void precomputeDisk() {
  const float c = (N - 1) * 0.5f;
  const float R = N * 0.5f;
  for (int y=0;y<N;y++){
    for (int x=0;x<N;x++){
      float nx=(x-c)/R;
      float ny=-(y-c)/R;
      float r2=nx*nx+ny*ny;
      int i=y*N+x;
      if (r2>1.0f){ mask[i]=0; xs[i]=ys[i]=0; }
      else { mask[i]=1; xs[i]=(int16_t)lroundf(nx*SCALE_Q15); ys[i]=(int16_t)lroundf(ny*SCALE_Q15); }
    }
  }
}

static inline uint16_t sampleEarthTex(float lat, float lon) {
  float u = (lon + (float)M_PI) * (1.0f / (2.0f * (float)M_PI));
  float v = ((float)M_PI * 0.5f - lat) * (1.0f / (float)M_PI);

  if (u < 0.0f) u += 1.0f;
  if (u >= 1.0f) u -= 1.0f;
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;

  int tx = (int)(u * (EARTH_TEX_W - 1));
  int ty = (int)(v * (EARTH_TEX_H - 1));
  uint32_t idx = (uint32_t)ty * EARTH_TEX_W + (uint32_t)tx;
  return pgm_read_word(&earth_tex[idx]);
}

static inline bool crosshairPixel(int x, int y) {
  int dx = x - 120;
  int dy = y - 120;
  if (dy == 0 && abs(dx) <= CROSS_HALF) return true;
  if (dx == 0 && abs(dy) <= CROSS_HALF) return true;

  int d2 = dx*dx + dy*dy;
  int r2 = CIRCLE_R*CIRCLE_R;
  // thin ring band
  if (abs(d2 - r2) <= (2*CIRCLE_R)) return true;
  return false;
}

// X = lon 0 at center (front), Y = east, Z = north
static bool haveTime = false;

static void solarDeclinationAndEoT(const tm& t, float& decl, float& eot_min) {
  int dayOfYear = t.tm_yday + 1;
  float hour = (float)t.tm_hour + (float)t.tm_min/60.0f + (float)t.tm_sec/3600.0f;

  float gamma = 2.0f * (float)M_PI / 365.0f * (dayOfYear - 1 + (hour - 12.0f)/24.0f);

  eot_min = 229.18f * (0.000075f
        + 0.001868f * cosf(gamma)
        - 0.032077f * sinf(gamma)
        - 0.014615f * cosf(2*gamma)
        - 0.040849f * sinf(2*gamma));

  decl = 0.006918f
       - 0.399912f * cosf(gamma)
       + 0.070257f * sinf(gamma)
       - 0.006758f * cosf(2*gamma)
       + 0.000907f * sinf(2*gamma)
       - 0.002697f * cosf(3*gamma)
       + 0.001480f * sinf(3*gamma);
}

static void computeSunVector(float& sx, float& sy, float& sz) {
  tm t;
  if (!getLocalTime(&t, 10)) {
    haveTime = false;
    sx=0.65f; sy=-0.35f; sz=0.68f;
  } else {
    haveTime = true;

    float decl, eot;
    solarDeclinationAndEoT(t, decl, eot);

    float minutesUTC = (float)(t.tm_hour*60 + t.tm_min) + (float)t.tm_sec/60.0f;

    // subsolar longitude, degrees east positive
    float lon_deg = (720.0f - minutesUTC - eot) / 4.0f;
    while (lon_deg > 180.0f) lon_deg -= 360.0f;
    while (lon_deg < -180.0f) lon_deg += 360.0f;

    float lon = lon_deg * (float)M_PI / 180.0f;

    float c = cosf(decl);
    sx = c * cosf(lon);
    sy = c * sinf(lon);
    sz = sinf(decl);
  }

  float inv = 1.0f / sqrtf(sx*sx + sy*sy + sz*sz);
  sx*=inv; sy*=inv; sz*=inv;
}

void setup() {
  tft.init();
  tft.setRotation(0);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);

  precomputeDisk();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < 8000) {
    delay(100);
  }
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
}

void loop() {
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  float dt = (lastMs == 0) ? 0.016f : (now - lastMs) * 0.001f;
  lastMs = now;

  static float yaw = 0.0f;
  yaw += SPIN_RAD_PER_SEC * dt;
  if (yaw > 2.0f*(float)M_PI) yaw -= 2.0f*(float)M_PI;

  float cy = cosf(yaw);
  float sy = sinf(yaw);

  static uint32_t lastSunMs = 0;
  static float sx = 0.65f, suny = -0.35f, sz = 0.68f;
  if (now - lastSunMs > 500) {
    computeSunVector(sx, suny, sz);
    lastSunMs = now;
  }

  for (int sySmall = 0; sySmall < N; sySmall++) {
    int y0 = sySmall * 2;

    for (int sxSmall = 0; sxSmall < N; sxSmall++) {
      int i = sySmall * N + sxSmall;
      uint16_t out = 0x0000;

      if (mask[i]) {
        float Yc = xs[i] / (float)SCALE_Q15;              // right
        float Zc = ys[i] / (float)SCALE_Q15;              // up
        float Xc = sqrtf(1.0f - (Yc*Yc + Zc*Zc));         // front

        float Xw = Xc * cy - Yc * sy;
        float Yw = Xc * sy + Yc * cy;
        float Zw = Zc;

        // lat/lon from world point
        float lat = asinf(Zw);
        float lon = atan2f(Yw, Xw);

        uint16_t tex = sampleEarthTex(lat, lon);

        int rr, gg, bb;
        unpack565(tex, rr, gg, bb);

        float d = Xw*sx + Yw*suny + Zw*sz;           // [-1,1]
        float diffuse = (d > 0.0f) ? d : 0.0f;

        float light = AMBIENT + DIFFUSE * diffuse;
        if (light > 1.0f) light = 1.0f;
        if (d < 0.0f) light *= NIGHT_FACTOR;

        float limb = LIMB_MIN + (1.0f - LIMB_MIN) * (Xc * Xc);

        float f = light * limb;

        out = pack565(
          clamp_u8((int)(rr * f)),
          clamp_u8((int)(gg * f)),
          clamp_u8((int)(bb * f))
        );
      }

      int dx = sxSmall * 2;
      line2x[dx]     = out;
      line2x[dx + 1] = out;
    }

    for (int x = 0; x < 240; x++) {
      if (crosshairPixel(x, y0))     line2x[x] = 0xFFFF;
    }
    tft.pushImage(0, y0, 240, 1, line2x);

    for (int x = 0; x < 240; x++) {
      if (crosshairPixel(x, y0 + 1)) line2x[x] = 0xFFFF;
    }
    tft.pushImage(0, y0 + 1, 240, 1, line2x);
  }

  if (!haveTime) {
    tft.fillCircle(10, 230, 3, TFT_RED);
  }

  delay(2);
}
