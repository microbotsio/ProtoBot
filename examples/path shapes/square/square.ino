/*
  Example: ProtoBot Square Shape Driving Demo
  Board: ProtoBot (CodeCell C6 Drive inside)

  Overview:
  - Demonstrates how to make ProtoBot drive in a square shape
  - Uses the built-in DriveSquare() function
  - Shows how to control shape size and repetition
  - Includes an optional manual movement example

  DriveSquare(shape-size, loop)
    shape-size  : 0–100  (larger value = larger square)
    loop        : SHAPE_ONCE = run once
                  SHAPE_LOOP = repeat continuously

  What this example does:
  - ProtoBot drives in a square once
  - Shape size is set to 60

  Other built-in shapes you can try:
    - myProtoBot.DriveCircle();     
    - myProtoBot.DriveInfinity();   
    - myProtoBot.DriveTriangle();   
*/

#include <ProtoBot.h>

ProtoBot myProtoBot;      // Create ProtoBot object
bool shape_step = false;  // Used only for manual movement example

void setup() {
  Serial.begin(115200);              // Start USB serial communication (for debugging)
  myProtoBot.Init();                 // Initialize robot (motors, sensors, internal systems)
  myProtoBot.runWithoutBluetooth();  //Turn on robot without BLE connection
}

void loop() {
  if (myProtoBot.Run(1)) {  // Run ProtoBot service loop at 1 Hz

    myProtoBot.DriveSquare(60, SHAPE_LOOP);  // Drive in a square shape with 60% size

    // Uncomment below to control the robot step-by-step
    // shape_step = !shape_step;
    // if (shape_step) {
    //   myProtoBot.Drive(100, 90);  // Drive forward at 100% speed
    // } else {
    //   myProtoBot.Rotate(180);  // Rotate 90 degrees
    // }
  }
}
