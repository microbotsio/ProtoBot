/*
  Example: ProtoBot Reactive Eyes on 1.5" 128x128 OLED SSD1327 Display
  Boards: ProtoBot with CodeCell C6 Drive

  Behavior:
  - Pupils look left/right based on heading (ReadHeading)
  - Eyes open wider with speed (ReadSpeed)
  - Blinks automatically (timing tuned to still be visible even if you only update at Run(10))
*/


#include <ProtoBot.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1327.h>

ProtoBot myProtoBot;

// ---- OLED (SSD1327 128x128) ----
#define OLED_W 128
#define OLED_H 128
Adafruit_SSD1327 display(OLED_W, OLED_H, &Wire, -1);

// Display Address
static const uint8_t OLED_ADDR_PRIMARY  = 0x3D;
static const uint8_t OLED_ADDR_FALLBACK = 0x3C;

// Grayscale colors (0..15)
static const uint8_t C_BLACK = 0;
static const uint8_t C_WHITE = 15;
static const uint8_t C_GRAY1 = 4;
static const uint8_t C_GRAY2 = 8;
static const uint8_t C_GRAY3 = 12;

// ---------- helpers ----------
static inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
static inline int clampi(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
static float wrap180(float deg) {
  while (deg >= 180.0f) deg -= 360.0f;
  while (deg <  -180.0f) deg += 360.0f;
  return deg;
}

// ---------- blink state ----------
static uint32_t nextBlinkMs  = 0;
static uint32_t blinkStartMs = 0;
static bool blinking         = false;

// ---------- drawing primitives ----------
static void drawBrow(int x0, int y0, int x1, int y1, uint8_t col, int thickness) {
  for (int i = 0; i < thickness; i++) {
    display.drawLine(x0, y0 + i, x1, y1 + i, col);
  }
}

static void drawEye(
  int cx, int cy,
  int eyeW, int eyeH,
  int pupilDx, int pupilDy,
  float open01,
  bool angry
) {
  const int rx = eyeW / 2;
  const int ry = eyeH / 2;

  // Eye white base
  display.fillRoundRect(cx - rx, cy - ry, eyeW, eyeH, ry, C_WHITE);
  display.drawRoundRect(cx - rx, cy - ry, eyeW, eyeH, ry, C_GRAY3);

  // Pupil
  const int pupilR = (eyeH >= 40) ? 7 : 6;
  int px = cx + pupilDx;
  int py = cy + pupilDy;

  px = clampi(px, cx - (rx - pupilR - 3), cx + (rx - pupilR - 3));
  py = clampi(py, cy - (ry - pupilR - 3), cy + (ry - pupilR - 3));

  display.fillCircle(px, py, pupilR, C_BLACK);
  display.fillCircle(px - 2, py - 2, 1, C_GRAY2);

  // Eyelids
  open01 = clampf(open01, 0.0f, 1.0f);

  // Keep your original "soft lids" look, but a touch stronger so blink reads well
  int cover = (int)((1.0f - open01) * (float)(eyeH * 0.65f)); // stronger than eyeH/2
  cover = clampi(cover, 0, eyeH);

  if (cover > 0) {
    // top lid
    display.fillRoundRect(cx - rx, cy - ry, eyeW, cover + 3, ry, C_BLACK);
    // bottom lid
    display.fillRoundRect(cx - rx, cy + ry - cover - 3, eyeW, cover + 3, ry, C_BLACK);
  }

  // Brows
  int browY = cy - ry - 12;
  if (angry) {
    drawBrow(cx - rx + 2, browY + 7, cx - 2,       browY + 2, C_WHITE, 2);
    drawBrow(cx + 2,       browY + 2, cx + rx - 2, browY + 7, C_WHITE, 2);
  } else {
    drawBrow(cx - rx + 2, browY + 5, cx + rx - 2, browY + 5, C_WHITE, 2);
  }
}

static void drawMouth(float speed, float turnMag01) {
  int y = 104; // pushed down a bit to make room for big eyes

  if (speed < 5.0f) {
    display.drawLine(58, y, 70, y, C_WHITE);
    display.drawLine(58, y + 1, 70, y + 1, C_GRAY2);
  } else if (turnMag01 > 0.75f) {
    display.drawCircle(64, y, 5, C_WHITE);
    display.drawCircle(64, y, 4, C_GRAY2);
  } else if (speed < 35.0f) {
    display.drawLine(56, y, 72, y, C_WHITE);
    display.drawPixel(55, y - 1, C_WHITE);
    display.drawPixel(73, y - 1, C_WHITE);
  } else {
    display.drawLine(56, y, 72, y, C_WHITE);
    display.drawLine(56, y + 1, 72, y + 1, C_WHITE);
  }
}

// ---------- face render ----------
static void drawFace(float headingDeg, float speedVal) {
  float h = wrap180(headingDeg);
  float speed = clampf(speedVal, 0.0f, 100.0f);

  float speed01   = clampf(speed / 70.0f, 0.0f, 1.0f);
  float turnMag01 = clampf(fabsf(h) / 90.0f, 0.0f, 1.0f);

  // Pupils: left/right with heading
  float look = clampf(h / 50.0f, -1.0f, 1.0f);
  int pupilDx = (int)(look * 14.0f);
  int pupilDy = (speed < 5.0f) ? 4 : 0;

  // Eyes open wider with speed
  float openBase = 0.45f + (0.55f * speed01); // 0.45..1.0
  openBase -= 0.04f * turnMag01;              // tiny squint on hard turns
  openBase = clampf(openBase, 0.15f, 1.0f);

  // "Focused" brows mainly on hard turns
  bool angry = (fabsf(h) > 75.0f);

  // Blink (tuned for 10 Hz updates)
  float openNow = openBase;
  if (blinking) {
    uint32_t t = millis() - blinkStartMs;

    // At 10 Hz (~100ms per frame), we need a blink that lasts ~300ms to be obvious
    if (t < 120) {
      openNow = 0.0f; // fully closed hold (guarantees at least one "closed" frame)
    } else if (t < 320) {
      openNow = openBase * ((t - 120) / 200.0f); // reopen over 200ms
    } else {
      blinking = false;

      // Schedule next blink
      uint32_t minGap = (speed < 5.0f) ? 3200 : 1400;
      uint32_t maxGap = (speed < 5.0f) ? 5200 : 2800;

      uint32_t span = (maxGap > minGap) ? (maxGap - minGap) : 0;
      uint32_t gap = minGap + (span ? (uint32_t)random(0, (long)span) : 0);

      nextBlinkMs = millis() + gap;
    }
  }

  // Draw frame
  display.clearDisplay();

  // Border
  display.drawRoundRect(2, 2, OLED_W - 4, OLED_H - 4, 10, C_GRAY1);

  // Mouth
  drawMouth(speed, turnMag01);

  // Big eyes (positioned to leave room for mouth)
  const int eyeCy  = 58;
  const int leftCx = 40;
  const int rightCx = 88;

  const int eyeW = 56;
  const int eyeH = 44;

  drawEye(leftCx,  eyeCy, eyeW, eyeH, pupilDx, pupilDy, openNow, angry);
  drawEye(rightCx, eyeCy, eyeW, eyeH, pupilDx, pupilDy, openNow, angry);

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  myProtoBot.Init();

  Wire.begin();
  Wire.setClock(400000);

  bool ok = display.begin(OLED_ADDR_PRIMARY);
  if (!ok) ok = display.begin(OLED_ADDR_FALLBACK);

  if (!ok) {
    Serial.println("SSD1327 not found. Check address (0x3C/0x3D) + wiring.");
    while (1) delay(10);
  }

  display.setRotation(0);   
  display.setContrast(255);

  delay(700);

  randomSeed((uint32_t)esp_random() ^ (uint32_t)micros());

  // First blink fairly soon after boot
  nextBlinkMs = millis() + (uint32_t)random(800, 1800);
}

void loop() {
  if (myProtoBot.Run(10)) {//Loop at 10Hz

    float heading = myProtoBot.ReadHeading();
    float speed   = (float)myProtoBot.ReadSpeed();

    // Blink scheduler 
    if (!blinking && millis() >= nextBlinkMs) {
      blinking = true;
      blinkStartMs = millis();
    }

    drawFace(heading, speed);
  }
}