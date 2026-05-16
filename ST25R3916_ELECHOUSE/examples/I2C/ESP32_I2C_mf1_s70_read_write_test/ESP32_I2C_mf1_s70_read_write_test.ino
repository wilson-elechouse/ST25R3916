/*
  Example: ESP32_I2C_mf1_s70_read_write_test
  Bus: I2C
  Default wiring: ESP32 SDA=21/SCL=22; ESP32-S3 SDA=8/SCL=9; ESP32-C3 SDA=4/SCL=5; IRQ=4 (ESP32/S3) or GPIO6 (C3); LED=2 (ESP32/S3) or GPIO12 (C3, optional)
  Target card: MIFARE One S70 / MIFARE Classic 4K

  Test flow:
    1. Probe the ST25R3916 I2C address
    2. Detect a likely S70 card by ATQA/SAK
    3. Authenticate to block 4 with default Key A = FF FF FF FF FF FF
    4. Read block 4 and save a backup
    5. Stop without writing when kEnableWriteTest=false
    6. If explicitly enabled, write a 16-byte test pattern
    7. Read block 4 again and verify
    8. Restore the original block contents and verify restore

  Safety:
    - Uses block 4, which is a normal data block in sector 1
    - Does not touch block 0 or a sector trailer
    - Write mode is disabled by default; set kEnableWriteTest=true below only on a sacrificial card/block
*/

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include <rfal_mf1.h>
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
// MF1 write needs a fast I2C bus so the data frame follows the card ACK quickly.
constexpr uint32_t kI2cClockHz = 400000UL;
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ARDUINO_ESP32C3_DEV)
// ESP32-C3 default I2C wiring. Edit these values to match your board.
constexpr int kPinSda = 4;
constexpr int kPinScl = 5;
constexpr int kPinIrq = 6;
constexpr int kPinLed = 12;
// Keep C3 conservative by default; raise to 400000UL only after your I2C wiring/module is validated at Fast-mode.
constexpr uint32_t kI2cClockHz = 100000UL;
#else
// Classic ESP32 default I2C wiring. Edit these values to match your board.
constexpr int kPinSda = 21;
constexpr int kPinScl = 22;
constexpr int kPinIrq = 4;
constexpr int kPinLed = 2;
// MF1 write needs a fast I2C bus so the data frame follows the card ACK quickly.
constexpr uint32_t kI2cClockHz = 400000UL;
#endif
constexpr uint8_t kI2cAddress = 0x50U;
constexpr bool kEnableWriteTest = false;

constexpr uint8_t kTestBlock = 4U;
constexpr uint8_t kDefaultKeyA[RFAL_MF1_KEY_LEN] = {
  0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU
};
constexpr uint8_t kBaselineBlock4[RFAL_MF1_BLOCK_LEN] = {
  0x53U, 0x54U, 0x32U, 0x35U, 0x52U, 0x33U, 0x39U, 0x31U,
  0x36U, 0x20U, 0x44U, 0x45U, 0x4DU, 0x4FU, 0x30U, 0x31U
};
constexpr uint8_t kPreferredTestBlock4[RFAL_MF1_BLOCK_LEN] = {
  0x49U, 0x32U, 0x43U, 0x20U, 0x53U, 0x37U, 0x30U, 0x20U,
  0x54U, 0x45U, 0x53U, 0x54U, 0x20U, 0x30U, 0x30U, 0x31U
};

RfalRfST25R3916Class gReader(&Wire, kPinIrq);
RfalNfcClass gNfc(&gReader);
RfalMf1Class gMf1(&gReader);
rfalNfcDiscoverParam gDiscoverParams = {};

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
    case ERR_INCOMPLETE_BYTE:
      return "ERR_INCOMPLETE_BYTE";
    case ERR_FRAMING:
      return "ERR_FRAMING";
    case ERR_PAR:
      return "ERR_PAR";
    case ERR_CRC:
      return "ERR_CRC";
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

