# ST25R3916 / ST25R3916B for ESP32

This repository is an ESP32-focused fork of the ST25R3916/ST25R3916B + NFC-RFAL Arduino libraries.
The current maintenance targets are classic `ESP32 Dev Module`, `ESP32-S3`, and `ESP32-C3` example bring-up. SPI is validated on ESP32-C3 for `ISO15693` profile, A/B/V scanning, ICODE block write/read/restore, and `ISO15693 / Type 5` NDEF write/read/restore. I2C bring-up is also validated in this branch for chip probe, `ISO14443A` scanning, S70 read/auth/dump diagnostics, dedicated `ISO15693` block read/write/restore on `NXP ICODE SLIX2`, `ISO15693 / Type 5` NDEF write/read/restore on ESP32-C3, and `ISO14443B / Type 4B` NDEF write/read/restore at `100 kHz`.

`ST25R3916_ELECHOUSE` examples still depend on the sibling `NFC-RFAL` library.

## Recommended Hardware

- Recommended module for new ESP32 NFC/RFID projects: [ELECHOUSE ST25R3916B NFC Module](https://www.elechouse.com/product/st25r3916b-nfc-module/).
- Choose the ST25R3916B module for new designs that need a newer ST25R3916-family reader IC, ISO 14443A/B, ISO 15693, FeliCa, NFC Forum tag support, and practical compatibility with this ELECHOUSE ST25R3916 software stack.
- Existing ST25R3916 boards remain supported. The driver accepts both ST25R3916 and ST25R3916B chip identities during initialization; validation notes below list the exact reader IC used in each hardware run.
- For first bring-up with the ST25R3916B module, use the `SPI` examples first; SPI is the stock and most regression-protected path in this repository.
- The `I2C` examples are also available after configuring the board for I2C mode and are maintained as an additional validated path for custom or alternate hardware setups.

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
  - ESP32 SPI/I2C validation notes and maintenance records kept in GitHub for repository users and maintainers.

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

Use `esp32:esp32:esp32` (`ESP32 Dev Module`) as the classic ESP32 target board.
For `ESP32-S3-N16R8`, use `esp32:esp32:esp32s3` (`ESP32S3 Dev Module`) with `FlashSize=16M` and `PSRAM=opi`.
On CH340-based ESP32-S3 boards, keep `CDCOnBoot=default` so `Serial` logs stay on the CH340 COM port.
For ESP32-C3 boards, use the official generic target `esp32:esp32:esp32c3` (`ESP32C3 Dev Module`). This repository was checked with Arduino-ESP32 core `3.3.8`; the validated C3 board required `FlashMode=dio`.

### Default SPI wiring

Classic ESP32 default:

- `SCK` -> `GPIO18`
- `MISO` -> `GPIO19`
- `MOSI` -> `GPIO23`
- `SS` -> `GPIO5`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional status LED)

ESP32-S3 default:

- `SCK` -> `GPIO12`
- `MISO` -> `GPIO13`
- `MOSI` -> `GPIO11`
- `SS` -> `GPIO10`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional status LED)

ESP32-C3 default:

- `SCK` -> `GPIO2`
- `MISO` -> `GPIO10`
- `MOSI` -> `GPIO3`
- `SS` -> `GPIO7`
- `IRQ` -> `GPIO6`
- `LED` -> `GPIO12` (optional status LED)

### Current SPI examples

- `ESP32_SPI_scan_14443A_led`
  - ISO14443A-only continuous polling test. Prints the UID repeatedly while a card is present and blinks `GPIO2` / D2.
- `ESP32_SPI_scan_14443AB_15693`
  - Multi-protocol discovery baseline for A/B/V. Rerun on ESP32-C3 with an `ISO15693` card.
- `ESP32_SPI_polling_hotplug`
  - Continuous polling with insert/remove reporting.
- `ESP32_SPI_card_profile`
  - One-shot card profiling for protocol/subtype/NDEF visibility.
- `ESP32_SPI_iso14443b_ndef_write_test`
  - Safe `ISO14443-4B / Type 4B` NDEF write/read/restore test over SPI, hardware-validated on a writable `ISO14443B` card.
- `ESP32_SPI_icode_slix2_read_write_test`
  - Safe `ISO15693 / NFC-V` single-block write/read/restore test over SPI, hardware-validated on a current `NXP ICODE` card, including ESP32-C3.
- `ESP32_SPI_ndef_write_read_restore`
  - Generic NDEF write/read/restore test over SPI, hardware-validated on a writable `ISO14443A / Type 2` card and on ESP32-C3 with the current `ISO15693 / Type 5` card.
- `ESP32_SPI_mf1_s70_read_write_test`
  - Safe MIFARE Classic S70 block write/read/restore test with default key flow.
