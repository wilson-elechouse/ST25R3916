# Current SPI Baseline

## Repository baseline

- Local branch: `main`
- Remote sync state: maintained on `origin/main`

## Hardware baseline

- Board: `ESP32 Dev Module`
- Upload port: `COM3`
- Reader stack: `ST25R3916` + `NFC-RFAL`
- Bus: `SPI`

### Classic ESP32 default wiring

- `SCK` -> `GPIO18`
- `MISO` -> `GPIO19`
- `MOSI` -> `GPIO23`
- `SS` -> `GPIO5`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional)

### ESP32-S3 bring-up wiring

- Board: `ESP32-S3-N16R8`
- Arduino target board: `esp32:esp32:esp32s3` (`ESP32S3 Dev Module`)
- `SCK` -> `GPIO12`
- `MISO` -> `GPIO13`
- `MOSI` -> `GPIO11`
- `SS` -> `GPIO10`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional)
- SPI bus: `FSPI`
- Initial validation card: `MIFARE One S70 / MIFARE Classic 4K`

### ESP32-C3 bring-up wiring

- Board: `ESP32-C3` hardware using the official generic Arduino target
- Arduino target board: `esp32:esp32:esp32c3` (`ESP32C3 Dev Module`)
- `SCK` -> `GPIO2`
- `MISO` -> `GPIO10`
- `MOSI` -> `GPIO3`
- `SS` -> `GPIO7`
- `IRQ` -> `GPIO6`
- `LED` -> `GPIO12` (optional)
- SPI bus: `FSPI`
- Board option used for validation: `FlashMode=dio`
- Validation cards: `NXP ICODE SLIX2 / ISO15693`, `MIFARE One S70 / MIFARE Classic 4K`

## Current SPI examples

- `ESP32_SPI_scan_14443AB_15693`
- `ESP32_SPI_polling_hotplug`
- `ESP32_SPI_card_profile`
- `ESP32_SPI_iso14443b_ndef_write_test`
- `ESP32_SPI_icode_slix2_read_write_test`
- `ESP32_SPI_ndef_write_read_restore`
- `ESP32_SPI_mf1_s70_read_write_test`
- `ESP32_SPI_mf1_s70_sector_range_dump`
- `ESP32_SPI_mf1_s70_serial_tool`
- `ESP32_SPI_t2t_write_read_test`

## Current expectations

- Arduino CLI: `1.2.2`
- Arduino target board: `ESP32 Dev Module`
- Arduino core: `esp32:esp32 3.3.8`
- Arduino-ESP32 compatibility target: core `3.x` style `SPIClass(...)` constructor
- SPI clock default: `5 MHz`
- IRQ mode: rising-edge interrupt on the `IRQ` pin

## Validated card coverage target

- ISO14443A
- ISO14443B
- ISO15693

## Known limits

- I2C is not part of the validated baseline yet.
- Correct `IRQ` wiring is required for stable continuous polling.
- Debug logging is off by default and must be enabled in `st25r3916_config.h` if needed.

## Latest validation snapshot

- Library examples compiled successfully with `arduino-cli` for `esp32:esp32:esp32`.
- Upload to `COM3` succeeded on an `ESP32-D0WDQ6` module.
- `ESP32_SPI_iso14443b_ndef_write_test` was hardware-validated on a writable `ISO14443B / Type 4B` card.
- Observed `PUPI`: `17 0E 0D 64`
- Original raw NDEF: `D1 01 0B 55 01 6F 6B 65 64 64 79 2E 63 6F 6D`
- Test raw NDEF: `D1 01 0D 54 02 65 6E 42 34 20 54 45 53 54 20 30 31`
- Write/read/restore cycle completed with `ERR_NONE` throughout.
- `ESP32_SPI_icode_slix2_read_write_test` was hardware-validated on a current `NXP ICODE` `ISO15693` card.
- Observed UID: `67 A6 D6 07 00 02 04 E0`
- Observed IC reference: `0x02`
- Observed block count and size: `40` blocks, `4` bytes per block
- Original block `8`: `00 00 00 00`
- Test pattern: `5A 6B 7C 8D`
- Write/read/restore cycle completed with `ERR_NONE` throughout.
- `ESP32_SPI_ndef_write_read_restore` was hardware-validated on a writable `ISO14443A / Type 2` NDEF card.
- Observed UID: `04 3F 45 32 F3 1C 90`
- Original raw NDEF: `D1 01 0B 55 01 6F 6B 65 64 64 79 2E 63 6F 6D`
- Test raw NDEF: `D1 01 12 54 02 65 6E 45 4C 45 43 48 4F 55 53 45 20 4E 44 45 46 20 31 00`
- Write/read/restore cycle completed with `ERR_NONE` throughout.

