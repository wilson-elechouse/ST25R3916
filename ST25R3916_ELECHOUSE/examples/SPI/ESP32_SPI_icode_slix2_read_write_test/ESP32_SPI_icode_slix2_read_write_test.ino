/*
  Example: ESP32_SPI_icode_slix2_read_write_test
  Bus: SPI
  Wiring: SCK=18, MISO=19, MOSI=23, SS=5, IRQ=4, LED=2 (optional)
  Target card: ISO15693 / NFC-V, validated on NXP ICODE SLIX2

  Usage:
    1. Wire the ST25R3916 board for SPI on an ESP32 Dev Module.
    2. Open Serial Monitor at 115200 baud.
    3. Place one ISO15693 card on the antenna.
    4. Reset the board.
    5. The sketch will detect one NFC-V card, read system information,
       choose a safe user block, write a reversible test pattern, verify it,
       restore the original data, and verify the restore.

  Test flow:
    1. Initialize the ST25R3916 over SPI.
    2. Wait for one ISO15693 card.
    3. Read system information to get block size and block count.
    4. Prefer block 8 for the write test, while avoiding the last block
       reserved by ICODE SLIX2 for the originality/counter feature.
    5. Read the original block contents and keep a backup in RAM.
    6. Write a generated test pattern.
    7. Read the block again and verify the write.
    8. Restore the original block contents.
    9. Read the block again and verify the restore.

  Safety:
    - The sketch writes only one normal user block.
    - It does not touch configuration commands, passwords, AFI/DSFID, EAS,
      privacy, or lock operations.
    - It avoids the last SLIX2 block, which is reserved for the counter feature.
    - It always attempts to restore the original block before finishing.
*/

#include <Arduino.h>
#include <SPI.h>
#include <string.h>

#include <rfal_nfc.h>
#include <rfal_nfcv.h>
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

constexpr uint16_t kPreferredTestBlock = 8U;
constexpr uint16_t kReservedTailBlocks = 1U;
constexpr uint16_t kSysInfoBufferLen = 32U;

struct NfcvSystemInfo {
  bool hasDsfid = false;
  bool hasAfi = false;
  bool hasMemSize = false;
  bool hasIcRef = false;
  bool usedExtendedCommand = false;
  uint8_t infoFlags = 0U;
  uint8_t uid[RFAL_NFCV_UID_LEN] = {0};
  uint8_t dsfid = 0U;
  uint8_t afi = 0U;
  uint8_t icRef = 0U;
  uint16_t blockCount = 0U;
  uint8_t blockSize = 0U;
};

SPIClass gSpi(VSPI);
RfalRfST25R3916Class gReader(&gSpi, kPinSs, kPinIrq);
RfalNfcClass gNfc(&gReader);

bool gTestPending = false;
bool gTestDone = false;

bool isNfcvDevice(const rfalNfcDevice *device)
{
  if (device == NULL) {
    return false;
  }

  return (device->type == RFAL_NFC_POLL_TYPE_NFCV) || (device->type == RFAL_NFC_LISTEN_TYPE_NFCV);
}

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
    case ERR_WRITE:
      return "ERR_WRITE";
    case ERR_IO:
      return "ERR_IO";
    case ERR_FRAMING:
      return "ERR_FRAMING";
    case ERR_CRC:
      return "ERR_CRC";
    default:
      return "ERR_OTHER";
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

const char *manufacturerToString(const uint8_t *uid)
{
  if (uid == NULL) {
    return "UNKNOWN";
  }

  switch (uid[6]) {
    case 0x04U:
      return "NXP";
    case 0x02U:
      return "ST";
    default:
      return "OTHER";
  }
}

