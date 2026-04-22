#include "rfal_mf1.h"

#include <Arduino.h>
#include <string.h>

namespace {

constexpr uint8_t kMf1Ack = 0x0AU;
constexpr uint8_t kMf1Read = 0x30U;
constexpr uint8_t kMf1Write = 0xA0U;
constexpr uint8_t kExpectedAtqa0 = 0x02U;
constexpr uint8_t kExpectedAtqa1 = 0x00U;
constexpr uint8_t kExpectedSak = 0x18U;

constexpr uint16_t kPackedNonceBits = 36U;
constexpr uint16_t kPackedAckBits = 4U;
constexpr uint16_t kPackedShortBits = 36U;
constexpr uint16_t kPackedAuthExchangeBits = 72U;
constexpr uint16_t kPackedBlockBits = 162U;

constexpr size_t kPackedNonceBytes = (kPackedNonceBits + 7U) / 8U;
constexpr size_t kPackedAckBytes = (kPackedAckBits + 7U) / 8U;
constexpr size_t kPackedShortBytes = (kPackedShortBits + 7U) / 8U;
constexpr size_t kPackedAuthExchangeBytes = (kPackedAuthExchangeBits + 7U) / 8U;
constexpr size_t kPackedBlockBytes = (kPackedBlockBits + 7U) / 8U;

constexpr uint32_t kTxRxFwt = rfalConvMsTo1fc(20U);

uint32_t bytesToUint32Msb(const uint8_t *buf)
{
  return ((uint32_t)buf[0] << 24) |
         ((uint32_t)buf[1] << 16) |
         ((uint32_t)buf[2] << 8) |
         (uint32_t)buf[3];
}

void uint32ToBytesMsb(uint32_t value, uint8_t *buf)
{
  buf[0] = (uint8_t)(value >> 24);
  buf[1] = (uint8_t)(value >> 16);
  buf[2] = (uint8_t)(value >> 8);
  buf[3] = (uint8_t)value;
}

uint8_t oddParity8(uint8_t value)
{
  return (uint8_t)!__builtin_parity((unsigned int)value);
}

uint8_t evenParity32(uint32_t value)
{
  return (uint8_t)__builtin_parity(value);
}

uint8_t cryptoFilter(uint32_t value)
{
  uint32_t f = 0U;

  f |= (0x000F22C0U >> (value & 0x0FU)) & 0x10U;
  f |= (0x0006C9C0U >> ((value >> 4) & 0x0FU)) & 0x08U;
  f |= (0x0003C8B0U >> ((value >> 8) & 0x0FU)) & 0x04U;
  f |= (0x0001E458U >> ((value >> 12) & 0x0FU)) & 0x02U;
  f |= (0x0000D938U >> ((value >> 16) & 0x0FU)) & 0x01U;
  return (uint8_t)((0xEC57E80AU >> f) & 0x01U);
}

uint64_t keyBytesToUint64(const uint8_t key[RFAL_MF1_KEY_LEN])
{
  uint64_t value = 0U;
  for (size_t i = 0; i < RFAL_MF1_KEY_LEN; i++) {
    value = (value << 8) | key[i];
  }
  return value;
}

void cryptoInit(rfalMf1CryptoState *state, const uint8_t key[RFAL_MF1_KEY_LEN])
{
  const uint64_t keyValue = keyBytesToUint64(key);

  state->odd = 0U;
  state->even = 0U;

  for (int bit = 47; bit > 0; bit -= 2) {
    state->odd = (state->odd << 1) | (uint32_t)((keyValue >> (((bit - 1) ^ 7))) & 0x01ULL);
    state->even = (state->even << 1) | (uint32_t)((keyValue >> ((bit ^ 7))) & 0x01ULL);
  }
}

uint8_t cryptoBigEndianBit(uint32_t value, uint8_t bit)
{
  return (uint8_t)((value >> (bit ^ 24U)) & 0x01U);
}

uint8_t cryptoBit(rfalMf1CryptoState *state, uint8_t inBit, bool encryptedInput)
{
  const uint8_t outBit = cryptoFilter(state->odd);
  uint32_t feedback = encryptedInput ? outBit : 0U;

  feedback ^= (uint32_t)(inBit & 0x01U);
  feedback ^= state->odd & 0x0029CE5CU;
  feedback ^= state->even & 0x00870804U;

  state->even = ((state->even << 1) & 0x00FFFFFFU) | evenParity32(feedback);

  const uint32_t previousOdd = state->odd;
  state->odd = state->even;
  state->even = previousOdd;

  return outBit;
}

uint8_t cryptoByte(rfalMf1CryptoState *state, uint8_t inByte, bool encryptedInput)
{
  uint8_t outByte = 0U;

  for (uint8_t bit = 0U; bit < 8U; bit++) {
    outByte |= (uint8_t)(cryptoBit(state, (uint8_t)((inByte >> bit) & 0x01U), encryptedInput) << bit);
  }

  return outByte;
}

uint32_t cryptoWord(rfalMf1CryptoState *state, uint32_t inWord, bool encryptedInput)
{
  uint32_t outWord = 0U;

  for (uint8_t bit = 0U; bit < 32U; bit++) {
    outWord |= (uint32_t)cryptoBit(state, cryptoBigEndianBit(inWord, bit), encryptedInput) << (bit ^ 24U);
  }

  return outWord;
}

uint32_t swapEndian32(uint32_t value)
{
  value = ((value >> 8) & 0x00FF00FFU) | ((value & 0x00FF00FFU) << 8);
  return (value >> 16) | (value << 16);
}

uint32_t prngSuccessor(uint32_t value, uint32_t steps)
{
  uint32_t prng = swapEndian32(value);

  while (steps-- != 0U) {
    const uint32_t feedback = ((prng >> 16) ^ (prng >> 18) ^ (prng >> 19) ^ (prng >> 21)) & 0x01U;
    prng = (prng >> 1) | (feedback << 31);
  }

  return swapEndian32(prng);
}

uint32_t buildReaderNonceSeed(uint32_t uid, uint8_t blockNo)
{
  uint32_t value = (uint32_t)micros();
  value ^= ((uint32_t)millis() << 12);
  value ^= (uid << 3);
  value ^= ((uint32_t)blockNo << 24);
  value ^= 0xA5C35A1FU;
  value ^= (value << 13);
  value ^= (value >> 17);
  value ^= (value << 5);
  return value;
}

void appendCrcA(RfalRfST25R3916Class *reader, uint8_t *buf, size_t payloadLen)
{
  const uint16_t crc = reader->rfalCrcCalculateCcitt(0x6363U, buf, (uint16_t)payloadLen);
  buf[payloadLen] = (uint8_t)(crc & 0x00FFU);
  buf[payloadLen + 1U] = (uint8_t)(crc >> 8);
}

bool verifyCrcA(RfalRfST25R3916Class *reader, const uint8_t *buf, size_t totalLen)
{
  if (totalLen < 2U) {
    return false;
  }

  const uint16_t crc = reader->rfalCrcCalculateCcitt(0x6363U, buf, (uint16_t)(totalLen - 2U));
  return (buf[totalLen - 2U] == (uint8_t)(crc & 0x00FFU)) &&
         (buf[totalLen - 1U] == (uint8_t)(crc >> 8));
}

void setPackedBit(uint8_t *buf, uint16_t bitIndex, uint8_t bitValue)
{
  const uint16_t byteIndex = bitIndex / 8U;
  const uint8_t bitPos = (uint8_t)(bitIndex % 8U);

  if (bitValue != 0U) {
    buf[byteIndex] |= (uint8_t)(1U << bitPos);
  } else {
    buf[byteIndex] &= (uint8_t)~(1U << bitPos);
  }
}

uint8_t getPackedBit(const uint8_t *buf, uint16_t bitIndex)
{
  return (uint8_t)((buf[bitIndex / 8U] >> (bitIndex % 8U)) & 0x01U);
}

void packBytesWithParity(const uint8_t *data, const uint8_t *parity, size_t byteCount, uint8_t *packed, size_t packedLen)
{
  memset(packed, 0, packedLen);

  uint16_t bitIndex = 0U;
  for (size_t i = 0; i < byteCount; i++) {
    for (uint8_t bit = 0U; bit < 8U; bit++) {
      setPackedBit(packed, bitIndex++, (uint8_t)((data[i] >> bit) & 0x01U));
    }
    setPackedBit(packed, bitIndex++, parity[i]);
  }
}

void unpackBytesWithParity(const uint8_t *packed, size_t byteCount, uint8_t *data, uint8_t *parity)
{
  uint16_t bitIndex = 0U;

  for (size_t i = 0; i < byteCount; i++) {
    uint8_t value = 0U;
    for (uint8_t bit = 0U; bit < 8U; bit++) {
      value |= (uint8_t)(getPackedBit(packed, bitIndex++) << bit);
    }
    data[i] = value;
    parity[i] = getPackedBit(packed, bitIndex++);
  }
}

bool hasAnyPackedData(const uint8_t *packed, size_t packedLen)
{
  for (size_t i = 0; i < packedLen; i++) {
    if (packed[i] != 0U) {
      return true;
    }
  }
  return false;
}

bool normalizePackedLength(ReturnCode err,
                           const uint8_t *packed,
                           size_t packedLen,
                           uint16_t expectedBits,
                           uint16_t *normalizedBits)
{
  if ((normalizedBits == NULL) || !hasAnyPackedData(packed, packedLen)) {
    return false;
  }

  if ((err == ERR_NONE) || (err == ERR_INCOMPLETE_BYTE)) {
    *normalizedBits = expectedBits;
    return true;
  }

  return false;
}

void buildEncryptedFrame(const uint8_t *plain,
                         size_t plainLen,
                         rfalMf1CryptoState *state,
                         uint8_t *cipher,
                         uint8_t *parity)
{
  for (size_t i = 0; i < plainLen; i++) {
    cipher[i] = (uint8_t)(cryptoByte(state, 0x00U, false) ^ plain[i]);
    parity[i] = (uint8_t)(cryptoFilter(state->odd) ^ oddParity8(plain[i]));
  }
}

ReturnCode decryptAckNibble(rfalMf1CryptoState *state, const uint8_t *packed, uint8_t *ack)
{
  if ((state == NULL) || (packed == NULL) || (ack == NULL)) {
    return ERR_PARAM;
  }

  uint8_t plainAck = 0U;
  for (uint8_t bit = 0U; bit < 4U; bit++) {
    const uint8_t rawBit = getPackedBit(packed, bit);
    plainAck |= (uint8_t)((cryptoBit(state, 0x00U, false) ^ rawBit) << bit);
  }

  *ack = plainAck;
  return ERR_NONE;
}

} // namespace