void formatId(const uint8_t *id, uint8_t idLen, char *out, size_t outLen)
{
  size_t written = 0U;
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

void printReturnCode(const char *label, ReturnCode err)
{
  Serial.print(label);
  Serial.print(": ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");
}

void printNonce(uint32_t cardNonce)
{
  uint8_t nonceBytes[4] = {
    (uint8_t)(cardNonce >> 24),
    (uint8_t)(cardNonce >> 16),
    (uint8_t)(cardNonce >> 8),
    (uint8_t)cardNonce
  };

  Serial.print("Card challenge Nt: ");
  printBytes(nonceBytes, sizeof(nonceBytes));
}

void onNfcStateChange(rfalNfcState state)
{
  if ((state == RFAL_NFC_STATE_ACTIVATED) && !gTestDone) {
    gTestPending = true;
    digitalWrite(kPinLed, HIGH);
  }
}

void printCardSummary(const rfalNfcDevice *device)
{
  char id[40];
  const uint8_t atqa[2] = {
    device->dev.nfca.sensRes.anticollisionInfo,
    device->dev.nfca.sensRes.platformInfo
  };

  formatId(device->nfcid, device->nfcidLen, id, sizeof(id));

  Serial.println();
  Serial.println("=== MF1 S70 Read/Write Test (I2C) ===");
  Serial.print("UID: ");
  Serial.println(id);
  Serial.print("ATQA: ");
  printBytes(atqa, sizeof(atqa));
  Serial.print("SAK: ");
  if (device->dev.nfca.selRes.sak < 0x10U) {
    Serial.print('0');
  }
  Serial.println(device->dev.nfca.selRes.sak, HEX);
  Serial.print("Target block: ");
  Serial.println(kTestBlock);
  Serial.print("Default Key A: ");
  printBytes(kDefaultKeyA, sizeof(kDefaultKeyA));
}

bool reacquireCard(uint32_t expectedUid, rfalNfcDevice **device)
{
  ReturnCode err;
  const unsigned long deadline = millis() + 1500UL;

  if (device == NULL) {
    return false;
  }
  *device = NULL;

  if (gNfc.rfalNfcGetState() > RFAL_NFC_STATE_IDLE) {
    err = gNfc.rfalNfcDeactivate(false);
    if (err != ERR_NONE) {
      printReturnCode("Deactivate before re-discover", err);
      return false;
    }
    delay(10);
  }

  err = gNfc.rfalNfcDiscover(&gDiscoverParams);
  if (err != ERR_NONE) {
    printReturnCode("Re-discover", err);
    return false;
  }

  while (millis() < deadline) {
    rfalNfcDevice *activeDevice = NULL;
    uint32_t activeUid = 0U;

    gNfc.rfalNfcWorker();
    if (gNfc.rfalNfcGetState() != RFAL_NFC_STATE_ACTIVATED) {
      delay(1);
      continue;
    }

    err = gNfc.rfalNfcGetActiveDevice(&activeDevice);
    if ((err != ERR_NONE) || !RfalMf1Class::isLikelyClassic4K(activeDevice)) {
      delay(1);
      continue;
    }

    if (!RfalMf1Class::getUid32(activeDevice, &activeUid) || (activeUid != expectedUid)) {
      Serial.println("Re-discovered card UID does not match the expected S70 card.");
      return false;
    }

    delay(60);
    *device = activeDevice;
    return true;
  }

  Serial.println("Timed out while re-activating the S70 card.");
  return false;
}

bool authenticateBlock(rfalNfcDevice *device, uint8_t blockNo, rfalMf1CryptoState *crypto)
{
  uint32_t cardNonce = 0U;
  const ReturnCode authErr = gMf1.authenticate(crypto, device, blockNo, kDefaultKeyA, RFAL_MF1_AUTH_KEY_A, &cardNonce);
  printReturnCode("AUTH result", authErr);
  if (authErr != ERR_NONE) {
    return false;
  }

  printNonce(cardNonce);
  return true;
}

bool readBlockAndLog(uint32_t expectedUid, uint8_t blockNo, const char *label, uint8_t *blockData)
{
  rfalNfcDevice *device = NULL;
  rfalMf1CryptoState crypto = {};

  if (!reacquireCard(expectedUid, &device)) {
    return false;
  }

  if (!authenticateBlock(device, blockNo, &crypto)) {
    return false;
  }

  const ReturnCode err = gMf1.readBlock(&crypto, blockNo, blockData);
  printReturnCode("READ result", err);
  if (err != ERR_NONE) {
    return false;
  }

  Serial.print(label);
  Serial.print(": ");
  printBytes(blockData, RFAL_MF1_BLOCK_LEN);
  return true;
}

bool writeBlockAndLog(uint32_t expectedUid,
                      uint8_t blockNo,
                      const uint8_t *blockData,
                      const char *label,
                      bool printPayloadBeforeWrite)
{
  if ((label != NULL) && printPayloadBeforeWrite) {
    Serial.print(label);
    Serial.print(": ");
    printBytes(blockData, RFAL_MF1_BLOCK_LEN);
  }

  rfalNfcDevice *device = NULL;
  rfalMf1CryptoState crypto = {};

  if (!reacquireCard(expectedUid, &device)) {
    return false;
  }

  if (!authenticateBlock(device, blockNo, &crypto)) {
    return false;
  }

  const ReturnCode err = gMf1.writeBlock(&crypto, blockNo, blockData);
  printReturnCode("WRITE result", err);
  if (err == ERR_NONE) {
    return true;
  }

  Serial.println("Write status was not ERR_NONE; re-reading block to check whether the card accepted it.");
  delay(20);

  uint8_t verifyBlock[RFAL_MF1_BLOCK_LEN] = {0};
  if (!readBlockAndLog(expectedUid, blockNo, "Block after write status check", verifyBlock)) {
    return false;
  }

  if (memcmp(verifyBlock, blockData, sizeof(verifyBlock)) != 0) {
    Serial.println("Write status check mismatch.");
    return false;
  }

  Serial.println("Write status check matched despite the non-ERR_NONE write status.");
  return true;
}

void selectTestPattern(const uint8_t *originalBlock, uint8_t *testPattern)
{
  memcpy(testPattern, kPreferredTestBlock4, RFAL_MF1_BLOCK_LEN);
  if (memcmp(originalBlock, testPattern, RFAL_MF1_BLOCK_LEN) == 0) {
    RfalMf1Class::makeTestPattern(originalBlock, testPattern, RFAL_MF1_BLOCK_LEN);
  }
}

void runMf1ReadWriteTest(rfalNfcDevice *device)
{
  uint8_t originalBlock[RFAL_MF1_BLOCK_LEN] = {0};
  uint8_t testPattern[RFAL_MF1_BLOCK_LEN] = {0};
  uint8_t verifyBlock[RFAL_MF1_BLOCK_LEN] = {0};
  uint8_t restoreVerifyBlock[RFAL_MF1_BLOCK_LEN] = {0};
  uint32_t expectedUid = 0U;

  printCardSummary(device);

  if (!RfalMf1Class::isLikelyClassic4K(device)) {
    Serial.println("Card is not the expected MIFARE Classic 4K / S70 profile. Test aborted.");
    return;
  }

  if (!RfalMf1Class::getUid32(device, &expectedUid)) {
    Serial.println("UID format is not supported by this S70 example. Test aborted.");
    return;
  }

  if (!readBlockAndLog(expectedUid, kTestBlock, "Original block 4", originalBlock)) {
    Serial.println("Initial block read failed. Test aborted.");
    return;
  }

  if (memcmp(originalBlock, kBaselineBlock4, sizeof(kBaselineBlock4)) == 0) {
    Serial.println("Block already contains the built-in baseline pattern.");
  }

  if (memcmp(originalBlock, kPreferredTestBlock4, sizeof(kPreferredTestBlock4)) == 0) {
    Serial.println("Block already contains the preferred test pattern. Using an alternate pattern for this run.");
  }

  if (!kEnableWriteTest) {
    Serial.println("Write test is disabled by kEnableWriteTest=false. Set it to true in this sketch to write block 4.");
    Serial.println("The read/auth path is complete; no card data was changed in this run.");
    return;
  }

  selectTestPattern(originalBlock, testPattern);
  if (memcmp(originalBlock, testPattern, sizeof(testPattern)) == 0) {
    Serial.println("Selected test pattern matches the current block. Test aborted to avoid a no-op write.");
    return;
  }

  if (!writeBlockAndLog(expectedUid, kTestBlock, testPattern, "Writing test pattern", true)) {
    Serial.println("Test-pattern write failed. Restore not required.");
    return;
  }

  if (!readBlockAndLog(expectedUid, kTestBlock, "Block after write", verifyBlock)) {
    Serial.println("Post-write read-back failed. Card may still contain the test pattern.");
    return;
  }

  if (memcmp(verifyBlock, testPattern, sizeof(verifyBlock)) != 0) {
    Serial.println("Post-write verification mismatch. Restore skipped to avoid using a desynchronized cipher state.");
    return;
  }

  Serial.println("Post-write verification passed.");

  if (!writeBlockAndLog(expectedUid, kTestBlock, originalBlock, NULL, false)) {
    Serial.println("Restore write failed. The test pattern may still be on the card.");
    return;
  }

  if (!readBlockAndLog(expectedUid, kTestBlock, "Block after restore", restoreVerifyBlock)) {
    Serial.println("Restore verification read failed. Manual follow-up may be required.");
    return;
  }

  if (memcmp(restoreVerifyBlock, originalBlock, sizeof(restoreVerifyBlock)) != 0) {
    Serial.println("Restore verification mismatch. Manual follow-up may be required.");
    return;
  }

  Serial.println("Restore verification passed.");
  Serial.println("MF1 S70 read/write/restore test completed.");
}

} // namespace