bool parseSystemInfoResponse(const uint8_t *rxBuf, uint16_t rcvLen, bool extended, NfcvSystemInfo *info)
{
  size_t index = 0U;

  if ((rxBuf == NULL) || (info == NULL) || (rcvLen < 10U)) {
    return false;
  }

  *info = NfcvSystemInfo();
  info->usedExtendedCommand = extended;

  index++;
  info->infoFlags = rxBuf[index++];

  if ((index + RFAL_NFCV_UID_LEN) > rcvLen) {
    return false;
  }
  memcpy(info->uid, &rxBuf[index], RFAL_NFCV_UID_LEN);
  index += RFAL_NFCV_UID_LEN;

  if ((info->infoFlags & RFAL_NFCV_SYSINFO_DFSID) != 0U) {
    if ((index + 1U) > rcvLen) {
      return false;
    }
    info->hasDsfid = true;
    info->dsfid = rxBuf[index++];
  }

  if ((info->infoFlags & RFAL_NFCV_SYSINFO_AFI) != 0U) {
    if ((index + 1U) > rcvLen) {
      return false;
    }
    info->hasAfi = true;
    info->afi = rxBuf[index++];
  }

  if ((info->infoFlags & RFAL_NFCV_SYSINFO_MEMSIZE) != 0U) {
    uint16_t blockCount = 0U;

    if (extended) {
      if ((index + 3U) > rcvLen) {
        return false;
      }
      blockCount = (uint16_t)rxBuf[index];
      index++;
      blockCount |= (uint16_t)((uint16_t)rxBuf[index] << 8U);
      index++;
    } else {
      if ((index + 2U) > rcvLen) {
        return false;
      }
      blockCount = (uint16_t)rxBuf[index++];
    }

    info->hasMemSize = true;
    info->blockCount = (uint16_t)(blockCount + 1U);
    info->blockSize = (uint8_t)(rxBuf[index++] + 1U);
  }

  if ((info->infoFlags & RFAL_NFCV_SYSINFO_ICREF) != 0U) {
    if ((index + 1U) > rcvLen) {
      return false;
    }
    info->hasIcRef = true;
    info->icRef = rxBuf[index++];
  }

  return true;
}

ReturnCode readSystemInfo(const uint8_t *uid, NfcvSystemInfo *info)
{
  uint8_t rxBuf[kSysInfoBufferLen] = {0};
  uint16_t rcvLen = 0U;

  ReturnCode err = gNfc.rfalNfcvPollerGetSystemInformation((uint8_t)RFAL_NFCV_REQ_FLAG_DEFAULT, uid, rxBuf, sizeof(rxBuf), &rcvLen);
  if ((err == ERR_NONE) && parseSystemInfoResponse(rxBuf, rcvLen, false, info)) {
    return ERR_NONE;
  }

  err = gNfc.rfalNfcvPollerExtendedGetSystemInformation((uint8_t)RFAL_NFCV_REQ_FLAG_DEFAULT, uid, (uint8_t)RFAL_NFCV_SYSINFO_REQ_ALL, rxBuf, sizeof(rxBuf), &rcvLen);
  if ((err == ERR_NONE) && parseSystemInfoResponse(rxBuf, rcvLen, true, info)) {
    return ERR_NONE;
  }

  return (err == ERR_NONE) ? ERR_PROTO : err;
}

uint16_t chooseTestBlock(const NfcvSystemInfo &info)
{
  if (!info.hasMemSize || (info.blockCount == 0U)) {
    return 0U;
  }

  const uint16_t highestWritableBlock = (info.blockCount > kReservedTailBlocks)
                                          ? (uint16_t)(info.blockCount - 1U - kReservedTailBlocks)
                                          : 0U;

  if (kPreferredTestBlock <= highestWritableBlock) {
    return kPreferredTestBlock;
  }
  return highestWritableBlock;
}

void makeTestPattern(const uint8_t *source, uint8_t *dest, uint8_t len)
{
  bool changed = false;

  for (uint8_t i = 0; i < len; i++) {
    dest[i] = (uint8_t)(source[i] ^ (uint8_t)(0x5AU + (i * 0x11U)));
    if (dest[i] != source[i]) {
      changed = true;
    }
  }

  if (!changed && (len > 0U)) {
    dest[0] ^= 0xFFU;
  }
}

ReturnCode readSingleBlock(const uint8_t *uid, uint16_t blockNum, uint8_t *blockData, uint8_t blockLen)
{
  uint8_t rxBuf[1U + RFAL_NFCV_MAX_BLOCK_LEN] = {0};
  uint16_t rcvLen = 0U;
  ReturnCode err = ERR_PARAM;

  if ((blockData == NULL) || (blockLen == 0U) || (blockLen > RFAL_NFCV_MAX_BLOCK_LEN)) {
    return ERR_PARAM;
  }

  if (blockNum <= 0xFFU) {
    err = gNfc.rfalNfcvPollerReadSingleBlock((uint8_t)RFAL_NFCV_REQ_FLAG_DEFAULT, uid, (uint8_t)blockNum, rxBuf, sizeof(rxBuf), &rcvLen);
  } else {
    err = gNfc.rfalNfcvPollerExtendedReadSingleBlock((uint8_t)RFAL_NFCV_REQ_FLAG_DEFAULT, uid, blockNum, rxBuf, sizeof(rxBuf), &rcvLen);
  }

  if (err != ERR_NONE) {
    return err;
  }

  if (rcvLen < (uint16_t)(blockLen + 1U)) {
    return ERR_PROTO;
  }

  memcpy(blockData, &rxBuf[1], blockLen);
  return ERR_NONE;
}

