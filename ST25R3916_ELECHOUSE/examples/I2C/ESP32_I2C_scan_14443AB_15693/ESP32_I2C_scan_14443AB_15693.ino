/*
  Example: ESP32_I2C_scan_14443AB_15693
  Bus: I2C
  Default wiring: ESP32 SDA=21/SCL=22; ESP32-S3 SDA=8/SCL=9; ESP32-C3 SDA=4/SCL=5; IRQ=4 (ESP32/S3) or GPIO6 (C3); LED=2 (ESP32/S3) or GPIO12 (C3, optional)
  Cards: ISO14443A, ISO14443B, ISO15693
  Goal: Multi-protocol discovery loop for the current ESP32 I2C baseline.
  Common failures: wrong SDA/SCL wiring, unstable IRQ handling, or poor antenna coupling.
*/

#include <Arduino.h>
#include <Wire.h>

#include <rfal_nfc.h>
#include <rfal_rfst25r3916.h>
#include <st25r3916_config.h>
#include <st_errno.h>

namespace {

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
// ESP32-S3 default I2C wiring. Edit these values to match your board.
constexpr int kPinSda = 8;
constexpr int kPinScl = 9;
constexpr int kPinIrq = 4;
constexpr int kPinLed = 2;
constexpr uint32_t kI2cClockHz = 100000UL;
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ARDUINO_ESP32C3_DEV)
// ESP32-C3 default I2C wiring. Edit these values to match your board.
constexpr int kPinSda = 4;
constexpr int kPinScl = 5;
constexpr int kPinIrq = 6;
constexpr int kPinLed = 12;
constexpr uint32_t kI2cClockHz = 100000UL;
#else
// Classic ESP32 default I2C wiring. Edit these values to match your board.
constexpr int kPinSda = 21;
constexpr int kPinScl = 22;
constexpr int kPinIrq = 4;
constexpr int kPinLed = 2;
constexpr uint32_t kI2cClockHz = 100000UL;
#endif
constexpr uint8_t kI2cAddress = 0x50U;

RfalRfST25R3916Class gReader(&Wire, kPinIrq);
RfalNfcClass gNfc(&gReader);

void waitForSerial()
{
  const unsigned long start = millis();
  while (!Serial && ((millis() - start) < 2000UL)) {
    delay(10);
  }
}

uint8_t probeI2cAddress()
{
  Wire.beginTransmission(kI2cAddress);
  return (uint8_t)Wire.endTransmission(true);
}
uint8_t waitForI2cAddress(unsigned long timeoutMs)
{
  const unsigned long start = millis();
  uint8_t status = 4U;
  do {
    status = probeI2cAddress();
    if (status == 0U) {
      return status;
    }
    delay(20);
  } while ((millis() - start) < timeoutMs);
  return status;
}

const char *deviceTypeToString(rfalNfcDevType type)
{
  switch (type) {
    case RFAL_NFC_LISTEN_TYPE_NFCA:
    case RFAL_NFC_POLL_TYPE_NFCA:
      return "ISO14443A";
    case RFAL_NFC_LISTEN_TYPE_NFCB:
    case RFAL_NFC_POLL_TYPE_NFCB:
      return "ISO14443B";
    case RFAL_NFC_LISTEN_TYPE_NFCV:
    case RFAL_NFC_POLL_TYPE_NFCV:
      return "ISO15693";
    default:
      return "UNKNOWN";
  }
}

void printId(const uint8_t *id, uint8_t idLen)
{
  for (uint8_t i = 0; i < idLen; i++) {
    if (id[i] < 0x10U) {
      Serial.print('0');
    }
    Serial.print(id[i], HEX);
    if (i + 1U < idLen) {
      Serial.print(' ');
    }
  }
  Serial.println();
}

void onNfcStateChange(rfalNfcState state)
{
  if (state == RFAL_NFC_STATE_START_DISCOVERY) {
    digitalWrite(kPinLed, LOW);
    return;
  }

  if (state != RFAL_NFC_STATE_ACTIVATED) {
    return;
  }

  rfalNfcDevice *device = NULL;
  if (gNfc.rfalNfcGetActiveDevice(&device) != ERR_NONE || (device == NULL)) {
    Serial.println("Failed to fetch activated device information");
    return;
  }

  digitalWrite(kPinLed, HIGH);
  Serial.print(deviceTypeToString(device->type));
  Serial.print(" ID: ");
  printId(device->nfcid, device->nfcidLen);

  const ReturnCode err = gNfc.rfalNfcDeactivate(true);
  if (err != ERR_NONE) {
    Serial.print("Deactivate failed: ");
    Serial.println((int)err);
  }
}

} // namespace

void setup()
{
  Serial.begin(115200);
  waitForSerial();

  pinMode(kPinLed, OUTPUT);
  digitalWrite(kPinLed, LOW);

  Serial.println("ESP32 I2C multi-protocol scanner");
  Serial.print("SDA=");
  Serial.print(kPinSda);
  Serial.print(" SCL=");
  Serial.print(kPinScl);
  Serial.print(" IRQ=");
  Serial.print(kPinIrq);
  Serial.print(" CLK=");
  Serial.println(kI2cClockHz);

  Wire.begin(kPinSda, kPinScl, kI2cClockHz);
  delay(20);

  const uint8_t probe = waitForI2cAddress(1500UL);
  Serial.print("I2C ACK probe (0x50): ");
  Serial.println((int)probe);
  if (probe != 0U) {
    Serial.println("No ACK from ST25R3916 I2C address.");
    while (true) {
      delay(250);
    }
  }

  ReturnCode err = gNfc.rfalNfcInitialize();
  if (err != ERR_NONE) {
    Serial.print("rfalNfcInitialize failed: ");
    Serial.println((int)err);
    while (true) {
      delay(250);
    }
  }

  rfalNfcDiscoverParam discover;
  memset(&discover, 0, sizeof(discover));
  discover.compMode = RFAL_COMPLIANCE_MODE_NFC;
  discover.devLimit = RFAL_ESP32_DEFAULT_DEVICE_LIMIT;
  discover.techs2Find = RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_B | RFAL_NFC_POLL_TECH_V;
  discover.totalDuration = RFAL_ESP32_DEFAULT_DISCOVERY_DURATION_MS;
  discover.notifyCb = onNfcStateChange;

  err = gNfc.rfalNfcDiscover(&discover);
  if (err != ERR_NONE) {
    Serial.print("rfalNfcDiscover failed: ");
    Serial.println((int)err);
    while (true) {
      delay(250);
    }
  }
}

void loop()
{
  gNfc.rfalNfcWorker();
}
