#include <BNO085.h>
#include <CodeCell.h>
#include <sh2.h>
#include <sh2_SensorValue.h>
#include <sh2_err.h>
#include <sh2_hal.h>
#include <sh2_util.h>
#include <shtp.h>

#include "ProtoBot.h"
#include "ProtoBot_Eyelight.h"
#include "ProtoBot_Ble.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEOTA.h>
#include <EEPROM.h>

BLECharacteristic *pJoystickCharacteristic = NULL;
BLECharacteristic *pJoystickXCharacteristic = NULL;
BLECharacteristic *pBatteryCharacteristic = NULL;
BLECharacteristic *pMetricsCharacteristic = NULL;
BLECharacteristic *pShapeCharacteristic = NULL;
BLECharacteristic *pSettingsCharacteristic = NULL;
BLECharacteristic *pActionRxCharacteristic = NULL;
BLECharacteristic *pBlockProgramCharacteristic = NULL;
BLECharacteristic *pModeCharacteristic = NULL;
BLECharacteristic *pStringCharacteristic = NULL;

MetricsPacket last_metrics = { 0 };
SettingsPacket last_settings = { 0 };

uint8_t BlockProgramCmd[5] = { 0, 0, 0, 0, 0 };
uint16_t BlockProgramData[5][3];

bool eep_save = 0, BlockProgramEnable = 0, BlockProgramLoop = 0, BlockSave = 0, shapeOnOFF = 0, shapeOnOFF_last = 0, avoidOnOFF = 0, shape_loop = 0, FrontAvoidOnOFF = 0, faceNorthOnOFF = 0, isAppConnectionReady = false;
uint8_t BlockIndex = 0, rx_joystick_x = 100, rx_joystick_y = 100, battery_val_last = 0, mode_val_last = 0xFF, reverse_button = 0, first_connect = 0, displayOnOFF = 0, displayRed = 0xff, displayGreen = 0xff, displayBlue = 0xff, touchGesture = 0, speed_reduction = 3;
uint16_t BlockTimer = 0, eepTimer = 0, sleepTimer = 0, heading_val_last = 0, shape_num = 0, shape_thr = 100;
float circle_angle = 90.0;

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();  // Use Arduino String

    if (pCharacteristic == pJoystickCharacteristic && value.length() > 0) {
      rx_joystick_y = static_cast<uint8_t>(value[1]);  // Joystick Y for speed
      rx_joystick_y = constrain(rx_joystick_y, 0, 200);
      rx_joystick_x = static_cast<uint8_t>(value[0]);  // Joystick X for direction
      rx_joystick_x = constrain(rx_joystick_x, 0, 200);
    } else if (pCharacteristic == pShapeCharacteristic && value.length() > 0) {
      shapeOnOFF = bool(static_cast<uint8_t>(value[0]));
      shape_num = static_cast<uint8_t>(value[1]);
      shape_thr = static_cast<uint8_t>(value[2]);         //0 to 100
      shape_loop = bool(static_cast<uint8_t>(value[3]));  //1 or 0
      circle_angle = 90;
    } else if (pCharacteristic == pActionRxCharacteristic && value.length() > 0) {
      reverse_button = static_cast<uint8_t>(value[0]);
    } else if (pCharacteristic == pSettingsCharacteristic && value.length() > 0) {
      eepTimer = 0;
      eep_save = 1;
      sleepTimer = 0;
      isAppConnectionReady = bool(static_cast<uint8_t>(value[0]));

      if ((isAppConnectionReady) && (first_connect != FIRST_CONNECT_KEY)) {
        first_connect = FIRST_CONNECT_KEY;
        eepTimer = 500;  //save immediately
        shapeOnOFF = 0;
      }
      avoidOnOFF = bool(static_cast<uint8_t>(value[1]));
      FrontAvoidOnOFF = bool(static_cast<uint8_t>(value[2]));
      faceNorthOnOFF = bool(static_cast<uint8_t>(value[3]));

      displayOnOFF = value[4];
      displayRed = value[5];
      displayGreen = value[6];
      displayBlue = value[7];
      speed_reduction = 4U - value[8];
      touchGesture = value[9];

      if (touchGesture == 2) {
        avoidOnOFF = 0;
      } else {
        //skip
      }

    } else if ((pCharacteristic == pBlockProgramCharacteristic) && (value.length() > 0)) {

      uint8_t index = 0;
      uint8_t length = value.length();
      uint8_t commandCount = 0;

      ClearDisplay();

      if (value[index++] == 1) {
        BlockProgramEnable = 1;
        BlockIndex = 0;
        BlockTimer = 0;
      } else {
        BlockProgramEnable = 0;
        return;
      }

      if (value[index++] == 1) {
        BlockProgramLoop = 1;
      } else {
        BlockProgramLoop = 0;
      }
      if (value[index++] == 0) {
        BlockSave = 1;
        eep_save = 1;
        BlockProgramEnable = 0;
      } else {
        BlockSave = 0;
      }

      // Serial.print(" On/Off: ");
      // Serial.println(BlockProgramEnable);
      // Serial.print(" Loop: ");
      // Serial.println(BlockProgramLoop);
      // Serial.print("BlockSave: ");
      // Serial.println(BlockSave);

      for (uint8_t c = 0; c < 5; c++) {
        BlockProgramCmd[c] = 0;
      }
      // Parse commands
      while ((index < length) && (commandCount < MAX_COMMANDS)) {
        BlockProgramCmd[commandCount] = value[index++];

        switch (BlockProgramCmd[commandCount]) {
          case COMMAND_ROTATE:
            BlockProgramData[commandCount][0] = fmod((int16_t)(value[index] | (value[index + 1] << 8)) + 90U, 360);  //Save & Wraps Angle
            index += 2;
            break;

          case COMMAND_DRIVE:
            BlockProgramData[commandCount][0] = value[index++];  //Time
            BlockProgramData[commandCount][1] = value[index++];  //Duty Cycle
            BlockProgramData[commandCount][2] = value[index++];  //Forward/Backward
            break;

          case COMMAND_WAIT:
            BlockProgramData[commandCount][0] = value[index++];  //Time
            break;

          case COMMAND_CIRCLE:
          case COMMAND_RECT:
          case COMMAND_TRIANGLE:
          case COMMAND_INFINITY:
            BlockProgramData[commandCount][0] = value[index++];                 //Duration
            BlockProgramData[commandCount][1] = value[index++];                 //Save loop
            BlockProgramData[commandCount][2] = BlockProgramCmd[commandCount];  //Save Shape Number
            break;

          case COMMAND_LED:
            BlockProgramData[commandCount][0] = value[index++];  //Save Red
            BlockProgramData[commandCount][1] = value[index++];  //Save Green
            BlockProgramData[commandCount][2] = value[index++];  //Save Blue
            break;

          default:
            return;
        }
        commandCount++;
      }
    } else {
      //skip
    }
  }
};

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    Serial.println(">> ProtoBot: Connected to App");
  }

  void onDisconnect(BLEServer *pServer) override {
    rx_joystick_x = 100;
    rx_joystick_y = 100;
    Serial.println(">> ProtoBot: Disconnected from App");
    delay(500);                     // Small delay to ensure proper disconnection before re-advertising
    BLEDevice::startAdvertising();  // Restart advertising so new devices can connect
    Serial.println(">> ProtoBot: Bluetooth Advertising..");
    isAppConnectionReady = false;
  }
};

