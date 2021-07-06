#ifndef TOGOS_SHT20_H
#define TOGOS_SHT20_H

// TOGoS SHT20.
// Forked from uFire_SHT20 to refactor and remove uncecessary bits

#include <math.h>

#include <Arduino.h>
#include <Wire.h>

// Warning: This is the original API from uFire_SHT20.
// I plan to simplify it (remove high-level logic and unnecessary stored values) a lot for v1.0.0

namespace TOGoS { namespace SHT20 {

inline float cToF(float c) { return c * 1.8 + 32; }

struct Temperature
{
  float tempC;
  Temperature(float c) : tempC(c) {}
  inline float getC() const { return this->tempC; }
  inline float getF() const { return cToF(this->tempC); }
};
    
struct TemperatureReading
{
  uint16_t value;
  TemperatureReading(uint16_t value) : value(value) { }
  inline float getF() const { return cToF(this->getC()); }
  inline float getC() const { return this->value * (175.72 / 65536.0) - 46.85; }
  inline bool isValid() const { return this->value != (uint16_t)0xFFFF; }
};

struct HumidityReading
{
  uint16_t value;
  HumidityReading(uint16_t value) : value(value) { }
  float getRh() const { return this->value * (1.25 / 65536.0) - 0.06; }
  float getRhPercent() const { return this->value * (125.0 / 65536.0) - 6.0; }
  inline bool isValid() const { return this->value != (uint16_t)0xFFFF; }
};

struct EverythingReading
{
  TemperatureReading temperature;
  HumidityReading humidity;
  EverythingReading(TemperatureReading temp, HumidityReading humid) :
    temperature(temp), humidity(humid) {}
  EverythingReading() : EverythingReading(0xFFFF, 0xFFFF) {}
  TemperatureReading getTemperature() const { return this->temperature; }
  HumidityReading getHumidity() const { return this->humidity; }
  
  // Convenience functions, because the original library
  // had similar things, but implemented in a silly way
  inline float getTemperatureF() const { return this->temperature.getF(); }
  inline float getTemperatureC() const { return this->temperature.getC(); }
  /** Returns relative humidity as 0.0-1.0 */
  inline float getRh() const { return this->humidity.getRh(); }
  inline float getRhPercent() const { return this->humidity.getRhPercent(); }
  float getVpdKpa() const;
  Temperature getDewPoint() const;
  // [2021-07-06]: I'm pretty sure these values are out of range
  // of what the SHT20 can actually return, so indiicate an invalid value:
  bool isValid() const { return this->temperature.isValid() && this->humidity.isValid(); }
};

class Driver
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
  Driver(TwoWire &i2c) : i2cPort(&i2c) { };
  
  bool begin(uint8_t address=DEFAULT_ADDRESS);
  EverythingReading readEverything();
  bool isConnected();
};

}}

#endif // ifndef TOGOS_SHT20_H
