#ifndef PROTOBOT_H
#define PROTOBOT_H

#include <Arduino.h>
#include <Wire.h>

#define SW_VERSION "1.0.6"
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
#define MOTOR_SPINSPEED 70U
#define MOTOR_SURGEDELAY 100U
#define ROT_TIME 60

#define VCNL4030_ADDRESS 0x51
#define VCNL4030_PS_CONF1_2 0x03
#define VCNL4030_PS_CONF3_MS 0x04
#define VCNL4030_PS_CANC 0x05
#define VCNL4030_PS_DATA 0x08
#define VCNL4030_INT_FLAG 0x0D
#define VCNL4030_REG_ID 0x0E

#define EYELIGHT_THRESHOLD 10

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

#define SPEED_FAST 1
#define SPEED_INTERMEDIATE 2
#define SPEED_SLOW 3

#define SHAPE_LOOP 1
#define SHAPE_ONCE 0

void TareProximity(void);

class ProtoBot {
private:
  uint16_t _prox_array[AVRG_FILTER_SIZE] = { 0 };
  uint8_t _reverse_joystick_x[6000] = { 0 };
  uint8_t _reverse_joystick_y[6000] = { 0 };
  float speed_level[4] = { 1.0, 1.0, 1.25, 1.5 };

  bool _low_voltage_flag = 0, _orientation_balance = 0, _EyeTracking_Flag = 0, _first_spin = 0, _manual_mode = 0, _FrontAvoidOnOFF_Flag = 0, _spin_flag = 0, _orientation_flag = 0, _balance_mode = 0, _blockTouchGesture = 0, _motor1_dir = 0, _motor2_dir = 0, _motor1_dir_last = 0, _motor2_dir_last = 0, _motor_off = 0, _proximity_flag = 0;
  uint8_t _balance_timer = 0, _shape_index = 0, _front_detect_timer = 0, _m1_speed, _m2_speed, _run_timer = 0, _turn_timer = 0, _mode = 0, _angle_done = 0, _block_shape_counter = 0, _init_timer = 0, _infinity_phase = 0, _bot_status = 0, _voltage_last = 100, _prox_index = 0;
  uint16_t _prox_last = 0, _eyelight_off_value = 0, _proximity_stable = 0, _eyelight_change = 0, _block_proxmity_last = 0, _block_light_light_last = 0, _driver_timer = 0, _advertise_timer = 0, _reverse_counter = 0, _drive_counter = 0, _prox_thr = 0, _proximity_last = 0, _send_timer = 0, _send_thr = 10, _squish_timer = 0, _proximity = 0, _drive_speed = 0, _motor1_speed = 0U, _motor2_speed = 0U;
  uint32_t _eyelight_value = 0, _eyelight_proximity = 0, _eyelight_proximity_last = 0, _eyelight_ambient = 0, _eyelight_ambient_last = 0;
  unsigned long _previousTime = 0;
  float _rotate_shape_angle = 0.0, _error = 0.0, _error_last = 0.0, _control_rotate = 0.0, _integral = 0.0, _derivative = 0.0, _Kp = 0.0, _Ki = 0.0, _Kd = 0.0, _control_speed = 0, _drive_angle = 0, _speed_grad = 0.0, _speed_c = 0.0, _yaw_zero_new = 0, _yaw_zero = 0.0, _roll_zero = 0.0, _pitch_zero = 0.0, _yaw = 0.0, _roll = 0.0, _pitch = 0.0;
  char *_mesage_last = nullptr;

  uint8_t JoyStick_Get_Speed(uint8_t joystick_x, uint8_t joystick_y);
  float JoyStick_Get_Angle(uint8_t joystick_x, uint8_t joystick_y);
  float PID_Controller(float target_output, float input_signal);

  void SaveReverse(uint8_t joystick_x, uint8_t joystick_y);
  void ClearReverse();
  void PlayReverse();

  bool EyeSensDetect();
  bool EyeSensInit();
  void EyeTracking();

  void StopHandler();
  void AvoidUpHandler();
  void HeadingHandler();
  void BlockHandler();
  void TouchHandler();
  void BalanceHandler(bool side);
  void DriveHandler(uint8_t xy_speed, float xy_angle);
  void ShapeHandler(uint8_t shape_type);
  void HibernateHandler(void);
  void RotateHandler(float rotate_angle, bool tare_on);

  uint16_t ProximityHandler();

public:
  ProtoBot();

  void Init();
  bool Run(uint8_t loop_timer);
  void PrintLog(char *mesage);
  void Drive(uint8_t speed, float angle);
  void DriveSquare(uint8_t speed_level, uint8_t shape_size, bool loop);
  void DriveCircle(uint8_t speed_level, uint8_t shape_size, bool loop);
  void DriveTriangle(uint8_t speed_level, uint8_t shape_size, bool loop);
  void DriveInfinity(uint8_t speed_level, uint8_t shape_size, bool loop);
  void Rotate(float rotate_angle);
  void EyeColor(uint8_t red, uint8_t green, uint8_t blue);
  void AlignNorth(bool status);
  void SetSpeed(uint8_t level);
  void Stop();

  bool ReadTap();
  uint8_t ReadBattery();
  uint16_t ReadProximityFront();
  uint16_t ReadProximityBase();
  uint16_t ReadLightFront();
  uint16_t ReadLightBase();
  uint16_t ReadSpeed();
  float ReadHeading();
  void ReadMotion(float &roll, float &pitch, float &yaw);
};

#endif