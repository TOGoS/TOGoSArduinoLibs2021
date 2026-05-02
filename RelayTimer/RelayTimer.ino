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

#define TAA_RELAYTIMER_COARSE_VERSION "3.0.23-dev"

#include <optional>

#include <TOGoSCommand.h>
#include <TOGoS/Command/ParseResult.h>
#include <TOGoS/Command/TLIBuffer.h>
#include <TOGoS/Command/TokenizedCommand.h>
#include <TOGoSStreamOperators.h>
#include <TOGoSBufferPrint.h>

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
		const char *sourceRef;
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
		bool active;
	public:
		boolean get() const {
			return this->active;
		}
		void set(bool on) {
			this->active = on;
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
	dest.accept("sourceRef"        , sourceRef        );
	dest.accept("relayControlPin"  , relayControlPin  );
	dest.accept("relayIsActiveLow" , relayIsActiveLow );
	dest.accept("buttonPin"        , buttonPin        );
	dest.accept("buttonIsActiveLow", buttonIsActiveLow);
}

//// Formatting functions

char hexDigit(int num) {
	num = num & 0xF;
	if( num < 10 ) return '0' + num;
	if( num < 16 ) return 'A' + num - 10;
	return '?'; // Should be unpossible
}

std::string hexByte(int num) {
	std::string hecks;
	hecks += hexDigit(num >> 4);
	hecks += hexDigit(num);
	return hecks;
}

std::string macAddressToHex(uint8_t *macAddress, const char *octetSeparator) {
	std::string hecks;
	for( int i = 0; i < 6; ++i ) {
		if( i > 0 ) hecks += octetSeparator;
		hecks += hexDigit(macAddress[i] >> 4);
		hecks += hexDigit(macAddress[i]);
	}
	return hecks;
}

const char *formatBool(int boolish) {
	return boolish ? "true" : "false";
}

const char *onOffStr(bool reg) {
	return reg ? "on" : "off";
}

const char *onOffAutoStr(std::optional<bool> reg) {
	return !reg.has_value() ? "auto" : *reg ? "on" : "off";
}

////

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
std::optional<bool> helloRelayStateOverride = {};
std::optional<bool> previousRelayState = false;

TOGoS::Arduino::RelayTimer::Relay<appConfig.relayControlPin, appConfig.relayIsActiveLow> theRelay;
TOGoS::Arduino::RelayTimer::Button<appConfig.buttonPin, appConfig.buttonIsActiveLow> theButton;


//// WiFi stuff

void updateHeloBroadcast(long currentTime, boolean forceUpdate);
void updateHeloListen();

#ifdef TAA_RELAYTIMER_WIFI_ENABLED

// Copied from EnvironmentalSensor2021

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

struct WiFiCredz {
  const char *ssid;
  const char *password;
  WiFiCredz(const char *ssid, const char *password) : ssid(ssid), password(password) {}
};

// TODO: Maybe these should come from appConfig?
const char *myHostname = NULL; // "relaytimer";
bool useStaticIp4 = true;
const byte myIp4[] = {10, 9, 254, 254};
const byte myIp4Gateway[] = {10, 9, 254, 254};
const byte myIp4Subnet[] = {255, 255, 255, 255};
const int myUdpPort = 16378;

std::vector<WiFiCredz> wifiNetworks;
int wifiNetworkIndex = -1;
unsigned long lastWifiReconnectAttempt = 0;

void configureWifi(ESP8266WiFiClass &wifi);

void configureWifi(ESP8266WiFiClass &wifi) {
	if( useStaticIp4 ) {
		Serial << F("# Configuring with static IPv4 address\n");
		// Unlike SSID/password, stuff config()ured does *not* seem to be retained.
		// So we need to wifi.config(...) each time before wifi.begin(...)ing.
		IPAddress ip4 = myIp4;
		IPAddress ip4Gateway = myIp4Gateway;
		IPAddress ip4Subnet = myIp4Subnet;
		wifi.config(ip4, ip4Gateway, ip4Subnet);
	}
	if( myHostname != NULL ) {
		Serial << F("# Configuring hostname = '") << myHostname << F("'\n");
		wifi.hostname(myHostname); // This needs to come after `config`
	}
	Serial << F("# configureWifi: done\n");
}

bool isWiFiConnected() {
	return WiFi.status() == WL_CONNECTED;
}

