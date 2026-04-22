#include <Arduino.h>
#include <SPI.h>

#include <rfal_nfc.h>
#include <rfal_rfst25r3916.h>
#include <st25r3916_config.h>
#include <st_errno.h>

namespace {

constexpr int kPinMosi = ST25R3916_DEFAULT_SPI_MOSI_PIN;
constexpr int kPinMiso = ST25R3916_DEFAULT_SPI_MISO_PIN;
constexpr int kPinSck = ST25R3916_DEFAULT_SPI_SCK_PIN;
constexpr int kPinSs = ST25R3916_DEFAULT_SPI_SS_PIN;
constexpr int kPinIrq = ST25R3916_DEFAULT_IRQ_PIN;
constexpr int kPinLed = ST25R3916_DEFAULT_LED_PIN;

SPIClass gSpi(VSPI);
RfalRfST25R3916Class gReader(&gSpi, kPinSs, kPinIrq);
RfalNfcClass gNfc(&gReader);

void waitForSerial()
{
  const unsigned long start = millis();
  while (!Serial && ((millis() - start) < 2000UL)) {
    delay(10);
  }
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
  pinMode(kPinSs, OUTPUT);
  digitalWrite(kPinSs, HIGH);
  gSpi.begin(kPinSck, kPinMiso, kPinMosi, kPinSs);

  Serial.println("ESP32 ST25R3916 multi-protocol reader");
  Serial.println("Protocols: ISO14443A, ISO14443B, ISO15693");

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