class OTACallbacks : public BLEOTACallbacks {
public:
  //This callback method is invoked just before the APP OTA update begins.
  void beforeStartOTA() {
    Serial.println(">> ProtoBot: Downloading New Update");
    //ota_flag = 1;
  }
  //This callback method is invoked just before the SPIFFS OTA update begins.
  void beforeStartSPIFFS() {
    Serial.println(">> ProtoBot: Installing New Update");
  }
  //This callback method is invoked just after the update completes.
  void afterStop() {
    Serial.println(">> ProtoBot: New Update Complete");
    //ota_flag = 0;
  }
  //This callback method is invoked just after the update abort.
  void afterAbort() {
    Serial.println(">> ProtoBot: New Update Aborted");
  }
};


void BLE_SendLog(char *datalog, size_t datalen) {
  pStringCharacteristic->setValue((uint8_t *)datalog, datalen);
  pStringCharacteristic->notify();
}

void BLE_Advertise(void) {
  BLEDevice::startAdvertising();  // Restart advertising so new devices can connect
}

void BLE_Update(uint8_t mode_val, uint8_t battery_val, uint16_t heading_val) {
  if (isAppConnectionReady) {
    if (battery_val != battery_val_last) {
      pBatteryCharacteristic->setValue(&battery_val, 1);
      pBatteryCharacteristic->notify();
    }
    battery_val_last = battery_val;

    if (mode_val != mode_val_last) {
      pModeCharacteristic->setValue(&mode_val, 1);
      pModeCharacteristic->notify();
    }
    mode_val_last = mode_val;

    if (shapeOnOFF != shapeOnOFF_last) {
      pShapeCharacteristic->setValue((uint8_t *)&shapeOnOFF, 1);
      pShapeCharacteristic->notify();
    }
    shapeOnOFF_last = shapeOnOFF;

    MetricsPacket metrics;
    metrics._heading = heading_val;

    if (memcmp(&metrics, &last_metrics, sizeof(MetricsPacket)) != 0) {
      pMetricsCharacteristic->setValue((uint8_t *)&metrics, sizeof(MetricsPacket));
      pMetricsCharacteristic->notify();
      last_metrics = metrics;  // Copy new values to previous
    }

    SettingsPacket settings;
    settings._isAppConnectionReady = isAppConnectionReady;
    settings._avoidOnOFF = avoidOnOFF;
    settings._FrontAvoidOnOFF = FrontAvoidOnOFF;
    settings._faceNorthOnOFF = faceNorthOnOFF;
    settings._screenOnOFF = displayOnOFF;
    settings._screenRed = displayRed;
    settings._screenGreen = displayGreen;
    settings._screenBlue = displayBlue;
    settings._speedLevel = 4U - speed_reduction;
    settings._proximityLockEnable = touchGesture;

    if (memcmp(&settings, &last_settings, sizeof(SettingsPacket)) != 0) {
      pSettingsCharacteristic->setValue((uint8_t *)&settings, sizeof(SettingsPacket));
      pSettingsCharacteristic->notify();
      last_settings = settings;  // Copy new values to previous
    }
  }
}

