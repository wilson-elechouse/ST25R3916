# I2C Gap Analysis

## Scope

This document records the current state of the repository's unfinished I2C path.
It is intentionally limited to code inspection. It does not claim that I2C is
working on ESP32.

## Current I2C Surface

The repository already contains an I2C-oriented constructor and multiple
low-level I2C transaction branches:

- `ST25R3916_110/src/rfal_rfst25r3916.cpp`
  - `RfalRfST25R3916Class(TwoWire *i2c, int int_pin)`
  - sets `i2c_enabled = true`
- `ST25R3916_110/src/st25r3916_com.cpp`
  - register read/write
  - FIFO read/write
  - PTMem read/write
  - direct command execution
  - test register read/write
- `ST25R3916_110/src/st25r3916.cpp`
  - shared chip initialization still runs for both SPI and I2C
- `ST25R3916_110/src/st25r3916_interrupt.cpp`
  - shared IRQ path still runs for both SPI and I2C

## Status Matrix

| Area | Status | Notes |
| --- | --- | --- |
| `TwoWire` constructor exists | Present | Basic object entry point is in place |
| Register read/write over I2C | Partial | Code exists, but error handling is missing |
| FIFO read/write over I2C | Partial | Code exists, but short-read and bus errors are not handled |
| PTMem read/write over I2C | Partial | Code exists, but not validated and not guarded against short reads |
| Direct command over I2C | Partial | Command path exists, but result is not checked |
| Test register read/write over I2C | Partial | Access path exists, but response validation is minimal |
| I2C bus address configuration | Missing | Address is hardcoded to `0x50` (`0xA0 >> 1`) |
| I2C speed configuration | Missing | No explicit `Wire.setClock()` path |
| ESP32 SDA/SCL pin configuration | Missing | No library-level API or example for choosing pins |
| `Wire.begin(...)` ownership | Missing | Library assumes caller prepared the bus |
| I2C example sketches | Present | `ESP32_I2C_probe_chip`, `ESP32_I2C_scan_14443A`, and `ESP32_I2C_scan_14443A_15693` now exist |
| I2C documentation | Partial | Wiring and validation notes exist, but hardware closure is still pending |
| I2C-specific validation results | Partial | Compile closure exists; ESP32 hardware validation is still missing |

## Key Findings

### 1. The constructor exists, but the end-to-end startup path is not isolated

The repository has a dedicated I2C constructor, but `rfalInitialize()` still
executes shared startup code that was shaped around SPI-first behavior.

Current risks:

- `rfalInitialize()` still executes `pinMode(cs_pin, OUTPUT)` and
  `digitalWrite(cs_pin, HIGH)` even when the I2C constructor set `cs_pin = -1`
- the code path is not yet clearly separated into "SPI init" and "I2C init"
- I2C enablement depends on object construction only; there is no explicit
  I2C configuration step

### 2. I2C transactions are implemented, but almost all bus errors are ignored

Across the I2C branches in `st25r3916_com.cpp`, the code currently:

- calls `beginTransmission()`
- writes bytes
- calls `endTransmission(...)`
- sometimes calls `requestFrom(...)`
- returns `ERR_NONE` regardless of the I2C result

Current gaps:

- `endTransmission()` return codes are never checked
- `requestFrom()` returned length is never checked
- no I2C-specific error mapping is performed
- timeout behavior is delegated entirely to `Wire`, with no repository-level
  handling or recovery policy

### 3. Short-read handling is incomplete

Multiple read paths fill buffers with:

- `while (dev_i2c->available()) { ... }`

but do not verify that the expected number of bytes was received.

Current risks:

- partial register reads are silently accepted
- FIFO reads may leave unread or uninitialized bytes in the destination buffer
- PTMem reads copy from a temporary buffer even if the I2C transaction returned
  fewer bytes than expected
- higher layers may see protocol errors that are actually transport errors

### 4. Chip bring-up currently hides transport failures

`st25r3916Initialize()` and `st25r3916CheckChipID()` still rely on shared helper
calls that ignore low-level I2C failures.

Current risks:

- I2C read failures can collapse into generic chip mismatch behavior
- oscillator, regulator, and interrupt setup routines have no I2C-specific
  bring-up diagnostics
- initialization may fail without clearly telling the caller whether the root
  cause is wiring, address, IRQ, or transport

### 5. I2C configuration is not yet user-facing

The current code does not yet define a complete user-facing I2C setup model.

Missing pieces:

- how the user selects SDA/SCL pins on ESP32
- how the user selects I2C frequency
- whether the library owns `Wire.begin(...)` or only consumes an already-started
  `TwoWire`
- whether the ST25R3916 I2C address should remain fixed or become configurable

### 6. IRQ and bus-state handling are shared, not yet I2C-validated

The repository already has shared state variables:

- `i2c_enabled`
- `bus_busy`
- `isr_pending`

and a shared interrupt flow that defers IRQ processing while the bus is busy.

This is promising, but not yet validated on I2C:

- the same deferred-IRQ logic may be fine, but it has only been closed on SPI
- there is no I2C-specific stress or hotplug validation yet
- there is no evidence yet that repeated-start reads and IRQ clearing interact
  correctly under I2C on ESP32

### 7. I2C examples now exist, but validation closure does not

The repository now has the minimum example artifacts for bring-up, but still
lacks the hardware closure needed to claim support:

- `examples/ESP32_I2C_probe_chip/ESP32_I2C_probe_chip.ino`
- `examples/ESP32_I2C_scan_14443A/ESP32_I2C_scan_14443A.ino`
- `examples/ESP32_I2C_scan_14443A_15693/ESP32_I2C_scan_14443A_15693.ino`
- compile closure
- known-good hardware validation results

## Gap Summary

### Already present

- a `TwoWire`-based hardware constructor
- basic low-level transaction branches for the major ST25R3916 access patterns
- shared interrupt and state machinery that can be extended for I2C

### Partially present

- register access
- FIFO access
- PTMem access
- direct command execution
- test register access
- shared bring-up path

### Missing before I2C can be claimed as supported

- I2C-specific startup isolation
- error handling and transport result checking
- short-read protection
- configurable ESP32 I2C wiring and speed strategy
- hardware validation and acceptance results

## Recommended Next Implementation Order

1. Isolate the I2C low-level path from SPI-specific init assumptions.
2. Add `endTransmission()` and `requestFrom()` result checks to every I2C path.
3. Introduce a consistent I2C error mapping policy.
4. Define how ESP32 `Wire.begin(...)`, SDA/SCL pins, clock, and IRQ are owned.
5. Close hardware validation on the chip probe and `ISO14443A` example.
6. Expand hardware validation to the extended `ISO14443A + ISO15693` example.

## Current Conclusion

The repository now has a working first-pass ESP32 I2C path for:

- chip probe
- `ISO14443A` scanning
- extended `A + V` scan operation with the current `ISO14443A` test card

Remaining gaps are now narrower:

- no recorded `ISO15693` hardware card result yet
- no broader board matrix yet
- no long-duration I2C hotplug/stress closure yet
