/*
  Example: ESP32_SPI_mf1_s70_serial_tool
  Bus: SPI
  Wiring: SCK=18, MISO=19, MOSI=23, SS=5, IRQ=4, LED=2 (optional)
  Target card: MIFARE One S70 / MIFARE Classic 4K

  Usage:
    1. Upload this sketch to the ESP32 and open Serial Monitor at 115200 baud
    2. Place one MIFARE One S70 / MIFARE Classic 4K card on the antenna
    3. Wait for the "mf1>" prompt
    4. Enter commands and press Enter

  Typical command flow:
    show
    card
    read 4
    dump 1 2
    dumpblock 4 7
    save 1 2
    saveblock 4 7

  Key selection examples:
    usea
    keya FFFFFFFFFFFF
    useb
    keyb FFFFFFFFFFFF

  Export format:
    save / saveblock print a stable text format that can be copied directly
    Example:
      EXPORT,sector,block,data
      DATA,1,4,5354323552333931362044454D4F3031
      EXPORT_DONE

  Serial commands:
    help
    show
    defaults
    usea
    useb
    key <12hex>
    keya <12hex>
    keyb <12hex>
    card
    read <block>
    dump <sector> [count]
    dumpblock <startBlock> <count>
    save <sector> [count]
    saveblock <startBlock> <count>

  Safety:
    - Read-only example
    - dump skips trailer blocks automatically
    - read rejects trailer blocks to avoid accidental trailer inspection
*/

#include <Arduino.h>
#include <SPI.h>

#include <ctype.h>
#include <stdlib.h>
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

constexpr size_t kCommandBufLen = 96U;
constexpr uint8_t kDefaultKey[RFAL_MF1_KEY_LEN] = {
  0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU
};

struct Mf1ToolConfig {
  uint8_t keyA[RFAL_MF1_KEY_LEN];
  uint8_t keyB[RFAL_MF1_KEY_LEN];
  uint8_t authCmd;
};

SPIClass gSpi(VSPI);
RfalRfST25R3916Class gReader(&gSpi, kPinSs, kPinIrq);
RfalNfcClass gNfc(&gReader);
RfalMf1Class gMf1(&gReader);
rfalNfcDiscoverParam gDiscoverParams = {};

