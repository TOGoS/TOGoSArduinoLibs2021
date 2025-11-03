// This should be a very simple program
// I may be overengineering it.
// 
// [2025-03-09]: For one thing, trying to make it be both a one-shot
// timer and a loopy one.  I guess I figure the two applications are
// similar enough tto share all the boilerplate for commands and
// stuff.  But then maybe I should put all the shared bits in a
// library and have separate applications instantiate them with
// different parameters.  Hmm.
//
// Requires TOGoSArduinoLibs 56c698e86a76a9cafb81a923b8d2044f01ad5d90
// (whatever versions of individual libraries that entails)

#define TAA_RELAYTIMER_COARSE_VERSION "3.0.13"

#include <optional>

#include <TOGoSCommand.h>
#include <TOGoS/Command/ParseResult.h>
#include <TOGoS/Command/TLIBuffer.h>
#include <TOGoS/Command/TokenizedCommand.h>
#include <TOGoSStreamOperators.h>

namespace TOGoS::Arduino::RelayTimer {
	//// Some generic stuff that might be moved to a library
	
	class PropConsumer {
	public:
		virtual void accept(const char *name, const char *value);
		virtual void accept(const char *name, int value);
		virtual void accept(const char *name, unsigned long value);
	};

	class PrefixPropConsumer : public PropConsumer {
		Print &printer;
		const char *prefix;
		const char *kvSep;
		const char *postfix;
	public:
		PrefixPropConsumer(Print &printer, const char *prefix, const char *kvSep, const char *postfix) :
			printer(printer), prefix(prefix), kvSep(kvSep), postfix(postfix) { }
		void accept(const char *name, const char *value) {
			printer << prefix << name << kvSep << value << postfix;
		}
		void accept(const char *name, int value) {
			printer << prefix << name << kvSep << value << postfix;
		}
		void accept(const char *name, unsigned long value) {
			printer << prefix << name << kvSep << value << postfix;
		}
	};
	
	////
	
	enum TimerMode {
		ONE_SHOT,
		LOOPING
	};
	
	struct AppConfig {
		const char *appName;
		const char *appVersion;
		int relayControlPin;
		bool relayIsActiveLow;
		int buttonPin;
		bool buttonIsActiveLow;
		TimerMode timerMode;
		union {
			struct {
				// Used by ONE_SHOT
				unsigned long shortPressTimerIncrement;
			} oneShotConfig;
			struct {
				// Used by LOOPING
				unsigned long onDuration;
				unsigned long loopDuration;
			} loopingConfig;
		};

		void emitProps(PropConsumer &dest) const;
	};
	
	template <int pin, bool activeLow>
	class Relay {
	public:
		void set(bool on) {
			digitalWrite(pin, (on ^ activeLow) ? HIGH : LOW);
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
		virtual void emitProps(PropConsumer &dest) const = 0;
		virtual void reset(unsigned long currentTime) = 0;
		virtual void input(SBInputEvent type, unsigned long currentTime) = 0;
		virtual bool isRelayOnAt(unsigned long currentTime) = 0;
		virtual bool isIndicatorOnAt(unsigned long currentTime) = 0;
	};
	
	class OneShotTimer : public Timer {
	public:
		unsigned long resetTime = 0;
		unsigned long activeDuration = 0;
		unsigned long shortPressTimerIncrement = 1000*3600;
		OneShotTimer(long shortPressTimerIncrement) : shortPressTimerIncrement(shortPressTimerIncrement) { }
		const char *getName() {
			return "OneShotTimer";
		};
		void emitProps(PropConsumer &dest) const {
			dest.accept("type", "OneShotTimer");
			dest.accept("resetTime", resetTime);
			dest.accept("activeDuration", activeDuration);
			dest.accept("shortPressTimerIncrement", shortPressTimerIncrement);
		}
		void reset(unsigned long currentTime) {
			this->resetTime = currentTime;
		}
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
	
