#ifndef TOGOS_SHT20_H
#define TOGOS_SHT20_H

// TOGoS SHT20.
// Forked from uFire_SHT20 to refactor and remove uncecessary bits

#include <math.h>

#include <Arduino.h>
#include <Wire.h>

// Warning: This is the original API from uFire_SHT20.
// I plan to simplify it (remove high-level logic and unnecessary stored values) a lot for v1.0.0

namespace TOGoS {
class SHT20
{
public:
  static const uint8_t RESOLUTION_12BITS     = 0b00000000;
  static const uint8_t RESOLUTION_11BITS     = 0b10000001;
  static const uint8_t RESOLUTION_10BITS     = 0b10000000;
  static const uint8_t RESOLUTION_8BITS      = 0b00000001;

  static const uint8_t DEFAULT_ADDRESS       = 0x40;
private:
  uint8_t address;
  TwoWire *i2cPort;
  void reset();
  uint8_t resolution = RESOLUTION_12BITS;
  uint8_t onchip_heater;
  uint8_t otp_reload;

public:
  // TODO: Don't store converted values;
  // just provide raw data and conversion functions.
  float tempC;
  float tempF;
  float vpd_kPa;
  float dew_pointC;
  float dew_pointF;
  float RH;
  
  SHT20(TwoWire &i2c) : i2cPort(&i2c) { };
  
  bool begin(uint8_t address=DEFAULT_ADDRESS);
  float temperature();
  float temperature_f();
  float humidity();
  float vpd();
  float dew_point();
  void measure_all();
  bool connected();
};
}

#endif // ifndef TOGOS_SHT20_H
