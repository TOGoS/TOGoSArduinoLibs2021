/**
 * Will (when done) demonstrate using TOGoS::TLIBuffer to read commands from Serial
 * and also from MQTT!
 */

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
#include <TOGoSStreamOperators.h>
#include <vector>

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

/*
namespace TOGoS { namespace Command {
  class MQTTConnectCommandHandler {
    CommandResult operator()(TokenizedCommand tcmd, CommandSource source);
  }
}}

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
TOGoS::Command::MQTTMaintainer

CommandResult TOGoS::Command::MQTTConnectCommandHandler::operator()(TokenizedCommand tcmd, CommandSource source) {
  
}
*/

Print& operator<<(Print& printer, const ParseError& parseError) {
  return printer << parseError.getErrorMessage() << " at index " << parseError.errorOffset;
}

Print& operator<<(Print& printer, const CommandResult& cresult) {
  switch( cresult.code ) {
  case CommandResultCode::SHRUG:
    Serial << "Unknown command";
    break;
  case CommandResultCode::HANDLED:
    Serial << "Ok";
    break;
  case CommandResultCode::NOT_ALLOWED:
    Serial << "Not allowed";
    break;
  }
  return printer;
}

CommandResult processCommand(const TokenizedCommand& tcmd, CommandSource source) {
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
  } else if( tcmd.path == "hi" || tcmd.path == "hello" ) {
    Serial << "# Hi there!\n";
    return CommandResult::ok();
  } else if( tcmd.path == "bye" ) {
    Serial << "# See ya later!\n";
    return CommandResult::ok();
  } else {
    return CommandResult::shrug();
  }
}

void processLine(const StringView& line) {
  TCPR pr = TokenizedCommand::parse(line);
  if( pr.isError() ) {
    Serial << "# Error parsing command: " << pr.error << "\n";
    return;
  }

  const TokenizedCommand &tcmd = pr.value;

  if( tcmd.path.size() == 0 ) return;
  
  CommandResult cresult = processCommand(tcmd, CommandSource::CEREAL);
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
      processLine( commandBuffer.str() );
      commandBuffer.reset();
    }
  }
  delay(10);
}
