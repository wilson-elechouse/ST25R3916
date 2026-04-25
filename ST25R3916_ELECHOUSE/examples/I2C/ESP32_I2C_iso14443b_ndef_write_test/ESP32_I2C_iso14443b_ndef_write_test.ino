/*
  Example: ESP32_I2C_iso14443b_ndef_write_test
  Bus: I2C
  Wiring: SDA=21, SCL=22, IRQ=4, LED=2 (optional)
  Target card: ISO14443-4B / Type 4B card exposing writable NDEF

  Test flow:
    1. Probe the ST25R3916 I2C address.
    2. Discover one ISO14443B card.
    3. Verify ISO-DEP activation and NDEF read/write availability.
    4. Read and back up the original raw NDEF message.
    5. Write a small test NDEF message.
    6. Read the message back and verify it.
    7. Restore the original NDEF message.
    8. Read the message back and verify the restore.

  Safety:
    - Uses the standard NDEF Type 4 Tag mapping.
    - Does not touch proprietary APDUs, keys, security state, or file structure.
    - Always attempts to restore the original NDEF message before finishing.
*/

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include <ndef_class.h>
#include <rfal_nfc.h>
#include <rfal_rfst25r3916.h>
#include <st25r3916_config.h>
#include <st_errno.h>

namespace {

constexpr int kPinSda = ST25R3916_DEFAULT_I2C_SDA_PIN;
constexpr int kPinScl = ST25R3916_DEFAULT_I2C_SCL_PIN;
constexpr int kPinIrq = ST25R3916_DEFAULT_IRQ_PIN;
constexpr int kPinLed = ST25R3916_DEFAULT_LED_PIN;
constexpr uint32_t kI2cClockHz = ST25R3916_DEFAULT_I2C_FREQUENCY;
constexpr uint8_t kI2cAddress = 0x50U;
constexpr uint32_t kMaxMessageLen = 256U;

const uint8_t kTestMessage[] = {
  0xD1U, 0x01U, 0x0DU, 0x54U, 0x02U, 0x65U, 0x6EU,
  0x42U, 0x34U, 0x20U, 0x54U, 0x45U, 0x53U, 0x54U, 0x20U, 0x30U, 0x31U
};

RfalRfST25R3916Class gReader(&Wire, kPinIrq);
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

bool isIso14443BDevice(const rfalNfcDevice *device)
{
  if (device == NULL) {
    return false;
  }

  return (device->type == RFAL_NFC_LISTEN_TYPE_NFCB) || (device->type == RFAL_NFC_POLL_TYPE_NFCB);
}

rfalNfcDevice makeNdefCompatibleDevice(const rfalNfcDevice *device)
{
  rfalNfcDevice normalized = {};
  if (device == NULL) {
    return normalized;
  }

  memcpy(&normalized, device, sizeof(normalized));
  if (normalized.type == RFAL_NFC_POLL_TYPE_NFCB) {
    normalized.type = RFAL_NFC_LISTEN_TYPE_NFCB;
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

void runNdefWriteRestoreTest(rfalNfcDevice *device)
{
  uint8_t originalMsg[kMaxMessageLen] = {0};
  uint8_t verifyMsg[kMaxMessageLen] = {0};
  uint8_t restoreMsg[kMaxMessageLen] = {0};
  uint32_t originalLen = 0U;
  uint32_t verifyLen = 0U;
  uint32_t restoreLen = 0U;
  char id[20];

  if (!isIso14443BDevice(device)) {
    Serial.println("Active device is not ISO14443B. Test aborted.");
    return;
  }

  if (device->rfInterface != RFAL_NFC_INTERFACE_ISODEP) {
    Serial.println("Card is ISO14443B but not ISO-DEP active. Test aborted.");
    return;
  }

  const rfalNfcDevice normalizedDevice = makeNdefCompatibleDevice(device);
  ndefInfo info = {};
  formatId(device->nfcid, device->nfcidLen, id, sizeof(id));

  Serial.println();
  Serial.println("=== ISO14443B NDEF Read/Write Test (I2C) ===");
  Serial.print("PUPI: ");
  Serial.println(id);

  if (!prepareWritableNdefContext(&normalizedDevice, &info)) {
    Serial.println("Card is not writable through the current NDEF Type 4B mapping.");
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

  err = gNdef.ndefPollerWriteRawMessage((originalLen == 0U) ? NULL : originalMsg, originalLen);
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
  Serial.println("ISO14443B NDEF write/read/restore test completed.");
}

} // namespace

void setup()
{
  Serial.begin(115200);
  waitForSerial();

  pinMode(kPinLed, OUTPUT);
  digitalWrite(kPinLed, LOW);

  Serial.println("ESP32 I2C ISO14443B NDEF write test");
  Serial.println("Waiting for one ISO14443B card...");

  Wire.begin(kPinSda, kPinScl, kI2cClockHz);
  delay(20);

  const uint8_t probe = probeI2cAddress();
  Serial.print("I2C ACK probe (0x50): ");
  Serial.println((int)probe);
  if (probe != 0U) {
    Serial.println("No ACK from ST25R3916 I2C address.");
    return;
  }

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
  params.techs2Find = (uint16_t)RFAL_NFC_POLL_TECH_B;

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
    runNdefWriteRestoreTest(device);
  } else {
    Serial.println("No active device available. Test aborted.");
  }

  const ReturnCode deactivateErr = gNfc.rfalNfcDeactivate(false);
  printReturnCode("Deactivate to idle", deactivateErr);
  Serial.println("Reader is now idle. Reset the board to run the test again.");
  digitalWrite(kPinLed, LOW);
}
