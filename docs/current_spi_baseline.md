# Current SPI Baseline

## Repository baseline

- Local branch: `main`
- Remote sync state: maintained on `origin/main`

## Hardware baseline

- Board: `ESP32 Dev Module`
- Upload port: `COM3`
- Reader stack: `ST25R3916` + `NFC-RFAL`
- Bus: `SPI`

### Default wiring

- `SCK` -> `GPIO18`
- `MISO` -> `GPIO19`
- `MOSI` -> `GPIO23`
- `SS` -> `GPIO5`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional)

## Current SPI examples

- `ESP32_SPI_scan_14443AB_15693`
- `ESP32_SPI_polling_hotplug`
- `ESP32_SPI_card_profile`
- `ESP32_SPI_iso14443b_ndef_write_test`
- `ESP32_SPI_mf1_s70_read_write_test`
- `ESP32_SPI_mf1_s70_sector_range_dump`
- `ESP32_SPI_mf1_s70_serial_tool`
- `ESP32_SPI_t2t_write_read_test`

## Current expectations

- Arduino CLI: `1.2.2`
- Arduino target board: `ESP32 Dev Module`
- Arduino core: `esp32:esp32 3.3.0-cn`
- Arduino-ESP32 compatibility target: core `3.x` style `SPIClass(0)` constructor
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
