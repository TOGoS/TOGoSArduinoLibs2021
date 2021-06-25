#ifndef TOGOS_SSD1306_FON5_H
#define TOGOS_SSD1306_FONT_H

// Make font stuff work
//#ifdef __AVR__
//  #include <avr/pgmspace.h>
//  #define OLEDFONT(name) static const uint8_t __attribute__ ((progmem))_n[]
//#elif defined(ESP8266)
//  [2021-06-25] This seems to just make things crash, so commenting-out for now!
//  #include <pgmspace.h>
//  #define OLEDFONT(name) static const uint8_t name[]
//#else
  #define TOGOS_SSD1306_FONTDATA_READ_BYTE(addr) (*(const uint8_t *)(addr))
  #define TOGOS_SSD1306_FONTDATA(name) static const uint8_t name[]
//#endif

#endif
