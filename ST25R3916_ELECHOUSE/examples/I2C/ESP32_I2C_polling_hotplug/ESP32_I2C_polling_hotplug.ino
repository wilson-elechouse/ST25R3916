/*
  Example: ESP32_I2C_polling_hotplug
  Bus: I2C
  Wiring: SDA=21, SCL=22, IRQ=4, LED=2 (optional)
  Cards: ISO14443A first-pass validated, ISO15693 can be enabled on the same loop
  Goal: Show insert/remove behaviour during continuous polling on ESP32 over I2C.
  Common failures: missing pull-ups, wrong SDA/SCL pins, noisy IRQ line, assuming 400kHz is stable too early.
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
constexpr unsigned long kCardAbsentAfterMs = 2000UL;

RfalRfST25R3916Class gReader(&Wire, kPinIrq);
RfalNfcClass gNfc(&gReader);

bool gCardPresent = false;
unsigned long gLastSeenAt = 0UL;
char gLastCardId[32] = {0};

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
    case RFAL_NFC_LISTEN_TYPE_NFCV:
    case RFAL_NFC_POLL_TYPE_NFCV:
      return "ISO15693";
    default:
      return "UNKNOWN";
  }
}

void formatId(const uint8_t *id, uint8_t idLen, char *out, size_t outLen)
{
  size_t written = 0U;
  out[0] = '\0';

  for (uint8_t i = 0U; i < idLen; i++) {
    written += (size_t)snprintf(out + written, (written < outLen) ? (outLen - written) : 0U, "%02X", id[i]);
    if ((i + 1U < idLen) && (written + 1U < outLen)) {
      out[written++] = ' ';
      out[written] = '\0';
    }
    if (written >= outLen) {
      out[outLen - 1U] = '\0';
      return;
    }
  }
}

void onNfcStateChange(rfalNfcState state)
{
  if (state != RFAL_NFC_STATE_ACTIVATED) {
    return;
  }

  rfalNfcDevice *device = NULL;
  if (gNfc.rfalNfcGetActiveDevice(&device) != ERR_NONE || (device == NULL)) {
    return;
  }

  char currentId[sizeof(gLastCardId)];
  formatId(device->nfcid, device->nfcidLen, currentId, sizeof(currentId));

  if (!gCardPresent || (strncmp(gLastCardId, currentId, sizeof(gLastCardId)) != 0)) {
    Serial.print("Card inserted: ");
    Serial.print(deviceTypeToString(device->type));
    Serial.print(" ");
    Serial.println(currentId);
  }

  strncpy(gLastCardId, currentId, sizeof(gLastCardId) - 1U);
  gLastCardId[sizeof(gLastCardId) - 1U] = '\0';
  gCardPresent = true;
  gLastSeenAt = millis();
  digitalWrite(kPinLed, HIGH);

  gNfc.rfalNfcDeactivate(true);
}

} // namespace

void setup()
{
  Serial.begin(115200);
  waitForSerial();

  pinMode(kPinLed, OUTPUT);
  digitalWrite(kPinLed, LOW);

  Serial.println("ESP32 I2C hotplug polling demo");
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
  discover.techs2Find = RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_V;
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

  if (gCardPresent && ((millis() - gLastSeenAt) > kCardAbsentAfterMs)) {
    gCardPresent = false;
    gLastCardId[0] = '\0';
    digitalWrite(kPinLed, LOW);
    Serial.println("Card removed");
  }
}