void updateWifi(unsigned long currentTime) {
	int status = WiFi.status();
	if( status == WL_CONNECTED || status == WL_IDLE_STATUS ) return;
	if( currentTime - lastWifiReconnectAttempt < 5000 ) return;
	
	Serial << F("# wifiUpdate: not connected; time to attempt [re]connect\n");
	if( wifiNetworks.size() == 0 ) {
		// Try to auto-connect to whatever's in memory
		Serial << F("# No WiFi networks configured; attempting auto-connect to previous network...\n");
		configureWifi(WiFi);
		WiFi.begin();
	} else {
		Serial << F("# wifiUpdate: ") << wifiNetworks.size() << F(" networks configured\n");
		++wifiNetworkIndex;
		if( wifiNetworkIndex >= wifiNetworks.size() ) {
			wifiNetworkIndex = 0;
		}
		
		const WiFiCredz &credz = wifiNetworks[wifiNetworkIndex];
		Serial << "# Attempting auto-connect to " << credz.ssid << "...\n";
		configureWifi(WiFi);
		WiFi.begin(credz.ssid, credz.password);
	}
	
	lastWifiReconnectAttempt = currentTime;
	Serial << "# wifiUpdate: done\n";
}

void emitWifiProps(TOGoS::Arduino::RelayTimer::PropConsumer &dest) {
	byte macAddressBuffer[6];
	
	dest.accept("mac-address", macAddressToHex(macAddressBuffer, ":").c_str());
	dest.accept("status-code", WiFi.status());
	dest.accept("ssid", WiFi.SSID().c_str());
	dest.accept("connected", formatBool(WiFi.status() == WL_CONNECTED));
	dest.accept("auto-connect", formatBool(WiFi.getAutoConnect()));
	dest.accept("auto-reconnect", formatBool(WiFi.getAutoReconnect()));
}

int broadcastVerbosity = 0;
long lastHeloBroadcast = -1;
WiFiUDP udp;
bool udpInitialized;

void updateHeloBroadcast(long currentTime, boolean forceUpdate) {
	if( currentTime - lastHeloBroadcast < 10000 && !forceUpdate ) return;
	if( !udpInitialized ) {
		if( broadcastVerbosity >= 200 ) Serial << "# UDP not initialized; skipping updateHelo\n";
		lastHeloBroadcast = currentTime; // So as not to spam Serial output
		return;
	}

	byte macAddressBuffer[6];
	WiFi.macAddress(macAddressBuffer);
   
	char buf[1024];
	TOGoS::BufferPrint bufPrn(buf, sizeof(buf));
   
	//bufPrn << "#HELO //" << macAddressToHex(macAddressBuffer, "-") << "/\n";
	bufPrn << "#HELO\n";
	bufPrn << "\n";
	// TODO: Use the PropConsumer to do all this so it can be shared
	bufPrn << "app-name " << appConfig.appName << "\n";
	bufPrn << "app-version " << appConfig.appVersion << "\n";
	bufPrn << "source-ref " << appConfig.sourceRef << "\n";
	bufPrn << "mac " << macAddressToHex(macAddressBuffer, ":") << "\n";
	bufPrn << "clock " << currentTime << "\n";
	bufPrn << "touch-button/pressed " << theButton.isPressed() << "\n";
	bufPrn << "relay/state/helo-override " << onOffAutoStr(helloRelayStateOverride) << "\n";
	bufPrn << "relay/state " << (theRelay.get() ? "on" : "off") << "\n";
	
	const char *broadcastAddr = "ff02::1";
	if( broadcastVerbosity >= 100 ) Serial << "# Broadcasting a HELO packet to [" << broadcastAddr << "]:" << myUdpPort << "\n";
	
	if( broadcastVerbosity >= 200 ) Serial << "# udp.beginPacket(\"" << broadcastAddr << "\", " << myUdpPort << ");\n";
	udp.beginPacket(broadcastAddr, myUdpPort);
	if( broadcastVerbosity >= 200 ) Serial << "# udp.write(buf, " << bufPrn.size() << ");\n";
	udp.write(buf, bufPrn.size());
	if( broadcastVerbosity >= 200 ) Serial << "# udp.endPacket();\n";
	udp.endPacket();
	
	if( broadcastVerbosity >= 200 ) Serial << "# lastHeloBroadcast = " << currentTime << "\n";
	lastHeloBroadcast = currentTime;
}

void updateHeloListen() {
	size_t len = udp.parsePacket();
	if( len == 0 ) return;
	
	Serial << "# Received " << len << "-byte UDP packet...";
#ifdef TAA_RELAYTIMER_HELO_OVERRIDE_ENABLED
	if( len == 27 ) { // "#HELO/PUT /relay/state\n\non\n"
		Serial << "which means 'on'!\n";
		helloRelayStateOverride = true;
	} else if( len == 28 ) { // "#HELO/PUT /relay/state\n\noff\n"
		Serial << "which means 'off'!\n";
		helloRelayStateOverride = false;
	} else if( len == 29 ) { // "#HELO/PUT /relay/state\n\nauto\n"
		Serial << "which means 'auto'!\n";
		helloRelayStateOverride = {};
	} else {
		Serial << "which means nothing to me; ignoring!\n";
	}
#else
	Serial << "but TAA_RELAYTIMER_HELO_OVERRIDE_ENABLED is not set, so I'm ignoring it.\n";
#endif
}

#else

void updateHeloBroadcast(long currentTime, boolean forceUpdate) { }
void updateHeloListen() { }

#endif

//// End WiFi stuff

