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

bool _eyelight_i2c_read(uint8_t add, uint8_t *buffer, size_t size);
bool _eyelight_i2c_write(uint8_t add, uint8_t *buffer, size_t size);

uint8_t _eyelight_i2c_write_size = 0;
uint8_t _eyelight_i2c_write_array[10] = { 0 };
uint8_t _eyelight_i2c_read_array[10] = { 0 };

extern bool BlockSave, eep_save, shapeOnOFF, avoidOnOFF, FrontAvoidOnOFF, faceNorthOnOFF, isAppConnectionReady, shape_loop, BlockProgramLoop, BlockProgramEnable;
extern uint8_t BlockIndex, rx_joystick_x, rx_joystick_y, shape_num, reverse_button, first_connect, touchGesture, speed_reduction, displayOnOFF, displayRed, displayGreen, displayBlue;
extern uint16_t BlockTimer, eepTimer, sleepTimer, shape_thr;
extern float circle_angle;
extern uint8_t BlockProgramCmd[5];
extern uint16_t BlockProgramData[5][3];

ProtoBot::ProtoBot() {
}

void ProtoBot::Init() {
  uint8_t error_timer = 0;

  myCodeCell.LED_SetBrightness(10);  //Turn on CodeCell LED
  myCodeCell.LED(0, 0xFF, 0);

  myCodeCell.Init(LIGHT + MOTION_ROTATION + MOTION_STATE);

  Serial.print(">> ProtoBot v" SW_VERSION);
  Serial.println(" Booting.. ");

  error_timer = 0;

  while (FrontAvoidInit() == 1) {
    delay(10);
    FrontAvoidReset();

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
    _shape_timer = 0;
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
  _proximity = Proximity();
  Heading();
  Zero();
  delay(300);
  myCodeCell.Motion_Read();
  myCodeCell.Light_Read();
  _proximity = Proximity();
  Heading();
  Zero();
  ClearReverse();

  for (uint8_t rv = 0; rv < 250; rv++) {
    _reverse_joystick_x[rv] = 100;
    _reverse_joystick_y[rv] = 100;
  }

  _motor1_dir = 1;
  _motor2_dir = 1;
  myCodeCell.LED_SetBrightness(0);  //Turn off CodeCell LED
  myEyeLight.show_DisplayInit();
  _proximity_last = _proximity;
  _init_timer = 0;
}

uint8_t ProtoBot::VoltageLevel() {
  uint8_t voltage = 0;
  if (_motor_off) {
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

  return _voltage_last;
}

void ProtoBot::PrintLog(char *mesage) {
  if ((_mesage_last == nullptr) || (strcmp(mesage, _mesage_last) != 0)) {
    size_t len = strlen(mesage);
    BLE_SendLog(mesage, len);
    _mesage_last = strdup(mesage);
  }
}

uint16_t ProtoBot::Proximity() {
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

void ProtoBot::BlockRun() {
  //Run Active Blocks
  float cmd_angle = 90;

  if (BlockProgramCmd[BlockIndex] != 0) {
    switch (BlockProgramCmd[BlockIndex]) {
      case COMMAND_ROTATE:
        BlockTimer++;
        if (BlockTimer < 120) {
          Rotate(BlockProgramData[BlockIndex][0], true);
        } else {
          BlockTimer = 0;
          BlockIndex++;
          Zero();
          delay(10);
        }
        break;

      case COMMAND_DRIVE:
        if (_orientation_flag) {
          if (BlockProgramData[BlockIndex][2] != 0) {
            cmd_angle = 270;
          } else {
            cmd_angle = 90;
          }
        } else {
          if (BlockProgramData[BlockIndex][2] != 0) {
            cmd_angle = 90;
          } else {
            cmd_angle = 270;
          }
        }
        BlockTimer++;
        if (BlockTimer < (BlockProgramData[BlockIndex][0] * 100)) {
          Drive(BlockProgramData[BlockIndex][1], cmd_angle);
        } else {
          BlockTimer = 0;
          BlockIndex++;
          Zero();
          delay(10);
        }
        break;

      case COMMAND_WAIT:
        BlockTimer++;
        if (BlockTimer < (BlockProgramData[BlockIndex][0] * 100)) {
          Zero();
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
        PathShape(shape_num);

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
        myEyeLight.show_Color(BlockProgramData[BlockIndex][0], BlockProgramData[BlockIndex][1], BlockProgramData[BlockIndex][2]);
        BlockIndex++;
        break;

      default:
        return;
    }
  } else {
    if (!BlockProgramLoop) {
      BlockProgramEnable = 0;
      ClearDisplay();
      Zero();
    }
    BlockIndex = 0;
  }
}

bool ProtoBot::Run(uint8_t loop_timer) {
  if (myCodeCell.Run(100)) {
    uint8_t motion_state = 0;
    uint8_t joystick_x = 0;
    uint8_t joystick_y = 0;

    Heading();
    _proximity = Proximity();
    if (_proximity < 1000) {
      _orientation_flag = 1;
    } else {
      _orientation_flag = 0;
    }
    _bot_status = myCodeCell.PowerStateRead();

    _send_timer++;
    if (_send_timer >= _send_thr) {  // Update metrics every 100ms
      _send_timer = 0;
      BLE_Update(_mode, VoltageLevel(), ((uint16_t)_yaw));
    }

    FrontAvoidRead();

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

    if (_bot_status == STATE_BAT_RUN) {
      if (BlockProgramEnable) {
        if (_blockTouchGesture) {
          if (myEyeLight.show_blockLoading(110, 2, 250, 250, 250)) {
            _blockTouchGesture = 0;
          }
        } else {
          _mode = AUTOMATE_MODE;
          BlockRun();
          if ((_proximity > (_proximity_last + 2)) && (touchGesture == 2) && (_orientation_flag)) {
            BlockProgramEnable = 0;
          }
        }
      } else if (isAppConnectionReady) {
        if (BlockSave == 1) {
          //wait
        } else {
          if (reverse_button == 4) {
            _mode = REV_MODE;
            PlayReverse();  //If Rev pressed overwrite joystick
          }

          if (_orientation_flag) {
            joystick_x = 200 - rx_joystick_x;
            joystick_y = 200 - rx_joystick_y;
          } else {
            avoidOnOFF = 0;
            joystick_x = rx_joystick_x;
            joystick_y = rx_joystick_y;
          }
          _drive_speed = JoyStick_Get_Speed(joystick_x, joystick_y);
          _drive_angle = JoyStick_Get_Angle(joystick_x, joystick_y);

          if ((shapeOnOFF) && (reverse_button != 4) && (_init_timer >= 30)) {
            _mode = SHAPE_MODE;
            PathShape(shape_num);
            sleepTimer = 0;
            _send_thr = 10;  // Update metrics every 100ms
            _drive_counter = 0;
          } else if ((_drive_speed == 0) && (_motor_off == 1) && (reverse_button != 4)) {
            _drive_counter = 0;
            if (avoidOnOFF) {
              sleepTimer = 0;
              _send_thr = 10;  // Update metrics every 100ms
              AvoidUpMode();
            } else if (((_pitch > 225) && (_pitch < 315)) || ((_pitch > 45) && (_pitch < 135))) {
              _balance_mode = 1;
              bool side_balance = false;

              if ((_pitch > 225) && (_pitch < 315)) {
                side_balance = true;
              }
              Balance(side_balance);
            } else if (faceNorthOnOFF) {
              _proximity_flag = 0;
              sleepTimer = 0;
              _send_thr = 10;  // Update metrics every 100ms
              if (_control_rotate > MOTOR_DEADZONE) {
                myEyeLight.show_Align(40);
              } else {
                myEyeLight.show_Default(90, displayRed, displayGreen, displayBlue);
              }
              Rotate(90, false);
            } else {
              if (_FrontAvoidClear) {
                _FrontAvoidClear = 0;
                _eyelight_value = 0;
                _eyelight_proximity_last = 0;
                _eyelight_proximity = 0;
              }
              if (FrontAvoidDetect()) {  //Proxmity detected
                _motor1_speed = 70;
                _motor2_speed = 70;
                if (_orientation_flag) {
                  _motor1_dir = 0;
                  _motor2_dir = 0;
                } else {
                  _motor1_dir = 1;
                  _motor2_dir = 1;
                }
#if defined(ARDUINO_ESP32C6_DEV)
                myCodeCell.Drive1.Drive(_motor1_dir, _motor1_speed);
                myCodeCell.Drive2.Drive(_motor2_dir, _motor2_speed);
#else
                Motor1.Drive(_motor1_dir, _motor1_speed);
                Motor2.Drive(_motor2_dir, _motor2_speed);
#endif
                delay(250);
              } else {
                if (_balance_mode) {
                  _balance_mode = 0;
                  Zero();
                }
                _proximity_flag = 0;
                motion_state = myCodeCell.Motion_StateRead();
                if (sleepTimer < 1000) {  //wait 10 sec
                  if (displayOnOFF != 0) {
                    myEyeLight.show_Default(90, displayRed, displayGreen, displayBlue);
                  } else {
                    ClearDisplay();
                  }
                  _send_thr = 10;  // Update metrics every 100ms
                  TouchHandle();
                } else {
                  _send_thr = 1000;  // Update metrics every 10000ms
                  ClearDisplay();
                  if (motion_state == MOTION_STATE_MOTION) {
                    sleepTimer = 0;
                  }
                }
              }
            }
          } else {  //Normal Mode
            if (_FrontAvoidClear) {
              _FrontAvoidClear = 0;
              _eyelight_value = 0;
              _eyelight_proximity_last = 0;
              _eyelight_proximity = 0;
            }
            if (FrontAvoidDetect()) {  //Proxmity detected
              _motor1_speed = 70;
              _motor2_speed = 70;
              if (_orientation_flag) {
                _motor1_dir = 0;
                _motor2_dir = 0;
              } else {
                _motor1_dir = 1;
                _motor2_dir = 1;
              }
#if defined(ARDUINO_ESP32C6_DEV)
              myCodeCell.Drive1.Drive(_motor1_dir, _motor1_speed);
              myCodeCell.Drive2.Drive(_motor2_dir, _motor2_speed);
#else
              Motor1.Drive(_motor1_dir, _motor1_speed);
              Motor2.Drive(_motor2_dir, _motor2_speed);
#endif
              delay(250);
            } else {
              _FrontAvoidClear = 1;
              if (reverse_button != 4) {
                SaveReverse(rx_joystick_x, rx_joystick_y);
                _mode = JOYSTICK_MODE;
              }
              Drive(_drive_speed, _drive_angle);
              sleepTimer = 0;
              _send_thr = 10;  // Update metrics every 100ms
            }
          }
        }
      } else {
        myEyeLight.show_Loading(110, 2, 0, 0, 250);
        _advertise_timer++;
        if (_advertise_timer >= 500) {  //if 5sec passed
          _advertise_timer = 0;
          BLE_Advertise();
        }
        if (first_connect == FIRST_CONNECT_KEY) {
          TouchHandle();
        } else {
          if (shapeOnOFF == 1) {
            PathShape(shape_num);  //Make square shape
          } else {
            first_connect = FIRST_CONNECT_KEY;
          }
        }
        sleepTimer = 0;
        _send_thr = 100;  // Update metrics every 1000ms
      }
    } else {
      _mode = 0xFF;  //Clear Mode
      if (_bot_status == STATE_BAT_CHRG) {
        myCodeCell.LED(0, 0, 255);
      } else if (_bot_status == STATE_BAT_LOW) {
        myCodeCell.LED(255, 0, 0);
      } else {
        //skip
      }
      Hibernate();
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
}

void ProtoBot::Balance(bool side) {
  _Kp = 8.0;
  _Ki = 0.05;
  _Kd = 0.3;
  if (side) {
    _control_rotate = PID_Controller(270, _pitch);
  } else {
    _control_rotate = PID_Controller(90, _pitch);
  }
  if (_control_rotate >= 0) {
    _motor1_dir = 1;
    _motor2_dir = 1;
  } else {
    _motor1_dir = 0;
    _motor2_dir = 0;
    _control_rotate = abs(_control_rotate);
  }

  long motor_duty = (long)constrain(_control_rotate, 0, 100);
  motor_duty = map(motor_duty, 0, 100, (MOTOR_DEADZONE - 3), MOTOR_MAXSPEED);

  myCodeCell.Drive1.Drive(_motor1_dir, (uint8_t)motor_duty);
  myCodeCell.Drive2.Drive(_motor2_dir, (uint8_t)motor_duty);

  if (motor_duty == MOTOR_DEADZONE) {
    myEyeLight.reset_Eye();
  } else {
    myEyeLight.show_Default(((motor_duty / 3) + 10), displayRed, displayGreen, displayBlue);
  }
}

void ProtoBot::TouchHandle(void) {
  if (_init_timer < 30) {
    _init_timer++;
    Zero();
  } else {
    if ((_proximity > (_proximity_last + 2)) && (touchGesture == 2) && (_orientation_flag) && (!BlockProgramEnable)) {
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
        Zero();
      }
    }
  }
}

void ProtoBot::Hibernate(void) {
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
  if (_bot_status == STATE_BAT_LOW) {
    if (_lowbat_tone == 0) {
      _lowbat_tone = 1;
    }
  } else {
    _lowbat_tone = 0;
  }
  myEyeLight.show_PowerStatus(_bot_status);
}

void ProtoBot::PathShape(uint8_t shape_type) {
  uint16_t time_shape_thr = 0;
  uint16_t new_shape_thr = 0;
  _motor1_dir = 1;

  if ((first_connect == FIRST_CONNECT_KEY) && (!BlockProgramEnable)) {
    myEyeLight.show_Loading(110, 2, displayRed, displayGreen, displayBlue);
  }
  _shape_timer++;
  switch (shape_type) {
    case 1:
      //Make circle shape
      new_shape_thr = map(shape_thr, 0, 100, MOTOR_DEADZONE, MOTOR_MAXSPEED);

#if defined(ARDUINO_ESP32C6_DEV)
      myCodeCell.Drive1.Drive(_motor1_dir, new_shape_thr);
      myCodeCell.Drive2.Drive(_motor1_dir, 100);
#else
      Motor1.Drive(_motor1_dir, new_shape_thr);
      Motor2.Drive(_motor1_dir, 100);
#endif
      circle_angle = fmod(_yaw - _yaw_zero + 360, 360);
      if ((circle_angle <= 100) && (circle_angle >= 80) && (_shape_timer >= 20)) {
        _shape_timer = 0;
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
          Drive(100, 90);
          if (_shape_timer > time_shape_thr) {
            _shape_timer = 0;
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
          Rotate(330, true);
          if ((_control_rotate < 25) || (_shape_timer > ROT_TIME)) {
            _angle_done++;
            if (_angle_done > 3) {
              _angle_done = 0;
              _shape_index++;
              _shape_timer = 0;
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
          Drive(100, 330);
          if (_shape_timer > time_shape_thr) {
            _shape_timer = 0;
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
          Rotate(210, true);
          if ((_control_rotate < 25) || (_shape_timer > ROT_TIME)) {
            _angle_done++;
            if (_angle_done > 3) {
              _angle_done = 0;
              _shape_index++;
              _shape_timer = 0;
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
          Drive(100, 210);
          if (_shape_timer > time_shape_thr) {
            _shape_timer = 0;
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
          Rotate(90, true);
          if ((_control_rotate < 25) || (_shape_timer > ROT_TIME)) {
            _angle_done++;
            if (_angle_done > 3) {
              _angle_done = 0;
              _shape_index = 0;
              _shape_timer = 0;
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
          Drive(100, 90);
          if (_shape_timer > time_shape_thr) {
            _shape_timer = 0;
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
          Rotate(0, true);
          if ((_control_rotate < 25) || (_shape_timer > ROT_TIME)) {
            _angle_done++;
            if (_angle_done > 3) {
              _angle_done = 0;
              _shape_index++;
              _shape_timer = 0;
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
          Drive(100, 0);
          if (_shape_timer > time_shape_thr) {
            _shape_timer = 0;
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
          Rotate(270, true);
          if ((_control_rotate < 25) || (_shape_timer > ROT_TIME)) {
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
              _shape_timer = 0;
            }
          } else {
            _angle_done = 0;
          }
          break;
        case 4:
          Drive(100, 270);
          if (_shape_timer > time_shape_thr) {
            _shape_timer = 0;
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
          Rotate(180, true);
          if ((_control_rotate < 25) || (_shape_timer > ROT_TIME)) {
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
              _shape_timer = 0;
            }
          } else {
            _angle_done = 0;
          }
          break;
        case 6:
          Drive(100, 180);
          if (_shape_timer > time_shape_thr) {
            _shape_timer = 0;
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
          Rotate(90, true);
          if ((_control_rotate < 25) || (_shape_timer > ROT_TIME)) {
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
              _shape_timer = 0;
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
      if (((abs(_error)) < 30) && (_shape_timer >= 5)) {
        _shape_timer = 0;
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
      Drive(100, circle_angle);
      break;
  }
}

void ProtoBot::AvoidUpMode() {
  if ((_proximity > (_proximity_last + 1)) || ((_proximity > _prox_thr) && (_proximity_flag))) {
    if (!_proximity_flag) {
      _prox_thr = _proximity;
    }
    _motor1_speed = MOTOR_MAXSPEED;
    _motor2_speed = MOTOR_MAXSPEED;
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
      if (_squish_timer < 40) {
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

void ProtoBot::Heading() {
  myCodeCell.Motion_RotationRead(_roll, _pitch, _yaw);
  _yaw = fmod(_yaw + 180, 360);      //Wraps Angle: (_yaw−360)*(_yaw/360)
  _roll = fmod(_roll + 180, 360);    //Wraps Angle
  _pitch = fmod(_pitch + 180, 360);  //Wraps Angle
}

void ProtoBot::Zero() {
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

  _motor_off = 1;
  _control_angle = 90;
  _integral = 0;
  _error_last = 0;
  _error = 0;
  _derivative = 0;
  _previousTime = millis();
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

void ProtoBot::Rotate(float rotate_angle, bool tare_on) {
  float rYaw = 0.0;

  _Kp = Kp_ROT;
  _Ki = Ki_ROT;
  _Kd = Kd_ROT;

  if (tare_on) {
    rYaw = fmod(_yaw - _yaw_zero + 360, 360);  // Ensures _yaw is within 0-360
  } else {
    rYaw = _yaw;
  }
  if (shapeOnOFF) {
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

void ProtoBot::Drive(uint8_t xy_speed, float xy_angle) {
  int speed_control = 0;
  uint8_t m1_speed = xy_speed;
  uint8_t m2_speed = xy_speed;

  _Kp = Kp_DRV;
  _Ki = Ki_DRV;
  _Kd = Kd_DRV;

  if (xy_angle < 180) {
    xy_angle = 180 - xy_angle;
  } else {
    xy_angle = (360 - xy_angle) + 180;
  }
  if (xy_speed == 0) {
    Zero();
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
    if (xy_speed < 50) {
      xy_speed = xy_speed / 3;
      if (xy_speed < MOTOR_DEADZONE) {
        xy_speed = MOTOR_DEADZONE;
      }
    } else if (xy_speed < 75) {
      xy_speed = xy_speed / 2;
    } else {
      //Skip
    }
    if (xy_speed < MOTOR_DEADZONE) {
      xy_speed = MOTOR_DEADZONE;
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
    myCodeCell.Drive2.Drive(_motor2_dir, (xy_speed / speed_level[speed_reduction]));
#else
    Motor1.Drive(_motor1_dir, (xy_speed / speed_level[speed_reduction]));
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
    if (xy_speed < 50) {
      xy_speed = xy_speed / 3;
      if (xy_speed < MOTOR_DEADZONE) {
        xy_speed = MOTOR_DEADZONE;
      }
    } else if (xy_speed < 75) {
      xy_speed = xy_speed / 2;
    } else {
      //Skip
    }
    if (xy_speed < MOTOR_DEADZONE) {
      xy_speed = MOTOR_DEADZONE;
    }
#if defined(ARDUINO_ESP32C6_DEV)
    myCodeCell.Drive1.Drive(_motor1_dir, (xy_speed / speed_level[speed_reduction]));
    myCodeCell.Drive2.Drive(_motor2_dir, (xy_speed / speed_level[speed_reduction]));
#else
    Motor1.Drive(_motor1_dir, (xy_speed / speed_level[speed_reduction]));
    Motor2.Drive(_motor2_dir, (xy_speed / speed_level[speed_reduction]));
#endif
    _motor_off = 0;
    if (!BlockProgramEnable) {
      myEyeLight.show_Turn(-1, 120, 3, displayRed, displayGreen, displayBlue);
    }
  } else {
    if (_spin_flag) {
      _spin_flag = 0;
      Zero();
    } else {
      //Robot is moving
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
            m1_speed = (uint8_t)_control_speed;
            m2_speed = xy_speed;
          } else {
            _control_speed = ((float)xy_speed) - _control_speed;
            m2_speed = (uint8_t)_control_speed;
            m1_speed = xy_speed;
          }
        } else {
          if (signbit(_control_speed)) {
            _control_speed = _control_speed + ((float)xy_speed);
            m2_speed = (uint8_t)_control_speed;
            m1_speed = xy_speed;
          } else {
            _control_speed = ((float)xy_speed) - _control_speed;
            m1_speed = (uint8_t)_control_speed;
            m2_speed = xy_speed;
          }
        }
        m2_speed = constrain(m2_speed, 0, MOTOR_MAXSPEED);
        m1_speed = constrain(m1_speed, 0, MOTOR_MAXSPEED);
#if defined(ARDUINO_ESP32C6_DEV)
        myCodeCell.Drive1.Drive(_motor2_dir, (m1_speed / speed_level[speed_reduction]));
        myCodeCell.Drive2.Drive(_motor1_dir, (m2_speed / speed_level[speed_reduction]));
#else
        Motor1.Drive(_motor1_dir, (m1_speed / speed_level[speed_reduction]));
        Motor2.Drive(_motor2_dir, (m2_speed / speed_level[speed_reduction]));
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

bool ProtoBot::FrontAvoidDetect() {
  bool detect = false;
  if (FrontAvoidOnOFF) {
    if ((_eyelight_dir) && (_eyelight_value > 30)) {
      _eyelight_value = 0;
      myEyeLight.show_Static(500, 0XFFFFFF);
      // Serial.print(" Detected! ");
      // PrintLog("Detected");
      detect = true;
    } else {
      // Serial.print(" Searching! ");
      // PrintLog("Searching");
      _eyelight_dir = 0;
    }

    // Serial.print(_eyelight_value);
    // Serial.print(" - _eyelight_proximity: ");
    // Serial.print(_eyelight_proximity);
    // Serial.print(" _eyelight_proximity_last: ");
    // Serial.println(_eyelight_proximity_last);
  } else {
    // PrintLog("OFF");
  }
  return detect;
}

void ProtoBot::FrontAvoidRead() {
  _frontavoid_timer++;
  if (_frontavoid_timer > 30) {
    _frontavoid_timer = 0;

    // Point to PS_DATA (0x08)
    Wire.beginTransmission(VCNL4030_ADDRESS);
    Wire.write(VCNL4030_PS_DATA);
    Wire.endTransmission(false);  // repeated start

    if (!_eyelight_i2c_read(VCNL4030_ADDRESS, _eyelight_i2c_read_array, 2)) {
      Wire.endTransmission(true);
      Serial.println(">> Error: EyeLight read failed");
    }

    _eyelight_proximity = (((uint32_t)(_eyelight_i2c_read_array[1]) << 8)) | ((uint32_t)_eyelight_i2c_read_array[0]);

    if (_eyelight_proximity > _eyelight_proximity_last) {
      _eyelight_dir = 1;
      _eyelight_value = _eyelight_proximity - _eyelight_proximity_last;
    } else {
      _eyelight_value = 0;
    }

    _eyelight_proximity_last = _eyelight_proximity;
  }
}

bool ProtoBot::FrontAvoidInit() {
  bool light_error = 0;
  uint8_t error_timer = 0;

  Wire.beginTransmission(VCNL4030_ADDRESS);
  if (Wire.endTransmission() != 0) {
    while (error_timer < 100) {
      delay(10);
      Wire.beginTransmission(VCNL4030_ADDRESS);
      if (Wire.endTransmission() != 0) {
        error_timer++;
        if (error_timer >= 99) {
          myCodeCell.LED(LED_SLEEP_BRIGHTNESS, 0, 0);
          Serial.println();
          Serial.println(">> Error: EyeLight Sensor not found - Check Hardware");
          //while (1) {delay(10);  }
        }
      } else {
        error_timer = 200;
      }
    }
  }

  // PS_CONF1_2 (0x03): LSB = 0x0E  (IT=8T),  MSB = 0x18  (HD=1, GAIN=01)
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = VCNL4030_PS_CONF1_2; /*Address*/
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x0E;                /*LSB*/
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x18;                /*MSB*/
  if (!_eyelight_i2c_write(VCNL4030_ADDRESS, _eyelight_i2c_write_array, _eyelight_i2c_write_size)) {
    light_error = 1;
  }
  _eyelight_i2c_write_size = 0;

  // PS_CONF3_MS (0x04): LSB = 0x01 (SC_EN=1, LED_I_LOW=0), MSB = 0x77 (SC_CUR=3, PS_SP=1, LED_I=5A)
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = VCNL4030_PS_CONF3_MS; /*Address*/
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x81;                 /*LSB*/
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x72;                 /*MSB*/
  if (!_eyelight_i2c_write(VCNL4030_ADDRESS, _eyelight_i2c_write_array, _eyelight_i2c_write_size)) {
    light_error = 1;
  }
  _eyelight_i2c_write_size = 0;

  // CANC = 0
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = VCNL4030_PS_CANC; /*Address*/
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x00;             /*LSB*/
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x00;             /*MSB*/
  if (!_eyelight_i2c_write(VCNL4030_ADDRESS, _eyelight_i2c_write_array, _eyelight_i2c_write_size)) {
    light_error = 1;
  }
  _eyelight_i2c_write_size = 0;
  Wire.endTransmission();

  if (!light_error) {
    Serial.println(">> ProtoBot: EyeLight Sensor Activated");

    _frontavoid_timer = 0;
    // Point to PS_DATA (0x08)
    Wire.beginTransmission(VCNL4030_ADDRESS);
    Wire.write(VCNL4030_PS_DATA);
    Wire.endTransmission(false);  // repeated start

    if (!_eyelight_i2c_read(VCNL4030_ADDRESS, _eyelight_i2c_read_array, 2)) {
      Wire.endTransmission(true);
      Serial.println(">> Error: EyeLight read failed");
    }

    _eyelight_proximity_last = ((uint16_t)(_eyelight_i2c_read_array[1]) << 8) | (uint16_t)(_eyelight_i2c_read_array[0]);
  }

  return light_error;
}


void ProtoBot::FrontAvoidReset() {
  _eyelight_i2c_write_size = 0;

  // Write PS_CONF1_2 (register 0x03) to default/safe state ===
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = VCNL4030_PS_CONF1_2;  // Address
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x00;                 // LSB: PS_CONF1 = all off
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x00;                 // MSB: PS_CONF2 = default
  if (!_eyelight_i2c_write(VCNL4030_ADDRESS, _eyelight_i2c_write_array, _eyelight_i2c_write_size)) {
    Serial.println(">> Error: EyeLight Sensor not responding");
  }

  // Write PS_CONF3_MS (register 0x04) to default/safe state ===
  _eyelight_i2c_write_size = 0;
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = VCNL4030_PS_CONF3_MS;  // Address
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x00;                  // LSB: PS_CONF3 = all off
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x00;                  // MSB: PS_MS = all off
  if (!_eyelight_i2c_write(VCNL4030_ADDRESS, _eyelight_i2c_write_array, _eyelight_i2c_write_size)) {
    Serial.println(">> Error: EyeLight Sensor not responding");
  }

  // Clear cancellation offset (optional but clean) ===
  _eyelight_i2c_write_size = 0;
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = VCNL4030_PS_CANC;  // Address
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x00;              // LSB
  _eyelight_i2c_write_array[_eyelight_i2c_write_size++] = 0x00;              // MSB
  if (!_eyelight_i2c_write(VCNL4030_ADDRESS, _eyelight_i2c_write_array, _eyelight_i2c_write_size)) {
    Serial.println(">> Error: EyeLight Sensor not responding");
  }
}

bool _eyelight_i2c_write(uint8_t add, uint8_t *buffer, size_t size) {
  Wire.beginTransmission(add);
  // Write the data buffer
  if (Wire.write(buffer, size) != size) {
    // If the number of bytes written is not equal to the length, return false
    Wire.endTransmission();  // Ensure to end transmission even if writing fails
    return false;
  }

  if (Wire.endTransmission() == 0) {
    return true;
  } else {
    return false;
  }
}

bool _eyelight_i2c_read(uint8_t add, uint8_t *buffer, size_t size) {
  size_t pos = 0;
  while (pos < size) {
    size_t read_size = min((size_t)32, size - pos);

    size_t recv = Wire.requestFrom(add, read_size);
    if (recv != read_size) return false;

    for (size_t i = 0; i < read_size; i++) {
      buffer[pos + i] = Wire.read();
    }
    pos += read_size;
  }
  return true;
}
