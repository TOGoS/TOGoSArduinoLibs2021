const char * APP_NAME = "EnvironmentalSensor2021 v0.0.1";
const int BOOTING_BLINKS = 5;
const int GOING_TO_SLEEP_BLINKS = 2;

/*
 * Wiring:
 * - D1 = I2C clock
 * - D2 = I2C data
 * - RST to d0 to enable waking from deep sleep,
 *   but this seems to need to be disconnected during flashing.
 * - 3V3 to peripherals' power +
 * - D5 to peripheras' power - (this way it will be unpowered while the ESP is asleep)
 */

extern "C" {
  #include <stdlib.h> // atol
}

// ESP8266 board includes 'functional' library, but unforch regular Arduino does not.
// So until I make a replacement (a la TOGoS/StringView), you're stuck with ESP boards.
// Which is fine for me because that's all I ever use anyway.
#include <functional>
#include <vector>

#include <ESP8266WiFi.h>
#include <Print.h>
#include <PubSubClient.h>

#include <TOGoSStringView.h>
#include <TOGoSCommand.h>
#include <TOGoS/Command/TLIBuffer.h>
#include <TOGoS/Command/ParseError.h>
#include <TOGoS/Command/ParseResult.h>
#include <TOGoS/Command/TokenizedCommand.h>
#include <TOGoS/Command/CommandDispatcher.h>
#include <TOGoS/Command/MQTTMaintainer.h>
#include <TOGoS/Command/MQTTCommandHandler.h>
#include <TOGoSStreamOperators.h>

using CommandHandler = TOGoS::Command::CommandHandler;
using CommandSource = TOGoS::Command::CommandSource;
using CommandResultCode = TOGoS::Command::CommandResultCode;
using CommandResult = TOGoS::Command::CommandResult;
using ParseError = TOGoS::Command::ParseError;
using StringView = TOGoS::StringView;
using TokenizedCommand = TOGoS::Command::TokenizedCommand;
using TCPR = TOGoS::Command::ParseResult<TokenizedCommand>;

void blinkBuiltin(int count) {
  for( ; count > 0 ; --count ) {
    delay(74);
    digitalWrite(LED_BUILTIN, LOW);
    delay(150);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(75);
  }
}