RfalMf1Class::RfalMf1Class(RfalRfST25R3916Class *reader)
  : reader_(reader)
{
}

bool RfalMf1Class::isNfcaDevice(const rfalNfcDevice *device)
{
  if (device == NULL) {
    return false;
  }

  return (device->type == RFAL_NFC_LISTEN_TYPE_NFCA) || (device->type == RFAL_NFC_POLL_TYPE_NFCA);
}

bool RfalMf1Class::isLikelyClassic4K(const rfalNfcDevice *device)
{
  if (!isNfcaDevice(device)) {
    return false;
  }

  return (device->dev.nfca.sensRes.anticollisionInfo == kExpectedAtqa0) &&
         (device->dev.nfca.sensRes.platformInfo == kExpectedAtqa1) &&
         (device->dev.nfca.selRes.sak == kExpectedSak);
}

bool RfalMf1Class::isValidClassicBlock(uint8_t blockNo)
{
  return (blockNo <= 0xFFU);
}

uint8_t RfalMf1Class::sectorBlockCount(uint8_t sectorNo)
{
  if (sectorNo < 32U) {
    return 4U;
  }

  if (sectorNo < 40U) {
    return 16U;
  }

  return 0U;
}

int16_t RfalMf1Class::sectorFirstBlock(uint8_t sectorNo)
{
  if (sectorNo < 32U) {
    return (int16_t)(sectorNo * 4U);
  }

  if (sectorNo < 40U) {
    return (int16_t)(128U + ((sectorNo - 32U) * 16U));
  }

  return -1;
}

