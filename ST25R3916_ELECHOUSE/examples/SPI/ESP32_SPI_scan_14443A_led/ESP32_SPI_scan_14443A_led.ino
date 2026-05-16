/*
  Example: ESP32_SPI_scan_14443A_led
  Bus: SPI
  Default wiring: ESP32 SCK=18/MISO=19/MOSI=23/SS=5; ESP32-S3 SCK=12/MISO=13/MOSI=11/SS=10; ESP32-C3 SCK=2/MISO=10/MOSI=3/SS=7; IRQ=4 (ESP32/S3) or GPIO6 (C3); LED=2 (ESP32/S3) or GPIO12 (C3, optional)
  Cards: ISO14443A
  Goal: Continuously poll for ISO14443A cards. Print UID when a card is detected,
        blink D2 while the card is present, and keep D2 off when no card is found.
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

constexpr unsigned long kCardLostAfterMs = 700UL;
constexpr unsigned long kLedBlinkIntervalMs = 150UL;
constexpr unsigned long kUidPrintIntervalMs = 1000UL;

SPIClass gSpi(kSpiBus);
RfalRfST25R3916Class gReader(&gSpi, kPinSs, kPinIrq);
RfalNfcClass gNfc(&gReader);

bool gCardPresent = false;
bool gLedOn = false;
unsigned long gLastSeenAt = 0;
unsigned long gLastBlinkAt = 0;
unsigned long gLastPrintAt = 0;
char gLastUid[40] = {0};

void waitForSerial()
{
  const unsigned long start = millis();
  while (!Serial && ((millis() - start) < 2000UL)) {
    delay(10);
  }
}

bool isNfcaDevice(const rfalNfcDevice *device)
{
  if (device == NULL) {
    return false;
  }

  return (device->type == RFAL_NFC_LISTEN_TYPE_NFCA) || (device->type == RFAL_NFC_POLL_TYPE_NFCA);
}

void formatId(const uint8_t *id, uint8_t idLen, char *out, size_t outLen)
{
  size_t written = 0;
  out[0] = '\0';

  for (uint8_t i = 0; i < idLen; i++) {
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

void setLed(bool on)
{
  gLedOn = on;
  digitalWrite(kPinLed, on ? HIGH : LOW);
}

void onNfcStateChange(rfalNfcState state)
{
  if (state != RFAL_NFC_STATE_ACTIVATED) {
    return;
  }

  rfalNfcDevice *device = NULL;
  if (gNfc.rfalNfcGetActiveDevice(&device) != ERR_NONE || (device == NULL)) {
    Serial.println("Failed to fetch activated device information");
    return;
  }

  if (!isNfcaDevice(device)) {
    Serial.print("Ignoring non-ISO14443A device type: ");
    Serial.println((int)device->type);
    gNfc.rfalNfcDeactivate(true);
    return;
  }

  char uid[sizeof(gLastUid)];
  formatId(device->nfcid, device->nfcidLen, uid, sizeof(uid));
  const unsigned long now = millis();
  const bool uidChanged = (strncmp(gLastUid, uid, sizeof(gLastUid)) != 0);

  if (!gCardPresent || uidChanged || ((now - gLastPrintAt) >= kUidPrintIntervalMs)) {
    Serial.print("ISO14443A UID: ");
    Serial.println(uid);
    gLastPrintAt = now;
  }

  strncpy(gLastUid, uid, sizeof(gLastUid) - 1U);
  gLastUid[sizeof(gLastUid) - 1U] = '\0';
  gCardPresent = true;
  gLastSeenAt = now;

  const ReturnCode err = gNfc.rfalNfcDeactivate(true);
  if (err != ERR_NONE) {
    Serial.print("Deactivate failed: ");
    Serial.println((int)err);
  }
}

void updateLed()
{
  const unsigned long now = millis();

  if (gCardPresent && ((now - gLastSeenAt) > kCardLostAfterMs)) {
    gCardPresent = false;
    gLastUid[0] = '\0';
    gLastPrintAt = 0;
    setLed(false);
    Serial.println("No ISO14443A card");
    return;
  }

  if (!gCardPresent) {
    setLed(false);
    return;
  }

  if ((now - gLastBlinkAt) >= kLedBlinkIntervalMs) {
    gLastBlinkAt = now;
    setLed(!gLedOn);
  }
}

} // namespace

void setup()
{
  Serial.begin(115200);
  waitForSerial();

  pinMode(kPinLed, OUTPUT);
  setLed(false);
  pinMode(kPinSs, OUTPUT);
  digitalWrite(kPinSs, HIGH);
  gSpi.begin(kPinSck, kPinMiso, kPinMosi, kPinSs);

  Serial.println("ESP32 SPI ISO14443A continuous scanner");
  Serial.print("Pins: SCK=");
  Serial.print(kPinSck);
  Serial.print(" MISO=");
  Serial.print(kPinMiso);
  Serial.print(" MOSI=");
  Serial.print(kPinMosi);
  Serial.print(" SS=");
  Serial.print(kPinSs);
  Serial.print(" IRQ=");
  Serial.print(kPinIrq);
  Serial.print(" D2/LED=");
  Serial.println(kPinLed);

  ReturnCode err = gNfc.rfalNfcInitialize();
  if (err != ERR_NONE) {
    Serial.print("rfalNfcInitialize failed: ");
    Serial.println((int)err);
    while (true) {
      setLed(false);
      delay(250);
    }
  }

  rfalNfcDiscoverParam discover;
  memset(&discover, 0, sizeof(discover));
  discover.compMode = RFAL_COMPLIANCE_MODE_NFC;
  discover.devLimit = RFAL_ESP32_DEFAULT_DEVICE_LIMIT;
  discover.techs2Find = RFAL_NFC_POLL_TECH_A;
  discover.totalDuration = RFAL_ESP32_DEFAULT_DISCOVERY_DURATION_MS;
  discover.notifyCb = onNfcStateChange;

  err = gNfc.rfalNfcDiscover(&discover);
  if (err != ERR_NONE) {
    Serial.print("rfalNfcDiscover failed: ");
    Serial.println((int)err);
    while (true) {
      setLed(false);
      delay(250);
    }
  }

  Serial.println("Polling ISO14443A cards...");
}

void loop()
{
  gNfc.rfalNfcWorker();
  updateLed();
}
