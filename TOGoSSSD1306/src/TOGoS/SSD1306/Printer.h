#ifndef TOGOS_SSD1306_PRINTER_H
#define TOGOS_SSD1306_PRINTER_H

#include "Driver.h"

namespace TOGoS { namespace SSD1306 {
  class Printer : public Print {
    TOGoS::SSD1306::Driver &driver;
    const uint8_t *font;
    uint8_t xorPattern = 0;
  public:
    Printer(TOGoS::SSD1306::Driver &driver, const uint8_t *font) : driver(driver), font(font) {}
    void gotoRowCol(uint8_t row, uint8_t col) { this->driver.gotoRowCol(row,col); }
    void setXor(uint8_t pattern) { this->xorPattern = pattern; }
    void clearToEndOfRow() { this->driver.clearToEndOfRow(this->xorPattern); }
    virtual size_t write(uint8_t ch) override;
    virtual int availableForWrite() const {
      return true;
    }
  };
}}

#endif
