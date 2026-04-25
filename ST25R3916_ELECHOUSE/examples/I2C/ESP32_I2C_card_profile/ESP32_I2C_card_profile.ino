/*
  Example: ESP32_I2C_card_profile
  Bus: I2C
  Wiring: SDA=21, SCL=22, IRQ=4, LED=2 (optional)
  Goal: Profile the active card once, report protocol/subtype and NDEF read/write capability.
*/

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include <ndef_class.h>
#include <rfal_nfc.h>
#include <rfal_nfcb.h>
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
constexpr unsigned long kDirectNfcbProbeDelayMs = 4000UL;

RfalRfST25R3916Class gReader(&Wire, kPinIrq);
RfalNfcClass gNfc(&gReader);
NdefClass gNdef(&gNfc);

bool gProfilePending = false;
bool gProfileDone = false;
rfalNfcState gLastState = RFAL_NFC_STATE_NOTINIT;
unsigned long gDiscoverStartMs = 0UL;
bool gDirectProbeDone = false;

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
    case RFAL_NFC_LISTEN_TYPE_ST25TB:
      return "ST25TB";
    case RFAL_NFC_LISTEN_TYPE_AP2P:
    case RFAL_NFC_POLL_TYPE_AP2P:
      return "AP2P";
    default:
      return "UNKNOWN";
  }
}

const char *nfcaSubtypeToString(uint8_t type)
{
  switch (type) {
    case RFAL_NFCA_T1T:
      return "NFCA T1T";
    case RFAL_NFCA_T2T:
      return "NFCA T2T";
    case RFAL_NFCA_T4T:
      return "NFCA T4T";
    case RFAL_NFCA_NFCDEP:
      return "NFCA NFC-DEP";
    case RFAL_NFCA_T4T_NFCDEP:
      return "NFCA T4T + NFC-DEP";
    default:
      return "NFCA Unknown";
  }
}

