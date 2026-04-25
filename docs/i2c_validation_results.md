# I2C Validation Results

## Current State

I2C work in this repository has now reached first-pass hardware validation for
ESP32 + ST25R3916 basic bring-up, `ISO14443A` scanning, and dedicated
`ISO15693` single-block read/write/restore on `NXP ICODE SLIX2`, plus
`ISO14443B / Type 4B` NDEF write/read/restore. The SPI-only example gap has
also now been closed on the I2C side with matching `scan`, `T2T`, and `MF1`
diagnostic sketches.

## Implemented I2C Artifacts

- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_probe_chip/ESP32_I2C_probe_chip.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_scan_14443A/ESP32_I2C_scan_14443A.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_scan_14443AB_15693/ESP32_I2C_scan_14443AB_15693.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_scan_14443A_15693/ESP32_I2C_scan_14443A_15693.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_polling_hotplug/ESP32_I2C_polling_hotplug.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_t2t_write_read_test/ESP32_I2C_t2t_write_read_test.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_mf1_s70_read_write_test/ESP32_I2C_mf1_s70_read_write_test.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_mf1_s70_sector_dump/ESP32_I2C_mf1_s70_sector_dump.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_mf1_s70_sector_range_dump/ESP32_I2C_mf1_s70_sector_range_dump.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_mf1_s70_serial_tool/ESP32_I2C_mf1_s70_serial_tool.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_card_profile/ESP32_I2C_card_profile.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_icode_slix2_read_write_test/ESP32_I2C_icode_slix2_read_write_test.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_ndef_write_read_restore/ESP32_I2C_ndef_write_read_restore.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_iso14443b_ndef_write_test/ESP32_I2C_iso14443b_ndef_write_test.ino`
- low-level I2C transaction result checks in `st25r3916_com.cpp`
- I2C-safe init branching in `rfal_rfst25r3916.cpp`

## Compile Results

The current I2C examples compile for:

- board: `esp32:esp32:esp32`
- library layout: local `NFC-RFAL` + `ST25R3916_ELECHOUSE`

## Validated Hardware Setup

- board: `ESP32 Dev Module`
- `SDA = GPIO21`
- `SCL = GPIO22`
- `IRQ = GPIO4` (`D4`)
- I2C clock: `100 kHz`
- validated `ISO14443A` card: `MIFARE One S70`, UID `D9 A9 CE 70`
- current `ISO14443A` / `MIFARE Classic 4K` validation card: UID `3B 58 C0 38`
- validated `ISO14443A` Type 2 card: UID `04 3F 45 32 F3 1C 90`
- validated `ISO15693` card: `NXP ICODE SLIX2`, UID `FA F3 E3 06 09 01 04 E0`
- validated `ISO14443B / Type 4B` card: PUPI `17 0E 0D 64`

## Hardware Validation Results

### `ESP32_I2C_probe_chip`

- `0x50` ACK probe: passed
- `IC_IDENTITY` read: passed
- `IC_IDENTITY = 0x2A`
- chip type: `ST25R3916`
- revision: `2`
- `rfalInitialize()`: `ERR_NONE (0)`
- post-init `IO_CONF2`: `0x38`

### `ESP32_I2C_scan_14443A`

- initialization: passed
- repeated `ISO14443A` detection: passed
- observed UID: `D9 A9 CE 70`
- result: basic ESP32 I2C `NFCA` scan path is working on hardware

### `ESP32_I2C_scan_14443AB_15693`

- initialization: passed
- multi-tech loop with current `ISO14443A` card: passed
- observed UID: `3B 58 C0 38`
- observed classification: `ISO14443A`
- result: the direct I2C counterpart of the SPI `A/B/V` scanner is working on hardware

### `ESP32_I2C_scan_14443A_15693`

- initialization: passed
- multi-tech loop with current `ISO14443A` card: passed
- observed UID: `D9 A9 CE 70`
- result: extended I2C scan remains stable with `A + V` polling enabled

### `ESP32_I2C_polling_hotplug`

- initialization: passed
- insert detection with current `ISO14443A` card: passed
- observed line: `Card inserted: ISO14443A D9 A9 CE 70`
- result: I2C hotplug example no longer floods repeated UID lines during steady presence

### `ESP32_I2C_mf1_s70_read_write_test`

- target card: `MIFARE One S70 / MIFARE Classic 4K`
- default key: `FF FF FF FF FF FF`
- authentication to `block 4`: passed
- write/read/restore cycle on `block 4`: passed
- original block: `53 54 32 35 52 33 39 31 36 20 44 45 4D 4F 30 31`
- test pattern: `F6 F1 97 90 F7 96 9C 94 93 85 E1 E0 E8 EA 95 94`
- restore verification: passed
- result: MF1 helper path is working over I2C for the validated S70 card

### `ESP32_I2C_t2t_write_read_test`

- current card on antenna during this pass: `MIFARE Classic 4K`, UID `3B 58 C0 38`
- observed `ATQA`: `02 00`
- observed `SAK`: `18`
- protection path: passed
- result: the sketch correctly refused raw `T2T` write/read on a non-Type-2 card instead of miswriting a `MIFARE Classic`

### `ESP32_I2C_mf1_s70_sector_range_dump`

- target card: current `MIFARE Classic 4K / S70`, UID `3B 58 C0 38`
- sector range `1-2` dump: passed
- block reads `4/5/6/8/9/10`: `ERR_NONE`
- trailer blocks `7/11`: skipped as intended
- result: the I2C sector-range MF1 dump path is working on hardware

### `ESP32_I2C_mf1_s70_serial_tool`

- target card: current `MIFARE Classic 4K / S70`, UID `3B 58 C0 38`
- boot and prompt: passed
- validated commands: `show`, `card`, `read 4`, `dump 1 2`
- authentication and read results: `ERR_NONE`
- result: the interactive I2C MF1 read-only tool is working on hardware

### `ESP32_I2C_icode_slix2_read_write_test`

- target card: `NXP ICODE SLIX2 / ISO15693`
- system info read: passed
- manufacturer: `NXP`
- IC reference: `0x01`
- memory geometry: `80` blocks, `4` bytes per block
- selected safe test block: `8`
- original block `8`: `00 00 00 00`
- test pattern: `5A 6B 7C 8D`
- write/read/restore cycle on block `8`: passed
- restore verification: passed
- result: NFC-V single-block write/read/restore is working over I2C for the validated ICODE SLIX2 card

### `ESP32_I2C_card_profile`

- target card: current writable `ISO14443B / Type 4B` card
- type: `ISO14443B`
- RF interface: `ISO-DEP`
- PUPI: `17 0E 0D 64`
- `SENSB_RES` length: `12`
- `ISO-DEP` support advertised: yes
- `NDEF detect`: passed
- `NDEF state`: `READWRITE`
- `NDEF message length`: `15`
- `NDEF area length`: `128`
- result: current card is confirmed to be a writable `ISO14443-4B / Type 4B` NDEF card

### `ESP32_I2C_iso14443b_ndef_write_test`

- target card: current writable `ISO14443B / Type 4B` NDEF card
- PUPI: `17 0E 0D 64`
- initial raw NDEF: `D1 01 0B 55 01 6F 6B 65 64 64 79 2E 63 6F 6D`
- test raw NDEF: `D1 01 0D 54 02 65 6E 42 34 20 54 45 53 54 20 30 31`
- NDEF write/read/restore cycle: passed
- restore verification: passed
- result: `ISO14443B / Type 4B` NDEF writing is working over I2C for the validated card

### `ESP32_I2C_ndef_write_read_restore`

- target card in this validation pass: writable `ISO14443A / Type 2` NDEF card
- UID: `04 3F 45 32 F3 1C 90`
- original raw NDEF: `D1 01 0B 55 01 6F 6B 65 64 64 79 2E 63 6F 6D`
- test raw NDEF: `D1 01 12 54 02 65 6E 45 4C 45 43 48 4F 55 53 45 20 4E 44 45 46 20 31 00`
- NDEF write/read/restore cycle: passed
- restore verification: passed
- result: generic I2C NDEF write/read/restore is working on the validated Type 2 NDEF card

## Known Limits

- `ESP32_I2C_scan_14443A` and `ESP32_I2C_scan_14443A_15693` intentionally
  deactivate after each activation, so a stationary card will be printed repeatedly
- `ESP32_I2C_polling_hotplug` is the preferred long-running demo when insert/remove
  behaviour matters
- `ESP32_I2C_mf1_s70_read_write_test` is validated only for the current default-key
  `MIFARE One S70` card and safe data block `4`
- `ESP32_I2C_icode_slix2_read_write_test` is validated only for the current
  `NXP ICODE SLIX2` card and safe data block `8`
- `ESP32_I2C_ndef_write_read_restore` is validated in this pass on the current
  writable `ISO14443A / Type 2` NDEF card
- `ESP32_I2C_iso14443b_ndef_write_test` is validated only for the current writable
  `ISO14443B / Type 4B` NDEF card
- `ESP32_I2C_mf1_s70_sector_dump` compiles and shares the same MF1 helper path, but this pass focused hardware time on the wider `sector_range_dump` and `serial_tool` variants
- the multi-tech `ESP32_I2C_scan_14443A_15693` sketch was not rerun in this pass
  with the SLIX2 card even though the underlying NFC-V path is now hardware-validated
- I2C support should still be treated as first-pass ESP32 support, not broad
  cross-board closure

## Current Conclusion

The repository can now claim:

`ESP32 I2C bring-up, ISO14443A scanning, direct A/B/V scanning, hotplug polling, MF1 S70 block write/read/restore, MF1 sector-range dump, MF1 serial diagnostics, ISO15693 ICODE SLIX2 single-block write/read/restore, ISO14443B / Type 4B NDEF write/read/restore, and generic NDEF write/read/restore on a writable Type 2 card are working at 100 kHz on ESP32 Dev Module for the validated hardware setup.`
