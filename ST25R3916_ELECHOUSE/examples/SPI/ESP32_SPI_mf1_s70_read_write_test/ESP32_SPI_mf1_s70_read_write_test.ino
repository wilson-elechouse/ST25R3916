/*
  Example: ESP32_SPI_mf1_s70_read_write_test
  Bus: SPI
  Wiring: SCK=18, MISO=19, MOSI=23, SS=5, IRQ=4, LED=2 (optional)
  Target card: MIFARE One S70 / MIFARE Classic 4K

  Test flow:
    1. Detect a likely S70 card by ATQA/SAK
    2. Authenticate to block 4 with default Key A = FF FF FF FF FF FF
    3. Read block 4 and save a backup
    4. Write a 16-byte test pattern
    5. Read block 4 again and verify
    6. Restore the original block contents
    7. Read block 4 again and verify restore

  Safety:
    - Uses block 4, which is a normal data block in sector 1
    - Does not touch block 0 or a sector trailer
    - Always attempts to restore the original 16 bytes before finishing
*/

#include <Arduino.h>
#include <SPI.h>
#include <string.h>

#include <rfal_mf1.h>
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

constexpr uint8_t kTestBlock = 4U;
constexpr uint8_t kDefaultKeyA[RFAL_MF1_KEY_LEN] = {
  0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU
};
constexpr uint8_t kBaselineBlock4[RFAL_MF1_BLOCK_LEN] = {
  0x53U, 0x54U, 0x32U, 0x35U, 0x52U, 0x33U, 0x39U, 0x31U,
  0x36U, 0x20U, 0x44U, 0x45U, 0x4DU, 0x4FU, 0x30U, 0x31U
};

SPIClass gSpi(VSPI);
RfalRfST25R3916Class gReader(&gSpi, kPinSs, kPinIrq);
RfalNfcClass gNfc(&gReader);
RfalMf1Class gMf1(&gReader);

bool gTestPending = false;
bool gTestDone = false;

void waitForSerial()
{
  const unsigned long start = millis();
  while (!Serial && ((millis() - start) < 2000UL)) {
    delay(10);
  }
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
  Serial.println("=== MF1 S70 Read/Write Test ===");
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

bool readBlockAndLog(rfalMf1CryptoState *crypto, uint8_t blockNo, const char *label, uint8_t *blockData)
{
  const ReturnCode err = gMf1.readBlock(crypto, blockNo, blockData);
  printReturnCode("READ result", err);
  if (err != ERR_NONE) {
    return false;
  }

  Serial.print(label);
  Serial.print(": ");
  printBytes(blockData, RFAL_MF1_BLOCK_LEN);
  return true;
}

bool writeBlockAndLog(rfalMf1CryptoState *crypto,
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

  const ReturnCode err = gMf1.writeBlock(crypto, blockNo, blockData);
  printReturnCode("WRITE result", err);
  return (err == ERR_NONE);
}

void runMf1ReadWriteTest(rfalNfcDevice *device)
{
  uint8_t originalBlock[RFAL_MF1_BLOCK_LEN] = {0};
  uint8_t testPattern[RFAL_MF1_BLOCK_LEN] = {0};
  uint8_t verifyBlock[RFAL_MF1_BLOCK_LEN] = {0};
  uint8_t restoreVerifyBlock[RFAL_MF1_BLOCK_LEN] = {0};
  uint32_t cardNonce = 0U;
  rfalMf1CryptoState crypto = {};

  printCardSummary(device);

  if (!RfalMf1Class::isLikelyClassic4K(device)) {
    Serial.println("Card is not the expected MIFARE Classic 4K / S70 profile. Test aborted.");
    return;
  }

  const ReturnCode authErr = gMf1.authenticate(&crypto, device, kTestBlock, kDefaultKeyA, RFAL_MF1_AUTH_KEY_A, &cardNonce);
  printReturnCode("AUTH result", authErr);
  if (authErr != ERR_NONE) {
    Serial.println("Authentication failed. Test aborted.");
    return;
  }

  uint8_t nonceBytes[4] = {
    (uint8_t)(cardNonce >> 24),
    (uint8_t)(cardNonce >> 16),
    (uint8_t)(cardNonce >> 8),
    (uint8_t)cardNonce
  };
  Serial.print("Card challenge Nt: ");
  printBytes(nonceBytes, sizeof(nonceBytes));

  if (!readBlockAndLog(&crypto, kTestBlock, "Original block 4", originalBlock)) {
    Serial.println("Initial block read failed. Test aborted.");
    return;
  }

  RfalMf1Class::makeTestPattern(kBaselineBlock4, testPattern, sizeof(testPattern));
  if (memcmp(originalBlock, testPattern, sizeof(testPattern)) == 0) {
    Serial.println("Detected a previous interrupted test pattern. Restoring baseline block first.");
    if (!writeBlockAndLog(&crypto, kTestBlock, kBaselineBlock4, NULL, false)) {
      Serial.println("Recovery restore failed. Manual follow-up may be required.");
      return;
    }

    if (!readBlockAndLog(&crypto, kTestBlock, "Block after recovery restore", restoreVerifyBlock)) {
      Serial.println("Recovery verification failed. Manual follow-up may be required.");
      return;
    }

    if (memcmp(restoreVerifyBlock, kBaselineBlock4, sizeof(restoreVerifyBlock)) != 0) {
      Serial.println("Recovery restore verification mismatch. Manual follow-up may be required.");
      return;
    }

    Serial.println("Recovery restore passed.");
    return;
  }

  RfalMf1Class::makeTestPattern(originalBlock, testPattern, sizeof(testPattern));
  if (!writeBlockAndLog(&crypto, kTestBlock, testPattern, "Writing test pattern", true)) {
    Serial.println("Test-pattern write failed. Restore not required.");
    return;
  }

  if (!readBlockAndLog(&crypto, kTestBlock, "Block after write", verifyBlock)) {
    Serial.println("Post-write read-back failed. Card may still contain the test pattern.");
    return;
  }

  if (memcmp(verifyBlock, testPattern, sizeof(verifyBlock)) != 0) {
    Serial.println("Post-write verification mismatch. Restore skipped to avoid using a desynchronized cipher state.");
    return;
  }

  Serial.println("Post-write verification passed.");

  if (!writeBlockAndLog(&crypto, kTestBlock, originalBlock, NULL, false)) {
    Serial.println("Restore write failed. The test pattern may still be on the card.");
    return;
  }

  if (!readBlockAndLog(&crypto, kTestBlock, "Block after restore", restoreVerifyBlock)) {
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
  pinMode(kPinSs, OUTPUT);
  digitalWrite(kPinSs, HIGH);

  Serial.println("ESP32 SPI MF1 S70 read/write test");
  Serial.println("Waiting for one likely MIFARE Classic 4K / S70 card...");

  gSpi.begin(kPinSck, kPinMiso, kPinMosi, kPinSs);

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

  if (gNfc.rfalNfcDiscover(&params) != ERR_NONE) {
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
