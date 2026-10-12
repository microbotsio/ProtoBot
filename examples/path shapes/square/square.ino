/*
  Example: ProtoBot Square Shape Driving Demo
  Board: ProtoBot (CodeCell C6 Drive inside)

  Overview:
  - Demonstrates how to make ProtoBot drive in a square shape
  - Uses the built-in DriveSquare() function
  - Shows how to control speed, size, and repetition
  - Includes an optional manual movement example

  DriveSquare(shape-size, loop)
    shape-size  : 0–100  (larger value = larger square)
    loop        : SHAPE_ONCE = run once
                  SHAPE_LOOP = repeat continuously

  What this example does:
  - ProtoBot drives in a square once
  - Shape size is set to 60

  Other built-in shapes you can try:
    - myProtoBot.DriveCircle(60, SHAPE_LOOP);     
    - myProtoBot.DriveInfinity(60, SHAPE_LOOP);   
    - myProtoBot.DriveTriangle(60, SHAPE_LOOP);   
*/

#include <ProtoBot.h>

ProtoBot myProtoBot;  // Create ProtoBot object
int step = 0;         // Used only for manual movement example

void setup() {
  Serial.begin(115200);  // Start USB serial communication (for debugging)
  myProtoBot.Init();     // Initialize robot (motors, sensors, internal systems)
  myProtoBot.runWithoutBluetooth(); //Turn on robot without BLE connection
}

void loop() {
  if (myProtoBot.Run(3)) {  // Run ProtoBot service loop at 3 Hz (~every 333 ms)

    myProtoBot.DriveSquare(60, SHAPE_LOOP);  // Drive in a square shape with 60% size

    // Uncomment below to control the robot step-by-step
    // step++;
    // switch (step) {
    //   case 1:
    //     myProtoBot.Drive(100, 90); // Drive forward at 100% speed
    //     break;
    //   case 2:
    //     myProtoBot.Rotate(180); // Rotate 90 degrees
    //     step = 0;  // Restart sequence
    //     break;
    // }
  }
}