	class LoopTimer : Timer {
	public:
		unsigned long cycleStartTime = 0;
		unsigned long cycleDuration  = 3600*1000*24;
		unsigned long activeDuration = 3600*1000*12;
		LoopTimer(unsigned long onDuration, unsigned long loopDuration) : activeDuration(onDuration), cycleDuration(loopDuration) { }
		const char *getName() {
			return "LoopTimer";
		};
		void emitProps(PropConsumer &dest) const {
			dest.accept("type", "LoopTimer");
			dest.accept("cycleStartTime", cycleStartTime);
			dest.accept("cycleDuration", cycleDuration);
			dest.accept("activeDuration", activeDuration);
		}
		void reset(unsigned long currentTime) {
			this->cycleStartTime = currentTime;
		}
		void input(SBInputEvent type, unsigned long currentTime) {
			switch( type ) {
			case SBInputEvent::SHORT_PRESS:
				this->cycleStartTime = currentTime;
				Serial << "# Jump to on time\n";
				break;
			case SBInputEvent::LONG_PRESS:
				this->cycleStartTime = currentTime - activeDuration;
				Serial << "# Jump to off time\n";
				break;
			}
		}
		bool isRelayOnAt(unsigned long currentTime) {
			long phase = (currentTime - this->cycleStartTime) % this->cycleDuration;
			return phase < this->activeDuration;
		}
		bool isIndicatorOnAt(unsigned long currentTime) {
			return this->isRelayOnAt(currentTime);
		}
	};
}

void TOGoS::Arduino::RelayTimer::AppConfig::emitProps(TOGoS::Arduino::RelayTimer::PropConsumer &dest) const {
	dest.accept("appName"          , appName          );
	dest.accept("appVersion"       , appVersion       );
	dest.accept("relayControlPin"  , relayControlPin  );
	dest.accept("relayIsActiveLow" , relayIsActiveLow );
	dest.accept("buttonPin"        , buttonPin        );
	dest.accept("buttonIsActiveLow", buttonIsActiveLow);
}

// config.h should declare a `constexpr TOGoS::Arduino::RelayTimer::AppConfig appConfig`:
#include "config.h"

using TLIBuffer = TOGoS::Command::TLIBuffer;
using TokenizedCommand = TOGoS::Command::TokenizedCommand;
using SBInputEvent = TOGoS::Arduino::RelayTimer::SBInputEvent;

TOGoS::Arduino::RelayTimer::Timer *theTimer =
	appConfig.timerMode == TOGoS::Arduino::RelayTimer::TimerMode::ONE_SHOT ? (TOGoS::Arduino::RelayTimer::Timer *) new TOGoS::Arduino::RelayTimer::OneShotTimer(
		appConfig.oneShotConfig.shortPressTimerIncrement
	) :
	(TOGoS::Arduino::RelayTimer::Timer *) new TOGoS::Arduino::RelayTimer::LoopTimer(
		appConfig.loopingConfig.onDuration,
		appConfig.loopingConfig.loopDuration
	);

std::optional<long> buttonDownTime = {};
unsigned long currentTickTime = 0;
std::optional<bool> previousRelayState = false;

TOGoS::Arduino::RelayTimer::Relay<appConfig.relayControlPin, appConfig.relayIsActiveLow> theRelay;
TOGoS::Arduino::RelayTimer::Button<appConfig.buttonPin, appConfig.buttonIsActiveLow> theButton;

void printHelp() {
	Serial << "# Welcome to " << appConfig.appName << "\n";
	Serial << "# Version: " << appConfig.appVersion << "\n";
	Serial << "# Commands:\n";
	Serial << "#   help     ; print this help\n";
	Serial << "#   echo ... ; echo stuff back to serial\n";
	Serial << "#   info     ; show constants and other info\n";
	Serial << "#   button/long-press  ; do long-press action\n";
	Serial << "#   button/short-press ; do short-press action\n";
}

void emitPinConstants(TOGoS::Arduino::RelayTimer::PropConsumer &dest) {
	dest.accept("D0", D0);
	dest.accept("D1", D1);
	dest.accept("D2", D2);
	dest.accept("D3", D3);
	dest.accept("D4", D4);
	dest.accept("D5", D5);
	dest.accept("D6", D6);
	dest.accept("D7", D7);
	dest.accept("D8", D8);
	dest.accept("LED_BUILTIN", LED_BUILTIN);
}

void printInfo() {
	TOGoS::Arduino::RelayTimer::PrefixPropConsumer infoPropEmitter = TOGoS::Arduino::RelayTimer::PrefixPropConsumer(Serial, "#  ", " = ", "\n");
	Serial << "# Pins constants:\n";
	emitPinConstants(infoPropEmitter);
	Serial << "# App config:\n";
	appConfig.emitProps(infoPropEmitter);
	Serial << "# Other constants:\n";
	Serial << "#  HIGH = " << HIGH << "\n";
	Serial << "#  LOW  = " << LOW << "\n";
	Serial << "# Timer:\n";
	theTimer->emitProps(infoPropEmitter);
	// Serial << "#  name = \"" << theTimer->getName() << "\"\n";
}

TLIBuffer commandBuffer;

void processLine(const TOGoS::StringView& line) {
	if( line.size() == 0 ) return;
	if( line[0] == '#' ) return;

	TOGoS::Command::ParseResult<TOGoS::Command::TokenizedCommand> parseResult = TOGoS::Command::TokenizedCommand::parse(line);

	if( parseResult.isError() ) {
		Serial << "# Error parsing '" << line << "': " << parseResult.error.getErrorMessage() << "\n";
		return;
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
	} else if( tcmd.path == "info" ) {
		printInfo();
	} else if( tcmd.path == "button/short-press" ) {
		theTimer->input(SBInputEvent::SHORT_PRESS, currentTickTime);
	} else if( tcmd.path == "button/long-press" ) {
		theTimer->input(SBInputEvent::LONG_PRESS, currentTickTime);
	} else {
		Serial << "# Unrecognized command: '" << tcmd.path << "'; try 'help'.\n";
	}
}

void setup() {
	delay(1000); // Standard 'give me time to reprogram it' delay
	Serial.begin(115200);
	Serial << "# " << appConfig.appName << " setup()\n";
	Serial << "# Version: " << appConfig.appVersion << "\n";
	
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(appConfig.relayControlPin, OUTPUT);
	pinMode(appConfig.buttonPin, INPUT);
	
	currentTickTime = millis();
	Serial << "# Resetting timer at " << currentTickTime << "\n";
	theTimer->reset(currentTickTime);
}

void loop() {
	currentTickTime = millis();
	
	if( theButton.isPressed() ) {
		if( !buttonDownTime.has_value() ) {
			buttonDownTime = currentTickTime;
		}
	} else {
		if( buttonDownTime.has_value() ) {
			long buttonPressTime = currentTickTime - *buttonDownTime;
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
			
			buttonDownTime = {};
		}
	}
	
	digitalWrite(LED_BUILTIN, theTimer->isIndicatorOnAt(currentTickTime) ? LOW : HIGH);
	bool shouldRelayBeOn = theTimer->isRelayOnAt(currentTickTime);
	if( !previousRelayState.has_value() || shouldRelayBeOn != *previousRelayState ) {
		Serial << "# Switching relay " << (shouldRelayBeOn ? "on" : "off") << "\n";
		theRelay.set(shouldRelayBeOn);
		previousRelayState = shouldRelayBeOn;
	}
	while( Serial.available() > 0 ) {
		TLIBuffer::BufferState bufState = commandBuffer.onChar(Serial.read());
		if( bufState == TLIBuffer::BufferState::READY ) {
			processLine( commandBuffer.str() );
			commandBuffer.reset();
		}
	}
	delay(10);
}