void setup()
{
  Serial.begin(115200);
  waitForSerial();

  pinMode(kPinLed, OUTPUT);
  digitalWrite(kPinLed, LOW);

  Serial.println("ESP32 I2C MF1 S70 read/write test");
  Serial.print("SDA=");
  Serial.print(kPinSda);
  Serial.print(" SCL=");
  Serial.print(kPinScl);
  Serial.print(" IRQ=");
  Serial.print(kPinIrq);
  Serial.print(" CLK=");
  Serial.println(kI2cClockHz);
  Serial.println("Waiting for one likely MIFARE Classic 4K / S70 card...");

  Wire.begin(kPinSda, kPinScl, kI2cClockHz);
  delay(20);

  const uint8_t probe = waitForI2cAddress(1500UL);
  Serial.print("I2C ACK probe (0x50): ");
  Serial.println((int)probe);
  if (probe != 0U) {
    Serial.println("No ACK from ST25R3916 I2C address.");
    return;
  }

  if (gNfc.rfalNfcInitialize() != ERR_NONE) {
    Serial.println("rfalNfcInitialize() failed.");
    return;
  }

  rfalNfcDiscoverParam params = {};
  params.compMode = RFAL_COMPLIANCE_MODE_NFC;
  params.devLimit = 1U;
  params.nfcfBR = RFAL_BR_212;
  params.ap2pBR = RFAL_BR_424;
  params.notifyCb = onNfcStateChange;
  params.totalDuration = 1000U;
  params.techs2Find = (uint16_t)RFAL_NFC_POLL_TECH_A;
  gDiscoverParams = params;

  if (gNfc.rfalNfcDiscover(&gDiscoverParams) != ERR_NONE) {
    Serial.println("rfalNfcDiscover() failed.");
  }
}

void loop()
{
  gNfc.rfalNfcWorker();

  if (gTestDone || !gTestPending) {
    return;
  }

  gTestPending = false;
  gTestDone = true;

  rfalNfcDevice *device = NULL;
  if ((gNfc.rfalNfcGetActiveDevice(&device) == ERR_NONE) && RfalMf1Class::isNfcaDevice(device)) {
    delay(60);
    runMf1ReadWriteTest(device);
  } else {
    Serial.println("Active device is not ISO14443A. Test aborted.");
  }

  const ReturnCode deactivateErr = gNfc.rfalNfcDeactivate(false);
  printReturnCode("Deactivate to idle", deactivateErr);
  Serial.println("Reader is now idle. Reset the board to run the test again.");
  digitalWrite(kPinLed, LOW);
}