int16_t RfalMf1Class::blockToSector(uint8_t blockNo)
{
  if (blockNo < 128U) {
    return (int16_t)(blockNo / 4U);
  }

  return (int16_t)(32U + ((blockNo - 128U) / 16U));
}

bool RfalMf1Class::isTrailerBlock(uint8_t blockNo)
{
  const int16_t sectorNo = blockToSector(blockNo);
  if (sectorNo < 0) {
    return false;
  }

  const int16_t firstBlock = sectorFirstBlock((uint8_t)sectorNo);
  const uint8_t blockCount = sectorBlockCount((uint8_t)sectorNo);

  if ((firstBlock < 0) || (blockCount == 0U)) {
    return false;
  }

  return (blockNo == (uint8_t)(firstBlock + blockCount - 1));
}

bool RfalMf1Class::getUid32(const rfalNfcDevice *device, uint32_t *uid)
{
  if ((device == NULL) || (uid == NULL) || (device->nfcidLen != 4U)) {
    return false;
  }

  *uid = bytesToUint32Msb(device->nfcid);
  return true;
}

void RfalMf1Class::makeTestPattern(const uint8_t *source, uint8_t *pattern, size_t len)
{
  if ((source == NULL) || (pattern == NULL)) {
    return;
  }

  for (size_t i = 0; i < len; i++) {
    pattern[i] = (uint8_t)(source[i] ^ 0xA5U);
  }
}