ReturnCode writeSingleBlock(const uint8_t *uid, uint16_t blockNum, const uint8_t *blockData, uint8_t blockLen)
{
  ReturnCode err = ERR_PARAM;

  if ((blockData == NULL) || (blockLen == 0U) || (blockLen > RFAL_NFCV_MAX_BLOCK_LEN)) {
    return ERR_PARAM;
  }

  if (blockNum <= 0xFFU) {
    err = gNfc.rfalNfcvPollerWriteSingleBlock((uint8_t)RFAL_NFCV_REQ_FLAG_DEFAULT, uid, (uint8_t)blockNum, blockData, blockLen);
  } else {
    err = gNfc.rfalNfcvPollerExtendedWriteSingleBlock((uint8_t)RFAL_NFCV_REQ_FLAG_DEFAULT, uid, blockNum, blockData, blockLen);
  }

  if ((err != ERR_NONE) && (err != ERR_NOTSUPP)) {
    const uint8_t optionFlags = (uint8_t)RFAL_NFCV_REQ_FLAG_DEFAULT | (uint8_t)RFAL_NFCV_REQ_FLAG_OPTION;
    if (blockNum <= 0xFFU) {
      err = gNfc.rfalNfcvPollerWriteSingleBlock(optionFlags, uid, (uint8_t)blockNum, blockData, blockLen);
    } else {
      err = gNfc.rfalNfcvPollerExtendedWriteSingleBlock(optionFlags, uid, blockNum, blockData, blockLen);
    }
  }

  return err;
}

void printSystemInfoSummary(const NfcvSystemInfo &info)
{
  char uid[3U * RFAL_NFCV_UID_LEN] = {0};

  formatId(info.uid, RFAL_NFCV_UID_LEN, uid, sizeof(uid));

  Serial.println();
  Serial.println("=== ISO15693 / ICODE Read/Write Test (SPI) ===");
  Serial.print("UID: ");
  Serial.println(uid);
  Serial.print("Manufacturer: ");
  Serial.println(manufacturerToString(info.uid));
  Serial.print("System info command: ");
  Serial.println(info.usedExtendedCommand ? "Extended" : "Standard");
  if (info.hasDsfid) {
    Serial.print("DSFID: 0x");
    if (info.dsfid < 0x10U) {
      Serial.print('0');
    }
    Serial.println(info.dsfid, HEX);
  }
  if (info.hasAfi) {
    Serial.print("AFI: 0x");
    if (info.afi < 0x10U) {
      Serial.print('0');
    }
    Serial.println(info.afi, HEX);
  }
  if (info.hasIcRef) {
    Serial.print("IC reference: 0x");
    if (info.icRef < 0x10U) {
      Serial.print('0');
    }
    Serial.println(info.icRef, HEX);
  }
  if (info.hasMemSize) {
    Serial.print("Block count: ");
    Serial.println(info.blockCount);
    Serial.print("Block size: ");
    Serial.print(info.blockSize);
    Serial.println(" bytes");
  }
}

void onNfcStateChange(rfalNfcState state)
{
  if ((state == RFAL_NFC_STATE_ACTIVATED) && !gTestDone) {
    gTestPending = true;
    digitalWrite(kPinLed, HIGH);
  }
}

