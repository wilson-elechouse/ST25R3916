/*
  Example: ESP32_SPI_ndef_write_read_restore
  Bus: SPI
  Wiring: SCK=18, MISO=19, MOSI=23, SS=5, IRQ=4, LED=2 (optional)
  Goal: Generic NDEF write/read/restore test for standard writable NDEF tags over SPI.

  Supported path:
    - NFC-A Type 2 / Type 4
    - NFC-B Type 4
    - NFC-V Type 5

  Test flow:
    1. Initialize the ST25R3916 over SPI.
    2. Discover one NDEF-capable card.
    3. Detect the NDEF mapping and verify it is writable.
    4. Read and back up the original raw NDEF message.
    5. Write a small test raw NDEF message.
    6. Read the message back and verify it.
    7. Restore the original NDEF message.
    8. Read the message back and verify the restore.

  Safety:
    - Uses the standard NFC Forum NDEF mapping only.
    - Does not touch security settings, keys, lock bits, or proprietary commands.
    - Always attempts to restore the original NDEF contents before finishing.
*/

#include <Arduino.h>
#include <SPI.h>
#include <string.h>

#include <ndef_class.h>
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
constexpr uint32_t kMaxMessageLen = 256U;
constexpr uint8_t kEmptyMessagePlaceholder = 0x00U;

const uint8_t kTestMessage[] = {
  0xD1U, 0x01U, 0x12U, 0x54U, 0x02U, 0x65U, 0x6EU,
  0x45U, 0x4CU, 0x45U, 0x43U, 0x48U, 0x4FU, 0x55U, 0x53U, 0x45U,
  0x20U, 0x4EU, 0x44U, 0x45U, 0x46U, 0x20U, 0x31U, 0x00U
};

SPIClass gSpi(VSPI);
RfalRfST25R3916Class gReader(&gSpi, kPinSs, kPinIrq);
RfalNfcClass gNfc(&gReader);
NdefClass gNdef(&gNfc);

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
    default:
      return "ERR_OTHER";
  }
}

const char *ndefStateToString(ndefState state)
{
  switch (state) {
    case NDEF_STATE_INVALID:
      return "INVALID";
    case NDEF_STATE_INITIALIZED:
      return "INITIALIZED";
    case NDEF_STATE_READWRITE:
      return "READWRITE";
    case NDEF_STATE_READONLY:
      return "READONLY";
    default:
      return "UNKNOWN";
  }
}

