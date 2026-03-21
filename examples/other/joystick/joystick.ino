/*
  Example: ProtoBot Joystick BLE Controller
  Board: CodeCell C3 Light / CodeCell C3 / CodeCell C6 / CodeCell C6 Drive
         (or any ESP32 board with BLE support)

  Important:
  - This code must be flashed on the JOYSTICK controller device not on ProtoBot
  - ProtoBot should be running its standard firmware

  Overview:
  - Scans for a ProtoBot over BLE (by name or service UUID)
  - Connects automatically when ProtoBot is detected
  - Sends joystick X/Y data in real-time
  - Sends initial settings packet to enable control mode

  Features:
  - Fast scanning with filtering to avoid unrelated BLE devices
  - Verifies correct device after connection via GATT services
  - Automatically reconnects if connection is lost

  Joystick Mapping:
    - X and Y analog inputs mapped to:
        0   → full reverse / left
        100 → center (idle)
        200 → full forward / reverse / right

  BLE Communication:
    - Sends joystick data (2 bytes): [X, Y]
    - Sends settings packet (10 bytes) on connect:
        [0] isAppConnectionReady = 1
        [1] avoidOnOFF          = 0
        [2] FrontAvoidOnOFF     = 0
        [3] faceNorthOnOFF      = 0
        [4] displayOnOFF        = 0xFF
        [5] displayRed          = 0xFF
        [6] displayGreen        = 0xFF
        [7] displayBlue         = 0xFF
        [8] speed_reduction     = 3  (→ robot interprets as level 1 - full speed)
        [9] touchGesture        = 0

  Requirements:
    - ProtoBot powered ON and advertising
    - Joystick connected to ADC pins 2 and 3
    - Matching SERVICE_UUID / GENERAL_SERVICE_UUID with ProtoBot firmware
*/

#include <CodeCell.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

CodeCell myCodeCell;

#define SERVICE_UUID "12345678-1234-1234-1234-123456789012"
#define GENERAL_SERVICE_UUID "12345678-1234-1234-1234-123456789015"
#define JOYSTICK_UUID "abcd1234-abcd-1234-abcd-123456789012"
#define SETTINGS_UUID "dcba4330-dcba-4321-dcba-432123456789"

const int JOYSTICK_X_PIN = 2;
const int JOYSTICK_Y_PIN = 3;

const int JOYSTICK_X_CENTER = 3035;
const int JOYSTICK_Y_CENTER = 3055;

BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pJoystickChar = nullptr;
BLERemoteCharacteristic* pSettingsChar = nullptr;
BLEAdvertisedDevice* myDevice = nullptr;
BLEScan* pScan = nullptr;

bool doConnect = false;
bool connected = false;
bool scanRequested = false;

static BLEUUID protoServiceUUID(SERVICE_UUID);
static BLEUUID protoGeneralServiceUUID(GENERAL_SERVICE_UUID);
static BLEUUID joystickCharUUID(JOYSTICK_UUID);
static BLEUUID settingsCharUUID(SETTINGS_UUID);

uint8_t mapJoystick(int raw, int center) {
  if (raw >= center) {
    return (uint8_t)map(raw, center, 4095, 100, 200);
  } else {
    return (uint8_t)map(raw, 0, center, 0, 100);
  }
}

void clearCandidate() {
  if (myDevice) {
    delete myDevice;
    myDevice = nullptr;
  }
}

void clearConnectionState() {
  connected = false;
  doConnect = false;

  pJoystickChar = nullptr;
  pSettingsChar = nullptr;

  if (pClient) {
    if (pClient->isConnected()) {
      pClient->disconnect();
    }
    delete pClient;
    pClient = nullptr;
  }

  clearCandidate();
}

bool matchesProtoBotName(BLEAdvertisedDevice& advertisedDevice) {
  if (!advertisedDevice.haveName()) {
    return false;
  }

  String name = advertisedDevice.getName().c_str();
  name.toLowerCase();
  name.trim();

  if (name == "protobot" || name == "bot" || name == "robot") {
    return true;
  }

  return false;
}

bool matchesProtoBotUUID(BLEAdvertisedDevice& advertisedDevice) {
  if (!advertisedDevice.haveServiceUUID()) {
    return false;
  }

  if (advertisedDevice.isAdvertisingService(protoServiceUUID)) {
    return true;
  }

  if (advertisedDevice.isAdvertisingService(protoGeneralServiceUUID)) {
    return true;
  }

  return false;
}

bool isProtoBotAdvert(BLEAdvertisedDevice& advertisedDevice) {
  // Ignore very weak devices to reduce false attempts
  if (advertisedDevice.getRSSI() < -85) {
    return false;
  }

  // Option A: accept by device name first
  if (matchesProtoBotName(advertisedDevice)) {
    return true;
  }

  // Also allow direct UUID match if visible in advertisement
  if (matchesProtoBotUUID(advertisedDevice)) {
    return true;
  }

  return false;
}

class MyClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) override {
    Serial.println(">> BLE link connected");
  }

  void onDisconnect(BLEClient* pclient) override {
    Serial.println(">> BLE link disconnected");
    connected = false;
    pJoystickChar = nullptr;
    pSettingsChar = nullptr;
    scanRequested = true;
  }
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (connected || doConnect || myDevice != nullptr) {
      return;
    }

    if (!isProtoBotAdvert(advertisedDevice)) {
      Serial.println(">> Ignoring device (not a ProtoBot candidate)");
      return;
    }

    Serial.print(">> ProtoBot found: ");
    Serial.println(advertisedDevice.getAddress().toString().c_str());

    myDevice = new BLEAdvertisedDevice(advertisedDevice);
    doConnect = true;
    BLEDevice::getScan()->stop();
  }
};

bool sendInitialSettings() {
  if (!pSettingsChar) {
    return false;
  }

  uint8_t settings[10] = {
    1,     // isAppConnectionReady
    0,     // avoidOnOFF
    0,     // FrontAvoidOnOFF
    0,     // faceNorthOnOFF
    0xFF,  // displayOnOFF
    0xFF,  // displayRed
    0xFF,  // displayGreen
    0xFF,  // displayBlue
    3,     // speed_reduction = 1  => send 4 - 1 = 3
    0      // touchGesture
  };

  try {
    pSettingsChar->writeValue(settings, sizeof(settings), false);
    Serial.println(">> Sent full settings packet");
    return true;
  } catch (...) {
    Serial.println(">> Failed writing settings packet");
    return false;
  }
}

bool connectToServer() {
  if (!myDevice) {
    return false;
  }

  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallbacks());

  if (!pClient->connect(myDevice)) {
    Serial.println(">> Failed to connect");
    delete pClient;
    pClient = nullptr;
    return false;
  }

  Serial.println(">> Joystick Connected");

  BLERemoteService* service = nullptr;

  try {
    service = pClient->getService(protoServiceUUID);
  } catch (...) {
    service = nullptr;
  }

  if (!service) {
    Serial.println(">> SERVICE_UUID not found, trying GENERAL_SERVICE_UUID...");
    try {
      service = pClient->getService(protoGeneralServiceUUID);
    } catch (...) {
      service = nullptr;
    }
  }

  if (!service) {
    Serial.println(">> Target ProtoBot service not found on this device - Disconnecting");
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
    return false;
  }

  Serial.println(">> Target ProtoBot service FOUND!");

  try {
    pJoystickChar = service->getCharacteristic(joystickCharUUID);
  } catch (...) {
    pJoystickChar = nullptr;
  }

  if (!pJoystickChar) {
    Serial.println(">> Joystick characteristic missing, disconnecting.");
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
    return false;
  }

  try {
    pSettingsChar = service->getCharacteristic(settingsCharUUID);
  } catch (...) {
    pSettingsChar = nullptr;
  }

  if (!pSettingsChar) {
    Serial.println(">> Settings characteristic missing, disconnecting.");
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
    pJoystickChar = nullptr;
    return false;
  }

  if (!sendInitialSettings()) {
    Serial.println(">> Failed to send initial settings, disconnecting.");
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
    pJoystickChar = nullptr;
    pSettingsChar = nullptr;
    return false;
  }

  connected = true;
  Serial.println(">> ProtoBot GATT connected");
  return true;
}

void startScan() {
  if (pScan->isScanning()) {
    return;
  }

  clearCandidate();
  doConnect = false;

  Serial.println(">> Scanning for ProtoBot...");
  pScan->start(2, false);
}

void setup() {
  Serial.begin(115200);
  myCodeCell.Init(LIGHT);

  BLEDevice::init("Joystick-Controller");

  pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(80);

  startScan();
}

void loop() {
  if (myCodeCell.Run(10)) {
    if (connected) {
      if (!pClient || !pClient->isConnected()) {
        Serial.println(">> Lost connection to ProtoBot");
        clearConnectionState();
        scanRequested = true;
        return;
      }

      int rawX = myCodeCell.pinADC(JOYSTICK_X_PIN);
      int rawY = myCodeCell.pinADC(JOYSTICK_Y_PIN);

      uint8_t x = mapJoystick(rawX, JOYSTICK_X_CENTER);
      uint8_t y = mapJoystick(rawY, JOYSTICK_Y_CENTER);

      if ((x > 80) && (x < 120)) {
        x = 100;
      }
      if ((y > 80) && (y < 120)) {
        y = 100;
      }

      uint8_t buf[2] = { x, y };

      if (pJoystickChar) {
        try {
          pJoystickChar->writeValue(buf, 2, false);
          Serial.printf(">> Sent -> X=%d Y=%d (rawX=%d rawY=%d)\n", x, y, rawX, rawY);
        } catch (...) {
          Serial.println(">> Error: joystick write failed");
          clearConnectionState();
          scanRequested = true;
        }
      } else {
        Serial.println(">> Error: pJoystickChar is null");
        clearConnectionState();
        scanRequested = true;
      }

      return;
    }

    if (doConnect && !connected) {
      bool ok = connectToServer();

      if (ok) {
        Serial.println(">> Connected to ProtoBot");
      } else {
        Serial.println(">> Candidate failed validation, rescanning...");
        clearConnectionState();
        scanRequested = true;
      }

      doConnect = false;
      return;
    }

    if (!connected && !doConnect) {
      if (scanRequested || !pScan->isScanning()) {
        scanRequested = false;
        startScan();
      }
    }
  }
}