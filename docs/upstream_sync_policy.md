# Upstream Sync Policy

## Fork posture

This repository is treated as an ESP32-oriented fork, not as a strict mirror of the original stm32duino libraries.

## What gets synchronized

- Small SPI stability fixes
- Interrupt handling fixes that help ESP32 polling behaviour
- Safe build and compatibility fixes for Arduino-ESP32
- Narrow, easy-to-revert backports

## What does not get synchronized by default

- Large-scale upstream version jumps
- I2C-specific behavioural changes before the dedicated I2C phase
- API reshaping that would break existing SPI sketches
- Broad cleanup-only updates with no ESP32 value

## Review rule

Each upstream sync should answer these questions before merge:

1. Does it improve the current ESP32 SPI path?
2. Can it be validated with the existing SPI examples?
3. Can it be reverted cleanly if it regresses discovery or activation?

## Code annotation tags

Use these tags in comments when touching shared code:

- `ESP32-specific`
- `SPI-path`
- `I2C-prep`
- `Backported-from-upstream`
