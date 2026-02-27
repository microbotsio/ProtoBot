/*
  Example: Scrolling Message on 0.91" 128x32 OLED Display
  Boards: ProtoBot with CodeCell C6 Drive

  Overview:
  - Demonstrates how to control an external I2C OLED display with ProtoBot
  - Connect the OLED display to the ProtoBot expansion header (SDA,SCL,GND,3V3)
  - Displays a scrolling text message across the screen
  - Uses Adafruit_SSD1306 and Adafruit_GFX libraries for graphics control
*/

#include <ProtoBot.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
#define OLED_ADDR     0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Message and scroll parameters
const char* MESSAGE = "ProtoBot says Hello World!";
int16_t textX;
int16_t textY;
int16_t textW, textH;

ProtoBot myProtoBot;
char myMessage[18];

void setup() {
  Serial.begin(115200);
  myProtoBot.Init();

  // Initialize OLED
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(">> OLED Display: Not detected at 0x3C!");
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setTextWrap(false);

  // Calculate initial text position for centering
  int16_t x1, y1;
  display.getTextBounds(MESSAGE, 0, 0, &x1, &y1, (uint16_t*)&textW, (uint16_t*)&textH);
  textY = (SCREEN_HEIGHT - textH) / 2 - y1;
  textX = SCREEN_WIDTH;

  Serial.println(">> OLED Display: Ready - Scrolling Message");
}

void loop() {
  if (myProtoBot.Run(10)) {  // Run loop at 10 Hz (maximum recommended)
    display.clearDisplay();
    display.setCursor(textX, textY);
    display.print(MESSAGE);
    display.display();

    textX -= 10;  // Adjust scrolling speed (pixels per loop)
    if (textX < -textW) {
      textX = SCREEN_WIDTH;  // Restart scroll from right edge
    }
  }
}