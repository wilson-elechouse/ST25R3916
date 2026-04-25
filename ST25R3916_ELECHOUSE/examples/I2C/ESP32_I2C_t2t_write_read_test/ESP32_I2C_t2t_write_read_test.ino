/*
  Example: ESP32_I2C_t2t_write_read_test
  Bus: I2C
  Wiring: SDA=21, SCL=22, IRQ=4, LED=2 (optional)
  Goal: For a NFCA T2T tag, back up one page, write test bytes, verify, then restore.
  Notes:
    - Type 2 Tag WRITE affects one 4-byte page.
    - This sketch avoids pages 0-3 and defaults to page 8.
    - Do not remove power or the card during the restore phase.
*/

#include <Arduino.h>
#include <Wire.h>

#include <rfal_nfc.h>
#include <rfal_nfca.h>
#include <rfal_rfst25r3916.h>
#include <rfal_t2t.h>
#include <st25r3916_config.h>
#include <st_errno.h>

namespace {

constexpr int kPinSda = ST25R3916_DEFAULT_I2C_SDA_PIN;
constexpr int kPinScl = ST25R3916_DEFAULT_I2C_SCL_PIN;
constexpr int kPinIrq = ST25R3916_DEFAULT_IRQ_PIN;
constexpr int kPinLed = ST25R3916_DEFAULT_LED_PIN;
constexpr uint32_t kI2cClockHz = ST25R3916_DEFAULT_I2C_FREQUENCY;
constexpr uint8_t kI2cAddress = 0x50U;

constexpr uint8_t kTestPage = 8U;
constexpr uint8_t kDumpStartPage = 4U;
constexpr uint8_t kPagesToDump = 12U;
constexpr uint8_t kWritePattern[RFAL_T2T_WRITE_DATA_LEN] = { 'S', 'T', '2', 'T' };

RfalRfST25R3916Class gReader(&Wire, kPinIrq);
RfalNfcClass gNfc(&gReader);

bool gTestPending = false;
bool gTestDone = false;

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

const char *returnCodeToString(ReturnCode code)
{
  switch (code) {
    case ERR_NONE:
      return "ERR_NONE";
    case ERR_REQUEST:
      return "ERR_REQUEST";
    case ERR_PARAM:
      return "ERR_PARAM";
    case ERR_PROTO:
      return "ERR_PROTO";
    case ERR_TIMEOUT:
      return "ERR_TIMEOUT";
    case ERR_WRONG_STATE:
      return "ERR_WRONG_STATE";
    case ERR_NOTSUPP:
      return "ERR_NOTSUPP";
    case ERR_IO:
      return "ERR_IO";
    default:
      return "ERR_OTHER";
  }
}

void printBytes(const uint8_t *buf, size_t len)
{
  for (size_t i = 0; i < len; i++) {
    if (buf[i] < 0x10U) {
      Serial.print('0');
    }
    Serial.print(buf[i], HEX);
    if (i + 1U < len) {
      Serial.print(' ');
    }
  }
  Serial.println();
}

void printPage(uint8_t page, const uint8_t *data)
{
  Serial.print("Page ");
  if (page < 10U) {
    Serial.print('0');
  }
  Serial.print(page);
  Serial.print(": ");
  printBytes(data, RFAL_T2T_BLOCK_LEN);
}

bool isT2TDevice(const rfalNfcDevice *device)
{
  if (device == NULL) {
    return false;
  }

  return (((device->type == RFAL_NFC_LISTEN_TYPE_NFCA) || (device->type == RFAL_NFC_POLL_TYPE_NFCA)) &&
          (device->dev.nfca.type == RFAL_NFCA_T2T));
}

bool nfcaHasProprietarySakBits(const rfalNfcDevice *device)
{
  if (device == NULL) {
    return false;
  }

  if ((device->type != RFAL_NFC_LISTEN_TYPE_NFCA) && (device->type != RFAL_NFC_POLL_TYPE_NFCA)) {
    return false;
  }

  return (device->dev.nfca.selRes.sak & (uint8_t)~RFAL_NFCA_SEL_RES_CONF_MASK) != 0U;
}

void printNfcaActivationDetails(const rfalNfcDevice *device)
{
  if (device == NULL) {
    return;
  }

  if ((device->type != RFAL_NFC_LISTEN_TYPE_NFCA) && (device->type != RFAL_NFC_POLL_TYPE_NFCA)) {
    return;
  }

  Serial.print("ATQA: ");
  if (device->dev.nfca.sensRes.anticollisionInfo < 0x10U) {
    Serial.print('0');
  }
  Serial.print(device->dev.nfca.sensRes.anticollisionInfo, HEX);
  Serial.print(' ');
  if (device->dev.nfca.sensRes.platformInfo < 0x10U) {
    Serial.print('0');
  }
  Serial.println(device->dev.nfca.sensRes.platformInfo, HEX);

  Serial.print("SAK: ");
  if (device->dev.nfca.selRes.sak < 0x10U) {
    Serial.print('0');
  }
  Serial.println(device->dev.nfca.selRes.sak, HEX);
}

void onNfcStateChange(rfalNfcState state)
{
  if ((state == RFAL_NFC_STATE_ACTIVATED) && !gTestDone) {
    gTestPending = true;
    digitalWrite(kPinLed, HIGH);
  }
}

ReturnCode readPageWindow(uint8_t startPage, uint8_t *data)
{
  uint16_t rcvdLen = 0;
  for (uint8_t attempt = 0; attempt < 2U; attempt++) {
    ReturnCode err = gNfc.rfalT2TPollerRead(startPage, data, RFAL_T2T_READ_DATA_LEN, &rcvdLen);
    if ((err == ERR_NONE) && (rcvdLen == RFAL_T2T_READ_DATA_LEN)) {
      return ERR_NONE;
    }

    if ((attempt + 1U) < 2U) {
      delay(40);
    } else if ((err == ERR_NONE) && (rcvdLen != RFAL_T2T_READ_DATA_LEN)) {
      return ERR_PROTO;
    } else {
      return err;
    }
  }

  return ERR_TIMEOUT;
}

ReturnCode writePageWithRetry(uint8_t page, const uint8_t *data)
{
  ReturnCode lastErr = ERR_TIMEOUT;

  for (uint8_t attempt = 0; attempt < 2U; attempt++) {
    lastErr = gNfc.rfalT2TPollerWrite(page, data);
    if (lastErr == ERR_NONE) {
      return ERR_NONE;
    }

    if ((attempt + 1U) < 2U) {
      delay(40);
    }
  }

  return lastErr;
}

void dumpFirstPages(void)
{
  Serial.println("T2T page dump:");

  for (uint8_t startPage = kDumpStartPage; startPage < (uint8_t)(kDumpStartPage + kPagesToDump); startPage = (uint8_t)(startPage + 4U)) {
    uint8_t window[RFAL_T2T_READ_DATA_LEN];
    const ReturnCode err = readPageWindow(startPage, window);

    Serial.print("Read from page ");
    Serial.print(startPage);
    Serial.print(": ");
    Serial.print(returnCodeToString(err));
    Serial.print(" (");
    Serial.print((int)err);
    Serial.println(")");

    if (err != ERR_NONE) {
      return;
    }

    for (uint8_t offset = 0; offset < 4U; offset++) {
      printPage((uint8_t)(startPage + offset), &window[offset * RFAL_T2T_BLOCK_LEN]);
    }
  }
}

void runT2TWriteReadTest(rfalNfcDevice *device)
{
  (void)device;

  uint8_t pageWindow[RFAL_T2T_READ_DATA_LEN];
  uint8_t originalPage[RFAL_T2T_BLOCK_LEN];
  uint8_t verifyWindow[RFAL_T2T_READ_DATA_LEN];
  bool backupReady = false;
  bool writeSucceeded = false;

  printNfcaActivationDetails(device);
  if (nfcaHasProprietarySakBits(device)) {
    Serial.println("Refusing raw T2T write/read test on this card.");
    Serial.println("Reason: SAK contains proprietary bits, so this is not a plain NFC Forum Type 2 tag.");
    Serial.println("Reason: cards such as MIFARE Classic need a card-specific command/auth flow.");
    return;
  }

  delay(60);
  dumpFirstPages();

  ReturnCode err = readPageWindow(kTestPage, pageWindow);
  Serial.print("Backup read at page ");
  Serial.print(kTestPage);
  Serial.print(": ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");

  if (err != ERR_NONE) {
    return;
  }

  memcpy(originalPage, pageWindow, sizeof(originalPage));
  backupReady = true;

  Serial.print("Original page ");
  Serial.print(kTestPage);
  Serial.print(": ");
  printBytes(originalPage, sizeof(originalPage));

  Serial.print("Writing test pattern: ");
  printBytes(kWritePattern, sizeof(kWritePattern));

  err = writePageWithRetry(kTestPage, kWritePattern);
  Serial.print("Write result: ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");

  if (err == ERR_NONE) {
    writeSucceeded = true;
  } else {
    Serial.println("Write failed before verification. Restore will still be attempted from backup.");
  }

  err = readPageWindow(kTestPage, verifyWindow);
  Serial.print("Verify read: ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");

  if (err == ERR_NONE) {
    Serial.print("Page after write: ");
    printBytes(verifyWindow, RFAL_T2T_BLOCK_LEN);

    if (memcmp(verifyWindow, kWritePattern, RFAL_T2T_BLOCK_LEN) == 0) {
      Serial.println("Verification: test pattern matched.");
    } else {
      Serial.println("Verification: mismatch, page does not contain the expected pattern.");
    }
  }

  if (backupReady) {
    err = writePageWithRetry(kTestPage, originalPage);
    Serial.print("Restore write: ");
    Serial.print(returnCodeToString(err));
    Serial.print(" (");
    Serial.print((int)err);
    Serial.println(")");

    if (err == ERR_NONE) {
      err = readPageWindow(kTestPage, verifyWindow);
      Serial.print("Restore verify read: ");
      Serial.print(returnCodeToString(err));
      Serial.print(" (");
      Serial.print((int)err);
      Serial.println(")");

      if (err == ERR_NONE) {
        Serial.print("Page after restore: ");
        printBytes(verifyWindow, RFAL_T2T_BLOCK_LEN);

        if (memcmp(verifyWindow, originalPage, RFAL_T2T_BLOCK_LEN) == 0) {
          Serial.println("Restore verification: original data restored.");
        } else {
          Serial.println("Restore verification: restore mismatch.");
        }
      }
    }
  }

  if (writeSucceeded) {
    Serial.println("T2T write/read/restore sequence completed.");
  }
}

} // namespace

