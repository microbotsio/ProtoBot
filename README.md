# ProtoBot

<img src="https://microbots.io/cdn/shop/files/protobotpalmsize_7cef90ce-c1ab-44e0-adb8-4fcbe82cad47_800x.png?v=1767707072" alt="ProtoBot" width="300" align="right" style="margin-left: 20px;">

## Build Your Own Smart Palm-Sized Robot

**ProtoBot** is a smart, palm-sized robot designed for anyone excited about building robots.

Follow the step-by-step build guide, assemble your robot, then:

- Drive it with the MicroLink app  
- Automate it with block coding  
- Customize it with 3D-printed parts  
- Expand it with external sensors  
- Or dive deeper using the Arduino library  

ProtoBot is offered in **two maker levels**, making it accessible to everyone — from curious students to experienced makers.

---

## Choose Your Maker Level

### Beginner (No Soldering Required)

- Comes pre-soldered  
- Simply screw the parts together  
- No tools required  
- Start driving instantly  

Perfect for students, classrooms, and first-time builders.

---

### Pro (Full Build Experience)

- Solder the motors and electronics yourself  
- Assemble the robot from scratch  
- Optionally 3D-print your own custom shell  

Requires:
- Soldering iron  
- (Optional) 3D printer for custom shell  

Perfect for makers who want the complete hands-on experience.

---

## What You’ll Build & Explore

With ProtoBot, you will:

- Assemble a fully working robot with motors, wheels, sensors, and lights  
- Learn how compact robotics systems are built  
- Experiment with motion sensing, light sensing, and proximity detection  

Using the free **MicroLink App** (Android & iOS), you can:

### Drive & Rewind
Control ProtoBot with a joystick or retrace its path.

### Shape & Automate
Use beginner-friendly block coding to:
- Draw shapes  
- Run custom actions  
- Create repeatable behaviors  

### Dodge & Avoid
Use built-in sensors to react to obstacles.

### Sense & Balance
Watch how ProtoBot uses motion sensing to:
- Steer  
- Align  
- Drive patterns  
- Maintain heading  

### Record & Expand
Attach external I²C sensors (temperature, humidity, etc.) and log real-time data.

### Learn & Hack
Go further with the Arduino library to:
- Create custom behaviours  
- Read sensors directly  
- Control LEDs  
- Program your own missions  

---

## What’s Inside Every ProtoBot Kit

- **CodeCell C6 Drive** with FlexPCB sticker-base  
  (clean wiring, RGB eye-lights, proximity sensing & motor connections)
- 4 × N20 motors  
- Aluminium wheels with silicone tyres  
- Breadboard mount for expansion  
- All required screws  
- Free screwdriver  

Everything needed to bring your robot to life.

---

## Made for Makers

ProtoBot’s body and accessories are **fully open-source**.

You can:

- 3D-print the shell at home in your favorite colors  
- Modify the design files  
- Create new attachments  
- Design your own add-ons  

The included breadboard allows you to attach:

- Extra I²C sensors  
- Displays  
- Power modules  
- Custom circuits  

ProtoBot becomes a tiny, powerful playground for experimentation.

---

# ProtoBot Arduino Library

This repository contains the **ProtoBot Arduino library**, used for direct programming and advanced control.

It allows you to:

- Control motors
- Generate shapes
- Read sensors
- Control RGB LEDs
- Build autonomous behaviours

---

## Repository Structure

```
ProtoBot/
│
├── examples/              # Arduino example sketches
├── circuits/              # Circuit reference diagrams
├── mounts/                # Mechanical mounts
├── 3d_prints/             # Chassis and accessory STL files
├── accessories/           # Optional printable add-ons
└── src/                   # ProtoBot library source
```

You can use the included files to:

- 3D print your own ProtoBot  
- Modify mounts  
- Print accessories  
- Extend hardware with custom expansions  

---

## Installation

1. Install the ProtoBot library  
2. Install required dependencies:
   - CodeCell library  
   - BLEOTA  
   - Adafruit_NeoPixel  
3. Select **ESP32-C6** board in Arduino IDE  
4. Upload an example from `/examples/`

---

## Basic Usage Example

```cpp
#include <ProtoBot.h>

ProtoBot myProtoBot;

void setup() {
  Serial.begin(115200);
  myProtoBot.Init();
}

void loop() {
  if (myProtoBot.Run(3)) {
    myProtoBot.Drive(100, 90);  // Drive forward
  }
}
```

`Run(loop_timer)` controls how often your logic executes.

- Lower frequency → longer movements → larger shapes  
- Higher frequency → shorter movements → smaller shapes  

---

## Manual Control API

### Core System

```cpp
void Init();
bool Run(uint8_t loop_timer);
void PrintLog(char *message);
```

---

### Motion Control

```cpp
void Drive(uint8_t speed, float angle);
void Rotate(float rotate_angle);
void Stop();
void SetSpeed(uint8_t level);
```

---

### Predefined Shape Movement

```cpp
void DriveSquare(uint8_t speed_level, uint8_t shape_size, bool loop);
void DriveCircle(uint8_t speed_level, uint8_t shape_size, bool loop);
void DriveTriangle(uint8_t speed_level, uint8_t shape_size, bool loop);
void DriveInfinity(uint8_t speed_level, uint8_t shape_size, bool loop);
```

---

### LED Control

```cpp
void EyeColor(uint8_t red, uint8_t green, uint8_t blue);
```

---

### Orientation

```cpp
void AlignNorth(bool status);
```

---

### Sensor Access

```cpp
bool ReadTouch();
uint8_t ReadBattery();
uint16_t ReadProximityFront();
uint16_t ReadProximityBase();
uint16_t ReadLightFront();
uint16_t ReadLightBase();
float ReadHeading();
void ReadMotion(float &roll, float &pitch, float &yaw);
```

---

# Dependencies

ProtoBot depends on:

- CodeCell library  
- BLEOTA library  
- Adafruit_NeoPixel  

Ensure all dependencies are installed before compiling.

---

# Attribution & Licensing

## ProtoBot Library

The **ProtoBot library was entirely coded by microbots**.

---

## CodeCell Library Attribution

This `CodeCell` library contains various features, including device initialization, power management, light sensing, and motion sensing.

The VCNL4040 light sensor code does not rely on any external libraries.

Some of the BNO085 motion-sensor functions were adapted from the SparkFun BNO08x Arduino Library and the official library provided by CEVA for the SH2 sensor hub.

The SparkFun BNO08x library, originally written by Nathan Seidle and adjusted by Pete Lewis at SparkFun Electronics, is released under the MIT License. Significant modifications were made to adapt it and integrate into the `CodeCell` library.

Additionally, this project incorporates the official CEVA SH2 sensor hub library files, licensed under the Apache License 2.0.

CEVA’s notice:

> This software is licensed from CEVA, Inc.  
> Copyright (c) CEVA, Inc. and its licensors. All rights reserved.  
> CEVA and the CEVA logo are trademarks of CEVA, Inc.

---

ProtoBot is made to support your learning journey — from building and assembling your robot to soldering, coding, and experimenting with real sensors and hardware.
