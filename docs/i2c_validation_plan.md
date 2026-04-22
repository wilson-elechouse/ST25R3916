# I2C Validation Plan

## Scope

This document defines the validation sequence for the future ESP32 I2C path.
It is a plan, not a record of completed validation.

## Validation Principles

1. SPI remains the regression gate.
2. I2C validation starts with the smallest possible bring-up path.
3. No I2C support claim is made until examples, docs, and hardware results all exist.
4. `ISO14443A` is the first target. `ISO15693` follows after basic bring-up is stable.

## Pre-Validation Requirements

Before hardware validation starts, the repository should have:

- I2C low-level error checking for all active transaction paths
- I2C init isolated from SPI-only assumptions
- a basic `ESP32_I2C_probe_chip` example
- a basic `ESP32_I2C_scan_14443A` example
- an extended `ESP32_I2C_scan_14443A_15693` example
- documented I2C wiring for the validated ESP32 board

## Hardware To Record During Validation

Each validation run should record:

- ESP32 board type
- Arduino core version
- ST25R3916 board variant
- SDA pin
- SCL pin
- IRQ pin
- I2C clock used
- card types used for validation

Suggested clock order:

1. `100 kHz` first
2. `400 kHz` only after `100 kHz` is stable

## Phase 0: SPI Regression Gate

Every I2C validation pass should first confirm that SPI was not broken.

Minimum checks:

- compile the main SPI example
- compile the SPI 14443A example set
- compile the SPI multi-protocol example
- compile the SPI hotplug example

If available on hardware:

- confirm the current SPI baseline can still read the known-good card

## Phase 1: I2C Bring-Up Probe

Goal:

- prove that the ESP32 can talk to the ST25R3916 over I2C at all

Expected test behavior:

- initialize `Wire`
- initialize the reader object through the I2C constructor
- read chip identity
- clear interrupts
- bring oscillator up

Pass criteria:

- no silent failure
- no deadlock
- chip identity is valid
- transport failures return explicit errors

## Phase 2: Basic I2C Example

Target example:

- `examples/ESP32_I2C_scan_14443A/ESP32_I2C_scan_14443A.ino`

Goal:

- verify the minimal user path for I2C + card detection

Pass criteria:

- sketch compiles for `esp32:esp32:esp32`
- reader initializes successfully
- one `ISO14443A` card can be detected repeatedly
- UID output is stable
- card removal and re-approach do not require reboot

Minimum hardware repetition:

- at least `20` read cycles

## Phase 3: Extended I2C Example

Target example:

- `examples/ESP32_I2C_scan_14443A_15693/ESP32_I2C_scan_14443A_15693.ino`

Goal:

- confirm that the I2C path still behaves when multiple poll technologies are active

Pass criteria:

- `ISO14443A` can still be detected
- `ISO15693` can be detected if a validated card is available
- deactivation and rediscovery still work
- no obvious deadlock during repeated scan loops

`ISO14443B` should be treated as optional for the first I2C pass.

## Phase 4: Stability and Hotplug

Goal:

- verify that I2C does not only work once, but survives repeated traffic

Recommended checks:

- repeated card approach and removal
- continuous scan loop for at least `20` successful redetections
- at least one longer-duration run with no obvious lockup

Pass criteria:

- no permanent stuck state
- no requirement to reset the ESP32 between card cycles
- no recurring I2C short-read or timeout failures under normal use

## Phase 5: Fault-Oriented Checks

Goal:

- make transport failures diagnosable instead of silent

Recommended fault checks:

- wrong or disconnected IRQ
- wrong I2C address
- disconnected SDA or SCL
- no card present

Expected behavior:

- transport errors are surfaced explicitly
- initialization fails fast when the bus is broken
- the library does not hang indefinitely in normal failure paths

## Documentation Outputs Required For Completion

I2C should not be marked as supported until these outputs exist:

- `docs/i2c_gap_analysis.md`
- `docs/i2c_validation_plan.md`
- I2C example source files
- README wiring notes for I2C
- a short validation-results section with:
  - board
  - pins
  - clock
  - tested card types
  - known limitations

## Minimum Acceptance Criteria For I2C Support

The repository can claim first-pass I2C support only when all of the following are true:

1. I2C examples compile.
2. I2C initialization succeeds on the validated ESP32 setup.
3. At least one `ISO14443A` card can be stably detected.
4. Card removal and rediscovery work without reboot.
5. SPI examples still compile and the SPI baseline is still intact.
6. Wiring, limits, and known-good validation results are documented.

## Current Status

At the time this plan was written:

- the validation plan exists
- the code inspection gap analysis exists
- the chip probe and I2C scan examples exist
- compile closure exists for the current I2C examples
- no ESP32 I2C hardware validation has been completed yet
