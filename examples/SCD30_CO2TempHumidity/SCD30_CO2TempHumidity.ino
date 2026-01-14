/*
  Example: ProtoBot + External SCD-30 CO₂, Temperature & Humidity Sensor
  Boards: ProtoBot (CodeCell C6 Drive)

  Overview:
  - Demonstrates how to interface the Adafruit SCD-30 NDIR CO₂ sensor with ProtoBot
  - Connect the SCD-30 to the ProtoBot expansion header (SDA, SCL, GND, 3V3)
  - Reads CO₂ concentration (ppm), temperature, and humidity
  - Sends live sensor readings to the MicroLink App (ensure "Logs" are enabled)
*/

#include <ProtoBot.h>
#include "Adafruit_SCD30.h"

Adafruit_SCD30 scd30;
ProtoBot myProtoBot;
char myMessage[32];

void setup() {
  Serial.begin(115200);
  myProtoBot.Init();

  if (!scd30.begin()) {
    Serial.println(">> SCD-30 Sensor: Not detected, check wiring");
    while (1) delay(1);
  }

  Serial.println(">> SCD-30 Sensor: Initialized successfully");

  // Optional SCD30 settings:
  scd30.setMeasurementInterval(2);  // 2 seconds (default 2–1800)
}

void loop() {
  if (myProtoBot.Run(2)) {  // Run loop at 2 Hz (max 10 Hz)

    if (scd30.dataReady()) {
      if (scd30.read()) {

        float co2 = scd30.CO2;
        float temp = scd30.temperature;
        float hum = scd30.relative_humidity;

        sprintf(myMessage, "%.0fppm|%.1f°C|%.1f%%", co2, temp, hum);
        myProtoBot.PrintLog(myMessage);  // Send to MicroLink Logs
      }
    }
  }
}
