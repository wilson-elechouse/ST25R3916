/*
  Example: ESP32_I2C_scan_14443AB_15693
  Bus: I2C
  Wiring: SDA=21, SCL=22, IRQ=4, LED=2 (optional)
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

constexpr int kPinSda = ST25R3916_DEFAULT_I2C_SDA_PIN;
constexpr int kPinScl = ST25R3916_DEFAULT_I2C_SCL_PIN;
constexpr int kPinIrq = ST25R3916_DEFAULT_IRQ_PIN;
constexpr int kPinLed = ST25R3916_DEFAULT_LED_PIN;
constexpr uint32_t kI2cClockHz = ST25R3916_DEFAULT_I2C_FREQUENCY;
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

  const uint8_t probe = probeI2cAddress();
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
