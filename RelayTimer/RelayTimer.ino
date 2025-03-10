#include <TOGoSCommand.h>
#include <TOGoS/Command/ParseResult.h>
#include <TOGoS/Command/TLIBuffer.h>
#include <TOGoS/Command/TokenizedCommand.h>
#include <TOGoSStreamOperators.h>

namespace TOGoS::Arduino::RelayTimer {
	enum TimerMode {
		ONE_SHOT,
		LOOPING
	};
	struct AppConfig {
		const char *appName;
		int relayControlPin;
		bool relayIsActiveLow;
		int buttonPin;
		bool buttonIsActiveLow;
		TimerMode timerMode;
	};
	
	template <int pin, bool activeLow>
	class Relay {
	public:
		void set(bool on) {
			digitalWrite(pin, (on ^ activeLow) ? LOW : HIGH);
		}
	};
	
	template <int pin, bool activeLow>
	class Button {
	public:
		bool isPressed() {
			return digitalRead(pin) == (activeLow ? LOW : HIGH);
		}
	};
}

// config.h should declare a `constexpr TOGoS::Arduino::RelayTimer::AppConfig appConfig`:
#include "config.h"

using TLIBuffer = TOGoS::Command::TLIBuffer;
using TokenizedCommand = TOGoS::Command::TokenizedCommand;

void printHelp() {
	Serial << "# Welcome to " << appConfig.appName << "\n";
	Serial << "# Commands:\n";
	Serial << "#   echo ... ; echo stuff back to serial\n";
	Serial << "#   pins     ; dump hardware pin numbers\n";
}

void printConstants() {
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
  Serial << "#  appConfig.relayControlPin = " << appConfig.relayControlPin << "\n";
  Serial << "#  appConfig.buttonPin = " << appConfig.buttonPin << "\n";
  Serial << "# Other constants:\n";
  Serial << "#  HIGH = " << HIGH << "\n";
  Serial << "#  LOW  = " << LOW << "\n";
}

TLIBuffer commandBuffer;

/** Time at which the relay should switch back to its default state, in millis */
unsigned long relayResetTime = 0;
unsigned long currentTickTime = 0;
unsigned long shortPressTimerIncrement = 1000*3600;

void shortPress() {
	if( relayResetTime < currentTickTime ) {
		relayResetTime = currentTickTime;
	}
	relayResetTime += shortPressTimerIncrement;
	long minutesUntil = (relayResetTime - currentTickTime) / 60000;
	Serial << "# " << minutesUntil << " minutes until reset\n";
}
void longPress() {
	relayResetTime = currentTickTime;
	Serial << "# Cleared timer; switch off\n";
}

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
	} else if( tcmd.path == "constants" || tcmd.path == "pins" ) {
		printConstants();
	} else if( tcmd.path == "button/short-press" ) {
		shortPress();
	} else if( tcmd.path == "button/long-press" ) {
		longPress();
	}
}

void setup() {
	delay(1000); // Standard 'give me time to reprogram it' delay
	Serial.begin(115200);
	Serial << "# " << appConfig.appName << " setup()\n";
	
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(appConfig.relayControlPin, OUTPUT);
	pinMode(appConfig.buttonPin, INPUT);
}

int buttonIndicatorPhase = 0;

TOGoS::Arduino::RelayTimer::Relay<appConfig.relayControlPin, appConfig.relayIsActiveLow> theRelay;
TOGoS::Arduino::RelayTimer::Button<appConfig.buttonPin, appConfig.buttonIsActiveLow> theButton;

const unsigned long indicatorMaxHours = 8;
const unsigned long indicatorCycleTicks = 512;
const unsigned long indicatorHourTicks = indicatorCycleTicks / indicatorMaxHours;

long buttonDownTime = -1;

void loop() {
	currentTickTime = millis();

	if( theButton.isPressed() ) {
		if( buttonDownTime == -1 ) {
			buttonDownTime = currentTickTime;
		}
	} else {
		if( buttonDownTime != -1 ) {
			long buttonPressTime = currentTickTime - buttonDownTime;
			if( buttonPressTime < 100 ) {
				// Ignore
			} else {
				Serial << "# Button was held down for " << buttonPressTime << "ms\n";
				if( buttonPressTime < 500 ) {
					shortPress();
				} else {
					longPress();
				}
			}
			
			buttonDownTime = -1;
		}
	}
	
	bool active = relayResetTime > currentTickTime;
	// BUILTIN_LED flashes the number of hours left
	long indicatorOnDutyCycle = active ? (relayResetTime - currentTickTime) * indicatorCycleTicks / (indicatorMaxHours*3600000) : 0;
	bool indicatorOnMask = (buttonIndicatorPhase & (indicatorHourTicks-1)) < (indicatorHourTicks * 7 / 8);
	bool indicatorOn = (indicatorOnMask && (buttonIndicatorPhase & (indicatorCycleTicks-1)) < indicatorOnDutyCycle);
	digitalWrite(LED_BUILTIN, indicatorOn ? LOW : HIGH);
	theRelay.set(active);
	if( (buttonIndicatorPhase & 0x3FF) == 0 ) {
		long minutesUntil = relayResetTime > currentTickTime ? (relayResetTime-currentTickTime) / 60000 : 0;
		Serial << "# Timer: " << minutesUntil << " minutes left.\n";
	}
	while( Serial.available() > 0 ) {
		TLIBuffer::BufferState bufState = commandBuffer.onChar(Serial.read());
		if( bufState == TLIBuffer::BufferState::READY ) {
			processLine( commandBuffer.str() );
			commandBuffer.reset();
		}
	}
	delay(10);
	++buttonIndicatorPhase;
}
