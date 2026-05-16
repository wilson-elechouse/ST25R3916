/*
  Example: ESP32_SPI_scan_14443AB_15693
  Bus: SPI
  Default wiring: ESP32 SCK=18/MISO=19/MOSI=23/SS=5; ESP32-S3 SCK=12/MISO=13/MOSI=11/SS=10; ESP32-C3 SCK=2/MISO=10/MOSI=3/SS=7; IRQ=4 (ESP32/S3) or GPIO6 (C3); LED=2 (ESP32/S3) or GPIO12 (C3, optional)
  Cards: ISO14443A, ISO14443B, ISO15693
  Goal: Multi-protocol discovery loop for the current ESP32 SPI baseline.
  Common failures: Wrong SPI bus/board FQBN, wrong wiring, poor antenna coupling.
*/

#include <Arduino.h>
#include <SPI.h>

#include <rfal_nfc.h>
#include <rfal_rfst25r3916.h>
#include <st25r3916_config.h>
#include <st_errno.h>

namespace {

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
// ESP32-S3-N16R8 default SPI wiring. Edit these values to match your board.
constexpr uint8_t kSpiBus = FSPI;
constexpr int kPinSck = 12;
constexpr int kPinMiso = 13;
constexpr int kPinMosi = 11;
constexpr int kPinSs = 10;
constexpr int kPinIrq = 4;
constexpr int kPinLed = 2;
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ARDUINO_ESP32C3_DEV)
// ESP32-C3 default SPI wiring. Edit these values to match your board.
constexpr uint8_t kSpiBus = FSPI;
constexpr int kPinSck = 2;
constexpr int kPinMiso = 10;
constexpr int kPinMosi = 3;
constexpr int kPinSs = 7;
constexpr int kPinIrq = 6;
constexpr int kPinLed = 12;
#else
// Classic ESP32 default SPI wiring. Edit these values to match your board.
constexpr uint8_t kSpiBus = VSPI;
constexpr int kPinSck = 18;
constexpr int kPinMiso = 19;
constexpr int kPinMosi = 23;
constexpr int kPinSs = 5;
constexpr int kPinIrq = 4;
constexpr int kPinLed = 2;
#endif

SPIClass gSpi(kSpiBus);
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

  Serial.println("ESP32 SPI multi-protocol scanner");

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
