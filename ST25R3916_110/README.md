# ELECHOUSE ST25R3916 for ESP32

This library package contains the ESP32-oriented ST25R3916/ST25R3916B driver used by this repository.
It depends on the sibling `NFC-RFAL` library folder from the same repository.

## Scope

- Supported focus: `ESP32 Dev Module`
- Primary transport: `SPI`
- Bring-up transport: `I2C` (not yet hardware-validated)

## Default example wiring

- `SCK` -> `GPIO18`
- `MISO` -> `GPIO19`
- `MOSI` -> `GPIO23`
- `SS` -> `GPIO5`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional)

## Included examples

- `ESP32_SPI_scan_14443AB_15693`
- `ESP32_SPI_polling_hotplug`
- `ESP32_SPI_card_profile`
- `ESP32_SPI_mf1_s70_read_write_test`
- `ESP32_SPI_mf1_s70_sector_dump`
- `ESP32_SPI_mf1_s70_sector_range_dump`
- `ESP32_SPI_mf1_s70_serial_tool`
- `ESP32_I2C_probe_chip`
- `ESP32_I2C_scan_14443A`
- `ESP32_I2C_scan_14443A_15693`

## Notes

- SPI examples use `SPIClass(VSPI)`.
- I2C examples default to `SDA=21`, `SCL=22`, `IRQ=4`, `100kHz`.
- I2C examples are currently compile-tested but not yet validated on a known-good hardware setup.
- SPI tuning and debug macros live in `src/st25r3916_config.h`.
- Upstream updates are applied selectively; this library is not a mirror of the stm32duino branch.
