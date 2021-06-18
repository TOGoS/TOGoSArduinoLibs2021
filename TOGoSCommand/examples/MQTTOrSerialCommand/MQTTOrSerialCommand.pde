/**
 * Will (when done) demonstrate using TOGoS::TLIBuffer to read commands from Serial
 * and also from MQTT!
 */

const char *APP_NAME = "MQTTOrSerialCommand v2021-06-18";

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

Print& operator<<(Print& printer, const TokenizedCommand &cmd) {
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

Print& operator<<(Print& printer, const ParseError& parseError) {
  return printer << parseError.getErrorMessage() << " at index " << parseError.errorOffset;
}

Print& operator<<(Print& printer, const CommandResultCode& crc) {
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

// WiFiCommandHandler wifiCommandHandler(); // This confuses the compiler lol.
WiFiCommandHandler wifiCommandHandler;
WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
MQTTMaintainer mqttMaintainer = MQTTMaintainer::makeStandard(&pubSubClient, "mqtt-or-serial-demo", "mqtt-or-serial-demo"); // TODO: Let this be set later
MQTTCommandHandler mqttCommandHandler(&mqttMaintainer);

void printWifiStatus() {
  Serial << "wifi/status-code " << WiFi.status() << "\n";
  Serial << "wifi/ssid " << WiFi.SSID() << "\n";
  Serial << "wifi/connected " << (WiFi.status() == WL_CONNECTED ? "true" : "false") << "\n";
}

void printMqttStatus() {
  Serial << "mqtt/server/host " << mqttMaintainer.serverName << "\n";
  Serial << "mqtt/server/port " << mqttMaintainer.serverPort << "\n";
  Serial << "mqtt/connected " << (mqttMaintainer.isConnected() ? "true" : "false") << "\n";
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
  } else if( tcmd.path == "hi" || tcmd.path == "hello" ) {
    // It is customary for 'hello' command to respond with some basic info
    Serial << "# Hi there from " << APP_NAME << "!\n";
    Serial << "# Type 'help' for more help.\n";
    return CommandResult::ok();
  } else if( tcmd.path == "help" ) {
    Serial << "# Hello from " << APP_NAME << "!\n";
    Serial << "# Commands:\n";
    Serial << "#   echo $arg1 .. $argN\n";
    Serial << "#   echo/lines $arg1 .. $argN\n";
    Serial << "#   status ; print status of WiFi, MQTT connection, etc\n";
    Serial << "#   wifi/connect $ssid $password ; connect to a WiFi network\n";
    Serial << "#   mqtt/connect $server $port $username $password ; connect to an MQTT server\n";
    Serial << "#   mqtt/disconnect ; disconnect/stop trying to connect to any MQTT server\n";
    Serial << "#   mqtt/publish $topic $value ; publish to MQTT\n";
    Serial << "# WiFi and MQTT connections will automatically reconnect\n";
    Serial << "# unless server name is empty string (represented by '{}')\n";
    return CommandResult::ok();
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
  Serial.begin(115200);
  delay(100);
  Serial << "# Hi, I'm MQTTOrSerialCommand!\n";
  Serial << "# Enter commands of the form:\n";
  Serial << "#   <path> [<argument> ...]\n";
  Serial << "# For example:\n";
  Serial << "#   echo/lines foo bar {baz quux}\n";
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
