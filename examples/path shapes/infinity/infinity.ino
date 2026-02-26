/*
  Example: ProtoBot Infinity Shape Driving Demo
  Board: ProtoBot (CodeCell C6 Drive inside)

  Overview:
  - Demonstrates how to make ProtoBot drive in a figure-8 shape
  - Uses the built-in DriveInfinity() function
  - Shows how to control speed, size, and repetition

  DriveInfinity(speed-level, shape-size, loop)
    speed-level : SPEED_FAST/SPEED_INTERMEDIATE/SPEED_SLOW
    shape-size  : 0–100  (larger value = larger square)
    loop        : SHAPE_ONCE = run once
                  SHAPE_LOOP = repeat continuously

  Other built-in shapes you can try:
    - myProtoBot.DriveCircle();     
    - myProtoBot.DriveSquare();   
    - myProtoBot.DriveTriangle();   
*/

#include <ProtoBot.h>

ProtoBot myProtoBot;  // Create ProtoBot object

void setup() {
  Serial.begin(115200);  // Start USB serial communication (for debugging)
  myProtoBot.Init();     // Initialize robot (motors, sensors, internal systems)
}

void loop() {
  if (myProtoBot.Run(3)) {  // Run ProtoBot service loop at 3 Hz (~every 333 ms)
    myProtoBot.DriveInfinity(SPEED_INTERMEDIATE, 100, SHAPE_LOOP);  // Drive in a square shape with 100% shape size
  }
}
