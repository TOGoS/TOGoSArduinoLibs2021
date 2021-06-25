#ifndef TOGOS_SSD1306_PRINTER_H
#define TOGOS_SSD1306_PRINTER_H

#include "Driver.h"

namespace TOGoS { namespace SSD1306 {
  class Printer : public Print {
    TOGoS::SSD1306::Driver &driver;
    const uint8_t *font;
    uint8_t xorPattern;
  public:
    Printer(TOGoS::SSD1306::Driver &driver, const uint8_t *font) : driver(driver), font(font) {}
    void setXor(uint8_t pattern) { this->xorPattern = pattern; }
    virtual size_t write(uint8_t ch) override;
    virtual int availableForWrite() const {
      return true;
    }
  };
}}

#endif
