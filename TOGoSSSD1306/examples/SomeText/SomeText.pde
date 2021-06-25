#include <TOGoSSSD1306.h>

#include <TOGoS/SSD1306/font8x8.h>
#include <TOGoS/SSD1306/Printer.h>

TOGoS::SSD1306::Driver oled(Wire, 0x3C);
TOGoS::SSD1306::Printer oledPrinter(oled, TOGoS::SSD1306::font8x8);

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("Welcome to SomeText.pde, the TOGoSSSD1306 example!");
  delay(250);

  Serial.println("Wire.begin()...");
  Wire.begin();
  Serial.println("oled.begin()...");
  oled.begin();
  oled.displayOn();
  oled.setBrightness(255);
  Serial.println("oled.clear()...");
  oled.clear();
  oled.gotoRowCol(0,0);
  Serial.println("oled.sendData()...");
  oled.sendData(0xA5);
  oled.sendData(0x5A);
  oled.sendData(0x00);
  oledPrinter.setXor(0xFF);
  oledPrinter.print("Initializing!");
  oled.clearToEndOfRow(0xFF);
  oled.clearToEndOfRow(0x3F);
  oledPrinter.setXor(0x00);
  delay(5000);
  oled.clear();
  oled.gotoRowCol(0,0);
  oledPrinter.print("Header 1!");
  oled.gotoRowCol(1,0);
  oledPrinter.print("Header 2!");
  oled.gotoRowCol(2,0);
  oledPrinter.print("Body 1!");

  //oled.activateScroll();

  /*
  oled.sendCommand(0xA3);
  oled.sendData(2);
  oled.sendCommand(0xB6);
  oled.sendData(6);
  */
}

int hiCount = 0;

void loop() {
  // This causes the lower part of the screen to scroll horizontally
  oled.sendCommand(0x29);
  oled.sendCommand(0x00);
  oled.sendCommand(0x02);
  oled.sendCommand(0x00);
  oled.sendCommand(0x07);
  oled.sendCommand(hiCount);
  oled.sendCommand(0x2F);
  delay(100);
}

void xloop() {
  oled.sendData(0x00);
  oled.sendData(0x00);
  oledPrinter.print("Hi #");
  oledPrinter.print(hiCount++);
  oled.sendData(0x00);
  oled.sendData(0x00);
  oled.sendData(0x01);
  //delay(100);
  oled.sendData(0x03);
  //delay(100);
  oled.sendData(0x07);
  //delay(100);
  oled.sendData(0x0F);
  //delay(100);
  oled.sendData(0x1F);
  //delay(100);
  oled.sendData(0x3F);
  //delay(100);
  oled.sendData(0x7F);
  //delay(100);
  oled.sendData(0xFF);
  //delay(100);
}
