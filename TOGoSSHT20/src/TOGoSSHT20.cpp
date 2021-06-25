#include "TOGoSSHT20.h"

#define SHT20_TEMP             0xF3
#define SHT20_HUMID            0xF5
#define SHT20_WRITE_USER_REG   0xE6
#define SHT20_READ_USER_REG    0xE7
#define SHT20_RESET            0xFE
#define _DISABLE_ONCHIP_HEATER 0b00000000
#define _ENABLE_OTP_RELOAD     0b00000000
#define _DISABLE_OTP_RELOAD    0b00000010
#define _RESERVED_BITMASK      0b00111000
#define SOFT_RESET_DELAY       20
#define TEMPERATURE_DELAY      100
#define HUMIDITY_DELAY         40

/** Vapor deficit, in kPa */
float TOGoS::SHT20::EverythingReading::getVpdKpa() const {
  float tempC = this->getTempC();
  float es = 0.6108 * exp(17.27 * tempC / (tempC + 237.3));
  float ae = this->getRh() * es;
  return es - ae;
}
TOGoS::SHT20::Temperature TOGoS::SHT20::EverythingReading::getDewPoint() const {
  float tem = -1.0 * this->getTempC();
  float esdp = 6.112 * exp(-1.0 * 17.67 * tem / (243.5 - tem));
  float ed = this->getRh() * esdp;
  float eln = log(ed / 6.112);
  return TOGoS::SHT20::Temperature(-243.5 * eln / (eln - 17.67 ));
}





bool TOGoS::SHT20::Driver::begin(uint8_t address)
{
  this->address = address;

  return connected();
}

void TOGoS::SHT20::Driver::reset()
{
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_RESET);
  Wire.endTransmission();
  delay(SOFT_RESET_DELAY);
  this->onchip_heater = _DISABLE_ONCHIP_HEATER;
  this->otp_reload = _DISABLE_OTP_RELOAD;

  Wire.beginTransmission(this->address);
  Wire.write(SHT20_READ_USER_REG);
  Wire.endTransmission();
  Wire.requestFrom(this->address, 1);
  uint8_t config = Wire.read();
  config = ((config & _RESERVED_BITMASK) | this->resolution | this->onchip_heater | this->otp_reload);
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_WRITE_USER_REG);
  Wire.write(config);
  Wire.endTransmission();
}

void TOGoS::SHT20::Driver::measure_all()
{
  // also measures temp/humidity
  vpd();
  dew_point();
}

TOGoS::SHT20::EverythingReading TOGoS::SHT20::Driver::readEverything() {
  // TODO: There may be some redundancy, here.
  // Like maybe we don't need to call reset twice?
  // Or at all?  I don't actually know what reset()s purpose is.

  uint8_t lsb, msb;
  
  this->reset();
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_TEMP);
  Wire.endTransmission();
  delay(TEMPERATURE_DELAY);
  Wire.requestFrom(this->address, 2);
  msb = Wire.read();
  lsb = Wire.read();
  TOGoS::SHT20::TemperatureReading temp(msb << 8 | lsb);

  this->reset();
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_HUMID);
  Wire.endTransmission();
  delay(HUMIDITY_DELAY);
  Wire.requestFrom(this->address, 2);
  msb = Wire.read();
  lsb = Wire.read();
  TOGoS::SHT20::HumidityReading humid(msb << 8 | lsb);

  return TOGoS::SHT20::EverythingReading(temp, humid);
}

float TOGoS::SHT20::Driver::temperature()
{
  this->reset();
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_TEMP);
  Wire.endTransmission();
  delay(TEMPERATURE_DELAY);
  Wire.requestFrom(this->address, 2);
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  uint16_t value = msb << 8 | lsb;
  tempC = value * (175.72 / 65536.0)- 46.85;
  tempF = ((value * (175.72 / 65536.0)- 46.85)  * 1.8) + 32;
  return tempC;
}

float TOGoS::SHT20::Driver::temperature_f()
{
  this->reset();
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_TEMP);
  Wire.endTransmission();
  delay(TEMPERATURE_DELAY);
  Wire.requestFrom(this->address, 2);
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  uint16_t value = msb << 8 | lsb;
  tempC = value * (175.72 / 65536.0)- 46.85;
  tempF = ((value * (175.72 / 65536.0)- 46.85)  * 1.8) + 32;
  return tempF;
}

float TOGoS::SHT20::Driver::humidity()
{
  this->reset();
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_HUMID);
  Wire.endTransmission();
  delay(HUMIDITY_DELAY);
  Wire.requestFrom(this->address, 2);
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  uint16_t value = msb << 8 | lsb;
  RH = value * (125.0 / 65536.0) - 6.0;
  return RH;
}

float TOGoS::SHT20::Driver::vpd()
{
  temperature();
  humidity();

  float es = 0.6108 * exp(17.27 * tempC / (tempC + 237.3));
  float ae = RH / 100 * es;
  vpd_kPa = es - ae;

  return vpd_kPa;
}

float TOGoS::SHT20::Driver::dew_point()
{
  temperature();
  humidity();

  float tem = -1.0 * tempC;
  float esdp = 6.112 * exp(-1.0 * 17.67 * tem / (243.5 - tem));
  float ed = RH / 100.0 * esdp;
  float eln = log(ed / 6.112);
  dew_pointC = -243.5 * eln / (eln - 17.67 );

  dew_pointF = (dew_pointC * 1.8) + 32;
  return dew_pointC;
}

bool TOGoS::SHT20::Driver::connected()
{
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_READ_USER_REG);
  Wire.endTransmission();
  Wire.requestFrom(this->address, 1);
  uint8_t config = Wire.read();
  
  if (config != 0xFF) {
    return true;
  }
  else {
    return false;
  }
}
