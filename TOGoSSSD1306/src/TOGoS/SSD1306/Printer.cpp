#include "Printer.h"
#include "font.h"

size_t TOGoS::SSD1306::Printer::write(uint8_t ch) {
  int fontWidth = TOGOS_SSD1306_FONTDATA_READ_BYTE(&font[0]);
  int fontOffset = 2; // font 'header' size
  if(ch == '\r') {
    return 1;
  }
  if(ch == '\n') {
    this->driver.clearToEndOfRow();
    return 1;
  }
  if(ch < 32 || ch > 127) {
    ch = ' ';
  }
  for(uint8_t i=0; i<fontWidth; i++) {
    // Font array starts at 0, ASCII starts at 32
    this->driver.sendData(this->xorPattern ^ TOGOS_SSD1306_FONTDATA_READ_BYTE(&this->font[(ch-32)*fontWidth+fontOffset+i])); 
  }
  return 1;
}