char hexDigit(int num) {
  num = num & 0xF;
  if( num < 10 ) return '0' + num;
  if( num < 16 ) return 'A' + num - 10;
  return '?'; // Should be unpossible
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

void printMacAddressHex(uint8_t *macAddress, const char *octetSeparator, class Print& printer) {
  for( int i = 0; i < 6; ++i ) {
    if( i > 0 ) printer.print(octetSeparator);
    printer.print(hexDigit(macAddress[i] >> 4));
    printer.print(hexDigit(macAddress[i]));
  }
}



Print& operator<<(Print &printer, const TokenizedCommand &cmd) {
  printer << "path:{" << cmd.path << "}";

  if( cmd.argStr.size() > 0 ) {
    printer << " argStr:{" << cmd.argStr << "}";
  }
  printer << " args:{";
  for( int i=0; i<cmd.args.size(); ++i ) {
    if( i > 0 ) printer << " ";
    printer << "{" << cmd.args[i] << "}";
  }
  printer << "}";
  return printer;
}

Print& operator<<(Print &printer, const ParseError& parseError) {
  return printer << parseError.getErrorMessage() << " at index " << parseError.errorOffset;
}

Print& operator<<(Print &printer, const CommandResultCode& crc) {
  switch( crc ) {
  case CommandResultCode::SHRUG       : return printer << "Unknown command";
  case CommandResultCode::HANDLED     : return printer << "Ok";
  case CommandResultCode::FAILED      : return printer << "Failed";
  case CommandResultCode::NOT_ALLOWED : return printer << "Not allowed";
  case CommandResultCode::CALLER_ERROR: return printer << "Usage error";
  }
  return printer << "Unknown CommandResultCode: " << (int)crc;
}

Print& operator<<(Print& printer, const CommandResult& cresult) {
  printer << cresult.code;
  if( !cresult.value.empty() > 0 ) printer << ": " << cresult.value;
  return printer;
}

namespace TOGoS { namespace Command {
  class WiFiCommandHandler {
  public:
    CommandResult operator()(const TokenizedCommand &cmd, CommandSource src);
  };
}}

CommandResult TOGoS::Command::WiFiCommandHandler::operator()(const TokenizedCommand &cmd, CommandSource src) {
  if( cmd.path == "wifi/connect" ) {
    if( cmd.args.size() != 2 ) {
      return CommandResult::callerError(std::string(cmd.path)+" requires 2 arguments: ssid, secret");
    }
    WiFi.begin(std::string(cmd.args[0]).c_str(), std::string(cmd.args[1]).c_str());
    return CommandResult::ok();
  } else {
    return CommandResult::shrug();
  }
}

using MQTTMaintainer = TOGoS::Command::MQTTMaintainer;
using MQTTCommandHandler = TOGoS::Command::MQTTCommandHandler;
using WiFiCommandHandler = TOGoS::Command::WiFiCommandHandler;

std::string getDefaultClientId() {
  byte macAddressBuffer[6];
  return macAddressToHex(macAddressBuffer, ":");
}

// WiFiCommandHandler wifiCommandHandler(); // This confuses the compiler lol.
WiFiCommandHandler wifiCommandHandler;
WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
MQTTMaintainer mqttMaintainer = MQTTMaintainer::makeStandard(
  &pubSubClient, getDefaultClientId(), getDefaultClientId());
MQTTCommandHandler mqttCommandHandler(&mqttMaintainer);


void printWifiStatus() {
  byte macAddressBuffer[6];
  WiFi.macAddress(macAddressBuffer);

  Serial << "wifi/mac-address " << macAddressToHex(macAddressBuffer, ":") << "\n";
  Serial << "wifi/status-code " << WiFi.status() << "\n";
  Serial << "wifi/ssid " << WiFi.SSID() << "\n";
  Serial << "wifi/connected " << (WiFi.status() == WL_CONNECTED ? "true" : "false") << "\n";
}

void printMqttStatus() {
  Serial << "mqtt/server/host " << mqttMaintainer.serverName << "\n";
  Serial << "mqtt/server/port " << mqttMaintainer.serverPort << "\n";
  Serial << "mqtt/connected " << (mqttMaintainer.isConnected() ? "true" : "false") << "\n";
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
}

CommandResult processEchoCommand(const TokenizedCommand& tcmd, CommandSource source) {
  if( tcmd.path == "echo" ) {
    for( int i=0; i<tcmd.args.size(); ++i ) {
      if( i > 0 ) Serial << " ";
      Serial << tcmd.args[i];
    }
    Serial << "\n";
    return CommandResult::ok();
  } else if( tcmd.path == "echo/lines" ) {
    for( auto arg : tcmd.args ) {
      Serial << arg << "\n";
    }
    return CommandResult::ok();
  } else if( tcmd.path == "status" ) {
    printWifiStatus();
    printMqttStatus();
    return CommandResult::ok();
  } else if( tcmd.path == "pins" ) {
    printPins();
    return CommandResult::ok();
  } else if( tcmd.path == "hi" || tcmd.path == "hello" ) {
    Serial << "# Hello from " << APP_NAME << "!\n";
    return CommandResult::ok();
  } else if( tcmd.path == "digital-write" ) {
    if( tcmd.args.size() != 2 ) {
      return CommandResult::callerError("Usage: digitalwrite <pin : uint> <value : 0|1>");
    }
    digitalWrite(atoi(std::string(tcmd.args[0]).c_str()), atoi(std::string(tcmd.args[1]).c_str()));
    return Commandresult::ok();
  } else if( tcmd.path == "help" ) {
    Serial << "# Hello from " << APP_NAME << "!\n";
    Serial << "# \n";
    Serial << "# Commands:\n";
    Serial << "#   echo $arg1 .. $argN\n";
    Serial << "#   echo/lines $arg1 .. $argN\n";
    Serial << "#   status ; print status of WiFi, MQTT connection, etc\n";
    Serial << "#   pins ; print pin information\n";
    Serial << "#   wifi/connect $ssid $password ; connect to a WiFi network\n";
    Serial << "#   mqtt/connect $server $port $username $password ; connect to an MQTT server\n";
    Serial << "#   mqtt/disconnect ; disconnect/stop trying to connect to any MQTT server\n";
    Serial << "#   mqtt/publish $topic $value ; publish to MQTT\n";
    Serial << "#   deesleep $seconds ; go into deep sleep\n";
    Serial << "# \n";
    Serial << "# Use curly braces to quote {multi-word arguments}\n";
    Serial << "# \n";
    Serial << "# WiFi and MQTT connections will automatically reconnect\n";
    Serial << "# unless server name is empty string (represented by '{}')\n";
    return CommandResult::ok();
  } else if( tcmd.path == "deepsleep" ) {
    if( tcmd.args.size() != 1 ) {
      return CommandResult::callerError("Usage: deepsleep <time : seconds>");
    } else {
      long sleeptime = atol(std::string(tcmd.args[0]).c_str());
      Serial << "# Going into deep sleep for " << sleeptime << "s\n";
      blinkBuiltin(GOING_TO_SLEEP_BLINKS); // 3 blinks = going to sleep
      ESP.deepSleep(sleeptime * 1000000);
      return CommandResult::ok(); // Probably never get here lol
    }
  } else if( tcmd.path == "bye" ) {
    Serial << "# See ya later!\n";
    return CommandResult::ok();
  } else {
    return CommandResult::shrug();
  }
}

TOGoS::Command::CommandDispatcher serialCommandDispatcher({
  processEchoCommand,
  wifiCommandHandler,
  mqttCommandHandler,
});

void processSerialLine(const StringView& line) {
  TCPR pr = TokenizedCommand::parse(line);
  if( pr.isError() ) {
    Serial << "# Error parsing command: " << pr.error << "\n";
    return;
  }

  const TokenizedCommand &tcmd = pr.value;

  if( tcmd.path.size() == 0 ) return;
  
  CommandResult cresult = serialCommandDispatcher(tcmd, CommandSource::CEREAL);
  Serial << "# " << tcmd.path << ": " << cresult << "\n";
}

using TLIBuffer = TOGoS::Command::TLIBuffer;

TLIBuffer commandBuffer;

void setup() {
  delay(500); // Standard 'give me time to reprogram it' delay in case the program messes up some I/O pins
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);
  Serial << "\n\n"; // Get past any junk in the terminal
  Serial << "# " << APP_NAME << " setup()...\n";
  blinkBuiltin(BOOTING_BLINKS); // 5 blinks = booting, same as ArduinoPowerSwitch
  Serial << "# Type 'help' for help.\n";
}

void loop() {
  while( Serial.available() > 0 ) {
    TLIBuffer::BufferState bufState = commandBuffer.onChar(Serial.read());
    if( bufState == TLIBuffer::BufferState::READY ) {
      processSerialLine( commandBuffer.str() );
      commandBuffer.reset();
    }
  }
  delay(10);
  mqttMaintainer.update();
}
