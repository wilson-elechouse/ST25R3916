/*
  Example: ESP32_SPI_chip_info_debug
  Bus: SPI
  Wiring: SCK=18, MISO=19, MOSI=23, SS=5, IRQ=4
  Cards: No tag required
  Goal: Verify the reader chip is reachable before debugging higher-level polling issues.
  Common failures: CS pin held low by other hardware, SPI pins swapped, library folders not installed together.
*/

#include <Arduino.h>
#include <SPI.h>

#include <rfal_rfst25r3916.h>
#include <st25r3916_com.h>
#include <st25r3916_config.h>
#include <st_errno.h>

namespace {

constexpr int kPinMosi = ST25R3916_DEFAULT_SPI_MOSI_PIN;
constexpr int kPinMiso = ST25R3916_DEFAULT_SPI_MISO_PIN;
constexpr int kPinSck = ST25R3916_DEFAULT_SPI_SCK_PIN;
constexpr int kPinSs = ST25R3916_DEFAULT_SPI_SS_PIN;
constexpr int kPinIrq = ST25R3916_DEFAULT_IRQ_PIN;

SPIClass gSpi(VSPI);
RfalRfST25R3916Class gReader(&gSpi, kPinSs, kPinIrq);
unsigned long gNextDumpAt = 0;

void waitForSerial()
{
  const unsigned long start = millis();
  while (!Serial && ((millis() - start) < 2000UL)) {
    delay(10);
  }
}

void dumpRegister(const __FlashStringHelper *label, uint8_t reg)
{
  uint8_t value = 0;
  const ReturnCode err = gReader.st25r3916ReadRegister(reg, &value);
  Serial.print(label);
  Serial.print(": 0x");
  if (value < 0x10U) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
  Serial.print(" (err=");
  Serial.print((int)err);
  Serial.println(")");
}

} // namespace

void setup()
{
  Serial.begin(115200);
  waitForSerial();

  pinMode(kPinSs, OUTPUT);
  digitalWrite(kPinSs, HIGH);
  gSpi.begin(kPinSck, kPinMiso, kPinMosi, kPinSs);

  Serial.println("ESP32 SPI chip info debug");

  const ReturnCode err = gReader.rfalInitialize();
  if (err != ERR_NONE) {
    Serial.print("rfalInitialize failed: ");
    Serial.println((int)err);
    while (true) {
      delay(250);
    }
  }

  Serial.println("Reader initialized, starting periodic register dump");
}

void loop()
{
  if (millis() < gNextDumpAt) {
    delay(10);
    return;
  }

  gNextDumpAt = millis() + 1000UL;
  dumpRegister(F("IC_IDENTITY"), ST25R3916_REG_IC_IDENTITY);
  dumpRegister(F("IO_CONF2"), ST25R3916_REG_IO_CONF2);
  dumpRegister(F("OP_CONTROL"), ST25R3916_REG_OP_CONTROL);
  Serial.print(F("IRQ pin: "));
  Serial.println(digitalRead(kPinIrq));
  Serial.println();
}
