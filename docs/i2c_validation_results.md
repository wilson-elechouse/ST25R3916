# I2C Validation Results

## Current State

I2C work in this repository has now reached first-pass hardware validation for
ESP32 + ST25R3916 basic bring-up and `ISO14443A` scanning.

## Implemented I2C Artifacts

- `ST25R3916_110/examples/ESP32_I2C_probe_chip/ESP32_I2C_probe_chip.ino`
- `ST25R3916_110/examples/ESP32_I2C_scan_14443A/ESP32_I2C_scan_14443A.ino`
- `ST25R3916_110/examples/ESP32_I2C_scan_14443A_15693/ESP32_I2C_scan_14443A_15693.ino`
- low-level I2C transaction result checks in `st25r3916_com.cpp`
- I2C-safe init branching in `rfal_rfst25r3916.cpp`

## Compile Results

The current I2C examples compile for:

- board: `esp32:esp32:esp32`
- library layout: local `NFC-RFAL` + `ST25R3916_110`

## Validated Hardware Setup

- board: `ESP32 Dev Module`
- `SDA = GPIO21`
- `SCL = GPIO22`
- `IRQ = GPIO4` (`D4`)
- I2C clock: `100 kHz`
- validated card: `MIFARE One S70`
- validated UID: `D9 A9 CE 70`

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

### `ESP32_I2C_scan_14443A_15693`

- initialization: passed
- multi-tech loop with current `ISO14443A` card: passed
- observed UID: `D9 A9 CE 70`
- result: extended I2C scan remains stable with `A + V` polling enabled

## Known Limits

- no `ISO15693` card result has been recorded yet, so the `15693` path is still
  only partially validated
- the current scan examples intentionally deactivate after each activation, so
  a stationary card will be printed repeatedly
- I2C support should still be treated as first-pass ESP32 support, not broad
  cross-board closure

## Current Conclusion

The repository can now claim:

`ESP32 I2C basic bring-up is working for ST25R3916 chip probe and ISO14443A scanning at 100 kHz on ESP32 Dev Module.`
