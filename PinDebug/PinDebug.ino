#include <TOGoSStreamOperators.h>

#define APP_NAME "PinDebug.ino"

void setup() {
	delay(1000); // Standard 'give me time to reprogram it' delay
	Serial.begin(115200);
	Serial << "# "APP_NAME" setup()\n";
	// put your setup code here, to run once:
}

void loop() {
	// put your main code here, to run repeatedly:
	Serial << "# Hi, this is "APP_NAME"\n";
	Serial << "# I just print out the pin mappings\n";
	Serial << "#\n";
	Serial << "#  D0 = " << D0 << "\n";
	Serial << "#  D1 = " << D1 << "\n";
	Serial << "#  D2 = " << D2 << "\n";
	Serial << "#  D3 = " << D3 << "\n";
	Serial << "#  D4 = " << D4 << "\n";
	Serial << "#  D5 = " << D5 << "\n";
	Serial << "#  D6 = " << D6 << "\n";
	Serial << "#  D7 = " << D7 << "\n";
	Serial << "#  D8 = " << D8 << "\n";
	Serial << "#  LED_BUILTIN = " << LED_BUILTIN << "\n";
	delay(2000);
	Serial << "\n\n\n";
}
