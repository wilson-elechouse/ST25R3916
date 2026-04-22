# I2C Validation Results

## Current State

I2C work in this repository has now reached first-pass hardware validation for
ESP32 + ST25R3916 basic bring-up and `ISO14443A` scanning.

## Implemented I2C Artifacts

- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_probe_chip/ESP32_I2C_probe_chip.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_scan_14443A/ESP32_I2C_scan_14443A.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_scan_14443A_15693/ESP32_I2C_scan_14443A_15693.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_polling_hotplug/ESP32_I2C_polling_hotplug.ino`
- `ST25R3916_ELECHOUSE/examples/I2C/ESP32_I2C_mf1_s70_read_write_test/ESP32_I2C_mf1_s70_read_write_test.ino`
- low-level I2C transaction result checks in `st25r3916_com.cpp`
- I2C-safe init branching in `rfal_rfst25r3916.cpp`

## Compile Results

The current I2C examples compile for:

- board: `esp32:esp32:esp32`
- library layout: local `NFC-RFAL` + `ST25R3916_ELECHOUSE`

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

### `ESP32_I2C_polling_hotplug`

- initialization: passed
- insert detection with current `ISO14443A` card: passed
- observed line: `Card inserted: ISO14443A D9 A9 CE 70`
- result: I2C hotplug example no longer floods repeated UID lines during steady presence

### `ESP32_I2C_mf1_s70_read_write_test`

- target card: `MIFARE One S70 / MIFARE Classic 4K`
- default key: `FF FF FF FF FF FF`
- authentication to `block 4`: passed
- write/read/restore cycle on `block 4`: passed
- original block: `53 54 32 35 52 33 39 31 36 20 44 45 4D 4F 30 31`
- test pattern: `F6 F1 97 90 F7 96 9C 94 93 85 E1 E0 E8 EA 95 94`
- restore verification: passed
- result: MF1 helper path is working over I2C for the validated S70 card

## Known Limits

- no `ISO15693` card result has been recorded yet, so the `15693` path is still
  only partially validated
- `ESP32_I2C_scan_14443A` and `ESP32_I2C_scan_14443A_15693` intentionally
  deactivate after each activation, so a stationary card will be printed repeatedly
- `ESP32_I2C_polling_hotplug` is the preferred long-running demo when insert/remove
  behaviour matters
- `ESP32_I2C_mf1_s70_read_write_test` is validated only for the current default-key
  `MIFARE One S70` card and safe data block `4`
- I2C support should still be treated as first-pass ESP32 support, not broad
  cross-board closure

## Current Conclusion

The repository can now claim:

`ESP32 I2C bring-up, ISO14443A scanning, hotplug polling, and MF1 S70 block write/read/restore are working at 100 kHz on ESP32 Dev Module for the validated hardware setup.`
