/*
  Example: ProtoBot Default Template
  Boards: ProtoBot with CodeCell C6 Drive

  Overview:
  - Provides a basic starting point for new ProtoBot projects
  - Handles setup, initialization, and timed loop execution
  - The Run() function manages ProtoBot’s control algorithms via the MicroLink App
  - Read the Run() function to time your custom logic code (1–10 Hz)
*/

#include <protobot.h>

ProtoBot myProtoBot;

void setup() {
  Serial.begin(115200);
  myProtoBot.Init();  // Initialize ProtoBot
}

void loop() {
  if (myProtoBot.Run(2)) {  // Run loop at 2 Hz
    // Add your custom logic here
  }
}