bool ndefStateIsWritable(ndefState state)
{
  return (state == NDEF_STATE_INITIALIZED) || (state == NDEF_STATE_READWRITE);
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
    case RFAL_NFC_LISTEN_TYPE_NFCF:
    case RFAL_NFC_POLL_TYPE_NFCF:
      return "NFC-F";
    case RFAL_NFC_LISTEN_TYPE_NFCV:
    case RFAL_NFC_POLL_TYPE_NFCV:
      return "ISO15693";
    default:
      return "UNKNOWN";
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

rfalNfcDevice makeNdefCompatibleDevice(const rfalNfcDevice *device)
{
  rfalNfcDevice normalized = {};
  if (device == NULL) {
    return normalized;
  }

  memcpy(&normalized, device, sizeof(normalized));

  switch (normalized.type) {
    case RFAL_NFC_POLL_TYPE_NFCA:
      normalized.type = RFAL_NFC_LISTEN_TYPE_NFCA;
      break;
    case RFAL_NFC_POLL_TYPE_NFCB:
      normalized.type = RFAL_NFC_LISTEN_TYPE_NFCB;
      break;
    case RFAL_NFC_POLL_TYPE_NFCF:
      normalized.type = RFAL_NFC_LISTEN_TYPE_NFCF;
      break;
    case RFAL_NFC_POLL_TYPE_NFCV:
      normalized.type = RFAL_NFC_LISTEN_TYPE_NFCV;
      break;
    default:
      break;
  }

  return normalized;
}

void onNfcStateChange(rfalNfcState state)
{
  if ((state == RFAL_NFC_STATE_ACTIVATED) && !gTestDone) {
    gTestPending = true;
    digitalWrite(kPinLed, HIGH);
  }
}

bool prepareWritableNdefContext(const rfalNfcDevice *normalizedDevice, ndefInfo *info)
{
  if ((normalizedDevice == NULL) || (info == NULL)) {
    return false;
  }

  ReturnCode err = gNdef.ndefPollerContextInitialization(const_cast<rfalNfcDevice *>(normalizedDevice));
  printReturnCode("NDEF context init", err);
  if (err != ERR_NONE) {
    return false;
  }

  err = gNdef.ndefPollerNdefDetect(info);
  printReturnCode("NDEF detect", err);
  if (err != ERR_NONE) {
    return false;
  }

  Serial.print("NDEF state: ");
  Serial.println(ndefStateToString(info->state));
  Serial.print("NDEF message length: ");
  Serial.println(info->messageLen);
  Serial.print("NDEF available space: ");
  Serial.println(info->areaAvalableSpaceLen);
  Serial.print("NDEF area length: ");
  Serial.println(info->areaLen);

  return ndefStateIsWritable(info->state);
}

bool readRawMessageAndLog(uint8_t *buf, uint32_t bufLen, uint32_t *rcvdLen, const char *label)
{
  const ReturnCode err = gNdef.ndefPollerReadRawMessage(buf, bufLen, rcvdLen);
  printReturnCode(label, err);
  if (err != ERR_NONE) {
    return false;
  }

  Serial.print("Raw NDEF bytes (");
  Serial.print(*rcvdLen);
  Serial.println("):");
  printBytes(buf, *rcvdLen);
  return true;
}

void runGenericNdefWriteRestoreTest(rfalNfcDevice *device)
{
  uint8_t originalMsg[kMaxMessageLen] = {0};
  uint8_t verifyMsg[kMaxMessageLen] = {0};
  uint8_t restoreMsg[kMaxMessageLen] = {0};
  uint32_t originalLen = 0U;
  uint32_t verifyLen = 0U;
  uint32_t restoreLen = 0U;
  char id[40];

  if (device == NULL) {
    Serial.println("No active device. Test aborted.");
    return;
  }

  const rfalNfcDevice normalizedDevice = makeNdefCompatibleDevice(device);
  ndefInfo info = {};
  formatId(device->nfcid, device->nfcidLen, id, sizeof(id));

  Serial.println();
  Serial.println("=== Generic NDEF Write/Read/Restore Test (SPI) ===");
  Serial.print("Type: ");
  Serial.println(deviceTypeToString(device->type));
  Serial.print("ID: ");
  Serial.println(id);

  if (!prepareWritableNdefContext(&normalizedDevice, &info)) {
    Serial.println("Card is not writable through the current standard NDEF mapping.");
    return;
  }

  if (sizeof(kTestMessage) > info.areaAvalableSpaceLen) {
    Serial.println("Test message does not fit in the available NDEF space. Test aborted.");
    return;
  }

  if ((info.messageLen > 0U) && !readRawMessageAndLog(originalMsg, sizeof(originalMsg), &originalLen, "Read original raw NDEF")) {
    Serial.println("Failed to back up the original NDEF message. Test aborted.");
    return;
  }

  Serial.print("Writing test raw NDEF (");
  Serial.print(sizeof(kTestMessage));
  Serial.println(" bytes):");
  printBytes(kTestMessage, sizeof(kTestMessage));

  ReturnCode err = gNdef.ndefPollerWriteRawMessage(kTestMessage, sizeof(kTestMessage));
  printReturnCode("Write test NDEF", err);
  if (err != ERR_NONE) {
    Serial.println("Write failed. Restore not required.");
    return;
  }

  if (!prepareWritableNdefContext(&normalizedDevice, &info)) {
    Serial.println("Failed to refresh the NDEF context after write.");
    return;
  }

  if (!readRawMessageAndLog(verifyMsg, sizeof(verifyMsg), &verifyLen, "Read back written NDEF")) {
    Serial.println("Read-back failed after write.");
    return;
  }

  if ((verifyLen != sizeof(kTestMessage)) || (memcmp(verifyMsg, kTestMessage, sizeof(kTestMessage)) != 0)) {
    Serial.println("Post-write verification mismatch. Restore skipped.");
    return;
  }
  Serial.println("Post-write verification passed.");

  const uint8_t *restorePtr = (originalLen == 0U) ? &kEmptyMessagePlaceholder : originalMsg;
  err = gNdef.ndefPollerWriteRawMessage(restorePtr, originalLen);
  printReturnCode("Restore original NDEF", err);
  if (err != ERR_NONE) {
    Serial.println("Restore write failed. Manual follow-up may be required.");
    return;
  }

  if (!prepareWritableNdefContext(&normalizedDevice, &info)) {
    Serial.println("Failed to refresh the NDEF context after restore.");
    return;
  }

  if (originalLen > 0U) {
    if (!readRawMessageAndLog(restoreMsg, sizeof(restoreMsg), &restoreLen, "Read back restored NDEF")) {
      Serial.println("Restore verification read failed.");
      return;
    }

    if ((restoreLen != originalLen) || (memcmp(restoreMsg, originalMsg, originalLen) != 0)) {
      Serial.println("Restore verification mismatch.");
      return;
    }
  } else if (info.messageLen != 0U) {
    Serial.println("Restore verification mismatch: expected empty NDEF message.");
    return;
  }

  Serial.println("Restore verification passed.");
  Serial.println("Generic NDEF write/read/restore test completed.");
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

  Serial.println("ESP32 SPI generic NDEF write/read/restore test");
  Serial.println("Waiting for one NDEF-capable card...");

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
  params.techs2Find = (uint16_t)(RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_B | RFAL_NFC_POLL_TECH_V);

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
    runGenericNdefWriteRestoreTest(device);
  } else {
    Serial.println("No active device available. Test aborted.");
  }

  const ReturnCode deactivateErr = gNfc.rfalNfcDeactivate(false);
  printReturnCode("Deactivate to idle", deactivateErr);
  Serial.println("Reader is now idle. Reset the board to run the test again.");
  digitalWrite(kPinLed, LOW);
}
