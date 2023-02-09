#include <TOGoSCommand.h>
#include <TOGoS/Command/ParseResult.h>
#include <TOGoS/Command/TLIBuffer.h>
#include <TOGoS/Command/TokenizedCommand.h>
#include <TOGoSStreamOperators.h>

#define APP_NAME "RelayTimer.ino"

#include "config.h"

using TLIBuffer = TOGoS::Command::TLIBuffer;
using TokenizedCommand = TOGoS::Command::TokenizedCommand;

template <int pin, bool activeLow>
class Relay {
public:
	void set(bool on) {
		digitalWrite(pin, (on ^ activeLow) ? LOW : HIGH);
	}
};

/** Time at which the relay should switch back to its default state */
long timerResetMillis = 0;

void printHelp() {
	Serial << "# Welcome to " << APP_NAME << "\n";
	Serial << "# Commands:\n";
	Serial << "#   echo ... ; echo stuff back to serial\n";
	Serial << "#   pins     ; dump hardware pin numbers\n";
}

void printPins() {
  Serial << "# Pins constants:\n";
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
  Serial << "#  RELAY_CONTROL_PIN = " << RELAY_CONTROL_PIN << "\n";
  Serial << "#  BUTTON_PIN = " << BUTTON_PIN << "\n";
}

TLIBuffer commandBuffer;

void processLine(const TOGoS::StringView& line) {
	if( line.size() == 0 ) return;
	if( line[0] == '#' ) return;

	TOGoS::Command::ParseResult<TOGoS::Command::TokenizedCommand> parseResult = TOGoS::Command::TokenizedCommand::parse(line);

	if( parseResult.isError() ) {
		Serial << "# Error parsing '" << line << "': " << parseResult.error.getErrorMessage() << "\n";
	} else {
		Serial << "# That parsed Okay.\n";
	}

	const TokenizedCommand &tcmd = parseResult.value;
	if( tcmd.path == "" ) {
		return;
	} else if( tcmd.path == "echo" ) {
		for( int i=0; i<tcmd.args.size(); ++i ) {
			if( i > 0 ) Serial << " ";
			Serial << tcmd.args[i];
		}
		Serial << "\n";
	} else if( tcmd.path == "help" ) {
		printHelp();
	} else if( tcmd.path == "pins" ) {
		printPins();
	}
}

void setup() {
	delay(1000); // Standard 'give me time to reprogram it' delay
	Serial.begin(115200);
	Serial << "# "APP_NAME" setup()\n";
	
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(RELAY_CONTROL_PIN, OUTPUT);
	pinMode(BUTTON_PIN, INPUT);
}

void loop() {
	while( Serial.available() > 0 ) {
		TLIBuffer::BufferState bufState = commandBuffer.onChar(Serial.read());
		if( bufState == TLIBuffer::BufferState::READY ) {
			processLine( commandBuffer.str() );
			commandBuffer.reset();
		}
	}
	delay(10);
}
