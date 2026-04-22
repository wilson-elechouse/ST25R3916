#ifndef ST25R3916_CONFIG_H
#define ST25R3916_CONFIG_H

#include <Arduino.h>

/* ESP32-specific: default wiring used by the SPI examples in this fork. */
#ifndef ST25R3916_DEFAULT_SPI_SCK_PIN
#define ST25R3916_DEFAULT_SPI_SCK_PIN 18
#endif

#ifndef ST25R3916_DEFAULT_SPI_MISO_PIN
#define ST25R3916_DEFAULT_SPI_MISO_PIN 19
#endif

#ifndef ST25R3916_DEFAULT_SPI_MOSI_PIN
#define ST25R3916_DEFAULT_SPI_MOSI_PIN 23
#endif

#ifndef ST25R3916_DEFAULT_SPI_SS_PIN
#define ST25R3916_DEFAULT_SPI_SS_PIN 5
#endif

#ifndef ST25R3916_DEFAULT_IRQ_PIN
#define ST25R3916_DEFAULT_IRQ_PIN 4
#endif

#ifndef ST25R3916_DEFAULT_LED_PIN
#define ST25R3916_DEFAULT_LED_PIN 2
#endif

#ifndef ST25R3916_DEFAULT_I2C_SDA_PIN
#define ST25R3916_DEFAULT_I2C_SDA_PIN 21
#endif

#ifndef ST25R3916_DEFAULT_I2C_SCL_PIN
#define ST25R3916_DEFAULT_I2C_SCL_PIN 22
#endif

#ifndef ST25R3916_DEFAULT_I2C_FREQUENCY
#define ST25R3916_DEFAULT_I2C_FREQUENCY 100000UL
#endif

/* SPI-path: stay conservative on clock speed until board-specific validation says otherwise. */
#ifndef ST25R3916_DEFAULT_SPI_FREQUENCY
#define ST25R3916_DEFAULT_SPI_FREQUENCY 5000000UL
#endif

/* SPI-path: keep IRQ drain bounded if the line stays asserted unexpectedly. */
#ifndef ST25R3916_IRQ_READ_GUARD_LIMIT
#define ST25R3916_IRQ_READ_GUARD_LIMIT 32U
#endif

/* ESP32-specific: optional serial debug logging, disabled by default. */
#ifndef ST25R3916_ENABLE_DEBUG_LOG
#define ST25R3916_ENABLE_DEBUG_LOG 0
#endif

#if ST25R3916_ENABLE_DEBUG_LOG
#define ST25R3916_DEBUG_PRINT(message) Serial.print(message)
#define ST25R3916_DEBUG_PRINTLN(message) Serial.println(message)
#else
#define ST25R3916_DEBUG_PRINT(message) do { } while (0)
#define ST25R3916_DEBUG_PRINTLN(message) do { } while (0)
#endif

#endif /* ST25R3916_CONFIG_H */