void setup()
{
  Serial.begin(115200);
  waitForSerial();

  pinMode(kPinLed, OUTPUT);
  digitalWrite(kPinLed, LOW);
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

  Serial.println("ESP32 I2C T2T write/read test");
  Serial.print("Configured test page: ");
  Serial.println(kTestPage);
  Serial.println("Waiting for one NFCA T2T card...");

  ReturnCode err = gNfc.rfalNfcInitialize();
  if (err != ERR_NONE) {
    Serial.print("rfalNfcInitialize failed: ");
    Serial.print(returnCodeToString(err));
    Serial.print(" (");
    Serial.print((int)err);
    Serial.println(")");
    while (true) {
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
    Serial.print(returnCodeToString(err));
    Serial.print(" (");
    Serial.print((int)err);
    Serial.println(")");
    while (true) {
      delay(250);
    }
  }
}

void loop()
{
  if (!gTestDone) {
    gNfc.rfalNfcWorker();
  }

  if (gTestPending && !gTestDone) {
    rfalNfcDevice *device = NULL;
    gTestPending = false;

    if ((gNfc.rfalNfcGetActiveDevice(&device) == ERR_NONE) && isT2TDevice(device)) {
      runT2TWriteReadTest(device);
    } else {
      Serial.println("Active device is not a NFCA T2T tag. Test aborted.");
    }

    gTestDone = true;
    digitalWrite(kPinLed, LOW);

    const ReturnCode err = gNfc.rfalNfcDeactivate(false);
    Serial.print("Deactivate to idle: ");
    Serial.print(returnCodeToString(err));
    Serial.print(" (");
    Serial.print((int)err);
    Serial.println(")");
    Serial.println("Reader is now idle. Reset the board to run the test again.");
  }
}
