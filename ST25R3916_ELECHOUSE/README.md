# ELECHOUSE ST25R3916 for ESP32

This library package contains the ESP32-oriented ST25R3916/ST25R3916B driver used by this repository.
It depends on the sibling `NFC-RFAL` library folder from the same repository.

Related ELECHOUSE product:

- `https://www.elechouse.com/product/st25r3916_nfc_reader/`
- Treat the stock product path as `SPI` first; the `I2C` examples in this repository are additional validated code paths for alternate hardware setups.

## Scope

- Supported focus: `ESP32 Dev Module`
- Primary transport: `SPI`
- Bring-up transport: `I2C` (first-pass ESP32 hardware-validated for chip probe, `ISO14443A`, `ISO15693` `ICODE SLIX2` read/write/restore, and `ISO14443B / Type 4B` NDEF write/read/restore)

## Default example wiring

- `SCK` -> `GPIO18`
- `MISO` -> `GPIO19`
- `MOSI` -> `GPIO23`
- `SS` -> `GPIO5`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional)

## Included examples

SPI examples under `examples/SPI/`:

- `ESP32_SPI_scan_14443AB_15693`
- `ESP32_SPI_polling_hotplug`
- `ESP32_SPI_card_profile`
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

- SPI examples use `SPIClass(VSPI)`.
- I2C examples default to `SDA=21`, `SCL=22`, `IRQ=4`, `100kHz`.
- I2C on ESP32 now drains IRQs in normal context instead of doing `Wire` transactions in the hardware ISR.
- `ESP32_I2C_probe_chip`, `ESP32_I2C_scan_14443A`, `ESP32_I2C_scan_14443AB_15693`, `ESP32_I2C_t2t_write_read_test` (safe rejection path on a `MIFARE Classic` card), `ESP32_I2C_mf1_s70_sector_range_dump`, `ESP32_I2C_mf1_s70_serial_tool`, `ESP32_I2C_icode_slix2_read_write_test`, and `ESP32_I2C_iso14443b_ndef_write_test` are hardware-validated on `ESP32 Dev Module`.
- SPI tuning and debug macros live in `src/st25r3916_config.h`.
- Upstream updates are applied selectively; this library is not a mirror of the stm32duino branch.
- Deprecated or local-only example material is intentionally kept out of this tracked library folder.
