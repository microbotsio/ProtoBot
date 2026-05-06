#include <CodeCell.h>
#include "ProtoBot.h"
#include "ProtoBot_Ble.h"
#include "ProtoBot_Eyelight.h"
#include <EEPROM.h>
#include <BLEOTA.h>

#if defined(ARDUINO_ESP32C6_DEV)
#else
#include <DriveCell.h>

DriveCell Motor1(C3_IN1_pin1, C3_IN1_pin2);
DriveCell Motor2(C3_IN2_pin1, C3_IN2_pin2);
#endif

CodeCell myCodeCell;
EyeLight myEyeLight;

extern bool eyelight_ambient_enable, BlockSave, eep_save, shapeOnOFF, avoidOnOFF, FrontAvoidOnOFF, faceNorthOnOFF, isAppConnectionReady, shape_loop, BlockProgramLoop, BlockProgramEnable;
extern uint8_t BlockIndex, rx_joystick_x, rx_joystick_y, shape_num, reverse_button, first_connect, touchGesture, speed_reduction, displayOnOFF, displayRed, displayGreen, displayBlue;
extern uint16_t BlockTimer, eepTimer, sleepTimer, shape_thr, shape_timer;
extern float circle_angle;
extern uint8_t BlockProgramCmd[5];
extern uint16_t BlockProgramData[5][3];

ProtoBot::ProtoBot() {
}

void ProtoBot::Init() {
  uint8_t error_timer = 0;

  myCodeCell.LED_SetBrightness(10);  //Turn on CodeCell LED
  myCodeCell.LED(0, 0xFF, 0);
  delay(500);

  myCodeCell.Init(LIGHT + MOTION_ROTATION + MOTION_ACCELEROMETER + MOTION_GYRO);

  Serial.print(">> ProtoBot v" SW_VERSION);
  Serial.println(" Booting.. ");

  error_timer = 0;

  while (EyeSensInit() == 1) {
    delay(10);
    //FrontAvoidReset();

    error_timer++;
    if (error_timer >= 50U) {
      Serial.println(">> Error: EyeLight Sensor not found - Check Hardware");
      break;  // Exit the while loop
    }
  }


#if defined(ARDUINO_ESP32C6_DEV)
  myCodeCell.Drive1.Init();
  myCodeCell.Drive2.Init();
#else
  Motor1.Init();
  Motor2.Init();
#endif

  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println(">> ProtoBot: Failed to initialize EEPROM Region");
  }
  EEPROM.get(EEPROM_FIRST_CONNECT, first_connect);
  EEPROM.get(EEPROM_COLOR_RED, displayRed);
  EEPROM.get(EEPROM_COLOR_GREEN, displayGreen);
  EEPROM.get(EEPROM_COLOR_BLUE, displayBlue);
  EEPROM.get(EEPROM_DISPLAY_EN, displayOnOFF);
  EEPROM.get(EEPROM_SPD_LEVEL, speed_reduction);
  EEPROM.get(EEPROM_AUTO_LOCK, touchGesture);

  int addr = EEPROM_BLOCK_CMD;
  for (uint8_t ee = 0; ee < 5; ee++) {
    EEPROM.get(addr, BlockProgramCmd[ee]);
    addr += sizeof(uint16_t);  // Move forward 2 bytes
  }

  addr = EEPROM_BLOCK_DATA;
  for (uint8_t ee = 0; ee < 5; ee++) {
    for (uint8_t eee = 0; eee < 3; eee++) {
      EEPROM.get(addr, BlockProgramData[ee][eee]);
      addr += sizeof(uint16_t);  // Move forward 2 bytes
    }
  }

  if (first_connect != FIRST_CONNECT_KEY) {
    shape_thr = 30;
    shape_timer = 0;
    shapeOnOFF = 1;
    shape_loop = 0;
    speed_reduction = 3;
    shape_num = 4;
  }

  BLE_Init();

  _speed_grad = (100.0 - ((float)MOTOR_DEADZONE)) / 90.0;
  _speed_c = ((float)MOTOR_DEADZONE) - (10 * _speed_grad);

  myCodeCell.Motion_Read();
  myCodeCell.Light_Read();
  _proximity = ProximityHandler();
  HeadingHandler();
  StopHandler();
  delay(300);
  myCodeCell.Motion_Read();
  myCodeCell.Light_Read();
  _proximity = ProximityHandler();
  HeadingHandler();
  StopHandler();
  ClearReverse();

  for (uint8_t rv = 0; rv < 250; rv++) {
    _reverse_joystick_x[rv] = 100;
    _reverse_joystick_y[rv] = 100;
  }

  _motor1_dir = 1;
  _motor2_dir = 1;

  if (first_connect != FIRST_CONNECT_KEY) {
    myCodeCell.LED_SetBrightness(10);  //Turn on CodeCell LED
  } else {
    myCodeCell.LED_SetBrightness(0);  //Turn off CodeCell LED
  }
  myEyeLight.show_DisplayInit();
  _proximity_last = _proximity;
  _init_timer = 0;
  FrontAvoidOnOFF = 0;

  _prox_last = ReadProximityFront();
}

uint16_t ProtoBot::ProximityHandler() {
  _prox_array[_prox_index] = myCodeCell.Light_ProximityRead();
  _prox_index++;
  if (_prox_index >= AVRG_FILTER_SIZE) {
    _prox_index = 0;
  }
  uint32_t prox_avrg_total = 0;
  for (uint8_t ip = 0; ip < AVRG_FILTER_SIZE; ip++) {
    prox_avrg_total = prox_avrg_total + (uint32_t)_prox_array[ip];
  }
  prox_avrg_total = prox_avrg_total / AVRG_FILTER_SIZE;

  return ((uint16_t)prox_avrg_total);
}

void ProtoBot::BlockHandler() {
  //Run Active Blocks
  float cmd_angle = 90;
  uint16_t block_proxmity = 0, block_light_bright = 0, block_light_dark = 0;

  if (BlockProgramCmd[BlockIndex] != 0) {
    switch (BlockProgramCmd[BlockIndex]) {
      case COMMAND_ROTATE:
        BlockTimer++;
        if (BlockTimer < 120) {
          RotateHandler(BlockProgramData[BlockIndex][0], true);
        } else {
          BlockTimer = 0;
          BlockIndex++;
          StopHandler();
          delay(10);
        }
        break;

      case COMMAND_DRIVE:
        if (_orientation_flag) {
          if (BlockProgramData[BlockIndex][2] == 0) {
            cmd_angle = 270;
          } else {
            cmd_angle = 90;
          }
        } else {
          if (BlockProgramData[BlockIndex][2] == 0) {
            cmd_angle = 90;
          } else {
            cmd_angle = 270;
          }
        }
        BlockTimer++;
        if (BlockTimer < (BlockProgramData[BlockIndex][0] * 100)) {
          DriveHandler(BlockProgramData[BlockIndex][1], cmd_angle);
        } else {
          BlockTimer = 0;
          BlockIndex++;
          StopHandler();
          delay(10);
        }
        break;

      case COMMAND_WAIT:
        BlockTimer++;
        if (BlockTimer < (BlockProgramData[BlockIndex][0] * 100)) {
          StopHandler();
        } else {
          BlockTimer = 0;
          BlockIndex++;
        }
        break;

      case COMMAND_CIRCLE:
      case COMMAND_RECT:
      case COMMAND_TRIANGLE:
      case COMMAND_INFINITY:
        switch (BlockProgramData[BlockIndex][2]) {
          case COMMAND_CIRCLE:
            shape_num = 1;
            break;
          case COMMAND_RECT:
            shape_num = 3;
            break;
          case COMMAND_TRIANGLE:
            shape_num = 2;
            break;
          case COMMAND_INFINITY:
            shape_num = 4;
            break;
        }
        shapeOnOFF = 1;
        shape_loop = 0;
        shape_thr = BlockProgramData[BlockIndex][0];
        ShapeHandler(shape_num);

        if (shapeOnOFF == 0) {
          _block_shape_counter++;
          if (_block_shape_counter >= BlockProgramData[BlockIndex][1]) {
            shapeOnOFF = 0;
            _block_shape_counter = 0;
            BlockIndex++;
          } else {
          }
        }

        break;

      case COMMAND_LED:
        SetSpeed(0);
        myEyeLight.show_Color(BlockProgramData[BlockIndex][0], BlockProgramData[BlockIndex][1], BlockProgramData[BlockIndex][2]);
        BlockIndex++;
        break;

      case COMMAND_TAP:
        SetSpeed(0);
        if (ReadTap()) {
          BlockIndex++;
        }
        break;
      case COMMAND_PROXIMITY:
        SetSpeed(0);
        block_proxmity = ReadProximityFront();
        if (block_proxmity > (_block_proxmity_last + 3)) {
          BlockIndex++;
        }
        _block_proxmity_last = block_proxmity;
        break;
      case COMMAND_BRIGHT:
        SetSpeed(0);
        myEyeLight.show_Color(0, 0, 0);
        block_light_bright = ReadLightFront();
        if (block_light_bright > (_block_light_light_last + 100)) {
          BlockIndex++;
        }
        _block_light_light_last = block_light_bright;
        break;
      case COMMAND_DARK:
        SetSpeed(0);
        myEyeLight.show_Color(0, 0, 0);
        block_light_dark = ReadLightBase();
        if (block_light_dark < 4) {
          BlockIndex++;
        }
        break;
      default:
        break;
    }
    BLE_BlockUpdate(_mode, (BlockIndex + 1));
  } else {
    if (!BlockProgramLoop) {
      BlockProgramEnable = 0;
      ClearDisplay();
      StopHandler();
      BlockIndex = 0;
      BLE_BlockUpdate(_mode, BlockIndex);
    } else {
      BlockProgramEnable = 1;
      BlockIndex = 0;
    }
  }
}

