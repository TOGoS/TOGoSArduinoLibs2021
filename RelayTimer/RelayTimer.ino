// This should be a very simple program
// I may be overengineering it.
// 
// [2025-03-09]: For one thing, trying to make it be both a one-shot
// timer and a loopy one.  I guess I figure the two applications are
// similar enough tto share all the boilerplate for commands and
// stuff.  But then maybe I should put all the shared bits in a
// library and have separate applications instantiate them with
// different parameters.  Hmm.

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

	enum SBInputEvent {
		SHORT_PRESS = 1,
		LONG_PRESS = 2,
	};

	class Timer {
	public:
		virtual const char *getName() = 0;
		virtual void input(SBInputEvent type, unsigned long currentTime) = 0;
		virtual bool isRelayOnAt(unsigned long currentTime) = 0;
		virtual bool isIndicatorOnAt(unsigned long currentTime) = 0;
	};

	class OneShotTimer : public Timer {
	public:
		unsigned long resetTime = 0;
		unsigned long activeDuration = 0;
		unsigned long shortPressTimerIncrement = 1000*3600;
		virtual const char *getName() {
			return "OneShotTimer";
		};
		void input(SBInputEvent type, unsigned long currentTime) {
			switch( type ) {
			case SBInputEvent::SHORT_PRESS:
				{
					if( this->resetTime < currentTime ) {
						this->resetTime = currentTime;
					}
					this->resetTime += shortPressTimerIncrement;
					long minutesUntil = (this->resetTime - currentTime) / 60000;
					Serial << "# " << minutesUntil << " minutes until reset\n";
				}
				break;
			case SBInputEvent::LONG_PRESS:
				this->resetTime = currentTime;
				Serial << "# Cleared timer; switch off\n";
				break;
			}
		}
		bool isRelayOnAt(unsigned long currentTime) {
			return currentTime < this->resetTime;
		}
		bool isIndicatorOnAt(unsigned long currentTime) {
			const unsigned long indicatorMaxHours = 8;
			const unsigned long indicatorCycleTicks = 512;
			const unsigned long indicatorHourTicks = indicatorCycleTicks / indicatorMaxHours;
		
			int buttonIndicatorPhase = (int)(currentTime / 10);
		
			if( (buttonIndicatorPhase & 0x3FF) == 0 ) {
				long minutesUntil = this->resetTime > currentTime ? (this->resetTime-currentTime) / 60000 : 0;
				Serial << "# Timer: " << minutesUntil << " minutes left.\n";
			}

			bool active = this->resetTime > currentTime;
			// BUILTIN_LED flashes the number of hours left
			long indicatorOnDutyCycle = active ? (this->resetTime - currentTime) * indicatorCycleTicks / (indicatorMaxHours*3600000) : 0;
			bool indicatorOnMask = (buttonIndicatorPhase & (indicatorHourTicks-1)) < (indicatorHourTicks * 7 / 8);
			return (indicatorOnMask && (buttonIndicatorPhase & (indicatorCycleTicks-1)) < indicatorOnDutyCycle);
		}
	};
	/*
	class LoopTimer : Timer {
	public:
		unsigned long cycleStartTime = 0;
		unsigned long cycleDuration = 3600*24;
		unsigned long activeDuration = 0;
	};
	*/
}

// config.h should declare a `constexpr TOGoS::Arduino::RelayTimer::AppConfig appConfig`:
#include "config.h"

using TLIBuffer = TOGoS::Command::TLIBuffer;
using TokenizedCommand = TOGoS::Command::TokenizedCommand;
using SBInputEvent = TOGoS::Arduino::RelayTimer::SBInputEvent;

TOGoS::Arduino::RelayTimer::Timer *theTimer = new TOGoS::Arduino::RelayTimer::OneShotTimer();

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
  Serial << "# Timer:\n";
  Serial << "#  name = " << theTimer->getName() << "\n";
}

TLIBuffer commandBuffer;

/** Time at which the relay should switch back to its default state, in millis */
unsigned long relayResetTime = 0;
unsigned long currentTickTime = 0;
unsigned long shortPressTimerIncrement = 1000*3600;

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
		theTimer->input(SBInputEvent::SHORT_PRESS, currentTickTime);
	} else if( tcmd.path == "button/long-press" ) {
		theTimer->input(SBInputEvent::LONG_PRESS, currentTickTime);
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

TOGoS::Arduino::RelayTimer::Relay<appConfig.relayControlPin, appConfig.relayIsActiveLow> theRelay;
TOGoS::Arduino::RelayTimer::Button<appConfig.buttonPin, appConfig.buttonIsActiveLow> theButton;

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
					theTimer->input(SBInputEvent::SHORT_PRESS, currentTickTime);
				} else {
					theTimer->input(SBInputEvent::LONG_PRESS, currentTickTime);
				}
			}
			
			buttonDownTime = -1;
		}
	}
	
	digitalWrite(LED_BUILTIN, theTimer->isIndicatorOnAt(currentTickTime) ? LOW : HIGH);
	theRelay.set(theTimer->isRelayOnAt(currentTickTime));
	while( Serial.available() > 0 ) {
		TLIBuffer::BufferState bufState = commandBuffer.onChar(Serial.read());
		if( bufState == TLIBuffer::BufferState::READY ) {
			processLine( commandBuffer.str() );
			commandBuffer.reset();
		}
	}
	delay(10);
}