void BLE_Init(void) {
  BLEDevice::init(DEVICE_NAME);
  BLEServer *bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new MyServerCallbacks());

  BLEService *bleService = bleServer->createService(BLEUUID(SERVICE_UUID), 30, 0);
  BLEService *bleGeneralService = bleServer->createService(GENERAL_SERVICE_UUID);

  // Create BLE characteristics for motor speed and direction
  pJoystickCharacteristic = bleService->createCharacteristic(JOYSTICK_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pJoystickCharacteristic->addDescriptor(new BLE2902()); /*Add description to client configuration*/
  pJoystickCharacteristic->setCallbacks(new MyCallbacks());

  pMetricsCharacteristic = bleService->createCharacteristic(METRICS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pMetricsCharacteristic->addDescriptor(new BLE2902());

  pModeCharacteristic = bleService->createCharacteristic(MODE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pModeCharacteristic->addDescriptor(new BLE2902());

  pBatteryCharacteristic = bleGeneralService->createCharacteristic(BATTERY_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pBatteryCharacteristic->addDescriptor(new BLE2902()); /*Add description to client configuration*/

  pShapeCharacteristic = bleService->createCharacteristic(SHAPE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  pShapeCharacteristic->addDescriptor(new BLE2902()); /*Add description to client configuration*/
  pShapeCharacteristic->setCallbacks(new MyCallbacks());

  pSettingsCharacteristic = bleService->createCharacteristic(SETTINGS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  pSettingsCharacteristic->addDescriptor(new BLE2902()); /*Add description to client configuration*/
  pSettingsCharacteristic->setCallbacks(new MyCallbacks());

  pActionRxCharacteristic = bleService->createCharacteristic(ACTION_RX_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pActionRxCharacteristic->setCallbacks(new MyCallbacks());

  pBlockProgramCharacteristic = bleService->createCharacteristic(BLOCK_PROGRAM_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  pBlockProgramCharacteristic->addDescriptor(new BLE2902()); /*Add description to client configuration*/
  pBlockProgramCharacteristic->setCallbacks(new MyCallbacks());

  pStringCharacteristic = bleService->createCharacteristic(STRING_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pStringCharacteristic->addDescriptor(new BLE2902()); /*Add description to client configuration*/
  pStringCharacteristic->setCallbacks(new MyCallbacks());

  // Add OTA Service
  BLEOTA.begin(bleServer);
  BLEOTA.setCallbacks(new OTACallbacks());
  BLEOTA.setFWVersion(SW_VERSION);
  BLEOTA.setHWVersion(HW_VERSION);
  BLEOTA.setManufactuer(MANUFACTURER);

  BLEOTA.init();

  bleService->start();
  bleGeneralService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLEOTA.getBLEOTAuuid());
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->addServiceUUID(GENERAL_SERVICE_UUID);
  BLEDevice::startAdvertising();
  Serial.println(">> ProtoBot: Bluetooth Advertising..");
}
