/*
  Example: ProtoBot 5-pointed Star Shape Driving Demo
  Board: ProtoBot (CodeCell C6 Drive inside)

  Overview:
  - Demonstrates how to manually create a 5-pointed star shape using Drive() and Rotate() functions
  - Shows how step-by-step motion sequences work
  - Repeats continuously to form a star pattern

  What this example does:
  - ProtoBot drives forward
  - Rotates 36°
  - ProtoBot drives backward
  - Rotates 108°
  - Repeats the pattern to form a 5-pointed star
  - Loops continuously
*/

#include <ProtoBot.h>

ProtoBot myProtoBot;  // Create ProtoBot object
int step = 0;         // Step counter for movement sequence

void setup() {
  Serial.begin(115200);  // Start USB serial (for debugging/logs)
  myProtoBot.Init();     // Initialize robot (motors, sensors, etc)
}

void loop() {
  if (myProtoBot.Run(3)) { // Run ProtoBot service loop at 2 Hz 
    
    step++; // Move to next step in the sequence
    
    switch (step) {
      case 1:
        myProtoBot.Drive(100, 90);  // Move forward
        break;
      case 2:
        myProtoBot.Rotate(126);  // Rotate 90°+36°
        break;
      case 3:
        myProtoBot.Drive(100, 270);  // Drive backward
        break;
      case 4:
        myProtoBot.Rotate(342);  // Rotate 90°-108°
        step = 0;  // Restart sequence (continue remaining star sides)
        break;
    }
  }
}