ReturnCode RfalMf1Class::authenticate(rfalMf1CryptoState *state,
                                      const rfalNfcDevice *device,
                                      uint8_t blockNo,
                                      const uint8_t *key,
                                      uint8_t authCmd,
                                      uint32_t *cardNonce)
{
  uint8_t authReq[2] = { authCmd, blockNo };
  uint8_t rxPacked[kPackedNonceBytes] = {0};
  uint16_t rxBits = 0U;
  uint16_t normalizedBits = 0U;
  uint8_t ntBytes[4] = {0};
  uint8_t ntParity[4] = {0};
  uint32_t uid = 0U;

  if ((reader_ == NULL) || (state == NULL) || (device == NULL) || (key == NULL)) {
    return ERR_PARAM;
  }

  if (!getUid32(device, &uid)) {
    return ERR_PARAM;
  }

  ReturnCode err = transceiveBits(authReq,
                                  16U,
                                  rxPacked,
                                  (uint16_t)(kPackedNonceBytes * 8U),
                                  &rxBits,
                                  ((uint32_t)RFAL_TXRX_FLAGS_CRC_TX_AUTO |
                                   (uint32_t)RFAL_TXRX_FLAGS_CRC_RX_KEEP |
                                   (uint32_t)RFAL_TXRX_FLAGS_NFCIP1_OFF |
                                   (uint32_t)RFAL_TXRX_FLAGS_AGC_ON |
                                   (uint32_t)RFAL_TXRX_FLAGS_PAR_RX_KEEP |
                                   (uint32_t)RFAL_TXRX_FLAGS_PAR_TX_AUTO |
                                   (uint32_t)RFAL_TXRX_FLAGS_NFCV_FLAG_AUTO),
                                  kTxRxFwt);

  if (!normalizePackedLength(err, rxPacked, sizeof(rxPacked), kPackedNonceBits, &normalizedBits)) {
    return (err == ERR_NONE) ? ERR_PROTO : err;
  }

  unpackBytesWithParity(rxPacked, 4U, ntBytes, ntParity);
  for (size_t i = 0; i < 4U; i++) {
    if (ntParity[i] != oddParity8(ntBytes[i])) {
      return ERR_PROTO;
    }
  }

  const uint32_t nt = bytesToUint32Msb(ntBytes);
  if (cardNonce != NULL) {
    *cardNonce = nt;
  }

  const uint32_t nrSeed = buildReaderNonceSeed(uid, blockNo);
  uint8_t nrBytes[4];
  uint32ToBytesMsb(nrSeed, nrBytes);

  cryptoInit(state, key);
  (void)cryptoWord(state, nt ^ uid, false);

  uint8_t authPayload[8] = {0};
  uint8_t authParity[8] = {0};
  uint32_t readerPrng = prngSuccessor(nt, 32U);

  for (size_t i = 0; i < 4U; i++) {
    authPayload[i] = (uint8_t)(cryptoByte(state, nrBytes[i], false) ^ nrBytes[i]);
    authParity[i] = (uint8_t)(cryptoFilter(state->odd) ^ oddParity8(nrBytes[i]));
  }

  for (size_t i = 4U; i < 8U; i++) {
    readerPrng = prngSuccessor(readerPrng, 8U);
    const uint8_t plainByte = (uint8_t)(readerPrng & 0xFFU);
    authPayload[i] = (uint8_t)(cryptoByte(state, 0x00U, false) ^ plainByte);
    authParity[i] = (uint8_t)(cryptoFilter(state->odd) ^ oddParity8(plainByte));
  }

  uint8_t authPacked[kPackedAuthExchangeBytes] = {0};
  packBytesWithParity(authPayload, authParity, 8U, authPacked, sizeof(authPacked));

  memset(rxPacked, 0, sizeof(rxPacked));
  rxBits = 0U;
  err = transceiveBits(authPacked,
                       kPackedAuthExchangeBits,
                       rxPacked,
                       (uint16_t)(kPackedNonceBytes * 8U),
                       &rxBits,
                       ((uint32_t)RFAL_TXRX_FLAGS_CRC_TX_MANUAL |
                        (uint32_t)RFAL_TXRX_FLAGS_CRC_RX_KEEP |
                        (uint32_t)RFAL_TXRX_FLAGS_NFCIP1_OFF |
                        (uint32_t)RFAL_TXRX_FLAGS_AGC_ON |
                        (uint32_t)RFAL_TXRX_FLAGS_PAR_RX_KEEP |
                        (uint32_t)RFAL_TXRX_FLAGS_PAR_TX_NONE |
                        (uint32_t)RFAL_TXRX_FLAGS_NFCV_FLAG_AUTO),
                       kTxRxFwt);

  if (!normalizePackedLength(err, rxPacked, sizeof(rxPacked), kPackedNonceBits, &normalizedBits)) {
    return (err == ERR_NONE) ? ERR_PROTO : err;
  }

  uint8_t atCipher[4] = {0};
  uint8_t atParity[4] = {0};
  unpackBytesWithParity(rxPacked, 4U, atCipher, atParity);

  const uint32_t expectedAt = prngSuccessor(readerPrng, 32U);
  uint8_t expectedAtBytes[4];
  uint32ToBytesMsb(expectedAt, expectedAtBytes);

  uint8_t decryptedAt[4] = {0};
  for (size_t i = 0; i < 4U; i++) {
    decryptedAt[i] = (uint8_t)(cryptoByte(state, 0x00U, false) ^ atCipher[i]);
    const uint8_t expectedParity = (uint8_t)(cryptoFilter(state->odd) ^ oddParity8(decryptedAt[i]));
    if (atParity[i] != expectedParity) {
      return ERR_PROTO;
    }
  }

  return (memcmp(decryptedAt, expectedAtBytes, sizeof(decryptedAt)) == 0) ? ERR_NONE : ERR_PROTO;
}

