/*
  Example: ProtoBot Motion BLE Controller with CodeCell
  Board: CodeCell C6 / CodeCell C6 Drive

  Important:
  - This code must be flashed on the CONTROLLER device, not on ProtoBot
  - ProtoBot should be running its standard firmware

  Overview:
  - Uses CodeCell motion sensing for joystick inputs
  - Wakes when the onboard proximity sensor is triggered
  - On wake, the current Roll / Pitch / Yaw are stored as the new zero reference
  - That zero reference becomes joystick center:
      X = 100
      Y = 100
  - While active, tilting the controller sends joystick-style BLE commands to ProtoBot
  - When proximity is no longer detected, the controller sends center once and goes back to sleep

  Angle Handling:
  - Motion_RotationRead() returns angles in the range -180° to +180°
  - These are wrapped to 0° to 360°
  - Motion is then measured relative to the wake angle using shortest-angle delta
  - This avoids jumps when crossing 0° / 360°

  Output Mapping:
    - 100 = center / idle
    -   0 = full reverse / left
    - 200 = full forward / right

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
        [8] speed_reduction     = 2  (robot interprets this as medium speed level)
        [9] touchGesture        = 0

  Sleep Behavior:
  - If proximity rises above PROXIMITY_WAKE_THRESHOLD, the device wakes up (can be adjusted accordingly)
  - The waking-position becomes the new neutral joystick position
  - If proximity falls below PROXIMITY_SLEEP_THRESHOLD, the device sends center and sleeps again
*/

#include <Arduino.h>
#include <math.h>
#include <CodeCell.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

CodeCell myCodeCell;

#define SERVICE_UUID "12345678-1234-1234-1234-123456789012"
#define GENERAL_SERVICE_UUID "12345678-1234-1234-1234-123456789015"
#define JOYSTICK_UUID "abcd1234-abcd-1234-abcd-123456789012"
#define SETTINGS_UUID "dcba4330-dcba-4321-dcba-432123456789"

BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pJoystickChar = nullptr;
BLERemoteCharacteristic* pSettingsChar = nullptr;
BLEAdvertisedDevice* myDevice = nullptr;
BLEScan* pScan = nullptr;

bool doConnect = false;
bool connected = false;
bool scanRequested = false;
bool wokeFromProximity = false;
bool zeroCaptured = false;

static BLEUUID protoServiceUUID(SERVICE_UUID);
static BLEUUID protoGeneralServiceUUID(GENERAL_SERVICE_UUID);
static BLEUUID joystickCharUUID(JOYSTICK_UUID);
static BLEUUID settingsCharUUID(SETTINGS_UUID);

// Tune these for your setup
const uint16_t PROXIMITY_WAKE_THRESHOLD = 1000;
const uint16_t PROXIMITY_SLEEP_THRESHOLD = 300;

// Tilt range that maps to full joystick output
const float TILT_FULL_SCALE_DEG = 35.0f;

// Deadband around center to reduce jitter
const float TILT_DEADBAND_DEG = 3.0f;

// Current raw angles (-180 to +180 typically)
float rollRaw = 0.0f;
float pitchRaw = 0.0f;
float yawRaw = 0.0f;

// Wrapped angles (0 to 360)
float roll360 = 0.0f;
float pitch360 = 0.0f;
float yaw360 = 0.0f;

// Neutral angles captured on wake
float rollZero = 0.0f;
float pitchZero = 0.0f;
float yawZero = 0.0f;

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

float wrap360(float angleDeg) {
  return fmod(angleDeg + 360.0f, 360.0f);
}

// Returns shortest signed difference in degrees: -180 to +180
float shortestAngleDelta(float currentDeg, float referenceDeg) {
  float delta = currentDeg - referenceDeg;

  while (delta > 180.0f) {
    delta -= 360.0f;
  }
  while (delta < -180.0f) {
    delta += 360.0f;
  }

  return delta;
}

uint8_t mapTiltToJoystick(float deltaDeg) {
  if (deltaDeg > -TILT_DEADBAND_DEG && deltaDeg < TILT_DEADBAND_DEG) {
    return 100;
  }

  if (deltaDeg > TILT_FULL_SCALE_DEG) {
    deltaDeg = TILT_FULL_SCALE_DEG;
  }
  if (deltaDeg < -TILT_FULL_SCALE_DEG) {
    deltaDeg = -TILT_FULL_SCALE_DEG;
  }

  float normalized = deltaDeg / TILT_FULL_SCALE_DEG;  // -1.0 to +1.0
  int value = (int)(100.0f + normalized * 100.0f);    // 0 to 200

  if (value < 0) value = 0;
  if (value > 200) value = 200;

  return (uint8_t)value;
}

void readWrappedAngles() {
  myCodeCell.Motion_RotationRead(rollRaw, pitchRaw, yawRaw);

  roll360 = wrap360(rollRaw);
  pitch360 = wrap360(pitchRaw);
  yaw360 = wrap360(yawRaw);
}