## ESP32-S3 validation snapshot

- Date: `2026-05-15`
- Board: `ESP32-S3-N16R8`
- Initial upload port: `COM3` (`USB-SERIAL CH340`)
- Arduino target: `esp32:esp32:esp32s3`
- Board options used for validation: `FlashSize=16M`, `PSRAM=opi`, `USBMode=hwcdc`, `CDCOnBoot=default`, `UploadMode=default`, `FlashMode=qio`, `PartitionScheme=app3M_fat9M_16MB`
- `ESP32_SPI_card_profile` compiled and uploaded successfully over COM3.
- `ESP32_SPI_mf1_s70_read_write_test` compiled, uploaded, and completed a default-key block write/read/restore cycle on the same ESP32-S3 target.
- Observed card type: `ISO14443A`
- Observed UID: `3B 58 C0 38`
- Observed ATQA: `02 00`
- Observed SAK: `18`
- Card interpretation: likely `MIFARE Classic 4K / MIFARE One S70` compatible.
- NDEF detection result: `ERR_PROTO`, expected for this proprietary MIFARE Classic profile.
- MF1 test block: `4`
- MF1 original block: `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00`
- MF1 test pattern: `A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5`
- MF1 write/read/restore cycle completed with `ERR_NONE` throughout.

## ESP32-S3 ISO15693 validation snapshot

- Date: `2026-05-15`
- Board: `ESP32-S3-N16R8`
- Upload port: `COM7` (`USB-SERIAL CH340`)
- Arduino target: `esp32:esp32:esp32s3`
- Board options used for validation: `FlashSize=16M`, `PSRAM=opi`, `USBMode=hwcdc`, `CDCOnBoot=default`, `UploadMode=default`, `FlashMode=qio`, `PartitionScheme=app3M_fat9M_16MB`
- ESP32-S3 SPI examples compiled successfully: `ESP32_SPI_scan_14443AB_15693`, `ESP32_SPI_polling_hotplug`, `ESP32_SPI_card_profile`, `ESP32_SPI_icode_slix2_read_write_test`, `ESP32_SPI_ndef_write_read_restore`.
- `ESP32_SPI_card_profile` compiled and uploaded successfully over COM7.
- Observed card type: `ISO15693`
- Observed UID: `50 0A 3F 10 53 01 04 E0`
- NDEF state: `READWRITE`
- NDEF area length: `104`
- NDEF available space: `102`
- Original raw NDEF: `D1 01 0B 55 01 6F 6B 65 64 64 79 2E 63 6F 6D`
- `ESP32_SPI_icode_slix2_read_write_test` completed a single-block write/read/restore cycle.
- ISO15693 block count and size: `28` blocks, `4` bytes per block
- ISO15693 test block: `8`
- ISO15693 original block `8`: `00 00 00 00`
- ISO15693 test pattern: `5A 6B 7C 8D`
- ISO15693 write/read/restore cycle completed with `ERR_NONE` throughout.
- `ESP32_SPI_ndef_write_read_restore` completed a Type 5 NDEF write/read/restore cycle.
- Test raw NDEF: `D1 01 12 54 02 65 6E 45 4C 45 43 48 4F 55 53 45 20 4E 44 45 46 20 31 00`
- Type 5 NDEF write/read/restore cycle completed with `ERR_NONE` throughout.

