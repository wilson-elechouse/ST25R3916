#ifndef RFAL_MF1_H
#define RFAL_MF1_H

#include <stddef.h>
#include <stdint.h>

#include "rfal_nfc.h"
#include "rfal_rfst25r3916.h"
#include "st_errno.h"

#define RFAL_MF1_KEY_LEN       6U
#define RFAL_MF1_BLOCK_LEN     16U
#define RFAL_MF1_AUTH_KEY_A    0x60U
#define RFAL_MF1_AUTH_KEY_B    0x61U

typedef struct {
  uint32_t odd;
  uint32_t even;
} rfalMf1CryptoState;

class RfalMf1Class {
  public:
    explicit RfalMf1Class(RfalRfST25R3916Class *reader);

    static bool isNfcaDevice(const rfalNfcDevice *device);
    static bool isLikelyClassic4K(const rfalNfcDevice *device);
    static bool isValidClassicBlock(uint8_t blockNo);
    static uint8_t sectorBlockCount(uint8_t sectorNo);
    static int16_t sectorFirstBlock(uint8_t sectorNo);
    static int16_t blockToSector(uint8_t blockNo);
    static bool isTrailerBlock(uint8_t blockNo);
    static bool getUid32(const rfalNfcDevice *device, uint32_t *uid);
    static void makeTestPattern(const uint8_t *source, uint8_t *pattern, size_t len);

    ReturnCode authenticate(rfalMf1CryptoState *state,
                            const rfalNfcDevice *device,
                            uint8_t blockNo,
                            const uint8_t *key,
                            uint8_t authCmd = RFAL_MF1_AUTH_KEY_A,
                            uint32_t *cardNonce = nullptr);

    ReturnCode readBlock(rfalMf1CryptoState *state, uint8_t blockNo, uint8_t *blockData);
    ReturnCode writeBlock(rfalMf1CryptoState *state, uint8_t blockNo, const uint8_t *blockData);

  private:
    ReturnCode transceiveBits(uint8_t *txBuf,
                              uint16_t txBits,
                              uint8_t *rxBuf,
                              uint16_t rxBufBits,
                              uint16_t *rxBits,
                              uint32_t flags,
                              uint32_t fwt);

    RfalRfST25R3916Class *reader_;
};

#endif /* RFAL_MF1_H */