- `ESP32_SPI_mf1_s70_sector_range_dump`
  - Read-only dump across a sector range with trailer skipping.
- `ESP32_SPI_mf1_s70_serial_tool`
  - Interactive serial tool for read-only MF1 block/sector dump and export.
  Path: `ST25R3916_ELECHOUSE/examples/SPI/`

### Default I2C wiring

Classic ESP32 default:

- `SDA` -> `GPIO21`
- `SCL` -> `GPIO22`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional status LED)
- I2C clock: `100 kHz` default bring-up target

ESP32-S3 default:

- `SDA` -> `GPIO8`
- `SCL` -> `GPIO9`
- `IRQ` -> `GPIO4`
- `LED` -> `GPIO2` (optional status LED)
- I2C clock: `100 kHz` default bring-up target

ESP32-C3 default:

- `SDA` -> `GPIO4`
- `SCL` -> `GPIO5`
- `IRQ` -> `GPIO6`
- `LED` -> `GPIO12` (optional status LED)
- I2C clock: `100 kHz` default bring-up target

### Current I2C bring-up examples

- `ESP32_I2C_probe_chip`
  - Raw ACK + chip-ID + `rfalInitialize()` probe for the minimum hardware path. Hardware-validated.
- `ESP32_I2C_scan_14443A`
  - First end-to-end I2C card detection example. Hardware-validated with one `ISO14443A` card.
- `ESP32_I2C_scan_14443AB_15693`
  - Multi-protocol I2C discovery baseline for A/B/V. Hardware-validated with the current `ISO14443A` card and rerun on ESP32-C3 with an `ISO15693` card.
- `ESP32_I2C_polling_hotplug`
  - Continuous I2C polling with insert/remove reporting. Intended to replace repeated UID spam during long-running scan demos.
- `ESP32_I2C_t2t_write_read_test`
  - Safe raw Type 2 Tag page write/read/restore test over I2C. Also rejects non-T2T cards such as `MIFARE Classic`.
- `ESP32_I2C_mf1_s70_read_write_test`
  - MIFARE Classic S70 block read/auth test over I2C with default Key A. The write portion is guarded by `kEnableWriteTest=false`; set it to `true` only on a sacrificial card/block. ESP32-C3 defaults to `100 kHz`; raise `kI2cClockHz` manually only after validating the module and wiring at Fast-mode.
- `ESP32_I2C_mf1_s70_sector_range_dump`
  - Read-only dump across a MIFARE Classic sector range over I2C with trailer skipping.
- `ESP32_I2C_mf1_s70_serial_tool`
  - Interactive serial tool for read-only MF1 block/sector dump and export over I2C.
- `ESP32_I2C_card_profile`
  - One-shot card profiling for protocol, `ISO-DEP`, and NDEF visibility over I2C.
- `ESP32_I2C_icode_slix2_read_write_test`
  - Safe ISO15693 single-block write/read/restore test over I2C, hardware-validated on `NXP ICODE SLIX2`, including ESP32-C3 at `100 kHz`.
- `ESP32_I2C_ndef_write_read_restore`
  - Generic NDEF write/read/restore test over I2C for standard writable `Type 2 / Type 4 / Type 5` cards. ESP32-C3 is validated with the current `ISO15693 / Type 5` card.
- `ESP32_I2C_iso14443b_ndef_write_test`
  - Safe `ISO14443-4B / Type 4B` NDEF write/read/restore test over I2C, hardware-validated on a writable `ISO14443B` card.
  Path: `ST25R3916_ELECHOUSE/examples/I2C/`

## Notes

- SPI examples declare their SPI bus and pin numbers near the top of each `.ino` file. Edit `kSpiBus`, `kPinSck`, `kPinMiso`, `kPinMosi`, `kPinSs`, `kPinIrq`, and `kPinLed` directly in the example when using custom wiring.
- I2C examples also declare their pin numbers near the top of each `.ino` file. Edit `kPinSda`, `kPinScl`, `kPinIrq`, `kPinLed`, and `kI2cClockHz` directly in the example when using custom wiring.
- I2C examples assume the caller wants the library sketches to own `Wire.begin(...)` with the default ESP32 pins shown above.
- I2C on ESP32 now avoids doing `Wire` transactions in the hardware ISR path; IRQs are drained in normal context.
- Optional serial debug logging is controlled through `ST25R3916_ENABLE_DEBUG_LOG` in `ST25R3916_ELECHOUSE/src/st25r3916_config.h`.
- The `docs/` folder should stay in GitHub because it records the validated SPI/I2C baseline, limits, and maintenance decisions for this fork.
- Local scratch files, migration plans, and deprecated examples are kept under `.local_only/` and are not uploaded to GitHub.