Mf1ToolConfig gTool = {
  { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
  { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU },
  RFAL_MF1_AUTH_KEY_A
};

char gCommandBuf[kCommandBufLen] = {0};
size_t gCommandLen = 0U;

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

void printBytesCompact(const uint8_t *buf, size_t len)
{
  for (size_t i = 0; i < len; i++) {
    if (buf[i] < 0x10U) {
      Serial.print('0');
    }
    Serial.print(buf[i], HEX);
  }
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

void printPrompt()
{
  Serial.print("mf1> ");
}

void resetKeysToDefault()
{
  memcpy(gTool.keyA, kDefaultKey, sizeof(gTool.keyA));
  memcpy(gTool.keyB, kDefaultKey, sizeof(gTool.keyB));
  gTool.authCmd = RFAL_MF1_AUTH_KEY_A;
}

const uint8_t *activeKey()
{
  return (gTool.authCmd == RFAL_MF1_AUTH_KEY_B) ? gTool.keyB : gTool.keyA;
}

const char *activeKeyLabel()
{
  return (gTool.authCmd == RFAL_MF1_AUTH_KEY_B) ? "Key B" : "Key A";
}

void printToolConfig()
{
  Serial.println("Current MF1 read-only tool config:");
  Serial.print("  Active auth: ");
  Serial.println(activeKeyLabel());
  Serial.print("  Key A: ");
  printBytes(gTool.keyA, sizeof(gTool.keyA));
  Serial.print("  Key B: ");
  printBytes(gTool.keyB, sizeof(gTool.keyB));
}

void printHelp()
{
  Serial.println("Commands:");
  Serial.println("  help");
  Serial.println("  show");
  Serial.println("  defaults");
  Serial.println("  usea");
  Serial.println("  useb");
  Serial.println("  key <12hex>");
  Serial.println("  keya <12hex>");
  Serial.println("  keyb <12hex>");
  Serial.println("  card");
  Serial.println("  read <block>");
  Serial.println("  dump <sector> [count]");
  Serial.println("  dumpblock <startBlock> <count>");
  Serial.println("  save <sector> [count]");
  Serial.println("  saveblock <startBlock> <count>");
}

bool ensureIdle()
{
  if (gNfc.rfalNfcGetState() <= RFAL_NFC_STATE_IDLE) {
    return true;
  }

  const ReturnCode err = gNfc.rfalNfcDeactivate(false);
  if (err != ERR_NONE) {
    printReturnCode("Deactivate to idle", err);
    return false;
  }
  return true;
}

int hexNibble(char c)
{
  if ((c >= '0') && (c <= '9')) {
    return c - '0';
  }
  if ((c >= 'a') && (c <= 'f')) {
    return 10 + (c - 'a');
  }
  if ((c >= 'A') && (c <= 'F')) {
    return 10 + (c - 'A');
  }
  return -1;
}

bool parseHexKey(const char *text, uint8_t *outKey)
{
  char filtered[(RFAL_MF1_KEY_LEN * 2U) + 1U] = {0};
  size_t filteredLen = 0U;

  if ((text == NULL) || (outKey == NULL)) {
    return false;
  }

  while (*text != '\0') {
    const int nibble = hexNibble(*text);
    if (nibble >= 0) {
      if (filteredLen >= (sizeof(filtered) - 1U)) {
        return false;
      }
      filtered[filteredLen++] = *text;
    } else if ((*text != ':') && (*text != '-') && (*text != ' ')) {
      return false;
    }
    text++;
  }

  if (filteredLen != (RFAL_MF1_KEY_LEN * 2U)) {
    return false;
  }

  for (size_t i = 0U; i < RFAL_MF1_KEY_LEN; i++) {
    const int hi = hexNibble(filtered[i * 2U]);
    const int lo = hexNibble(filtered[(i * 2U) + 1U]);
    if ((hi < 0) || (lo < 0)) {
      return false;
    }
    outKey[i] = (uint8_t)((hi << 4) | lo);
  }

  return true;
}

bool parseUint8Value(const char *text, uint8_t *value)
{
  char *end = NULL;
  unsigned long parsed = 0UL;

  if ((text == NULL) || (value == NULL)) {
    return false;
  }

  parsed = strtoul(text, &end, 0);
  if ((end == text) || (*end != '\0') || (parsed > 255UL)) {
    return false;
  }

  *value = (uint8_t)parsed;
  return true;
}

bool acquireClassic4KCard(rfalNfcDevice **device, uint32_t *uid)
{
  ReturnCode err;
  const unsigned long deadline = millis() + 2000UL;

  if ((device == NULL) || (uid == NULL)) {
    return false;
  }

  if (!ensureIdle()) {
    return false;
  }

  err = gNfc.rfalNfcDiscover(&gDiscoverParams);
  if (err != ERR_NONE) {
    printReturnCode("Discover", err);
    return false;
  }

  while (millis() < deadline) {
    rfalNfcDevice *activeDevice = NULL;

    gNfc.rfalNfcWorker();
    if (gNfc.rfalNfcGetState() != RFAL_NFC_STATE_ACTIVATED) {
      delay(1);
      continue;
    }

    err = gNfc.rfalNfcGetActiveDevice(&activeDevice);
    if (err != ERR_NONE) {
      delay(1);
      continue;
    }

    if (!RfalMf1Class::isLikelyClassic4K(activeDevice)) {
      Serial.println("Detected card is not the expected MIFARE Classic 4K / S70.");
      (void)ensureIdle();
      return false;
    }

    if (!RfalMf1Class::getUid32(activeDevice, uid)) {
      Serial.println("Detected card UID format is not supported by this example.");
      (void)ensureIdle();
      return false;
    }

    *device = activeDevice;
    return true;
  }

  Serial.println("Timed out waiting for a likely MIFARE Classic 4K / S70 card.");
  (void)ensureIdle();
  return false;
}

void printCardSummary(const rfalNfcDevice *device, uint32_t uid)
{
  char id[40];
  const uint8_t atqa[2] = {
    device->dev.nfca.sensRes.anticollisionInfo,
    device->dev.nfca.sensRes.platformInfo
  };
  uint8_t uidBytes[4] = {
    (uint8_t)(uid >> 24),
    (uint8_t)(uid >> 16),
    (uint8_t)(uid >> 8),
    (uint8_t)uid
  };

  formatId(uidBytes, sizeof(uidBytes), id, sizeof(id));

  Serial.println("Card profile:");
  Serial.print("  UID: ");
  Serial.println(id);
  Serial.print("  ATQA: ");
  printBytes(atqa, sizeof(atqa));
  Serial.print("  SAK: ");
  if (device->dev.nfca.selRes.sak < 0x10U) {
    Serial.print('0');
  }
  Serial.println(device->dev.nfca.selRes.sak, HEX);
}

bool authenticateForBlock(rfalNfcDevice *device, uint8_t blockNo, rfalMf1CryptoState *crypto, bool verbose)
{
  uint32_t cardNonce = 0U;
  const ReturnCode authErr = gMf1.authenticate(crypto, device, blockNo, activeKey(), gTool.authCmd, &cardNonce);

  if (verbose) {
    printReturnCode("AUTH result", authErr);
  }
  if (authErr != ERR_NONE) {
    return false;
  }

  if (verbose) {
    uint8_t nonceBytes[4] = {
      (uint8_t)(cardNonce >> 24),
      (uint8_t)(cardNonce >> 16),
      (uint8_t)(cardNonce >> 8),
      (uint8_t)cardNonce
    };
    Serial.print("Card challenge Nt: ");
    printBytes(nonceBytes, sizeof(nonceBytes));
  }
  return true;
}

void handleCardCommand()
{
  rfalNfcDevice *device = NULL;
  uint32_t uid = 0U;

  if (!acquireClassic4KCard(&device, &uid)) {
    return;
  }

  printCardSummary(device, uid);
  (void)ensureIdle();
}

void handleReadCommand(uint8_t blockNo)
{
  rfalNfcDevice *device = NULL;
  rfalMf1CryptoState crypto = {};
  uint32_t uid = 0U;
  uint8_t blockData[RFAL_MF1_BLOCK_LEN] = {0};
  ReturnCode readErr;

  if (!RfalMf1Class::isValidClassicBlock(blockNo)) {
    Serial.println("Invalid block number.");
    return;
  }

  if (RfalMf1Class::isTrailerBlock(blockNo)) {
    Serial.println("Trailer blocks are intentionally blocked in this read-only example.");
    return;
  }

  if (!acquireClassic4KCard(&device, &uid)) {
    return;
  }

  printCardSummary(device, uid);
  Serial.print("Reading block ");
  Serial.print(blockNo);
  Serial.print(" using ");
  Serial.println(activeKeyLabel());

  if (!authenticateForBlock(device, blockNo, &crypto, true)) {
    (void)ensureIdle();
    return;
  }

  readErr = gMf1.readBlock(&crypto, blockNo, blockData);
  printReturnCode("READ result", readErr);
  if (readErr == ERR_NONE) {
    Serial.print("Block ");
    Serial.print(blockNo);
    Serial.print(": ");
    printBytes(blockData, sizeof(blockData));
  }

  (void)ensureIdle();
}

void printBlockRecord(uint8_t sectorNo, uint8_t blockNo, const uint8_t *blockData, bool exportMode)
{
  if (exportMode) {
    Serial.print("DATA,");
    Serial.print(sectorNo);
    Serial.print(',');
    Serial.print(blockNo);
    Serial.print(',');
    printBytesCompact(blockData, RFAL_MF1_BLOCK_LEN);
    Serial.println();
    return;
  }

  Serial.print("Block ");
  Serial.print(blockNo);
  Serial.print(": ");
  printBytes(blockData, RFAL_MF1_BLOCK_LEN);
}

bool dumpSectorWindow(rfalNfcDevice *device, uint8_t sectorNo, uint8_t startBlock, uint8_t endBlock, bool exportMode)
{
  const int16_t firstBlock = RfalMf1Class::sectorFirstBlock(sectorNo);
  const uint8_t blockCount = RfalMf1Class::sectorBlockCount(sectorNo);
  const uint8_t sectorLastBlock = (uint8_t)(firstBlock + blockCount - 1);
  rfalMf1CryptoState crypto = {};
  int16_t authBlock = -1;

  if ((firstBlock < 0) || (blockCount == 0U)) {
    Serial.print("Invalid sector number: ");
    Serial.println(sectorNo);
    return false;
  }

  if ((startBlock < (uint8_t)firstBlock) || (startBlock > sectorLastBlock) || (endBlock < startBlock) || (endBlock > sectorLastBlock)) {
    Serial.println("Requested block window is outside the target sector.");
    return false;
  }

  if (!exportMode) {
    Serial.println();
    Serial.print("[Sector ");
    Serial.print(sectorNo);
    Serial.print("] first block ");
    Serial.print((uint8_t)firstBlock);
    Serial.print(", total blocks ");
    Serial.println(blockCount);
  }

  for (uint8_t blockNo = startBlock; blockNo <= endBlock; blockNo++) {
    if (!RfalMf1Class::isTrailerBlock(blockNo)) {
      authBlock = blockNo;
      break;
    }
  }

  if (authBlock < 0) {
    if (!exportMode) {
      Serial.println("Requested window contains only trailer blocks. Nothing to read.");
    }
    return true;
  }

  if (!authenticateForBlock(device, (uint8_t)authBlock, &crypto, !exportMode)) {
    return false;
  }

  for (uint8_t blockNo = startBlock; blockNo <= endBlock; blockNo++) {
    uint8_t blockData[RFAL_MF1_BLOCK_LEN] = {0};
    ReturnCode readErr = ERR_NONE;

    if (RfalMf1Class::isTrailerBlock(blockNo)) {
      if (!exportMode) {
        Serial.print("Skipping trailer block ");
        Serial.println(blockNo);
      }
      continue;
    }

    readErr = gMf1.readBlock(&crypto, blockNo, blockData);
    if (!exportMode) {
      Serial.print("READ block ");
      Serial.print(blockNo);
      Serial.print(": ");
      Serial.print(returnCodeToString(readErr));
      Serial.print(" (");
      Serial.print((int)readErr);
      Serial.println(")");
    }
    if (readErr != ERR_NONE) {
      return false;
    }

    printBlockRecord(sectorNo, blockNo, blockData, exportMode);
  }

  return true;
}

void handleDumpCommand(uint8_t startSector, uint8_t sectorCount)
{
  if ((sectorCount == 0U) || ((uint16_t)startSector + (uint16_t)sectorCount > 40U)) {
    Serial.println("Invalid sector range.");
    return;
  }

  Serial.print("Dumping sectors ");
  Serial.print(startSector);
  Serial.print(" to ");
  Serial.print((uint8_t)(startSector + sectorCount - 1U));
  Serial.print(" using ");
  Serial.println(activeKeyLabel());

  for (uint8_t index = 0U; index < sectorCount; index++) {
    rfalNfcDevice *device = NULL;
    uint32_t uid = 0U;
    const uint8_t sectorNo = (uint8_t)(startSector + index);

    if (!acquireClassic4KCard(&device, &uid)) {
      return;
    }

    if (index == 0U) {
      printCardSummary(device, uid);
    }

    if (!dumpSectorWindow(device,
                          sectorNo,
                          (uint8_t)RfalMf1Class::sectorFirstBlock(sectorNo),
                          (uint8_t)(RfalMf1Class::sectorFirstBlock(sectorNo) + RfalMf1Class::sectorBlockCount(sectorNo) - 1U),
                          false)) {
      Serial.println("Sector dump failed.");
      (void)ensureIdle();
      return;
    }

    (void)ensureIdle();
  }

  Serial.println();
  Serial.println("Sector dump completed.");
}

void handleSaveCommand(uint8_t startSector, uint8_t sectorCount)
{
  if ((sectorCount == 0U) || ((uint16_t)startSector + (uint16_t)sectorCount > 40U)) {
    Serial.println("Invalid sector range.");
    return;
  }

  Serial.println("EXPORT,sector,block,data");
  for (uint8_t index = 0U; index < sectorCount; index++) {
    rfalNfcDevice *device = NULL;
    uint32_t uid = 0U;
    const uint8_t sectorNo = (uint8_t)(startSector + index);
    const int16_t firstBlock = RfalMf1Class::sectorFirstBlock(sectorNo);
    const uint8_t blockCount = RfalMf1Class::sectorBlockCount(sectorNo);

    if ((firstBlock < 0) || (blockCount == 0U)) {
      Serial.println("Export aborted: invalid sector.");
      return;
    }

    if (!acquireClassic4KCard(&device, &uid)) {
      return;
    }

    if (!dumpSectorWindow(device,
                          sectorNo,
                          (uint8_t)firstBlock,
                          (uint8_t)(firstBlock + blockCount - 1U),
                          true)) {
      Serial.println("Export aborted.");
      (void)ensureIdle();
      return;
    }

    (void)ensureIdle();
  }

  Serial.println("EXPORT_DONE");
}

void handleBlockRangeCommand(uint8_t startBlock, uint8_t blockCount, bool exportMode)
{
  const uint16_t endExclusive = (uint16_t)startBlock + (uint16_t)blockCount;
  bool printedCard = false;

  if ((blockCount == 0U) || (endExclusive > 256U)) {
    Serial.println("Invalid block range.");
    return;
  }

  if (exportMode) {
    Serial.println("EXPORT,sector,block,data");
  } else {
    Serial.print("Dumping blocks ");
    Serial.print(startBlock);
    Serial.print(" to ");
    Serial.print((uint8_t)(endExclusive - 1U));
    Serial.print(" using ");
    Serial.println(activeKeyLabel());
  }

  uint16_t currentBlock = startBlock;
  while (currentBlock < endExclusive) {
    const int16_t sectorNo = RfalMf1Class::blockToSector((uint8_t)currentBlock);
    const int16_t sectorFirstBlock = (sectorNo >= 0) ? RfalMf1Class::sectorFirstBlock((uint8_t)sectorNo) : -1;
    const uint8_t sectorSize = (sectorNo >= 0) ? RfalMf1Class::sectorBlockCount((uint8_t)sectorNo) : 0U;
    const uint16_t sectorLastBlock = (uint16_t)(sectorFirstBlock + sectorSize - 1);
    const uint8_t windowStart = (uint8_t)currentBlock;
    const uint8_t windowEnd = (uint8_t)((endExclusive - 1U < sectorLastBlock) ? (endExclusive - 1U) : sectorLastBlock);
    rfalNfcDevice *device = NULL;
    uint32_t uid = 0U;

    if ((sectorNo < 0) || (sectorFirstBlock < 0) || (sectorSize == 0U)) {
      Serial.println("Invalid block range.");
      return;
    }

    if (!acquireClassic4KCard(&device, &uid)) {
      return;
    }

    if (!printedCard && !exportMode) {
      printCardSummary(device, uid);
      printedCard = true;
    }

    if (!dumpSectorWindow(device, (uint8_t)sectorNo, windowStart, windowEnd, exportMode)) {
      Serial.println(exportMode ? "Export aborted." : "Block range dump failed.");
      (void)ensureIdle();
      return;
    }

    (void)ensureIdle();
    currentBlock = sectorLastBlock + 1U;
  }

  if (exportMode) {
    Serial.println("EXPORT_DONE");
  } else {
    Serial.println();
    Serial.println("Block range dump completed.");
  }
}

void handleKeyCommand(uint8_t *keySlot, const char *text, const char *label)
{
  if (!parseHexKey(text, keySlot)) {
    Serial.println("Key must contain 12 hex digits, for example FFFFFFFFFFFF.");
    return;
  }

  Serial.print(label);
  Serial.print(" updated to: ");
  printBytes(keySlot, RFAL_MF1_KEY_LEN);
}

void executeCommand(char *line)
{
  char *savePtr = NULL;
  char *cmd = strtok_r(line, " \t", &savePtr);

  if (cmd == NULL) {
    return;
  }

  for (char *p = cmd; *p != '\0'; ++p) {
    *p = (char)tolower((unsigned char)*p);
  }

  if (strcmp(cmd, "help") == 0) {
    printHelp();
    return;
  }

  if (strcmp(cmd, "show") == 0) {
    printToolConfig();
    return;
  }

  if (strcmp(cmd, "defaults") == 0) {
    resetKeysToDefault();
    Serial.println("Keys and auth mode reset to defaults.");
    printToolConfig();
    return;
  }

  if (strcmp(cmd, "usea") == 0) {
    gTool.authCmd = RFAL_MF1_AUTH_KEY_A;
    Serial.println("Active auth key set to Key A.");
    return;
  }

  if (strcmp(cmd, "useb") == 0) {
    gTool.authCmd = RFAL_MF1_AUTH_KEY_B;
    Serial.println("Active auth key set to Key B.");
    return;
  }

  if (strcmp(cmd, "key") == 0) {
    char *arg = strtok_r(NULL, " \t", &savePtr);
    if (arg == NULL) {
      Serial.println("Usage: key <12hex>");
      return;
    }
    handleKeyCommand((uint8_t *)activeKey(), arg, activeKeyLabel());
    return;
  }

  if (strcmp(cmd, "keya") == 0) {
    char *arg = strtok_r(NULL, " \t", &savePtr);
    if (arg == NULL) {
      Serial.println("Usage: keya <12hex>");
      return;
    }
    handleKeyCommand(gTool.keyA, arg, "Key A");
    return;
  }

  if (strcmp(cmd, "keyb") == 0) {
    char *arg = strtok_r(NULL, " \t", &savePtr);
    if (arg == NULL) {
      Serial.println("Usage: keyb <12hex>");
      return;
    }
    handleKeyCommand(gTool.keyB, arg, "Key B");
    return;
  }

  if (strcmp(cmd, "card") == 0) {
    handleCardCommand();
    return;
  }

  if (strcmp(cmd, "read") == 0) {
    char *arg = strtok_r(NULL, " \t", &savePtr);
    uint8_t blockNo = 0U;

    if ((arg == NULL) || !parseUint8Value(arg, &blockNo)) {
      Serial.println("Usage: read <block>");
      return;
    }

    handleReadCommand(blockNo);
    return;
  }

  if (strcmp(cmd, "dump") == 0) {
    char *sectorArg = strtok_r(NULL, " \t", &savePtr);
    char *countArg = strtok_r(NULL, " \t", &savePtr);
    uint8_t sectorNo = 0U;
    uint8_t sectorCount = 1U;

    if ((sectorArg == NULL) || !parseUint8Value(sectorArg, &sectorNo)) {
      Serial.println("Usage: dump <sector> [count]");
      return;
    }

    if ((countArg != NULL) && !parseUint8Value(countArg, &sectorCount)) {
      Serial.println("Usage: dump <sector> [count]");
      return;
    }

    handleDumpCommand(sectorNo, sectorCount);
    return;
  }

  if (strcmp(cmd, "dumpblock") == 0) {
    char *startArg = strtok_r(NULL, " \t", &savePtr);
    char *countArg = strtok_r(NULL, " \t", &savePtr);
    uint8_t startBlock = 0U;
    uint8_t blockCount = 0U;

    if ((startArg == NULL) || (countArg == NULL) ||
        !parseUint8Value(startArg, &startBlock) ||
        !parseUint8Value(countArg, &blockCount)) {
      Serial.println("Usage: dumpblock <startBlock> <count>");
      return;
    }

    handleBlockRangeCommand(startBlock, blockCount, false);
    return;
  }

  if (strcmp(cmd, "save") == 0) {
    char *sectorArg = strtok_r(NULL, " \t", &savePtr);
    char *countArg = strtok_r(NULL, " \t", &savePtr);
    uint8_t sectorNo = 0U;
    uint8_t sectorCount = 1U;

    if ((sectorArg == NULL) || !parseUint8Value(sectorArg, &sectorNo)) {
      Serial.println("Usage: save <sector> [count]");
      return;
    }

    if ((countArg != NULL) && !parseUint8Value(countArg, &sectorCount)) {
      Serial.println("Usage: save <sector> [count]");
      return;
    }

    handleSaveCommand(sectorNo, sectorCount);
    return;
  }

  if (strcmp(cmd, "saveblock") == 0) {
    char *startArg = strtok_r(NULL, " \t", &savePtr);
    char *countArg = strtok_r(NULL, " \t", &savePtr);
    uint8_t startBlock = 0U;
    uint8_t blockCount = 0U;

    if ((startArg == NULL) || (countArg == NULL) ||
        !parseUint8Value(startArg, &startBlock) ||
        !parseUint8Value(countArg, &blockCount)) {
      Serial.println("Usage: saveblock <startBlock> <count>");
      return;
    }

    handleBlockRangeCommand(startBlock, blockCount, true);
    return;
  }

  Serial.println("Unknown command. Type help.");
}

void processSerialInput()
{
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      gCommandBuf[gCommandLen] = '\0';
      Serial.println();
      executeCommand(gCommandBuf);
      gCommandLen = 0U;
      gCommandBuf[0] = '\0';
      printPrompt();
      continue;
    }

    if (gCommandLen + 1U >= sizeof(gCommandBuf)) {
      Serial.println();
      Serial.println("Command too long.");
      gCommandLen = 0U;
      gCommandBuf[0] = '\0';
      printPrompt();
      continue;
    }

    gCommandBuf[gCommandLen++] = c;
    Serial.write(c);
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

  Serial.println("ESP32 SPI MF1 S70 serial read-only tool");
  Serial.println("Place one likely MIFARE Classic 4K / S70 card on the antenna.");

  gSpi.begin(kPinSck, kPinMiso, kPinMosi, kPinSs);

  if (gNfc.rfalNfcInitialize() != ERR_NONE) {
    Serial.println("rfalNfcInitialize() failed.");
    return;
  }

  gDiscoverParams = {};
  gDiscoverParams.compMode = RFAL_COMPLIANCE_MODE_NFC;
  gDiscoverParams.devLimit = 1U;
  gDiscoverParams.nfcfBR = RFAL_BR_212;
  gDiscoverParams.ap2pBR = RFAL_BR_424;
  gDiscoverParams.totalDuration = 1000U;
  gDiscoverParams.techs2Find = (uint16_t)RFAL_NFC_POLL_TECH_A;

  resetKeysToDefault();
  printHelp();
  printToolConfig();
  printPrompt();
}

void loop()
{
  if (gNfc.rfalNfcGetState() > RFAL_NFC_STATE_IDLE) {
    gNfc.rfalNfcWorker();
  }

  processSerialInput();
}
