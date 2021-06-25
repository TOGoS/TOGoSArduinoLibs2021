#ifndef TOGOS_SSD1306_DRIVER_H
#define TOGOS_SSD1306_DRIVER_H

#include <Wire.h>

namespace TOGoS { namespace SSD1306 {
  class Driver {
    static const uint8_t rowCount = 8;
    static const uint8_t columnCount = 128;
    
    static const uint8_t SSD1306_Command_Mode         = 0x80;
    static const uint8_t SSD1306_Data_Mode            = 0x40;
    static const uint8_t SSD1306_Display_Off_Cmd      = 0xAE;
    static const uint8_t SSD1306_Display_On_Cmd       = 0xAF;
    static const uint8_t SSD1306_Normal_Display_Cmd   = 0xA6;
    static const uint8_t SSD1306_Inverse_Display_Cmd  = 0xA7;
    static const uint8_t SSD1306_Activate_Scroll_Cmd  = 0x2F;
    static const uint8_t SSD1306_Dectivate_Scroll_Cmd = 0x2E;
    static const uint8_t SSD1306_Set_Brightness_Cmd   = 0x81;

    static const uint8_t DEFAULT_ADDRESS              = 0x3C;
    static const uint8_t ALTERNATE_ADDRESS            = 0x3D;
    
    TwoWire *i2c;
    uint8_t address;
  public:
    Driver(TwoWire &i2c) : i2c(&i2c) { }
    void begin(uint8_t address=DEFAULT_ADDRESS);
    void sendCommand(uint8_t command);
    void sendData(uint8_t data);
    
    void displayOn();
    void clear(uint8_t data=0x00);
    void clearToEndOfRow(uint8_t data=0x00);
    void setBrightness(uint8_t brightness);
    void gotoRowCol(uint8_t row, uint8_t col);
    
    uint8_t atRow = 0, atColumn = 0;

    int getRowCount() { return rowCount; }
    int getColumnCount() { return columnCount; }
  };
}}

#endif