ReturnCode RfalMf1Class::readBlock(rfalMf1CryptoState *state, uint8_t blockNo, uint8_t *blockData)
{
  uint8_t plainCmd[4] = { kMf1Read, blockNo, 0x00U, 0x00U };
  uint8_t cipherCmd[4] = {0};
  uint8_t parityCmd[4] = {0};
  uint8_t packedCmd[kPackedShortBytes] = {0};
  uint8_t rxPacked[kPackedBlockBytes] = {0};
  uint16_t rxBits = 0U;
  uint16_t normalizedBits = 0U;

  if ((reader_ == NULL) || (state == NULL) || (blockData == NULL)) {
    return ERR_PARAM;
  }

  appendCrcA(reader_, plainCmd, 2U);
  buildEncryptedFrame(plainCmd, sizeof(plainCmd), state, cipherCmd, parityCmd);
  packBytesWithParity(cipherCmd, parityCmd, sizeof(cipherCmd), packedCmd, sizeof(packedCmd));

  const ReturnCode err = transceiveBits(packedCmd,
                                        kPackedShortBits,
                                        rxPacked,
                                        (uint16_t)(kPackedBlockBytes * 8U),
                                        &rxBits,
                                        ((uint32_t)RFAL_TXRX_FLAGS_CRC_TX_MANUAL |
                                         (uint32_t)RFAL_TXRX_FLAGS_CRC_RX_KEEP |
                                         (uint32_t)RFAL_TXRX_FLAGS_NFCIP1_OFF |
                                         (uint32_t)RFAL_TXRX_FLAGS_AGC_ON |
                                         (uint32_t)RFAL_TXRX_FLAGS_PAR_RX_KEEP |
                                         (uint32_t)RFAL_TXRX_FLAGS_PAR_TX_NONE |
                                         (uint32_t)RFAL_TXRX_FLAGS_NFCV_FLAG_AUTO),
                                        kTxRxFwt);

  if (!normalizePackedLength(err, rxPacked, sizeof(rxPacked), kPackedBlockBits, &normalizedBits)) {
    return (err == ERR_NONE) ? ERR_PROTO : err;
  }

  uint8_t cipherReply[18] = {0};
  uint8_t parityReply[18] = {0};
  unpackBytesWithParity(rxPacked, 18U, cipherReply, parityReply);

  uint8_t plainReply[18] = {0};
  for (size_t i = 0; i < 18U; i++) {
    plainReply[i] = (uint8_t)(cryptoByte(state, 0x00U, false) ^ cipherReply[i]);
    const uint8_t expectedParity = (uint8_t)(cryptoFilter(state->odd) ^ oddParity8(plainReply[i]));
    if (parityReply[i] != expectedParity) {
      return ERR_PROTO;
    }
  }

  if (!verifyCrcA(reader_, plainReply, sizeof(plainReply))) {
    return ERR_PROTO;
  }

  memcpy(blockData, plainReply, RFAL_MF1_BLOCK_LEN);
  return ERR_NONE;
}