void captureZeroReference() {
  readWrappedAngles();

  rollZero = roll360;
  pitchZero = pitch360;
  yawZero = yaw360;
  zeroCaptured = true;

  Serial.println(">> Motion zero captured");
  Serial.printf(">> Zero | Roll: %.2f  Pitch: %.2f  Yaw: %.2f\n", rollZero, pitchZero, yawZero);
}

bool matchesProtoBotName(BLEAdvertisedDevice& advertisedDevice) {
  if (!advertisedDevice.haveName()) {
    return false;
  }

  String name = advertisedDevice.getName().c_str();
  name.toLowerCase();
  name.trim();

  return (name == "protobot" || name == "bot" || name == "robot");
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
  if (advertisedDevice.getRSSI() < -85) {
    return false;
  }

  if (matchesProtoBotName(advertisedDevice)) {
    return true;
  }

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
    2,     // speed_reduction = medium speed level
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

void sendCenteredJoystick() {
  if (!connected || !pJoystickChar) {
    return;
  }

  uint8_t buf[2] = { 100, 100 };

  try {
    pJoystickChar->writeValue(buf, 2, false);
    Serial.println(">> Sent center joystick: X=100 Y=100");
  } catch (...) {
    Serial.println(">> Failed sending center joystick");
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

  Serial.println(">> Controller connected");

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
    Serial.println(">> Target ProtoBot service not found - disconnecting");
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
    return false;
  }

  Serial.println(">> Target ProtoBot service FOUND");

  try {
    pJoystickChar = service->getCharacteristic(joystickCharUUID);
  } catch (...) {
    pJoystickChar = nullptr;
  }

  if (!pJoystickChar) {
    Serial.println(">> Joystick characteristic missing - disconnecting");
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
    Serial.println(">> Settings characteristic missing - disconnecting");
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
    pJoystickChar = nullptr;
    return false;
  }

  if (!sendInitialSettings()) {
    Serial.println(">> Failed to send initial settings - disconnecting");
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

void goToSleep() {
  Serial.println(">> No proximity present - going to sleep");

  sendCenteredJoystick();
  delay(50);

  clearConnectionState();
  delay(50);

  myCodeCell.SleepProximityTrigger(PROXIMITY_WAKE_THRESHOLD);
}

void setup() {
  wokeFromProximity = myCodeCell.WakeUpCheck();

  Serial.begin(115200);

  myCodeCell.Init(LIGHT + MOTION_ROTATION);

  BLEDevice::init("Motion-Controller");

  pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(80);

  uint16_t proximityVal = myCodeCell.Light_ProximityRead();

  if (wokeFromProximity || proximityVal > PROXIMITY_WAKE_THRESHOLD) {
    Serial.println(">> Woke / activated by proximity");
    delay(150);
    captureZeroReference();
    startScan();
  } else {
    Serial.println(">> Waiting for proximity trigger");
    myCodeCell.SleepProximityTrigger(PROXIMITY_WAKE_THRESHOLD);
  }
}

void loop() {
  if (myCodeCell.Run(10)) {

    uint16_t proximityVal = myCodeCell.Light_ProximityRead();

    if (proximityVal < PROXIMITY_SLEEP_THRESHOLD) {
      goToSleep();
      return;
    }

    if (!zeroCaptured) {
      captureZeroReference();
    }

    if (connected) {
      if (!pClient || !pClient->isConnected()) {
        Serial.println(">> Lost connection to ProtoBot");
        clearConnectionState();
        scanRequested = true;
        return;
      }

      readWrappedAngles();

      float rollDelta = shortestAngleDelta(roll360, rollZero);
      float pitchDelta = shortestAngleDelta(pitch360, pitchZero);
      float yawDelta = shortestAngleDelta(yaw360, yawZero);

      uint8_t x = mapTiltToJoystick(pitchDelta);
      uint8_t y = mapTiltToJoystick(rollDelta);

      uint8_t buf[2] = { x, y };

      if (pJoystickChar) {
        try {
          pJoystickChar->writeValue(buf, 2, false);

          Serial.printf(">> Proximity=%u\n", proximityVal);
          Serial.printf(">> RAW   | Roll: %7.2f  Pitch: %7.2f  Yaw: %7.2f\n", rollRaw, pitchRaw, yawRaw);
          Serial.printf(">> WRAP  | Roll: %7.2f  Pitch: %7.2f  Yaw: %7.2f\n", roll360, pitch360, yaw360);
          Serial.printf(">> DELTA | Roll: %7.2f  Pitch: %7.2f  Yaw: %7.2f\n", rollDelta, pitchDelta, yawDelta);
          Serial.printf(">> Sent  | X=%d Y=%d\n\n", x, y);
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
        sendCenteredJoystick();
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