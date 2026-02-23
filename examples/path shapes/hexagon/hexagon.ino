/*
  Example: ProtoBot Hexagon Shape (Manual Driving)
  Board: ProtoBot (CodeCell C6 Drive inside)

  Overview:
  - Demonstrates how to manually create a hexagon shape using the Drive() and Rotate() functions
  - Shows how repeating forward movement with small rotations, can creates polygon shapes

  How it works:
  - The robot drives forward
  - Then rotates approximately 60°
  - This sequence repeats continuously
  - After 6 repetitions, a hexagon shape is formed
  
  Tip:
  - Lower Run() frequency → longer sides → bigger shape
  - Higher Run() frequency → shorter sides → smaller shape
*/

#include <ProtoBot.h>

ProtoBot myProtoBot;  // Create ProtoBot object
int step = 0;

void setup() {
  Serial.begin(115200);  // Start USB serial communication (for debugging)
  myProtoBot.Init();     // Initialize robot (motors, sensors, internal systems)
}

void loop() {
  if (myProtoBot.Run(3)) {  // Run ProtoBot service loop at 3 Hz (~every 333 ms)
    step++;
    switch (step) {
      case 1:
        myProtoBot.Drive(100, 90);  // Drive forward
        break;
      case 2:
        myProtoBot.Rotate(150);  // 180 - 60deg = 120 internal -> 60deg turn
        step = 0;
        break;
    }
  }
}