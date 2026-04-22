/*
  Example: ESP32_I2C_probe_chip
  Bus: I2C
  Goal: Verify the minimum ESP32 <-> ST25R3916 I2C communication path before any card polling.

  Default wiring for ESP32 Dev Module:
  - SDA -> GPIO21
  - SCL -> GPIO22
  - IRQ -> GPIO4
  - GND -> GND
  - VCC -> board supply required by your ST25R3916 module

  Serial monitor:
  - Baud rate: 115200

  What this sketch checks:
  1. Raw I2C ACK on address 0x50
  2. Raw chip-ID register read through the library I2C path
  3. Full rfalInitialize() bring-up

  If step 1 fails:
  - SDA/SCL/address/power is still wrong

  If step 1 passes but step 3 fails:
  - basic I2C transport works, but IRQ/init path still needs attention
*/

#include <Arduino.h>
#include <Wire.h>

#include <rfal_rfst25r3916.h>
#include <st25r3916_com.h>
#include <st_errno.h>

namespace {

constexpr int kPinSda = 21;
constexpr int kPinScl = 22;
constexpr int kPinIrq = ST25R3916_DEFAULT_IRQ_PIN;
constexpr uint32_t kI2cClockHz = 100000UL;
constexpr uint8_t kI2cAddress = 0x50U;

RfalRfST25R3916Class gReader(&Wire, kPinIrq);

void waitForSerial()
{
  const unsigned long start = millis();
  while (!Serial && ((millis() - start) < 2000UL)) {
    delay(10);
  }
}

const char *wireStatusToString(uint8_t status)
{
  switch (status) {
    case 0U:
      return "ACK";
    case 1U:
      return "data too long";
    case 2U:
      return "address NACK";
    case 3U:
      return "data NACK";
    case 4U:
      return "other error";
    case 5U:
      return "timeout";
    default:
      return "unknown";
  }
}

const char *returnCodeToString(ReturnCode err)
{
  switch (err) {
    case ERR_NONE:
      return "ERR_NONE";
    case ERR_PARAM:
      return "ERR_PARAM";
    case ERR_TIMEOUT:
      return "ERR_TIMEOUT";
    case ERR_IO:
      return "ERR_IO";
    case ERR_HW_MISMATCH:
      return "ERR_HW_MISMATCH";
    default:
      return "OTHER";
  }
}

const char *chipTypeToString(uint8_t icIdentity)
{
  const uint8_t chipType = icIdentity & ST25R3916_REG_IC_IDENTITY_ic_type_mask;
  if (chipType == ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916) {
    return "ST25R3916";
  }
  if (chipType == ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916b) {
    return "ST25R3916B";
  }
  return "unknown";
}

void printChipIdentity(uint8_t icIdentity)
{
  const uint8_t revision = (uint8_t)((icIdentity & ST25R3916_REG_IC_IDENTITY_ic_rev_mask) >>
                                     ST25R3916_REG_IC_IDENTITY_ic_rev_shift);
  Serial.print("IC_IDENTITY = 0x");
  if (icIdentity < 0x10U) {
    Serial.print('0');
  }
  Serial.println(icIdentity, HEX);
  Serial.print("Chip type   = ");
  Serial.println(chipTypeToString(icIdentity));
  Serial.print("Revision    = ");
  Serial.println(revision);
}

uint8_t rawAddressProbe()
{
  Wire.beginTransmission(kI2cAddress);
  return (uint8_t)Wire.endTransmission(true);
}

void runProbe()
{
  Serial.println("ESP32 I2C ST25R3916 minimal probe");
  Serial.print("SDA=");
  Serial.print(kPinSda);
  Serial.print(" SCL=");
  Serial.print(kPinScl);
  Serial.print(" IRQ=");
  Serial.print(kPinIrq);
  Serial.print(" ADDR=0x");
  Serial.print(kI2cAddress, HEX);
  Serial.print(" CLK=");
  Serial.println(kI2cClockHz);
  Serial.println();

  Wire.begin(kPinSda, kPinScl, kI2cClockHz);
  delay(20);

  Serial.println("[1/3] Raw Wire ACK probe");
  const uint8_t probeStatus = rawAddressProbe();
  Serial.print("endTransmission() = ");
  Serial.print(probeStatus);
  Serial.print(" (");
  Serial.print(wireStatusToString(probeStatus));
  Serial.println(")");
  if (probeStatus != 0U) {
    Serial.println("Result: I2C bus is not responding at 0x50.");
    return;
  }

  Serial.println();
  Serial.println("[2/3] Chip-ID register read through library I2C path");
  uint8_t icIdentity = 0U;
  ReturnCode err = gReader.st25r3916ReadRegister(ST25R3916_REG_IC_IDENTITY, &icIdentity);
  Serial.print("st25r3916ReadRegister() = ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");
  if (err != ERR_NONE) {
    Serial.println("Result: raw ACK works, but register read failed.");
    return;
  }
  printChipIdentity(icIdentity);

  Serial.println();
  Serial.println("[3/3] Full reader bring-up");
  err = gReader.rfalInitialize();
  Serial.print("rfalInitialize() = ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");
  if (err != ERR_NONE) {
    Serial.println("Result: I2C transport works, but full init failed.");
    Serial.println("Check IRQ wiring, power, and init path next.");
    return;
  }

  uint8_t ioConf2 = 0U;
  err = gReader.st25r3916ReadRegister(ST25R3916_REG_IO_CONF2, &ioConf2);
  Serial.print("Post-init IO_CONF2 read = ");
  Serial.print(returnCodeToString(err));
  Serial.print(" (");
  Serial.print((int)err);
  Serial.println(")");
  if (err == ERR_NONE) {
    Serial.print("IO_CONF2 = 0x");
    if (ioConf2 < 0x10U) {
      Serial.print('0');
    }
    Serial.println(ioConf2, HEX);
  }

  Serial.println("Result: minimum I2C hardware communication is working.");
}

} // namespace

void setup()
{
  Serial.begin(115200);
  waitForSerial();
  delay(100);
  runProbe();
}

void loop()
{
  delay(1000);
}
