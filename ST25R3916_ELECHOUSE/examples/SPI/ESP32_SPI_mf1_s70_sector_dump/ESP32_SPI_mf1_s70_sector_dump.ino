/*
  Example: ESP32_SPI_mf1_s70_sector_dump
  Bus: SPI
  Wiring: SCK=18, MISO=19, MOSI=23, SS=5, IRQ=4, LED=2 (optional)
  Target card: MIFARE One S70 / MIFARE Classic 4K

  Test flow:
    1. Detect a likely S70 card by ATQA/SAK
    2. Authenticate to sector 1 with default Key A = FF FF FF FF FF FF
    3. Read blocks 4, 5, and 6
    4. Skip the sector trailer block 7

  Safety:
    - Read-only example
    - Only accesses normal data blocks in sector 1
    - Does not touch block 0 or any sector trailer
*/

#include <Arduino.h>
#include <SPI.h>

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

constexpr uint8_t kSectorStartBlock = 4U;
constexpr uint8_t kSectorDataBlockCount = 3U;
constexpr uint8_t kDefaultKeyA[RFAL_MF1_KEY_LEN] = {
  0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU
};

SPIClass gSpi(VSPI);
RfalRfST25R3916Class gReader(&gSpi, kPinSs, kPinIrq);
RfalNfcClass gNfc(&gReader);
RfalMf1Class gMf1(&gReader);

bool gDumpPending = false;
bool gDumpDone = false;

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
  if ((state == RFAL_NFC_STATE_ACTIVATED) && !gDumpDone) {
    gDumpPending = true;
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
  Serial.println("=== MF1 S70 Sector Dump ===");
  Serial.print("UID: ");
  Serial.println(id);
  Serial.print("ATQA: ");
  printBytes(atqa, sizeof(atqa));
  Serial.print("SAK: ");
  if (device->dev.nfca.selRes.sak < 0x10U) {
    Serial.print('0');
  }
  Serial.println(device->dev.nfca.selRes.sak, HEX);
  Serial.print("Sector 1 blocks: ");
  Serial.print(kSectorStartBlock);
  Serial.print(", ");
  Serial.print(kSectorStartBlock + 1U);
  Serial.print(", ");
  Serial.println(kSectorStartBlock + 2U);
}

void runSectorDump(rfalNfcDevice *device)
{
  rfalMf1CryptoState crypto = {};
  uint32_t cardNonce = 0U;

  printCardSummary(device);

  if (!RfalMf1Class::isLikelyClassic4K(device)) {
    Serial.println("Card is not the expected MIFARE Classic 4K / S70 profile. Dump aborted.");
    return;
  }

  const ReturnCode authErr = gMf1.authenticate(&crypto,
                                               device,
                                               kSectorStartBlock,
                                               kDefaultKeyA,
                                               RFAL_MF1_AUTH_KEY_A,
                                               &cardNonce);
  printReturnCode("AUTH result", authErr);
  if (authErr != ERR_NONE) {
    Serial.println("Authentication failed. Dump aborted.");
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

  for (uint8_t i = 0U; i < kSectorDataBlockCount; i++) {
    const uint8_t blockNo = (uint8_t)(kSectorStartBlock + i);
    uint8_t blockData[RFAL_MF1_BLOCK_LEN] = {0};
    const ReturnCode readErr = gMf1.readBlock(&crypto, blockNo, blockData);

    Serial.print("READ block ");
    Serial.print(blockNo);
    Serial.print(": ");
    Serial.print(returnCodeToString(readErr));
    Serial.print(" (");
    Serial.print((int)readErr);
    Serial.println(")");
    if (readErr != ERR_NONE) {
      Serial.println("Sector dump aborted.");
      return;
    }

    Serial.print("Block ");
    Serial.print(blockNo);
    Serial.print(": ");
    printBytes(blockData, sizeof(blockData));
  }

  Serial.println("Skipping trailer block 7.");
  Serial.println("Sector dump completed.");
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

  Serial.println("ESP32 SPI MF1 S70 sector dump");
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

  if (gDumpDone || !gDumpPending) {
    return;
  }

  gDumpPending = false;
  gDumpDone = true;

  rfalNfcDevice *device = NULL;
  if ((gNfc.rfalNfcGetActiveDevice(&device) == ERR_NONE) && RfalMf1Class::isNfcaDevice(device)) {
    delay(60);
    runSectorDump(device);
  } else {
    Serial.println("Active device is not ISO14443A. Dump aborted.");
  }

  const ReturnCode deactivateErr = gNfc.rfalNfcDeactivate(false);
  printReturnCode("Deactivate to idle", deactivateErr);
  Serial.println("Reader is now idle. Reset the board to run the dump again.");
  digitalWrite(kPinLed, LOW);
}
