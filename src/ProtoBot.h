#ifndef PROTOBOT_H
#define PROTOBOT_H

#include <Arduino.h>
#include <Wire.h>

#define MOTION_MODE 1

#define SW_VERSION "1.0.1"
#define HW_VERSION "CodeCell C6 Drive"
#define MANUFACTURER "Microbots"

#define XY_MIDPOINT 100U

#define STATE_BAT_RUN 0U
#define STATE_USB 1U
#define STATE_INIT 2U
#define STATE_BAT_LOW 3U
#define STATE_BAT_FULL 4U
#define STATE_BAT_CHRG 5U
#define STATE_MTR_ON 7U
#define STATE_MTR_OFF 8U

#define DEVICE_NAME "protobot"
#define MOTOR_ORIENTATION 1
#define MOTOR_DEADZONE 50U
#define MOTOR_MAXSPEED 90U
#define MOTOR_SURGEDELAY 50U
#define ROT_TIME 40

#define VCNL4030_ADDRESS 0x51
#define VCNL4030_PS_CONF1_2 0x03
#define VCNL4030_PS_CONF3_MS 0x04
#define VCNL4030_PS_CANC 0x05
#define VCNL4030_PS_DATA 0x08
#define VCNL4030_INT_FLAG 0x0D
#define VCNL4030_REG_ID 0x0E

#define LED_NUM 6
#define LED_BRIGHTNESS 200

#define JOYSTICK_MODE 0
#define REV_MODE 1
#define SHAPE_MODE 2
#define AUTOMATE_MODE 3

#define Kp_ROT 3
#define Ki_ROT 2.8
#define Kd_ROT 0.3

#define Kp_DRV 4
#define Ki_DRV 1.1
#define Kd_DRV 0.8

#define WINDUP_MAX 100000.0   // Maximum value for _integral term
#define WINDUP_MIN -100000.0  // Minimum value for _integral term

#define AVRG_FILTER_SIZE 32U

#define EEPROM_SIZE 28
#define EEPROM_FIRST_CONNECT 0
#define EEPROM_COLOR_RED 1
#define EEPROM_COLOR_GREEN 2
#define EEPROM_COLOR_BLUE 3
#define EEPROM_DISPLAY_EN 4
#define EEPROM_SPD_LEVEL 5
#define EEPROM_AUTO_LOCK 6
#define EEPROM_BLOCK_CMD 7
#define EEPROM_BLOCK_DATA 12


#if defined(ARDUINO_ESP32C6_DEV)
#else
#define C3_IN1_pin1 6U
#define C3_IN1_pin2 5U
#define C3_IN2_pin1 3U
#define C3_IN2_pin2 2U
#endif

#define FIRST_CONNECT_KEY 0xC

void TareProximity(void);

class ProtoBot {
private:
  uint16_t _prox_array[AVRG_FILTER_SIZE] = { 0 };
  uint8_t _reverse_joystick_x[6000] = { 0 };
  uint8_t _reverse_joystick_y[6000] = { 0 };
  float speed_level[4] = { 1.0, 1.0, 1.25, 1.5 };

  bool _spin_flag = 0, _FrontAvoidClear = 0, _eyelight_dir = 0, _orientation_flag = 0, _balance_mode = 0, _line_side = 0, _line_revdir = 0, _blockTouchGesture = 0, _motor1_dir = 0, _motor2_dir = 0, _motor1_dir_last = 0, _motor2_dir_last = 0, _motor_off = 0, _proximity_flag = 0, _lowbat_tone = 0;
  uint8_t _frontavoid_timer = 0, _run_timer = 0, _miss_counter = 0, _line_hit = 0, _line_miss = 0, _turn_timer = 0, _mode = 0, _angle_done = 0, _shape_index = 0, _block_shape_counter = 0, _init_timer = 0, _proximity_timer = 0, _motionoff_last = 0, _infinity_phase = 0, _bot_status = 0, _shape_done = 0, _voltage_last = 100, _prox_index = 0;
  uint16_t _advertise_timer = 0, _line_darkness = 0, _line_angle = 0, _reverse_counter = 0, _drive_counter = 0, _prox_thr = 0, _proximity_last = 0, _send_timer = 0, _send_thr = 10, _squish_timer = 0, _proximity = 0, _drive_speed = 0, _shape_timer = 0, _motor1_speed = 0U, _motor2_speed = 0U;
  uint32_t _eyelight_value = 0, _eyelight_proximity = 0, _eyelight_proximity_last = 0;
  unsigned long _previousTime = 0;
  float _error = 0.0, _error_last = 0.0, _control_rotate = 0.0, _integral = 0.0, _derivative = 0.0, _control_angle = 0, _Kp = 0.0, _Ki = 0.0, _Kd = 0.0, _control_speed = 0, _drive_angle = 0, _speed_grad = 0.0, _speed_c = 0.0, _yaw_zero_new = 0, _yaw_zero = 0.0, _roll_zero = 0.0, _pitch_zero = 0.0, _yaw = 0.0, _roll = 0.0, _pitch = 0.0;

  char *_mesage_last = nullptr;

  uint8_t JoyStick_Get_Speed(uint8_t joystick_x, uint8_t joystick_y);
  float JoyStick_Get_Angle(uint8_t joystick_x, uint8_t joystick_y);
  void Zero();
  void AvoidUpMode();
  float PID_Controller(float target_output, float input_signal);
  uint8_t VoltageLevel();
  void Heading();
  uint16_t Proximity();
  void SaveReverse(uint8_t joystick_x, uint8_t joystick_y);
  void ClearReverse();
  void PlayReverse();
  void BlockRun();
  void TouchHandle();
  void Balance(bool side);
  bool FrontAvoidDetect();
  bool FrontAvoidInit();
  void FrontAvoidReset();
  void FrontAvoidRead();

public:
  ProtoBot();
  void Init();
  bool Run(uint8_t loop_timer);
  void Drive(uint8_t xy_speed, float xy_angle);
  void Rotate(float rotate_angle, bool tare_on);
  void PathShape(uint8_t shape_type);
  void Hibernate(void);
  void PrintLog(char *mesage);
};

#endif