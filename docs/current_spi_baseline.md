# Current SPI Baseline

## Repository baseline

- Local branch: `main`
- Remote sync state: fast-forwarded to `origin/main`
- Synced commit: `476b1c5`

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

- `ESP32_SPI_scan_14443A`
- `ESP32_SPI_scan_14443AB_15693`
- `ESP32_SPI_polling_hotplug`
- `ESP32_SPI_chip_info_debug`

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
- Top-level compatibility sketch also compiled successfully.
- Upload to `COM3` succeeded on an `ESP32-D0WDQ6` module.
- A minimal on-device SPI register probe returned `chipId=0x00` repeatedly.
- Because the reader IC did not return a valid identity value, card-type validation is still blocked by hardware communication.
