# ST25R3916 for ESP32

This repository is an ESP32-focused fork of the ST25R3916 + NFC-RFAL Arduino libraries.
The current maintenance target is `ESP32 Dev Module` over `SPI`. I2C basic bring-up is also validated in this branch for chip probe and `ISO14443A` scanning at `100 kHz`.

`ST25R3916_ELECHOUSE` examples still depend on the sibling `NFC-RFAL` library.

## Repository layout

- `ST25R3916_ELECHOUSE/`
  - ST25R3916/ST25R3916B Arduino library.
- `NFC-RFAL/`
  - RFAL dependency used by the ST25R3916 library.
- `ST25R3916_ELECHOUSE/examples/SPI/`
  - Curated ESP32 SPI examples.
- `ST25R3916_ELECHOUSE/examples/I2C/`
  - Curated ESP32 I2C examples.
- `docs/`
  - ESP32 SPI baseline and upstream sync policy for this fork.

## Maintenance policy

- This fork is maintained for ESP32 first.
- SPI is the primary and regression-protected path.
- I2C is developed as a separate bring-up track and currently has first-pass ESP32 validation only.
- Upstream changes are pulled selectively as small backports instead of broad version jumps.
- Compatibility with stm32duino release cadence is not guaranteed.

## Arduino usage

Install both library folders into your Arduino libraries path:

1. `NFC-RFAL`
2. `ST25R3916_ELECHOUSE`

Use `esp32:esp32:esp32` (`ESP32 Dev Module`) as the target board.

### Default SPI wiring

- `SCK` -> `GPIO18`
- `MISO` -> `GPIO19`
- `MOSI` -> `GPIO23`
- `SS` -> `GPIO5`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional status LED)

### Current SPI examples

- `ESP32_SPI_scan_14443AB_15693`
  - Multi-protocol discovery baseline for A/B/V.
- `ESP32_SPI_polling_hotplug`
  - Continuous polling with insert/remove reporting.
- `ESP32_SPI_card_profile`
  - One-shot card profiling for protocol/subtype/NDEF visibility.
- `ESP32_SPI_mf1_s70_read_write_test`
  - Safe MIFARE Classic S70 block write/read/restore test with default key flow.
- `ESP32_SPI_mf1_s70_sector_dump`
  - Read-only dump of one MIFARE Classic sector.
- `ESP32_SPI_mf1_s70_sector_range_dump`
  - Read-only dump across a sector range with trailer skipping.
- `ESP32_SPI_mf1_s70_serial_tool`
  - Interactive serial tool for read-only MF1 block/sector dump and export.
  Path: `ST25R3916_ELECHOUSE/examples/SPI/`

### Default I2C wiring

- `SDA` -> `GPIO21`
- `SCL` -> `GPIO22`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional status LED)
- I2C clock: `100 kHz` default bring-up target

### Current I2C bring-up examples

- `ESP32_I2C_probe_chip`
  - Raw ACK + chip-ID + `rfalInitialize()` probe for the minimum hardware path. Hardware-validated.
- `ESP32_I2C_scan_14443A`
  - First end-to-end I2C card detection example. Hardware-validated with one `ISO14443A` card.
- `ESP32_I2C_scan_14443A_15693`
  - Extended I2C scan example after the basic path is stable. Validated with the current `ISO14443A` card; `ISO15693` still needs a hardware card result.
- `ESP32_I2C_polling_hotplug`
  - Continuous I2C polling with insert/remove reporting. Intended to replace repeated UID spam during long-running scan demos.
- `ESP32_I2C_mf1_s70_read_write_test`
  - Safe MIFARE Classic S70 block write/read/restore test over I2C with default Key A.
  Path: `ST25R3916_ELECHOUSE/examples/I2C/`

## Notes

- SPI examples use `SPIClass(VSPI)`.
- I2C examples assume the caller wants the library sketches to own `Wire.begin(...)` with the default ESP32 pins shown above.
- I2C on ESP32 now avoids doing `Wire` transactions in the hardware ISR path; IRQs are drained in normal context.
- Optional serial debug logging is controlled through `ST25R3916_ENABLE_DEBUG_LOG` in `ST25R3916_ELECHOUSE/src/st25r3916_config.h`.
- Local scratch files, migration plans, and deprecated examples are kept under `.local_only/` and are not uploaded to GitHub.