ReturnCode RfalMf1Class::writeBlock(rfalMf1CryptoState *state, uint8_t blockNo, const uint8_t *blockData)
{
  uint8_t plainCmd[4] = { kMf1Write, blockNo, 0x00U, 0x00U };
  uint8_t cipherCmd[4] = {0};
  uint8_t parityCmd[4] = {0};
  uint8_t packedCmd[kPackedShortBytes] = {0};
  uint8_t ackPacked[kPackedAckBytes] = {0};
  uint16_t ackBits = 0U;
  uint16_t normalizedBits = 0U;

  if ((reader_ == NULL) || (state == NULL) || (blockData == NULL)) {
    return ERR_PARAM;
  }

  appendCrcA(reader_, plainCmd, 2U);
  buildEncryptedFrame(plainCmd, sizeof(plainCmd), state, cipherCmd, parityCmd);
  packBytesWithParity(cipherCmd, parityCmd, sizeof(cipherCmd), packedCmd, sizeof(packedCmd));

  ReturnCode err = transceiveBits(packedCmd,
                                  kPackedShortBits,
                                  ackPacked,
                                  (uint16_t)(kPackedAckBytes * 8U),
                                  &ackBits,
                                  ((uint32_t)RFAL_TXRX_FLAGS_CRC_TX_MANUAL |
                                   (uint32_t)RFAL_TXRX_FLAGS_CRC_RX_KEEP |
                                   (uint32_t)RFAL_TXRX_FLAGS_NFCIP1_OFF |
                                   (uint32_t)RFAL_TXRX_FLAGS_AGC_ON |
                                   (uint32_t)RFAL_TXRX_FLAGS_PAR_RX_KEEP |
                                   (uint32_t)RFAL_TXRX_FLAGS_PAR_TX_NONE |
                                   (uint32_t)RFAL_TXRX_FLAGS_NFCV_FLAG_AUTO),
                                  kTxRxFwt);

  if (!normalizePackedLength(err, ackPacked, sizeof(ackPacked), kPackedAckBits, &normalizedBits)) {
    return (err == ERR_NONE) ? ERR_PROTO : err;
  }

  uint8_t ack = 0U;
  err = decryptAckNibble(state, ackPacked, &ack);
  if ((err != ERR_NONE) || ((ack & 0x0FU) != kMf1Ack)) {
    return ERR_PROTO;
  }

  uint8_t plainData[18] = {0};
  memcpy(plainData, blockData, RFAL_MF1_BLOCK_LEN);
  appendCrcA(reader_, plainData, RFAL_MF1_BLOCK_LEN);

  uint8_t cipherData[18] = {0};
  uint8_t parityData[18] = {0};
  buildEncryptedFrame(plainData, sizeof(plainData), state, cipherData, parityData);

  uint8_t packedData[kPackedBlockBytes] = {0};
  packBytesWithParity(cipherData, parityData, sizeof(cipherData), packedData, sizeof(packedData));

  memset(ackPacked, 0, sizeof(ackPacked));
  ackBits = 0U;
  err = transceiveBits(packedData,
                       kPackedBlockBits,
                       ackPacked,
                       (uint16_t)(kPackedAckBytes * 8U),
                       &ackBits,
                       ((uint32_t)RFAL_TXRX_FLAGS_CRC_TX_MANUAL |
                        (uint32_t)RFAL_TXRX_FLAGS_CRC_RX_KEEP |
                        (uint32_t)RFAL_TXRX_FLAGS_NFCIP1_OFF |
                        (uint32_t)RFAL_TXRX_FLAGS_AGC_ON |
                        (uint32_t)RFAL_TXRX_FLAGS_PAR_RX_KEEP |
                        (uint32_t)RFAL_TXRX_FLAGS_PAR_TX_NONE |
                        (uint32_t)RFAL_TXRX_FLAGS_NFCV_FLAG_AUTO),
                       kTxRxFwt);

  if (!normalizePackedLength(err, ackPacked, sizeof(ackPacked), kPackedAckBits, &normalizedBits)) {
    return (err == ERR_NONE) ? ERR_PROTO : err;
  }

  ack = 0U;
  err = decryptAckNibble(state, ackPacked, &ack);
  if ((err != ERR_NONE) || ((ack & 0x0FU) != kMf1Ack)) {
    return ERR_PROTO;
  }

  return ERR_NONE;
}

ReturnCode RfalMf1Class::transceiveBits(uint8_t *txBuf,
                                        uint16_t txBits,
                                        uint8_t *rxBuf,
                                        uint16_t rxBufBits,
                                        uint16_t *rxBits,
                                        uint32_t flags,
                                        uint32_t fwt)
{
  rfalTransceiveContext ctx = {};

  if (reader_ == NULL) {
    return ERR_PARAM;
  }

  ctx.txBuf = txBuf;
  ctx.txBufLen = txBits;
  ctx.rxBuf = rxBuf;
  ctx.rxBufLen = rxBufBits;
  ctx.rxRcvdLen = rxBits;
  ctx.flags = flags;
  ctx.fwt = fwt;

  ReturnCode err = reader_->rfalStartTransceive(&ctx);
  if (err != ERR_NONE) {
    return err;
  }

  const unsigned long deadline = millis() + 300UL;
  do {
    reader_->rfalWorker();
    err = reader_->rfalGetTransceiveStatus();
    if (err != ERR_BUSY) {
      return err;
    }
    delay(1);
  } while (millis() < deadline);

  return ERR_TIMEOUT;
}