bool ProtoBot::Run(uint8_t loop_timer) {
  if (myCodeCell.Run(95)) { //Run loop at 95Hz
    uint8_t joystick_x = 0;
    uint8_t joystick_y = 0;

    _bot_status = myCodeCell.PowerStateRead();

    HeadingHandler();

    if (!_balance_mode) {
      _proximity = ProximityHandler();
      if ((_roll > 350) || (_roll < 10)) {
        _orientation_flag = 0;
      } else {
        _orientation_flag = 1;
      }

      if (!BlockProgramEnable) {
        _send_timer++;
        if (_send_timer >= _send_thr) {  // Update metrics every 100ms
          _send_timer = 0;
          BLE_Update(_mode, ReadBattery(), ((uint16_t)_yaw));
        }
      }

      eepTimer++;
      if (eepTimer >= 500) {
        eepTimer = 0;
        if (eep_save) {
          eep_save = 0;
          EEPROM.put(EEPROM_FIRST_CONNECT, first_connect);
          EEPROM.put(EEPROM_COLOR_RED, displayRed);
          EEPROM.put(EEPROM_COLOR_GREEN, displayGreen);
          EEPROM.put(EEPROM_COLOR_BLUE, displayBlue);
          EEPROM.put(EEPROM_DISPLAY_EN, displayOnOFF);
          EEPROM.put(EEPROM_SPD_LEVEL, speed_reduction);
          EEPROM.put(EEPROM_AUTO_LOCK, touchGesture);

          myCodeCell.LED_SetBrightness(0);  //Turn off CodeCell LED

          if (BlockSave) {
            BlockSave = 0;
            int addr = EEPROM_BLOCK_CMD;
            for (uint8_t ee = 0; ee < 5; ee++) {
              EEPROM.put(addr, BlockProgramCmd[ee]);
              addr += sizeof(uint16_t);  // Move forward 2 bytes
            }
            addr = EEPROM_BLOCK_DATA;
            for (uint8_t ee = 0; ee < 5; ee++) {
              for (uint8_t eee = 0; eee < 3; eee++) {
                EEPROM.put(addr, BlockProgramData[ee][eee]);
                addr += sizeof(uint16_t);  // Move forward 2 bytes
              }
            }
          }
          EEPROM.commit();  //Commit EEPROM change
        } else {
          //skip
        }
      }
      if (first_connect == FIRST_CONNECT_KEY) {
        BLEOTA.process();
      }
    }

    if ((_bot_status == STATE_BAT_RUN) && (!_low_voltage_flag)) {
      if (_balance_mode) {
        if (((_pitch > 225) && (_pitch < 315)) || ((_pitch > 45) && (_pitch < 135))) {
          BalanceHandler(_orientation_balance);
        } else {
          _balance_mode = 0;
          myCodeCell.Drive1.Drive(_motor1_dir, 0);
          myCodeCell.Drive2.Drive(_motor2_dir, 0);
        }
      } else if (BlockProgramEnable) {
        if (_blockTouchGesture) {
          if (myEyeLight.show_blockLoading(110, 2, 250, 250, 250)) {
            _blockTouchGesture = 0;
          }
        } else {
          _mode = AUTOMATE_MODE;
          BlockHandler();
          if ((_proximity > (_proximity_last + 2)) && (touchGesture == 2) && (!_orientation_flag)) {
            BlockProgramEnable = 0;
          }
        }
      } else if ((isAppConnectionReady) || (_manual_mode)) {
        if (BlockSave == 1) {
          //wait
        } else {
          if (reverse_button == 4) {
            _mode = REV_MODE;
            PlayReverse();  //If Rev pressed overwrite joystick
          }

          if (!_manual_mode) {
            if (!_orientation_flag) {
              joystick_x = 200 - rx_joystick_x;
              joystick_y = 200 - rx_joystick_y;
            } else {
              avoidOnOFF = 0;
              joystick_x = rx_joystick_x;
              joystick_y = rx_joystick_y;
            }

            _drive_speed = JoyStick_Get_Speed(joystick_x, joystick_y);
            _drive_angle = JoyStick_Get_Angle(joystick_x, joystick_y);
          } else {
            //skip
          }

          if ((shapeOnOFF) && (reverse_button != 4) && (_init_timer >= 30)) {
            _mode = SHAPE_MODE;
            ShapeHandler(shape_num);
            sleepTimer = 0;
            _send_thr = 10;  // Update metrics every 100ms
            _drive_counter = 0;
          } else if ((_drive_speed == 0) && (_motor_off == 1) && (reverse_button != 4)) {
            //Robot is not driving
            _drive_counter = 0;

            if (avoidOnOFF) {
              sleepTimer = 0;
              _send_thr = 10;  // Update metrics every 100ms
              AvoidUpHandler();
            } else if (faceNorthOnOFF) {
              _proximity_flag = 0;
              sleepTimer = 0;
              _send_thr = 10;  // Update metrics every 100ms
              if (_control_rotate > MOTOR_DEADZONE) {
                myEyeLight.show_Align(40);
              } else {
                myEyeLight.show_Default(90, displayRed, displayGreen, displayBlue);
              }
              RotateHandler(180, false);
            } else if ((((_roll < 190) && ((_roll > 170))) && ((_pitch < 190) && ((_pitch > 170)))) || (((_roll < 10) || ((_roll > 350))) && ((_pitch < 10) || ((_pitch > 350))))) {
              if ((_proximity < (_proximity_stable + 20)) && ((_proximity > (_proximity_stable - 20))) || (_proximity_stable == 0)) {
                //Robot is stable on table
                if (ReadTap()) {
                  sleepTimer = 0;
                } else {
                  if (FrontAvoidOnOFF) {
                    EyeTracking();
                  } else {
                    _EyeTracking_Flag = 0;
                    _proximity_flag = 0;
                    _balance_mode = 0;
                    StopHandler();
                  }
                  if (sleepTimer < 1000) {  //wait 10 sec
                    if (displayOnOFF != 0) {
                      myEyeLight.show_Default(90, displayRed, displayGreen, displayBlue);
                    } else {
                      ClearDisplay();
                    }
                    _send_thr = 10;  // Update metrics every 100ms
                    TouchHandler();
                  } else {
                    _send_thr = 1000;  // Update metrics every 10000ms
                    ClearDisplay();
                  }
                }
              } else {
                //Wait for Proximity to be stable
                StopHandler();
                myEyeLight.show_Default(30, displayRed, displayGreen, displayBlue);
              }
              _proximity_stable = _proximity;
            } else if ((((_pitch > 240) && (_pitch < 300)) || ((_pitch > 60) && (_pitch < 120))) && ((_roll > 350) || (_roll < 10) || ((_roll < 190) && (_roll > 170)))) {
              myEyeLight.show_Default(30, 0xff, 0x66, 0x0);
              _balance_timer++;
              if (_balance_timer > 40U) {
                _balance_timer = 0;
                _balance_mode = 1;
                _orientation_balance = false;
                if ((_pitch > 225) && (_pitch < 315)) {
                  _orientation_balance = true;
                }
              }
            } else {
              //User picked up robot
              sleepTimer = 0;
              myEyeLight.show_Default(30, displayRed, displayGreen, displayBlue);
              StopHandler();
            }
          } else {                                                                                             //Normal Mode
            if ((EyeSensDetect()) && (_drive_angle != 180) && (_drive_angle != 360) && (_drive_angle != 0)) {  //Proxmity detected
#if defined(ARDUINO_ESP32C6_DEV)
              myCodeCell.Drive1.Drive(_motor1_dir, 0);
              myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
              Motor1.Drive(_motor1_dir, 0);
              Motor2.Drive(_motor2_dir, 0);
#endif
              delay(100);
              StopHandler();
              _motor1_dir = !_motor2_dir;
              _motor2_dir = !_motor2_dir;
#if defined(ARDUINO_ESP32C6_DEV)
              myCodeCell.Drive1.Drive(_motor1_dir, 70);
              myCodeCell.Drive2.Drive(_motor2_dir, 70);
#else
              Motor1.Drive(_motor1_dir, 70);
              Motor2.Drive(_motor2_dir, 70);
#endif
              _control_rotate = 0xFF;
              delay(350);
              while (_control_rotate > 10U) {
                delay(10);
                myCodeCell.Motion_Read();
                HeadingHandler();
                RotateHandler(270, true);
              }
              StopHandler();
            } else {
              if (reverse_button != 4) {
                SaveReverse(rx_joystick_x, rx_joystick_y);
                _mode = JOYSTICK_MODE;
              }
              DriveHandler(_drive_speed, _drive_angle);
              sleepTimer = 0;
              _send_thr = 10;  // Update metrics every 100ms
            }
          }
        }
      } else {
        myEyeLight.show_Loading(110, 2, 0, 0, 250);
        _advertise_timer++;
        if (_advertise_timer >= 300) {  //if 3sec passed
          _advertise_timer = 0;
          BLE_Advertise();
        }
        if (first_connect == FIRST_CONNECT_KEY) {
          TouchHandler();
        } else {
          _driver_timer++;
          if (_driver_timer > 60) {
            _driver_timer = 0;
            _motor1_dir = !_motor1_dir;
            _motor2_dir = !_motor2_dir;
          }

          _motor1_speed = 70;
          _motor2_speed = 70;

#if defined(ARDUINO_ESP32C6_DEV)
          myCodeCell.Drive1.Drive(_motor1_dir, _motor1_speed);
          myCodeCell.Drive2.Drive(_motor2_dir, _motor2_speed);
#else
          Motor1.Drive(_motor1_dir, _motor1_speed);
          Motor2.Drive(_motor2_dir, _motor2_speed);
#endif

          sleepTimer = 0;
          _send_thr = 100;  // Update metrics every 1000ms
        }
        sleepTimer = 0;
        _send_thr = 100;  // Update metrics every 1000ms
      }
    } else if ((_bot_status == STATE_USB) && (first_connect != FIRST_CONNECT_KEY)) {
      //First time robot sequence
      _low_voltage_flag = 0;
      _driver_timer++;
      if (_driver_timer > 60) {
        _driver_timer = 0;
        _motor1_dir = !_motor1_dir;
        _motor2_dir = !_motor2_dir;
      }

      _motor1_speed = 60;
      _motor2_speed = 60;

#if defined(ARDUINO_ESP32C6_DEV)
      myCodeCell.Drive1.Drive(_motor1_dir, _motor1_speed);
      myCodeCell.Drive2.Drive(_motor2_dir, _motor2_speed);
#else
      Motor1.Drive(_motor1_dir, _motor1_speed);
      Motor2.Drive(_motor2_dir, _motor2_speed);
#endif

      sleepTimer = 0;
      _send_thr = 100;  // Update metrics every 1000ms

    } else {
      _mode = 0xFF;  //Clear Mode
      if (_bot_status == STATE_BAT_CHRG) {
        myCodeCell.LED(0, 0, 10);
        _low_voltage_flag = 0;
      } else if (_bot_status == STATE_BAT_LOW) {
        _low_voltage_flag = 1;
        myCodeCell.LED(10, 0, 0);
        BLE_Update(_mode, 0, ((uint16_t)_yaw));
      } else {
        //skip
      }
      HibernateHandler();
    }
    _proximity_last = _proximity;

    _run_timer++;
    if (loop_timer > 10) {
      loop_timer = 10;
    }

    if ((_run_timer >= (100 / loop_timer)) && (!BlockProgramEnable) && (!shapeOnOFF)) {
      _run_timer = 0;
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
  _manual_mode = 0;  //reset manual mode
}

void ProtoBot::BalanceHandler(bool side) {
  _Kd = 10;//12;
  _Kp = 6;

  float gyro_x = 0.0;
  float gyro_y = 0.0;
  float gyro_z = 0.0;

  myCodeCell.Motion_GyroRead(gyro_x, gyro_y, gyro_z);
  
  if (side) {
    _control_rotate = (_Kp * (270 - _pitch)) - (_Kd * gyro_y);  //Use direct gyro rate of fall for derivative
  } else {
    _control_rotate = (_Kp * (90 - _pitch)) - (_Kd * gyro_y);  //Use direct gyro rate of fall for derivative
  }
  if (_control_rotate >= 0) {
    _motor1_dir = 1;
    _motor2_dir = 1;
  } else {
    _motor1_dir = 0;
    _motor2_dir = 0;
    _control_rotate = fabsf(_control_rotate);
  }

  if (_control_rotate > 100.0) {
    _control_rotate = 100.0;
  }
  long motor_duty = (long)_control_rotate;
  if (motor_duty >= 5) {
    motor_duty = map(motor_duty, 5, 100, (MOTOR_DEADZONE + 5), MOTOR_MAXSPEED);
  } else {
    motor_duty = 0;
  }

  myCodeCell.Drive1.Drive(_motor1_dir, (uint8_t)motor_duty);
  myCodeCell.Drive2.Drive(_motor2_dir, (uint8_t)motor_duty);

  myEyeLight.show_Default(((motor_duty / 3) + 10), 0xff, 0x66, 0x0);
}

void ProtoBot::TouchHandler(void) {
  if (_init_timer < 30) {
    _init_timer++;
    StopHandler();
  } else {
    if ((_proximity > (_proximity_last + 2)) && (touchGesture == 2) && (!_orientation_flag) && (!BlockProgramEnable)) {
      ClearDisplay();  // Clear Lock annimation
      sleepTimer = 0;
      BlockProgramEnable = 1;
      _blockTouchGesture = 1;
      BlockIndex = 0;
      BlockTimer = 0;
      int addr = EEPROM_BLOCK_CMD;
      for (uint8_t ee = 0; ee < 5; ee++) {
        EEPROM.get(addr, BlockProgramCmd[ee]);
        addr += sizeof(uint16_t);  // Move forward 2 bytes
      }

      addr = EEPROM_BLOCK_DATA;
      for (uint8_t ee = 0; ee < 5; ee++) {
        for (uint8_t eee = 0; eee < 3; eee++) {
          EEPROM.get(addr, BlockProgramData[ee][eee]);
          addr += sizeof(uint16_t);  // Move forward 2 bytes
        }
      }
    } else {
      if (!BlockProgramEnable) {
        sleepTimer++;
        StopHandler();
      }
    }
  }
}

void ProtoBot::HibernateHandler(void) {
  _motor1_dir = 0;
  _motor2_dir = 0;
  _motor1_speed = 0;
  _motor2_speed = 0;
#if defined(ARDUINO_ESP32C6_DEV)
  myCodeCell.Drive1.Drive(_motor1_dir, _motor1_speed);
  myCodeCell.Drive2.Drive(_motor2_dir, _motor2_speed);
#else
  Motor1.Drive(_motor1_dir, _motor1_speed);
  Motor2.Drive(_motor2_dir, _motor2_speed);
#endif

  StopHandler();

  myEyeLight.show_PowerStatus(_bot_status);
}

void ProtoBot::ShapeHandler(uint8_t shape_type) {
  uint16_t time_shape_thr = 0;
  uint16_t new_shape_thr = 0;
  _motor1_dir = 1;

  if ((first_connect == FIRST_CONNECT_KEY) && (!BlockProgramEnable)) {
    myEyeLight.show_Loading(110, 2, displayRed, displayGreen, displayBlue);
  }
  shape_timer++;
  switch (shape_type) {
    case 1:
      //Make circle shape
      new_shape_thr = map(shape_thr, 0, 100, MOTOR_DEADZONE, MOTOR_MAXSPEED);
      _motor1_dir = 0;

#if defined(ARDUINO_ESP32C6_DEV)
      myCodeCell.Drive1.Drive(_motor1_dir, new_shape_thr);
      myCodeCell.Drive2.Drive(_motor1_dir, 100);
#else
      Motor1.Drive(_motor1_dir, new_shape_thr);
      Motor2.Drive(_motor1_dir, 100);
#endif
      circle_angle = fmod(_yaw - _yaw_zero + 360, 360);
      if ((circle_angle <= 100) && (circle_angle >= 80) && (shape_timer >= 20)) {
        shape_timer = 0;
        if (!shape_loop) {
          shapeOnOFF = 0;
        }
      }
      break;
    case 2:
      //Make triangle shape
      time_shape_thr = shape_thr + 30;
      switch (_shape_index) {
        case 0:
          DriveHandler(100, 90);
          if (shape_timer > time_shape_thr) {
            shape_timer = 0;
            _shape_index++;
#if defined(ARDUINO_ESP32C6_DEV)
            myCodeCell.Drive1.Drive(_motor1_dir, 0);
            myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
            Motor1.Drive(_motor1_dir, 0);
            Motor2.Drive(_motor2_dir, 0);
#endif
            _motor_off = 1;
            _integral = 0;
            _error_last = 0;
            _previousTime = millis();
          }
          break;
        case 1:
          RotateHandler(330, true);
          if ((_control_rotate < 10) || (shape_timer > ROT_TIME)) {
            _angle_done++;
            if (_angle_done > 3) {
              _angle_done = 0;
              _shape_index++;
              shape_timer = 0;
#if defined(ARDUINO_ESP32C6_DEV)
              myCodeCell.Drive1.Drive(_motor1_dir, 0);
              myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
              Motor1.Drive(_motor1_dir, 0);
              Motor2.Drive(_motor2_dir, 0);
#endif
              _motor_off = 1;
              _integral = 0;
              _error_last = 0;
              _previousTime = millis();
            }
          } else {
            _angle_done = 0;
          }
          break;
        case 2:
          DriveHandler(100, 330);
          if (shape_timer > time_shape_thr) {
            shape_timer = 0;
            _shape_index++;
#if defined(ARDUINO_ESP32C6_DEV)
            myCodeCell.Drive1.Drive(_motor1_dir, 0);
            myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
            Motor1.Drive(_motor1_dir, 0);
            Motor2.Drive(_motor2_dir, 0);
#endif
            _motor_off = 1;
            _integral = 0;
            _error_last = 0;
            _previousTime = millis();
          }
          break;
        case 3:
          RotateHandler(210, true);
          if ((_control_rotate < 10) || (shape_timer > ROT_TIME)) {
            _angle_done++;
            if (_angle_done > 3) {
              _angle_done = 0;
              _shape_index++;
              shape_timer = 0;
#if defined(ARDUINO_ESP32C6_DEV)
              myCodeCell.Drive1.Drive(_motor1_dir, 0);
              myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
              Motor1.Drive(_motor1_dir, 0);
              Motor2.Drive(_motor2_dir, 0);
#endif
              _motor_off = 1;
              _integral = 0;
              _error_last = 0;
              _previousTime = millis();
            }
          } else {
            _angle_done = 0;
          }
          break;
        case 4:
          DriveHandler(100, 210);
          if (shape_timer > time_shape_thr) {
            shape_timer = 0;
            _shape_index++;
#if defined(ARDUINO_ESP32C6_DEV)
            myCodeCell.Drive1.Drive(_motor1_dir, 0);
            myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
            Motor1.Drive(_motor1_dir, 0);
            Motor2.Drive(_motor2_dir, 0);
#endif
            _motor_off = 1;
            _integral = 0;
            _error_last = 0;
            _previousTime = millis();
          }
          break;
        case 5:
          RotateHandler(90, true);
          if ((_control_rotate < 10) || (shape_timer > ROT_TIME)) {
            _angle_done++;
            if (_angle_done > 3) {
              _angle_done = 0;
              _shape_index = 0;
              shape_timer = 0;
#if defined(ARDUINO_ESP32C6_DEV)
              myCodeCell.Drive1.Drive(_motor1_dir, 0);
              myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
              Motor1.Drive(_motor1_dir, 0);
              Motor2.Drive(_motor2_dir, 0);
#endif
              _motor_off = 1;
              _integral = 0;
              _error_last = 0;
              _previousTime = millis();
              if (!shape_loop) {
                shapeOnOFF = 0;
              }
            }
          } else {
            _angle_done = 0;
          }
          break;
      }
      break;

    case 3:
      //Make square shape
      time_shape_thr = shape_thr + 30;
      switch (_shape_index) {
        case 0:
          DriveHandler(100, 90);
          if (shape_timer > time_shape_thr) {
            shape_timer = 0;
            _shape_index++;
#if defined(ARDUINO_ESP32C6_DEV)
            myCodeCell.Drive1.Drive(_motor1_dir, 0);
            myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
            Motor1.Drive(_motor1_dir, 0);
            Motor2.Drive(_motor2_dir, 0);
#endif
            _motor_off = 1;
            _integral = 0;
            _error_last = 0;
            _previousTime = millis();
          }
          break;
        case 1:
          RotateHandler(0, true);
          if ((_control_rotate < 10) || (shape_timer > ROT_TIME)) {
            _angle_done++;
            if (_angle_done > 3) {
              _angle_done = 0;
              _shape_index++;
              shape_timer = 0;
#if defined(ARDUINO_ESP32C6_DEV)
              myCodeCell.Drive1.Drive(_motor1_dir, 0);
              myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
              Motor1.Drive(_motor1_dir, 0);
              Motor2.Drive(_motor2_dir, 0);
#endif
              _motor_off = 1;
              _integral = 0;
              _error_last = 0;
              _previousTime = millis();
            }
          } else {
            _angle_done = 0;
          }
          break;
        case 2:
          DriveHandler(100, 0);
          if (shape_timer > time_shape_thr) {
            shape_timer = 0;
            _shape_index++;
#if defined(ARDUINO_ESP32C6_DEV)
            myCodeCell.Drive1.Drive(_motor1_dir, 0);
            myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
            Motor1.Drive(_motor1_dir, 0);
            Motor2.Drive(_motor2_dir, 0);
#endif
            _motor_off = 1;
            _integral = 0;
            _error_last = 0;
            _previousTime = millis();
          }
          break;
        case 3:
          RotateHandler(270, true);
          if ((_control_rotate < 10) || (shape_timer > ROT_TIME)) {
            _angle_done++;
            if (_angle_done > 3) {
              _angle_done = 0;
              _shape_index++;
#if defined(ARDUINO_ESP32C6_DEV)
              myCodeCell.Drive1.Drive(_motor1_dir, 0);
              myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
              Motor1.Drive(_motor1_dir, 0);
              Motor2.Drive(_motor2_dir, 0);
#endif
              _motor_off = 1;
              _integral = 0;
              _error_last = 0;
              _previousTime = millis();
              shape_timer = 0;
            }
          } else {
            _angle_done = 0;
          }
          break;
        case 4:
          DriveHandler(100, 270);
          if (shape_timer > time_shape_thr) {
            shape_timer = 0;
            _shape_index++;
#if defined(ARDUINO_ESP32C6_DEV)
            myCodeCell.Drive1.Drive(_motor1_dir, 0);
            myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
            Motor1.Drive(_motor1_dir, 0);
            Motor2.Drive(_motor2_dir, 0);
#endif
            _motor_off = 1;
            _integral = 0;
            _error_last = 0;
            _previousTime = millis();
          }
          break;
        case 5:
          RotateHandler(180, true);
          if ((_control_rotate < 10) || (shape_timer > ROT_TIME)) {
            _angle_done++;
            if (_angle_done > 3) {
              _angle_done = 0;
              _shape_index++;
#if defined(ARDUINO_ESP32C6_DEV)
              myCodeCell.Drive1.Drive(_motor1_dir, 0);
              myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
              Motor1.Drive(_motor1_dir, 0);
              Motor2.Drive(_motor2_dir, 0);
#endif
              _motor_off = 1;
              _integral = 0;
              _error_last = 0;
              _previousTime = millis();
              shape_timer = 0;
            }
          } else {
            _angle_done = 0;
          }
          break;
        case 6:
          DriveHandler(100, 180);
          if (shape_timer > time_shape_thr) {
            shape_timer = 0;
            _shape_index++;
#if defined(ARDUINO_ESP32C6_DEV)
            myCodeCell.Drive1.Drive(_motor1_dir, 0);
            myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
            Motor1.Drive(_motor1_dir, 0);
            Motor2.Drive(_motor2_dir, 0);
#endif
            _motor_off = 1;
            _integral = 0;
            _error_last = 0;
            _previousTime = millis();
          }
          break;
        case 7:
          RotateHandler(90, true);
          if ((_control_rotate < 10) || (shape_timer > ROT_TIME)) {
            _angle_done++;
            if (_angle_done > 3) {
              _angle_done = 0;
              _shape_index = 0;
#if defined(ARDUINO_ESP32C6_DEV)
              myCodeCell.Drive1.Drive(_motor1_dir, 0);
              myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
              Motor1.Drive(_motor1_dir, 0);
              Motor2.Drive(_motor2_dir, 0);
#endif
              _motor_off = 1;
              _integral = 0;
              _error_last = 0;
              _previousTime = millis();
              shape_timer = 0;
              if (!shape_loop) {
                shapeOnOFF = 0;
              }
            }
          } else {
            _angle_done = 0;
          }
          break;
      }
      break;
    case 4:
      //Make infinity shape
      _motor1_dir = 1;
      if (((abs(_error)) < 15) && (shape_timer >= 5)) {
        shape_timer = 0;
        _motor1_dir = 1;
        _motor2_dir = 1;
        if (_infinity_phase == 0) {
          circle_angle = circle_angle + ((104 - shape_thr));
          if (circle_angle >= 450) {  //360+90 - first angle is 90deg
            circle_angle = 450;
            _infinity_phase = 1;
          }
        } else {
          circle_angle = circle_angle - ((104 - shape_thr));
          if (circle_angle <= 90) {  //360+90 - first angle is 90deg
            circle_angle = 90;
            _infinity_phase = 0;
            if (!shape_loop) {
              shapeOnOFF = 0;
            }
          }
        }
      }
      DriveHandler(100, circle_angle);
      break;
    case 5:
      //Make rotation
      if (shape_timer < ROT_TIME) {
        RotateHandler(_rotate_shape_angle, true);
      } else {
        shapeOnOFF = 0;
        shape_timer = 0;
        StopHandler();
      }
      break;
  }
}

void ProtoBot::AvoidUpHandler() {
  if ((_proximity > (_proximity_last + 1)) || ((_proximity > _prox_thr) && (_proximity_flag))) {
    if (!_proximity_flag) {
      _prox_thr = _proximity;
    }
    _motor1_speed = 100;
    _motor2_speed = 100;
    _proximity_flag = 1;
    _motor1_dir = 1;
    _motor2_dir = 1;
#if defined(ARDUINO_ESP32C6_DEV)
    myCodeCell.Drive1.Drive(_motor1_dir, _motor1_speed);
    myCodeCell.Drive2.Drive(_motor2_dir, _motor2_speed);
#else
    Motor1.Drive(_motor1_dir, _motor1_speed);
    Motor2.Drive(_motor2_dir, _motor2_speed);
#endif
  } else {
    if (_proximity_flag) {
      if (_squish_timer <= 1) {
        _squish_timer++;  // Keep motors on for a few seconds
      } else {
        _proximity_flag = 0;
        _prox_thr = 0;
      }
    } else {
      if ((_motor1_speed != 0) || (_motor2_speed != 0)) {
        _squish_timer = 0;
        _motor1_speed = 0;
        _motor2_speed = 0;
#if defined(ARDUINO_ESP32C6_DEV)
        myCodeCell.Drive1.Drive(_motor1_dir, _motor1_speed);
        myCodeCell.Drive2.Drive(_motor2_dir, _motor2_speed);
#else
        Motor1.Drive(_motor1_dir, _motor1_speed);
        Motor2.Drive(_motor2_dir, _motor2_speed);
#endif
      }
    }
  }
  if (_proximity_flag) {
    myEyeLight.show_AvoidUp(70);
  } else {
    myEyeLight.show_Default(90, displayRed, displayGreen, displayBlue);
  }
}

float ProtoBot::JoyStick_Get_Angle(uint8_t joystick_x, uint8_t joystick_y) {
  int dx = joystick_x - XY_MIDPOINT;             // X difference from center
  int dy = joystick_y - XY_MIDPOINT;             // Y difference fro
  float angle = atan2(dy, dx) * (180.0 / M_PI);  // Convert radians to degrees
  angle = angle + 180;
  return angle;
}

uint8_t ProtoBot::JoyStick_Get_Speed(uint8_t joystick_x, uint8_t joystick_y) {
  uint8_t uspeed = 0;
  int dx = joystick_x - XY_MIDPOINT;  // X difference from center
  int dy = joystick_y - XY_MIDPOINT;  // Y difference fro
  float speed = sqrt(((dx * dx) + (dy * dy)));
  uspeed = (uint8_t)speed;
  uspeed = constrain(uspeed, 0, 100);
  return uspeed;
}

void ProtoBot::HeadingHandler() {
  myCodeCell.Motion_RotationRead(_roll, _pitch, _yaw);
  _yaw = fmod(_yaw + 360.0f, 360.0f);      //Wraps Angle: (_yaw−360)*(_yaw/360)
  _roll = fmod(_roll + 360.0f, 360.0f);    //Wraps Angle
  _pitch = fmod(_pitch + 360.0f, 360.0f);  //Wraps Angle
}

void ProtoBot::StopHandler() {
  if (!_EyeTracking_Flag) {
#if defined(ARDUINO_ESP32C6_DEV)
    myCodeCell.Drive1.Drive(_motor1_dir, 0);
    myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
    Motor1.Drive(_motor1_dir, 0);
    Motor2.Drive(_motor2_dir, 0);
#endif

    _yaw_zero = fmod(_yaw - 90 + 360, 360);  //Align Robot to 90deg centre
    _roll_zero = _roll;                      //Align
    _pitch_zero = _pitch;                    //Align

    _m1_speed = 0;
    _m2_speed = 0;
    //_first_spin = 0;
    _motor_off = 1;
    _integral = 0;
    _error_last = 0;
    _error = 0;
    _derivative = 0;
    _previousTime = millis();
    _front_detect_timer = 0;
    _FrontAvoidOnOFF_Flag = 0;
    _balance_mode = 0;
  }
}

float ProtoBot::PID_Controller(float target_output, float input_signal) {
  float control_signal = 0.0;
  float deltaTime = 0.0;

  unsigned long currentTime = millis();
  deltaTime = (currentTime - _previousTime) / 1000.0;  // Convert to seconds
  if (deltaTime < 0.001) {
    deltaTime = 0.001;  // Prevent division by zero
  }

  // Normalize angles to 0 - 360 range
  target_output = fmod(target_output + 360, 360);  //Wraps Angle: (target_output−360)*(target_output/360)
  input_signal = fmod(input_signal + 360, 360);    //Wraps Angle: (input_signal−360)*(input_signal/360)


  // Calculate _error and wrap correctly
  _error = target_output - input_signal;

  // Check _error to determine which direction to turn
  if (_error > 180) {
    _error -= 360;
  }
  if (_error < -180) {
    _error += 360;
  }

  // Reset _integral if _error direction changes to prevent windup
  if (signbit(_error) != signbit(_error_last)) {
    _integral = 0;
  }

  // PID calculations
  _integral += (_error * deltaTime);
  _integral = constrain(_integral, WINDUP_MIN, WINDUP_MAX);  // Avoid _integral windup

  _derivative = (_error - _error_last) / deltaTime;
  control_signal = (_Kp * _error) + (_Ki * _integral) + (_Kd * _derivative);

  // Update for next loop
  _error_last = _error;
  _previousTime = currentTime;

  return control_signal;
}

void ProtoBot::RotateHandler(float rotate_angle, bool tare_on) {
  float rYaw = 0.0;
  if (_bot_status == STATE_BAT_RUN) {
    _Kp = Kp_ROT;
    _Ki = Ki_ROT;
    _Kd = Kd_ROT;

    if (tare_on) {
      rYaw = fmod(_yaw - _yaw_zero + 360, 360);  // Ensures _yaw is within 0-360
    } else {
      rYaw = _yaw;
    }
    if (shapeOnOFF) {
      if ((!_manual_mode) || ((_manual_mode) && (shape_num = 2))) {
        if (rotate_angle < 180) {
          rotate_angle = 180 - rotate_angle;
        } else {
          rotate_angle = (360 - rotate_angle) + 180;
        }

        if (rotate_angle < 180) {
          //skip
        } else {
          rYaw = rYaw + 180;
        }
      }
    }
    _control_rotate = PID_Controller(rotate_angle, rYaw);
    if (_control_rotate >= 0) {
      _motor1_dir = 0;
      _motor2_dir = 1;
    } else {
      _motor1_dir = 1;
      _motor2_dir = 0;
      _control_rotate = abs(_control_rotate);
    }
    _control_rotate = constrain(_control_rotate, 0, 90);

#if defined(ARDUINO_ESP32C6_DEV)
    myCodeCell.Drive1.Drive(_motor1_dir, (uint8_t)_control_rotate);
    myCodeCell.Drive2.Drive(_motor2_dir, (uint8_t)_control_rotate);
#else
    Motor1.Drive(_motor1_dir, (uint8_t)_control_rotate);
    Motor2.Drive(_motor2_dir, (uint8_t)_control_rotate);
#endif
  }
}

void ProtoBot::DriveHandler(uint8_t xy_speed, float xy_angle) {
  int speed_control = 0;
  _m1_speed = xy_speed;
  _m2_speed = xy_speed;

  _Kp = Kp_DRV;
  _Ki = Ki_DRV;
  _Kd = Kd_DRV;

  if (xy_angle < 180) {
    xy_angle = 180 - xy_angle;
  } else {
    xy_angle = (360 - xy_angle) + 180;
  }
  if (xy_speed == 0) {
    StopHandler();
  } else if ((xy_angle == 180) && (!shapeOnOFF)) {
    _spin_flag = 1;
    if ((_motor1_dir != 0) || (_motor2_dir != 1)) {
      _motor1_dir = 0;
      _motor2_dir = 1;
#if defined(ARDUINO_ESP32C6_DEV)
      myCodeCell.Drive1.Drive(_motor1_dir, 0);
      myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
      Motor1.Drive(_motor1_dir, 0);
      Motor2.Drive(_motor2_dir, 0);
#endif
      delay(MOTOR_SURGEDELAY);  // Surge Current delay
    }
    if (xy_speed < (MOTOR_SPINSPEED - 20U)) {
      xy_speed = xy_speed / 3;
    } else if (xy_speed < (MOTOR_SPINSPEED - 10U)) {
      xy_speed = xy_speed / 2;
    } else {
      //Skip
    }

    if (xy_speed < MOTOR_DEADZONE) {
      xy_speed = MOTOR_DEADZONE;
    } else if (xy_speed > MOTOR_SPINSPEED) {
      xy_speed = MOTOR_SPINSPEED;
    } else {
      //skip
    }

    if (speed_reduction == 0) {
      speed_reduction = 1;
    } else if (speed_reduction >= 3) {
      speed_reduction = 3;
    } else {
      //skip
    }
#if defined(ARDUINO_ESP32C6_DEV)
    myCodeCell.Drive1.Drive(_motor1_dir, (xy_speed / speed_level[speed_reduction]));
    if (!_first_spin) {
      _first_spin = 1;
      delay(100);
    }
    myCodeCell.Drive2.Drive(_motor2_dir, (xy_speed / speed_level[speed_reduction]));
#else
    Motor1.Drive(_motor1_dir, (xy_speed / speed_level[speed_reduction]));
    if (!_first_spin) {
      _first_spin = 1;
      delay(100);
    }
    Motor2.Drive(_motor2_dir, (xy_speed / speed_level[speed_reduction]));
#endif
    _motor_off = 0;
    if (!BlockProgramEnable) {
      myEyeLight.show_Turn(1, 120, 3, displayRed, displayGreen, displayBlue);
    }
  } else if ((xy_angle == 360) && (!shapeOnOFF)) {
    _spin_flag = 1;
    if ((_motor1_dir != 1) || (_motor2_dir != 0)) {
      _motor1_dir = 1;
      _motor2_dir = 0;
#if defined(ARDUINO_ESP32C6_DEV)
      myCodeCell.Drive1.Drive(_motor1_dir, 0);
      myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
      Motor1.Drive(_motor1_dir, 0);
      Motor2.Drive(_motor2_dir, 0);
#endif
      delay(MOTOR_SURGEDELAY);  // Surge Current delay
    }

    if (xy_speed < (MOTOR_SPINSPEED - 20U)) {
      xy_speed = xy_speed / 3;
    } else if (xy_speed < (MOTOR_SPINSPEED - 10U)) {
      xy_speed = xy_speed / 2;
    } else {
      //Skip
    }

    if (xy_speed < MOTOR_DEADZONE) {
      xy_speed = MOTOR_DEADZONE;
    } else if (xy_speed > MOTOR_SPINSPEED) {
      xy_speed = MOTOR_SPINSPEED;
    } else {
      //skip
    }
    if (xy_speed < MOTOR_DEADZONE) {
      xy_speed = MOTOR_DEADZONE;
    } else if (xy_speed > MOTOR_SPINSPEED) {
      xy_speed = MOTOR_SPINSPEED;
    } else {
      //skip
    }
    if (speed_reduction == 0) {
      speed_reduction = 1;
    } else if (speed_reduction >= 3) {
      speed_reduction = 3;
    } else {
      //skip
    }
#if defined(ARDUINO_ESP32C6_DEV)
    myCodeCell.Drive1.Drive(_motor1_dir, (xy_speed / speed_level[speed_reduction]));
    if (!_first_spin) {
      _first_spin = 1;
      delay(100);
    }
    myCodeCell.Drive2.Drive(_motor2_dir, (xy_speed / speed_level[speed_reduction]));
#else
    Motor1.Drive(_motor1_dir, (xy_speed / speed_level[speed_reduction]));
    if (!_first_spin) {
      _first_spin = 1;
      delay(100);
    }
    Motor2.Drive(_motor2_dir, (xy_speed / speed_level[speed_reduction]));
#endif
    _motor_off = 0;
    if (!BlockProgramEnable) {
      myEyeLight.show_Turn(-1, 120, 3, displayRed, displayGreen, displayBlue);
    }
  } else {
    if (_spin_flag) {
      _spin_flag = 0;
      StopHandler();
    } else {
      //Robot is moving
      //_first_spin = 0;
      xy_speed = (uint8_t)((_speed_grad * xy_speed) + _speed_c);  // Scale speed to remove motor's deadzone
      if (reverse_button == 4) {
        _yaw = fmod(_yaw - _yaw_zero_new + 360, 360);  // Ensures _yaw is within 0-360
      } else {
        _yaw = fmod(_yaw - _yaw_zero + 360, 360);  // Ensures _yaw is within 0-360
      }

      if ((shapeOnOFF) && ((shape_num == 1) || (shape_num == 4))) {
        //Skip
      } else {
        if (xy_angle < 180) {
          //Forward
          _motor1_dir = 0;
          _motor2_dir = 0;
          if ((!shapeOnOFF) && (!BlockProgramEnable)) {
            if ((abs(_error) < 20) || (_turn_timer < 25)) {
              if (_turn_timer < 25) {
                _turn_timer++;
              } else {
                _turn_timer = 0;
              }
            } else if (signbit(_error)) {
              _turn_timer = 25;
              myEyeLight.reset_Eye();
            } else {
              _turn_timer = 25;
              myEyeLight.reset_Eye();
            }
            myEyeLight.show_Default((145 - xy_speed), displayRed, displayGreen, displayBlue);
          }
        } else {
          //Reverse
          if ((!shapeOnOFF) && (!BlockProgramEnable)) {
            if ((abs(_error) < 20) || (_turn_timer < 25)) {
              if (_turn_timer < 25) {
                _turn_timer++;
              } else {
                _turn_timer = 0;
              }
            } else if (signbit(_error)) {
              _turn_timer = 25;
              myEyeLight.reset_Eye();
            } else {
              _turn_timer = 25;
              myEyeLight.reset_Eye();
            }
            myEyeLight.show_Default((145 - xy_speed), displayRed, displayGreen, displayBlue);
          }
          _motor1_dir = 1;
          _motor2_dir = 1;
          _yaw = _yaw + 180;
        }
      }

      if ((_motor1_dir_last != _motor1_dir) || (_motor2_dir_last != _motor2_dir)) {
#if defined(ARDUINO_ESP32C6_DEV)
        myCodeCell.Drive1.Drive(_motor1_dir, 0);
        myCodeCell.Drive2.Drive(_motor2_dir, 0);
#else
        Motor1.Drive(_motor1_dir, 0);
        Motor2.Drive(_motor2_dir, 0);
#endif
        delay(MOTOR_SURGEDELAY);  // Surge Current delay
        _integral = 0;
        _previousTime = millis();
      } else {
        _control_speed = PID_Controller(xy_angle, _yaw);
        if (_motor1_dir == 0) {
          if (signbit(_control_speed)) {
            _control_speed = _control_speed + ((float)xy_speed);
            _m1_speed = (uint8_t)_control_speed;
            _m2_speed = xy_speed;
          } else {
            _control_speed = ((float)xy_speed) - _control_speed;
            _m2_speed = (uint8_t)_control_speed;
            _m1_speed = xy_speed;
          }
        } else {
          if (signbit(_control_speed)) {
            _control_speed = _control_speed + ((float)xy_speed);
            _m2_speed = (uint8_t)_control_speed;
            _m1_speed = xy_speed;
          } else {
            _control_speed = ((float)xy_speed) - _control_speed;
            _m1_speed = (uint8_t)_control_speed;
            _m2_speed = xy_speed;
          }
        }
        _m2_speed = constrain(_m2_speed, 0, MOTOR_MAXSPEED);
        _m1_speed = constrain(_m1_speed, 0, MOTOR_MAXSPEED);
#if defined(ARDUINO_ESP32C6_DEV)
        myCodeCell.Drive1.Drive(_motor2_dir, (_m1_speed / speed_level[speed_reduction]));
        myCodeCell.Drive2.Drive(_motor1_dir, (_m2_speed / speed_level[speed_reduction]));
#else
        Motor1.Drive(_motor1_dir, (_m1_speed / speed_level[speed_reduction]));
        Motor2.Drive(_motor2_dir, (_m2_speed / speed_level[speed_reduction]));
#endif
      }
      _motor_off = 0;
    }
  }
  _motor1_dir_last = _motor1_dir;
  _motor2_dir_last = _motor2_dir;
}

void ProtoBot::SaveReverse(uint8_t joystick_x, uint8_t joystick_y) {
  if (_drive_counter < 6000) {
    _yaw_zero_new = _yaw_zero;
    _reverse_joystick_x[_drive_counter] = 200 - joystick_x;
    _reverse_joystick_y[_drive_counter] = 200 - joystick_y;
    _drive_counter++;
    _reverse_counter = (_drive_counter - 1);
  } else {
    //Stop saving
  }
}

void ProtoBot::PlayReverse() {
  if (_reverse_counter > 0) {
    _reverse_counter--;
    rx_joystick_x = _reverse_joystick_x[_reverse_counter];
    rx_joystick_y = _reverse_joystick_y[_reverse_counter];
  } else {
    ClearReverse();
  }
}

void ProtoBot::ClearReverse() {
  for (uint16_t rv = 0; rv < 6000; rv++) {
    _reverse_joystick_x[rv] = 100;
    _reverse_joystick_y[rv] = 100;
  }
  _drive_counter = 0;
  reverse_button = 0;
  rx_joystick_x = 100;
  rx_joystick_y = 100;
}

bool ProtoBot::EyeSensInit() {
  uint8_t err;

  // PS_CONF1_2 (0x03): keep your earlier stable base (SD=0)
  Wire.beginTransmission(VCNL4030_ADDRESS);
  Wire.write((uint8_t)VCNL4030_PS_CONF1_2);
  Wire.write((uint8_t)0x0E);  // CONF1: RUN (SD=0), stable IT/PERS
  Wire.write((uint8_t)0x28);  // CONF2: single mode x8, 16-bit output,
  err = Wire.endTransmission(true);

  // PS_CONF3_MS (0x04): smart persistence ON, LED current MIN (5 mA)
  Wire.beginTransmission(VCNL4030_ADDRESS);
  Wire.write((uint8_t)VCNL4030_PS_CONF3_MS);
  Wire.write((uint8_t)0x11);  // CONF3: LED_I_LOW = 0 (normal current), PS_SMART_PERS = 1 (on), PS_SC_EN = 1 (sunlight cancel on)
  Wire.write((uint8_t)0x00);  // MS: LED_I bits => 50 mA
  err = Wire.endTransmission(true);

  // ALS_CONF is command code 0x00 (write word: low byte then high byte)
  // Low byte (0x00_L): ALS_SD=0 (power on), ALS_INT_EN=0, ALS_PERS=0, ALS_IT=50ms (000)
  // High byte (0x00_H): set WHITE_SD=1 (keep white channel off), ALS_NS=1 (x1 sensitivity)
  Wire.beginTransmission(VCNL4030_ADDRESS);
  Wire.write((uint8_t)0x00);  // ALS_CONF (0x00_L / 0x00_H)
  Wire.write((uint8_t)0x00);  // 0x00_L
  Wire.write((uint8_t)0x03);  // 0x00_H  (ALS_NS=1, WHITE_SD=1)
  err = Wire.endTransmission(true);

  if (err) {
    Serial.println(">> Error: EyeLight Sensor not found - Check Hardware");
    return false;
  }
  ReadProximityFront();

  return true;
}

bool ProtoBot::EyeSensDetect() {
  bool detect = false;
  if (FrontAvoidOnOFF) {
    if ((!_FrontAvoidOnOFF_Flag) || (_EyeTracking_Flag)) {
      StopHandler();
      _EyeTracking_Flag = 0;
      _FrontAvoidOnOFF_Flag = 1;
      _eyelight_value = ReadProximityFront();  //Update last
      delay(1);
      _eyelight_value = ReadProximityFront();  //Save new threshold
    }
    if (ReadProximityFront() > (_eyelight_value + 10U)) {
      sleepTimer = 0;
      myEyeLight.show_Static(500, 0XFFFFFF);
      if (_front_detect_timer <= 3) {
        _front_detect_timer++;
      } else {
        detect = true;
      }
    } else {
      _front_detect_timer = 0;
    }
  } else {
    _FrontAvoidOnOFF_Flag = 0;
    _front_detect_timer = 0;
  }
  return detect;
}

void ProtoBot::EyeTracking() {
  uint16_t prox = ReadProximityFront();

  if ((prox > (_prox_last + 3U)) && (!_EyeTracking_Flag)) {
    //Hand detected
    _EyeTracking_Flag = 1;
    myEyeLight.show_FullColor(0xff, 0x66, 0x0);  //light up eye
    delay(500);                                  //add small stabilization delay
    _eyelight_value = ReadProximityFront();      // Read proximity value after stabilization
  } else if (!_EyeTracking_Flag) {
    //Hand still not detected
    _eyelight_off_value = prox;  //save value
  } else {
    if (_EyeTracking_Flag) {
      //Hand already detected
      sleepTimer = 0;
      if (prox < (_eyelight_off_value + EYELIGHT_THRESHOLD)) {
        // Hand lost
        _EyeTracking_Flag = 0;
        _eyelight_value = 0;
        StopHandler();
      } else if (prox > (_eyelight_value + EYELIGHT_THRESHOLD)) {
        _motor1_dir = 1;
        _motor2_dir = 1;
        myCodeCell.Drive1.Drive(_motor1_dir, (uint8_t)(MOTOR_DEADZONE + 10));
        myCodeCell.Drive2.Drive(_motor2_dir, (uint8_t)(MOTOR_DEADZONE + 10));
      } else if (prox < (_eyelight_value - EYELIGHT_THRESHOLD)) {
        _motor1_dir = 0;
        _motor2_dir = 0;
        myCodeCell.Drive1.Drive(_motor1_dir, (uint8_t)(MOTOR_DEADZONE + 10));
        myCodeCell.Drive2.Drive(_motor2_dir, (uint8_t)(MOTOR_DEADZONE + 10));
      } else {
        //Stable - hand within region
        myCodeCell.Drive1.Drive(_motor1_dir, 0);
        myCodeCell.Drive2.Drive(_motor2_dir, 0);
        StopHandler();
      }
    }
  }

  _prox_last = prox;
}


void ProtoBot::Drive(uint8_t speed, float angle) {
  speed = constrain(speed, 0, 100);
  if (speed != 0) {
    speed = (uint8_t)(map(speed, 1, 100, MOTOR_DEADZONE, 100));
  }
  _drive_speed = speed;
  _drive_angle = fmod(angle, 360.0);

  shapeOnOFF = 0;
  _manual_mode = 1;
}

void ProtoBot::DriveSquare(uint8_t speed_level, uint8_t shape_size, bool loop) {
  if (!shapeOnOFF) {
    shape_size = constrain(shape_size, 0, 100);
    SetSpeed(speed_level);
    shapeOnOFF = 1;
    shape_num = 3;
    shape_thr = shape_size;  //0 to 100
    shape_loop = loop;
    _manual_mode = 1;
  }
}

void ProtoBot::DriveCircle(uint8_t speed_level, uint8_t shape_size, bool loop) {
  if (!shapeOnOFF) {
    shape_size = constrain(shape_size, 0, 100);
    SetSpeed(speed_level);
    shapeOnOFF = 1;
    shape_num = 1;
    shape_thr = shape_size;  //0 to 100
    shape_loop = loop;
    _manual_mode = 1;
  }
}

void ProtoBot::DriveInfinity(uint8_t speed_level, uint8_t shape_size, bool loop) {
  if (!shapeOnOFF) {
    shape_size = constrain(shape_size, 0, 100);
    SetSpeed(speed_level);
    shapeOnOFF = 1;
    shape_num = 4;
    shape_thr = shape_size;  //0 to 100
    shape_loop = loop;
    _manual_mode = 1;
  }
}

void ProtoBot::DriveTriangle(uint8_t speed_level, uint8_t shape_size, bool loop) {
  if (!shapeOnOFF) {
    shape_size = constrain(shape_size, 0, 100);
    SetSpeed(speed_level);
    shapeOnOFF = 1;
    shape_num = 2;
    shape_thr = shape_size;  //0 to 100
    shape_loop = loop;
    _manual_mode = 1;
  }
}

void ProtoBot::Rotate(float rotate_angle) {
  _rotate_shape_angle = fmod(rotate_angle, 360.0);
  shapeOnOFF = 1;
  shape_num = 5;
  shape_timer = 0;
  shape_loop = 0;
  _manual_mode = 1;
}

void ProtoBot::Stop() {
  StopHandler();
}

void ProtoBot::EyeColor(uint8_t red, uint8_t green, uint8_t blue) {
  displayRed = red;
  displayGreen = green;
  displayBlue = blue;
}

void ProtoBot::PrintLog(char *mesage) {
  if ((_mesage_last == nullptr) || (strcmp(mesage, _mesage_last) != 0)) {
    size_t len = strlen(mesage);
    BLE_SendLog(mesage, len);

    if (_mesage_last != nullptr) {
      free(_mesage_last);
    }
    _mesage_last = strdup(mesage);
  }
}

uint8_t ProtoBot::ReadBattery() {
  uint16_t voltage = 0;
  if (!_low_voltage_flag) {
    if ((_motor_off) && (!_balance_mode)) {
      voltage = ((myCodeCell.BatteryVoltageRead() / 8U) - 408U);
    } else {
      voltage = _voltage_last;
    }
    if (_bot_status == STATE_BAT_CHRG) {
      _voltage_last = 101;
    } else if (_bot_status == STATE_USB) {
      _voltage_last = 102;
    } else if (_bot_status == STATE_BAT_FULL) {
      _voltage_last = 100;
    } else if (voltage <= 1) {
      _voltage_last = 1;
    } else if ((voltage <= _voltage_last) && (voltage <= 100)) {
      _voltage_last = voltage;
    } else {
      //Spike Voltage Filter
    }
  } else {
    _voltage_last = 0;
    if (_bot_status == STATE_BAT_CHRG) {
      _low_voltage_flag = 0;
    }
  }
  return _voltage_last;
}

void ProtoBot::AlignNorth(bool status) {
  if (status) {
    faceNorthOnOFF = 1;
  } else {
    faceNorthOnOFF = 0;
  }
}

void ProtoBot::SetSpeed(uint8_t level) {
  speed_reduction = level;
  if (speed_reduction == 0) {
    speed_reduction = 1;
  } else if (speed_reduction >= 3) {
    speed_reduction = 3;
  } else {
    //skip
  }
}

bool ProtoBot::ReadTap() {
  float ax = 0.0, ay = 0.0, az = 0.0;  // Store acceleration values
  if (ReadSpeed() == 0) {
    myCodeCell.Motion_AccelerometerRead(ax, ay, az);  // Read accelerometer X, Y, Z data
    float totalAcceleration = sqrt((ax * ax) + (ay * ay) + (az * az));

    if (totalAcceleration > 11) {
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

void ProtoBot::ReadMotion(float &roll, float &pitch, float &yaw) {
  roll = _roll;
  pitch = _pitch;
  yaw = _yaw;
}

float ProtoBot::ReadHeading() {
  return _yaw;
}

uint16_t ProtoBot::ReadSpeed() {
  return _drive_speed;
}

uint16_t ProtoBot::ReadLightBase() {
  return myCodeCell.Light_WhiteRead();
}

uint16_t ProtoBot::ReadProximityBase() {
  return _proximity;
}

uint16_t ProtoBot::ReadProximityFront() {
  if (speed_reduction == SPEED_FAST) {
    speed_reduction = SPEED_INTERMEDIATE;
  }

  Wire.beginTransmission(VCNL4030_ADDRESS);
  Wire.write((uint8_t)0x08);  // PS_DATA
  Wire.endTransmission(false);

  uint8_t got = Wire.requestFrom((uint8_t)VCNL4030_ADDRESS, (uint8_t)2, (bool)true);
  if (got != 2) {
    while (Wire.available()) { (void)Wire.read(); }
    Serial.println(">> Error: EyeLight Sensor not found - Check Hardware");
    return 0xFFFF;  // sentinel = invalid
  }

  uint8_t lo = Wire.read();
  uint8_t hi = Wire.read();

  _eyelight_proximity = (((uint16_t)hi << 8) | lo);

  if ((_eyelight_proximity == 0) || (_eyelight_proximity == 0xFFFF)) {
    EyeSensInit();  //Reset sensor
    _eyelight_proximity = _eyelight_proximity_last;
  } else {
    _eyelight_proximity_last = _eyelight_proximity;
  }

  return _eyelight_proximity;
}

uint16_t ProtoBot::ReadLightFront() {
  if (speed_reduction == SPEED_FAST) {
    speed_reduction = SPEED_INTERMEDIATE;
  }

  Wire.beginTransmission(VCNL4030_ADDRESS);
  Wire.write((uint8_t)0x0B);  // ALS_DATA
  Wire.endTransmission(false);

  uint8_t got = Wire.requestFrom((uint8_t)VCNL4030_ADDRESS, (uint8_t)2, (bool)true);
  if (got != 2) {
    while (Wire.available()) { (void)Wire.read(); }
    Serial.println(">> Error: EyeLight Sensor not found - Check Hardware");
    return 0xFFFF;  // sentinel = invalid
  }

  uint8_t lo = Wire.read();  // ALS_Data_L
  uint8_t hi = Wire.read();  // ALS_Data_M

  _eyelight_ambient = (((uint16_t)hi << 8) | lo);

  if (eyelight_ambient_enable) {  //only read value when LED lights up on other side
    if ((_eyelight_ambient == 0) || (_eyelight_ambient == 0xFFFF)) {
      EyeSensInit();  //Reset sensor
      _eyelight_ambient = _eyelight_ambient_last;
    } else {
      _eyelight_ambient_last = _eyelight_ambient;
    }
  } else {
    _eyelight_ambient = _eyelight_ambient_last;
  }
  return _eyelight_ambient;
}
