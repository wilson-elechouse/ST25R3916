# ST25R3916 for ESP32

This repository is an ESP32-focused fork of the ST25R3916 + NFC-RFAL Arduino libraries.
The current maintenance target is `ESP32 Dev Module` over `SPI`. I2C remains a later stage and is not claimed as supported in this branch.

## Repository layout

- `ST25R3916_110/`
  - ST25R3916/ST25R3916B Arduino library.
- `NFC-RFAL/`
  - RFAL dependency used by the ST25R3916 library.
- `ST25R3916_110/examples/`
  - Curated ESP32 SPI examples for bring-up, scanning, hotplug polling, and chip debug.
- `ESP32_ST25R3916_reading_14443AB_15693/`
  - Top-level compatibility sketch that mirrors the multi-protocol SPI example.
- `docs/`
  - ESP32 SPI baseline and upstream sync policy for this fork.

## Maintenance policy

- This fork is maintained for ESP32 first.
- SPI is the primary and regression-protected path.
- I2C is deferred to a separate validation phase.
- Upstream changes are pulled selectively as small backports instead of broad version jumps.
- Compatibility with stm32duino release cadence is not guaranteed.

## Arduino usage

Install both library folders into your Arduino libraries path:

1. `NFC-RFAL`
2. `ST25R3916_110`

Use `esp32:esp32:esp32` (`ESP32 Dev Module`) as the target board.

### Default SPI wiring

- `SCK` -> `GPIO18`
- `MISO` -> `GPIO19`
- `MOSI` -> `GPIO23`
- `SS` -> `GPIO5`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional status LED)

### Current SPI examples

- `ESP32_SPI_scan_14443A`
  - Minimal ISO14443A UID reader.
- `ESP32_SPI_scan_14443AB_15693`
  - Multi-protocol discovery baseline for A/B/V.
- `ESP32_SPI_polling_hotplug`
  - Continuous polling with insert/remove reporting.
- `ESP32_SPI_chip_info_debug`
  - Low-level chip/register sanity check.

## Notes

- `SPIClass(0)` is used in examples for Arduino-ESP32 core 3.x compatibility.
- Optional serial debug logging is controlled through `ST25R3916_ENABLE_DEBUG_LOG` in `ST25R3916_110/src/st25r3916_config.h`.
- The original archive is kept in the repository for reference, but the extracted source folders are the maintained path.
