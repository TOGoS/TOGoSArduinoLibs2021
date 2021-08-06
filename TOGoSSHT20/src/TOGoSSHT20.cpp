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
  float tempC = this->getTemperatureC();
  float es = 0.6108 * exp(17.27 * tempC / (tempC + 237.3));
  float ae = this->getRh() * es;
  return es - ae;
}
TOGoS::SHT20::Temperature TOGoS::SHT20::EverythingReading::getDewPoint() const {
  float tem = -1.0 * this->getTemperatureC();
  float esdp = 6.112 * exp(-1.0 * 17.67 * tem / (243.5 - tem));
  float ed = this->getRh() * esdp;
  float eln = log(ed / 6.112);
  return TOGoS::SHT20::Temperature(-243.5 * eln / (eln - 17.67 ));
}





bool TOGoS::SHT20::Driver::begin(uint8_t address)
{
  this->address = address;

  return this->isConnected();
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
  Wire.requestFrom(this->address, (uint8_t)1);
  uint8_t config = Wire.read();
  config = ((config & _RESERVED_BITMASK) | this->resolution | this->onchip_heater | this->otp_reload);
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_WRITE_USER_REG);
  Wire.write(config);
  Wire.endTransmission();
}

TOGoS::SHT20::EverythingReading TOGoS::SHT20::Driver::readEverything() {
  // TODO: There may be some redundancy, here.
  // Like maybe we don't need to call reset every time?
  
  uint8_t lsb, msb;
  
  this->reset();
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_TEMP);
  Wire.endTransmission();
  delay(TEMPERATURE_DELAY);
  Wire.requestFrom(this->address, (uint8_t)2);
  msb = Wire.read();
  lsb = Wire.read();
  TOGoS::SHT20::TemperatureReading temp(msb << 8 | lsb);

  this->reset();
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_HUMID);
  Wire.endTransmission();
  delay(HUMIDITY_DELAY);
  Wire.requestFrom(this->address, (uint8_t)2);
  msb = Wire.read();
  lsb = Wire.read();
  TOGoS::SHT20::HumidityReading humid(msb << 8 | lsb);

  return TOGoS::SHT20::EverythingReading(temp, humid);
}

bool TOGoS::SHT20::Driver::isConnected()
{
  Wire.beginTransmission(this->address);
  Wire.write(SHT20_READ_USER_REG);
  Wire.endTransmission();
  Wire.requestFrom(this->address, (uint8_t)1);
  uint8_t config = Wire.read();
  
  if (config != 0xFF) {
    return true;
  }
  else {
    return false;
  }
}