## ESP32-C3 ISO15693 validation snapshot

- Date: `2026-05-16`
- Board: `ESP32-C3` hardware using the official generic Arduino target
- Upload port: `COM8`
- Arduino target: `esp32:esp32:esp32c3`
- Arduino-ESP32 core: `3.3.8`
- Board option used for validation: `FlashMode=dio`
- Reader board used in this validation run: ST25R3916. ST25R3916B remains a supported ST25R3916-family target, but it was not the board attached for this run.
- SPI wiring: `SCK=GPIO2`, `MISO=GPIO10`, `MOSI=GPIO3`, `SS=GPIO7`, `IRQ=GPIO6`
- Optional LED default for C3 examples: `GPIO12`
- `ESP32_SPI_card_profile`: detected `ISO15693`, UID `FA F3 E3 06 09 01 04 E0`, RF interface `RF`, NDEF state `READWRITE`
- NDEF area length: `312`
- NDEF available space: `310`
- Original raw NDEF: `D1 01 0B 55 01 6F 6B 65 64 64 79 2E 63 6F 6D`
- `ESP32_SPI_scan_14443AB_15693`: repeatedly detected `ISO15693 ID: FA F3 E3 06 09 01 04 E0`
- `ESP32_SPI_icode_slix2_read_write_test`: completed a single-block write/read/restore cycle
- ISO15693 system info: manufacturer `NXP`, IC reference `0x01`, `80` blocks, `4` bytes per block
- ISO15693 test block: `8`
- ISO15693 original block `8`: `00 00 00 00`
- ISO15693 test pattern: `5A 6B 7C 8D`
- ISO15693 block restore verification: passed
- `ESP32_SPI_ndef_write_read_restore`: completed a Type 5 NDEF write/read/restore cycle
- Test raw NDEF: `D1 01 12 54 02 65 6E 45 4C 45 43 48 4F 55 53 45 20 4E 44 45 46 20 31 00`
- Type 5 NDEF restore verification: passed; original 15-byte NDEF message was restored

## ESP32-C3 S70 validation snapshot

- Date: `2026-05-16`
- Board: `ESP32-C3` hardware using the official generic Arduino target
- Upload port: `COM8`
- Arduino target: `esp32:esp32:esp32c3`
- Arduino-ESP32 core: `3.3.8`
- Board option used for validation: `FlashMode=dio`
- Reader board used in this validation run: ST25R3916. ST25R3916B remains a supported ST25R3916-family target, but it was not the board attached for this run.
- SPI wiring: `SCK=GPIO2`, `MISO=GPIO10`, `MOSI=GPIO3`, `SS=GPIO7`, `IRQ=GPIO6`
- Optional LED default for C3 examples: `GPIO12`
- `ESP32_SPI_card_profile`: detected `ISO14443A`, UID `3B 58 C0 38`, ATQA `02 00`, SAK `18`
- Card interpretation: likely `MIFARE Classic 4K / MIFARE One S70` compatible
- NDEF detection result: `ERR_PROTO`, expected for this proprietary MIFARE Classic card type
- `ESP32_SPI_mf1_s70_read_write_test`: default Key A authentication passed on block `4`
- Original block `4`: `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00`
- MF1 test pattern: `A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5 A5`
- MF1 block write/read/restore cycle: passed with `ERR_NONE` throughout
- Block `4` after restore: `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00`
- `ESP32_SPI_mf1_s70_sector_range_dump`: sectors `1-2` authenticated and read successfully with default Key A
- Sector dump data blocks read successfully: `4`, `5`, `6`, `8`, `9`, `10`
- Trailer blocks skipped as intended: `7`, `11`
