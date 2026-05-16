/*
  Example: ESP32_I2C_probe_chip
  Bus: I2C
  Goal: Verify the minimum ESP32 <-> ST25R3916/ST25R3916B I2C communication path before any card polling.

  Default wiring:
  - ESP32: SDA -> GPIO21, SCL -> GPIO22
  - ESP32-S3: SDA -> GPIO8, SCL -> GPIO9
  - ESP32-C3: SDA -> GPIO4, SCL -> GPIO5
  - IRQ -> GPIO4 (ESP32/ESP32-S3) or GPIO6 (ESP32-C3)
  - GND -> GND
  - VCC -> board supply required by your ST25R3916/ST25R3916B module

  Serial monitor:
  - Baud rate: 115200

  What this sketch checks:
  1. Raw I2C ACK scan across the bus
  2. Raw I2C ACK on the expected ST25R3916/ST25R3916B address 0x50
  3. Raw chip-ID register read through the library I2C path
  4. Full rfalInitialize() bring-up

  If no address ACKs:
  - SDA/SCL/power/I2C-mode selection is still wrong

  If another address ACKs but 0x50 does not:
  - confirm the ST25R3916/ST25R3916B I2C address configuration

  If 0x50 passes but full init fails:
  - basic I2C transport works, but IRQ/init path still needs attention
*/

#include <Arduino.h>
#include <Wire.h>

#include <rfal_rfst25r3916.h>
#include <st25r3916_com.h>
#include <st_errno.h>

namespace {

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
// ESP32-S3 default I2C wiring. Edit these values to match your board.
constexpr int kPinSda = 8;
constexpr int kPinScl = 9;
constexpr int kPinIrq = 4;
constexpr uint32_t kI2cClockHz = 100000UL;
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ARDUINO_ESP32C3_DEV)
// ESP32-C3 default I2C wiring. Edit these values to match your board.
constexpr int kPinSda = 4;
constexpr int kPinScl = 5;
constexpr int kPinIrq = 6;
constexpr uint32_t kI2cClockHz = 100000UL;
#else
// Classic ESP32 default I2C wiring. Edit these values to match your board.
constexpr int kPinSda = 21;
constexpr int kPinScl = 22;
constexpr int kPinIrq = 4;
constexpr uint32_t kI2cClockHz = 100000UL;
#endif

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

uint8_t scanI2cBus()
{
  uint8_t count = 0U;
  for (uint8_t address = 1U; address < 0x7FU; address++) {
    Wire.beginTransmission(address);
    const uint8_t status = (uint8_t)Wire.endTransmission(true);
    if (status == 0U) {
      if (count == 0U) {
        Serial.print("ACK address(es):");
      }
      Serial.print(" 0x");
      if (address < 0x10U) {
        Serial.print('0');
      }
      Serial.print(address, HEX);
      count++;
    }
  }
  if (count == 0U) {
    Serial.println("ACK address(es): none");
  } else {
    Serial.println();
  }
  return count;
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

  Serial.println("[1/4] Raw Wire bus scan");
  const uint8_t ackCount = scanI2cBus();
  if (ackCount == 0U) {
    Serial.println("Result: no I2C devices ACKed on this bus.");
    Serial.println("Check SDA/SCL order, GND, VCC, pull-ups, and ST25R3916/ST25R3916B I2C/SPI mode selection.");
    return;
  }

  Serial.println();
  Serial.println("[2/4] Expected ST25R3916/ST25R3916B address probe");
  const uint8_t probeStatus = rawAddressProbe();
  Serial.print("endTransmission() = ");
  Serial.print(probeStatus);
  Serial.print(" (");
  Serial.print(wireStatusToString(probeStatus));
  Serial.println(")");
  if (probeStatus != 0U) {
    Serial.println("Result: I2C bus is not responding at 0x50.");
    Serial.println("If the scan found another address, confirm the module address straps/configuration.");
    return;
  }

  Serial.println();
  Serial.println("[3/4] Chip-ID register read through library I2C path");
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
  Serial.println("[4/4] Full reader bring-up");
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
