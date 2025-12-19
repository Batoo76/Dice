#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include <Wire.h>

// I2C pins for QMI8658 on Waveshare board
#define QMI8658_SDA 6
#define QMI8658_SCL 7

// Initialize I2C bus
static bool DEV_I2C_Init(void) {
  Wire.begin(QMI8658_SDA, QMI8658_SCL);
  return true;
}

// Write a byte to I2C device
static void DEV_I2C_Write_Byte(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// Read n bytes from I2C device
static void DEV_I2C_Read_nByte(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)addr, (uint8_t)len);
  for (uint16_t i = 0; i < len; i++) {
    if (Wire.available()) {
      buf[i] = Wire.read();
    }
  }
}

#endif

