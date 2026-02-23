/*
  Example: ProtoBot Cross/Plus Shape Driving Demo
  Board: ProtoBot (CodeCell C6 Drive inside)

  Overview:
  - Demonstrates how to manually create a cross/plus shape using Drive() and Rotate() functions
  - Shows how step-by-step motion sequences work
  - Repeats continuously to form a cross pattern

  What this example does:
  - ProtoBot drives forward
  - Rotates 90°
  - Repeats the pattern to form a cross
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
  if (myProtoBot.Run(3)) {  // Run ProtoBot service loop at 3 Hz (~every 333 ms)
    
    step++;                 // Move to next step in the sequence
    
    switch (step) {
      case 1:
        myProtoBot.Drive(100, 90);  // Move right
        break;
      case 2:
        myProtoBot.Rotate(180);  // Rotate +90°
        break;
      case 3:
        myProtoBot.Drive(100, 90);  // Move right again
        break;
      case 4:
        myProtoBot.Rotate(0);  // Rotate -90°
        break;
      case 5:
        myProtoBot.Drive(100, 90);  // Move right again
        break;
      case 6:
        myProtoBot.Rotate(0);  // Rotate -90°
        step = 0;  // Restart sequence (continuous cross)
        break;
    }
  }
}