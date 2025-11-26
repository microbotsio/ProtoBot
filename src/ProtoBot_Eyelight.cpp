#include "ProtoBot.h"
#include "ProtoBot_BLE.h"
#include "ProtoBot_Eyelight.h"
#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel ProtoBotDisplay(LED_NUM, 1, NEO_GRB + NEO_KHZ800);

bool hsv_flag = 0, animation_done = 0, dir_last = 0, animation_flag = 1, connect_flag = 0;
uint8_t gradient_speed = 4, animation_num = 0, animation_timer = 0, animation_delay = 0;
uint16_t low_battery = 0, hsv_offset = 30000;
uint8_t tailLen = 2;
extern uint8_t displayOnOFF;
extern bool isAppConnectionReady;

struct CometState {
  int head = 0;         // current LED index
  int dir = 1;          // +1 or -1
  uint32_t lastMs = 0;  // for non-blocking timing
};
CometState gComet;  // shared between normal comet & turn comet

static inline void drawCometFrame(const CometState& s, uint8_t tailLen, uint32_t headColor);

static inline uint32_t gammaColor(uint8_t r, uint8_t g, uint8_t b) {
  return ProtoBotDisplay.gamma32(ProtoBotDisplay.Color(r, g, b));
}

static inline uint32_t scaleColor(uint32_t c, uint8_t k) {  // k: 0..255
  uint8_t r = (c >> 16) & 0xFF;
  uint8_t g = (c >> 8) & 0xFF;
  uint8_t b = c & 0xFF;
  r = (uint16_t)r * k / 255;
  g = (uint16_t)g * k / 255;
  b = (uint16_t)b * k / 255;
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

const uint32_t ORANGE = gammaColor(255, 160, 0);
const uint32_t RED = gammaColor(255, 0, 0);
const uint32_t YELLOW = gammaColor(255, 255, 0);
const uint32_t GREEN = gammaColor(0, 255, 0);
const uint32_t BLUE = gammaColor(0, 0, 255);

void ClearDisplay(void) {
  ProtoBotDisplay.clear();
  ProtoBotDisplay.show();
  animation_done = 0;
  animation_timer = 0;
  animation_num = 0;
}

EyeLight::EyeLight() {
}

void EyeLight::show_DisplayInit(void) {
  ProtoBotDisplay.begin();
  ProtoBotDisplay.setBrightness(LED_BRIGHTNESS);
  ProtoBotDisplay.clear();
  ProtoBotDisplay.show();
  connect_flag = 1;
}

void EyeLight::show_Align(uint16_t intervalMs) {
  static uint32_t headColor = BLUE;
  static int head = 0, dir = 1;
  
  static uint32_t lastMs = 0;
  static uint8_t tailLen = 5;

  uint32_t now = millis();
  if (now - lastMs < intervalMs) return;
  lastMs = now;

  ProtoBotDisplay.clear();

  for (int t = 0; t < tailLen; t++) {
    int idx = head - t * dir;
    if (idx < 0 || idx >= LED_NUM) break;
    uint8_t fade = map(t, 0, tailLen - 1, 255, 40);
    ProtoBotDisplay.setPixelColor(idx, scaleColor(headColor, fade));
  }

  ProtoBotDisplay.show();

  head += dir;
  if (head <= 0 || head >= LED_NUM - 1) {
    dir = -dir;
    if(dir==1){
        headColor = RED;
    }
    else{
        headColor = BLUE;
    }
  }
}

void EyeLight::show_FullColor(uint8_t r, uint8_t g, uint8_t b) {
  ProtoBotDisplay.clear();
  for (uint8_t led_num = 0; led_num < LED_NUM; led_num++) {
    ProtoBotDisplay.setPixelColor(led_num, (ProtoBotDisplay.gamma32(ProtoBotDisplay.Color(r, g, b))));
  }
  if (displayOnOFF != 0) { ProtoBotDisplay.show(); }
}

uint16_t EyeLight::HSV_Fade(uint16_t hsv_min, uint16_t hsv_max) {
  if (hsv_offset > hsv_max) {
    hsv_flag = 1;
  } else if (hsv_offset < hsv_min) {
    hsv_flag = 0;
  } else if (hsv_offset < (hsv_min - 20)) {
    hsv_offset = hsv_min;
    hsv_flag = 0;
  } else {
    //skip
  }
  if (hsv_flag) {
    hsv_offset = hsv_offset - 15;
  } else {
    hsv_offset = hsv_offset + 15;
  }
  return hsv_offset;
}

void EyeLight::show_PowerStatus(uint8_t state) {
  ProtoBotDisplay.clear();
  if (state == STATE_BAT_LOW) {
    //Battery Low State
    if (low_battery < 1000) {
      low_battery++;
      show_Static(500, RED);
    } else {
      ClearDisplay();
    }
  } else {
    low_battery = 0;
    if (state == STATE_BAT_CHRG) {
      //Battery Charging State
      show_Charging(90);
    } else if (state == STATE_BAT_FULL) {
      //Battery Charged State
      show_Static(500, GREEN);
    } else if (state == STATE_USB) {
      show_Static(500, BLUE);
    } else {
      //Skip
    }
  }
}

void EyeLight::show_Charging(uint16_t intervalMs) {
  static uint32_t headColor = YELLOW;
  static int head = 0, dir = 1, head_num = 0;
  
  static uint32_t lastMs = 0;
  static uint8_t tailLen = 2;

  uint32_t now = millis();
  if (now - lastMs < intervalMs) return;
  lastMs = now;

  ProtoBotDisplay.clear();

  for (int t = 0; t < tailLen; t++) {
    int idx = head - t * dir;
    if (idx < 0 || idx >= LED_NUM) break;
    uint8_t fade = map(t, 0, tailLen - 1, 255, 40);
    ProtoBotDisplay.setPixelColor(idx, scaleColor(headColor, fade));
  }

  ProtoBotDisplay.show();

  head += dir;
  if (head <= 0 || head >= LED_NUM - 1) {
    dir = -dir;
    if (tailLen < 4) {
      tailLen++;
    }
    head_num++;
    switch (head_num) {
      case 1:
        headColor = RED;
        break;
      case 2:
        headColor = YELLOW;
        break;
      case 3:
        headColor = GREEN;
        break;
      case 4:
        headColor = YELLOW;
        head_num = 0;
        break;
    }
  }
}

void EyeLight::show_Dizzy(uint16_t intervalMs, uint8_t sparkProb, uint8_t r, uint8_t g, uint8_t b) {
  static uint32_t lastMs = 0;
  uint32_t sparkColor = ProtoBotDisplay.gamma32(ProtoBotDisplay.Color(r, g, b));

  uint32_t now = millis();
  if (now - lastMs < intervalMs) return;
  lastMs = now;

  // Faster decay
  for (int i = 0; i < LED_NUM; i++) {
    uint32_t c = ProtoBotDisplay.getPixelColor(i);
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = c & 0xFF;
    r = (uint16_t)r * 170 / 255;
    g = (uint16_t)g * 170 / 255;
    b = (uint16_t)b * 170 / 255;
    ProtoBotDisplay.setPixelColor(i, (uint32_t)r << 16 | (uint32_t)g << 8 | b);
  }

  // More frequent sparks, anywhere
  if (random(0, 100) < sparkProb) {
    int idx = random(0, LED_NUM);
    ProtoBotDisplay.setPixelColor(idx, sparkColor);
  }

  ProtoBotDisplay.show();
}

void EyeLight::show_Default(uint16_t intervalMs, uint8_t r, uint8_t g, uint8_t b) {
  uint32_t headColor = ProtoBotDisplay.gamma32(ProtoBotDisplay.Color(r, g, b));
  static int head = 0, dir = 1, head_num = 0;

  static uint32_t lastMs = 0;

  uint32_t now = millis();
  if (now - lastMs < intervalMs) return;
  lastMs = now;

  ProtoBotDisplay.clear();

  for (int t = 0; t < tailLen; t++) {
    int idx = head - t * dir;
    if (idx < 0 || idx >= LED_NUM) break;
    uint8_t fade = map(t, 0, tailLen - 1, 255, 40);
    ProtoBotDisplay.setPixelColor(idx, scaleColor(headColor, fade));
  }

  ProtoBotDisplay.show();

  head += dir;
  if (head <= 0 || head >= LED_NUM - 1) {
    dir = -dir;
    if (tailLen < 4) {
      tailLen++;
    }
  }
}

void EyeLight::reset_Eye() {
  tailLen = 2;
}

void EyeLight::show_Static(uint16_t periodMs, uint32_t color) {
  static uint32_t startMs = 0;
  uint32_t now = millis();
  if (startMs == 0) startMs = now;
  bool on = ((now - startMs) / (periodMs / 2)) % 2 == 0;

  ProtoBotDisplay.clear();
  if (on) {
    for (int i = 0; i < LED_NUM; i++) {
      ProtoBotDisplay.setPixelColor(i, color);
    }
  }
  ProtoBotDisplay.show();
}

void EyeLight::show_Color(uint8_t r, uint8_t g, uint8_t b) {
  ProtoBotDisplay.clear();
  for (uint8_t led_num = 0; led_num < LED_NUM; led_num++) {
    ProtoBotDisplay.setPixelColor(led_num, (ProtoBotDisplay.gamma32(ProtoBotDisplay.Color(r, g, b))));
  }
  if (displayOnOFF != 0) { ProtoBotDisplay.show(); }
}

void EyeLight::show_Loading(uint16_t intervalMs, uint8_t tailLen, uint8_t r, uint8_t g, uint8_t b) {
  static int step = 0;
  static bool inward = true;  // true: edges->center, false: center->edges
  static uint32_t lastMs = 0;
  uint32_t color = ProtoBotDisplay.gamma32(ProtoBotDisplay.Color(r, g, b));

  uint32_t now = millis();
  if (now - lastMs < intervalMs) return;
  lastMs = now;

  ProtoBotDisplay.clear();

  const int half = LED_NUM / 2;  // 3 for 6
  int leftIdx, rightIdx;
  int dirLeft, dirRight;  // tail direction (where it extends)

  if (inward) {
    leftIdx = step;                 // 0 -> 2
    rightIdx = LED_NUM - 1 - step;  // 5 -> 3
    dirLeft = -1;                   // tail extends outward (toward index 0)
    dirRight = +1;                  // tail extends outward (toward index 5)
  } else {
    leftIdx = (half - 1) - step;  // 2 -> 0
    rightIdx = (half) + step;     // 3 -> 5
    dirLeft = +1;                 // tail extends toward center
    dirRight = -1;                // tail extends toward center
  }

  // Draw both heads + tails
  for (int t = 0; t < tailLen; t++) {
    uint8_t fade = map(t, 0, tailLen - 1, 255, 60);
    int L = leftIdx + t * dirLeft;
    if (L >= 0 && L < LED_NUM) ProtoBotDisplay.setPixelColor(L, scaleColor(color, fade));
    int R = rightIdx + t * dirRight;
    if (R >= 0 && R < LED_NUM) ProtoBotDisplay.setPixelColor(R, scaleColor(color, fade));
  }

  ProtoBotDisplay.show();

  // Advance
  if (++step >= half) {
    step = 0;
    inward = !inward;
  }
}

void EyeLight::show_Turn(int dir, uint8_t intensity, uint8_t tailLen, uint8_t r, uint8_t g, uint8_t b) {
  uint32_t color = ProtoBotDisplay.gamma32(ProtoBotDisplay.Color(r, g, b));

  if (dir == 0) return;  // no turn, let your normal comet or other mode run

  // Map intensity to interval (faster when higher), ~140ms..40ms
  uint16_t intervalMs = (uint16_t)(140 - (intensity * 100UL / 255));

  // Define the active region based on direction
  int regionL, regionR;
  if (dir < 0) {  // LEFT: use left half (0..2)
    regionL = 0;
    regionR = (LED_NUM / 2) - 1;
  } else {  // RIGHT: use right half (3..5)
    regionL = (LED_NUM / 2);
    regionR = LED_NUM - 1;
  }

  // Initialize head into region if out of bounds (preserve direction)
  if (gComet.head < regionL || gComet.head > regionR) {
    gComet.head = (dir < 0) ? regionR : regionL;  // start at inner edge of that half
    gComet.dir = (dir < 0) ? -1 : +1;             // push outward first
  }

  uint32_t now = millis();
  if (now - gComet.lastMs < intervalMs) {
    // Even if waiting, draw current frame so overlay looks stable
    drawCometFrame(gComet, tailLen, color);
    return;
  }
  gComet.lastMs = now;

  // Draw frame
  drawCometFrame(gComet, tailLen, color);

  // Advance within the region, bounce at region ends
  gComet.head += gComet.dir;
  if (gComet.head <= regionL || gComet.head >= regionR) gComet.dir = -gComet.dir;
}

static inline void drawCometFrame(const CometState& s, uint8_t tailLen, uint32_t headColor) {
  ProtoBotDisplay.clear();
  for (int t = 0; t < tailLen; t++) {
    int idx = s.head - t * s.dir;
    if (idx < 0 || idx >= LED_NUM) break;
    uint8_t fade = map(t, 0, tailLen - 1, 255, 40);  // head→tail fade

    uint8_t r = (headColor >> 16) & 0xFF;
    uint8_t g = (headColor >> 8) & 0xFF;
    uint8_t b = headColor & 0xFF;
    r = (uint16_t)r * fade / 255;
    g = (uint16_t)g * fade / 255;
    b = (uint16_t)b * fade / 255;

    ProtoBotDisplay.setPixelColor(idx, (uint32_t)r << 16 | (uint32_t)g << 8 | b);
  }
  ProtoBotDisplay.show();
}

void EyeLight::show_AvoidUp(uint16_t intervalMsb) {
  
const uint32_t warnCore   = ProtoBotDisplay.gamma32(ProtoBotDisplay.Color(255,   0,   0)); // vivid red
const uint32_t warnMiddle = ProtoBotDisplay.gamma32(ProtoBotDisplay.Color(255, 180,   0)); // bright yellow-orange
const uint32_t warnOuter  = ProtoBotDisplay.gamma32(ProtoBotDisplay.Color(100,   0,   0)); // very dark red


static uint32_t lastMs = 0;
static int radius = -1;  // -1 (idle) -> 0 (center) -> 1 -> 2 -> reset

uint32_t now = millis();
if (now - lastMs < intervalMsb) return;
lastMs = now;

if (radius < 0) radius = 0;

ProtoBotDisplay.clear();

auto drawRing = [&](int r, uint32_t baseColor, uint8_t k) {
  uint32_t c = scaleColor(baseColor, k);
  if (r == 0) {           // center pair
    ProtoBotDisplay.setPixelColor(2, c);
    ProtoBotDisplay.setPixelColor(3, c);
  } else if (r == 1) {    // mid pair
    ProtoBotDisplay.setPixelColor(1, c);
    ProtoBotDisplay.setPixelColor(4, c);
  } else if (r == 2) {    // outer pair
    ProtoBotDisplay.setPixelColor(0, c);
    ProtoBotDisplay.setPixelColor(5, c);
  }
};

// Current ring full brightness; previous rings fade
if (radius == 0) drawRing(0, warnCore,   255);
if (radius == 1) drawRing(1, warnMiddle, 255);
if (radius == 2) drawRing(2, warnOuter,  255);

// trailing fade for the last two rings
if (radius - 1 >= 0) {
  int r = radius - 1;
  drawRing(r, (r==0?warnCore:(r==1?warnMiddle:warnOuter)), 160);
}
if (radius - 2 >= 0) {
  int r = radius - 2;
  drawRing(r, (r==0?warnCore:(r==1?warnMiddle:warnOuter)), 90);
}

ProtoBotDisplay.show();

// Advance radius; reset after reaching edges
if (++radius > 2) radius = -1;  // small pause between bursts

}

bool EyeLight::show_blockLoading(uint16_t intervalMs, uint8_t tailLen, uint8_t r, uint8_t g, uint8_t b) {
  static int step = 0;
  bool inward = true;  // true: edges->center, false: center->edges
  static uint32_t lastMs = 0;
  uint32_t color = ProtoBotDisplay.gamma32(ProtoBotDisplay.Color(r, g, b));

  uint32_t now = millis();
  if (now - lastMs < intervalMs){  
    return false;
  }
  lastMs = now;

  ProtoBotDisplay.clear();

  const int half = LED_NUM / 2;  // 3 for 6
  int leftIdx, rightIdx;
  int dirLeft, dirRight;  // tail direction (where it extends)

  if (inward) {
    leftIdx = step;                 // 0 -> 2
    rightIdx = LED_NUM - 1 - step;  // 5 -> 3
    dirLeft = -1;                   // tail extends outward (toward index 0)
    dirRight = +1;                  // tail extends outward (toward index 5)
  } else {
    leftIdx = (half - 1) - step;  // 2 -> 0
    rightIdx = (half) + step;     // 3 -> 5
    dirLeft = +1;                 // tail extends toward center
    dirRight = -1;                // tail extends toward center
  }

  // Draw both heads + tails
  for (int t = 0; t < tailLen; t++) {
    uint8_t fade = map(t, 0, tailLen - 1, 255, 60);
    int L = leftIdx + t * dirLeft;
    if (L >= 0 && L < LED_NUM) ProtoBotDisplay.setPixelColor(L, scaleColor(color, fade));
    int R = rightIdx + t * dirRight;
    if (R >= 0 && R < LED_NUM) ProtoBotDisplay.setPixelColor(R, scaleColor(color, fade));
  }

  ProtoBotDisplay.show();

  // Advance
  if (++step >= (half+5)) {
    step = 0;
    return true;
  }
  else{
    return false;
  }
}