const char *rfInterfaceToString(rfalNfcRfInterface rfInterface)
{
  switch (rfInterface) {
    case RFAL_NFC_INTERFACE_RF:
      return "RF";
    case RFAL_NFC_INTERFACE_ISODEP:
      return "ISO-DEP";
    case RFAL_NFC_INTERFACE_NFCDEP:
      return "NFC-DEP";
    default:
      return "UNKNOWN";
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

bool isNfcaDevice(const rfalNfcDevice *device)
{
  if (device == NULL) {
    return false;
  }

  return (device->type == RFAL_NFC_LISTEN_TYPE_NFCA) || (device->type == RFAL_NFC_POLL_TYPE_NFCA);
}

bool isNfcbDevice(const rfalNfcDevice *device)
{
  if (device == NULL) {
    return false;
  }

  return (device->type == RFAL_NFC_LISTEN_TYPE_NFCB) || (device->type == RFAL_NFC_POLL_TYPE_NFCB);
}

bool nfcaHasProprietarySakBits(const rfalNfcDevice *device)
{
  if (!isNfcaDevice(device)) {
    return false;
  }

  return (device->dev.nfca.selRes.sak & (uint8_t)~RFAL_NFCA_SEL_RES_CONF_MASK) != 0U;
}

const char *nfcaLikelyFamily(const rfalNfcDevice *device)
{
  if (!isNfcaDevice(device)) {
    return "n/a";
  }

  const uint16_t atqa = ((uint16_t)device->dev.nfca.sensRes.platformInfo << 8) | device->dev.nfca.sensRes.anticollisionInfo;
  const uint8_t sak = device->dev.nfca.selRes.sak;

  if ((atqa == 0x0004U) && (sak == 0x08U)) {
    return "Likely MIFARE Classic 1K or compatible";
  }

  if ((atqa == 0x0002U) && (sak == 0x18U)) {
    return "Likely MIFARE Classic 4K or compatible";
  }

  if ((sak == 0x00U) || (sak == 0x04U)) {
    return "Likely NFC Forum Type 2 / Ultralight / NTAG family";
  }

  if (nfcaHasProprietarySakBits(device)) {
    return "Likely proprietary ISO14443A card, not a plain NFC Forum Type 2 tag";
  }

  return "Unclear from ATQA/SAK alone";
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

void printBytesAsHex(const uint8_t *buf, size_t len)
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

rfalNfcDevice makeNdefCompatibleDevice(const rfalNfcDevice *device)
{
  rfalNfcDevice normalized = {};
  if (device == NULL) {
    return normalized;
  }

  memcpy(&normalized, device, sizeof(normalized));

  if (normalized.type == RFAL_NFC_POLL_TYPE_NFCA) {
    normalized.type = RFAL_NFC_LISTEN_TYPE_NFCA;
  } else if (normalized.type == RFAL_NFC_POLL_TYPE_NFCB) {
    normalized.type = RFAL_NFC_LISTEN_TYPE_NFCB;
  } else if (normalized.type == RFAL_NFC_POLL_TYPE_NFCF) {
    normalized.type = RFAL_NFC_LISTEN_TYPE_NFCF;
  } else if (normalized.type == RFAL_NFC_POLL_TYPE_NFCV) {
    normalized.type = RFAL_NFC_LISTEN_TYPE_NFCV;
  }

  return normalized;
}

void onNfcStateChange(rfalNfcState state)
{
  if (state != gLastState) {
    Serial.print("NFC state: ");
    Serial.println((int)state);
    gLastState = state;
  }

  if ((state == RFAL_NFC_STATE_ACTIVATED) && !gProfileDone) {
    gProfilePending = true;
    digitalWrite(kPinLed, HIGH);
  }
}

void printNfcbDetails(const rfalNfcDevice *device)
{
  if (!isNfcbDevice(device)) {
    return;
  }

  const rfalNfcbListenDevice *nfcb = &device->dev.nfcb;

  Serial.print("PUPI: ");
  printBytesAsHex(nfcb->sensbRes.nfcid0, RFAL_NFCB_NFCID0_LEN);
  Serial.print("SENSB_RES length: ");
  Serial.println(nfcb->sensbResLen);
  Serial.print("ISO-DEP advertised in SENSB_RES: ");
  Serial.println(rfalNfcbIsIsoDepSupported(nfcb) ? "YES" : "NO");
  Serial.print("FSCI: ");
  Serial.println(rfalNfcbGetFSCI(&nfcb->sensbRes));
  Serial.print("FWI: ");
  Serial.println((nfcb->sensbRes.protInfo.FwiAdcFo >> RFAL_NFCB_SENSB_RES_FWI_SHIFT) & RFAL_NFCB_SENSB_RES_FWI_MASK);
  Serial.print("DID supported: ");
  Serial.println((nfcb->sensbRes.protInfo.FwiAdcFo & RFAL_NFCB_SENSB_RES_FO_DID_MASK) ? "YES" : "NO");
  Serial.print("NAD supported: ");
  Serial.println((nfcb->sensbRes.protInfo.FwiAdcFo & RFAL_NFCB_SENSB_RES_FO_NAD_MASK) ? "YES" : "NO");
  Serial.print("Advanced protocol features: ");
  Serial.println((nfcb->sensbRes.protInfo.FwiAdcFo & RFAL_NFCB_SENSB_RES_ADC_ADV_FEATURE_MASK) ? "YES" : "NO");
}

void printIsoDepDetails(const rfalNfcDevice *device)
{
  if ((device == NULL) || (device->rfInterface != RFAL_NFC_INTERFACE_ISODEP)) {
    return;
  }

  Serial.print("ISO-DEP FSC/FSD: ");
  Serial.println(device->proto.isoDep.info.FSx);
  Serial.print("ISO-DEP FWI: ");
  Serial.println(device->proto.isoDep.info.FWI);
  Serial.print("ISO-DEP DID: ");
  Serial.println(device->proto.isoDep.info.DID);
  Serial.print("ISO-DEP NAD support: ");
  Serial.println(device->proto.isoDep.info.supNAD ? "YES" : "NO");
  Serial.print("ISO-DEP DID support: ");
  Serial.println(device->proto.isoDep.info.supDID ? "YES" : "NO");
}

void printActiveCardProfile(rfalNfcDevice *device)
{
  char id[40];
  formatId(device->nfcid, device->nfcidLen, id, sizeof(id));

  Serial.println();
  Serial.println("=== Card Profile ===");
  Serial.print("Type: ");
  Serial.println(deviceTypeToString(device->type));
  Serial.print("UID/PUPI: ");
  Serial.println(id);
  Serial.print("ID length: ");
  Serial.println(device->nfcidLen);
  Serial.print("RF interface: ");
  Serial.println(rfInterfaceToString(device->rfInterface));

  if (isNfcaDevice(device)) {
    const uint8_t atqa[2] = {
      device->dev.nfca.sensRes.anticollisionInfo,
      device->dev.nfca.sensRes.platformInfo
    };

    Serial.print("ATQA: ");
    printBytesAsHex(atqa, sizeof(atqa));
    Serial.print("SAK: ");
    if (device->dev.nfca.selRes.sak < 0x10U) {
      Serial.print('0');
    }
    Serial.println(device->dev.nfca.selRes.sak, HEX);
    Serial.print("NFCA subtype: ");
    Serial.println(nfcaSubtypeToString(device->dev.nfca.type));
    Serial.print("Likely family: ");
    Serial.println(nfcaLikelyFamily(device));
    Serial.print("Proprietary SAK bits present: ");
    Serial.println(nfcaHasProprietarySakBits(device) ? "YES" : "NO");
  }

  if (isNfcbDevice(device)) {
    printNfcbDetails(device);
  }

  printIsoDepDetails(device);

  const rfalNfcDevice normalized = makeNdefCompatibleDevice(device);
  ndefInfo info = {};

  ReturnCode err = gNdef.ndefPollerContextInitialization(const_cast<rfalNfcDevice *>(&normalized));
  Serial.print("NDEF context init: ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");

  if (err != ERR_NONE) {
    Serial.println("NDEF result: context setup failed, card may be outside NFC Forum tag mappings.");
    return;
  }

  err = gNdef.ndefPollerNdefDetect(&info);
  Serial.print("NDEF detect: ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");

  if (err != ERR_NONE) {
    Serial.println("NDEF result: not exposed as readable NDEF with the current mapping.");
    return;
  }

  Serial.print("NDEF state: ");
  Serial.println(ndefStateToString(info.state));
  Serial.print("Writable: ");
  Serial.println(ndefStateIsWritable(info.state) ? "YES" : "NO");
  Serial.print("NDEF message length: ");
  Serial.println(info.messageLen);
  Serial.print("NDEF area length: ");
  Serial.println(info.areaLen);
  Serial.print("NDEF available space: ");
  Serial.println(info.areaAvalableSpaceLen);
  Serial.print("NDEF mapping version: ");
  Serial.print(info.majorVersion);
  Serial.print('.');
  Serial.println(info.minorVersion);

  if (info.messageLen > 0U) {
    const uint32_t previewLen = (info.messageLen < 64U) ? info.messageLen : 64U;
    uint8_t raw[64] = {0};
    uint32_t rcvdLen = 0U;
    err = gNdef.ndefPollerReadRawMessage(raw, sizeof(raw), &rcvdLen);
    Serial.print("Raw NDEF read: ");
    Serial.print(returnCodeToString(err));
    Serial.print(" (");
    Serial.print((int)err);
    Serial.println(")");
    if (err == ERR_NONE) {
      Serial.print("Raw NDEF bytes (first ");
      Serial.print(previewLen);
      Serial.println("):");
      printBytesAsHex(raw, (size_t)previewLen);
    }
  }

  if (ndefStateIsWritable(info.state)) {
    Serial.println("Next step: safe to try an NDEF write/read-back example on this card.");
  } else {
    Serial.println("Next step: do not write yet; this card is currently read-only or non-NDEF.");
  }
}

bool runDirectNfcbProfile()
{
  rfalNfcbListenDevice nfcb = {};
  rfalIsoDepDevice isoDep = {};
  rfalNfcDevice device = {};
  uint8_t sensbResLen = 0U;

  Serial.println();
  Serial.println("High-level discover did not activate a card yet.");
  Serial.println("Trying direct NFC-B probe...");

  ReturnCode err = gNfc.rfalNfcDeactivate(false);
  Serial.print("Stop high-level discover: ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");
  if ((err != ERR_NONE) && (err != ERR_WRONG_STATE)) {
    return false;
  }

  err = gNfc.rfalNfcbPollerInitialize();
  Serial.print("rfalNfcbPollerInitialize: ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");
  if (err != ERR_NONE) {
    return false;
  }

  err = gNfc.rfalNfcbPollerCheckPresence(RFAL_NFCB_SENS_CMD_SENSB_REQ, RFAL_NFCB_SLOT_NUM_1, &nfcb.sensbRes, &sensbResLen);
  if (err == ERR_TIMEOUT) {
    Serial.println("REQB timeout, retrying with WUPB...");
    err = gNfc.rfalNfcbPollerCheckPresence(RFAL_NFCB_SENS_CMD_ALLB_REQ, RFAL_NFCB_SLOT_NUM_1, &nfcb.sensbRes, &sensbResLen);
  }

  Serial.print("rfalNfcbPollerCheckPresence: ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");
  if (err != ERR_NONE) {
    Serial.println("No direct NFC-B response was detected.");
    return false;
  }

  nfcb.sensbResLen = sensbResLen;
  device.type = RFAL_NFC_LISTEN_TYPE_NFCB;
  device.dev.nfcb = nfcb;
  device.nfcid = device.dev.nfcb.sensbRes.nfcid0;
  device.nfcidLen = RFAL_NFCB_NFCID0_LEN;
  device.rfInterface = RFAL_NFC_INTERFACE_RF;

  if (rfalNfcbIsIsoDepSupported(&device.dev.nfcb)) {
    gNfc.rfalIsoDepInitialize();
    err = gNfc.rfalIsoDepPollBHandleActivation((rfalIsoDepFSxI)RFAL_ISODEP_FSDI_DEFAULT,
                                               RFAL_ISODEP_NO_DID,
                                               RFAL_BR_424,
                                               0x00,
                                               &device.dev.nfcb,
                                               NULL,
                                               0U,
                                               &isoDep);
    Serial.print("rfalIsoDepPollBHandleActivation: ");
    Serial.print(returnCodeToString(err));
    Serial.print(" (");
    Serial.print((int)err);
    Serial.println(")");
    if (err == ERR_NONE) {
      device.rfInterface = RFAL_NFC_INTERFACE_ISODEP;
      device.proto.isoDep = isoDep;
    }
  }

  printActiveCardProfile(&device);
  return true;
}

} // namespace

void setup()
{
  Serial.begin(115200);
  waitForSerial();

  pinMode(kPinLed, OUTPUT);
  digitalWrite(kPinLed, LOW);

  Serial.println("ESP32 I2C card profile demo");
  Serial.println("Waiting for a card to build one profile report...");

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

  Serial.println("Starting rfalNfcInitialize...");
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
  Serial.println("rfalNfcInitialize ok.");

  rfalNfcDiscoverParam discover = {};
  discover.compMode = RFAL_COMPLIANCE_MODE_NFC;
  discover.devLimit = RFAL_ESP32_DEFAULT_DEVICE_LIMIT;
  discover.techs2Find = RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_B | RFAL_NFC_POLL_TECH_V;
  discover.totalDuration = RFAL_ESP32_DEFAULT_DISCOVERY_DURATION_MS;
  discover.notifyCb = onNfcStateChange;
  gDiscoverStartMs = millis();

  Serial.println("Starting rfalNfcDiscover...");
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
  Serial.println("rfalNfcDiscover ok.");
}

void loop()
{
  if (!gProfileDone) {
    gNfc.rfalNfcWorker();
  }

  if (gProfilePending && !gProfileDone) {
    rfalNfcDevice *device = NULL;
    gProfilePending = false;

    if ((gNfc.rfalNfcGetActiveDevice(&device) == ERR_NONE) && (device != NULL)) {
      printActiveCardProfile(device);
    } else {
      Serial.println("Failed to fetch active device for profiling.");
    }

    gProfileDone = true;
    digitalWrite(kPinLed, LOW);

    const ReturnCode err = gNfc.rfalNfcDeactivate(false);
    Serial.print("Deactivate to idle: ");
    Serial.print(returnCodeToString(err));
    Serial.print(" (");
    Serial.print((int)err);
    Serial.println(")");
    Serial.println("Reader is now idle. Reset the board to profile another card.");
  }

  if (!gProfileDone && !gDirectProbeDone && ((millis() - gDiscoverStartMs) >= kDirectNfcbProbeDelayMs)) {
    gDirectProbeDone = true;
    gProfileDone = runDirectNfcbProfile();
    if (gProfileDone) {
      digitalWrite(kPinLed, LOW);
      Serial.println("Reader is now idle. Reset the board to profile another card.");
    }
  }
}
