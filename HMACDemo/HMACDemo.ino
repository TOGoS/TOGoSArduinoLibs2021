#include <Arduino.h>
#include <Hash.h>

void setup() {
  Serial.begin(115200);

  String result = sha1("test string");

  Serial.println();
  Serial.print(result);
}

void loop() {
	delay(1000);
}
