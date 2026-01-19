/*
  Example: ProtoBot + External SGP40 Air Quality (VOC - Volatile Organic Compounds) Sensor
  Boards: ProtoBot (CodeCell C6 Drive)

  Overview:
  - Demonstrates how to interface an external I2C sensor with ProtoBot
  - Connect the SGP40 sensor to the ProtoBot expansion header (SDA, SCL, GND, 3V3)
  - Reads raw VOC index values using Adafruit SGP40 library
  - Sends live VOC readings to the MicroLink App (ensure "Logs" are enabled)

  Air Quality Interpretation (RAW SGP40 ticks):
  -------------------------------------------------------
    0 – 10,000    = Good air quality
    10,001 – 20,000 = Moderate
    20,001+       = Poor
  -------------------------------------------------------
  Note: SGP40 raw output is *not* a calibrated AQI value.
*/

#include <ProtoBot.h>
#include <Adafruit_SGP40.h>

ProtoBot myProtoBot;
Adafruit_SGP40 sgp40;

char myMessage[48];

void setup() {
  Serial.begin(115200);
  myProtoBot.Init();

  if (!sgp40.begin()) {
    Serial.println(">> SGP40 Sensor: Not detected, check wiring");
    while (1) delay(1);
  }
  Serial.println(">> SGP40 Sensor: Initialized successfully");
}

void loop() {
  if (myProtoBot.Run(2)) { // Run loop at 2 Hz (max 10 Hz)

    uint16_t voc_raw = sgp40.measureRaw(); // Raw VOC ticks (0–65535)

    const char* quality = "";

    if (voc_raw < 10000) {
      quality = "Good";
    } else if (voc_raw < 20000) {
      quality = "Moderate";
    } else {
      quality = "Poor";
    }

    // Send labelled output to MicroLink log
    sprintf(myMessage, "VOC: %u (%s)", voc_raw, quality);
    myProtoBot.PrintLog(myMessage);

    Serial.println(myMessage); // Optional: also print to Serial Monitor
  }
}
