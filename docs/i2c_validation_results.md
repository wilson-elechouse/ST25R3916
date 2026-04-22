# I2C Validation Results

## Current State

I2C work in this repository has reached code and compile bring-up, but not
hardware validation closure.

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

## Default Bring-Up Assumptions

- board: `ESP32 Dev Module`
- `SDA = GPIO21`
- `SCL = GPIO22`
- `IRQ = GPIO4`
- I2C clock: `100 kHz`
- card priority for first validation: `ISO14443A`

## Hardware Validation

No known-good ESP32 + ST25R3916 I2C hardware result has been recorded yet in
this repository.

That means the current statement is still:

`I2C examples and low-level groundwork exist, but hardware validation is pending.`

## Next Required Result

The next acceptable validation record should capture:

- whether address `0x50` ACKs
- whether `IC_IDENTITY` can be read
- whether `rfalInitialize()` succeeds
- whether `ESP32_I2C_scan_14443A` can repeatedly detect one known card
- the exact ESP32 pins and clock used
