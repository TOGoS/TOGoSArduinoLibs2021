#include <string>
#include <Arduino.h>
#include <Hash.h>
#include <TOGoSStreamOperators.h>

char hexDigit(int num) {
  num = num & 0xF;
  if( num < 10 ) return '0' + num;
  if( num < 16 ) return 'a' + num - 10;
  return '?'; // Should be unpossible
}

std::string toHex(uint8_t *data, size_t len) {
  std::string hecks;
  for( off_t i = 0; i < len; ++i ) {
    hecks += hexDigit(data[i] >> 4);
    hecks += hexDigit(data[i]);
  }
  return hecks;
}

void setup() {
	uint8_t sha1Buf[20];
	const char *message = "Hello, world!";
	
	Serial.begin(115200);
	sha1(message, strlen(message), sha1Buf);
	
	delay(4000);
	
	Serial << "\n";
	Serial << "hex(sha1(\"" << message << "\")): ";
	Serial << toHex(sha1Buf, sizeof(sha1Buf)).c_str();
	Serial << "\n";
}

void loop() {
	delay(1000);
}
