#include <TOGoSCommand.h>
#include <TOGoS/Command/ParseResult.h>
#include <TOGoS/Command/TLIBuffer.h>
#include <TOGoS/Command/TokenizedCommand.h>
#include <TOGoSStreamOperators.h>

#define APP_NAME "RelayTimer.ino"

#include "config.h"

/** Time at which the relay should switch back to its default state */
long timerResetMillis = 0;

template <int pin, bool activeLow>
class Relay {
public:
	void set(bool on) {
		digitalWrite(pin, (on ^ activeLow) ? LOW : HIGH);
	}
};

TOGoS::Command::TLIBuffer commandBuffer;

void processLine(const TOGoS::StringView& line) {
	if( line.size() == 0 ) return;
	if( line[0] == '#' ) return;

	TOGoS::Command::ParseResult<TOGoS::Command::TokenizedCommand> parseResult = TOGoS::Command::TokenizedCommand::parse(line);

	if( parseResult.isError() ) {
		Serial << "# Error parsing '" << line << "': " << parseResult.error.getErrorMessage() << "\n";
	} else {
		Serial << "# That parsed Okay.\n";
	}
}

void setup() {
	delay(1000); // Standard 'give me time to reprogram it' delay
	Serial.begin(115200);
	Serial << "# "APP_NAME" setup()\n";
	// put your setup code here, to run once:
}

void loop() {
	while( Serial.available() > 0 ) {
		TOGoS::Command::TLIBuffer::BufferState bufState = commandBuffer.onChar(Serial.read());
		if( bufState == TOGoS::Command::TLIBuffer::BufferState::READY ) {
			processLine( commandBuffer.str() );
			commandBuffer.reset();
		}
	}
	delay(10);
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
