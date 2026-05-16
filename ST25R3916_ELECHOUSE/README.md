# ELECHOUSE ST25R3916 / ST25R3916B for ESP32

This library package contains the ESP32-oriented ST25R3916/ST25R3916B driver used by this repository.
It depends on the sibling `NFC-RFAL` library folder from the same repository.

Recommended ELECHOUSE product:

- [ST25R3916B NFC Module](https://www.elechouse.com/product/st25r3916b-nfc-module/)
- This is the recommended ELECHOUSE module for new ESP32 NFC/RFID projects using this library. It keeps the same ST25R3916 software-family workflow while targeting the newer ST25R3916B reader IC.
- The library remains compatible with existing ST25R3916 boards. Runtime chip-ID checks accept both ST25R3916 and ST25R3916B; hardware validation notes call out the exact chip variant used in each run.
- Treat the ST25R3916B quick-start path as `SPI` first; the `I2C` examples in this repository are additional validated code paths after board configuration for alternate hardware setups.

## Scope

- Supported focus: classic `ESP32 Dev Module`, `ESP32-S3`, and `ESP32-C3` example bring-up
- Primary transport: `SPI`
- Bring-up transport: `I2C` (first-pass ESP32 hardware-validated for chip probe, `ISO14443A`, S70 read/auth/dump diagnostics, `ISO15693` `ICODE SLIX2` read/write/restore, ESP32-C3 `ISO15693 / Type 5` NDEF write/read/restore, and `ISO14443B / Type 4B` NDEF write/read/restore)
- ESP32-C3 target used for validation: `esp32:esp32:esp32c3` (`ESP32C3 Dev Module`) on Arduino-ESP32 core `3.3.8`, with `FlashMode=dio`

## Default example wiring

Classic ESP32 SPI:

- `SCK` -> `GPIO18`
- `MISO` -> `GPIO19`
- `MOSI` -> `GPIO23`
- `SS` -> `GPIO5`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional)

ESP32-S3 SPI:

- `SCK` -> `GPIO12`
- `MISO` -> `GPIO13`
- `MOSI` -> `GPIO11`
- `SS` -> `GPIO10`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional)

ESP32-C3 SPI:

- `SCK` -> `GPIO2`
- `MISO` -> `GPIO10`
- `MOSI` -> `GPIO3`
- `SS` -> `GPIO7`
- `IRQ` -> `GPIO6`
- `LED` -> `GPIO12` (optional)

ESP32-C3 I2C:

- `SDA` -> `GPIO4`
- `SCL` -> `GPIO5`
- `IRQ` -> `GPIO6`
- `LED` -> `GPIO12` (optional)

## Included examples

SPI examples under `examples/SPI/`:

- `ESP32_SPI_scan_14443A_led`
- `ESP32_SPI_scan_14443AB_15693`
- `ESP32_SPI_polling_hotplug`
- `ESP32_SPI_card_profile`
- `ESP32_SPI_iso14443b_ndef_write_test`
- `ESP32_SPI_icode_slix2_read_write_test`
- `ESP32_SPI_ndef_write_read_restore`
- `ESP32_SPI_mf1_s70_read_write_test`
- `ESP32_SPI_mf1_s70_sector_range_dump`
- `ESP32_SPI_mf1_s70_serial_tool`

I2C examples under `examples/I2C/`:

- `ESP32_I2C_probe_chip`
- `ESP32_I2C_scan_14443A`
- `ESP32_I2C_scan_14443AB_15693`
- `ESP32_I2C_polling_hotplug`
- `ESP32_I2C_t2t_write_read_test`
- `ESP32_I2C_mf1_s70_read_write_test`
- `ESP32_I2C_mf1_s70_sector_range_dump`
- `ESP32_I2C_mf1_s70_serial_tool`
- `ESP32_I2C_card_profile`
- `ESP32_I2C_icode_slix2_read_write_test`
- `ESP32_I2C_ndef_write_read_restore`
- `ESP32_I2C_iso14443b_ndef_write_test`

## Notes

- SPI examples declare their bus and pin constants near the top of each `.ino`; ESP32-C3 uses `SPIClass(FSPI)`.
- I2C examples declare `kPinSda`, `kPinScl`, `kPinIrq`, `kPinLed`, and `kI2cClockHz` near the top of each `.ino`.
- I2C examples default to `100kHz`; the guarded S70 write-test sketch stays read/auth-only unless `kEnableWriteTest=true` is set manually.
- I2C on ESP32 now drains IRQs in normal context instead of doing `Wire` transactions in the hardware ISR.
- `ESP32_SPI_iso14443b_ndef_write_test` is hardware-validated on `ESP32 Dev Module` with a writable `ISO14443B / Type 4B` card.
- `ESP32_SPI_icode_slix2_read_write_test` is hardware-validated on `ESP32 Dev Module` with a current `NXP ICODE` `ISO15693` card.
- `ESP32_SPI_ndef_write_read_restore` is hardware-validated on `ESP32 Dev Module` with a writable `ISO14443A / Type 2` card.
- `ESP32_I2C_probe_chip`, `ESP32_I2C_scan_14443A`, `ESP32_I2C_scan_14443AB_15693`, `ESP32_I2C_t2t_write_read_test` (safe rejection path on a `MIFARE Classic` card), `ESP32_I2C_mf1_s70_read_write_test` in default read/auth mode, `ESP32_I2C_mf1_s70_sector_range_dump`, `ESP32_I2C_mf1_s70_serial_tool`, `ESP32_I2C_icode_slix2_read_write_test`, and `ESP32_I2C_iso14443b_ndef_write_test` are hardware-validated on `ESP32 Dev Module`.
- `ESP32_I2C_probe_chip` and `ESP32_I2C_mf1_s70_read_write_test` read/auth mode are also hardware-validated on `ESP32-C3` with an ST25R3916 I2C board and an S70 card.
- `ESP32_I2C_card_profile`, `ESP32_I2C_scan_14443AB_15693`, `ESP32_I2C_icode_slix2_read_write_test`, and `ESP32_I2C_ndef_write_read_restore` are also hardware-validated on `ESP32-C3` with the same ST25R3916 I2C board and an `NXP ICODE SLIX2` / `ISO15693` card.
- SPI tuning and debug macros live in `src/st25r3916_config.h`.
- Upstream updates are applied selectively; this library is not a mirror of the stm32duino branch.
- Deprecated or local-only example material is intentionally kept out of this tracked library folder.
