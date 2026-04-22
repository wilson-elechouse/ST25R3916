# ELECHOUSE ST25R3916 for ESP32

This library package contains the ESP32-oriented ST25R3916/ST25R3916B driver used by this repository.
It depends on the sibling `NFC-RFAL` library folder from the same repository.

## Scope

- Supported focus: `ESP32 Dev Module`
- Primary transport: `SPI`
- Deferred transport: `I2C`

## Default example wiring

- `SCK` -> `GPIO18`
- `MISO` -> `GPIO19`
- `MOSI` -> `GPIO23`
- `SS` -> `GPIO5`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional)

## Included examples

- `ESP32_SPI_scan_14443A`
- `ESP32_SPI_scan_14443AB_15693`
- `ESP32_SPI_polling_hotplug`
- `ESP32_SPI_chip_info_debug`

## Notes

- `SPIClass(0)` is used instead of `VSPI` to stay compatible with Arduino-ESP32 core 3.x.
- SPI tuning and debug macros live in `src/st25r3916_config.h`.
- Upstream updates are applied selectively; this library is not a mirror of the stm32duino branch.
