#include "Driver.h"
#include <Wire.h>

// Data sheet for the SSD1306: https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf

void TOGoS::SSD1306::Driver::begin() {
  sendCommand(0xAE);            //display off
  sendCommand(0xA6);            //Set Normal Display (default)
  sendCommand(0xAE);            //DISPLAYOFF
  sendCommand(0xD5);            //SETDISPLAYCLOCKDIV
  sendCommand(0x80);            // the suggested ratio 0x80
  sendCommand(0xA8);            //SSD1306_SETMULTIPLEX
  sendCommand(0x3F);
  sendCommand(0xD3);            //SETDISPLAYOFFSET
  sendCommand(0x0);             //no offset
  sendCommand(0x40|0x0);        //SETSTARTLINE
  sendCommand(0x8D);            //CHARGEPUMP
  sendCommand(0x14);
  sendCommand(0x20);            //MEMORYMODE
  sendCommand(0x00);            //0x0 act like ks0108
  sendCommand(0xA1);            //SEGREMAP   Mirror screen horizontally (A0)
  sendCommand(0xC8);            //COMSCANDEC Rotate screen vertically (C0)
  sendCommand(0xDA);            //0xDA
  sendCommand(0x12);            //COMSCANDEC
  sendCommand(0x81);            //SETCONTRAST
  sendCommand(0xCF);            //
  sendCommand(0xd9);            //SETPRECHARGE 
  sendCommand(0xF1); 
  sendCommand(0xDB);            //SETVCOMDETECT                
  sendCommand(0x40);
  sendCommand(0xA4);            //DISPLAYALLON_RESUME        
  sendCommand(0xA6);            //NORMALDISPLAY             
  sendCommand(0x2E);            //Stop scroll
  sendCommand(0x20);            //Set Memory Addressing Mode
  sendCommand(0x00);            //Set Memory Addressing Mode ab Horizontal addressing mode
}

void TOGoS::SSD1306::Driver::sendCommand(uint8_t command) {
  this->twi.beginTransmission(this->address);    // begin I2C communication
  this->twi.write(SSD1306_Command_Mode);   // Set OLED Command mode
  this->twi.write(command);
  this->twi.endTransmission();             // End I2C communication
}

void TOGoS::SSD1306::Driver::sendData(uint8_t data) {
  this->twi.beginTransmission(this->address); // begin I2C transmission
  this->twi.write(SSD1306_Data_Mode);            // data mode
  this->twi.write(data);
  this->twi.endTransmission();                    // stop I2C transmission

  // Increment where we think we are, assuming the usual addressing mode:
  ++this->atColumn;
  if( this->atColumn == this->columnCount ) {
    this->atColumn = 0;
    ++this->atRow;
    if( this->atRow == this->rowCount ) this->atRow = 0;
  }
}

void TOGoS::SSD1306::Driver::displayOn() {
  this->sendCommand(SSD1306_Display_On_Cmd);     //display on
}

void TOGoS::SSD1306::Driver::setBrightness(uint8_t brightness) {
   sendCommand(SSD1306_Set_Brightness_Cmd);
   sendCommand(brightness);
}

void TOGoS::SSD1306::Driver::gotoRowCol(uint8_t row, uint8_t col) {
  this->sendCommand(0xB0 + row);                          //set page address
  this->sendCommand(0x00 + (col & 0x0F));    //set column lower addr
  this->sendCommand(0x10 + ((col>>4)&0x0F)); //set column higher addr
  this->atColumn = col;
  this->atRow = row;
}

void TOGoS::SSD1306::Driver::clear(uint8_t data) {
  this->gotoRowCol(0,0);
  do {
    this->sendData(data);
  } while( this->atColumn != 0 || this->atRow != 0 );
}

void TOGoS::SSD1306::Driver::clearToEndOfRow(uint8_t data) {
  do {
    this->sendData(data);
  } while( this->atColumn != 0 );
}