void printHelp() {
	Serial << "# Welcome to " << appConfig.appName << "\n";
	Serial << "# Version: " << appConfig.appVersion << "\n";
	Serial << "# Commands:\n";
	Serial << "#   help     ; print this help\n";
	Serial << "#   echo ... ; echo stuff back to serial\n";
	Serial << "#   info     ; show constants and other info\n";
	Serial << "#   button/long-press  ; do long-press action\n";
	Serial << "#   button/short-press ; do short-press action\n";
#ifdef TAA_RELAYTIMER_WIFI_ENABLED
	Serial << "#   wifi/connect <ssid> <password> ; attempt to connect to WiFi\n";
	Serial << "#   wifi/connect ; Attempt to connect to WiFi without explicit ssid/password.\n";
	Serial << "#                ; This may or not actually use the last-configured ssid/password.\n";
	Serial << "#                ; It may depend on the board, or I may be confised.\n";
#endif
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
	dest.accept("TAA_RELAYTIMER_WIFI_ENABLED",
#ifdef TAA_RELAYTIMER_WIFI_ENABLED
		"(defined)"
#else
		"(not defined)"
#endif
   );
	dest.accept("TAA_RELAYTIMER_HELO_OVERRIDE_ENABLED",
#ifdef TAA_RELAYTIMER_HELO_OVERRIDE_ENABLED
		"(defined)"
#else
		"(not defined)"
#endif
   );
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
	
#ifdef TAA_RELAYTIMER_WIFI_ENABLED
	Serial << "# WiFi:\n";
	// TODO: It probably wouldn't hurt to just print out the SSIDs.
	Serial << "#  hardcoded-network-count = " << wifiNetworks.size() << "\n";
	emitWifiProps(infoPropEmitter);
#endif
	
	Serial << "# Timer:\n";
	theTimer->emitProps(infoPropEmitter);
	// Serial << "#  name = \"" << theTimer->getName() << "\"\n";
	
	Serial << "# Button:\n";
	Serial << "#  pressed = " << formatBool(theButton.isPressed()) << "\n";
	Serial << "# Relay:\n";
	Serial << "#  state according to timer = " << onOffStr(theTimer->isRelayOnAt(currentTickTime)) << "\n";
	Serial << "#  state accoding to HELO override = " << onOffAutoStr(helloRelayStateOverride) << "\n";
	Serial << "#  state = " << onOffStr(theRelay.get()) << "\n";
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
#ifdef TAA_RELAYTIMER_WIFI_ENABLED
	} else if( tcmd.path == "wifi/connect" ) {
		if( tcmd.args.size() == 0 ) {
			configureWifi(WiFi);
			WiFi.begin();
		} else if( tcmd.args.size() == 2 ) {
			configureWifi(WiFi);
			WiFi.begin(std::string(tcmd.args[0]).c_str(), std::string(tcmd.args[1]).c_str());
		} else {
			Serial << F("# Error: ") << std::string(tcmd.path) << F(" requires either 0 or 2 arguments: ssid, secret\n");
		}
#endif
	} else {
		Serial << F("# Unrecognized command: '") << tcmd.path << F("'; try 'help'.\n");
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

#ifdef TOGOSARDUINOAPPS2021_WIFINET0_SSID
	wifiNetworks.emplace_back(TOGOSARDUINOAPPS2021_WIFINET0_SSID, TOGOSARDUINOAPPS2021_WIFINET0_PASSWORD);
#endif
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
	bool shouldRelayBeOn =
		helloRelayStateOverride.has_value() ? *helloRelayStateOverride :
		theTimer->isRelayOnAt(currentTickTime);
	bool shouldForceHeloUpdate = false;
	if( !previousRelayState.has_value() || shouldRelayBeOn != *previousRelayState ) {
		Serial << "# Switching relay " << (shouldRelayBeOn ? "on" : "off") << "\n";
		theRelay.set(shouldRelayBeOn);
		previousRelayState = shouldRelayBeOn;
		shouldForceHeloUpdate = true;
	}
	while( Serial.available() > 0 ) {
		TLIBuffer::BufferState bufState = commandBuffer.onChar(Serial.read());
		if( bufState == TLIBuffer::BufferState::READY ) {
			processLine( commandBuffer.str() );
			commandBuffer.reset();
		}
	}
#ifdef TAA_RELAYTIMER_WIFI_ENABLED
	updateWifi(currentTickTime);
	
	if( !udpInitialized && isWiFiConnected() ) {
		// beginPacket() seems to crash if called before WiFi connected.
		// Which is odd -- why didn't ES2021 run into that problem?
		int stat = udp.begin(16378);
		Serial << "# udp.begin(16378)... " << stat << "\n";
		if( stat ) udpInitialized = true;
	}
	if( udpInitialized ) {
		updateHeloBroadcast(currentTickTime, shouldForceHeloUpdate);
		updateHeloListen();
	}
#endif
	
	delay(10);
}
