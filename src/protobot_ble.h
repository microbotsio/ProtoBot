#ifndef PROTOBOT_BLE_H
#define PROTOBOT_BLE_H

#include <Arduino.h>

#define SERVICE_UUID "12345678-1234-1234-1234-123456789012"
#define GENERAL_SERVICE_UUID "12345678-1234-1234-1234-123456789015"
#define JOYSTICK_UUID "abcd1234-abcd-1234-abcd-123456789012"
#define METRICS_UUID "dcba4322-dcba-4321-dcba-432123456789"
#define BATTERY_UUID "56781234-5678-5678-5678-567812345671"
#define SHAPE_UUID "dcba4327-dcba-4321-dcba-432123456789"
#define SETTINGS_UUID "dcba4330-dcba-4321-dcba-432123456789"
#define ACTION_RX_UUID "dcba4328-dcba-4321-dcba-432123456789"
#define BLOCK_PROGRAM_UUID "dcba4421-dcba-4321-dcba-432123456789"
#define MODE_UUID "dcba4329-dcba-4321-dcba-432123456789"
#define STRING_UUID "dcba4330-dcba-4321-dcba-432123456791"

#define MAX_COMMANDS 5U

#define COMMAND_DRIVE 1U
#define COMMAND_ROTATE 2U
#define COMMAND_CIRCLE 3U
#define COMMAND_RECT 4U
#define COMMAND_TRIANGLE 5U
#define COMMAND_INFINITY 6U
#define COMMAND_LED 7U
#define COMMAND_WAIT 8U

void BLE_Init(void);
void BLE_SendLog(char *datalog, size_t datalen);
void BLE_Update(uint8_t mode_val, uint8_t battery_val, uint16_t heading_val);
void BLE_Advertise(void);

struct MetricsPacket {
  uint16_t _heading;
};


struct SettingsPacket {
  uint8_t _isAppConnectionReady;
  uint8_t _avoidOnOFF;
  uint8_t _FrontAvoidOnOFF;
  uint8_t _faceNorthOnOFF;
  uint8_t _screenOnOFF;            
  uint8_t _screenRed;          
  uint8_t _screenGreen;            
  uint8_t _screenBlue;             
  uint8_t _speedLevel;            
  uint8_t _proximityLockEnable;    
};


#endif