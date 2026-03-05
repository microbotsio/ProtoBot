/*
  Example: ProtoBot Thermal Camera Demo
  Board: ProtoBot (with CodeCell C6 Drive)

  External I²C Devices:
    - MLX90640 Thermal Camera
    - 128x128 SSD1327 OLED Display

  3D-printed OLED mount files can be downloaded here: https://github.com/microbotsio/ProtoBot/tree/main/hardware/mounts/OLED_Holder

  Behavior:
    - Reads a thermal image frame from the MLX90640
    - Displays the frame on the OLED
*/

#include <Wire.h>
#include <Adafruit_MLX90640.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1327.h>

#include <ProtoBot.h>

ProtoBot myProtoBot;

Adafruit_MLX90640 mlx;
float frame[32 * 24];

// --- SSD1327 (typically 128x128) ---
#define OLED_W 128
#define OLED_H 128
Adafruit_SSD1327 display(OLED_W, OLED_H, &Wire, -1); // reset pin = -1 if not used

// Thermal image scaling: 32x24 -> 128x96 using 4x scale
static const uint8_t SCALE = 4;
static const uint8_t IMG_W = 32 * SCALE; // 128
static const uint8_t IMG_H = 24 * SCALE; // 96
static const uint8_t IMG_X = (OLED_W - IMG_W) / 2;  // 0
static const uint8_t IMG_Y = (OLED_H - IMG_H) / 2;  // 16

static inline uint8_t clamp_u8(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return (uint8_t)v;
}

/*
  Faster version of your draw function:
  - Still auto-contrast each frame
  - Avoids a divide per pixel by precomputing k = 15/range
  - Reduces index math a bit
*/
void drawThermalToOLED(const float *f) {
  // Find min/max for auto-contrast
  float tmin = 1000.0f;
  float tmax = -1000.0f;

  for (int i = 0; i < 32 * 24; i++) {
    float t = f[i];
    if (t < tmin) tmin = t;
    if (t > tmax) tmax = t;
  }

  float range = tmax - tmin;
  if (range < 0.1f) range = 0.1f;

  // Precompute scale factor: gray = (t - tmin) * (15 / range)
  const float k = 15.0f / range;

  // Clear only the image area
  display.fillRect(IMG_X, IMG_Y, IMG_W, IMG_H, 0);

  // Overlay text
  display.setCursor(0, 0);
  display.setTextColor(SSD1327_WHITE);
  display.print(tmin, 1);
  display.print("..");
  display.print(tmax, 1);
  display.println(" C");

  int idx = 0;
  int py = IMG_Y;

  for (uint8_t y = 0; y < 24; y++) {
    int px = IMG_X;
    for (uint8_t x = 0; x < 32; x++, idx++) {
      int g = (int)((f[idx] - tmin) * k + 0.5f); // 0..15-ish
      uint8_t gray = (g < 0) ? 0 : (g > 15 ? 15 : (uint8_t)g);

      display.fillRect(px, py, SCALE, SCALE, gray);
      px += SCALE;
    }
    py += SCALE;
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  myProtoBot.Init();

  // --- OLED init ---
  if (!display.begin(0x3D)) { // try 0x3C if your board is wired that way
    Serial.println("SSD1327 not found (try 0x3C?)");
    while (1) delay(10);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1327_WHITE);
  display.setCursor(0, 0);
  display.println("MLX90640 -> OLED");
  display.display();

  // --- MLX90640 init ---
  if (!mlx.begin(0x33, &Wire)) {
    Serial.println("MLX90640 not found!");
    while (1) delay(10);
  }
  Serial.println("Found Adafruit MLX90640");

  mlx.setMode(MLX90640_CHESS);
  mlx.setResolution(MLX90640_ADC_16BIT);
  mlx.setRefreshRate(MLX90640_4_HZ);
}

void loop() {
  if (myProtoBot.Run(1)) { // 1 Hz for thermal update
    if (mlx.getFrame(frame) != 0) {
      Serial.println("MLX frame read failed");
      return;
    }
    drawThermalToOLED(frame);
  }
}