void runReadWriteTest(rfalNfcDevice *device)
{
  NfcvSystemInfo info;
  uint8_t originalBlock[RFAL_NFCV_MAX_BLOCK_LEN] = {0};
  uint8_t testPattern[RFAL_NFCV_MAX_BLOCK_LEN] = {0};
  uint8_t verifyBlock[RFAL_NFCV_MAX_BLOCK_LEN] = {0};
  uint8_t restoreVerifyBlock[RFAL_NFCV_MAX_BLOCK_LEN] = {0};
  const uint8_t *uid = device->dev.nfcv.InvRes.UID;

  if (!isNfcvDevice(device)) {
    Serial.println("Active device is not ISO15693 / NFC-V. Test aborted.");
    if (device != NULL) {
      Serial.print("Observed device type: ");
      Serial.println((int)device->type);
    }
    return;
  }

  const ReturnCode sysInfoErr = readSystemInfo(uid, &info);
  printReturnCode("GET SYSINFO", sysInfoErr);
  if (sysInfoErr != ERR_NONE) {
    Serial.println("System information read failed. Test aborted.");
    return;
  }

  if (!info.hasMemSize || (info.blockSize == 0U) || (info.blockCount <= (kReservedTailBlocks + 1U))) {
    Serial.println("Card memory geometry is not suitable for the safe write test. Test aborted.");
    return;
  }

  const uint16_t testBlock = chooseTestBlock(info);
  if (testBlock >= info.blockCount) {
    Serial.println("Failed to choose a writable test block. Test aborted.");
    return;
  }

  printSystemInfoSummary(info);
  Serial.print("Preferred test block: ");
  Serial.println(kPreferredTestBlock);
  Serial.print("Selected test block: ");
  Serial.println(testBlock);
  Serial.print("Reserved tail blocks skipped: ");
  Serial.println(kReservedTailBlocks);

  ReturnCode err = readSingleBlock(uid, testBlock, originalBlock, info.blockSize);
  printReturnCode("READ original", err);
  if (err != ERR_NONE) {
    Serial.println("Initial block read failed. Test aborted.");
    return;
  }
  Serial.print("Original block ");
  Serial.print(testBlock);
  Serial.print(": ");
  printBytes(originalBlock, info.blockSize);

  makeTestPattern(originalBlock, testPattern, info.blockSize);
  Serial.print("Writing test pattern: ");
  printBytes(testPattern, info.blockSize);

  err = writeSingleBlock(uid, testBlock, testPattern, info.blockSize);
  printReturnCode("WRITE block", err);
  if (err != ERR_NONE) {
    Serial.println("Write failed. Restore not required.");
    return;
  }

  err = readSingleBlock(uid, testBlock, verifyBlock, info.blockSize);
  printReturnCode("READ after write", err);
  if (err != ERR_NONE) {
    Serial.println("Post-write read-back failed. The test pattern may still be on the card.");
    return;
  }
  Serial.print("Block after write: ");
  printBytes(verifyBlock, info.blockSize);

  if (memcmp(verifyBlock, testPattern, info.blockSize) != 0) {
    Serial.println("Post-write verification mismatch. Restore skipped.");
    return;
  }
  Serial.println("Post-write verification passed.");

  err = writeSingleBlock(uid, testBlock, originalBlock, info.blockSize);
  printReturnCode("RESTORE block", err);
  if (err != ERR_NONE) {
    Serial.println("Restore write failed. Manual follow-up may be required.");
    return;
  }

  err = readSingleBlock(uid, testBlock, restoreVerifyBlock, info.blockSize);
  printReturnCode("READ after restore", err);
  if (err != ERR_NONE) {
    Serial.println("Restore verification read failed. Manual follow-up may be required.");
    return;
  }
  Serial.print("Block after restore: ");
  printBytes(restoreVerifyBlock, info.blockSize);

  if (memcmp(restoreVerifyBlock, originalBlock, info.blockSize) != 0) {
    Serial.println("Restore verification mismatch. Manual follow-up may be required.");
    return;
  }

  Serial.println("Restore verification passed.");
  Serial.println("ISO15693 ICODE SLIX2 read/write/restore test completed.");
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

  Serial.println("ESP32 SPI ICODE SLIX2 read/write test");
  Serial.print("SCK=");
  Serial.print(kPinSck);
  Serial.print(" MISO=");
  Serial.print(kPinMiso);
  Serial.print(" MOSI=");
  Serial.print(kPinMosi);
  Serial.print(" SS=");
  Serial.print(kPinSs);
  Serial.print(" IRQ=");
  Serial.println(kPinIrq);
  Serial.println("Waiting for one ISO15693 / NFC-V card...");

  const ReturnCode initErr = gNfc.rfalNfcInitialize();
  printReturnCode("rfalNfcInitialize", initErr);
  if (initErr != ERR_NONE) {
    return;
  }

  rfalNfcDiscoverParam params = {};
  params.compMode = RFAL_COMPLIANCE_MODE_NFC;
  params.devLimit = 1U;
  params.nfcfBR = RFAL_BR_212;
  params.ap2pBR = RFAL_BR_424;
  params.notifyCb = onNfcStateChange;
  params.totalDuration = 1000U;
  params.techs2Find = (uint16_t)RFAL_NFC_POLL_TECH_V;

  const ReturnCode discoverErr = gNfc.rfalNfcDiscover(&params);
  printReturnCode("rfalNfcDiscover", discoverErr);
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
  if ((gNfc.rfalNfcGetActiveDevice(&device) == ERR_NONE) && (device != NULL)) {
    delay(60);
    runReadWriteTest(device);
  } else {
    Serial.println("No active device available. Test aborted.");
  }

  const ReturnCode deactivateErr = gNfc.rfalNfcDeactivate(false);
  printReturnCode("Deactivate to idle", deactivateErr);
  Serial.println("Reader is now idle. Reset the board to run the test again.");
  digitalWrite(kPinLed, LOW);
}
