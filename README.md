# ProtoBot

<img src="https://microbots.io/cdn/shop/files/protobotpalmsize_7cef90ce-c1ab-44e0-adb8-4fcbe82cad47_800x.png?v=1767707072" alt="ProtoBot" width="300" align="right" style="margin-left: 20px;">

## Overview

ProtoBot is a smart, palm-sized educational robot designed to teach practical robotics through hands-on building and programming.

It combines mechanical assembly, embedded electronics, sensors, and software into a compact system that learners can fully understand, modify, and extend.

This repository contains the **ProtoBot Arduino library**, example sketches, and hardware files that support further experimentation, customization, and development.

More information: http://microbots.io/protobot

---

## Learning Focus

ProtoBot supports learning across multiple areas of robotics:

- Mechanical assembly and 3D printing  
- Basic electronics and soldering  
- Motor control and movement logic  
- Sensor integration  
- Block-based automation  
- Programming via the Arduino library  
- Data logging and expansion with I²C devices  

To support different experience levels, ProtoBot is available in three configurations:

- **Beginner (Pre-Soldered)**  
  Electronics are pre-assembled. You complete mechanical assembly and begin programming immediately.

- **Pro**  
  Requires soldering the motors and electronics before full assembly.

- **Pro – 3D Print-It-Yourself**  
  Same as the Pro version, but you print your own robot shell using the provided open-source 3D files.

All versions share the same hardware architecture and software ecosystem, allowing development from basic assembly to full hardware customization.

---

## Hardware Overview

Each ProtoBot includes:

- **CodeCell C6 Drive module**, integrating:
  - ESP32-C6  
  - USB-C battery charging  
  - Light & proximity sensing  
  - 9-axis IMU motion sensor  
  - Motor drivers  

- 4 × N20 motors  
- Aluminium wheels with silicone tyres  
- FlexPCB front base (RGB eye LEDs, proximity sensing & motor connections)  
- Breadboard mount for expansion  
- Assembly hardware  

Once assembled, ProtoBot provides a complete robotics platform in a compact form factor.

---

# Repository Structure

This repository includes:

- ProtoBot Arduino Library  
- Ready-to-run Arduino examples  
- Hardware files and user guide  
- 3D-printable mechanical parts and accessories  

Mechanical components are open-source and intended for modification and experimentation.

---

# Installation (USB Programming via Arduino IDE)

ProtoBot can be programmed and updated via the Arduino IDE using its open-source library.

---

## 1. Install ESP32 Board Support

If this is your first time using Arduino with ESP32:

1. Open **Arduino IDE**  
2. Go to **File → Preferences**  
3. In **Additional Board Manager URLs**, add:

```
https://dl.espressif.com/dl/package_esp32_index.json
```

4. Click **OK**
5. Go to **Tools → Board → Board Manager**
6. Search for **ESP32**
7. Install or update to the latest version

---

## 2. Install ProtoBot Library

1. Go to **Sketch → Include Library → Manage Libraries**
2. Search for **ProtoBot**
3. Install the latest version  
   (If already installed, ensure it is up to date)

---

## 3. Install Required Dependencies

Ensure the following libraries are installed:

- CodeCell  
- DriveCell  
- BLEOTA  
- Adafruit_NeoPixel  

Install them via **Library Manager** if needed.

---

## 4. Select the Board

ProtoBot uses the CodeCell C6 Drive module.

Go to **Tools → Board** and select:

```
ESP32C6 Dev Module
```

Set the following Tools Settings:

- **USB CDC On Boot** → Enabled  
- **CPU Clock Speed** → 160 MHz  
- **Flash Size** → 8MB  
- **Partition Scheme** → 8M with spiffs (3MB APP / 1.5MB SPIFFS) 

Go to **Tools → Port** and select the correct COM port of your ProtoBot robot.

---

## 5. Upload an Example

1. Open **File → Examples → ProtoBot**
2. Choose an example sketch
3. Click **Upload**

You are now ready to program and customize ProtoBot.

---

# Manual Control API

The ProtoBot library exposes direct control functions for motors, sensors, and LEDs.

## Core System

```cpp
void Init();
bool Run(uint8_t loop_timer);
void PrintLog(char *message);
```

- `Init()` initializes motors, sensors, and internal systems
- `Run()` provides a timed execution structure
- `PrintLog()` outputs sensor logging or debug information via the MicroLink App

