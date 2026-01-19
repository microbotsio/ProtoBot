/*
  Example: ProtoBot + External VEML7700 Lux Sensor
  Boards: ProtoBot (CodeCell C6 Drive)

  Overview:
  - Demonstrates how to interface an external I2C sensor with ProtoBot
  - Connect the VEML7700 sensor to the ProtoBot expansion header (SDA,SCL,GND,3V3)
  - Reads ambient light (lux) values
  - Sends live sensor readings to the MicroLink App (ensure "Logs" are enabled)
*/

#include <ProtoBot.h>
#include <Adafruit_VEML7700.h>

Adafruit_VEML7700 veml = Adafruit_VEML7700();
ProtoBot myProtoBot;
char myMessage[24];

void setup() {
  Serial.begin(115200);
  myProtoBot.Init();

  if (!veml.begin()) {
    Serial.println(">> VEML7700 Sensor: Not detected, check wiring");
    while (1) {
      delay(1);
    }
  }
  Serial.println(">> VEML7700 Sensor: Initialized successfully");

  // Optional: configure gain & integration time
  veml.setGain(VEML7700_GAIN_1);  
  veml.setIntegrationTime(VEML7700_IT_100MS);
}

void loop() {
  if (myProtoBot.Run(2)) {  // Run loop at 2 Hz (max 10 Hz)
    float lux = veml.readLux();  // Read ambient light level

    sprintf(myMessage, "Lux: %.1f lx", lux);
    myProtoBot.PrintLog(myMessage);  // Show value on MicroLink App
  }
}
