#ifndef PROTOBOT_EYELIGHT_H
#define PROTOBOT_EYELIGHT_H

#include <Arduino.h>
void ClearDisplay(void);

class EyeLight {
private:

public:
  EyeLight();

  void show_DisplayInit();
  void show_PowerStatus(uint8_t state);
  void show_Static(uint16_t periodMs, uint32_t color);
  void show_Charging(uint16_t intervalMs);
  void show_Default(uint16_t intervalMs, uint8_t r, uint8_t g, uint8_t b);
  void show_Loading(uint16_t intervalMs, uint8_t tailLen, uint8_t r, uint8_t g, uint8_t b);
  void show_Turn(int dir, uint8_t intensity, uint8_t tailLen, uint8_t r, uint8_t g, uint8_t b);
  void show_Dizzy(uint16_t intervalMs, uint8_t sparkProb, uint8_t r, uint8_t g, uint8_t b);
  void reset_Eye();
  void show_Align(uint16_t intervalMs);
  void show_FullColor(uint8_t r, uint8_t g, uint8_t b);
  void show_AvoidUp(uint16_t intervalMs);
  void show_Color(uint8_t r, uint8_t g, uint8_t b);
  uint16_t HSV_Fade(uint16_t hsv_min, uint16_t hsv_max);
  bool show_blockLoading(uint16_t intervalMs, uint8_t tailLen, uint8_t r, uint8_t g, uint8_t b);
};


#endif