---

## Motion Control

```cpp
void Drive(uint8_t speed, float angle);
void Rotate(float rotate_angle);
void Stop();
void SetSpeed(uint8_t level);
```

- `Drive(speed, angle)`  
  - Speed: 0–100  
  - Angle: 0–360°  

- `Rotate(angle)` rotates in place.
- `Stop()` halts all motors.
- `SetSpeed(level)` adjusts the motor speed preset.

---

## Path Shape Movement

```cpp
void DriveSquare(uint8_t speed_level, uint8_t shape_size, bool loop);
void DriveCircle(uint8_t speed_level, uint8_t shape_size, bool loop);
void DriveTriangle(uint8_t speed_level, uint8_t shape_size, bool loop);
void DriveInfinity(uint8_t speed_level, uint8_t shape_size, bool loop);
```

These functions demonstrate how forward motion combined with controlled rotation produces geometric paths.

Parameters:

- `speed_level` – preset motor speed (e.g. `SPEED_FAST`, `SPEED_INTERMEDIATE`, `SPEED_SLOW`)  
- `shape_size` – range 1–100 to adjust the scale  
- `loop` – repetition control (`SHAPE_ONCE` or `SHAPE_LOOP`)  

These functions are useful for understanding sequencing and motion control logic.

---

## LED Control

```cpp
void EyeColor(uint8_t red, uint8_t green, uint8_t blue);
```

Controls the color of the six front RGB LEDs.

---

## Orientation Control

```cpp
void AlignNorth(bool status);
```

Uses the IMU to maintain heading alignment when enabled.

---

## Sensor Access

```cpp
bool ReadTap();
uint8_t ReadBattery();
uint16_t ReadProximityFront();
uint16_t ReadProximityBase();
uint16_t ReadLightFront();
float ReadHeading();
void ReadMotion(float &roll, float &pitch, float &yaw);
```

Provides access to:

- Tap detection  
- Battery level  
- Proximity (front & base)  
- Ambient light  
- Heading  
- Roll, pitch, yaw  

These functions can be combined to implement obstacle avoidance, reactive lighting, or orientation-based behaviour.

---

# Example Sketches

The `/examples` folder includes demonstrations of:

- Square, triangle, and custom path shapes  
- Manual drive control via joystick  
- Sensor-based reactions  
- External I²C sensor and display integration  
- Arduino OTA usage  

Examples are structured to be readable and educational for beginners.

---

# 3D Prints & Hardware

The `/hardware` folder provides:

- Printable chassis parts  
- Mechanical mounts  
- Expansion accessories  
- Circuit reference diagrams  

You are encouraged to modify and extend the robot's mechanical design.

---

# Dependencies

ProtoBot depends on the following libraries:

- CodeCell  
- DriveCell  
- BLEOTA  
- Adafruit_NeoPixel  

Ensure all required libraries are installed and updated before compiling examples.

---

# Attribution & Third-Party Components

## ProtoBot Library

The **ProtoBot library was entirely developed by microbots** and builds upon the CodeCell library as its core.

The CodeCell library provides:

- Peripheral initialization  
- Power & charge management  
- Light sensing  
- Motion sensing  
- Motor drivers  

Some of the BNO085 9-axis sensor-fusion motion-sensing functionality was adapted from:

- **SparkFun BNO08x Arduino Library** (MIT License)  
- **CEVA SH2 Sensor Hub Library** (Apache License 2.0)  

The SparkFun BNO08x library was originally written by Nathan Seidle and adjusted by Pete Lewis at SparkFun Electronics. It was modified for integration into the CodeCell library.

The CEVA SH2 sensor hub library is licensed under the Apache License 2.0.

CEVA notice:

> This software is licensed from CEVA, Inc.  
> Copyright (c) CEVA, Inc. and its licensors. All rights reserved.  
> CEVA and the CEVA logo are trademarks of CEVA, Inc.

Full license texts are provided in the repository’s LICENSE and NOTICE files.

---

## Additional Libraries

ProtoBot also uses:

- **BLEOTA** – bootloader OTA functionality  
- **Adafruit_NeoPixel** – control of the six front RGB LEDs  

Please refer to their respective repositories for license details.

---

ProtoBot is intended as an educational robotics platform supporting learning through building, programming, and experimentation.
