/*
  Example: ESP32_SPI_card_profile
  Bus: SPI
  Wiring: SCK=18, MISO=19, MOSI=23, SS=5, IRQ=4, LED=2 (optional)
  Goal: Profile the active card once, report protocol/subtype and NDEF read/write capability.
*/

#include <Arduino.h>
#include <SPI.h>

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

SPIClass gSpi(VSPI);
RfalRfST25R3916Class gReader(&gSpi, kPinSs, kPinIrq);
RfalNfcClass gNfc(&gReader);
NdefClass gNdef(&gNfc);

bool gProfilePending = false;
bool gProfileDone = false;

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

void onNfcStateChange(rfalNfcState state)
{
  if ((state == RFAL_NFC_STATE_ACTIVATED) && !gProfileDone) {
    gProfilePending = true;
    digitalWrite(kPinLed, HIGH);
  }
}

void printActiveCardProfile(rfalNfcDevice *device)
{
  char id[40];
  formatId(device->nfcid, device->nfcidLen, id, sizeof(id));

  Serial.println();
  Serial.println("=== Card Profile ===");
  Serial.print("Type: ");
  Serial.println(deviceTypeToString(device->type));
  Serial.print("UID: ");
  Serial.println(id);
  Serial.print("UID length: ");
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

  ndefInfo info;
  memset(&info, 0, sizeof(info));

  ReturnCode err = gNdef.ndefPollerContextInitialization(device);
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
    if (isNfcaDevice(device) && nfcaHasProprietarySakBits(device)) {
      Serial.println("Interpretation: this card was bucketed into NFCA T2T by mask, but SAK suggests a proprietary card family.");
      Serial.println("Interpretation: raw T2T read/write should not be attempted on this card without a card-specific protocol.");
    }
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
    if (info.messageLen <= 64U) {
      uint8_t raw[64];
      uint32_t rcvdLen = 0;
      err = gNdef.ndefPollerReadRawMessage(raw, sizeof(raw), &rcvdLen);
      Serial.print("Raw NDEF read: ");
      Serial.print(returnCodeToString(err));
      Serial.print(" (");
      Serial.print((int)err);
      Serial.println(")");
      if (err == ERR_NONE) {
        const uint32_t bytesToPrint = (rcvdLen < sizeof(raw)) ? rcvdLen : (uint32_t)sizeof(raw);
        Serial.print("Raw NDEF bytes (first ");
        Serial.print(bytesToPrint);
        Serial.println("):");
        printBytesAsHex(raw, (size_t)bytesToPrint);
      }
    } else {
      Serial.println("Raw NDEF preview skipped: message is larger than 64 bytes.");
    }
  }

  if (ndefStateIsWritable(info.state)) {
    Serial.println("Next step: safe to try an NDEF write/read-back example on this card.");
  } else {
    Serial.println("Next step: do not write yet; this card is currently read-only or non-NDEF.");
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
  gSpi.begin(kPinSck, kPinMiso, kPinMosi, kPinSs);

  Serial.println("ESP32 SPI card profile demo");
  Serial.println("Waiting for a card to build one profile report...");

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
  discover.techs2Find = RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_B | RFAL_NFC_POLL_TECH_V;
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
}
