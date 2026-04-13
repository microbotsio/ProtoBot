/*
  Example: ProtoBot + External SHTC3 Temperature & Humidity Sensor
  Boards: ProtoBot (CodeCell C6 Drive)

  Overview:
  - Demonstrates how to interface an external I2C sensor with ProtoBot
  - Connect the SHTC3 sensor to the ProtoBot expansion header(SDA,SCL,GND,3V3)
  - Reads temperature and humidity values
  - Sends live sensor readings to the MicroLink App (ensure "Logs" are enabled)
  - The eye will turn red when humidity exceeds 75% RH & green when it is below this level
*/

#include <ProtoBot.h>
#include "Adafruit_SHTC3.h"

Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();
ProtoBot myProtoBot;
char myMessage[18];

void setup() {
  Serial.begin(115200);
  myProtoBot.Init();

  if (!shtc3.begin()) {
    Serial.println(">> SHTC3 Sensor: Not detected, check wiring");
    while (1) {
      delay(1);
    }
  }
  Serial.println(">> SHTC3 Sensor: Initialized successfully");
}

void loop() {
  if (myProtoBot.Run(2)) {  // Run loop at 2 Hz (max 10 Hz)
    sensors_event_t humidity, temp;
    shtc3.getEvent(&humidity, &temp);  // Read latest readings

    sprintf(myMessage, "%.1f°C / %.1f%%rH", temp.temperature, humidity.relative_humidity);
    myProtoBot.PrintLog(myMessage);  // Show value on MicroLink App

    
    if(humidity.relative_humidity > 75.0){ //Humidty detected
      myProtoBot.EyeColor(0xFF, 0, 0);//Turn eye red
    }
    else{
      myProtoBot.EyeColor(0, 0xFF, 0);//Turn eye green
    }
  }
}
